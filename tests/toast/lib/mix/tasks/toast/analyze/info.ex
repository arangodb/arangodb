defmodule Mix.Tasks.Toast.Analyze.Info do
  @moduledoc false

  import ToastTest.Formatting, only: [colorize: 3]

  alias Mix.Tasks.Toast.Analyze.Data

  def run(result_dir, opts, color) do
    results = Data.load_results(result_dir)

    results
    |> Data.maybe_filter_suite(opts[:suite])
    |> Enum.each(&print_suite_info(&1, color))
  end

  defp print_suite_info(result, color) do
    bar = String.duplicate("═", 80)
    Mix.shell().info("\n#{colorize(bar, :cyan, color)}")
    Mix.shell().info(colorize(" Suite: #{result.suite}", :bright, color))
    Mix.shell().info(colorize(bar, :cyan, color))

    # Time range
    started = if result.started_at, do: Data.fmt_dt(result.started_at), else: "?"
    finished = if result.finished_at, do: Data.fmt_dt(result.finished_at), else: "?"
    Mix.shell().info("  Time:    #{started} .. #{finished}")

    if result.times_us do
      run_s = Float.round((result.times_us[:run] || 0) / 1_000_000, 1)
      Mix.shell().info("  Runtime: #{run_s}s")
    end

    # Version
    Mix.shell().info("  Version: #{Map.get(result, :version, "?")}")

    # Modules & tests
    test_counts = count_tests(result.modules)

    Mix.shell().info(
      "  Modules: #{map_size(result.modules)}  Tests: #{test_counts.total} " <>
        "(#{test_counts.passed} passed, #{test_counts.failed} failed, " <>
        "#{test_counts.skipped} skipped)"
    )

    # Issues
    issue_counts =
      result.issues
      |> Enum.frequencies_by(& &1.type)

    Mix.shell().info("  Issues:  #{length(result.issues)} #{format_freq(issue_counts)}")

    # Events
    Mix.shell().info("  Events:  #{length(Map.get(result, :events, []))}")

    # Warnings
    warnings = Map.get(result, :warnings, [])

    if warnings != [] do
      Mix.shell().info("  Warnings: #{length(warnings)}")
    end

    # Deployments & servers
    deployments = Map.get(result, :deployments, %{})
    Mix.shell().info("")
    Mix.shell().info(colorize("  Deployments (#{map_size(deployments)}):", :bright, color))

    deployments
    |> Enum.sort_by(fn {did, _} -> did end)
    |> Enum.each(&print_deployment_info(&1, color))

    # Coredump data
    print_coredump_data(Map.get(result, :coredumps, []), color)
  end

  defp print_deployment_info({did, deployment}, color) do
    mode = if deployment[:mode], do: " (#{deployment.mode})", else: ""
    Mix.shell().info("")
    Mix.shell().info("    #{colorize("#{did}#{mode}", :cyan, color)}")

    if deployment[:started_at] do
      stopped =
        if deployment[:stopped_at], do: Data.fmt_dt(deployment.stopped_at), else: "running"

      Mix.shell().info("      Time: #{Data.fmt_dt(deployment.started_at)} .. #{stopped}")
    end

    deployment
    |> Map.get(:servers, %{})
    |> Enum.sort_by(fn {sid, _} -> sid end)
    |> Enum.each(fn {sid, meta} -> print_server_info(sid, meta, color) end)
  end

  defp print_coredump_data([], _color), do: :ok

  defp print_coredump_data(coredumps, color) do
    Mix.shell().info("")
    Mix.shell().info(colorize("  Coredumps (#{length(coredumps)}):", :bright, color))

    Enum.each(coredumps, fn cd ->
      threads = cd[:threads] || []
      thread_count = length(threads)
      debugger = if cd[:debugger], do: " (#{cd.debugger})", else: ""
      Mix.shell().info("")

      Mix.shell().info(
        "    #{colorize("#{cd.server_id}", :red, color)}  #{cd.core_path}#{debugger}"
      )

      summary =
        [
          if(cd[:signal], do: "signal: #{cd.signal}"),
          "#{thread_count} thread(s)",
          if(cd[:faulting_address], do: "fault addr: #{cd.faulting_address}"),
          if(cd[:crash_thread], do: "crash thread: #{cd.crash_thread}")
        ]
        |> Toast.Utils.compact()
        |> Enum.join(", ")

      Mix.shell().info("      #{summary}")
    end)
  end

  defp print_server_info(sid, meta, color) do
    role = if meta[:role], do: " (#{meta.role})", else: ""
    Mix.shell().info("      #{colorize("#{sid}#{role}", :cyan, color)}")

    parts = server_detail_parts(meta)

    if parts != [] do
      Mix.shell().info("        #{Enum.join(parts, "  ")}")
    end

    Enum.each(meta[:incarnations] || [], &print_incarnation/1)

    # Collected log windows
    logs = meta[:logs] || []
    total_entries = Enum.reduce(logs, 0, fn {_, _, entries}, acc -> acc + length(entries) end)

    Mix.shell().info("        Logs: #{length(logs)} window(s), #{total_entries} entries")
  end

  defp server_detail_parts(meta) do
    []
    |> maybe_append_part(meta[:endpoint], "endpoint")
    |> maybe_append_part(meta[:arango_id], "arango")
    |> maybe_append_part(meta[:log_file], "log")
  end

  defp maybe_append_part(parts, value, label) when is_binary(value),
    do: parts ++ ["#{label}=#{value}"]

  defp maybe_append_part(parts, _value, _label), do: parts

  defp print_incarnation(inc) do
    stopped = if inc[:stopped_at], do: Data.fmt_dt(inc.stopped_at), else: "running"
    Mix.shell().info("        pid=#{inc.pid}  #{Data.fmt_dt(inc.started_at)} .. #{stopped}")
  end

  defp count_tests(modules) do
    zero = %{total: 0, passed: 0, failed: 0, skipped: 0}

    Enum.reduce(modules, zero, fn {_mod, data}, acc ->
      Enum.reduce(data.tests, acc, &tally_test/2)
    end)
  end

  defp tally_test(test, acc) do
    acc = %{acc | total: acc.total + 1}

    case test.outcome do
      :passed -> %{acc | passed: acc.passed + 1}
      :failed -> %{acc | failed: acc.failed + 1}
      outcome when outcome in [:skipped, :excluded] -> %{acc | skipped: acc.skipped + 1}
      _ -> acc
    end
  end

  defp format_freq(freq) when map_size(freq) == 0, do: ""

  defp format_freq(freq) do
    parts = Enum.map_join(freq, ", ", fn {type, count} -> "#{count} #{type}" end)
    "(#{parts})"
  end
end
