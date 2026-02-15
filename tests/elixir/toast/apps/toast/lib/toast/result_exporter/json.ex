defmodule Toast.ResultExporter.JSON do
  @moduledoc "Transform test results and diagnostics into a JSON report."

  @toast_version "0.1.0"

  @doc "Build a JSON-safe Elixir map from test results and diagnostics."
  @spec build(map(), map() | nil) :: map()
  def build(test_results, diagnostics) do
    suites = build_suites(test_results.tests)

    %{
      "toast_version" => @toast_version,
      "generated_at" => DateTime.to_iso8601(DateTime.utc_now()),
      "test_run" => build_test_run(test_results),
      "summary" => build_summary(test_results.tests),
      "test_suites" => suites,
      "server_health" => build_server_health(diagnostics)
    }
  end

  @doc "Render test results and diagnostics as a pretty-printed JSON string."
  @spec render(map(), map() | nil) :: String.t()
  def render(test_results, diagnostics) do
    test_results
    |> build(diagnostics)
    |> format_json()
  end

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
    counts = count_outcomes(tests)

    %{
      "total" => length(tests),
      "passed" => Map.get(counts, :passed, 0),
      "failed" => Map.get(counts, :failed, 0),
      "skipped" => Map.get(counts, :skipped, 0),
      "excluded" => Map.get(counts, :excluded, 0),
      "invalid" => Map.get(counts, :invalid, 0)
    }
  end

  defp count_outcomes(tests) do
    Enum.frequencies_by(tests, & &1.outcome)
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
    if Toast.ResultExporter.cluster_diagnostics?(diagnostics) do
      Map.new(diagnostics, fn {server_id, diag} ->
        {server_id, build_single_server_health(diag)}
      end)
    else
      build_single_server_health(diagnostics)
    end
  end

  defp build_single_server_health(diag) do
    [
      health_entry("sanitizer_errors", diag[:sanitizer_errors], &Enum.map(&1, fn e -> build_sanitizer_error(e) end)),
      health_entry("crash_report", diag[:crash_report], &build_crash_report/1),
      health_entry("log_issues", diag[:server_log], &build_log_issues/1)
    ]
    |> Enum.reject(&is_nil/1)
    |> Map.new()
  end

  defp health_entry(_key, nil, _transform), do: nil
  defp health_entry(key, value, transform), do: {key, transform.(value)}

  defp build_sanitizer_error(error) do
    %{
      "content" => error.content,
      "file_path" => error.file_path,
      "timestamp" => DateTime.to_iso8601(error.timestamp),
      "sanitizer_type" => Atom.to_string(error.sanitizer_type),
      "server_id" => error.server_id
    }
  end

  defp build_crash_report(report) do
    %{
      "signal_number" => report.signal_number,
      "signal_name" => report.signal_name,
      "crash_header" => report.crash_header,
      "backtrace" => report.backtrace,
      "fatal_lines" => report.fatal_lines
    }
  end

  defp build_log_issues(log) do
    %{
      "assertion_failures" => log.assertion_failures,
      "warnings" => log.warnings
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
