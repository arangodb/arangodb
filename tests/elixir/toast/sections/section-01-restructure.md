Now I have a thorough understanding of the current state and the target. Let me generate the section content.

# Section 1: Project Restructure

## Overview

Flatten the existing umbrella project into a single Mix project. Move files from `apps/toast/` and `apps/smoke_test/` into the new structure. Rename modules to establish the `Toast.*` (infrastructure library) and `ToastTest.*` (test framework) namespace boundary. No functional changes -- same behavior, flat structure.

This section is the foundation for all subsequent sections. Nothing else can proceed until this is complete.

## Current State

The project at `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/` is an Elixir umbrella with two apps:

```
tests/elixir/toast/
  mix.exs                    # Umbrella root (Toast.Umbrella.MixProject)
  apps/
    toast/                   # Core framework (Toast.MixProject, app: :toast)
      lib/toast/
      test/
      mix.exs
    smoke_test/              # Example test suite (SmokeTest.MixProject, app: :smoke_test)
      lib/
      test/
      mix.exs
```

The umbrella root `mix.exs` defines `Toast.Umbrella.MixProject` with `apps_path: "apps"` and no dependencies. The `toast` app has dependencies `{:req, "~> 0.5"}` and `{:erlexec, "~> 2.0"}`. The `smoke_test` app depends on `{:toast, in_umbrella: true}`.

## Target State

```
tests/elixir/toast/
  mix.exs                    # Single project (Toast.MixProject, app: :toast)
  lib/
    toast/                   # Infrastructure library (no ExUnit deps)
      deployment/
      process/
      diagnostics/
      client.ex
      config.ex
      port_allocator.ex
      log_formatter.ex
      application.ex
    toast_test/              # Test framework (ExUnit integration)
      runner.ex
      case.ex
      cli_formatter.ex
      result_formatter.ex
      result_exporter/
  test/                      # Unit tests for the framework
    toast/
    toast_test/
    test_helper.exs
  suites/                    # Integration test suites
    smoke/
      test_version.exs
      test_collection.exs
      test_aql.exs
```

## Verification (Tests)

This section has no unit tests to write. It is a structural reorganization verified by:

1. `mix compile` succeeds with zero warnings about undefined modules
2. `mix test` runs the existing unit tests (from `apps/toast/test/`) and they all pass
3. `mix toast` runs the smoke tests (from `apps/smoke_test/test/`) and they all pass
4. `mix xref graph` shows no broken references

These are the acceptance criteria. Run each after completing the implementation steps below.

## Implementation Steps

### Step 1: Create the new root `mix.exs`

Replace the umbrella root `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/mix.exs` with a single-project `mix.exs`. The current umbrella file defines:

```elixir
defmodule Toast.Umbrella.MixProject do
  use Mix.Project

  def project do
    [
      apps_path: "apps",
      version: "0.1.0",
      start_permanent: Mix.env() == :prod,
      deps: deps()
    ]
  end

  defp deps do
    []
  end
end
```

Replace it with a combined single-project definition. Key changes:

