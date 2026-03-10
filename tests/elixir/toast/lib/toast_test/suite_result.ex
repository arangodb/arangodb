defmodule ToastTest.SuiteResult do
  @type t :: %__MODULE__{
          version: pos_integer(),
          suite: String.t(),
          started_at: DateTime.t(),
          finished_at: DateTime.t(),
          times_us: %{async: non_neg_integer(), load: non_neg_integer(), run: non_neg_integer()},
          modules: %{module() => module_result()},
          issues: [issue()],
          events: %{atom() => [map()]}
        }

  @type module_result :: %{
          started_at: DateTime.t(),
          finished_at: DateTime.t(),
          setup_finished_at: DateTime.t() | nil,
          teardown_started_at: DateTime.t() | nil,
          tests: [test_result()]
        }

  @type test_result :: %{
          name: atom(),
          outcome: :passed | :failed | :skipped | :excluded | :invalid,
          duration_us: non_neg_integer(),
          started_at: DateTime.t(),
          finished_at: DateTime.t(),
          tags: map()
        }

  @type scope :: :suite | {:module, module()} | {:test, module(), atom()}

  @type issue :: %{
          type: :test_failure | :crash | :sanitizer_report | :deadlock,
          scope: scope(),
          confidence: :high | :low | nil,
          detail: map()
        }

  defstruct [
    :suite,
    :started_at,
    :finished_at,
    :times_us,
    version: 1,
    modules: %{},
    issues: [],
    events: %{}
  ]

  @spec build(map(), [issue()], %{atom() => [map()]}) :: t()
  def build(test_data, issues, events \\ %{}) do
    %__MODULE__{
      version: 1,
      suite: test_data.suite,
      started_at: test_data.started_at,
      finished_at: test_data.finished_at,
      times_us: test_data.times_us,
      modules: test_data.modules,
      issues: issues,
      events: events
    }
  end

  @spec write_all(t(), Path.t()) :: :ok
  def write_all(%__MODULE__{} = result, result_dir) do
    File.mkdir_p!(result_dir)
    write_outcomes_json(result, result_dir)
    write_diagnostics_etf(result, result_dir)
    write_junit_xml(result, result_dir)
    :ok
  end

  @spec write_outcomes_json(t(), Path.t()) :: :ok
  def write_outcomes_json(%__MODULE__{} = result, result_dir) do
    tests = flat_tests(result.modules)

    data = %{
      "suite" => result.suite,
      "started_at" => DateTime.to_iso8601(result.started_at),
      "finished_at" => DateTime.to_iso8601(result.finished_at),
      "duration_us" => result.times_us.run,
      "summary" => build_summary(tests),
      "tests" => Enum.map(tests, &encode_test/1)
    }

    json = data |> :json.format(&json_encoder/3, %{}) |> IO.iodata_to_binary()
    File.write!(Path.join(result_dir, "outcomes.json"), json)
    :ok
  end

  @spec write_diagnostics_etf(t(), Path.t()) :: :ok
  def write_diagnostics_etf(%__MODULE__{} = result, result_dir) do
    binary = :erlang.term_to_binary(result, [:compressed])
    File.write!(Path.join(result_dir, "#{result.suite}.diagnostics.etf"), binary)
    :ok
  end

  @spec write_junit_xml(t(), Path.t()) :: :ok
  def write_junit_xml(%__MODULE__{} = result, result_dir) do
    xml = render_junit_xml(result)
    File.write!(Path.join(result_dir, "#{result.suite}.xml"), xml)
    :ok
  end

  # --- outcomes.json helpers ---

  defp flat_tests(modules) do
    Enum.flat_map(modules, fn {mod, %{tests: tests}} ->
      Enum.map(tests, &Map.put(&1, :module, mod))
    end)
  end

  defp build_summary(tests) do
    counts = Enum.frequencies_by(tests, & &1.outcome)

    %{
      "passed" => Map.get(counts, :passed, 0),
      "failed" => Map.get(counts, :failed, 0),
      "skipped" => Map.get(counts, :skipped, 0),
      "excluded" => Map.get(counts, :excluded, 0),
      "invalid" => Map.get(counts, :invalid, 0)
    }
  end

  defp encode_test(test) do
    %{
      "module" => Atom.to_string(test.module),
      "name" => Atom.to_string(test.name),
      "outcome" => Atom.to_string(test.outcome),
      "duration_us" => test.duration_us
    }
  end

  defp json_encoder(value, encoder, opts) when is_atom(value) and not is_boolean(value) do
    if is_nil(value), do: "null", else: value |> Atom.to_string() |> encoder.(encoder, opts)
  end

  defp json_encoder(value, encoder, opts), do: :json.format_value(value, encoder, opts)

  # --- JUnit XML rendering ---

  defp render_junit_xml(result) do
    tests = flat_tests(result.modules)
    total = length(tests)
    failures = Enum.count(tests, &(&1.outcome == :failed))
    skipped = Enum.count(tests, &(&1.outcome in [:skipped, :excluded]))
    errors = Enum.count(tests, &(&1.outcome == :invalid))
    time = format_duration(result.times_us.run)

    # Build failure index: %{{module, test_name} => issue}
    failure_index = build_failure_index(result.issues)

    suites =
      result.modules
      |> Enum.sort_by(fn {mod, _} -> Atom.to_string(mod) end)
      |> Enum.map_join("\n", &render_testsuite(&1, failure_index))

    system_err = render_system_err(result.issues)

    parts =
      [
        ~s(<?xml version="1.0" encoding="UTF-8"?>),
        ~s(<testsuites name="toast" tests="#{total}" failures="#{failures}" errors="#{errors}" skipped="#{skipped}" time="#{time}">),
        suites,
        system_err,
        ~s(</testsuites>)
      ]
      |> Enum.reject(&(&1 == ""))

    Enum.join(parts, "\n")
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
        parts = []
        parts = if detail[:signal], do: parts ++ ["Signal: #{detail.signal}"], else: parts
        parts = if detail[:server], do: parts ++ ["Server: #{detail.server}"], else: parts
        parts = if detail[:logs], do: parts ++ ["Logs: #{detail.logs}"], else: parts

        parts =
          if detail[:coredump_path],
            do: parts ++ ["Coredump: #{detail.coredump_path}"],
            else: parts

        Enum.join(parts, "\n")
      end

    sanitizer_parts =
      for %{type: :sanitizer_report, detail: detail} <- issues do
        parts = []
        parts = if detail[:server], do: parts ++ ["Server: #{detail.server}"], else: parts
        parts = if detail[:report], do: parts ++ [detail.report], else: parts
        Enum.join(parts, "\n")
      end

    all_parts = crash_parts ++ sanitizer_parts

    if all_parts == [] do
      ""
    else
      content = Enum.join(all_parts, "\n\n")

      ~s(<system-err><![CDATA[#{String.replace(content, "]]>", "]]]]><![CDATA[>")}]]></system-err>)
    end
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
end
