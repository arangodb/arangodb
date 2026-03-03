defmodule ToastTest.Case do
  @moduledoc """
  ExUnit.CaseTemplate for Toast test suites.

  Provides test context with `deployment`, `endpoint`, and `client` keys.
  Requires a deployment to be registered via `setup_suite/2` or `setup_suite!/2`
  in the suite's `test_helper.exs`.

  ## Usage

  In `test_helper.exs`:

      ExUnit.start()
      ToastTest.Case.setup_suite()

  In test modules:

      defmodule MyTest do
        use ToastTest.Case

        test "server version", %{client: client} do
          assert {:ok, %{"server" => "arango"}} = Client.Admin.version(client)
        end
      end

  ## Deployment tags

  Tests can be restricted to a specific deployment mode:

      @tag :cluster_only
      test "sharding", %{client: client} do
        # only runs with --cluster
      end

      @tag :single_only
      test "local feature", %{client: client} do
        # only runs with single server (skipped in cluster mode)
      end

  These tags also work as `@moduletag` to apply to all tests in a module.
  """

  use ExUnit.CaseTemplate

  require Logger

  @standalone_key :__standalone__

  using do
    quote do
      alias Toast.Client
      @moduletag :toast_suite
    end
  end

  setup context do
    suite_key = resolve_suite_key(context.module)
    deployment = ToastTest.DeploymentRegistry.get(suite_key)
    extra_context = ToastTest.DeploymentRegistry.get_extra_context(suite_key)

    base = %{
      deployment: deployment,
      endpoint: deployment.endpoint,
      client: Toast.Client.new(deployment.endpoint, api_version: deployment.config.api_version)
    }

    Map.merge(base, extra_context)
  end

  defp resolve_suite_key(module) do
    if function_exported?(module, :__toast_suite__, 0),
      do: module.__toast_suite__(),
      else: @standalone_key
  end

  @doc """
  Start a deployment and register it for test modules. Raises on failure.
  Call from test_helper.exs after ExUnit.start().

  When called without arguments, reads the deployment mode from
  `TOAST_DEPLOYMENT_MODE` (via `Toast.Config`).
  """
  @spec setup_suite!(atom(), keyword()) :: Toast.Deployment.t()
  def setup_suite!(mode \\ nil, opts \\ []) do
    case do_setup_suite(mode, opts) do
      {:ok, deployment} -> deployment
      {:error, mode, reason} -> raise "Failed to start #{mode} deployment: #{inspect(reason)}"
    end
  end

  @doc """
  Start a deployment and register it. On failure, marks deployment as
  unavailable so tests are skipped gracefully.
  Returns {:ok, deployment} or {:error, reason}.

  When called without arguments, reads the deployment mode from
  `TOAST_DEPLOYMENT_MODE` (via `Toast.Config`).
  """
  @spec setup_suite(atom(), keyword()) :: {:ok, Toast.Deployment.t()} | {:error, term()}
  def setup_suite(mode \\ nil, opts \\ []) do
    case do_setup_suite(mode, opts) do
      {:ok, deployment} -> {:ok, deployment}
      {:error, _mode, reason} -> {:error, reason}
    end
  end

  defp do_setup_suite(mode, opts) do
    config = Toast.Config.load(opts)
    mode = mode || config.deployment_mode
    setup_timeouts(config)

    case Toast.Deployment.start(mode, config) do
      {:ok, deployment} ->
        register_deployment(deployment)
        register_after_suite(deployment, config.keep_work_dir)
        {:ok, deployment}

      {:error, reason} ->
        {:error, mode, reason}
    end
  end

  @doc "Register a standalone deployment for test modules to use."
  @spec register_deployment(Toast.Deployment.t()) :: :ok
  def register_deployment(deployment) do
    ToastTest.DeploymentRegistry.ensure_init()
    ToastTest.DeploymentRegistry.put(@standalone_key, deployment)
  end

  @doc "Retrieve the registered standalone deployment."
  @spec get_deployment() :: Toast.Deployment.t()
  def get_deployment do
    ToastTest.DeploymentRegistry.ensure_init()
    ToastTest.DeploymentRegistry.get(@standalone_key)
  rescue
    RuntimeError ->
      reraise """
             No deployment registered.
             Call ToastTest.Case.setup_suite/2 in your test_helper.exs after ExUnit.start().
             """,
             __STACKTRACE__
  end

  defp setup_timeouts(config) do
    exclude = ExUnit.configuration()[:exclude] || []

    exclude =
      case config.deployment_mode do
        :single_server -> Enum.uniq([:cluster_only | exclude])
        :cluster -> Enum.uniq([:single_only | exclude])
      end

    ExUnit.configure(timeout: config.test_timeout, exclude: exclude)
  end

  defp register_after_suite(deployment, keep_work_dir) do
    register_formatters()

    ExUnit.after_suite(fn stats ->
      diagnostics =
        case Toast.Deployment.stop_and_collect(deployment) do
          {:ok, diag} -> diag
          {:error, _reason, partial_diag} -> partial_diag
        end

      test_results = Application.get_env(:toast, :__test_results__)
      sanitizer_matching = Toast.Diagnostics.SanitizerMatcher.match(diagnostics, test_results)
      crash_matching = Toast.Diagnostics.CrashMatcher.match(diagnostics, test_results)

      # Only print CRASHED SERVERS when crash attribution has no data
      # (e.g., no crash log to parse). Otherwise attribution replaces it.
      if crash_matching.matched == [] and crash_matching.unmatched == [] do
        print_diagnostics_summary(diagnostics)
      end

      crash_affected = find_crash_affected_tests(crash_matching, test_results)
      print_crash_attribution(crash_matching, crash_affected)
      print_sanitizer_summary(sanitizer_matching)
      ToastTest.ResultExporter.export(test_results, diagnostics, sanitizer_matching, crash_matching)

      cond do
        keep_work_dir ->
          Logger.info("Keeping server data in #{deployment.work_dir} (--keep-work-dir)")

        stats.failures > 0 or ToastTest.Runner.aborted?() ->
          Logger.warning("Test failures — preserving server data in #{deployment.work_dir}")

        true ->
          File.rm_rf(deployment.work_dir)
      end
    end)
  end

  defp register_formatters do
    current = Application.get_env(:ex_unit, :formatters, [ExUnit.CLIFormatter])
    # Replace ExUnit.CLIFormatter with ToastTest.CLIFormatter since we want our own output format
    formatters =
      current
      |> List.delete(ExUnit.CLIFormatter)
      |> ensure_at_front(ToastTest.CLIFormatter)
      |> ensure_at_end(ToastTest.ResultFormatter)

    ExUnit.configure(formatters: formatters)
  end

  defp ensure_at_front(list, item) do
    if item in list, do: list, else: [item | list]
  end

  defp ensure_at_end(list, item) do
    if item in list, do: list, else: list ++ [item]
  end

  defp print_diagnostics_summary(nil), do: :ok

  defp print_diagnostics_summary(diagnostics) do
    case Toast.Diagnostics.Summary.format_crashed_servers(diagnostics) do
      nil -> :ok
      text -> IO.puts(text)
    end
  end

  defp print_crash_attribution(%{matched: [], unmatched: []}, []), do: :ok

  defp print_crash_attribution(crash_matching, crash_affected) do
    case Toast.Diagnostics.Summary.format_crash_attribution(crash_matching, crash_affected) do
      nil -> :ok
      text -> IO.puts(text)
    end
  end

  defp find_crash_affected_tests(_crash_matching, nil), do: []

  defp find_crash_affected_tests(%{matched: matched, unmatched: unmatched}, test_results) do
    # Find the earliest crash timestamp
    all_crashes = Enum.map(matched, & &1.crash) ++ unmatched
    timestamps = all_crashes |> Enum.map(& &1.timestamp) |> Enum.reject(&is_nil/1)

    case timestamps do
      [] ->
        []

      _ ->
        earliest = Enum.min(timestamps, DateTime)
        # Matched test names — these are attributed to a crash, not "affected"
        attributed = MapSet.new(matched, fn m -> {m.module, m.test} end)

        test_results.tests
        |> Enum.filter(fn t ->
          t.outcome == :failed and
            t.started_at != nil and
            DateTime.compare(t.started_at, earliest) in [:gt, :eq] and
            not MapSet.member?(attributed, {t.module, t.name})
        end)
    end
  end

  defp print_sanitizer_summary(%{matched: [], unmatched: []}), do: :ok

  defp print_sanitizer_summary(sanitizer_matching) do
    case Toast.Diagnostics.Summary.format_sanitizer_issues(sanitizer_matching) do
      nil -> :ok
      text -> IO.puts(text)
    end
  end
end
