defmodule Toast.Process.ServerProcess do
  @moduledoc """
  GenServer that manages a single OS process via erlexec.

  Provides:
  - Process lifecycle (start/stop)
  - Crash detection via erlexec monitoring
  - Graceful shutdown: SIGTERM → wait → SIGABRT → wait → SIGKILL
  - Process group management (child cleanup on BEAM exit)

  The GenServer does NOT auto-restart the OS process on crash.
  Crash events are reported to registered listeners (typically the
  deployment controller) via `{:server_crashed, server_id, info}` messages.
  """

  use GenServer, restart: :temporary

  require Logger

  @default_stop_timeout 30_000
  @sigkill 9
  @sigstop 19
  @sigcont 18

  # Escalation chain waits after SIGTERM times out
  @abort_wait 5_000
  @kill_wait 5_000

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

  @type status ::
          :starting
          | :running
          | :stopping
          | :aborting
          | :killing
          | :stopped
          | :crashed
          | :paused
          | :killed

  @type crash_info :: Toast.Process.CrashInfo.t()

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
    {gen_opts, init_opts} = Keyword.split(opts, [:name])
    GenServer.start_link(__MODULE__, init_opts, gen_opts)
  end

  @doc """
  Launch the OS process. The GenServer must be started first via `start_link/1`.
  """
  @spec launch(GenServer.server()) :: :ok | {:error, term()}
  def launch(server) do
    GenServer.call(server, :launch)
  end

  # GenServer.call headroom: SIGABRT wait + SIGKILL wait + buffer
  @escalation_overhead @abort_wait + @kill_wait + 5_000

  @doc "Maximum time the escalation chain (SIGABRT → SIGKILL) can add beyond the SIGTERM timeout."
  @spec escalation_overhead() :: pos_integer()
  def escalation_overhead, do: @escalation_overhead

  @doc """
  Gracefully stop the OS process.

  Sends SIGTERM and waits up to `timeout` ms for the process to exit.
  If it doesn't exit in time, escalates to SIGABRT, then SIGKILL.
  """
  @spec stop(GenServer.server(), timeout()) :: :ok | :escalated | {:error, term()}
  def stop(server, timeout \\ @default_stop_timeout) do
    GenServer.call(server, {:stop, timeout}, timeout + @escalation_overhead)
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

  @spec kill(GenServer.server()) :: :ok | {:error, :not_running}
  def kill(server), do: GenServer.call(server, :kill)

  @doc """
  Send a signal to the OS process without changing the GenServer state.

  The process remains in its current state (:running, :paused, etc.) and will
  transition normally when the OS process exits (via the erlexec DOWN message).
  This is used for SIGABRT during timeout handling — the signal triggers the
  crash handler (backtrace + coredump) and the crash flows through the normal
  expected-crash path.
  """
  @spec send_signal(GenServer.server(), pos_integer() | atom()) :: :ok | {:error, :not_running}
  def send_signal(server, signal), do: GenServer.call(server, {:send_signal, signal})

  @spec pause(GenServer.server()) :: :ok | {:error, :not_running}
  def pause(server), do: GenServer.call(server, :pause)

  @spec resume(GenServer.server()) :: :ok | {:error, :not_paused}
  def resume(server), do: GenServer.call(server, :resume)

  @spec relaunch(GenServer.server(), keyword()) :: :ok | {:error, term()}
  def relaunch(server, opts \\ []), do: GenServer.call(server, {:relaunch, opts})

  # --- Internal state ---

  defmodule State do
    @moduledoc false
    @enforce_keys [:id, :executable, :args, :original_args]
    defstruct [
      :id,
      :executable,
      :args,
      :original_args,
      :working_dir,
      :exec_pid,
      :os_pid,
      :listener,
      :output_handler,
      :stop_from,
      :stop_timer,
      env: [],
      status: :stopped
    ]
  end

  # --- Server callbacks ---

  @impl true
  def init(opts) do
    Process.flag(:trap_exit, true)

    args = Keyword.get(opts, :args, [])

    state = %State{
      id: Keyword.fetch!(opts, :id),
      executable: Keyword.fetch!(opts, :executable),
      args: args,
      original_args: args,
      env: Keyword.get(opts, :env, []),
      working_dir: Keyword.get(opts, :working_dir),
      listener: Keyword.get(opts, :listener),
      output_handler: Keyword.get(opts, :output_handler)
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

  def handle_call({:stop, _timeout}, _from, %{status: status} = state)
      when status in [:stopped, :crashed, :killed] do
    {:reply, :ok, state}
  end

  def handle_call({:stop, _timeout}, _from, %{status: :stopping} = state) do
    {:reply, {:error, :already_stopping}, state}
  end

  # Paused process: resume (SIGCONT) then proceed with normal stop flow
  def handle_call({:stop, timeout}, from, %{status: :paused} = state) do
    :exec.kill(state.os_pid, @sigcont)
    new_state = do_stop(%{state | status: :running}, timeout, from)
    {:noreply, new_state}
  end

  def handle_call(:kill, _from, %{status: status} = state)
      when status in [:running, :paused] do
    Logger.info("#{state.id}: killing process (SIGKILL)")
    :exec.kill(state.os_pid, @sigkill)
    {:reply, :ok, %{state | status: :killed}}
  end

  def handle_call(:kill, _from, state) do
    {:reply, {:error, :not_running}, state}
  end

  def handle_call({:send_signal, signal}, _from, %{status: status} = state)
      when status in [:running, :paused] do
    :exec.kill(state.os_pid, signal)
    {:reply, :ok, state}
  end

  def handle_call({:send_signal, _signal}, _from, state) do
    {:reply, {:error, :not_running}, state}
  end

  def handle_call(:pause, _from, %{status: :running} = state) do
    Logger.debug("#{state.id}: pausing process (SIGSTOP)")
    :exec.kill(state.os_pid, @sigstop)
    {:reply, :ok, %{state | status: :paused}}
  end

  def handle_call(:pause, _from, state) do
    {:reply, {:error, :not_running}, state}
  end

  def handle_call(:resume, _from, %{status: :paused} = state) do
    Logger.debug("#{state.id}: resuming process (SIGCONT)")
    :exec.kill(state.os_pid, @sigcont)
    {:reply, :ok, %{state | status: :running}}
  end

  def handle_call(:resume, _from, state) do
    {:reply, {:error, :not_paused}, state}
  end

  def handle_call({:relaunch, opts}, _from, %{status: status} = state)
      when status in [:stopped, :killed, :crashed] do
    Logger.info("#{state.id}: relaunching process")
    extra_args = Keyword.get(opts, :args, [])
    merged_state = %{state | exec_pid: nil, os_pid: nil, args: state.original_args ++ extra_args}

    case do_launch(merged_state) do
      {:ok, new_state} ->
        Logger.debug("#{state.id}: process relaunched (os_pid=#{new_state.os_pid})")
        {:reply, :ok, new_state}

      {:error, reason} ->
        {:reply, {:error, reason}, state}
    end
  end

  def handle_call({:relaunch, _opts}, _from, %{status: status} = state) do
    {:reply, {:error, {:already_launched, status}}, state}
  end

  def handle_call(:status, _from, state) do
    {:reply, state.status, state}
  end

  def handle_call(:os_pid, _from, state) do
    {:reply, state.os_pid, state}
  end

  @impl true
  def handle_info({:DOWN, os_pid, :process, _pid, reason}, %{os_pid: os_pid} = state) do
    new_state = handle_exit(state, reason)
    {:noreply, new_state}
  end

  def handle_info(:stop_timeout, %{status: :stopping} = state) do
    Logger.warning(
      "#{state.id} (pid=#{state.os_pid}) stop timed out, sending SIGABRT for crash backtrace"
    )

    if state.os_pid, do: :exec.kill(state.os_pid, :sigabrt)

    timer_ref = Process.send_after(self(), :abort_timeout, @abort_wait)
    {:noreply, %{state | stop_timer: timer_ref, status: :aborting}}
  end

  def handle_info(:abort_timeout, %{status: :aborting} = state) do
    Logger.warning(
      "#{state.id} (pid=#{state.os_pid}) did not exit after SIGABRT, sending SIGKILL"
    )

    if state.os_pid, do: :exec.kill(state.os_pid, @sigkill)

    timer_ref = Process.send_after(self(), :kill_timeout, @kill_wait)
    {:noreply, %{state | stop_timer: timer_ref, status: :killing}}
  end

  def handle_info(:kill_timeout, %{status: :killing} = state) do
    Logger.error("#{state.id} (pid=#{state.os_pid}) did not exit after SIGKILL, giving up")

    if state.stop_from, do: GenServer.reply(state.stop_from, :escalated)

    {:noreply, reset_process_state(state, :stopped)}
  end

  def handle_info(msg, state) when msg in [:stop_timeout, :abort_timeout, :kill_timeout] do
    {:noreply, state}
  end

  def handle_info({:stderr, os_pid, data}, %{os_pid: os_pid, output_handler: handler} = state)
      when is_function(handler) do
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
      when pid != nil and status in [:running, :stopping, :aborting, :killing, :paused] do
    :exec.stop(pid)
  catch
    :exit, _ -> :ok
  end

  def terminate(_reason, _state), do: :ok

  # --- Internal ---

  defp do_launch(state) do
    cmd = [to_charlist(state.executable) | Enum.map(state.args, &to_charlist/1)]

    exec_opts =
      [:monitor, {:stdin, :null}, {:kill_timeout, 300}, {:group, 0}, :kill_group] ++
        if(state.output_handler, do: [:stderr], else: []) ++
        exec_env(state.env) ++
        exec_cd(state.working_dir)

    case :exec.run(cmd, exec_opts) do
      {:ok, exec_pid, os_pid} ->
        log_launch(state, os_pid)
        {:ok, %{state | exec_pid: exec_pid, os_pid: os_pid, status: :running}}

      {:error, reason} ->
        Logger.error("Failed to launch #{state.id}: #{inspect(reason)}")
        {:error, reason}
    end
  end

  defp log_launch(state, os_pid) do
    Logger.debug(fn ->
      cmd_line = Enum.join([state.executable | state.args], " ")
      env_part = if state.env != [], do: "\n  env: #{inspect(state.env)}", else: ""
      "#{state.id} (os_pid=#{os_pid}) cmd: #{cmd_line}#{env_part}"
    end)
  end

  defp do_stop(state, timeout, from) do
    Logger.debug("Stopping #{state.id} (pid=#{state.os_pid}) with SIGTERM")

    # Non-blocking: initiates SIGTERM (erlexec kill_timeout=300s is a safety net only)
    :exec.stop(state.exec_pid)

    # Our timers handle the escalation: stop_timeout → SIGABRT → abort_timeout → SIGKILL
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
        reset_process_state(state, :stopped)

      status when status in [:aborting, :killing] ->
        Logger.debug("#{state.id} exited during stop after escalation (status=#{exit_status})")
        if state.stop_from, do: GenServer.reply(state.stop_from, :escalated)
        reset_process_state(state, :stopped)

      :killed ->
        %{state | exec_pid: nil, os_pid: nil}

      _ ->
        crash_info = %Toast.Process.CrashInfo{
          exit_status: exit_status,
          signal: signal,
          timestamp: DateTime.utc_now(),
          os_pid: state.os_pid
        }

        Logger.error("#{state.id} crashed (status=#{exit_status}, signal=#{inspect(signal)})")
        notify_listener(state.listener, state.id, crash_info)
        reset_process_state(state, :crashed)
    end
  end

  defp decode_exit_reason(:normal), do: {0, nil}

  defp decode_exit_reason({:exit_status, status}) do
    case :exec.status(status) do
      {:status, code} ->
        {code, nil}

      {:signal, sig, _core} ->
        signum = signal_to_int(sig)
        exit_status = if signum, do: 128 + signum, else: 128
        {exit_status, signum || sig}
    end
  end

  defp decode_exit_reason(_other), do: {1, nil}

  defp signal_to_int(sig) when is_integer(sig), do: sig

  defp signal_to_int(sig) when is_atom(sig) do
    :exec.signal_to_int(sig)
  rescue
    FunctionClauseError ->
      Logger.warning("Unknown signal atom from erlexec: #{inspect(sig)}")
      nil
  end

  defp notify_listener(nil, _id, _info), do: :ok

  defp notify_listener(listener, id, crash_info) do
    send(listener, {:server_crashed, id, crash_info})
  end

  defp cancel_timer(nil), do: :ok
  defp cancel_timer(ref), do: Process.cancel_timer(ref)

  defp reset_process_state(state, new_status) do
    %{state | status: new_status, exec_pid: nil, os_pid: nil, stop_from: nil, stop_timer: nil}
  end

  defp exec_env([]), do: []

  defp exec_env(env),
    do: [{:env, Enum.map(env, fn {k, v} -> {to_charlist(k), to_charlist(v)} end)}]

  defp exec_cd(nil), do: []
  defp exec_cd(dir), do: [{:cd, to_charlist(dir)}]
end
