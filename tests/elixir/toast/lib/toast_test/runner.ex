defmodule ToastTest.Runner do
  @moduledoc "Suite-based test runner for Toast, orchestrating test execution within deployed ArangoDB environments."

  # This implementation is largely taken from ExUnit.Runner
  # SPDX-License-Identifier: Apache-2.0
  # SPDX-FileCopyrightText: 2021 The Elixir Team
  # SPDX-FileCopyrightText: 2012 Plataformatec

  alias ToastTest.ExUnitCompat, as: Compat

  require Logger

  @current_key __MODULE__
  @abort_table :toast_suite_abort

  ## Public API

  @spec run_suites(
          [{module(), [module()]} | {module(), [module()], keyword()}],
          keyword()
        ) :: map()
  def run_suites(suites, global_opts) do
    clear_abort!()
    ToastTest.DeploymentRegistry.init()
    start_process_history()
    runner = self()
    id = {__MODULE__, runner}

    try do
      _ =
        System.trap_signal(:sigquit, id, fn ->
          ref = Process.monitor(runner)
          send(runner, {ref, self(), :sigquit})

          receive do
            ^ref -> :ok
            {:DOWN, ^ref, _, _, _} -> :ok
          after
            5_000 -> :ok
          end

          Process.demonitor(ref, [:flush])
          :ok
        end)

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
          {suite_module, test_modules, suite_opts} = normalize_suite_entry(suite_entry)

          suite_result =
            run_suite(suite_module, test_modules, global_opts, suite_opts, global_deadline)

          result = %{
            suite_module: suite_module,
            stats: suite_result.stats,
            diagnostics: suite_result.diagnostics
          }

          {[result | results], merge_stats(acc, suite_result.stats)}
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

  @spec abort!(String.t() | {atom(), String.t()}) :: :ok
  def abort!(reason) do
    if :ets.insert_new(@abort_table, {:aborted, reason}) do
      IO.puts([
        IO.ANSI.red(),
        "====================================",
        "\n   ",
        abort_display_reason(reason),
        "\n   !!! Aborting further tests !!!\n",
        "====================================\n",
        IO.ANSI.reset()
      ])
    end

    :ok
  end

  @spec clear_abort!() :: :ok
  def clear_abort!() do
    try do
      :ets.delete(@abort_table)
    catch
      :error, :badarg -> :ok
    end

    :ets.new(@abort_table, [:named_table, :set, :public])
    :ok
  end

  @spec aborted?() :: String.t() | {atom(), String.t()} | nil
  def aborted? do
    case :ets.lookup(@abort_table, :aborted) do
      [{:aborted, reason}] -> reason
      [] -> nil
    end
  catch
    :error, :badarg -> nil
  end

  ## Stacktrace

  @spec prune_stacktrace(Exception.stacktrace()) :: Exception.stacktrace()
  def prune_stacktrace([{ExUnit.Assertions, _, _, _} | t]), do: prune_stacktrace(t)
  def prune_stacktrace([{ExUnit.Runner, _, _, _} | _]), do: []
  def prune_stacktrace([h | t]), do: [h | prune_stacktrace(t)]
  def prune_stacktrace([]), do: []

  ## Suite execution

  defp run_suite(suite_module, test_modules, global_opts, suite_opts, global_deadline) do
    config = suite_module.deployment_config()
    suite_timeout = Keyword.get(config, :timeout, 3_600_000)
    timeout_factor = Keyword.get(global_opts, :timeout_factor, 1)
    suite_deadline = compute_suite_deadline(suite_timeout, global_deadline)

    suite_run = %ToastTest.SuiteRun{
      suite_module: suite_module,
      suite_deadline: suite_deadline,
      timeout_factor: timeout_factor
    }

    validate_no_async!(test_modules)

    mode = resolve_deployment_mode(config, global_opts)
    deployment_opts = build_deployment_opts(config, global_opts)
    toast_config = Toast.Config.load(deployment_opts)
    callback_opts = Keyword.take(deployment_opts, [:on_crash, :on_event])

    case Toast.Deployment.start(mode, toast_config, callback_opts) do
      {:ok, deployment} ->
        suite_run = %{suite_run | deployment: deployment}
        ToastTest.DeploymentRegistry.put(suite_module, deployment)

        {stats, test_results} =
          case run_suite_setup(suite_module, deployment) do
            {:ok, extra_context} ->
              ToastTest.DeploymentRegistry.put_extra_context(suite_module, extra_context)
              result = run_suite_tests(suite_run, test_modules, global_opts, suite_opts)
              run_suite_teardown(suite_module, deployment)
              result

            {:error, reason} ->
              mark_all_errored_stats(test_modules, reason, global_opts)
          end

        diagnostics = collect_diagnostics(deployment)
        finalize_suite(suite_module, test_results, diagnostics)
        cleanup_between_suites()
        %{stats: stats, diagnostics: diagnostics}

      {:error, reason} ->
        Logger.error("Deployment failed for suite #{inspect(suite_module)}: #{inspect(reason)}")
        {stats, test_results} = mark_all_errored_stats(test_modules, reason, global_opts)
        finalize_suite(suite_module, test_results, nil)
        cleanup_between_suites()
        %{stats: stats, diagnostics: nil}
    end
  end

  defp run_suite_tests(suite_run, test_modules, global_opts, suite_opts) do
    opts = normalize_opts(Keyword.merge(ExUnit.configuration(), global_opts))
    {manager, stats_pid} = start_event_pipeline(opts)

    config = build_test_config(opts, suite_opts, suite_run, manager, stats_pid)

    :erlang.system_flag(:backtrace_depth, Keyword.fetch!(opts, :stacktrace_depth))

    start_time = System.monotonic_time()
    Compat.suite_started(manager, opts)

    run_suite_modules(config, test_modules)

    stats = collect_suite_stats(config, start_time)
    test_results = take_test_results()
    {stats, test_results}
  end

  defp start_event_pipeline(opts) do
    {:ok, manager} = Compat.start_event_manager()
    {:ok, stats_pid} = Compat.add_runner_stats(manager, opts)

    formatters =
      opts
      |> Keyword.get(:formatters, [])
      |> List.delete(ExUnit.CLIFormatter)
      |> ensure_in_list(ToastTest.CLIFormatter, :front)
      |> ensure_in_list(ToastTest.ResultFormatter, :back)

    for formatter <- formatters do
      Compat.add_formatter(manager, formatter, opts)
    end

    {manager, stats_pid}
  end

  defp ensure_in_list(list, item, :front) do
    if item in list, do: list, else: [item | list]
  end

  defp ensure_in_list(list, item, :back) do
    if item in list, do: list, else: list ++ [item]
  end

  defp build_test_config(opts, suite_opts, suite_run, manager, stats_pid) do
    only_test_ids =
      case Keyword.fetch(suite_opts, :only_test_ids) do
        {:ok, ids} -> ids
        :error -> opts[:only_test_ids]
      end

    %{
      capture_log: opts[:capture_log],
      exclude: opts[:exclude],
      include: opts[:include],
      manager: manager,
      max_failures: opts[:max_failures],
      only_test_ids: only_test_ids,
      runner_pid: self(),
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
    Compat.stop(config.manager)

    after_suite_callbacks = Application.fetch_env!(:ex_unit, :after_suite)
    Enum.each(after_suite_callbacks, fn callback -> callback.(stats) end)

    stats
  end

  defp run_suite_modules(config, test_modules) do
    Enum.each(test_modules, fn module ->
      check_suite_deadline!(config)

      cond do
        reason = aborted?() ->
          emit_skipped_module(config, module, "Suite aborted: " <> abort_display_reason(reason))

        max_failures_reached?(config) ->
          :ok

        true ->
          run_module(config, module)
      end
    end)
  end

  ## Suite helpers

  defp validate_no_async!(test_modules) do
    async_modules =
      Enum.filter(test_modules, fn mod ->
        case Compat.get_test_metadata(mod) do
          %{tags: %{async: true}} -> true
          _ -> false
        end
      end)

    if async_modules != [] do
      names = Enum.map_join(async_modules, ", ", &inspect/1)
      raise "Toast does not support async test modules. Found: #{names}"
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

  defp build_deployment_opts(suite_config, global_opts) do
    base = [
      on_crash: &ToastTest.CrashMonitor.handle_crash/2,
      on_event: &ToastTest.ProcessHistory.handle_event/1
    ]

    suite_args =
      for key <- [:server_args, :coordinator_args, :dbserver_args, :agent_args],
          args = Keyword.get(suite_config, key, %{}),
          args != %{},
          do: {key, args}

    base
    |> Keyword.merge(suite_args)
    |> Keyword.merge(
      Keyword.take(global_opts, [
        :build_dir,
        :work_dir,
        :startup_timeout,
        :shutdown_timeout,
        :sanitizer,
        :show_server_logs,
        :keep_work_dir,
        :cluster_agents,
        :cluster_dbservers,
        :cluster_coordinators,
        :replication_factor
      ])
    )
  end

  defp run_suite_setup(suite_module, deployment) do
    if function_exported?(suite_module, :setup_deployment, 1) do
      suite_module.setup_deployment(deployment)
    else
      {:ok, %{}}
    end
  end

  defp run_suite_teardown(suite_module, deployment) do
    if function_exported?(suite_module, :teardown_deployment, 1) do
      suite_module.teardown_deployment(deployment)
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

  defp mark_all_errored_stats(test_modules, reason, global_opts) do
    opts = normalize_opts(Keyword.merge(ExUnit.configuration(), global_opts))
    {manager, stats_pid} = start_event_pipeline(opts)
    Compat.suite_started(manager, opts)

    for module <- test_modules do
      emit_errored_module(manager, module, reason)
    end

    times_us = %{async: nil, load: nil, run: 0}
    Compat.suite_finished(manager, times_us)
    stats = Compat.stats(stats_pid)
    Compat.stop(manager)
    test_results = take_test_results()
    {stats, test_results}
  end

  defp emit_errored_module(manager, module, reason) do
    test_module = Compat.get_test_metadata(module)
    Compat.module_started(manager, test_module)

    for test <- test_module.tests do
      errored = %{
        test
        | state:
            {:failed,
             [{:error, RuntimeError.exception("Deployment failed: #{inspect(reason)}"), []}]}
      }

      Compat.test_started(manager, errored)
      Compat.test_finished(manager, errored)
    end

    Compat.module_finished(manager, test_module)
  end

  defp collect_diagnostics(deployment) do
    pid_history = ToastTest.ProcessHistory.pids_by_server()
    opts = [pid_history: pid_history]

    case Toast.Deployment.stop_and_collect(deployment, opts) do
      {:ok, diagnostics} -> diagnostics
      {:error, _reason, partial} -> partial
      _ -> nil
    end
  end

  defp cleanup_between_suites do
    ToastTest.StateCleanup.reset()
  end

  defp take_test_results do
    results = Application.get_env(:toast, :__test_results__)
    Application.delete_env(:toast, :__test_results__)
    results
  end

  defp finalize_suite(suite_module, test_results, diagnostics) do
    tests = if test_results, do: ToastTest.ResultFormatter.flat_tests(test_results)

    sanitizer_matching = Toast.Diagnostics.SanitizerMatcher.match(diagnostics, tests)
    crash_matching = Toast.Diagnostics.CrashMatcher.match(diagnostics, tests)
    log_matching = Toast.Diagnostics.LogMatcher.match(diagnostics, tests)

    print_diagnostics_report(
      diagnostics,
      test_results,
      crash_matching,
      sanitizer_matching,
      log_matching
    )

    suite_name = derive_suite_name(suite_module)

    ToastTest.ResultExporter.export(
      suite_name,
      test_results,
      diagnostics,
      sanitizer_matching,
      crash_matching,
      log_matching
    )
  end

  defp derive_suite_name(suite_module) do
    suite_module |> Module.split() |> hd() |> Macro.underscore()
  end

  defp print_diagnostics_report(diagnostics, test_results, crash_matching, sanitizer_matching, log_matching) do
    alias Toast.Diagnostics.Summary

    if crash_matching.matched == [] and crash_matching.unmatched == [] do
      maybe_print(Summary.format_crashed_servers(diagnostics))
    end

    crash_affected = find_crash_affected_tests(crash_matching, test_results)
    maybe_print(Summary.format_crash_attribution(crash_matching, crash_affected))
    maybe_print(Summary.format_sanitizer_issues(sanitizer_matching))
    maybe_print(Summary.format_log_issues(log_matching))
  end

  defp maybe_print(nil), do: :ok
  defp maybe_print(text), do: IO.puts(text)

  defp find_crash_affected_tests(_crash_matching, nil), do: []

  defp find_crash_affected_tests(%{matched: matched, unmatched: unmatched}, test_results) do
    all_crashes = Enum.map(matched, & &1.crash) ++ unmatched
    timestamps = all_crashes |> Enum.map(& &1.timestamp) |> Enum.reject(&is_nil/1)

    case timestamps do
      [] ->
        []

      _ ->
        earliest = Enum.min(timestamps, DateTime)
        attributed = MapSet.new(matched, fn m -> {m.module, m.test} end)

        test_results
        |> ToastTest.ResultFormatter.flat_tests()
        |> Enum.filter(fn t ->
          t.outcome == :failed and
            t.started_at != nil and
            DateTime.compare(t.started_at, earliest) in [:gt, :eq] and
            not MapSet.member?(attributed, {t.module, t.name})
        end)
    end
  end

  defp start_process_history do
    case ToastTest.ProcessHistory.start_link(name: ToastTest.ProcessHistory) do
      {:ok, _pid} -> :ok
      {:error, {:already_started, _pid}} -> :ok
    end
  end

  defp emit_skipped_module(config, module, reason) do
    test_module = Compat.get_test_metadata(module)
    Compat.module_started(config.manager, test_module)

    skipped_tests =
      for test <- test_module.tests do
        %{test | state: {:skipped, reason}}
      end

    for test <- skipped_tests do
      Compat.test_started(config.manager, test)
      Compat.test_finished(config.manager, test)
    end

    Compat.module_finished(config.manager, %{test_module | tests: skipped_tests})
  end

  defp normalize_opts(opts) do
    {include, exclude} = ExUnit.Filters.normalize(opts[:include], opts[:exclude])

    opts
    |> Keyword.put(:exclude, exclude)
    |> Keyword.put(:include, include)
  end

  ## Running modules

  defp run_module(config, module) do
    test_module = Compat.get_test_metadata(module)
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

    if reason = aborted?() do
      finish_aborted_module(config, test_module, invalid_tests, finished_tests, reason)
    else
      finish_pending_module(config, test_module, invalid_tests, finished_tests)
    end
  end

  defp finish_aborted_module(config, test_module, invalid_tests, finished_tests, reason) do
    abort_msg = "Suite aborted: " <> abort_display_reason(reason)

    for test <- invalid_tests do
      skipped = %{test | state: {:skipped, abort_msg}}
      Compat.test_started(config.manager, skipped)
      Compat.test_finished(config.manager, skipped)
    end

    test_module = %{test_module | tests: Enum.reverse(finished_tests, invalid_tests)}
    Compat.module_finished(config.manager, test_module)
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
  end

  ## Test preparation

  defp prepare_tests(config, tests) do
    include = config.include
    exclude = config.exclude
    test_ids = config.only_test_ids
    name_pattern = Map.get(config, :test_name_pattern)

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
    test_name = Atom.to_string(test.name)
    downcased_name = String.downcase(test_name)
    downcased_pattern = String.downcase(pattern)
    String.contains?(downcased_name, downcased_pattern)
  end

  ## Module test execution

  defp run_module_tests(_config, test_module, []) do
    {test_module, [], []}
  end

  defp run_module_tests(config, test_module, tests) do
    Process.put(@current_key, test_module)
    %ExUnit.TestModule{name: module, tags: tags, parameters: params} = test_module

    async? = Map.get(tags, :async, false)
    context = tags |> Map.merge(params) |> Map.merge(%{module: module, async: async?})

    config
    |> run_setup_all(test_module, context, fn context ->
      if max_failures_reached?(config) or aborted?(),
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
    parent_pid = self()

    {module_pid, module_ref} =
      spawn_monitor(fn ->
        ExUnit.OnExitHandler.register(self())

        result =
          try do
            {:ok, Compat.get_setup_all(module, context)}
          catch
            kind, error ->
              failed = failed(kind, error, prune_stacktrace(__STACKTRACE__))
              {:error, %{test_module | state: failed}}
          end

        send(parent_pid, {self(), :setup_all, result})

        ref = Process.monitor(parent_pid)

        receive do
          {^parent_pid, :exit} -> :ok
          {:DOWN, ^ref, _, _, _} -> :ok
        end
      end)

    {ok_or_error, test_module} =
      receive do
        {^module_pid, :setup_all, {:ok, context}} ->
          finished_tests = callback.(context)
          :ok = exit_setup_all(module_pid, module_ref)
          {{:ok, finished_tests}, test_module}

        {^module_pid, :setup_all, {:error, test_module}} ->
          :ok = exit_setup_all(module_pid, module_ref)
          {:error, test_module}

        {:DOWN, ^module_ref, :process, ^module_pid, error} ->
          {:error, %{test_module | state: failed({:EXIT, module_pid}, error, [])}}
      end

    timeout = get_timeout(config, %{})
    {ok_or_error, exec_on_exit(test_module, module_pid, timeout)}
  end

  defp exit_setup_all(pid, ref) do
    send(pid, {self(), :exit})

    receive do
      {:DOWN, ^ref, _, _, _} -> :ok
    end
  end

  defp run_tests(config, tests, params, context) do
    run_tests_loop(config, tests, params, context, [])
  end

  defp run_tests_loop(_config, [], _params, _context, acc) do
    {acc, []}
  end

  defp run_tests_loop(config, [test | rest] = remaining, params, context, acc) do
    check_suite_deadline!(config)

    prev_test =
      case acc do
        [prev | _] -> prev
        [] -> nil
      end

    if aborted?() do
      {acc, remaining}
    else
      case check_between_tests(config, prev_test) do
        {:error, reason} ->
          abort!(reason)
          {acc, remaining}

        :ok ->
          test = %{test | parameters: params}
          Process.put(@current_key, test)

          case run_test(config, test, context) do
            {:ok, test} -> run_tests_loop(config, rest, params, context, [test | acc])
            :max_failures_reached -> {acc, rest}
          end
      end
    end
  end

  @spec check_between_tests(map(), ExUnit.Test.t() | nil) :: :ok | {:error, term()}
  defp check_between_tests(%{suite_run: %{deployment: nil}}, _prev_test), do: :ok

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

  defp check_between_tests(_config, _prev_test), do: :ok

  defp run_test(config, test, context) do
    Compat.test_started(config.manager, test)
    test = spawn_test(config, test, context)

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

  ## Per-test execution

  defp spawn_test(config, test, context) do
    parent_pid = self()
    timeout = get_timeout(config, test.tags)
    {test_pid, test_ref} = spawn_test_monitor(config, test, parent_pid, context)
    test = receive_test_reply(test, test_pid, test_ref, timeout)
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

  defp maybe_capture_log(true, test, fun) do
    maybe_capture_log([], test, fun)
  end

  defp maybe_capture_log(false, _test, fun) do
    fun
  end

  defp maybe_capture_log(capture_log_opts, test, fun) do
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

  defp receive_test_reply(test, test_pid, test_ref, timeout) do
    receive do
      {^test_pid, :test_finished, test} ->
        Process.demonitor(test_ref, [:flush])
        test

      {:DOWN, ^test_ref, :process, ^test_pid, error} ->
        %{test | state: failed({:EXIT, test_pid}, error, [])}
    after
      timeout ->
        case Process.info(test_pid, :current_stacktrace) do
          {:current_stacktrace, stacktrace} ->
            Process.demonitor(test_ref, [:flush])
            Process.exit(test_pid, :kill)

            exception =
              ExUnit.TimeoutError.exception(
                timeout: timeout,
                type: Atom.to_string(test.tags.test_type)
              )

            %{test | state: failed(:error, exception, stacktrace)}

          nil ->
            receive_test_reply(test, test_pid, test_ref, timeout)
        end
    end
  end

  defp exec_test_setup(%ExUnit.Test{module: module} = test, context) do
    if reason = aborted?() do
      {:skipped, %{test | state: {:skipped, "Suite aborted: " <> abort_display_reason(reason)}}}
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
    case ExUnit.OnExitHandler.run(pid, timeout) do
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
    previous = ExUnit.RunnerStats.increment_failure_counter(stats_pid, bump)

    cond do
      previous >= max_failures -> :surpassed
      previous + bump < max_failures -> :no
      true -> {:reached, max_failures - previous}
    end
  end

  defp max_failures_reached?(%{stats_pid: stats_pid, max_failures: max_failures}) do
    max_failures != :infinity and
      ExUnit.RunnerStats.get_failure_counter(stats_pid) >= max_failures
  end

  defp get_timeout(config, tags) do
    base = if config.trace, do: :infinity, else: Map.get(tags, :timeout, config.timeout)

    base =
      if base != :infinity and Map.has_key?(tags, :timeout),
        do: base * config.timeout_factor,
        else: base

    clamp_to_deadline(config.suite_deadline, base)
  end

  defp clamp_to_deadline(nil, timeout), do: timeout

  defp clamp_to_deadline(deadline, timeout) do
    remaining = max(deadline - System.monotonic_time(:millisecond), 1)
    if timeout == :infinity, do: remaining, else: min(timeout, remaining)
  end

  defp check_suite_deadline!(%{suite_deadline: nil}), do: :ok

  defp check_suite_deadline!(%{suite_deadline: deadline}) do
    if System.monotonic_time(:millisecond) >= deadline do
      abort!({:timeout, "Suite timeout exceeded"})
    end
  end

  defp abort_display_reason(reason) do
    case reason do
      {_type, msg} -> msg
      msg when is_binary(msg) -> msg
      _ -> "unknown"
    end
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
