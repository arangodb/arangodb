defmodule ToastTest.SuiteResult.JUnitXML do
  @moduledoc false

  alias ToastTest.SuiteResult

  @spec write(SuiteResult.t(), Path.t()) :: :ok
  def write(%SuiteResult{} = result, result_dir) do
    xml = render(result)
    File.write!(Path.join(result_dir, "#{result.suite}.xml"), xml)
    :ok
  end

  defp render(result) do
    tests = SuiteResult.flat_tests(result)
    total = length(tests)
    failures = Enum.count(tests, &(&1.outcome == :failed))
    skipped = Enum.count(tests, &(&1.outcome in [:skipped, :excluded]))
    errors = Enum.count(tests, &(&1.outcome == :invalid))
    time = format_duration(result.times_us.run)

    failure_index = build_failure_index(result.issues)

    suites =
      result.modules
      |> Enum.sort_by(fn {mod, _} -> Atom.to_string(mod) end)
      |> Enum.map_join("\n", &render_testsuite(&1, failure_index))

    system_err = render_system_err(result.issues)

    [
      ~s(<?xml version="1.0" encoding="UTF-8"?>),
      ~s(<testsuites name="toast" tests="#{total}" failures="#{failures}" errors="#{errors}" skipped="#{skipped}" time="#{time}">),
      suites,
      system_err,
      ~s(</testsuites>)
    ]
    |> Enum.reject(&(&1 == ""))
    |> Enum.join("\n")
  end

  defp build_failure_index(issues) do
    for %{type: :test_failure, scope: {:test, mod, name}} = issue <- issues,
        into: %{} do
      {{mod, name}, issue}
    end
  end

  defp render_testsuite({module, %{tests: tests}}, failure_index) do
    name = Atom.to_string(module)
    total = length(tests)
    failures = Enum.count(tests, &(&1.outcome == :failed))
    errors = Enum.count(tests, &(&1.outcome == :invalid))
    skipped = Enum.count(tests, &(&1.outcome in [:skipped, :excluded]))
    time = tests |> Enum.map(& &1.duration_us) |> Enum.sum() |> format_duration()

    cases = Enum.map_join(tests, "\n", &render_testcase(&1, name, module, failure_index))

    [
      ~s(  <testsuite name="#{xml_escape(name)}" tests="#{total}" failures="#{failures}" errors="#{errors}" skipped="#{skipped}" time="#{time}">),
      cases,
      ~s(  </testsuite>)
    ]
    |> Enum.join("\n")
  end

  defp render_testcase(test, classname, module, failure_index) do
    name = xml_escape(Atom.to_string(test.name))
    cn = xml_escape(classname)
    time = format_duration(test.duration_us)

    case test.outcome do
      :passed ->
        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}"/>)

      :failed ->
        child = render_failure(module, test.name, failure_index)

        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}">\n#{child}\n    </testcase>)

      :invalid ->
        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}">\n      <error/>\n    </testcase>)

      outcome when outcome in [:skipped, :excluded] ->
        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}">\n      <skipped/>\n    </testcase>)
    end
  end

  defp render_failure(module, test_name, failure_index) do
    case Map.get(failure_index, {module, test_name}) do
      %{detail: %{test: %{state: {:failed, failures}}}} ->
        render_failure_details(failures)

      _ ->
        ~s(      <failure/>)
    end
  end

  defp render_failure_details([{:error, %{message: message}, _stack} | _]) do
    ~s(      <failure message="#{xml_escape(message)}"/>)
  end

  defp render_failure_details(_), do: ~s(      <failure/>)

  defp render_system_err(issues) do
    crash_parts =
      for %{type: :crash, detail: detail} <- issues do
        render_crash_detail(detail)
      end

    sanitizer_parts =
      for %{type: :sanitizer_report, detail: detail} <- issues do
        [labeled("Server", detail[:server]), detail[:report]]
        |> Toast.Utils.compact_join("\n")
      end

    case crash_parts ++ sanitizer_parts do
      [] -> ""
      parts -> wrap_cdata("system-err", Enum.join(parts, "\n\n"))
    end
  end

  defp render_crash_detail(detail) do
    coredump_lines = Enum.map(detail[:coredumps] || [], &format_coredump/1)

    [
      labeled("Server", detail[:server]),
      coredump_lines,
      labeled("Logs", detail[:logs])
    ]
    |> List.flatten()
    |> Toast.Utils.compact_join("\n")
  end

  defp format_coredump(%{path: path, signal: signal}) when is_binary(signal),
    do: "Coredump: #{path}, Signal: #{signal}"

  defp format_coredump(%{path: path}), do: "Coredump: #{path}"

  defp labeled(_label, nil), do: nil
  defp labeled(label, value), do: "#{label}: #{value}"

  defp wrap_cdata(tag, content) do
    escaped = String.replace(content, "]]>", "]]]]><![CDATA[>")
    ~s(<#{tag}><![CDATA[#{escaped}]]></#{tag}>)
  end

  defp format_duration(us) do
    :erlang.float_to_binary(us / 1_000_000, decimals: 3)
  end

  defp xml_escape(nil), do: ""

  defp xml_escape(text) when is_binary(text) do
    text
    |> String.replace("&", "&amp;")
    |> String.replace("<", "&lt;")
    |> String.replace(">", "&gt;")
    |> String.replace("\"", "&quot;")
    |> String.replace("'", "&apos;")
  end
end