- Module name: `Toast.MixProject` (not `Toast.Umbrella.MixProject`)
- Remove `apps_path: "apps"` -- this is what makes it an umbrella
- Add `app: :toast`
- Add `elixir: "~> 1.19"` (from the toast app's mix.exs)
- Merge dependencies from both apps: `{:req, "~> 0.5"}` and `{:erlexec, "~> 2.0"}` (smoke_test's `{:toast, in_umbrella: true}` is no longer needed since everything is one project)
- Add `elixirc_paths: elixirc_paths(Mix.env())` where `elixirc_paths(:test)` returns `["lib"]` and the default also returns `["lib"]` (both are the same for now; the distinction matters later when `toast_test` modules need to be available during test compilation)
- Add `test_paths: ["test"]` to keep unit tests separate from suites
- Add `mod: {Toast.Application, []}` in the `application/0` function
- Remove `build_path`, `config_path`, `deps_path`, `lockfile` overrides (these were only needed for umbrella sub-apps to share paths)

The resulting `mix.exs` should look like:

```elixir
defmodule Toast.MixProject do
  use Mix.Project

  def project do
    [
      app: :toast,
      version: "0.1.0",
      elixir: "~> 1.19",
      start_permanent: Mix.env() == :prod,
      test_paths: ["test"],
      deps: deps()
    ]
  end

  def application do
    [
      extra_applications: [:logger],
      mod: {Toast.Application, []}
    ]
  end

  defp deps do
    [
      {:req, "~> 0.5"},
      {:erlexec, "~> 2.0"}
    ]
  end
end
```

### Step 2: Move `apps/toast/lib/` to `lib/`

Move all files from `apps/toast/lib/` to the project root `lib/`. This preserves the existing directory structure under `lib/toast/`. The files to move:

**Source**: `apps/toast/lib/`
**Destination**: `lib/`

This means:
- `apps/toast/lib/toast.ex` -> `lib/toast.ex`
- `apps/toast/lib/toast/application.ex` -> `lib/toast/application.ex`
- `apps/toast/lib/toast/config.ex` -> `lib/toast/config.ex`
- `apps/toast/lib/toast/client.ex` -> `lib/toast/client.ex`
- `apps/toast/lib/toast/deployment.ex` -> `lib/toast/deployment.ex`
- `apps/toast/lib/toast/deployment/` -> `lib/toast/deployment/` (entire directory)
- `apps/toast/lib/toast/process/` -> `lib/toast/process/` (entire directory)
- `apps/toast/lib/toast/diagnostics/` -> `lib/toast/diagnostics/` (entire directory)
- `apps/toast/lib/toast/utils/` -> `lib/toast/utils/` (entire directory)
- `apps/toast/lib/toast/log_formatter.ex` -> `lib/toast/log_formatter.ex`
- `apps/toast/lib/mix/tasks/toast.ex` -> `lib/mix/tasks/toast.ex`
- `apps/toast/lib/mix/tasks/toast.gen.suite.ex` -> `lib/mix/tasks/toast.gen.suite.ex`

And the modules that will be renamed (moved to `toast_test` namespace) in Step 5:
- `apps/toast/lib/toast/runner.ex` -> temporarily to `lib/toast/runner.ex`
- `apps/toast/lib/toast/test_case.ex` -> temporarily to `lib/toast/test_case.ex`
- `apps/toast/lib/toast/cli_formatter.ex` -> temporarily to `lib/toast/cli_formatter.ex`
- `apps/toast/lib/toast/result_formatter.ex` -> temporarily to `lib/toast/result_formatter.ex`
- `apps/toast/lib/toast/result_exporter.ex` -> temporarily to `lib/toast/result_exporter.ex`
- `apps/toast/lib/toast/result_exporter/` -> temporarily to `lib/toast/result_exporter/`

It is cleaner to move everything first, then rename in Step 5.

### Step 3: Move `apps/toast/test/` to `test/`

Move all framework unit tests from `apps/toast/test/` to the project root `test/`. Preserve the directory structure.

**Source**: `apps/toast/test/`
**Destination**: `test/`

This means:
- `apps/toast/test/test_helper.exs` -> `test/test_helper.exs`
- `apps/toast/test/config_test.exs` -> `test/config_test.exs`
- `apps/toast/test/client_test.exs` -> `test/client_test.exs`
- `apps/toast/test/cli_formatter_test.exs` -> `test/cli_formatter_test.exs`
- `apps/toast/test/result_formatter_test.exs` -> `test/result_formatter_test.exs`
- `apps/toast/test/result_exporter_test.exs` -> `test/result_exporter_test.exs`
- `apps/toast/test/test_case_test.exs` -> `test/test_case_test.exs`
- `apps/toast/test/deployment/` -> `test/deployment/` (entire directory)
- `apps/toast/test/process/` -> `test/process/` (entire directory)
- `apps/toast/test/diagnostics/` -> `test/diagnostics/` (entire directory)
- `apps/toast/test/utils/` -> `test/utils/` (entire directory)
- `apps/toast/test/result_exporter/` -> `test/result_exporter/` (entire directory)

The existing `test/test_helper.exs` content is `ExUnit.start(exclude: [:integration])`. This should be preserved as-is. **Invariant**: `test/test_helper.exs` must contain only `ExUnit.start()` and optional test configuration — no deployment startup code. Unit tests (`mix test`) must not start ArangoDB servers.

### Step 4: Move `apps/smoke_test/test/` to `suites/smoke/`

Move the smoke test files into the new `suites/` directory structure. At this stage, the smoke tests keep their existing module names and `use Toast.TestCase` -- they will not yet be converted to the new suite system (that happens in section-04-suite-system and section-05-runner).

**Source**: `apps/smoke_test/test/`
**Destination**: `suites/smoke/`

The files to move:
- `apps/smoke_test/test/test_helper.exs` -> `suites/smoke/test_helper.exs`
- `apps/smoke_test/test/smoke_test/version_test.exs` -> `suites/smoke/test_version.exs`
- `apps/smoke_test/test/smoke_test/collection_test.exs` -> `suites/smoke/test_collection.exs`
- `apps/smoke_test/test/smoke_test/aql_test.exs` -> `suites/smoke/test_aql.exs`

Note the file renaming: the target convention uses `test_` prefix instead of `_test` suffix, and the files are flattened out of the `smoke_test/` subdirectory. For this phase, the naming is cosmetic preparation; the `mix toast` task still uses the existing `*_test.exs` pattern to discover tests.

**Important**: The `mix toast` task needs to know where to find these files. Currently it uses `Mix.Project.config()[:test_paths]` which defaults to `["test"]`. After the move, `mix toast` must look in `suites/` instead. Update the `default_test_paths/0` function in `lib/mix/tasks/toast.ex` from:

```elixir
defp default_test_paths do
  if File.dir?("test"), do: ["test"], else: []
end
```

to:

```elixir
defp default_test_paths do
  if File.dir?("suites"), do: ["suites"], else: []
end
```

Also update the `test_pattern` used for file discovery. The current pattern is `"*_test.exs"`. For this phase, keep supporting both `*_test.exs` (legacy) and `test_*.exs` (new convention) by using `"{test_*,*_test}.exs"` as the pattern. Once the migration to the new naming convention is complete (section-04-suite-system), the pattern simplifies to `"test_*.exs"`.

Also remove the `@recursive true` attribute from the mix task. This attribute tells Mix to run the task recursively in umbrella sub-apps -- it is no longer needed in a flat project and would cause errors.

The `smoke/test_helper.exs` content remains:

```elixir
ExUnit.start()

case Toast.TestCase.setup_suite() do
  {:ok, _} -> :ok
  {:error, _} -> ExUnit.configure(exclude: [:toast_suite])
end
```

The `mix toast` task already loads `test_helper.exs` files from the test paths, so this continues to work.

### Step 5: Rename modules to establish namespace boundary

Rename the ExUnit-dependent modules from `Toast.*` to `ToastTest.*`. This establishes the architectural boundary: `Toast.*` is the infrastructure library, `ToastTest.*` is the test framework.

**Module renames**:

| Old Name | New Name | Old File | New File |
|----------|----------|----------|----------|
| `Toast.Runner` | `ToastTest.Runner` | `lib/toast/runner.ex` | `lib/toast_test/runner.ex` |
| `Toast.TestCase` | `ToastTest.Case` | `lib/toast/test_case.ex` | `lib/toast_test/case.ex` |
| `Toast.CLIFormatter` | `ToastTest.CLIFormatter` | `lib/toast/cli_formatter.ex` | `lib/toast_test/cli_formatter.ex` |
| `Toast.ResultFormatter` | `ToastTest.ResultFormatter` | `lib/toast/result_formatter.ex` | `lib/toast_test/result_formatter.ex` |
| `Toast.ResultExporter` | `ToastTest.ResultExporter` | `lib/toast/result_exporter.ex` | `lib/toast_test/result_exporter.ex` |
| `Toast.ResultExporter.JSON` | `ToastTest.ResultExporter.JSON` | `lib/toast/result_exporter/json.ex` | `lib/toast_test/result_exporter/json.ex` |
| `Toast.ResultExporter.JUnitXML` | `ToastTest.ResultExporter.JUnitXML` | `lib/toast/result_exporter/junit_xml.ex` | `lib/toast_test/result_exporter/junit_xml.ex` |

For each rename:

1. Create the `lib/toast_test/` directory (and subdirectories as needed)
2. Move the file to its new location
3. Change the `defmodule` declaration to the new name
4. Update all references to the old name throughout the codebase

**Backward compatibility**: Keep `Toast.TestCase` as a deprecated alias pointing to `ToastTest.Case`. Create a thin wrapper at `lib/toast/test_case.ex`:

```elixir
defmodule Toast.TestCase do
  @moduledoc deprecated: "Use ToastTest.Case instead"

  defmacro __using__(opts) do
    quote do
      use ToastTest.Case, unquote(opts)
    end
  end

  defdelegate setup_suite!(), to: ToastTest.Case
  defdelegate setup_suite!(mode), to: ToastTest.Case
  defdelegate setup_suite!(mode, opts), to: ToastTest.Case
  defdelegate setup_suite(), to: ToastTest.Case
  defdelegate setup_suite(mode), to: ToastTest.Case
  defdelegate setup_suite(mode, opts), to: ToastTest.Case
  defdelegate register_deployment(deployment), to: ToastTest.Case
  defdelegate get_deployment(), to: ToastTest.Case
end
```

This ensures the existing smoke test files (which `use Toast.TestCase`) continue to work without modification during this phase. The smoke tests will be migrated to the new suite system in later sections.

### Step 6: Update all module references

After renaming, search the entire codebase for references to the old module names and update them. Key locations to check:

**In `lib/toast/deployment.ex`**:
- `Toast.Runner.abort!("Server crashed: #{id}")` in `crash_monitor_loop/0` -> `ToastTest.Runner.abort!("Server crashed: #{id}")`

**In `lib/mix/tasks/toast.ex`**:
- `Toast.Runner.run(options, ...)` -> `ToastTest.Runner.run(options, ...)`
- `Toast.Runner.aborted?()` -> `ToastTest.Runner.aborted?()`

**In `lib/toast_test/case.ex`** (the renamed `ToastTest.Case`):
- `Toast.CLIFormatter` -> `ToastTest.CLIFormatter`
- `Toast.ResultFormatter` -> `ToastTest.ResultFormatter`
- `Toast.ResultExporter.export()` -> `ToastTest.ResultExporter.export()`

**In `lib/toast_test/cli_formatter.ex`** (the renamed `ToastTest.CLIFormatter`):
- `Toast.Runner.aborted?()` -> `ToastTest.Runner.aborted?()`

**In `lib/toast_test/result_exporter.ex`** (the renamed `ToastTest.ResultExporter`):
- Internal alias `Toast.ResultExporter.{JSON, JUnitXML}` -> `ToastTest.ResultExporter.{JSON, JUnitXML}`

**In `lib/toast/application.ex`**:
- `Toast.ResultExporter.result_dir()` -> `ToastTest.ResultExporter.result_dir()`

**In test files** (`test/`):
- Any test that references `Toast.Runner` -> `ToastTest.Runner`
- Any test that references `Toast.TestCase` -> `ToastTest.Case`
- Any test that references `Toast.CLIFormatter` -> `ToastTest.CLIFormatter`
- Any test that references `Toast.ResultFormatter` -> `ToastTest.ResultFormatter`
- Any test that references `Toast.ResultExporter` -> `ToastTest.ResultExporter`

**In suite files** (`suites/smoke/`):
- The smoke tests `use Toast.TestCase` which is preserved as a deprecated alias, so no changes needed here yet.

Use `mix xref graph` and `grep -r "Toast.Runner\|Toast.TestCase\|Toast.CLIFormatter\|Toast.ResultFormatter\|Toast.ResultExporter"` to find all references. Be careful not to rename references to `Toast.ResultExporter` in the `@results_key`, `@diagnostics_key` etc. Application env keys -- those use atom keys like `:__test_results__` under the `:toast` application, which is correct and should not change.

**Important special case**: The `Toast.Application` module references `Toast.ResultExporter.result_dir()` in its `setup_file_logger/0` function. Since `Toast.Application` is in `lib/toast/` (the infrastructure library) but `ToastTest.ResultExporter` is in `lib/toast_test/` (the test framework), this creates a dependency from the library into the test framework. For this phase, this is acceptable -- the clean separation is completed in section-02-library-extraction. For now, just update the reference to `ToastTest.ResultExporter.result_dir()`.

### Step 7: Update the `toast.gen.suite` mix task

The `Mix.Tasks.Toast.Gen.Suite` task at `lib/mix/tasks/toast.gen.suite.ex` generates new umbrella apps. It needs to be updated for the flat project structure. The generated files should create a suite folder under `suites/` instead of an umbrella app under `apps/`.

For this phase, the task can be simplified or stubbed since it will be significantly reworked when the suite system is implemented (section-04-suite-system). The minimal change: update it to generate files in `suites/<name>/` instead of `apps/<name>/`, remove the `mix.exs` generation (no per-suite mix.exs in the flat project), and update the template to use `ToastTest.Case` (or keep using `Toast.TestCase` since the alias exists).

### Step 8: Update `.formatter.exs`

Move `apps/toast/.formatter.exs` to the project root (or create a new one) to cover the new structure:

```elixir
[
  inputs: ["{mix,.formatter}.exs", "{config,lib,test,suites}/**/*.{ex,exs}"]
]
```

### Step 9: Update `.gitignore`

Add `.toast.local.exs` to the project's `.gitignore` (preparation for section-02-library-extraction's local config support):

```
.toast.local.exs
```

### Step 10: Delete `apps/` directory

After verifying everything works (Steps 11-12), delete the entire `apps/` directory. Also remove the `apps/smoke_test/lib/smoke_test.ex` placeholder module -- it was only needed as the umbrella app's library entry point.

### Step 11: Verify `mix test` works

Run `mix test` from the project root. All existing unit tests from `apps/toast/test/` (now in `test/`) should pass. If any tests reference old module names, fix them.

### Step 12: Verify `mix toast` works

Run `mix toast` (with appropriate `TOAST_BUILD_DIR`) from the project root. The smoke tests from `suites/smoke/` should load and run against a live ArangoDB deployment, producing the same results as before the restructure.

## Dependencies

This section has no dependencies on other sections. All subsequent sections depend on this one.

## Key Decisions

- **File naming in `suites/`**: Files are renamed from `*_test.exs` (ExUnit convention) to `test_*.exs` (Toast convention) during the move. The `mix toast` task supports both patterns during the transition.
- **Deprecated alias for `Toast.TestCase`**: A thin `Toast.TestCase` wrapper that delegates to `ToastTest.Case` is preserved so existing test files continue to compile without changes.
- **`Toast.Application` -> `ToastTest.ResultExporter` dependency**: This cross-boundary reference is acceptable temporarily and will be cleaned up in section-02-library-extraction.
- **No functional changes**: The restructure changes only file locations and module names. All runtime behavior is identical before and after.

## File Inventory

### Files created

- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/mix.exs` (replaced)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/.formatter.exs` (new at root)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/runner.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/case.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/cli_formatter.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_formatter.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter/json.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast_test/result_exporter/junit_xml.ex`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/test_case.ex` (deprecated alias)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/suites/smoke/test_helper.exs`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/suites/smoke/test_version.exs`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/suites/smoke/test_collection.exs`
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/suites/smoke/test_aql.exs`

### Files moved (from `apps/toast/lib/` to `lib/`)

All files listed in Step 2. The Toast infrastructure modules keep their existing names and paths under `lib/toast/`.

### Files moved (from `apps/toast/test/` to `test/`)

All files listed in Step 3.

### Files modified

- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex` (update `Toast.Runner` reference)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/application.ex` (update `Toast.ResultExporter` reference)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/mix/tasks/toast.ex` (update runner references, remove `@recursive`, update `default_test_paths`)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/mix/tasks/toast.gen.suite.ex` (update for flat structure)
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/.gitignore` (add `.toast.local.exs`)
- All test files in `test/` that reference renamed modules

### Files deleted

- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/apps/` (entire directory, after verification)