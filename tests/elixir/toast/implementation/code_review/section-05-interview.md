# Section 05 Code Review Interview

## Auto-fixes applied
- **H2 setup_deployment extra_context stored**: Added `put_extra_context`/`get_extra_context` to DeploymentRegistry, updated Case.ex to merge extra context into test context
- **M2 Health check moved per-test**: Added suite_run deployment health check to `check_config_deployments` in `run_tests_loop` (per-test), removed redundant per-module check
- **M5 Fixed double-serialization**: Changed `RuntimeError.exception(inspect(reason))` to `RuntimeError.exception("Deployment failed: #{inspect(reason)}")`
- **L6 Removed duplicate tests**: Removed CrashMonitor tests from runner_test.exs (already in crash_monitor_test.exs)

## Interview decisions

### 1. ExUnitCompat adapter completeness (H1)
**Decision: Route suite-mode calls through Compat**

All new suite-mode code now uses `Compat.*` wrappers instead of direct `EM.*` and `module.__ex_unit__()` calls. This includes `run_module/2`, `emit_skipped_module/3`, `mark_all_errored_stats/3`, and `validate_no_async!/1`. Shared code (`run_setup_all`, `exec_test_setup`) also updated since Compat wrappers are trivial delegates. Legacy functions remain untouched with direct `EM.*` calls.

### 2. SIGQUIT handling (H4)
**Decision: Add SIGQUIT trap to run_suites**

`run_suites/2` now wraps execution in `System.trap_signal(:sigquit, ...)` matching the legacy `run/2` pattern. Suite iteration extracted into `defp do_run_suites/2`.

### 3. apply_toast_env and code deduplication (M3+M4)
**Decision: Clean up both**

- `apply_toast_env` moved from `run/1` to `run_legacy_mode/3` only. Suite mode uses `build_global_opts → Toast.Config.load(opts_to_config_list(opts))`. `opts_to_config_list` now handles `--cluster`/`--single` flags.
- `run_module` and `run_module_legacy` deduplicated: shared body extracted into `finish_module_execution/4`. Suite entry uses Compat for metadata/module_started, legacy entry uses EM directly, shared body uses EM (equivalent for both paths).

## Items let go
- H3: Abort table cleanup ordering — works correctly, just fragile
- M1: Custom between_tests callback — deferred (only `:default` and `false` used currently)
- M6: Registry init/clear asymmetry — functional as-is
- L1-L5: Minor style/coverage items
