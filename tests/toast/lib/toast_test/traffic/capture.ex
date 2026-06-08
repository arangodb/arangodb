################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule ToastTest.Traffic.Capture do
  @moduledoc """
  Manages a tcpdump process that captures network traffic on loopback
  during a test suite run. Produces a raw pcap file for post-processing.
  """

  use GenServer, restart: :temporary

  require Logger

  @stop_timeout 5_000

  defstruct [:pcap_path, :exec_pid, :os_pid, :stop_from]

  @type t :: %__MODULE__{
          pcap_path: Path.t(),
          exec_pid: pid() | nil,
          os_pid: non_neg_integer() | nil,
          stop_from: GenServer.from() | nil
        }

  # --- Client API ---

  @doc """
  Start a traffic capture process under the given supervisor.

  ## Options
    * `:pcap_path` — file path for the captured pcap (required)
    * `:supervisor` — DynamicSupervisor to start under (default: `Toast.Process.Supervisor`)
  """
  @spec start(keyword()) :: {:ok, pid()} | {:error, term()}
  def start(opts) do
    supervisor = Keyword.get(opts, :supervisor, Toast.Process.Supervisor)
    DynamicSupervisor.start_child(supervisor, {__MODULE__, opts})
  end

  @doc "Stop the capture and return the pcap file path."
  @spec stop(pid()) :: {:ok, Path.t()} | {:error, term()}
  def stop(pid) do
    GenServer.call(pid, :stop, @stop_timeout + 2_000)
  catch
    :exit, _ -> {:error, :capture_dead}
  end

  def start_link(opts) do
    GenServer.start_link(__MODULE__, opts)
  end

  # --- Server callbacks ---

  @impl true
  def init(opts) do
    pcap_path = Keyword.fetch!(opts, :pcap_path)

    case find_tcpdump() do
      {:ok, tcpdump_path} ->
        case launch_tcpdump(tcpdump_path, pcap_path) do
          {:ok, exec_pid, os_pid} ->
            Logger.info("Traffic capture started (pid=#{os_pid}, pcap=#{pcap_path})")
            {:ok, %__MODULE__{pcap_path: pcap_path, exec_pid: exec_pid, os_pid: os_pid}}

          {:error, reason} ->
            {:stop, reason}
        end

      {:error, reason} ->
        {:stop, reason}
    end
  end

  @impl true
  def handle_call(:stop, _from, %{os_pid: nil} = state) do
    {:reply, {:ok, state.pcap_path}, state}
  end

  def handle_call(:stop, from, state) do
    Logger.info("Stopping traffic capture (pid=#{state.os_pid})")
    :exec.kill(state.os_pid, 15)
    Process.send_after(self(), :stop_timeout, @stop_timeout)
    {:noreply, %{state | stop_from: from}}
  end

  @impl true
  def handle_info({:DOWN, os_pid, :process, _pid, reason}, %{os_pid: os_pid} = state) do
    pcap_size = file_size(state.pcap_path)
    state = %{state | exec_pid: nil, os_pid: nil}

    if state.stop_from do
      Logger.info(
        "tcpdump exited (pid=#{os_pid}, reason=#{inspect(reason)}, pcap_size=#{pcap_size})"
      )

      GenServer.reply(state.stop_from, {:ok, state.pcap_path})
      {:stop, :normal, %{state | stop_from: nil}}
    else
      Logger.warning(
        "tcpdump exited unexpectedly (pid=#{os_pid}, reason=#{inspect(reason)}, pcap_size=#{pcap_size})"
      )

      {:noreply, state}
    end
  end

  def handle_info(:stop_timeout, %{os_pid: nil} = state) do
    {:noreply, state}
  end

  def handle_info(:stop_timeout, state) do
    Logger.warning("tcpdump did not exit after SIGTERM, sending SIGKILL (pid=#{state.os_pid})")
    :exec.kill(state.os_pid, 9)
    {:noreply, state}
  end

  def handle_info({:stderr, os_pid, data}, state) do
    Logger.debug("tcpdump stderr (pid=#{os_pid}): #{String.trim(to_string(data))}")
    {:noreply, state}
  end

  def handle_info(_msg, state), do: {:noreply, state}

  @impl true
  def terminate(_reason, %{os_pid: nil}), do: :ok

  def terminate(_reason, %{os_pid: os_pid}) do
    Logger.info("Terminating traffic capture, killing tcpdump (pid=#{os_pid})")

    try do
      :exec.kill(os_pid, 15)
      Process.sleep(500)
      :exec.kill(os_pid, 9)
    catch
      _, _ -> :ok
    end
  end

  # --- Internal ---

  defp find_tcpdump do
    case System.find_executable("tcpdump") do
      nil ->
        {:error,
         {:tcpdump_not_found,
          "tcpdump not found in PATH. Install tcpdump to use --capture-traffic."}}

      path ->
        {:ok, path}
    end
  end

  defp launch_tcpdump(tcpdump_path, pcap_path) do
    File.mkdir_p!(Path.dirname(pcap_path))

    args =
      ["-ni", "lo", "--immediate-mode", "-U", "-s0", "-w", pcap_path]
      |> Enum.map(&to_charlist/1)

    cmd = [to_charlist(tcpdump_path) | args]
    opts = [:monitor, {:stdin, :null}, :stderr]

    Logger.info("Launching tcpdump: #{tcpdump_path} #{Enum.join(args, " ")}")

    case :exec.run(cmd, opts) do
      {:ok, exec_pid, os_pid} ->
        case wait_for_startup(exec_pid, os_pid, pcap_path) do
          :ok -> {:ok, exec_pid, os_pid}
          {:error, reason} -> {:error, reason}
        end

      {:error, reason} ->
        {:error, {:tcpdump_launch_failed, reason}}
    end
  end

  defp wait_for_startup(exec_pid, os_pid, pcap_path) do
    receive do
      {:stderr, ^os_pid, data} ->
        output = to_string(data)
        Logger.debug("tcpdump startup (pid=#{os_pid}): #{String.trim(output)}")

        cond do
          String.contains?(output, "permission denied") or
              String.contains?(output, "Operation not permitted") ->
            try_kill(os_pid)

            {:error,
             {:tcpdump_permission_denied,
              "tcpdump lacks capture permissions. " <>
                "Run: sudo setcap cap_net_raw+ep #{System.find_executable("tcpdump") || "$(which tcpdump)"}"}}

          String.contains?(output, "listening on") ->
            :ok

          true ->
            wait_for_startup(exec_pid, os_pid, pcap_path)
        end
    after
      3_000 ->
        if File.exists?(pcap_path) do
          Logger.warning(
            "tcpdump did not print 'listening on' within 3s, but pcap file exists — proceeding"
          )

          :ok
        else
          {:error, :tcpdump_startup_timeout}
        end
    end
  end

  defp file_size(path) do
    case File.stat(path) do
      {:ok, %{size: size}} -> size
      {:error, _} -> :missing
    end
  end

  defp try_kill(os_pid) do
    :exec.kill(os_pid, 9)
  catch
    _, _ -> :ok
  end
end
