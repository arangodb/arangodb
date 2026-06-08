################################################################################
## DISCLAIMER
##
## Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
## Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
##
## Licensed under the Business Source License 1.1 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     https://github.com/arangodb/arangodb/blob/devel/LICENSE
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
## Copyright holder is ArangoDB GmbH, Cologne, Germany
################################################################################

defmodule ToastTest.Runner do
  @moduledoc "Suite-based test runner for Toast, orchestrating test execution within deployed ArangoDB environments."

  # This implementation is largely taken from ExUnit.Runner
  # SPDX-License-Identifier: Apache-2.0
  # SPDX-FileCopyrightText: 2021 The Elixir Team
  # SPDX-FileCopyrightText: 2012 Plataformatec

  alias ToastTest.ExUnitCompat, as: Compat
  alias ToastTest.Abort

  require Logger

  defmodule RunContext do
    @moduledoc false

    defmodule Filters do
      @moduledoc false
      defstruct [:include, :exclude, :only_test_ids, :test_name_pattern]

      @type t :: %__MODULE__{
              include: [term()] | nil,
              exclude: [term()] | nil,
              only_test_ids: MapSet.t() | nil,
              test_name_pattern: String.t() | nil
            }
    end

    @enforce_keys [:manager, :stats_pid, :result_collector_pid, :timeout_settings]
    defstruct [
      :between_tests,
      :capture_log,
      :filters,
      :manager,
      :max_failures,
      :result_collector_pid,
      :stats_pid,
      :timeout_settings
    ]

    @type t :: %__MODULE__{
            between_tests: (ExUnit.Test.t() | nil -> :ok | {:error, term()}),
            capture_log: boolean() | nil,
            filters: Filters.t(),
            manager: ToastTest.ExUnitCompat.event_manager(),
            max_failures: non_neg_integer() | :infinity,
            result_collector_pid: pid(),
            stats_pid: pid(),
            timeout_settings: ToastTest.Runner.Timeout.Settings.t()
          }
  end

  defmodule SuiteEntry do
    @moduledoc false
    defstruct [:module, :test_modules, :opts, :name]

    @type t :: %__MODULE__{
            module: module(),
            test_modules: [module()],
            opts: keyword(),
            name: String.t()
          }
  end

  defmodule Pipeline do
    @moduledoc false
    defstruct [:manager, :stats_pid, :result_collector_pid]

    @type t :: %__MODULE__{
            manager: ToastTest.ExUnitCompat.event_manager(),
            stats_pid: pid(),
            result_collector_pid: pid()
          }
  end

  @current_key __MODULE__
  def current_key, do: @current_key

  ## Suite config

  @topology_keys [:cluster_agents, :cluster_dbservers, :cluster_coordinators, :replication_factor]
  @known_suite_config_keys [
                             :mode,
                             :timeout,
                             :server_args,
                             :coordinator_args,
                             :dbserver_args,
                             :agent_args,
                             :between_tests,
                             :authentication,
                             :jwt_algorithm
                           ] ++ @topology_keys

  ## Public API

  @spec run_suites(
          [
            {module(), [module()]}
            | {module(), [module()], keyword()}
            | {module(), [module()], keyword(), String.t()}
          ],
          ToastTest.Config.t(),
          keyword()
        ) :: map()
  def run_suites(suites, %ToastTest.Config{} = test_config, ex_unit_opts) do
    Abort.clear!()
    ToastTest.DeploymentRegistry.clear()
    runner = self()
    id = {__MODULE__, runner}

    try do
      _ =
        System.trap_signal(:sigquit, id, fn ->
          case Process.info(runner, :dictionary) do
            {:dictionary, dict} ->
              manager = Keyword.get(dict, :toast_manager)
              current = Keyword.get(dict, @current_key)

              if manager do
                running = if current, do: List.wrap(current), else: []
                Compat.sigquit(manager, running)
              end

            nil ->
              :ok
          end
        end)

      Logger.info("Starting #{length(suites)} suite(s)")
      do_run_suites(suites, test_config, ex_unit_opts)
    after
      System.untrap_signal(:sigquit, id)
    end
  end

  defp do_run_suites(suites, test_config, ex_unit_opts) do
    global_deadline = System.monotonic_time(:millisecond) + test_config.global_timeout

    mode_exclusion =
      case test_config.deployment_mode do
        :cluster -> [:single_only]
        :single_server -> [:cluster_only]
      end

    ex_unit_opts =
      ex_unit_opts
      |> Keyword.update(:exclude, mode_exclusion, &(mode_exclusion ++ &1))
      |> Keyword.put(:timeout, test_config.test_timeout)

    {suite_results, acc_stats} =
      Enum.reduce(suites, {[], %{total: 0, failures: 0, skipped: 0, excluded: 0}}, fn
        suite_entry, {results, acc} ->
          __MODULE__.Timeout.check_global_deadline!(global_deadline)

          entry = normalize_suite_entry(suite_entry)

          suite_result = run_suite(entry, test_config, ex_unit_opts, global_deadline)

          result = %{
            suite_module: entry.module,
            stats: suite_result.stats,
            suite_result: suite_result.suite_result
          }

          {[result | results], merge_stats(acc, suite_result.stats)}
      end)

    %{
      suites: Enum.reverse(suite_results),
      stats: acc_stats
    }
  end

  defp normalize_suite_entry({suite_module, test_modules, suite_opts, suite_name}),
    do: %SuiteEntry{
      module: suite_module,
      test_modules: test_modules,
      opts: suite_opts,
      name: suite_name
    }

  defp normalize_suite_entry({suite_module, test_modules, suite_opts}),
    do: %SuiteEntry{
      module: suite_module,
      test_modules: test_modules,
      opts: suite_opts,
      name: default_suite_name(suite_module)
    }

  defp normalize_suite_entry({suite_module, test_modules}),
    do: %SuiteEntry{
      module: suite_module,
      test_modules: test_modules,
      opts: [],
      name: default_suite_name(suite_module)
    }

  defp default_suite_name(suite_module) do
    suite_module |> Module.split() |> List.last() |> Macro.underscore()
  end

  ## Suite config validation

  defp validate_suite_config!(suite_module, config) do
    unknown = Keyword.keys(config) -- @known_suite_config_keys

    if unknown != [] do
      Logger.warning(
        "#{inspect(suite_module)}.deployment_config/0 returned unknown keys: #{inspect(unknown)}"
      )
    end
  end

  defp validate_no_async!(test_modules) do
    async_modules =
      Enum.filter(
        test_modules,
        &match?(%{tags: %{async: true}}, Compat.get_test_metadata(&1))
      )

    if async_modules != [] do
      names = Enum.map_join(async_modules, ", ", &inspect/1)
      raise "Toast does not support async test modules. Found: #{names}"
    end
  end

  defp resolve_mode(suite_config, test_config) do
    case Keyword.get(suite_config, :mode, :auto) do
      :auto -> test_config.deployment_mode
      :manual -> :manual
      mode -> mode
    end
  end

  ## Event pipeline

  defp start_event_pipeline(opts, suite_name) do
    {:ok, manager} = Compat.start_event_manager()
    {:ok, stats_pid} = Compat.add_runner_stats(manager, opts)

    formatters =
      opts
      |> Keyword.get(:formatters, [])
      |> List.delete(ExUnit.CLIFormatter)
      |> ensure_in_list(ToastTest.Formatting.CLI, :front)
      |> ensure_in_list(ToastTest.ResultCollector, :back)

    formatter_pids =
      Map.new(formatters, fn formatter ->
        formatter_opts =
          if formatter == ToastTest.ResultCollector,
            do: Keyword.put(opts, :suite, suite_name),
            else: opts

        {:ok, pid} = Compat.add_formatter(manager, formatter, formatter_opts)
        {formatter, pid}
      end)

    result_collector_pid =
      Map.get(formatter_pids, ToastTest.ResultCollector) ||
        raise "ResultCollector formatter failed to start"

    %Pipeline{manager: manager, stats_pid: stats_pid, result_collector_pid: result_collector_pid}
  end

  defp build_run_context(opts, suite_opts, suite_run, %Pipeline{} = pipeline, between_tests_fn) do
    %RunContext{
      between_tests: between_tests_fn,
      capture_log: opts[:capture_log],
      filters: %RunContext.Filters{
        include: opts[:include],
        exclude: opts[:exclude],
        only_test_ids: Keyword.get(suite_opts, :only_test_ids, opts[:only_test_ids]),
        test_name_pattern: Keyword.get(suite_opts, :test_name_pattern)
      },
      manager: pipeline.manager,
      max_failures: opts[:max_failures] || :infinity,
      result_collector_pid: pipeline.result_collector_pid,
      stats_pid: pipeline.stats_pid,
      timeout_settings: %__MODULE__.Timeout.Settings{
        base_timeout: opts[:timeout],
        timeout_factor: suite_run.timeout_factor,
        suite_deadline: suite_run.suite_deadline,
        suite_timeout: suite_run.suite_timeout,
        global_deadline: suite_run.global_deadline,
        global_timeout: suite_run.global_timeout,
        disable_timeouts: suite_run.test_config.attach_debugger
      }
    }
  end

  defp normalize_opts(opts) do
    {include, exclude} = ExUnit.Filters.normalize(opts[:include], opts[:exclude])
    Keyword.merge(opts, include: include, exclude: exclude)
  end

  defp derive_suite_name(suite_module, deployment_mode) do
    base = suite_module |> Module.split() |> Enum.map_join("_", &Macro.underscore/1)
    "#{base}.#{deployment_mode}"
  end

  defp ensure_in_list(list, item, :front) do
    if item in list, do: list, else: [item | list]
  end

  defp ensure_in_list(list, item, :back) do
    if item in list, do: list, else: list ++ [item]
  end

  ## Suite execution

  defp run_suite(%SuiteEntry{} = entry, test_config, ex_unit_opts, global_deadline) do
    suite_config = entry.module.deployment_config()
    validate_suite_config!(entry.module, suite_config)
    suite_timeout = Keyword.get(suite_config, :timeout, 3_600_000)
    timeout_factor = test_config.timeout_factor
    suite_deadline = __MODULE__.Timeout.compute_suite_deadline(suite_timeout, global_deadline)

    validate_no_async!(entry.test_modules)

    mode = resolve_mode(suite_config, test_config)

    suite_run = %ToastTest.SuiteRun{
      suite_module: entry.module,
      deployment_mode: mode,
      suite_deadline: suite_deadline,
      suite_timeout: suite_timeout,
      global_deadline: global_deadline,
      global_timeout: test_config.global_timeout,
      timeout_factor: timeout_factor,
      test_config: test_config,
      between_tests:
        if(mode == :manual, do: false, else: Keyword.get(suite_config, :between_tests, :default))
    }

    case mode do
      :manual ->
        run_suite_manual(suite_run, entry, ex_unit_opts)

      _ ->
        deploy_config = ToastTest.DeployConfig.build(mode, suite_config)

        Logger.info("Running suite #{inspect(entry.module)} (mode=#{mode})")
        Logger.debug("Deployment config: #{inspect(deploy_config)}")

        id = Toast.Deployment.generate_id(mode)
        deployment_dir = Path.join([test_config.base_dir, entry.name, id])

        case Toast.Deployment.start(deploy_config, deployment_dir,
               id: id,
               event_listener: ToastTest.ManagedDeploymentListener
             ) do
          {:ok, deployment} ->
            run_suite_with_deployment(deployment, suite_run, entry, ex_unit_opts)

          {:error, reason} ->
            handle_deployment_failure(suite_run, entry, reason, ex_unit_opts)
        end
    end
  end

  defp run_suite_with_deployment(deployment, suite_run, %SuiteEntry{} = entry, ex_unit_opts) do
    suite_module = suite_run.suite_module
    test_config = suite_run.test_config
    ToastTest.DeploymentRegistry.put(suite_module, deployment)
    Logger.debug("Suite #{inspect(suite_module)}: deployment ready")

    if test_config.attach_debugger do
      if test_config.ci do
        Logger.warning("--attach-debugger ignored in CI mode")
      else
        ToastTest.DebuggerAttach.prompt(deployment, test_config.debugger)
      end
    end

    {stats, test_data} =
      case run_suite_setup(suite_module, deployment) do
        {:ok, extra_context} ->
          Logger.debug("Suite #{inspect(suite_module)}: setup complete")
          ToastTest.DeploymentRegistry.put_extra_context(suite_module, extra_context)

          result =
            run_suite_tests(deployment, suite_run, entry.test_modules, ex_unit_opts, entry.opts)

          run_suite_teardown(suite_module, deployment)
          result

        {:error, reason} ->
          mark_all_with_state(
            entry.test_modules,
            fn _test ->
              {:failed,
               [{:error, RuntimeError.exception("Suite setup failed: #{inspect(reason)}"), []}]}
            end,
            ex_unit_opts,
            entry.module,
            suite_run.deployment_mode
          )
      end

    finalize_suite(deployment, stats, test_data, test_config)
  end

  defp run_suite_manual(suite_run, %SuiteEntry{} = entry, ex_unit_opts) do
    Logger.info("Running suite #{inspect(suite_run.suite_module)} (mode=manual)")

    {stats, test_data} =
      run_suite_tests(nil, suite_run, entry.test_modules, ex_unit_opts, entry.opts)

    finalize_suite(nil, stats, test_data, suite_run.test_config)
  end

  defp handle_deployment_failure(suite_run, %SuiteEntry{} = entry, reason, ex_unit_opts) do
    suite_module = suite_run.suite_module
    test_config = suite_run.test_config
    mode = suite_run.deployment_mode

    Logger.error("Deployment failed for suite #{inspect(suite_module)}: #{inspect(reason)}")
    Abort.abort!({:deploy_failed, "Deployment failed: #{inspect(reason)}"})

    {stats, test_data} =
      mark_all_with_state(
        entry.test_modules,
        fn _test ->
          {:skipped, Abort.format_skip("Deployment failed: #{inspect(reason)}")}
        end,
        ex_unit_opts,
        entry.module,
        mode
      )

    finalize_suite(nil, stats, test_data, test_config)
  end

  defp finalize_suite(deployment, stats, test_data, test_config) do
    suite_result = __MODULE__.PostExecution.run(deployment, test_data, test_config)
    ToastTest.StateCleanup.reset()
    %{stats: stats, suite_result: suite_result}
  end

  defp build_between_tests_fn(%ToastTest.SuiteRun{between_tests: false}, _deployment, _pipeline),
    do: fn _prev -> :ok end

  defp build_between_tests_fn(
         %ToastTest.SuiteRun{} = suite_run,
         deployment,
         %Pipeline{} = pipeline
       ) do
    check_fn =
      if function_exported?(suite_run.suite_module, :between_tests, 2) do
        &suite_run.suite_module.between_tests(deployment, &1)
      else
        &ToastTest.Runner.BetweenTests.check(deployment, &1)
      end

    barrier_timeout = suite_run.test_config.coredump_timeout
    collector = pipeline.result_collector_pid

    fn
      nil ->
        :ok

      prev_test ->
        result =
          with :ok <-
                 Toast.Deployment.CrashBarrier.await_settled(deployment, timeout: barrier_timeout),
               :ok <-
                 Toast.Deployment.HealthBarrier.await_healthy(deployment,
                   timeout: barrier_timeout
                 ) do
            check_fn.(prev_test)
          end

        # Extend prev_test's attribution window to now, so any crash whose
        # :DOWN arrived during the barrier wait still attributes to it.
        ToastTest.ResultCollector.notify_between_tests_finished(collector, prev_test)

        result
    end
  end

  ## Suite setup/teardown

  defp run_suite_setup(suite_module, deployment) do
    if function_exported?(suite_module, :setup_deployment, 1) do
      suite_module.setup_deployment(deployment)
    else
      {:ok, %{}}
    end
  end

  defp run_suite_teardown(suite_module, deployment) do
    Logger.debug("Suite #{inspect(suite_module)}: teardown starting")

    if function_exported?(suite_module, :teardown_deployment, 1) do
      case suite_module.teardown_deployment(deployment) do
        {:error, reason} ->
          Logger.warning(
            "#{inspect(suite_module)}.teardown_deployment/1 failed: #{inspect(reason)}"
          )

        _ ->
          :ok
      end
    end

    :ok
  end

  ## Suite test orchestration

  defp run_suite_tests(deployment, suite_run, test_modules, ex_unit_opts, suite_opts) do
    opts = normalize_opts(Keyword.merge(ExUnit.configuration(), ex_unit_opts))
    suite_name = derive_suite_name(suite_run.suite_module, suite_run.deployment_mode)
    pipeline = start_event_pipeline(opts, suite_name)

    between_tests_fn = build_between_tests_fn(suite_run, deployment, pipeline)

    config =
      build_run_context(
        opts,
        suite_opts,
        suite_run,
        pipeline,
        between_tests_fn
      )

    # Expose manager to SIGQUIT signal handler (reads via Process.info/2)
    Process.put(:toast_manager, pipeline.manager)
    :erlang.system_flag(:backtrace_depth, Keyword.fetch!(opts, :stacktrace_depth))

    start_time = System.monotonic_time()
    Compat.suite_started(pipeline.manager, opts)

    __MODULE__.TestExecution.run_modules(config, test_modules)

    collect_suite_stats(config, pipeline, start_time)
  end

  defp collect_suite_stats(config, %Pipeline{} = pipeline, start_time) do
    if __MODULE__.TestExecution.max_failures_reached?(config) do
      Compat.max_failures_reached(config.manager)
    end

    run_us =
      System.convert_time_unit(
        System.monotonic_time() - start_time,
        :native,
        :microsecond
      )

    {stats, test_data} =
      finish_event_pipeline(pipeline, %{async: nil, load: nil, run: run_us})

    after_suite_callbacks = Application.fetch_env!(:ex_unit, :after_suite)
    Enum.each(after_suite_callbacks, & &1.(stats))

    {stats, test_data}
  end

  defp merge_stats(acc, suite_stats) do
    %{
      total: acc.total + Map.get(suite_stats, :total, 0),
      failures: acc.failures + Map.get(suite_stats, :failures, 0),
      skipped: acc.skipped + Map.get(suite_stats, :skipped, 0),
      excluded: acc.excluded + Map.get(suite_stats, :excluded, 0)
    }
  end

  defp mark_all_with_state(test_modules, state_fn, ex_unit_opts, suite_module, mode) do
    emit_all_modules(test_modules, ex_unit_opts, suite_module, mode, fn manager, module ->
      __MODULE__.TestExecution.emit_module_with_state(manager, module, fn test ->
        %{test | state: state_fn.(test)}
      end)
    end)
  end

  defp emit_all_modules(test_modules, ex_unit_opts, suite_module, mode, emit_fn) do
    opts = normalize_opts(Keyword.merge(ExUnit.configuration(), ex_unit_opts))
    suite_name = derive_suite_name(suite_module, mode)
    pipeline = start_event_pipeline(opts, suite_name)
    Compat.suite_started(pipeline.manager, opts)

    for module <- test_modules, do: emit_fn.(pipeline.manager, module)

    finish_event_pipeline(pipeline, %{async: nil, load: nil, run: 0})
  end

  defp finish_event_pipeline(%Pipeline{} = pipeline, times_us) do
    Compat.suite_finished(pipeline.manager, times_us)
    stats = Compat.stats(pipeline.stats_pid)
    test_data = ToastTest.ResultCollector.get_data(pipeline.result_collector_pid)
    Compat.stop(pipeline.manager)
    {stats, test_data}
  end
end
