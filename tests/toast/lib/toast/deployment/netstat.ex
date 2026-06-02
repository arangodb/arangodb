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

defmodule Toast.Deployment.Netstat do
  @moduledoc false

  alias Toast.Deployment

  require Logger

  @system_threshold 15_000
  @per_server_budget 500

  @type direction_stats :: %{String.t() => non_neg_integer()}

  @type server_snapshot :: %{
          pid: non_neg_integer(),
          sockets: %{
            total: non_neg_integer(),
            in: direction_stats(),
            out: direction_stats()
          }
        }

  @type snapshot :: %{String.t() => server_snapshot()}

  @type exhaustion_detail :: %{
          total: non_neg_integer(),
          threshold: non_neg_integer(),
          kind: :system | :deployment,
          baseline: non_neg_integer(),
          deployment_delta: non_neg_integer(),
          by_server: snapshot()
        }

  @type tool :: :ss | :netstat

  @spec system_threshold() :: non_neg_integer()
  def system_threshold, do: @system_threshold

  @spec per_server_budget() :: non_neg_integer()
  def per_server_budget, do: @per_server_budget

  @doc """
  Detect which socket tool is available. Returns `:ss`, `:netstat`, or `nil`.
  """
  @spec detect_tool() :: tool() | nil
  def detect_tool do
    cond do
      System.find_executable("ss") -> :ss
      System.find_executable("netstat") -> :netstat
      true -> nil
    end
  end

  @doc """
  Check system socket count against port exhaustion thresholds.

  Checks two thresholds:
  - System threshold (#{@system_threshold}): total sockets on the system
  - Deployment threshold (#{@per_server_budget} per server): sockets added
    since baseline, scaled by server count to catch leaks early

  Returns `{:ok, total}` when below both thresholds, or
  `{:port_exhaustion, detail}` with a per-server breakdown when either is
  reached. The per-server breakdown requires an additional call with PID
  resolution (~40ms) that is only made when a threshold is reached.
  """
  @spec check(Deployment.t(), tool(), non_neg_integer()) ::
          {:ok, non_neg_integer()} | {:port_exhaustion, exhaustion_detail()}
  def check(%Deployment{} = deployment, tool, baseline) do
    total = count_sockets(tool)
    deployment_delta = total - baseline
    server_count = map_size(deployment.servers)
    deployment_threshold = server_count * @per_server_budget

    {exceeded, threshold} =
      cond do
        total >= @system_threshold ->
          {:system, @system_threshold}

        deployment_threshold > 0 and deployment_delta >= deployment_threshold ->
          {:deployment, deployment_threshold}

        true ->
          {nil, nil}
      end

    if exceeded do
      by_server = detailed_snapshot(deployment, tool)

      {:port_exhaustion,
       %{
         total: total,
         threshold: threshold,
         kind: exceeded,
         baseline: baseline,
         deployment_delta: deployment_delta,
         by_server: by_server
       }}
    else
      {:ok, total}
    end
  end

  @doc """
  Count all non-LISTEN TCP sockets on the system (~2ms with ss, ~12ms with netstat).
  """
  @spec count_sockets(tool()) :: non_neg_integer()
  def count_sockets(tool) do
    case run_count(tool) do
      {:ok, output} ->
        count_lines(output)

      {:error, reason} ->
        Logger.warning("Netstat: failed to count sockets: #{inspect(reason)}")
        0
    end
  end

  @netstat_header_lines 2

  defp run_count(:ss), do: run_cmd("ss", ["-tnH"])

  defp run_count(:netstat) do
    case run_cmd("netstat", ["-tn"]) do
      {:ok, output} -> {:ok, drop_lines(output, @netstat_header_lines)}
      error -> error
    end
  end

  # --- Slow path: PID-based snapshot (~40ms ss, ~55ms netstat) ---

  defp detailed_snapshot(%Deployment{} = deployment, tool) do
    servers =
      deployment
      |> Deployment.server_instances()
      |> Enum.filter(&(&1.operational_state == :running and &1.pid != nil))
      |> Enum.map(&%{id: &1.id, pid: &1.pid, port: &1.port})

    case run_detail(tool) do
      {:ok, output} ->
        build_snapshot_by_pid(output, servers, tool)

      {:error, reason} ->
        Logger.warning("Netstat: failed to gather detailed snapshot: #{inspect(reason)}")
        empty = %{total: 0, in: %{}, out: %{}}
        Map.new(servers, &{&1.id, %{pid: &1.pid, sockets: empty}})
    end
  end

  defp run_detail(:ss), do: run_cmd("ss", ["-tnpH"])

  defp run_detail(:netstat) do
    case run_cmd("netstat", ["-tnp"]) do
      {:ok, output} -> {:ok, drop_lines(output, @netstat_header_lines)}
      error -> error
    end
  end

  # --- Internals ---

  defp run_cmd(cmd, args) do
    case System.cmd(cmd, args, stderr_to_stdout: true) do
      {output, 0} -> {:ok, output}
      {output, code} -> {:error, "#{cmd} exited with #{code}: #{output}"}
    end
  end

  defp drop_lines(output, 0), do: output

  defp drop_lines(output, n) do
    case :binary.match(output, "\n") do
      {pos, 1} -> drop_lines(:binary.part(output, pos + 1, byte_size(output) - pos - 1), n - 1)
      :nomatch -> ""
    end
  end

  @doc false
  @spec count_lines(String.t()) :: non_neg_integer()
  def count_lines(""), do: 0

  def count_lines(output) do
    count = length(:binary.matches(output, "\n"))
    if String.ends_with?(output, "\n"), do: count, else: count + 1
  end

  @pid_pattern_ss ~r/pid=(\d+)/
  @pid_pattern_netstat ~r/^\s*(\d+)\//

  @doc false
  @spec build_snapshot_by_pid(
          String.t(),
          [%{id: String.t(), pid: non_neg_integer(), port: non_neg_integer()}],
          tool()
        ) :: snapshot()
  def build_snapshot_by_pid(output, servers, tool \\ :ss) do
    pid_to_server = Map.new(servers, &{&1.pid, &1})

    grouped =
      output
      |> String.split("\n", trim: true)
      |> Enum.flat_map(&parse_detail_line(&1, tool))
      |> Enum.filter(&Map.has_key?(pid_to_server, &1.pid))
      |> Enum.group_by(&Map.fetch!(pid_to_server, &1.pid).id)

    Map.new(servers, fn server ->
      sockets = Map.get(grouped, server.id, [])

      {inbound, outbound} =
        Enum.split_with(sockets, &(&1.local_port == server.port))

      {server.id,
       %{
         pid: server.pid,
         sockets: %{
           total: length(sockets),
           in: Enum.frequencies_by(inbound, & &1.state),
           out: Enum.frequencies_by(outbound, & &1.state)
         }
       }}
    end)
  end

  defp parse_detail_line(line, tool) do
    fields = String.split(line, ~r/\s+/, trim: true)

    with {:ok, state, local, pid_text} <- extract_fields(fields, tool),
         {:ok, pid} <- extract_pid(pid_text, pid_pattern(tool)),
         {:ok, local_port} <- extract_port(local) do
      [%{state: state, pid: pid, local_port: local_port}]
    else
      _ -> []
    end
  end

  # ss: STATE Recv-Q Send-Q Local:Port Remote:Port users:((...pid=PID...))
  defp extract_fields([state, _, _, local, _ | rest], :ss),
    do: {:ok, state, local, Enum.join(rest, " ")}

  # netstat: Proto Recv-Q Send-Q Local:Port Remote:Port State PID/Program
  defp extract_fields([_, _, _, local, _, state, pid_prog | _], :netstat),
    do: {:ok, state, local, pid_prog}

  defp extract_fields(_, _), do: :error

  defp pid_pattern(:ss), do: @pid_pattern_ss
  defp pid_pattern(:netstat), do: @pid_pattern_netstat

  defp extract_port(addr) do
    case addr |> String.split(":") |> List.last() do
      nil ->
        :error

      port_str ->
        case Integer.parse(port_str) do
          {port, ""} -> {:ok, port}
          _ -> :error
        end
    end
  end

  defp extract_pid(text, pattern) do
    case Regex.run(pattern, text) do
      [_, pid_str] ->
        case Integer.parse(pid_str) do
          {pid, ""} -> {:ok, pid}
          _ -> :error
        end

      _ ->
        :error
    end
  end
end
