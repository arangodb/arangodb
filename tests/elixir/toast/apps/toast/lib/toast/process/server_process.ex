defmodule Toast.Process.ServerProcess do
  @moduledoc """
  GenServer that manages a single OS process via an Erlang Port.

  Provides:
  - Process lifecycle (start/stop)
  - Crash detection via `:exit_status`
  - Graceful shutdown: SIGTERM → wait → SIGKILL escalation
  - Output capture for diagnostics

  The GenServer does NOT auto-restart the OS process on crash.
  Crash events are reported to registered listeners (typically the
  deployment controller) via `{:server_crashed, server_id, info}` messages.
  """

  use GenServer

  require Logger

  alias Toast.Process.Signal

  @default_stop_timeout 30_000
  @kill_escalation_timeout 5_000

  # --- Types ---

  @type server_id :: String.t()

  @type start_opts :: [
          id: server_id(),
          executable: Path.t(),
          args: [String.t()],
          env: [{String.t(), String.t()}],
          working_dir: Path.t(),
          listener: pid(),
          name: GenServer.name()
        ]

  @type status :: :starting | :running | :stopping | :stopped | :crashed

  @type crash_info :: %{
          exit_status: non_neg_integer(),
          signal: non_neg_integer() | nil,
          timestamp: DateTime.t()
        }

  @type state :: %{
          id: server_id(),
          executable: Path.t(),
          args: [String.t()],
          env: [{String.t(), String.t()}],
          working_dir: Path.t() | nil,
          port: port() | nil,
          os_pid: pos_integer() | nil,
          status: status(),
          listener: pid() | nil,
          stop_timer: reference() | nil,
          stop_from: GenServer.from() | nil,
          output_buffer: iodata()
        }

  # --- Client API ---

  @doc """
  Start a ServerProcess GenServer.

  ## Options
    * `:id` - unique identifier for this server (required)
    * `:executable` - path to the executable (required)
    * `:args` - command line arguments (default: `[]`)
    * `:env` - environment variables as `[{key, value}]` (default: `[]`)
    * `:working_dir` - working directory (default: `nil`)
    * `:listener` - pid to receive crash notifications (default: `nil`)
    * `:name` - GenServer name registration (optional)
  """
  @spec start_link(start_opts()) :: GenServer.on_start()
  def start_link(opts) do
    {name, init_opts} = Keyword.pop(opts, :name)

    if name do
      GenServer.start_link(__MODULE__, init_opts, name: name)
    else
      GenServer.start_link(__MODULE__, init_opts)
    end
  end

  @doc """
  Launch the OS process. The GenServer must be started first via `start_link/1`.
  """
  @spec launch(GenServer.server()) :: :ok | {:error, term()}
  def launch(server) do
    GenServer.call(server, :launch)
  end

  @doc """
  Gracefully stop the OS process.

  Sends SIGTERM and waits up to `timeout` ms for the process to exit.
  If it doesn't exit in time, escalates to SIGKILL.
  """
  @spec stop(GenServer.server(), timeout()) :: :ok | {:error, term()}
  def stop(server, timeout \\ @default_stop_timeout) do
    GenServer.call(server, {:stop, timeout}, timeout + 5_000)
  end

  @doc """
  Get the current status of the managed process.
  """
  @spec status(GenServer.server()) :: status()
  def status(server) do
    GenServer.call(server, :status)
  end

  @doc """
  Get the OS pid of the managed process, if running.
  """
  @spec os_pid(GenServer.server()) :: pos_integer() | nil
  def os_pid(server) do
    GenServer.call(server, :os_pid)
  end

  @doc """
  Get the server id.
  """
  @spec id(GenServer.server()) :: server_id()
  def id(server) do
    GenServer.call(server, :id)
  end

  # --- Server callbacks ---

  @impl true
  def init(opts) do
    state = %{
      id: Keyword.fetch!(opts, :id),
      executable: Keyword.fetch!(opts, :executable),
      args: Keyword.get(opts, :args, []),
      env: Keyword.get(opts, :env, []),
      working_dir: Keyword.get(opts, :working_dir),
      port: nil,
      os_pid: nil,
      status: :stopped,
      listener: Keyword.get(opts, :listener),
      stop_timer: nil,
      stop_from: nil,
      output_buffer: []
    }

    {:ok, state}
  end

  @impl true
  def handle_call(:launch, _from, %{status: :stopped} = state) do
    case do_launch(state) do
      {:ok, new_state} ->
        {:reply, :ok, new_state}

      {:error, reason} ->
        {:reply, {:error, reason}, state}
    end
  end

  def handle_call(:launch, _from, %{status: status} = state) do
    {:reply, {:error, {:already_launched, status}}, state}
  end

  def handle_call({:stop, timeout}, from, %{status: :running} = state) do
    new_state = do_graceful_stop(state, timeout, from)
    {:noreply, new_state}
  end

  def handle_call({:stop, _timeout}, _from, %{status: :stopped} = state) do
    {:reply, :ok, state}
  end

  def handle_call({:stop, _timeout}, _from, %{status: :crashed} = state) do
    {:reply, :ok, state}
  end

  def handle_call({:stop, _timeout}, _from, %{status: :stopping} = state) do
    # Already stopping — caller will get a reply when the process exits
    {:reply, {:error, :already_stopping}, state}
  end

  def handle_call(:status, _from, state) do
    {:reply, state.status, state}
  end

  def handle_call(:os_pid, _from, state) do
    {:reply, state.os_pid, state}
  end

  def handle_call(:id, _from, state) do
    {:reply, state.id, state}
  end

  @impl true
  def handle_info({port, {:data, data}}, %{port: port} = state) do
    {:noreply, %{state | output_buffer: [state.output_buffer, data]}}
  end

  def handle_info({port, {:exit_status, exit_status}}, %{port: port} = state) do
    new_state = handle_exit(state, exit_status)
    {:noreply, new_state}
  end

  def handle_info(:kill_escalation, %{status: :stopping} = state) do
    new_state = do_kill_escalation(state)
    {:noreply, new_state}
  end

  def handle_info(:kill_escalation, state) do
    {:noreply, state}
  end

  def handle_info(:kill_group_escalation, %{status: :stopping} = state) do
    Logger.warning("[Toast] #{state.id} (pid=#{state.os_pid}) did not exit after SIGKILL, killing process group")

    Signal.kill_group(state.os_pid)

    if state.stop_from do
      GenServer.reply(state.stop_from, :ok)
    end

    {:noreply, %{state | status: :stopped, port: nil, os_pid: nil, stop_timer: nil, stop_from: nil}}
  end

  def handle_info(:kill_group_escalation, state) do
    {:noreply, state}
  end

  def handle_info({port, {:exit_status, _}}, state) when is_port(port) do
    # Late exit_status from an already-cleared port — safe to ignore
    {:noreply, state}
  end

  def handle_info({port, {:data, _}}, state) when is_port(port) do
    {:noreply, state}
  end

  # --- Internal ---

  defp do_launch(state) do
    executable = to_charlist(state.executable)

    port_opts =
      [:binary, :exit_status, :use_stdio, :stderr_to_stdout, {:args, state.args}] ++
        env_opt(state.env) ++
        cd_opt(state.working_dir)

    try do
      port = Port.open({:spawn_executable, executable}, port_opts)

      # Port.info returns nil if the port has already closed (process exited instantly).
      # We still track the port so handle_info can process the exit_status message.
      os_pid =
        case Port.info(port, :os_pid) do
          {:os_pid, pid} -> pid
          nil -> nil
        end

      cmd_line = Enum.join([state.executable | state.args], " ")

      Logger.debug(
        "[Toast] Launched #{state.id} (os_pid=#{os_pid})\n" <>
          "  cmd: #{cmd_line}\n" <>
          "  working_dir: #{state.working_dir}" <>
          if(state.env != [], do: "\n  env: #{inspect(state.env)}", else: "")
      )

      {:ok,
       %{
         state
         | port: port,
           os_pid: os_pid,
           status: :running,
           output_buffer: []
       }}
    rescue
      e -> {:error, e}
    end
  end

  defp do_graceful_stop(state, timeout, from) do
    Logger.debug("[Toast] Stopping #{state.id} (pid=#{state.os_pid}) with SIGTERM")

    Signal.term(state.os_pid)
    timer_ref = Process.send_after(self(), :kill_escalation, timeout)

    %{state | status: :stopping, stop_timer: timer_ref, stop_from: from}
  end

  defp do_kill_escalation(state) do
    Logger.warning("[Toast] #{state.id} (pid=#{state.os_pid}) did not exit after SIGTERM, sending SIGKILL")

    Signal.kill(state.os_pid)

    # Schedule a process group kill as final escalation
    timer_ref = Process.send_after(self(), :kill_group_escalation, @kill_escalation_timeout)
    %{state | stop_timer: timer_ref}
  end

  defp handle_exit(state, exit_status) do
    cancel_timer(state.stop_timer)

    signal = if exit_status > 128, do: exit_status - 128, else: nil

    case state.status do
      :stopping ->
        Logger.debug("[Toast] #{state.id} exited during stop (status=#{exit_status})")

        if state.stop_from do
          GenServer.reply(state.stop_from, :ok)
        end

        %{state | status: :stopped, port: nil, os_pid: nil, stop_timer: nil, stop_from: nil}

      _ ->
        crash_info = %{
          exit_status: exit_status,
          signal: signal,
          timestamp: DateTime.utc_now()
        }

        output = IO.iodata_to_binary(state.output_buffer)

        Logger.error(
          "[Toast] #{state.id} crashed (status=#{exit_status}, signal=#{inspect(signal)})" <>
            if(output != "", do: "\n  output: #{String.slice(output, 0, 2000)}", else: "")
        )

        notify_listener(state.listener, state.id, crash_info)

        %{state | status: :crashed, port: nil, os_pid: nil}
    end
  end

  defp notify_listener(nil, _id, _info), do: :ok

  defp notify_listener(listener, id, crash_info) do
    send(listener, {:server_crashed, id, crash_info})
  end

  defp cancel_timer(nil), do: :ok
  defp cancel_timer(ref), do: Process.cancel_timer(ref)

  defp env_opt([]), do: []
  defp env_opt(env), do: [{:env, Enum.map(env, fn {k, v} -> {to_charlist(k), to_charlist(v)} end)}]

  defp cd_opt(nil), do: []
  defp cd_opt(dir), do: [{:cd, to_charlist(dir)}]
end
