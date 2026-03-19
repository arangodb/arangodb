defmodule ToastTest.SuiteResult.JUnitXML do
  @moduledoc false

  alias ToastTest.{IssueFormatting, SuiteResult}

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

    suites =
      result.modules
      |> Enum.sort_by(fn {mod, _} -> Atom.to_string(mod) end)
      |> Enum.map_join("\n", &render_testsuite(&1, result.suite, failure_index, issue_index))

    system_err = render_system_err(Map.get(issue_index, :suite, []))

    [
      ~s(<?xml version="1.0" encoding="UTF-8"?>),
      ~s(<testsuites name="#{xml_escape(result.suite)}" tests="#{total}" failures="#{failures}" errors="#{errors}" skipped="#{skipped}" time="#{time}">),
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

  # Groups non-test-failure issues by their attributed scope.
  defp build_issue_index(issues) do
    issues
    |> Enum.reject(&(&1.type == :test_failure))
    |> Enum.group_by(& &1.scope)
  end

  defp render_testsuite({module, %{tests: tests}}, suite, failure_index, issue_index) do
    name = Atom.to_string(module)
    total = length(tests)
    failures = Enum.count(tests, &(&1.outcome == :failed))
    skipped = Enum.count(tests, &(&1.outcome in [:skipped, :excluded]))

    errors =
      Enum.count(tests, &(&1.outcome == :invalid)) +
        count_infra_errors(tests, module, issue_index)

    time = tests |> Enum.map(& &1.duration_us) |> Enum.sum() |> format_duration()

    cases =
      Enum.map_join(tests, "\n", &render_testcase(&1, module, suite, failure_index, issue_index))

    module_err = render_system_err(Map.get(issue_index, {:module, module}, []), "    ")

    [
      ~s(  <testsuite name="#{xml_escape(name)}" tests="#{total}" failures="#{failures}" errors="#{errors}" skipped="#{skipped}" time="#{time}">),
      cases,
      module_err,
      ~s(  </testsuite>)
    ]
    |> Enum.reject(&(&1 == ""))
    |> Enum.join("\n")
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
      |> Enum.map(&issue_type_label/1)
      |> Enum.join(", ")

    details =
      issues
      |> Enum.map(&render_issue_detail/1)
      |> Enum.reject(&is_nil/1)

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

  # --- system-err rendering (for module/suite-scoped issues) ---

  defp render_system_err(issues, indent \\ "")

  defp render_system_err([], _indent), do: ""

  defp render_system_err(issues, indent) do
    parts = Enum.map(issues, &render_issue_detail/1)

    case Enum.reject(parts, &is_nil/1) do
      [] -> ""
      non_empty -> indent <> wrap_cdata("system-err", Enum.join(non_empty, "\n\n"))
    end
  end

  # Issue detail rendering — delegates to shared IssueFormatting.
  defp render_issue_detail(%{type: :sanitizer_report} = issue),
    do: IssueFormatting.format_sanitizer(issue)

  defp render_issue_detail(%{type: :crash} = issue),
    do: IssueFormatting.format_crash(issue)

  defp render_issue_detail(%{type: :timeout} = issue),
    do: IssueFormatting.format_timeout(issue)

  defp render_issue_detail(_), do: nil

  # --- XML helpers ---

  defp escape_cdata(text), do: String.replace(text, "]]>", "]]]]><![CDATA[>")

  defp wrap_cdata(tag, content) do
    ~s(<#{tag}><![CDATA[#{escape_cdata(content)}]]></#{tag}>)
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
