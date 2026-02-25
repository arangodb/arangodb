# Section 05 Code Review

## HIGH SEVERITY

### H1. ExUnitCompat adapter created but mostly bypassed
The plan's key architectural requirement is that ALL ExUnit internal API calls go through ExUnitCompat. But in the new suite-mode code, the runner still uses `EM.module_started`, `EM.test_started`, `module.__ex_unit__()`, etc. directly instead of through the adapter. This defeats the isolation purpose.

### H2. setup_deployment/1 extra_context silently discarded
At runner.ex:143, `{:ok, _extra_context}` throws away the extra context. Plan requires storing it alongside the deployment in the registry.

### H3. Abort table cleanup ordering is fragile but functional
StateCleanup.reset() recreates abort table; run_suites only calls clear_abort!() once at start. Works but fragile.

### H4. No SIGQUIT handling in suite mode
run/2 (legacy) traps SIGQUIT for diagnostic dumps. run_suites/2 has no SIGQUIT trap. This is a regression for production debugging.

## MEDIUM SEVERITY

### M1. Custom between_tests callback silently dropped
`_other -> :ok` catch-all ignores custom callbacks. Plan specifies `when is_function(callback, 2)`.

### M2. Health check runs per-module, not per-test
check_between_tests runs in run_suite_modules after each module, not in run_tests_loop between individual tests as the plan specifies.

### M3. apply_toast_env still called, polluting env
Plan says to remove System.put_env calls. apply_toast_env is still called alongside the new build_global_opts.

### M4. ~90 lines duplicated between run_module and run_module_legacy
Nearly identical abort handling, pending test emission, module_finished logic.

### M5. Double-serialization in mark_all_errored_stats
`RuntimeError.exception(inspect(reason))` double-serializes. If reason is "connection refused", tests show `RuntimeError: "connection refused"` with escaped quotes.

### M6. Registry init/clear asymmetry (functional but confusing)

## LOW SEVERITY

### L1. Missing @moduledoc on new modules
### L2. CrashMonitor uses list concatenation instead of IO lists
### L3. Test coverage thin for core orchestration
### L4. after_suite callbacks accessed outside ExUnitCompat
### L5. opts_to_config_list doesn't handle --cluster/--single
### L6. Duplicate CrashMonitor tests in two files
