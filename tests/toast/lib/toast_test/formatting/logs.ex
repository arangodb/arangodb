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

defmodule ToastTest.Formatting.Logs do
  @moduledoc """
  Display formatting for server log output.

  Handles server tags, color assignment, and rendering of merged
  log streams into human-readable output. Data transformation
  (filtering, merging, windowing) lives in `ToastTest.Analyze.Logs`.
  """

  @role_abbrevs %{
    coordinator: "CO",
    dbserver: "DB",
    agent: "AG",
    single: "SNG"
  }

  # Muted 256-color ANSI palettes per role
  @coordinator_colors [67, 103, 110, 66, 109, 60, 68, 102, 146]
  @dbserver_colors [137, 174, 95, 180, 130, 215, 101, 172, 144]
  @agent_colors [101, 138, 66, 144, 96]

  # --- Server tag ---

  @doc "Short uppercase tag for a server. `role` is an atom like `:coordinator`."
  def server_tag(server_id, role) do
    abbrev =
      @role_abbrevs[role] ||
        String.upcase(String.slice(Atom.to_string(role || :""), 0, 3))

    abbrev <> extract_instance_num(server_id)
  end

  # --- Server color ---

  @doc "256-color ANSI code for a role (atom) and instance number."
  def server_color(role, instance_num) do
    palette =
      case role do
        :coordinator -> @coordinator_colors
        :dbserver -> @dbserver_colors
        :single -> @dbserver_colors
        :agent -> @agent_colors
        _ -> @coordinator_colors
      end

    Enum.at(palette, rem(instance_num, length(palette)))
  end

  @doc "Format a line with a colorized server tag prefix."
  def format_tagged_line(server_id, role, line, color_enabled) do
    tag = server_tag(server_id, role)

    if color_enabled do
      color_code = server_color(role, instance_number(server_id))

      [IO.ANSI.color(color_code), "[", tag, "] ", line, IO.ANSI.reset()]
    else
      "[#{tag}] #{line}"
    end
  end

  defp instance_number(server_id) do
    case extract_instance_num(server_id) do
      "" -> 0
      s -> String.to_integer(s)
    end
  end

  def format_event(%{event: :server_started, server_id: sid, pid: pid}),
    do: ">>> server_started #{sid} (pid=#{pid})"

  def format_event(%{event: :server_stopped, server_id: sid}),
    do: ">>> server_stopped #{sid}"

  def format_event(%{event: :server_crashed, server_id: sid, pid: pid, signal: sig}),
    do: ">>> server_crashed #{sid} (pid=#{pid}, signal=#{sig})"

  def format_event(%{event: :server_killed, server_id: sid}),
    do: ">>> server_killed #{sid}"

  def format_event(%{event: :server_unhealthy, server_id: sid}),
    do: ">>> server_unhealthy #{sid}"

  def format_event(%{event: :server_paused, server_id: sid}),
    do: ">>> server_paused #{sid}"

  def format_event(%{event: :server_resumed, server_id: sid}),
    do: ">>> server_resumed #{sid}"

  def format_event(%{event: :test_started, module: mod, name: name}),
    do: ">>> test_started #{inspect(mod)} > #{name}"

  def format_event(%{event: :test_finished, module: mod, name: name, outcome: outcome}),
    do: ">>> test_finished #{inspect(mod)} > #{name} (#{outcome})"

  def format_event(%{event: :module_started, module: mod}),
    do: ">>> module_started #{inspect(mod)}"

  def format_event(%{event: :module_finished, module: mod}),
    do: ">>> module_finished #{inspect(mod)}"

  def format_event(%{event: :deployment_starting, deployment_id: did, mode: mode}),
    do: ">>> deployment_starting #{did} (#{mode})"

  def format_event(%{event: :deployment_started, deployment_id: did}),
    do: ">>> deployment_started #{did}"

  def format_event(%{event: :deployment_stopped, deployment_id: did}),
    do: ">>> deployment_stopped #{did}"

  def format_event(%{event: :timeout_kill, reason: reason}),
    do: ">>> timeout_kill #{reason}"

  def format_event(%{event: :server_identified, server_id: sid, arango_id: aid}),
    do: ">>> server_identified #{sid} => #{aid}"

  def format_event(%{event: :custom, kind: kind, payload: payload}) when payload == %{},
    do: ">>> custom:#{kind}"

  def format_event(%{event: :custom, kind: kind, payload: payload}),
    do: ">>> custom:#{kind} #{format_payload(payload)}"

  def format_event(%{event: name}),
    do: ">>> #{name}"

  defp format_payload(payload) do
    payload
    |> Enum.map_join(" ", fn {k, v} -> "#{k}: #{inspect(v)}" end)
  end

  @doc "Format a single log entry as a human-readable line."
  def format_entry(entry, color_enabled) do
    line = format_entry_line(entry)

    if color_enabled do
      case level_emphasis(entry) do
        "" -> line
        extra -> "#{extra}#{line}\e[0m"
      end
    else
      line
    end
  end

  defp format_entry_line(entry) do
    ts = entry.time |> DateTime.from_unix!(:microsecond) |> DateTime.to_iso8601()
    level = format_level(entry[:level])
    id = if entry[:id], do: " [#{entry.id}]", else: ""
    topic = if entry[:topic], do: " {#{entry.topic}}", else: ""
    pid = if entry[:pid], do: " [#{entry.pid}]", else: ""

    "#{ts}#{pid} #{level}#{id}#{topic} #{entry.message}"
  end

  defp format_level(:fatal), do: "FATAL"
  defp format_level(:error), do: "ERROR"
  defp format_level(:warning), do: "WARN"
  defp format_level(:info), do: "INFO"
  defp format_level(:debug), do: "DEBUG"
  defp format_level(:trace), do: "TRACE"
  defp format_level(_), do: "???"

  defp level_emphasis(%{level: level}) when level in [:error, :fatal], do: IO.ANSI.inverse()
  defp level_emphasis(%{level: :warning}), do: IO.ANSI.bright()
  defp level_emphasis(_), do: ""

  # --- Private helpers ---

  # Extract trailing digits from a server ID as the instance number string.
  # "cluster-00-coordinator-0" -> "0", "single-00" -> "00", "single" -> ""
  defp extract_instance_num(server_id) do
    case Regex.run(~r/(\d+)$/, server_id) do
      [_, num] -> num
      nil -> ""
    end
  end
end
