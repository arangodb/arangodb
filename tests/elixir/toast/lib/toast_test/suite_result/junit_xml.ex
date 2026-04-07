defmodule ToastTest.SuiteResult.JUnitXML do
  @moduledoc false

  alias ToastTest.{Formatting.Issues, SuiteResult}

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
    time = format_duration(result.times_us.run)

    failure_index = build_failure_index(result.issues)
    issue_index = build_issue_index(result.issues)

    errors =
      Enum.count(tests, &(&1.outcome == :invalid)) +
        count_infra_errors(tests, issue_index)

    suite_issues = Map.get(issue_index, :suite, [])
    synthetic_suite = synthetic_testcases_for(suite_issues, result.suite)

    sorted_modules =
      Enum.sort_by(result.modules, fn {mod, _} -> Atom.to_string(mod) end)

    {suite_xmls, module_synthetic_count} =
      Enum.map_reduce(sorted_modules, 0, fn entry, acc ->
        {xml, syn_count} = render_testsuite(entry, result.suite, failure_index, issue_index)
        {xml, acc + syn_count}
      end)

    suites = Enum.join(suite_xmls, "\n")

    infra_suite = render_infra_testsuite(synthetic_suite)

    total = total + length(synthetic_suite) + module_synthetic_count
    errors = errors + length(synthetic_suite) + module_synthetic_count

    [
      ~s(<?xml version="1.0" encoding="UTF-8"?>),
      ~s(<testsuites name="#{xml_escape(result.suite)}" tests="#{total}" failures="#{failures}" errors="#{errors}" skipped="#{skipped}" time="#{time}">),
      suites,
      infra_suite,
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

  # Groups non-test-failure issues by their attributed scope.
  defp build_issue_index(issues) do
    issues
    |> Enum.reject(&(&1.type == :test_failure))
    |> Enum.group_by(& &1.scope)
  end

  defp render_testsuite({module, %{tests: tests}}, suite, failure_index, issue_index) do
    name = Atom.to_string(module)
    failures = Enum.count(tests, &(&1.outcome == :failed))
    skipped = Enum.count(tests, &(&1.outcome in [:skipped, :excluded]))
    time = tests |> Enum.map(& &1.duration_us) |> Enum.sum() |> format_duration()

    module_issues = Map.get(issue_index, {:module, module}, [])
    classname = "#{suite}::#{module}"
    synthetic = synthetic_testcases_for(module_issues, classname)
    synthetic_count = length(synthetic)

    total = length(tests) + synthetic_count

    errors =
      Enum.count(tests, &(&1.outcome == :invalid)) +
        count_infra_errors(tests, module, issue_index) +
        synthetic_count

    cases =
      Enum.map_join(tests, "\n", &render_testcase(&1, module, suite, failure_index, issue_index))

    synthetic_xml = Enum.map_join(synthetic, "\n", & &1)

    xml =
      [
        ~s(  <testsuite name="#{xml_escape(name)}" tests="#{total}" failures="#{failures}" errors="#{errors}" skipped="#{skipped}" time="#{time}">),
        cases,
        synthetic_xml,
        ~s(  </testsuite>)
      ]
      |> Enum.reject(&(&1 == ""))
      |> Enum.join("\n")

    {xml, synthetic_count}
  end

  defp render_testcase(test, module, suite, failure_index, issue_index) do
    name = xml_escape(Atom.to_string(test.name))
    cn = xml_escape("#{suite}::#{module}")
    time = format_duration(test.duration_us)
    test_issues = Map.get(issue_index, {:test, module, test.name}, [])
    infra_errors = render_infra_errors(test.outcome, test_issues)

    children =
      case test.outcome do
        :passed ->
          infra_errors

        :failed ->
          [render_failure(module, test.name, failure_index, test_issues) | infra_errors]

        :invalid ->
          ["      <error/>"]

        outcome when outcome in [:skipped, :excluded] ->
          ["      <skipped/>" | infra_errors]
      end

    render_testcase_element(name, cn, time, children)
  end

  # Tests that are already :invalid have their own <error/>.
  # For all others, infrastructure issues produce a single <error> element
  # (JUnit XML schema allows at most one per testcase).
  defp render_infra_errors(:invalid, _issues), do: []
  defp render_infra_errors(_outcome, []), do: []

  defp render_infra_errors(_outcome, issues) do
    message =
      issues
      |> Enum.map_join(", ", &issue_type_label/1)

    details =
      issues
      |> Enum.map(&render_issue_detail/1)
      |> Toast.Utils.compact()

    case details do
      [] ->
        [~s(      <error message="#{xml_escape(message)}"/>)]

      parts ->
        body = Enum.join(parts, "\n\n")

        [
          ~s(      <error message="#{xml_escape(message)}"><![CDATA[#{escape_cdata(body)}]]></error>)
        ]
    end
  end

  defp issue_type_label(%{type: :crash}), do: "crash"

  defp issue_type_label(%{type: :sanitizer_report, detail: %{kind: kind}}) when is_binary(kind),
    do: "sanitizer report: #{kind}"

  defp issue_type_label(%{type: :sanitizer_report}), do: "sanitizer report"
  defp issue_type_label(%{type: :timeout}), do: "timeout"
  defp issue_type_label(%{type: type}), do: Atom.to_string(type)

  # Counts tests with infrastructure issues that wouldn't otherwise show as errors.
  defp count_infra_errors(tests, module \\ nil, issue_index)

  defp count_infra_errors(tests, nil, issue_index) do
    Enum.count(tests, fn test ->
      test.outcome not in [:failed, :invalid] and
        Map.has_key?(issue_index, {:test, test.module, test.name})
    end)
  end

  defp count_infra_errors(tests, module, issue_index) do
    Enum.count(tests, fn test ->
      test.outcome not in [:failed, :invalid] and
        Map.has_key?(issue_index, {:test, module, test.name})
    end)
  end

  defp render_testcase_element(name, cn, time, children) do
    case Enum.reject(children, &(&1 == "")) do
      [] ->
        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}"/>)

      parts ->
        inner = Enum.join(parts, "\n")

        ~s(    <testcase name="#{name}" classname="#{cn}" time="#{time}">\n#{inner}\n    </testcase>)
    end
  end

  defp render_failure(module, test_name, failure_index, issues) do
    case Map.get(failure_index, {module, test_name}) do
      %{detail: %{test: %ExUnit.Test{state: {:failed, failures}} = test}} ->
        message_attr = failure_message_attr(failures)
        body = format_failure_body(test, failures, issues)
        render_failure_element(message_attr, body)

      _ ->
        render_failure_element("", infra_issue_note(issues))
    end
  end

  defp failure_message_attr([{:error, %{message: msg}, _} | _]),
    do: ~s( message="#{xml_escape(msg)}")

  defp failure_message_attr(_), do: ""

  defp format_failure_body(test, failures, issues) do
    formatted =
      ExUnit.Formatter.format_test_failure(test, failures, 0, :infinity, &plain_formatter_cb/2)
      |> strip_ansi()

    case issues do
      [] -> formatted
      _ -> formatted <> "\n\n" <> infra_issue_note(issues)
    end
  end

  defp render_failure_element(_message_attr, "") do
    ~s(      <failure/>)
  end

  defp render_failure_element(message_attr, body) do
    ~s(      <failure#{message_attr}><![CDATA[#{escape_cdata(body)}]]></failure>)
  end

  # Plain-text formatter callback — reduces ANSI output from ExUnit.Formatter.
  # strip_ansi/1 handles any remaining escape codes that ExUnit emits directly.
  defp plain_formatter_cb(:diff_enabled?, _default), do: false
  defp plain_formatter_cb(_, msg), do: msg

  defp strip_ansi(text) do
    String.replace(text, ~r/\e\[[0-9;]*m/, "")
  end

  defp infra_issue_note([]), do: ""

  defp infra_issue_note(issues) do
    header = "Additional infrastructure issues:"

    items =
      Enum.map(issues, fn issue ->
        label = issue_type_label(issue)
        server = issue.detail[:server]
        if server, do: "- #{label} (#{server})", else: "- #{label}"
      end)

    Enum.join([header | items], "\n")
  end

  # --- Synthetic testcase rendering (for module/suite-scoped issues) ---

  # Returns a list of rendered XML strings, one per issue that has renderable detail.
  defp synthetic_testcases_for(issues, classname) do
    issues
    |> Enum.map(&render_synthetic_testcase(&1, classname))
    |> Toast.Utils.compact()
  end

  defp render_synthetic_testcase(issue, classname) do
    detail = render_issue_detail(issue)
    if detail == nil, do: nil, else: do_render_synthetic_testcase(issue, classname, detail)
  end

  defp do_render_synthetic_testcase(issue, classname, detail) do
    label = issue_type_label(issue)
    server = issue.detail[:server]
    phase = phase_prefix(issue.detail[:phase])
    name_base = if phase, do: "#{phase} #{label}", else: label
    name = if server, do: "#{name_base} (#{server})", else: name_base
    escaped_name = xml_escape(name)
    escaped_cn = xml_escape(classname)
    escaped_msg = xml_escape(label)

    ~s(    <testcase name="#{escaped_name}" classname="#{escaped_cn}" time="0.000">\n) <>
      ~s(      <error message="#{escaped_msg}"><![CDATA[#{escape_cdata(detail)}]]></error>\n) <>
      ~s(    </testcase>)
  end

  # Wraps suite-scoped synthetic testcases in an _infrastructure_ testsuite.
  defp render_infra_testsuite([]), do: ""

  defp render_infra_testsuite(synthetic) do
    count = length(synthetic)
    inner = Enum.join(synthetic, "\n")

    [
      ~s(  <testsuite name="_infrastructure_" tests="#{count}" failures="0" errors="#{count}" skipped="0" time="0.000">),
      inner,
      ~s(  </testsuite>)
    ]
    |> Enum.join("\n")
  end

  defp phase_prefix(:setup), do: "setup_all"
  defp phase_prefix(:teardown), do: "on_exit"
  defp phase_prefix(:startup), do: "startup"
  defp phase_prefix(:shutdown), do: "shutdown"
  defp phase_prefix(_), do: nil

  # Issue detail rendering — delegates to shared Issues.
  defp render_issue_detail(%{type: :sanitizer_report} = issue),
    do: Issues.format_sanitizer(issue)

  defp render_issue_detail(%{type: :crash} = issue),
    do: Issues.format_crash(issue)

  defp render_issue_detail(%{type: :timeout} = issue),
    do: Issues.format_timeout(issue)

  defp render_issue_detail(_), do: nil

  # --- XML helpers ---

  defp escape_cdata(text), do: String.replace(text, "]]>", "]]]]><![CDATA[>")

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
