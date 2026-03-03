defmodule ToastTest.ResultExporter.JSON do
  @moduledoc "Transform test results and diagnostics into a JSON report."

  import Toast.Utils, only: [conditional_put: 3, conditional_put: 4]

  @toast_version "0.1.0"

  @doc "Build a JSON-safe Elixir map from test results and diagnostics."
  @spec build(map(), map() | nil, map() | nil, map() | nil) :: map()
  def build(test_results, diagnostics, sanitizer_matching \\ nil, crash_matching \\ nil) do
    suites = build_suites(test_results.tests)

    base = %{
      "toast_version" => @toast_version,
      "generated_at" => DateTime.to_iso8601(DateTime.utc_now()),
      "test_run" => build_test_run(test_results),
      "summary" => build_summary(test_results.tests),
      "test_suites" => suites,
      "server_health" => build_server_health(diagnostics)
    }

    base
    |> conditional_put("sanitizer_matching", build_sanitizer_matching(sanitizer_matching))
    |> conditional_put("crash_matching", build_crash_matching(crash_matching))
  end

  @doc "Render test results and diagnostics as a JSON string."
  @spec render(map(), map() | nil, map() | nil, map() | nil) :: String.t()
  def render(test_results, diagnostics, sanitizer_matching \\ nil, crash_matching \\ nil) do
    test_results
    |> build(diagnostics, sanitizer_matching, crash_matching)
    |> format_json()
  end

  @doc "Render suite-level results as a JSON string."
  @spec render_suites(map()) :: String.t()
  def render_suites(suite_results) do
    suite_results
    |> build_suites_report()
    |> format_json()
  end

  defp build_suites_report(suite_results) do
    suites =
      Enum.map(suite_results.suites, fn suite ->
        %{
          "name" => suite.name,
          "suite_module" => Atom.to_string(suite.suite_module),
          "deployment_mode" => Atom.to_string(suite.deployment_mode),
          "started_at" => format_datetime(suite.started_at),
          "finished_at" => format_datetime(suite.finished_at),
          "duration_seconds" => us_to_seconds(suite.duration_us),
          "diagnostics" => suite[:diagnostics],
          "tests" => Enum.map(suite.tests, &build_suite_test/1)
        }
      end)

    %{
      "toast_version" => @toast_version,
      "generated_at" => DateTime.to_iso8601(DateTime.utc_now()),
      "started_at" => format_datetime(suite_results.started_at),
      "finished_at" => format_datetime(suite_results.finished_at),
      "duration_seconds" => us_to_seconds(suite_results.global_duration_us),
      "suites" => suites,
      "summary" => build_global_summary(suite_results.summary)
    }
  end

  defp build_suite_test(test) do
    %{
      "module" => Atom.to_string(test.module),
      "name" => test.name,
      "outcome" => Atom.to_string(test.outcome),
      "duration_seconds" => us_to_seconds(test.duration_us)
    }
  end

  defp build_global_summary(summary) do
    %{
      "total" => summary.total,
      "passed" => summary.passed,
      "failed" => summary.failed,
      "skipped" => summary.skipped,
      "errored" => Map.get(summary, :errored, 0)
    }
  end

  defp format_datetime(nil), do: nil
  defp format_datetime(%DateTime{} = dt), do: DateTime.to_iso8601(dt)

  # --- Test run ---

  defp build_test_run(test_results) do
    %{
      "started_at" => DateTime.to_iso8601(test_results.suite_started_at),
      "finished_at" => DateTime.to_iso8601(test_results.suite_finished_at),
      "duration_seconds" => us_to_seconds(test_results.times_us.run)
    }
  end

  # --- Summary ---

  defp build_summary(tests) do
    counts = Enum.frequencies_by(tests, & &1.outcome)

    %{
      "total" => length(tests),
      "passed" => Map.get(counts, :passed, 0),
      "failed" => Map.get(counts, :failed, 0),
      "skipped" => Map.get(counts, :skipped, 0),
      "excluded" => Map.get(counts, :excluded, 0),
      "invalid" => Map.get(counts, :invalid, 0)
    }
  end

  # --- Suites ---

  defp build_suites(tests) do
    tests
    |> Enum.group_by(& &1.module)
    |> Map.new(fn {module, module_tests} ->
      {Atom.to_string(module), build_suite(module_tests)}
    end)
  end

  defp build_suite(tests) do
    %{
      "duration_seconds" => tests |> Enum.map(& &1.duration_us) |> Enum.sum() |> us_to_seconds(),
      "summary" => build_summary(tests),
      "tests" => Enum.map(tests, &build_test/1)
    }
  end

  defp build_test(test) do
    %{
      "name" => test.name,
      "outcome" => Atom.to_string(test.outcome),
      "duration_seconds" => us_to_seconds(test.duration_us),
      "file" => test.tags.file,
      "line" => test.tags.line,
      "failure" => build_failure(test.failure)
    }
  end

  defp build_failure(nil), do: nil

  defp build_failure(%{message: message}) do
    %{"message" => message}
  end

  defp build_failure(failures) when is_list(failures) do
    Enum.map(failures, fn f ->
      %{
        "kind" => f.kind,
        "message" => f.message,
        "stacktrace" => f.stacktrace
      }
    end)
  end

  # --- Server health ---

  defp build_server_health(nil), do: nil

  defp build_server_health(diagnostics) do
    diagnostics
    |> Toast.Diagnostics.to_server_entries()
    |> Map.new(fn {server_id, diag} -> {server_id, build_single_server_health(diag)} end)
  end

  defp build_single_server_health(diag) do
    %{}
    |> conditional_put("sanitizer_errors", diag[:sanitizer_errors], fn errs -> Enum.map(errs, &build_sanitizer_error/1) end)
    |> conditional_put("crash_report", diag[:log_report], &build_crash_report/1)
    |> conditional_put("log_issues", diag[:log_report], &build_log_issues/1)
    |> conditional_put("server", diag[:server], &build_server_instance/1)
  end

  defp build_sanitizer_error(error) do
    %{
      "content" => error.content,
      "file_path" => error.file_path,
      "timestamp" => DateTime.to_iso8601(error.timestamp),
      "sanitizer_type" => Atom.to_string(error.sanitizer_type),
      "server_id" => error.server_id
    }
  end

  defp build_server_instance(server) do
    %{
      "id" => server.id,
      "role" => Atom.to_string(server.role),
      "pid" => server.pid,
      "endpoint" => server.endpoint,
      "log_file" => server.log_file
    }
  end

  defp build_crash_report(report) do
    %{
      "signal_number" => report.signal_number,
      "signal_name" => report.signal_name,
      "crash_header" => report.crash_header,
      "backtrace" => report.backtrace,
      "fatal_lines" => report.fatal_lines,
      "crash_output" => report.crash_output
    }
  end

  defp build_log_issues(log) do
    %{
      "assertion_failures" => Enum.map(log.assertion_failures, &extract_message/1),
      "warnings" => Enum.map(log.warnings, &extract_message/1)
    }
  end

  defp extract_message(%{message: msg}), do: msg
  defp extract_message(msg) when is_binary(msg), do: msg

  # --- Sanitizer matching ---

  defp build_sanitizer_matching(nil), do: nil
  defp build_sanitizer_matching(%{matched: [], unmatched: []}), do: nil

  defp build_sanitizer_matching(%{matched: matched, unmatched: unmatched}) do
    %{
      "matched" => Enum.map(matched, &build_match_entry/1),
      "unmatched" => Enum.map(unmatched, &build_sanitizer_error/1)
    }
  end

  defp build_sanitizer_matching(_), do: nil

  defp build_match_entry(entry) do
    %{
      "module" => Atom.to_string(entry.module),
      "test" => entry.test,
      "confidence" => Atom.to_string(entry.confidence),
      "error" => build_sanitizer_error(entry.error)
    }
  end

  # --- Crash matching ---

  defp build_crash_matching(nil), do: nil
  defp build_crash_matching(%{matched: [], unmatched: []}), do: nil

  defp build_crash_matching(%{matched: matched, unmatched: unmatched}) do
    %{
      "matched" => Enum.map(matched, &build_crash_match_entry/1),
      "unmatched" => Enum.map(unmatched, &build_crash_info/1)
    }
  end

  defp build_crash_matching(_), do: nil

  defp build_crash_match_entry(entry) do
    %{
      "module" => Atom.to_string(entry.module),
      "test" => entry.test,
      "confidence" => Atom.to_string(entry.confidence),
      "crash" => build_crash_info(entry.crash)
    }
  end

  defp build_crash_info(crash) do
    %{
      "server_id" => crash.server_id,
      "signal_name" => crash.signal_name,
      "signal_number" => crash.signal_number,
      "crash_header" => crash.crash_header,
      "backtrace" => crash.backtrace,
      "fatal_lines" => crash.fatal_lines,
      "crash_output" => crash.crash_output,
      "log_file" => crash.log_file,
      "timestamp" => DateTime.to_iso8601(crash.timestamp)
    }
  end

  # --- Helpers ---

  defp us_to_seconds(us), do: us / 1_000_000

  defp format_json(value) do
    value
    |> :json.format(&json_encoder/3, %{})
    |> IO.iodata_to_binary()
  end

  defp json_encoder(value, encoder, opts) when is_atom(value) and not is_boolean(value) do
    if is_nil(value) do
      "null"
    else
      value |> Atom.to_string() |> encoder.(encoder, opts)
    end
  end

  defp json_encoder(value, encoder, opts), do: :json.format_value(value, encoder, opts)
end
