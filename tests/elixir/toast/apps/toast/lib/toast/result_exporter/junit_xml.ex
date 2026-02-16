defmodule Toast.ResultExporter.JUnitXML do
  @moduledoc "Transform test results and diagnostics into JUnit XML format."

  @doc "Render test results and diagnostics as a JUnit XML string."
  @spec render(map(), map() | nil) :: String.t()
  def render(test_results, diagnostics) do
    suites = group_by_module(test_results.tests)
    all_tests = test_results.tests

    total = length(all_tests)
    failures = count_by_outcome(all_tests, :failed)
    errors = count_by_outcome(all_tests, :invalid)
    skipped = count_by_outcome(all_tests, :skipped) + count_by_outcome(all_tests, :excluded)
    time = format_duration(test_results.times_us.run)

    suite_elements = Enum.map_join(suites, "\n", &render_testsuite/1)
    system_err = render_system_err(diagnostics)

    [
      ~s(<?xml version="1.0" encoding="UTF-8"?>),
      ~s(<testsuites name="toast" tests="#{total}" failures="#{failures}" errors="#{errors}" skipped="#{skipped}" time="#{time}">),
      suite_elements,
      system_err,
      ~s(</testsuites>)
    ]
    |> Enum.reject(&(&1 == ""))
    |> Enum.join("\n")
  end

  # --- Test suites ---

  defp group_by_module(tests), do: Enum.group_by(tests, & &1.module)

  defp render_testsuite({module, tests}) do
    name = Atom.to_string(module)
    total = length(tests)
    failures = count_by_outcome(tests, :failed)
    errors = count_by_outcome(tests, :invalid)
    skipped = count_by_outcome(tests, :skipped) + count_by_outcome(tests, :excluded)
    time = tests |> Enum.map(& &1.duration_us) |> Enum.sum() |> format_duration()

    cases = Enum.map_join(tests, "\n", &render_testcase(&1, name))

    [
      ~s(  <testsuite name="#{xml_escape(name)}" tests="#{total}" failures="#{failures}" errors="#{errors}" skipped="#{skipped}" time="#{time}">),
      cases,
      ~s(  </testsuite>)
    ]
    |> Enum.join("\n")
  end

  # --- Test cases ---

  defp render_testcase(test, classname) do
    name = xml_escape(test.name)
    cn = xml_escape(classname)
    time = format_duration(test.duration_us)

    case test.outcome do
      :passed ->
        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}"/>)

      :failed ->
        child = render_failure(test.failure)
        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}">\n#{child}\n    </testcase>)

      :invalid ->
        child = render_error(test.failure)
        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}">\n#{child}\n    </testcase>)

      outcome when outcome in [:skipped, :excluded] ->
        child = render_skipped(test.failure)
        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}">\n#{child}\n    </testcase>)
    end
  end

  defp render_failure(failures) when is_list(failures) do
    Enum.map_join(failures, "\n", fn f ->
      message = xml_escape(f.message)
      type = xml_escape(f.kind)
      stacktrace = xml_escape(f.stacktrace)
      ~s(      <failure message="#{message}" type="#{type}">#{stacktrace}</failure>)
    end)
  end

  defp render_failure(%{message: message}) do
    ~s(      <failure message="#{xml_escape(message)}"/>)
  end

  defp render_failure(nil) do
    ~s(      <failure/>)
  end

  defp render_error(%{message: message}) do
    ~s(      <error message="#{xml_escape(message)}" type="RuntimeError"/>)
  end

  defp render_error(failures) when is_list(failures) do
    Enum.map_join(failures, "\n", fn f ->
      message = xml_escape(f.message)
      type = xml_escape(f.kind)
      ~s(      <error message="#{message}" type="#{type}"/>)
    end)
  end

  defp render_error(nil) do
    ~s(      <error message="setup_all failed" type="RuntimeError"/>)
  end

  defp render_skipped(%{message: message}) do
    ~s(      <skipped message="#{xml_escape(message)}"/>)
  end

  defp render_skipped(nil) do
    ~s(      <skipped/>)
  end

  # --- system-err for diagnostics ---

  defp render_system_err(nil), do: ""

  defp render_system_err(diagnostics) do
    text = format_diagnostics(diagnostics)

    if text == "" do
      ""
    else
      ~s(<system-err>#{xml_cdata(text)}</system-err>)
    end
  end

  defp format_diagnostics(diagnostics) do
    if Toast.ResultExporter.cluster_diagnostics?(diagnostics) do
      diagnostics
      |> Enum.sort_by(fn {server_id, _} -> server_id end)
      |> Enum.map_join("\n", fn {server_id, diag} ->
        section = format_single_diagnostics(diag)

        if section == "" do
          ""
        else
          "[#{server_id}]\n#{section}"
        end
      end)
      |> String.trim()
    else
      format_single_diagnostics(diagnostics)
    end
  end

  defp format_single_diagnostics(diag) do
    [
      format_sanitizer_section(Map.get(diag, :sanitizer_errors)),
      format_crash_section(Map.get(diag, :crash_report)),
      format_log_section(Map.get(diag, :server_log))
    ]
    |> Enum.reject(&is_nil/1)
    |> Enum.join("\n\n")
  end

  defp format_sanitizer_section(nil), do: nil
  defp format_sanitizer_section([]), do: nil

  defp format_sanitizer_section(errors) do
    sections =
      Enum.map_join(errors, "\n\n", fn e ->
        "--- #{e.file_path} (#{e.sanitizer_type}) ---\n#{e.content}"
      end)

    "Sanitizer Errors:\n#{sections}"
  end

  defp format_crash_section(nil), do: nil
  defp format_crash_section(%{signal_name: nil}), do: nil

  defp format_crash_section(report) do
    "Crash Report:\n  Signal: #{report.signal_name} (#{report.signal_number})\n  #{report.crash_header}"
  end

  defp format_log_section(nil), do: nil

  defp format_log_section(log) do
    a_count = length(log.assertion_failures)
    w_count = length(log.warnings)

    if a_count > 0 or w_count > 0 do
      "Log Issues:\n  Assertions: #{a_count}\n  Warnings: #{w_count}"
    end
  end

  # --- Helpers ---

  defp count_by_outcome(tests, outcome) do
    Enum.count(tests, &(&1.outcome == outcome))
  end

  defp format_duration(us) do
    :erlang.float_to_binary(us / 1_000_000, decimals: 3)
  end

  defp xml_escape(text) when is_binary(text) do
    text
    |> String.replace("&", "&amp;")
    |> String.replace("<", "&lt;")
    |> String.replace(">", "&gt;")
    |> String.replace("\"", "&quot;")
    |> String.replace("'", "&apos;")
  end

  defp xml_escape(nil), do: ""

  defp xml_cdata(text) when is_binary(text) do
    escaped = String.replace(text, "]]>", "]]]]><![CDATA[>")
    "<![CDATA[#{escaped}]]>"
  end
end
