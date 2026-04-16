defmodule ToastTest.SuiteResult do
  @moduledoc false

  @type incarnation :: %{
          pid: non_neg_integer(),
          started_at: Toast.timestamp(),
          stopped_at: Toast.timestamp() | nil
        }

  @type server_meta :: %{
          id: String.t(),
          deployment_id: String.t(),
          role: atom(),
          endpoint: String.t() | nil,
          log_file: Path.t() | nil,
          arango_id: String.t() | nil,
          incarnations: [incarnation()],
          logs: [{Toast.timestamp(), Toast.timestamp(), [map()]}]
        }

  @type deployment_meta :: %{
          id: String.t(),
          mode: :cluster | :single_server,
          stacktrace: list() | nil,
          started_at: Toast.timestamp(),
          stopped_at: Toast.timestamp() | nil,
          servers: %{String.t() => server_meta()}
        }

  @type coredump_frame :: %{
          function: String.t(),
          file: String.t() | nil,
          line: integer() | nil
        }

  @type coredump_thread :: %{
          id: String.t(),
          name: String.t() | nil,
          frames: [coredump_frame()]
        }

  @type coredump_report :: %{
          core_path: Path.t(),
          server_id: String.t(),
          debugger: :gdb | :lldb | nil,
          signal: String.t() | nil,
          faulting_address: String.t() | nil,
          registers: String.t() | nil,
          disassembly: String.t() | nil,
          crash_thread: String.t() | nil,
          threads: [coredump_thread()]
        }

  @type t :: %__MODULE__{
          version: pos_integer(),
          suite: String.t(),
          started_at: DateTime.t(),
          finished_at: DateTime.t() | nil,
          times_us: %{
            async: non_neg_integer() | nil,
            load: non_neg_integer() | nil,
            run: non_neg_integer()
          },
          modules: %{module() => module_result()},
          issues: [issue()],
          deployments: %{String.t() => deployment_meta()},
          coredumps: [coredump_report()],
          events: [map()],
          warnings: [String.t()]
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
          outcome: :passed | :failed | :skipped | :excluded | :invalid | :invalidated,
          duration_us: non_neg_integer(),
          started_at: DateTime.t() | nil,
          finished_at: DateTime.t() | nil,
          tags: map()
        }

  @type scope :: :suite | {:module, module()} | {:test, module(), atom()}

  @type issue :: %{
          type: :test_failure | :crash | :sanitizer_report | :timeout,
          scope: scope(),
          confidence: :high | :low | nil,
          detail: map()
        }

  require Logger

  defstruct [
    :suite,
    :started_at,
    :finished_at,
    :times_us,
    version: 1,
    modules: %{},
    issues: [],
    deployments: %{},
    coredumps: [],
    events: [],
    warnings: []
  ]

  @spec build(ToastTest.ResultCollector.test_data(), [issue()], keyword()) :: t()
  def build(test_data, issues, opts \\ []) do
    %__MODULE__{
      version: 1,
      suite: test_data.suite,
      started_at: test_data.started_at,
      finished_at: test_data.finished_at,
      times_us: test_data.times_us || %{async: nil, load: nil, run: 0},
      modules: test_data.modules,
      issues: issues,
      deployments: Keyword.get(opts, :deployments, %{}),
      coredumps: Keyword.get(opts, :coredumps, []),
      events: Keyword.get(opts, :events, []),
      warnings: Keyword.get(opts, :warnings, [])
    }
  end

  @spec write_all(t(), Path.t()) :: :ok
  def write_all(%__MODULE__{} = result, result_dir) do
    Logger.info("Writing results to #{result_dir}")
    File.mkdir_p!(result_dir)
    __MODULE__.JSON.write(result, result_dir)
    Logger.debug("Wrote #{Path.join(result_dir, "outcomes.json")}")
    write_diagnostics_etf(result, result_dir)
    Logger.debug("Wrote #{Path.join(result_dir, "#{result.suite}.diagnostics.etf")}")
    __MODULE__.JUnitXML.write(result, result_dir)
    Logger.debug("Wrote #{Path.join(result_dir, "#{result.suite}.xml")}")
    :ok
  end

  @spec write_outcomes_json(t(), Path.t()) :: :ok
  def write_outcomes_json(%__MODULE__{} = result, result_dir) do
    __MODULE__.JSON.write(result, result_dir)
  end

  @spec write_diagnostics_etf(t(), Path.t()) :: :ok
  def write_diagnostics_etf(%__MODULE__{} = result, result_dir) do
    binary = :erlang.term_to_binary(result, [:compressed])
    File.write!(Path.join(result_dir, "#{result.suite}.diagnostics.etf"), binary)
    :ok
  end

  @spec write_junit_xml(t(), Path.t()) :: :ok
  def write_junit_xml(%__MODULE__{} = result, result_dir) do
    __MODULE__.JUnitXML.write(result, result_dir)
  end

  @spec flat_tests(t()) :: [map()]
  def flat_tests(%__MODULE__{modules: modules}) do
    Enum.flat_map(modules, fn {mod, %{tests: tests}} ->
      Enum.map(tests, &Map.put(&1, :module, mod))
    end)
  end
end
