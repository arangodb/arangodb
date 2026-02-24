defmodule ToastTest.ResultExporter.JUnitXML do
  @moduledoc "Transform test results and diagnostics into JUnit XML format."

  alias Toast.Diagnostics.Matcher

  @doc "Render test results and diagnostics as a JUnit XML string."
  @spec render(map(), map() | nil, map() | nil, map() | nil) :: String.t()
  def render(test_results, diagnostics, sanitizer_matching \\ nil, crash_matching \\ nil) do
    suites = group_by_module(test_results.tests)
    all_tests = test_results.tests

    total = length(all_tests)
    failures = count_by_outcome(all_tests, :failed)
    errors = count_by_outcome(all_tests, :invalid)
    skipped = count_by_outcome(all_tests, :skipped) + count_by_outcome(all_tests, :excluded)
    time = format_duration(test_results.times_us.run)

    suite_elements = Enum.map_join(suites, "\n", &render_testsuite/1)
    system_err = render_system_err(diagnostics, sanitizer_matching, crash_matching)

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

  defp render_system_err(nil, _sanitizer_matching, _crash_matching), do: ""

  defp render_system_err(diagnostics, sanitizer_matching, crash_matching) do
    parts =
      [
        format_diagnostics(diagnostics),
        format_matching_attribution(
          "Crash Attribution",
          "Unattributed crashes",
          crash_matching,
          :crash,
          &format_crash_detail/1
        ),
        format_matching_attribution(
          "Sanitizer Attribution",
          "Unattributed sanitizer errors",
          sanitizer_matching,
          :error,
          &format_sanitizer_detail/1
        )
      ]
      |> Enum.reject(&(&1 == ""))
      |> Enum.join("\n\n")

    if parts == "" do
      ""
    else
      ~s(<system-err>#{xml_cdata(parts)}</system-err>)
    end
  end

  defp format_diagnostics(diagnostics) do
    if ToastTest.ResultExporter.cluster_diagnostics?(diagnostics) do
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
    parts = [
      "Crash Report:",
      "  Signal: #{report.signal_name} (#{report.signal_number})"
    ]

    parts =
      case Map.get(report, :crash_output, []) do
        lines when is_list(lines) and lines != [] ->
          formatted = Enum.map(lines, &"    #{&1}")
          parts ++ ["  Crash output:" | formatted]

        _ ->
          parts ++ [if(report.crash_header, do: "  #{report.crash_header}")]
      end

    parts =
      case report.fatal_lines do
        lines when is_list(lines) and lines != [] ->
          formatted = Enum.map(lines, &"    #{&1}")
          parts ++ ["  Fatal lines:" | formatted]

        _ ->
          parts
      end

    parts
    |> Enum.reject(&is_nil/1)
    |> Enum.join("\n")
  end

  defp format_log_section(nil), do: nil

  defp format_log_section(log) do
    a_count = length(log.assertion_failures)
    w_count = length(log.warnings)

    if a_count > 0 or w_count > 0 do
      "Log Issues:\n  Assertions: #{a_count}\n  Warnings: #{w_count}"
    end
  end

  # --- Matching attribution (shared by crash and sanitizer) ---

  defp format_matching_attribution(_title, _unmatched_title, nil, _item_key, _detail_fn), do: ""

  defp format_matching_attribution(
         _title,
         _unmatched_title,
         %{matched: [], unmatched: []},
         _item_key,
         _detail_fn
       ),
       do: ""

  defp format_matching_attribution(
         title,
         unmatched_title,
         %{matched: matched, unmatched: unmatched},
         item_key,
         detail_fn
       ) do
    parts = []

    parts =
      if matched != [] do
        grouped = Enum.group_by(matched, fn e -> {e.module, e.test} end)

        entries =
          grouped
          |> Enum.sort_by(fn {{mod, name}, _} -> {inspect(mod), name} end)
          |> Enum.map(fn {{module, test_name}, entries} ->
            confidence_label =
              entries
              |> Enum.map(& &1.confidence)
              |> Matcher.confidence_label()

            header = "#{inspect(module)} - #{test_name} (#{confidence_label}):"

            details =
              Enum.map_join(entries, "\n", fn e ->
                detail_fn.(Map.fetch!(e, item_key))
              end)

            "#{header}\n#{details}"
          end)

        parts ++ ["#{title}:" | entries]
      else
        parts
      end

    parts =
      if unmatched != [] do
        entries =
          Enum.map_join(unmatched, "\n", detail_fn)

        parts ++ ["#{unmatched_title}:\n#{entries}"]
      else
        parts
      end

    Enum.join(parts, "\n")
  end

  defp format_matching_attribution(_title, _unmatched_title, _other, _item_key, _detail_fn),
    do: ""

  defp format_crash_detail(crash) do
    signal = "#{crash.signal_name} (signal #{crash.signal_number})"
    "  [#{signal}] #{crash.server_id} - #{crash.log_file}"
  end

  defp format_sanitizer_detail(error) do
    type = error.sanitizer_type |> Atom.to_string() |> String.upcase()
    "  [#{type}] #{error.server_id} - #{error.file_path}"
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
