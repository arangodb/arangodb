defmodule ToastTest.Runner do
  @moduledoc "Suite-based test runner for Toast, orchestrating test execution within deployed ArangoDB environments."

  # This implementation is largely taken from ExUnit.Runner
  # SPDX-License-Identifier: Apache-2.0
  # SPDX-FileCopyrightText: 2021 The Elixir Team
  # SPDX-FileCopyrightText: 2012 Plataformatec

  alias ToastTest.ExUnitCompat, as: Compat
  alias ToastTest.{Abort, TestLifecycle, EventStore, SuiteResult}

  require Logger

  defmodule Config do
    @moduledoc false
    @enforce_keys [:manager, :stats_pid, :result_collector_pid, :suite_run]
    defstruct [
      :capture_log,
      :exclude,
      :include,
      :manager,
      :max_failures,
      :only_test_ids,
      :result_collector_pid,
      :stats_pid,
      :suite_deadline,
      :suite_run,
      :test_name_pattern,
      :timeout,
      :timeout_factor,
      :trace
    ]

    @type t :: %__MODULE__{
            capture_log: boolean() | nil,
            exclude: [term()] | nil,
            include: [term()] | nil,
            manager: pid(),
            max_failures: non_neg_integer() | :infinity | nil,
            only_test_ids: MapSet.t() | nil,
            result_collector_pid: pid(),
            stats_pid: pid(),
            suite_deadline: integer() | nil,
            suite_run: ToastTest.SuiteRun.t(),
            test_name_pattern: String.t() | nil,
            timeout: pos_integer() | nil,
            timeout_factor: number() | nil,
            trace: boolean() | nil
          }
  end

  @current_key __MODULE__

  ## Public API

  @spec run_suites(
          [{module(), [module()]} | {module(), [module()], keyword()}],
          keyword()
        ) :: map()
  def run_suites(suites, global_opts) do
    Abort.clear!()
    ToastTest.DeploymentRegistry.init()
    start_event_store()
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
      do_run_suites(suites, global_opts)
    after
      System.untrap_signal(:sigquit, id)
    end
  end

  defp do_run_suites(suites, global_opts) do
    global_deadline = Keyword.get(global_opts, :global_deadline)

    {suite_results, acc_stats} =
      Enum.reduce(suites, {[], %{total: 0, failures: 0, skipped: 0, excluded: 0}}, fn
        suite_entry, {results, acc} ->
          check_global_deadline!(global_deadline)
          {suite_module, test_modules, suite_opts} = normalize_suite_entry(suite_entry)

          if Abort.reason() do
            Logger.info("Skipping suite #{inspect(suite_module)} (aborted)")
            {results, acc}
          else
            suite_result =
              run_suite(suite_module, test_modules, global_opts, suite_opts, global_deadline)

            result = %{
              suite_module: suite_module,
              stats: suite_result.stats,
              suite_result: suite_result.suite_result
            }

            {[result | results], merge_stats(acc, suite_result.stats)}
          end
      end)

    %{
      suites: Enum.reverse(suite_results),
      stats: acc_stats
    }
  end

  defp normalize_suite_entry({suite_module, test_modules, suite_opts}),
    do: {suite_module, test_modules, suite_opts}

  defp normalize_suite_entry({suite_module, test_modules}),
    do: {suite_module, test_modules, []}

  ## Stacktrace

  @spec prune_stacktrace(Exception.stacktrace()) :: Exception.stacktrace()
  def prune_stacktrace([{ExUnit.Assertions, _, _, _} | t]), do: prune_stacktrace(t)
  def prune_stacktrace([{ExUnit.Runner, _, _, _} | _]), do: []
  def prune_stacktrace([h | t]), do: [h | prune_stacktrace(t)]
  def prune_stacktrace([]), do: []

  ## Suite execution

  defp run_suite(suite_module, test_modules, global_opts, suite_opts, global_deadline) do
    config = suite_module.deployment_config()
    validate_suite_config!(suite_module, config)
    suite_timeout = Keyword.get(config, :timeout, 3_600_000)
    timeout_factor = Keyword.get(global_opts, :timeout_factor, 1)
    suite_deadline = compute_suite_deadline(suite_timeout, global_deadline)

    validate_no_async!(test_modules)

    mode = resolve_deployment_mode(config, global_opts)

    suite_run = %ToastTest.SuiteRun{
      suite_module: suite_module,
      deployment_mode: mode,
      suite_deadline: suite_deadline,
      timeout_factor: timeout_factor
    }

    Logger.info("Running suite #{inspect(suite_module)} (mode=#{mode})")
    deployment_opts = build_deployment_opts(config, global_opts)
    toast_config = Toast.Config.load(deployment_opts)
    Logger.debug("Toast config: #{inspect(toast_config)}")

    case Toast.Deployment.start(mode, toast_config) do
      {:ok, deployment} ->
        run_suite_with_deployment(
          deployment,
          suite_run,
          suite_module,
          test_modules,
          global_opts,
          suite_opts,
          toast_config
        )

      {:error, reason} ->
        handle_deployment_failure(
          suite_module,
          test_modules,
          reason,
          global_opts,
          toast_config,
          mode
        )
    end
  end

  defp run_suite_with_deployment(
         deployment,
         suite_run,
         suite_module,
         test_modules,
         global_opts,
         suite_opts,
         toast_config
       ) do
    suite_run = %{suite_run | deployment: deployment}
    ToastTest.DeploymentRegistry.put(suite_module, deployment)
    Logger.debug("Suite #{inspect(suite_module)}: deployment ready")

    {stats, test_data} =
      case run_suite_setup(suite_module, deployment) do
        {:ok, extra_context} ->
          Logger.debug("Suite #{inspect(suite_module)}: setup complete")
          ToastTest.DeploymentRegistry.put_extra_context(suite_module, extra_context)
          result = run_suite_tests(suite_run, test_modules, global_opts, suite_opts)
          run_suite_teardown(suite_module, deployment)
          result

        {:error, reason} ->
          mark_all_errored_stats(
            test_modules,
            reason,
            global_opts,
            suite_module,
            suite_run.deployment_mode
          )
      end

    suite_result = post_execution(deployment, test_data, toast_config)

    ToastTest.StateCleanup.reset()
    %{stats: stats, suite_result: suite_result}
  end

  defp handle_deployment_failure(
         suite_module,
         test_modules,
         reason,
         global_opts,
         toast_config,
         mode
       ) do
    Logger.error("Deployment failed for suite #{inspect(suite_module)}: #{inspect(reason)}")
    Abort.abort!({:deploy_failed, "Deployment failed: #{inspect(reason)}"})

    {stats, test_data} =
      mark_all_skipped_stats(test_modules, reason, global_opts, suite_module, mode)

    suite_result = build_suite_result(%{}, test_data, toast_config)

    ToastTest.StateCleanup.reset()
    %{stats: stats, suite_result: suite_result}
  end

  defp run_suite_tests(suite_run, test_modules, global_opts, suite_opts) do
    opts = normalize_opts(Keyword.merge(ExUnit.configuration(), global_opts))
    suite_name = derive_suite_name(suite_run.suite_module, suite_run.deployment_mode)
    {manager, stats_pid, result_collector_pid} = start_event_pipeline(opts, suite_name)

    config =
      build_test_config(opts, suite_opts, suite_run, manager, stats_pid, result_collector_pid)

    # Expose manager to SIGQUIT signal handler (reads via Process.info/2)
    Process.put(:toast_manager, manager)

    :erlang.system_flag(:backtrace_depth, Keyword.fetch!(opts, :stacktrace_depth))

    start_time = System.monotonic_time()
    Compat.suite_started(manager, opts)

    run_suite_modules(config, test_modules)

    collect_suite_stats(config, start_time)
  end

  defp start_event_pipeline(opts, suite_name) do
    {:ok, manager} = Compat.start_event_manager()
    {:ok, stats_pid} = Compat.add_runner_stats(manager, opts)

    formatters =
      opts
      |> Keyword.get(:formatters, [])
      |> List.delete(ExUnit.CLIFormatter)
      |> ensure_in_list(ToastTest.CLIFormatter, :front)
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

    {manager, stats_pid, result_collector_pid}
  end

  defp ensure_in_list(list, item, :front) do
    if item in list, do: list, else: [item | list]
  end

  defp ensure_in_list(list, item, :back) do
    if item in list, do: list, else: list ++ [item]
  end

  defp build_test_config(opts, suite_opts, suite_run, manager, stats_pid, result_collector_pid) do
    only_test_ids = Keyword.get(suite_opts, :only_test_ids, opts[:only_test_ids])

    %Config{
      capture_log: opts[:capture_log],
      exclude: opts[:exclude],
      include: opts[:include],
      manager: manager,
      max_failures: opts[:max_failures],
      only_test_ids: only_test_ids,
      result_collector_pid: result_collector_pid,
      stats_pid: stats_pid,
      suite_deadline: suite_run.suite_deadline,
      suite_run: suite_run,
      test_name_pattern: Keyword.get(suite_opts, :test_name_pattern),
      timeout: opts[:timeout],
      timeout_factor: suite_run.timeout_factor,
      trace: opts[:trace]
    }
  end

  defp collect_suite_stats(config, start_time) do
    stop_time = System.monotonic_time()

    if max_failures_reached?(config) do
      Compat.max_failures_reached(config.manager)
    end

    run_us = System.convert_time_unit(stop_time - start_time, :native, :microsecond)
    times_us = %{async: nil, load: nil, run: run_us}
    Compat.suite_finished(config.manager, times_us)

    stats = Compat.stats(config.stats_pid)
    test_data = ToastTest.ResultCollector.get_data(config.result_collector_pid)
    Compat.stop(config.manager)

    after_suite_callbacks = Application.fetch_env!(:ex_unit, :after_suite)
    Enum.each(after_suite_callbacks, & &1.(stats))

    {stats, test_data}
  end

  defp run_suite_modules(config, test_modules) do
    Enum.each(test_modules, fn module ->
      check_suite_deadline!(config)

      cond do
        reason = Abort.reason() ->
          emit_skipped_module(
            config,
            module,
            Abort.prefix() <> Abort.display_reason(reason)
          )

        max_failures_reached?(config) ->
          :ok

        true ->
          run_module(config, module)
      end
    end)
  end

  ## Suite helpers

  @topology_keys [:cluster_agents, :cluster_dbservers, :cluster_coordinators, :replication_factor]
  @known_suite_config_keys [
                             :mode,
                             :timeout,
                             :server_args,
                             :coordinator_args,
                             :dbserver_args,
                             :agent_args,
                             :between_tests
                           ] ++ @topology_keys

  defp validate_no_async!(test_modules) do
    async_modules =
      Enum.filter(test_modules, &match?(%{tags: %{async: true}}, Compat.get_test_metadata(&1)))

    if async_modules != [] do
      names = Enum.map_join(async_modules, ", ", &inspect/1)
      raise "Toast does not support async test modules. Found: #{names}"
    end
  end

  defp validate_suite_config!(suite_module, config) do
    unknown = Keyword.keys(config) -- @known_suite_config_keys

    if unknown != [] do
      Logger.warning(
        "#{inspect(suite_module)}.deployment_config/0 returned unknown keys: #{inspect(unknown)}"
      )
    end
  end

  defp compute_suite_deadline(suite_timeout, nil) do
    System.monotonic_time(:millisecond) + suite_timeout
  end

  defp compute_suite_deadline(suite_timeout, global_deadline) do
    now = System.monotonic_time(:millisecond)
    suite_end = now + suite_timeout
    min(suite_end, global_deadline)
  end

  defp resolve_deployment_mode(suite_config, global_opts) do
    case Keyword.get(suite_config, :mode, :auto) do
      :auto -> Keyword.get(global_opts, :deployment_mode, :single_server)
      mode -> mode
    end
  end

  @infra_keys [
    :build_dir,
    :work_dir,
    :sanitizer_override,
    :show_server_logs,
    :keep_work_dir
  ]

  defp build_deployment_opts(suite_config, global_opts) do
    base = []

    suite_args =
      for key <- [:server_args, :coordinator_args, :dbserver_args, :agent_args],
          args = Keyword.get(suite_config, key, %{}),
          args != %{},
          do: {key, args}

    suite_topology = Keyword.take(suite_config, @topology_keys)

    # Precedence (highest to lowest): suite topology > global CLI opts > suite server args > base
    #
    # Suite topology (cluster shape) is part of the test contract — the test requires a specific
    # topology to be meaningful, so it must win. Infrastructure opts (build_dir, work_dir, etc.)
    # are deployment environment settings where CLI should override suite defaults.
    base
    |> Keyword.merge(suite_args)
    |> Keyword.merge(Keyword.take(global_opts, @infra_keys ++ @topology_keys))
    |> Keyword.merge(suite_topology)
  end

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

  defp merge_stats(acc, suite_stats) do
    %{
      total: acc.total + Map.get(suite_stats, :total, 0),
      failures: acc.failures + Map.get(suite_stats, :failures, 0),
      skipped: acc.skipped + Map.get(suite_stats, :skipped, 0),
      excluded: acc.excluded + Map.get(suite_stats, :excluded, 0)
    }
  end

  defp mark_all_skipped_stats(test_modules, reason, global_opts, suite_module, mode) do
    skip_reason = Abort.prefix() <> "Deployment failed: #{inspect(reason)}"

    emit_all_modules(test_modules, global_opts, suite_module, mode, fn manager, module ->
      emit_module_with_state(manager, module, fn test ->
        %{test | state: {:skipped, skip_reason}}
      end)
    end)
  end

  defp mark_all_errored_stats(test_modules, reason, global_opts, suite_module, mode) do
    emit_all_modules(test_modules, global_opts, suite_module, mode, fn manager, module ->
      emit_module_with_state(manager, module, fn test ->
        %{
          test
          | state:
              {:failed,
               [{:error, RuntimeError.exception("Deployment failed: #{inspect(reason)}"), []}]}
        }
      end)
    end)
  end

  defp emit_all_modules(test_modules, global_opts, suite_module, mode, emit_fn) do
    opts = normalize_opts(Keyword.merge(ExUnit.configuration(), global_opts))
    suite_name = derive_suite_name(suite_module, mode)
    {manager, stats_pid, result_collector_pid} = start_event_pipeline(opts, suite_name)
    Compat.suite_started(manager, opts)

    for module <- test_modules, do: emit_fn.(manager, module)

    times_us = %{async: nil, load: nil, run: 0}
    Compat.suite_finished(manager, times_us)
    stats = Compat.stats(stats_pid)
    test_data = ToastTest.ResultCollector.get_data(result_collector_pid)
    Compat.stop(manager)
    {stats, test_data}
  end

  defp emit_module_with_state(manager, module, transform_test) do
    test_module = Compat.get_test_metadata(module)
    EventStore.notify(%{event: :module_started, module: module})
    Compat.module_started(manager, test_module)

    transformed_tests =
      for test <- test_module.tests do
        transformed = transform_test.(test)
        Compat.test_started(manager, transformed)
        Compat.test_finished(manager, transformed)
        transformed
      end

    Compat.module_finished(manager, %{test_module | tests: transformed_tests})
    EventStore.notify(%{event: :module_finished, module: module})
  end

  defp post_execution(deployment, test_data, toast_config) do
    Logger.debug("Post-execution: stopping deployment")
    {servers, error} = stop_deployment(deployment, toast_config)
    if error, do: Logger.warning("Deployment stop error: #{inspect(error)}")

    try do
      build_suite_result(servers, test_data, toast_config)
    rescue
      e ->
        Logger.warning(
          "build_suite_result crashed, returning degraded result: " <>
            "#{Exception.format(:error, e, __STACKTRACE__)}"
        )

        SuiteResult.build(test_data, [])
    end
  end

  defp stop_deployment(deployment, toast_config) do
    case Toast.Deployment.stop(deployment, timeout: toast_config.shutdown_timeout) do
      {:ok, info} -> {info.servers, info.error}
      {:error, _reason, info} -> {info.servers, info.error}
    end
  end

  defp build_suite_result(servers, test_data, toast_config) do
    snapshot = EventStore.snapshot()

    Logger.debug("Collecting artifacts")
    artifact_opts = [coredump_dir: toast_config.coredump_dir, not_before: test_data.started_at]

    artifacts =
      ToastTest.ArtifactCollector.collect(servers, snapshot.pids_by_server, artifact_opts)

    Logger.debug("Running attribution")

    issues =
      ToastTest.Attribution.run(test_data, artifacts, snapshot.unexpected_crashes,
        timeout_kills: snapshot.timeout_kills,
        analyzer_opts: build_coredump_analyzer_opts(toast_config)
      )

    Logger.debug("Collecting server logs")
    windows = ToastTest.Attribution.TimeWindows.build(test_data)
    all_log_files = collect_log_files(snapshot.servers)
    server_logs = ToastTest.Attribution.ServerLogs.collect(issues, all_log_files, windows)

    Logger.debug("Building results (#{length(issues)} issues found)")
    warnings = coredump_warnings(snapshot.unexpected_crashes, artifacts, toast_config)
    deployments = build_deployments(snapshot, server_logs)

    suite_result =
      SuiteResult.build(test_data, issues,
        warnings: warnings,
        deployments: deployments,
        events: snapshot.events
      )

    SuiteResult.write_all(suite_result, toast_config.result_dir)
    print_post_exec_summary(suite_result)
    suite_result
  end

  defp build_deployments(snapshot, server_logs) do
    Map.new(snapshot.deployments, fn {did, deployment_info} ->
      servers_with_logs =
        Map.new(Map.get(snapshot.servers, did, %{}), fn {sid, server} ->
          {sid, Map.put(server, :logs, Map.get(server_logs, sid, []))}
        end)

      {did,
       %{
         id: did,
         mode: deployment_info.mode,
         stacktrace: deployment_info.stacktrace,
         started_at: deployment_info.started_at,
         stopped_at: deployment_info.stopped_at,
         servers: servers_with_logs
       }}
    end)
  end

  defp collect_log_files(servers_by_deployment) do
    for {_did, servers} <- servers_by_deployment,
        {sid, server} <- servers,
        log_file = server[:log_file],
        log_file != nil,
        into: %{} do
      {sid, log_file}
    end
  end

  defp coredump_warnings(crash_events, artifacts, toast_config) do
    if crash_events != [] and not ToastTest.ArtifactCollector.has_coredumps?(artifacts) do
      case Toast.Diagnostics.Coredump.coredump_discovery_warning(toast_config.coredump_dir) do
        nil -> []
        warning -> [warning]
      end
    else
      []
    end
  end

  defp build_coredump_analyzer_opts(toast_config) do
    opts = [timeout: toast_config.coredump_timeout]

    case Toast.Diagnostics.Coredump.resolve_debugger(toast_config.debugger) do
      nil -> opts
      debugger -> [{:debugger, debugger} | opts]
    end
  end

  defp print_post_exec_summary(suite_result) do
    ToastTest.PostExecSummary.print(suite_result)
  end

  defp derive_suite_name(suite_module, deployment_mode) do
    base = suite_module |> Module.split() |> Enum.map_join("_", &Macro.underscore/1)
    "#{base}.#{deployment_mode}"
  end

  defp start_event_store do
    case EventStore.start_link() do
      {:ok, _} -> :ok
      {:error, {:already_started, _}} -> :ok
    end
  end

  defp emit_skipped_module(config, module, reason) do
    emit_module_with_state(config.manager, module, fn test ->
      %{test | state: {:skipped, reason}}
    end)
  end

  defp normalize_opts(opts) do
    {include, exclude} = ExUnit.Filters.normalize(opts[:include], opts[:exclude])
    Keyword.merge(opts, include: include, exclude: exclude)
  end

  ## Running modules

  defp run_module(config, module) do
    test_module = Compat.get_test_metadata(module)
    EventStore.notify(%{event: :module_started, module: module})
    Compat.module_started(config.manager, test_module)

    {to_run_tests, excluded_and_skipped_tests} =
      prepare_tests(config, test_module.tests)

    finish_module_execution(config, test_module, to_run_tests, excluded_and_skipped_tests)
  end

  ## Module execution logic

  defp finish_module_execution(config, test_module, to_run_tests, excluded_and_skipped_tests) do
    for excluded_or_skipped_test <- excluded_and_skipped_tests do
      Compat.test_started(config.manager, excluded_or_skipped_test)
      Compat.test_finished(config.manager, excluded_or_skipped_test)
    end

    {test_module, invalid_tests, finished_tests} =
      run_module_tests(config, test_module, to_run_tests)

    if reason = Abort.reason() do
      finish_aborted_module(config, test_module, invalid_tests, finished_tests, reason)
    else
      finish_pending_module(config, test_module, invalid_tests, finished_tests)
    end
  end

  defp finish_aborted_module(config, test_module, invalid_tests, finished_tests, reason) do
    abort_msg = Abort.prefix() <> Abort.display_reason(reason)

    for test <- invalid_tests do
      skipped = %{test | state: {:skipped, abort_msg}}
      Compat.test_started(config.manager, skipped)
      Compat.test_finished(config.manager, skipped)
    end

    test_module = %{test_module | tests: Enum.reverse(finished_tests, invalid_tests)}
    Compat.module_finished(config.manager, test_module)
    EventStore.notify(%{event: :module_finished, module: test_module.name})
  end

  defp finish_pending_module(config, test_module, invalid_tests, finished_tests) do
    pending_tests =
      case process_max_failures(config, test_module) do
        :no -> invalid_tests
        {:reached, n} -> Enum.take(invalid_tests, n)
        :surpassed -> nil
      end

    if pending_tests do
      emit_pending_tests(config, test_module, pending_tests, finished_tests)
    end
  end

  defp emit_pending_tests(config, test_module, pending_tests, finished_tests) do
    for pending_test <- pending_tests do
      Compat.test_started(config.manager, pending_test)
      Compat.test_finished(config.manager, pending_test)
    end

    test_module = %{test_module | tests: Enum.reverse(finished_tests, pending_tests)}
    Compat.module_finished(config.manager, test_module)
    EventStore.notify(%{event: :module_finished, module: test_module.name})
  end

  ## Test preparation

  defp prepare_tests(config, tests) do
    include = config.include
    exclude = config.exclude
    test_ids = config.only_test_ids
    name_pattern = config.test_name_pattern

    {to_run, to_skip} =
      for test <- tests,
          include_test?(test_ids, test),
          match_test_name?(name_pattern, test),
          reduce: {[], []} do
        {to_run, to_skip} ->
          tags = Map.merge(test.tags, %{test: test.name, module: test.module})

          case ExUnit.Filters.eval(include, exclude, tags, tests) do
            :ok -> {[%{test | tags: tags} | to_run], to_skip}
            excluded_or_skipped -> {to_run, [%{test | state: excluded_or_skipped} | to_skip]}
          end
      end

    {Enum.reverse(to_run), Enum.reverse(to_skip)}
  end

  defp include_test?(test_ids, test) do
    test_ids == nil or MapSet.member?(test_ids, {test.module, test.name})
  end

  defp match_test_name?(nil, _test), do: true

  defp match_test_name?(pattern, test) do
    test.name
    |> Atom.to_string()
    |> String.downcase()
    |> String.contains?(String.downcase(pattern))
  end

  ## Module test execution

  defp run_module_tests(_config, test_module, []) do
    {test_module, [], []}
  end

  defp run_module_tests(config, test_module, tests) do
    Process.put(@current_key, test_module)
    %ExUnit.TestModule{name: module, tags: tags, parameters: params} = test_module

    context =
      tags
      |> Map.merge(params)
      |> Map.merge(%{module: module, async: Map.get(tags, :async, false)})

    config
    |> run_setup_all(test_module, context, fn context ->
      if max_failures_reached?(config) or Abort.reason(),
        do: {[], tests},
        else: run_tests(config, tests, test_module.parameters, context)
    end)
    |> case do
      {{:ok, {finished_tests, remaining_tests}}, test_module} ->
        {test_module, remaining_tests, finished_tests}

      {:error, test_module} ->
        {test_module, Enum.map(tests, &%{&1 | state: {:invalid, test_module}}), []}
    end
  end

  defp run_setup_all(
         _config,
         %ExUnit.TestModule{setup_all?: false} = test_module,
         context,
         callback
       ) do
    {{:ok, callback.(context)}, test_module}
  end

  defp run_setup_all(config, %ExUnit.TestModule{name: module} = test_module, context, callback) do
    {module_pid, module_ref} = TestLifecycle.spawn_setup_all(module, context)

    {ok_or_error, test_module} =
      receive do
        {^module_pid, :setup_all, {:ok, context}} ->
          finished_tests = callback.(context)
          :ok = TestLifecycle.exit_setup_all(module_pid, module_ref)
          {{:ok, finished_tests}, test_module}

        {^module_pid, :setup_all, {:error, {kind, error, stack}}} ->
          :ok = TestLifecycle.exit_setup_all(module_pid, module_ref)
          {:error, %{test_module | state: failed(kind, error, prune_stacktrace(stack))}}

        {:DOWN, ^module_ref, :process, ^module_pid, error} ->
          {:error, %{test_module | state: failed({:EXIT, module_pid}, error, [])}}
      end

    timeout = get_timeout(config, %{})
    {ok_or_error, exec_on_exit(test_module, module_pid, timeout)}
  end

  defp run_tests(config, tests, params, context) do
    run_tests_loop(config, tests, params, context, [])
  end

  defp run_tests_loop(_config, [], _params, _context, acc) do
    {acc, []}
  end

  defp run_tests_loop(config, [test | rest] = remaining, params, context, acc) do
    check_suite_deadline!(config)

    prev_test = List.first(acc)

    if Abort.reason() do
      {acc, remaining}
    else
      case check_between_tests(config, prev_test) do
        {:error, reason} ->
          Abort.abort!(reason)
          {acc, remaining}

        :ok ->
          run_next_test(config, test, rest, params, context, acc)
      end
    end
  end

  defp run_next_test(config, test, rest, params, context, acc) do
    test = %{test | parameters: params}
    Process.put(@current_key, test)

    case run_test(config, test, context) do
      {:ok, test} -> run_tests_loop(config, rest, params, context, [test | acc])
      :max_failures_reached -> {acc, rest}
    end
  end

  @spec check_between_tests(Config.t(), ExUnit.Test.t() | nil) :: :ok | {:error, term()}
  defp check_between_tests(
         %{suite_run: %{suite_module: suite_module, deployment: deployment}},
         prev_test
       ) do
    suite_config = suite_module.deployment_config()

    cond do
      Keyword.get(suite_config, :between_tests) == false ->
        :ok

      function_exported?(suite_module, :between_tests, 2) and prev_test != nil ->
        suite_module.between_tests(deployment, prev_test)

      true ->
        Toast.Deployment.check_health(deployment, prev_test)
    end
  end

  defp run_test(config, test, context) do
    EventStore.notify(%{
      event: :test_started,
      module: test.module,
      name: test.name
    })

    Compat.test_started(config.manager, test)
    test = spawn_test(config, test, context)

    EventStore.notify(%{
      event: :test_finished,
      module: test.module,
      name: test.name,
      outcome: test_outcome(test),
      duration_us: test.time
    })

    case process_max_failures(config, test) do
      :no ->
        Compat.test_finished(config.manager, test)
        {:ok, test}

      {:reached, 1} ->
        Compat.test_finished(config.manager, test)
        :max_failures_reached

      :surpassed ->
        :max_failures_reached
    end
  end

  defp test_outcome(%{state: nil}), do: :passed
  defp test_outcome(%{state: {:failed, _}}), do: :failed
  defp test_outcome(%{state: {:skipped, _}}), do: :skipped
  defp test_outcome(%{state: {:excluded, _}}), do: :excluded
  defp test_outcome(%{state: {:invalid, _}}), do: :invalid
  defp test_outcome(_), do: :unknown

  ## Per-test execution

  defp spawn_test(config, test, context) do
    parent_pid = self()
    timeout = get_timeout(config, test.tags)
    start_time = System.monotonic_time()
    {test_pid, test_ref} = spawn_test_monitor(config, test, parent_pid, context)
    Abort.register_test_pid(test_pid)
    test = receive_test_reply(test, test_pid, test_ref, timeout, start_time)
    Abort.unregister_test_pid()
    exec_on_exit(test, test_pid, timeout)
  end

  defp spawn_test_monitor(
         %{capture_log: capture_log},
         test,
         parent_pid,
         context
       ) do
    spawn_monitor(fn ->
      Process.set_label({test.case, test.name})
      ExUnit.OnExitHandler.register(self())
      context = context |> Map.merge(test.tags) |> Map.put(:test_pid, self())
      capture_log = Map.get(context, :capture_log, capture_log)

      {time, test} =
        :timer.tc(
          maybe_capture_log(capture_log, test, fn ->
            context = maybe_create_tmp_dir(context, test)
            run_test_with_setup(test, context)
          end)
        )

      send(parent_pid, {self(), :test_finished, %{test | time: time}})
      exit(:shutdown)
    end)
  end

  defp run_test_with_setup(test, context) do
    case exec_test_setup(test, context) do
      {:ok, context} -> exec_test(test, context)
      {:skipped, test} -> test
      {:error, test} -> test
    end
  end

  defp maybe_capture_log(false, _test, fun), do: fun
  defp maybe_capture_log(true, test, fun), do: maybe_capture_log([], test, fun)

  defp maybe_capture_log(capture_log_opts, test, fun) when is_list(capture_log_opts) do
    fn ->
      try do
        ExUnit.CaptureLog.with_log(capture_log_opts, fun)
      catch
        :exit, :noproc ->
          message =
            "could not run test, it uses @tag :capture_log" <>
              " but the :logger application is not running"

          %{test | state: failed(:error, RuntimeError.exception(message), [])}
      else
        {test, logs} -> %{test | logs: logs}
      end
    end
  end

  defp receive_test_reply(test, test_pid, test_ref, timeout, start_time) do
    receive do
      {^test_pid, :test_finished, test} ->
        Process.demonitor(test_ref, [:flush])
        test

      {:DOWN, ^test_ref, :process, ^test_pid, :killed} ->
        elapsed_us = elapsed_us(start_time)

        exception =
          case Abort.reason() do
            nil -> RuntimeError.exception("test process was killed")
            reason -> RuntimeError.exception("test aborted: #{Abort.display_reason(reason)}")
          end

        %{test | state: failed(:error, exception, []), time: elapsed_us}

      {:DOWN, ^test_ref, :process, ^test_pid, error} ->
        elapsed_us = elapsed_us(start_time)
        %{test | state: failed({:EXIT, test_pid}, error, []), time: elapsed_us}
    after
      timeout ->
        case Process.info(test_pid, :current_stacktrace) do
          {:current_stacktrace, stacktrace} ->
            Process.demonitor(test_ref, [:flush])
            Process.exit(test_pid, :kill)
            elapsed_us = elapsed_us(start_time)

            exception =
              ToastTest.TimeoutError.exception(
                timeout: timeout,
                type: Atom.to_string(test.tags.test_type)
              )

            %{test | state: failed(:error, exception, stacktrace), time: elapsed_us}

          nil ->
            receive_test_reply(test, test_pid, test_ref, timeout, start_time)
        end
    end
  end

  defp elapsed_us(start_time) do
    System.convert_time_unit(System.monotonic_time() - start_time, :native, :microsecond)
  end

  defp exec_test_setup(%ExUnit.Test{module: module} = test, context) do
    if reason = Abort.reason() do
      {:skipped,
       %{
         test
         | state: {:skipped, Abort.prefix() <> Abort.display_reason(reason)}
       }}
    else
      {:ok, Compat.get_test_setup(module, context)}
    end
  catch
    kind, error ->
      {:error, %{test | state: failed(kind, error, prune_stacktrace(__STACKTRACE__))}}
  end

  defp exec_test(%ExUnit.Test{module: module, name: name} = test, context) do
    apply(module, name, [context])
    test
  catch
    kind, error ->
      %{test | state: failed(kind, error, prune_stacktrace(__STACKTRACE__))}
  end

  defp exec_on_exit(test_or_case, pid, timeout) do
    case TestLifecycle.run_on_exit(pid, timeout) do
      :ok ->
        test_or_case

      {kind, reason, stack} ->
        state = test_or_case.state || failed(kind, reason, prune_stacktrace(stack))
        %{test_or_case | state: state}
    end
  end

  ## Helpers

  defp process_max_failures(%{max_failures: :infinity}, _), do: :no

  defp process_max_failures(config, %ExUnit.TestModule{state: {:failed, _}, tests: tests}) do
    process_max_failures(config.stats_pid, config.max_failures, length(tests))
  end

  defp process_max_failures(config, %ExUnit.Test{state: {:failed, _}}) do
    process_max_failures(config.stats_pid, config.max_failures, 1)
  end

  defp process_max_failures(config, _test_module_or_test) do
    if max_failures_reached?(config), do: :surpassed, else: :no
  end

  defp process_max_failures(stats_pid, max_failures, bump) do
    previous = Compat.increment_failure_counter(stats_pid, bump)

    cond do
      previous >= max_failures -> :surpassed
      previous + bump < max_failures -> :no
      true -> {:reached, max_failures - previous}
    end
  end

  defp max_failures_reached?(%{stats_pid: stats_pid, max_failures: max_failures}) do
    max_failures != :infinity and
      Compat.get_failure_counter(stats_pid) >= max_failures
  end

  defp get_timeout(config, tags) do
    config
    |> compute_base_timeout(tags)
    |> apply_timeout_factor(config.timeout_factor, Map.has_key?(tags, :timeout))
    |> clamp_to_deadline(config.suite_deadline)
  end

  defp compute_base_timeout(%{trace: true}, _tags), do: :infinity
  defp compute_base_timeout(config, tags), do: Map.get(tags, :timeout, config.timeout)

  defp apply_timeout_factor(:infinity, _factor, _has_tag), do: :infinity
  defp apply_timeout_factor(timeout, factor, true), do: timeout * factor
  defp apply_timeout_factor(timeout, _factor, false), do: timeout

  defp clamp_to_deadline(timeout, nil), do: timeout

  defp clamp_to_deadline(timeout, deadline) do
    remaining = max(deadline - System.monotonic_time(:millisecond), 1)
    if timeout == :infinity, do: remaining, else: min(timeout, remaining)
  end

  defp check_suite_deadline!(%{suite_deadline: nil}), do: :ok

  defp check_suite_deadline!(%{suite_deadline: deadline}) do
    if System.monotonic_time(:millisecond) >= deadline do
      abort_with_timeout(:test_timeout, "Suite timeout exceeded")
    end
  end

  defp check_global_deadline!(nil), do: :ok

  defp check_global_deadline!(deadline) do
    if System.monotonic_time(:millisecond) >= deadline do
      abort_with_timeout(:global_timeout, "Global execution timeout exceeded")
    end
  end

  defp abort_with_timeout(source, reason) do
    Logger.warning("#{reason} — aborting suite")
    EventStore.record_timeout_kill(source, reason, [])
    Abort.abort!({:timeout, reason})
  end

  defp failed(:error, %ExUnit.MultiError{errors: errors}, _stack) do
    errors =
      Enum.map(errors, fn {kind, reason, stack} ->
        {kind, Exception.normalize(kind, reason, stack), prune_stacktrace(stack)}
      end)

    {:failed, errors}
  end

  defp failed(kind, reason, stack) do
    {:failed, [{kind, Exception.normalize(kind, reason, stack), stack}]}
  end

  ## Tmp dir handling

  defp maybe_create_tmp_dir(%{tmp_dir: true} = tags, test) do
    create_tmp_dir!(test, "", tags)
  end

  defp maybe_create_tmp_dir(%{tmp_dir: path} = tags, test) when is_binary(path) do
    create_tmp_dir!(test, path, tags)
  end

  defp maybe_create_tmp_dir(%{tmp_dir: other}, _test) when other != false do
    raise ArgumentError, "expected :tmp_dir to be a boolean or a string, got: #{inspect(other)}"
  end

  defp maybe_create_tmp_dir(tags, _test) do
    tags
  end

  defp short_hash(module, test_name, parameters) do
    suffix = if parameters == %{}, do: "", else: :erlang.term_to_binary(parameters)

    (module <> "/" <> test_name <> suffix)
    |> :erlang.md5()
    |> Base.encode16(case: :lower)
    |> binary_slice(0..7)
  end

  defp create_tmp_dir!(test, extra_path, tags) do
    module_string = inspect(test.module)
    name_string = to_string(test.name)

    module = escape_path(module_string)
    name = escape_path(name_string)
    short_hash = short_hash(module_string, name_string, test.parameters)

    path = ["tmp", module, "#{name}-#{short_hash}", extra_path] |> Path.join() |> Path.expand()
    File.rm_rf!(path)
    File.mkdir_p!(path)
    Map.put(tags, :tmp_dir, path)
  end

  @escape Enum.map(~c" [~#%&*{}\\:<>?/+|\"]", &<<&1::utf8>>)

  defp escape_path(path) do
    String.replace(path, @escape, "-")
  end
end
