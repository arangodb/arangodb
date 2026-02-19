defmodule Toast.Process.ServerProcess do
  @moduledoc """
  GenServer that manages a single OS process via erlexec.

  Provides:
  - Process lifecycle (start/stop)
  - Crash detection via erlexec monitoring
  - Graceful shutdown: SIGTERM → wait → SIGKILL (handled by erlexec)
  - Process group management (child cleanup on BEAM exit)

  The GenServer does NOT auto-restart the OS process on crash.
  Crash events are reported to registered listeners (typically the
  deployment controller) via `{:server_crashed, server_id, info}` messages.
  """

  use GenServer

  require Logger

  @default_stop_timeout 30_000

  # --- Types ---

  @type server_id :: String.t()

  @type start_opts :: [
          id: server_id(),
          executable: Path.t(),
          args: [String.t()],
          env: [{String.t(), String.t()}],
          working_dir: Path.t(),
          listener: pid(),
          output_handler: (server_id(), binary() -> :ok) | nil,
          name: GenServer.name()
        ]

  @type status :: :starting | :running | :stopping | :stopped | :crashed

  @type crash_info :: %{
          exit_status: non_neg_integer(),
          signal: non_neg_integer() | nil,
          timestamp: DateTime.t()
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
    * `:output_handler` - `fn(server_id, binary) -> :ok` called for each stderr chunk (default: `nil`)
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
  If it doesn't exit in time, erlexec escalates to SIGKILL.
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
    Process.flag(:trap_exit, true)

    state = %{
      id: Keyword.fetch!(opts, :id),
      executable: Keyword.fetch!(opts, :executable),
      args: Keyword.get(opts, :args, []),
      env: Keyword.get(opts, :env, []),
      working_dir: Keyword.get(opts, :working_dir),
      exec_pid: nil,
      os_pid: nil,
      status: :stopped,
      listener: Keyword.get(opts, :listener),
      output_handler: Keyword.get(opts, :output_handler),
      stop_from: nil,
      stop_timer: nil
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
    new_state = do_stop(state, timeout, from)
    {:noreply, new_state}
  end

  def handle_call({:stop, _timeout}, _from, %{status: :stopped} = state) do
    {:reply, :ok, state}
  end

  def handle_call({:stop, _timeout}, _from, %{status: :crashed} = state) do
    {:reply, :ok, state}
  end

  def handle_call({:stop, _timeout}, _from, %{status: :stopping} = state) do
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
  def handle_info({:DOWN, os_pid, :process, _pid, reason}, %{os_pid: os_pid} = state) do
    new_state = handle_exit(state, reason)
    {:noreply, new_state}
  end

  def handle_info(:stop_timeout, %{status: :stopping} = state) do
    Logger.warning("#{state.id} (pid=#{state.os_pid}) stop timed out, sending SIGKILL")

    if state.os_pid, do: :exec.kill(state.os_pid, 9)

    # Give 5s for the DOWN message after SIGKILL; if it never comes, give up
    timer_ref = Process.send_after(self(), :kill_timeout, 5_000)
    {:noreply, %{state | stop_timer: timer_ref}}
  end

  def handle_info(:kill_timeout, %{status: :stopping} = state) do
    Logger.error("#{state.id} (pid=#{state.os_pid}) did not exit after SIGKILL, giving up")

    if state.stop_from, do: GenServer.reply(state.stop_from, :ok)

    {:noreply,
     %{state | status: :stopped, exec_pid: nil, os_pid: nil, stop_from: nil, stop_timer: nil}}
  end

  def handle_info(msg, state) when msg in [:stop_timeout, :kill_timeout] do
    {:noreply, state}
  end

  def handle_info({:stderr, os_pid, data}, %{os_pid: os_pid, output_handler: handler} = state)
      when handler != nil do
    handler.(state.id, data)
    {:noreply, state}
  end

  # Catch-all for late/orphaned messages
  def handle_info({:stdout, _os_pid, _data}, state), do: {:noreply, state}
  def handle_info({:stderr, _os_pid, _data}, state), do: {:noreply, state}
  def handle_info({:DOWN, _os_pid, :process, _pid, _reason}, state), do: {:noreply, state}

  def handle_info(msg, state) do
    Logger.debug("#{state.id}: unexpected message: #{inspect(msg)}")
    {:noreply, state}
  end

  @impl true
  def terminate(_reason, %{exec_pid: pid, status: status})
      when pid != nil and status in [:running, :stopping] do
    :exec.stop(pid)
  catch
    :exit, _ -> :ok
  end

  def terminate(_reason, _state), do: :ok

  # --- Internal ---

  defp do_launch(state) do
    cmd = [to_charlist(state.executable) | Enum.map(state.args, &to_charlist/1)]

    exec_opts =
      [:monitor, {:stdin, :null},
       {:kill_timeout, 5}, {:group, 0}, :kill_group] ++
        if(state.output_handler, do: [:stderr], else: []) ++
        exec_env(state.env) ++
        exec_cd(state.working_dir)

    case :exec.run(cmd, exec_opts) do
      {:ok, exec_pid, os_pid} ->
        Logger.debug(fn ->
          cmd_line = Enum.join([state.executable | state.args], " ")
          env_part = if state.env != [], do: "\n  env: #{inspect(state.env)}", else: ""
          "#{state.id} (os_pid=#{os_pid}) cmd: #{cmd_line}#{env_part}"
        end)

        {:ok,
         %{
           state
           | exec_pid: exec_pid,
             os_pid: os_pid,
             status: :running
         }}

      {:error, reason} ->
        Logger.error("Failed to launch #{state.id}: #{inspect(reason)}")
        {:error, reason}
    end
  end

  defp do_stop(state, timeout, from) do
    Logger.debug("Stopping #{state.id} (pid=#{state.os_pid}) with SIGTERM")

    # Non-blocking: initiates SIGTERM, then SIGKILL after kill_timeout (5s)
    :exec.stop(state.exec_pid)

    # Set our own timeout as a safety net
    timer_ref = Process.send_after(self(), :stop_timeout, timeout)

    %{state | status: :stopping, stop_from: from, stop_timer: timer_ref}
  end

  defp handle_exit(state, reason) do
    cancel_timer(state.stop_timer)

    {exit_status, signal} = decode_exit_reason(reason)

    case state.status do
      :stopping ->
        Logger.debug("#{state.id} exited during stop (status=#{exit_status})")

        if state.stop_from, do: GenServer.reply(state.stop_from, :ok)

        %{
          state
          | status: :stopped,
            exec_pid: nil,
            os_pid: nil,
            stop_from: nil,
            stop_timer: nil
        }

      _ ->
        crash_info = %{
          exit_status: exit_status,
          signal: signal,
          timestamp: DateTime.utc_now()
        }

        Logger.error(
          "#{state.id} crashed (status=#{exit_status}, signal=#{inspect(signal)})"
        )

        notify_listener(state.listener, state.id, crash_info)

        %{state | status: :crashed, exec_pid: nil, os_pid: nil}
    end
  end

  defp decode_exit_reason(:normal), do: {0, nil}

  defp decode_exit_reason({:exit_status, status}) do
    case :exec.status(status) do
      {:status, code} -> {code, nil}
      {:signal, sig, _core} ->
        signum = signal_to_int(sig)
        {128 + signum, signum}
    end
  end

  defp decode_exit_reason(_other), do: {1, nil}

  defp signal_to_int(sig) when is_integer(sig), do: sig

  defp signal_to_int(sig) when is_atom(sig) do
    :exec.signal_to_int(sig)
  rescue
    FunctionClauseError -> 0
  end

  defp notify_listener(nil, _id, _info), do: :ok

  defp notify_listener(listener, id, crash_info) do
    send(listener, {:server_crashed, id, crash_info})
  end

  defp cancel_timer(nil), do: :ok
  defp cancel_timer(ref), do: Process.cancel_timer(ref)

  defp exec_env([]), do: []
  defp exec_env(env), do: [{:env, Enum.map(env, fn {k, v} -> {to_charlist(k), to_charlist(v)} end)}]

  defp exec_cd(nil), do: []
  defp exec_cd(dir), do: [{:cd, to_charlist(dir)}]
end
