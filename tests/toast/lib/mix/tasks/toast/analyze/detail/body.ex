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

defmodule Mix.Tasks.Toast.Analyze.Detail.Body do
  @moduledoc false

  import ToastTest.Formatting, only: [colorize: 3, formatter_cb: 2]

  alias Mix.Tasks.Toast.Analyze.Data
  alias ToastTest.Formatting.{Backtrace, Issues}
  alias Toast.Diagnostics.Coredump.ThreadFilter

  def print(issue, color, bt_opts \\ %{})

  def print(
        %{type: :test_failure, detail: %{test: %ExUnit.Test{state: {:failed, failures}} = test}},
        _color,
        _bt_opts
      ) do
    formatted =
      ExUnit.Formatter.format_test_failure(
        test,
        failures,
        1,
        :infinity,
        &formatter_cb/2
      )

    Mix.shell().info("\n#{formatted}")
  end

  def print(%{type: :sanitizer_report, detail: detail}, _color, _bt_opts) do
    if detail[:timestamp] do
      Mix.shell().info("  Time:   #{Data.fmt_dt(detail.timestamp)}")
    end

    Mix.shell().info("")
    Mix.shell().info(detail.report)
  end

  def print(%{type: :crash, detail: detail}, color, bt_opts) do
    print_crash_info(detail, color)
    print_crash_backtrace(detail, color, bt_opts)
    print_crash_extra(detail, color, bt_opts)
  end

  def print(
        %{type: :infrastructure, detail: %{subtype: :timeout} = detail},
        color,
        _bt_opts
      ) do
    label = Issues.timeout_source_label(detail.source)
    Mix.shell().info("  #{colorize("[#{label}] #{detail.reason}", :red, color)}")

    if detail[:timestamp] do
      Mix.shell().info("  Time:   #{Data.fmt_dt(detail.timestamp)}")
    end

    if detail.servers != [] do
      Mix.shell().info("")
      Enum.each(detail.servers, &print_timeout_server(&1, color))
    end
  end

  def print(
        %{type: :infrastructure, detail: %{subtype: :port_exhaustion} = detail} = issue,
        color,
        _bt_opts
      ) do
    Mix.shell().info("  #{colorize("PORT EXHAUSTION", :red, color)}")

    if detail[:timestamp] do
      Mix.shell().info("  Time:   #{Data.fmt_dt(detail.timestamp)}")
    end

    kind = if detail[:kind] == :deployment, do: "deployment", else: "system"

    Mix.shell().info(
      "  Total:  #{colorize("#{detail.total} #{kind} sockets", :red, color)} (threshold: #{detail.threshold})"
    )

    if detail[:kind] == :deployment do
      Mix.shell().info(
        "  Delta:  #{detail.deployment_delta} sockets since deployment (baseline: #{detail.baseline})"
      )
    end

    Mix.shell().info("")
    Mix.shell().info(colorize("  Per-server breakdown (by process ownership):", :bright, color))

    sorted = Enum.sort_by(detail.by_server, fn {_, v} -> -v.sockets.total end)

    server_total =
      Enum.reduce(sorted, 0, fn {server_id, server}, acc ->
        Mix.shell().info("    #{colorize(server_id, :cyan, color)} (port #{server.port})")

        Mix.shell().info("      total  #{server.sockets.total}")
        print_direction("← in ", server.sockets.in, color)
        print_direction("→ out", server.sockets.out, color)

        acc + server.sockets.total
      end)

    rest = detail.total - server_total

    if rest > 0 do
      Mix.shell().info("    #{colorize("(other)", :faint, color)}  #{rest}")
    end

    print_netstat_trajectory(issue, color)
  end

  def print(%{type: :infrastructure, detail: %{subtype: subtype} = detail}, color, _bt_opts) do
    label = subtype |> to_string() |> String.replace("_", " ") |> String.upcase()
    Mix.shell().info("  #{colorize(label, :red, color)}")

    if detail[:timestamp] do
      Mix.shell().info("  Time:   #{Data.fmt_dt(detail.timestamp)}")
    end

    Mix.shell().info("  Detail: #{inspect(Map.drop(detail, [:subtype, :timestamp]))}")
  end

  defp print_direction(_label, stats, _color) when stats == %{}, do: :ok

  defp print_direction(label, stats, color) do
    formatted =
      stats
      |> Enum.sort_by(fn {_, n} -> -n end)
      |> Enum.map_join(", ", fn {state, n} -> "#{n} #{state}" end)

    Mix.shell().info("      #{colorize(label, :faint, color)}  #{formatted}")
  end

  @trajectory_limit 10

  defp print_netstat_trajectory(%{detail: %{timestamp: issue_ts}} = issue, color)
       when is_integer(issue_ts) do
    events = issue[:events] || []

    snapshots =
      events
      |> Enum.filter(&(&1.event == :netstat_snapshot))
      |> Enum.sort_by(& &1.timestamp)

    if snapshots != [] do
      entries = annotate_snapshots(snapshots, issue_ts, issue[:modules] || %{})

      print_top_contributors(entries, color)
      print_recent_trajectory(entries, length(snapshots), color)
    end
  end

  defp print_netstat_trajectory(_detail, _color), do: :ok

  @baseline_labels %{
    pre_deployment: "(pre-deployment)",
    deployment_ready: "(deployment ready)"
  }

  defp annotate_snapshots(snapshots, issue_ts, modules) do
    timeline = build_test_timeline(modules)

    {entries, _} =
      Enum.reduce(snapshots, {[], {0, timeline}}, fn snap, {acc, {prev, remaining}} ->
        delta = snap.total - prev
        marker = if snap.timestamp == issue_ts, do: :threshold, else: nil

        {label, remaining} =
          case Map.get(@baseline_labels, snap[:label]) do
            nil -> advance_timeline(snap.timestamp, remaining)
            baseline_label -> {baseline_label, remaining}
          end

        entry = %{
          total: snap.total,
          delta: delta,
          test: label,
          marker: marker,
          baseline: snap[:label] != nil
        }

        {[entry | acc], {snap.total, remaining}}
      end)

    Enum.reverse(entries)
  end

  defp print_top_contributors(entries, color) do
    top =
      entries
      |> Enum.filter(&(&1.delta > 0 and not &1.baseline))
      |> Enum.sort_by(& &1.delta, :desc)
      |> Enum.take(@trajectory_limit)

    if top != [] do
      Mix.shell().info("")
      Mix.shell().info(colorize("  Top contributors (by socket increase):", :bright, color))
      Enum.each(top, &print_trajectory_entry(&1, color))
    end
  end

  defp print_recent_trajectory(entries, total_count, color) do
    {baselines, test_entries} = Enum.split_while(entries, & &1.baseline)
    recent = Enum.take(test_entries, -@trajectory_limit)
    skipped = total_count - length(baselines) - length(recent)

    Mix.shell().info("")
    Mix.shell().info(colorize("  Recent socket count trajectory:", :bright, color))

    Enum.each(baselines, &print_trajectory_entry(&1, color))

    if skipped > 0 do
      Mix.shell().info(colorize("    ... #{skipped} earlier entries omitted", :faint, color))
    end

    Enum.each(recent, &print_trajectory_entry(&1, color))
  end

  defp print_trajectory_entry(entry, color) do
    delta_str = if entry.delta >= 0, do: "+#{entry.delta}", else: "#{entry.delta}"
    marker = if entry.marker == :threshold, do: " ← THRESHOLD", else: ""

    Mix.shell().info(
      "    #{colorize(String.pad_leading("#{entry.total}", 6), :bright, color)}  #{colorize(String.pad_leading(delta_str, 7), :faint, color)}  #{entry.test || "?"}#{colorize(marker, :red, color)}"
    )
  end

  defp build_test_timeline(modules) do
    for {mod, %{tests: tests}} <- modules,
        %{finished_at: %DateTime{} = finished_at} = test <- tests do
      {DateTime.to_unix(finished_at, :microsecond), "#{inspect(mod)}.#{test.name}"}
    end
    |> Enum.sort_by(&elem(&1, 0))
  end

  defp advance_timeline(snapshot_ts, timeline) do
    {passed, remaining} =
      Enum.split_while(timeline, fn {finished_us, _} -> finished_us <= snapshot_ts end)

    test_name = if passed == [], do: nil, else: passed |> List.last() |> elem(1)
    {test_name, remaining}
  end

  defp print_timeout_server(server, color) do
    pid_part = if server.os_pid, do: " (PID #{server.os_pid})", else: ""
    Mix.shell().info("  #{colorize("#{server.server_id}#{pid_part}", :cyan, color)}")

    if server.log_file do
      Mix.shell().info(colorize("    Log: #{server.log_file}", :blue, color))
    end

    if server[:coredump] do
      Mix.shell().info(colorize("    Coredump: #{server.coredump}", :blue, color))
    end
  end

  defp print_crash_info(%{crash_info: info}, color) do
    parts =
      [
        if(info.os_pid, do: "PID #{info.os_pid}"),
        Issues.format_signal(info.signal),
        if(info.exit_status, do: "exit_status: #{info.exit_status}"),
        if(match?(%DateTime{}, info.timestamp), do: "at: #{DateTime.to_iso8601(info.timestamp)}")
      ]
      |> Toast.Utils.compact()

    if parts != [] do
      Mix.shell().info("  #{colorize(Enum.join(parts, "  "), :red, color)}")
    end
  end

  defp print_crash_info(_detail, _color), do: :ok

  defp print_crash_backtrace(_detail, _color, %{coredumps: false}), do: :ok

  defp print_crash_backtrace(
         %{coredumps: [coredump | _]} = detail,
         color,
         %{threads: mode} = bt_opts
       )
       when mode in [:relevant, :all] do
    if is_binary(detail[:crash_lines]) do
      Mix.shell().info("")
      Mix.shell().info(detail.crash_lines)
    end

    threads = coredump.threads || []
    threads = if mode == :all, do: threads, else: ThreadFilter.filter_relevant(threads, coredump)

    if threads != [] do
      Mix.shell().info("")

      threads
      |> Enum.intersperse(:separator)
      |> Enum.each(fn
        :separator -> Mix.shell().info("")
        thread -> print_thread(thread, bt_opts.max_frames, color)
      end)
    end
  end

  defp print_crash_backtrace(%{crash_lines: crash_lines}, _color, _bt_opts)
       when is_binary(crash_lines) do
    Mix.shell().info("")
    Mix.shell().info(crash_lines)
  end

  defp print_crash_backtrace(%{coredumps: [coredump | _]}, _color, _bt_opts) do
    case Issues.format_coredump_backtrace(coredump) do
      nil -> :ok
      text -> Mix.shell().info("\n#{text}")
    end
  end

  defp print_crash_backtrace(_detail, color, _bt_opts) do
    Mix.shell().info("  #{colorize("No crash details available.", :faint, color)}")
  end

  defp print_crash_extra(%{coredumps: [coredump | _]}, color, %{disassembly: true}) do
    print_optional_section(Issues.format_registers(coredump), "Registers", color)
    print_optional_section(Issues.format_disassembly(coredump), "Disassembly", color)
  end

  defp print_crash_extra(_detail, _color, _bt_opts), do: :ok

  defp print_optional_section(nil, _label, _color), do: :ok

  defp print_optional_section(text, label, color) do
    Mix.shell().info("")
    Mix.shell().info(colorize("#{label}:", :blue, color))
    Mix.shell().info(text)
  end

  defp print_thread(thread, max_frames, color) do
    os_part = if thread[:os_id], do: " (LWP #{thread.os_id})", else: ""
    header = "Thread #{thread.id}#{os_part}:"
    Mix.shell().info(colorize(header, :blue, color))

    frames = thread[:frames] || []
    shown = Enum.take(frames, max_frames)
    remaining = length(frames) - length(shown)

    backtrace = Backtrace.format_backtrace(shown)
    Mix.shell().info(backtrace)
    if remaining > 0, do: Mix.shell().info("  ... (#{remaining} more frames)")
  end
end
