# 🥑 TOAST 🍞

**TO**olkit for **A**rango **S**ystem **T**oasting, ah, Testing is an Elixir-based
integration testing framework for ArangoDB. It manages ArangoDB server deployments
(single-server and cluster), runs tests against them, collects diagnostics (crash
logs, sanitizer errors, core dumps), and produces CI-friendly reports (JSON, JUnit XML).

## Installation

- Install elixir (>= 1.19) and erlang (>= 17)
  A version manager like asdf or mise let's you pick a specific version (and actually manage
  multiple concurrently installed versions) so you do not depend on the versions available
  in your OS distribution's package management.
- Install hex (package manager) and rebar (Erlang build system) with `mix local.hex` and `mix local.rebar`
- Pull all dependent packages via `mix deps.get`

## Quick Start

```bash
# Run all suites against a local build
mix toast --build-dir /path/to/build

# Run a specific suite
mix toast --build-dir /path/to/build smoke

# Run in cluster mode
mix toast --build-dir /path/to/build --cluster

# Run with debugger (pauses after deployment, disables test timeouts)
mix toast --build-dir /path/to/build --attach-debugger
```

A minimal test suite consists of two files under `suites/<name>/`:

```
suites/
  smoke/              # Suite name
    suite.ex          # Suite definition
    test_version.exs  # Test file (must start with test_)
```

**suite.ex:**
```elixir
defmodule Smoke.Suite do
  use ToastTest.Suite
end
```

**test_version.exs:**
```elixir
defmodule Smoke.VersionTest do
  use Smoke.Suite

  test "returns arango server info", %{client: client} do
    body = Client.Admin.version!(client)
    assert body["server"] == "arango"
  end
end
```

Note: the module names of all files belonging to a suite must be namespaced with the suite name.

## Toast CLI

Instead of running `mix toast` directly from the `tests/toast/` directory,
you can use the `toast` wrapper script from anywhere in the repository. It
automatically finds the correct `toast` directory and forwards to `mix toast`.

Set it up once:

```bash
tests/toast/toast --setup-completion
```

This adds a `toast` shell function and bash completion to your `~/.bashrc`.
After restarting your shell (or `source ~/.bashrc`), you get:

- **`toast run [options] [suites...]`** — runs tests (equivalent to `mix toast`)
- **`toast analyze [subcommand] [options]`** — analyzes results (equivalent to `mix toast.analyze`)
- **Tab completion** for suites, test files within suites, options, and analyze subcommands

Be aware that when you give the build directory to the toast script, you still have to give it 
relative to the toast directory, e.g. `toast run --build ../../build` although you run the script 
from somewhere else in the directory.

## Running Tests

### Command-Line Usage

```
mix toast [options] [suites...]
```

When no suite names are given, all suites under `suites/` are run. Suite names
correspond to directory names:

```bash
mix toast                           # Run all suites
mix toast smoke                     # Run only the "smoke" suite
mix toast smoke resilience          # Run multiple suites
```

### Filtering Tests

Filter by file within a suite using `<suite_name>/<file>`:

```bash
mix toast smoke/test_version.exs
```

Filter by line number to run a single test:

```bash
mix toast smoke/test_version.exs:4
```

Filter by test name substring (case-insensitive):

```bash
mix toast --test "version"
```

Filter by tag:

```bash
mix toast --only cluster_only
mix toast --exclude slow
mix toast --include edge_case
```

### Toast Options

| Option | Description |
|---|---|
| `--build-dir PATH` / `-b` | Path to ArangoDB build directory |
| `--base-dir PATH` | Base directory for server data and logs |
| `--result-dir PATH` | Output directory for test results (default: `toast-results`) |
| `--cluster` | Use cluster deployment |
| `--single` | Use single-server deployment (default) |
| `--test NAME` | Filter tests by name substring (case-insensitive) |
| `--show-server-logs` | Print arangod output to stdout |
| `--global-timeout MS` | Global timeout in milliseconds (default: 3600000) |
| `--test-timeout MS` | Per-test timeout in milliseconds (default: 300000) |
| `--startup-timeout MS` | Server startup timeout in milliseconds (default: 60000) |
| `--shutdown-timeout MS` | Server shutdown timeout in milliseconds (default: 60000) |
| `--timeout-factor N` | Timeout multiplier (default: 1, auto-set to 3 for sanitizer builds, 10 for rr) |
| `--keep-data` | Keep server data and logs even on success |
| `--sanitizer TYPE` | Sanitizer type: `tsan` or `alubsan` (auto-detected from build dir) |
| `--attach-debugger` | Pause after deployment for live debugger attachment (disables test timeouts) |
| `--rr ROLES` | Record with rr: `default`, `all`, or comma-separated roles (single, agent, dbserver, coordinator). `default` records single server or dbserver+coordinator in cluster mode |
| `--http2` | Use HTTP/2 (h2c) for client requests (default: HTTP/1.1) |
| `--memory-budget BYTES` | Memory budget for server processes (auto-detected from system) |
| `--cluster-agents N` | Number of agency nodes (default: 3) |
| `--cluster-dbservers N` | Number of DB servers (default: 3) |
| `--cluster-coordinators N` | Number of coordinators (default: 1) |
| `--replication-factor N` | Default replication factor (default: 2) |
| `--test-buckets TOTAL/INDEX` | Run only bucket INDEX of TOTAL (0-indexed). See [Test Bucketing](#test-bucketing). |
| `--ci` | Enable CI mode (packages results for upload) |
| `--force-all-tiers` | Package all tiers regardless of outcome (CI only) |
| `--no-agency-dump` | Skip agency state dump on error |

### Filtering Options

| Option | Description |
|---|---|
| `--include TAG` / `-i` | Include tests matching the filter |
| `--exclude TAG` / `-e` | Exclude tests matching the filter |
| `--only TAG` | Run only tests matching the filter (excludes all others) |
| `--max-failures N` | Stop after N failures |

## Writing Tests

### Test Suites

A test suite defines a deployment configuration and groups related test files.
Each suite lives in its own directory under `suites/` and must contain a
`suite.ex` file.

```elixir
defmodule Smoke.Suite do
  use ToastTest.Suite
end
```

The `use ToastTest.Suite` macro accepts configuration options:

| Option | Default | Description |
|---|---|---|
| `mode` | `:auto` | Deployment mode: `:single_server`, `:cluster`, `:auto` (follows CLI), or `:manual` (no automatic deployment) |
| `timeout` | `3_600_000` | Suite-level timeout in milliseconds |
| `server_args` | `%{}` | Extra arangod CLI arguments as a map (e.g., `%{"log.level" => "debug"}`) |
| `coordinator_args` | `%{}` | Extra arguments for coordinators (cluster mode) |
| `dbserver_args` | `%{}` | Extra arguments for DB servers (cluster mode) |
| `agent_args` | `%{}` | Extra arguments for agents (cluster mode) |
| `between_tests` | `:default` | Health check behavior between tests (`:default` or `false`) |
| `authentication` | `false` | Enable JWT authentication for the deployment |
| `jwt_algorithm` | `:hmac` | JWT signing algorithm: `:hmac` or `:ecdsa` |

Server arguments are specified as maps where keys are arangod option names and
values are strings or lists of strings:

```elixir
defmodule MyApp.Suite do
  use ToastTest.Suite,
    server_args: %{
      "log.level" => "debug",
      "query.memory-limit" => "1073741824"
    }
end
```

Example with explicit cluster configuration:

```elixir
defmodule Resilience.Suite do
  use ToastTest.Suite,
    mode: :cluster,
    authentication: true,
    jwt_algorithm: :ecdsa
end
```

### JavaScript Test Suites

Toast can run legacy JavaScript tests (jsunity/arangosh-based) through the same
framework. Each JS file in the configured directories becomes a test module that
participates in Toast's full pipeline -- filtering, bucketing, result collection,
diagnostics, JUnit XML, and JSON export.

#### Defining a JS Suite

```elixir
defmodule ShellClientAql.Suite do
  use ToastTest.JavascriptSuite,
    paths: ["tests/js/client/aql", "tests/js/common/aql"],
    mode: :auto,
    server_args: %{"log.level" => "debug"},
    js_extra_args: %{"agency.supervision-ok-threshold" => "15"}
end
```

`use ToastTest.JavascriptSuite` accepts all `ToastTest.Suite` options plus:

| Option | Default | Description |
|---|---|---|
| `paths` | *(required)* | List of directories containing JS test files, relative to the repository root |
| `js_extra_args` | `%{}` | Extra arangosh arguments passed to the JS test runner |
| `weights` | `%{}` | Map of `%{"filename.js" => weight}` for bucketing; unspecified files get weight 1 |

#### How It Works

- Each `.js` file in the configured paths becomes a separate test module
- JS files are run via arangosh using `@arangodb/testrunner.runCommandLineTests()`
- Individual test results from jsunity are mapped back to ExUnit-compatible results
- All Toast features work: filtering by suite/file, test bucketing, diagnostics

#### Running JS Suites

JS suites are run the same way as Elixir suites:

```bash
mix toast shell_client_aql
mix toast shell_client_aql/aql-projections.js
```

#### Filename-Based Tags

Tags are automatically derived from JS filename suffixes. Segments separated by
hyphens are matched against a known set of conventions -- for example,
`aql-join-cluster.js` contains the segment `cluster` and gets the tag
`cluster_only: true`.

Common segment-to-tag mappings: `cluster` -> `:cluster_only`, `noncluster` ->
`:single_only`, `fp` -> `:failure_points`, `nightly` -> `:nightly`.

#### Module Naming

In analyze output, JUnit XML, and JSON results, JS test modules are identified
by their file path (e.g., `tests/js/client/aql/aql-projections.js`) rather than
a generated Elixir module name.

### Suite Callbacks

Suites can implement optional callbacks for lifecycle hooks:

```elixir
defmodule MyApp.Suite do
  use ToastTest.Suite

  @impl ToastTest.Suite
  def setup_deployment(deployment) do
    # Called once after the deployment starts, before any tests run.
    # Use this to create databases, collections, or seed data.
    client = Toast.Client.new(Toast.Deployment.default_endpoint(deployment))
    Toast.Client.Collection.create!(client, "shared_data")
    {:ok, %{shared_collection: "shared_data"}}
  end

  @impl ToastTest.Suite
  def teardown_deployment(deployment) do
    # Called once after all tests complete, before the deployment stops.
    :ok
  end

  @impl ToastTest.Suite
  def between_tests(deployment, _prev_test) do
    # Called between each test. Return :ok or {:error, reason}.
    # The default behavior runs two phases:
    #   1. CrashBarrier — checks /proc/<pid>/status for in-flight crashes
    #   2. HealthBarrier — waits for each server's health monitor to report healthy
    case Toast.Deployment.status(deployment) do
      :ready -> :ok
      other -> {:error, "Deployment not ready (status: #{other})"}
    end
  end
end
```

The map returned from `setup_deployment/1` is merged into the test context,
making its keys available to all tests in the suite.

### Manual Deployments

Suites with `mode: :manual` skip the automatic deployment lifecycle. No
deployment is started before tests, and the test context does not include
`deployment`, `endpoint`, or `client`. Between-test health checks are
disabled. This is useful for tests that need to start and stop deployments
as part of the test itself.

```elixir
defmodule Lifecycle.Suite do
  use ToastTest.Suite, mode: :manual
end
```

Tests in manual suites use the `with_deployment` helper to create scoped
deployments:

```elixir
defmodule Lifecycle.StartupTest do
  use Lifecycle.Suite

  test "server starts and responds" do
    with_deployment fn deployment ->
      endpoint = Toast.Deployment.default_endpoint(deployment)
      client = Toast.Client.new(endpoint)
      body = Client.Admin.version!(client)
      assert body["server"] == "arango"
    end
  end

  test "cluster deployment" do
    with_deployment [mode: :cluster, cluster_dbservers: 2], fn deployment ->
      assert Toast.Deployment.status(deployment) == :ready
    end
  end
end
```

`with_deployment` accepts an optional keyword list of deployment options
(`:mode`, `:server_args`, `:authentication`, `:cluster_dbservers`, etc.)
and a function. It starts a deployment, calls the function, and guarantees
shutdown even if the function raises. When `:mode` is `:auto` (the default),
the global deployment mode from CLI/environment is used.

The `with_deployment` helper is also available in non-manual suites for tests
that need an additional deployment beyond the suite's primary one.

### Test Files

Test files must be placed in the suite directory and their filenames must start
with `test_` and end with `.exs`. Any `.ex` files (except `suite.ex`) in the
suite directory are compiled as helpers.

```elixir
defmodule Smoke.CollectionTest do
  use Smoke.Suite

  setup %{client: client} do
    name = "test_coll_#{System.unique_integer([:positive])}"
    on_exit(fn -> Client.Collection.drop(client, name) end)
    %{collection: name}
  end

  test "create and list collection", %{client: client, collection: name} do
    Client.Collection.create!(client, name)
    collections = Client.Collection.list!(client, exclude_system: true)
    assert Enum.any?(collections, &(&1["name"] == name))
  end
end
```

Every test receives the following in its context:

| Key | Type | Description |
|---|---|---|
| `deployment` | `Toast.Deployment.t()` | The active deployment struct |
| `endpoint` | `String.t()` | The primary HTTP endpoint URL |
| `client` | `Toast.Client.t()` | Pre-configured REST client |

The `use Smoke.Suite` line automatically aliases `Toast.Client` as `Client` and
imports `ToastTest.Expect`, so you can write `Client.Admin.version(client)` and
use the `expect` macro directly.

### Deployment Mode Tags

Tests can be restricted to a specific deployment mode:

```elixir
@tag :cluster_only
test "sharding works", %{client: client} do
  # Only runs with --cluster, skipped in single-server mode
end

@tag :single_only
test "local feature", %{client: client} do
  # Only runs in single-server mode, skipped in cluster mode
end
```

These also work as `@moduletag` to apply to all tests in a module.

### Non-Fatal Expectations

The `expect` macro works like `assert` but does not stop test execution on
failure -- similar to Google Test's `EXPECT_*` macros. This allows a single test
to report multiple independent failures:

```elixir
test "multiple independent checks", %{client: client} do
  expect {:ok, _} = Client.Admin.version(client)
  expect {:ok, _} = Client.Admin.engine(client)
  expect {:ok, _} = Client.Admin.statistics(client)
end
```

If any expectations fail, the test is marked as failed with all recorded
failures. Variable bindings from pattern matches inside `expect` are unreliable
when the expectation fails -- use `assert` when you need matched values for
subsequent code.

## Deployments

Toast manages the full lifecycle of ArangoDB server processes.

### Single-Server Mode

The default mode starts a single `arangod` process. The `endpoint` in the test
context points directly to this server.

### Cluster Mode

Cluster mode starts a full ArangoDB cluster with configurable topology:

- **Agents** -- Raft-based consensus nodes (default: 3)
- **DB Servers** -- Data storage nodes (default: 3)
- **Coordinators** -- Query routing nodes (default: 1)

The `endpoint` in the test context points to the first coordinator.

### Deployment Lifecycle

For each suite:

1. A deployment is started according to the suite's mode and configuration
2. `setup_deployment/1` is called (if defined)
3. Tests are run sequentially; between each test, a health check verifies the
   deployment is still healthy
4. `teardown_deployment/1` is called (if defined)
5. The deployment is stopped and diagnostics are collected
6. On failure, the work directory (logs, data) is preserved; on success, it is
   cleaned up (unless `--keep-data` is set)

## Client API

`Toast.Client` is a thin REST client for ArangoDB. Tests receive a
pre-configured client in their context. All client modules have bang variants
that raise on error -- use those in tests. See the `@moduledoc` on each
`Toast.Client.*` module for the full API.

Available modules: `Collection`, `Document`, `AQL`, `Database`, `Index`,
`Graph`, `Vertex`, `Edge`, `View`, `Analyzer`, `User`, `Transaction`, `Admin`.

```elixir
# Collections and documents
Client.Collection.create!(client, "users")
meta = Client.Document.insert!(client, "users", %{"name" => "Alice"})
doc = Client.Document.get!(client, "users", meta["_key"])

# AQL queries (cursor pagination is handled automatically)
results = Client.AQL.execute!(client, "FOR u IN users RETURN u")

# Database scoping
db_client = Client.with_database(client, "mydb")

# Cluster: get a client for a specific server
dbserver_client = Toast.Deployment.client!(deployment, "dbserver-0")

# Raw HTTP for endpoints not covered by client modules
{:ok, response} = Client.get(client, "/_api/engine")
{:ok, body} = Client.unwrap({:ok, response})
```

## Server Control

Tests can stop, kill, pause, and restart individual servers to test resilience.
Targets can be specified by server ID string or by role keyword list.

```elixir
alias Toast.Deployment

# Stop a server gracefully (SIGTERM)
:ok = Deployment.stop_server(deployment, "dbserver-0")

# Kill a server (SIGKILL)
:ok = Deployment.kill_server(deployment, "dbserver-0")

# Pause a server (SIGSTOP) -- process stays alive but frozen
:ok = Deployment.pause_server(deployment, "dbserver-0")

# Resume a paused server (SIGCONT)
:ok = Deployment.resume_server(deployment, "dbserver-0")

# Restart a stopped/killed server
:ok = Deployment.restart_server(deployment, "dbserver-0")

# Start a server (after stop/kill)
:ok = Deployment.start_server(deployment, "dbserver-0")
```

### Targeting by Role

```elixir
# Target all servers of a role
Deployment.stop_server(deployment, role: :dbserver)

# Target a specific server by role and index
Deployment.stop_server(deployment, role: :dbserver, index: 0)

# Target by ArangoDB-assigned internal ID
Deployment.stop_server(deployment, arango_id: "PRMR-abc123")
```

### Querying Deployment State

```elixir
# Overall deployment status: :ready, :degraded, :failed, :stopped
status = Deployment.status(deployment)

# List all servers
servers = Deployment.servers(deployment)

# List servers by role
dbservers = Deployment.servers(deployment, role: :dbserver)

# Get a specific server
{:ok, server} = Deployment.server(deployment, "dbserver-0")
```

After modifying the deployment (stopping/killing servers), tests must restore
all servers before the test finishes. The between-tests health check will
detect degraded deployments and abort the suite.

### Failure Points

Failure points trigger debug-mode behaviors in ArangoDB (only available in builds with USE_FAILURE_TESTS).

```elixir
alias Toast.Deployment.FailurePoint

# Set a failure point on a specific server
:ok = FailurePoint.set(deployment, "dbserver-0", "crash-after-commit")

# Set a failure point on all servers of a role
:ok = FailurePoint.set(deployment, [role: :dbserver], "crash-after-commit")

# Clear a specific failure point
:ok = FailurePoint.clear(deployment, "dbserver-0", "crash-after-commit")

# Clear all failure points on all servers
:ok = FailurePoint.clear_all(deployment)
```

## Crash Testing

For tests that intentionally crash a server, use the expect/verify protocol to
prevent the crash monitor from aborting the suite.

```elixir
test "handles server crash gracefully", %{deployment: d, client: client} do
  alias Toast.Deployment.FailurePoint

  [dbserver | _] = Deployment.servers(d, role: :dbserver)

  # 1. Set up the failure point that will cause the crash
  :ok = FailurePoint.set(d, dbserver.id, "crash-after-commit")

  # 2. Tell Toast to expect a crash (suppresses abort)
  :ok = Deployment.expect_crash(d, dbserver.id)

  # 3. Trigger the crash (e.g., by performing a write)
  Client.Document.insert(client, "test_coll", %{"trigger" => true})

  # 4. Verify the crash happened
  {:ok, crash_info} = Deployment.verify_crash(d, dbserver.id, timeout: 10_000)

  # 5. Clean up: clear failure points and restart
  FailurePoint.clear_all(d)
  :ok = Deployment.start_server(d, dbserver.id)
end
```

`expect_crash/3` accepts a `:timeout` option (default: 30000ms) for how long to
wait for the crash. `verify_crash/3` also accepts `:timeout` (default: 5000ms)
for how long to poll for crash confirmation.

## Analyzing Results

After a test run, results are exported to the `toast-results/` directory (or the
directory specified by `--result-dir`):

- `results.json` -- Structured test results with diagnostics
- `results.xml` -- JUnit XML format for CI integration
- `.diagnostics.etf` -- Serialized diagnostics for offline analysis

### Exit Codes

| Code | Meaning |
|---|---|
| 0 | All tests passed |
| 1 | Test failures |
| 2 | Sanitizer errors detected |
| 3 | Infrastructure failure (deployment failed to start, etc.) |
| 4 | Server crash |

### CI Mode

With `--ci`, Toast packages results into tiers for upload. Tiers are gated on
what issues occurred so successful runs don't pay the cost of uploading large
artifacts:

- **Tier 1** (always published): `results.json`, `results.xml`, `toast.log`,
  agency dumps
- **Tier 2** (any failure — test failures, sanitizer errors, infrastructure
  failures, or crashes): server logs and sanitizer reports bundled into
  `toast-logs.tar.gz`
- **Tier 3** (server crash only): core dump files, compressed with zstd
  (falling back to gzip), and the work directory archived as
  `work-dir.tar.gz`

Pass `--force-all-tiers` (or set `TOAST_FORCE_ALL_TIERS=1`) to bypass the
gating and always package every tier — useful for debugging flaky tests or
when you want full post-mortem artifacts regardless of outcome.

### Analyzing Results Offline

Use `mix toast.analyze` to inspect results from a previous run:

```bash
# List all issues (default)
mix toast.analyze

# List issues from a specific result directory
mix toast.analyze /path/to/toast-results

# Show detailed diagnostics for all issues
mix toast.analyze detail all

# Show detail for a specific issue by index
mix toast.analyze detail 3

# Show detail for a range of issues
mix toast.analyze detail 2-4

# Show only crash or sanitizer issues
mix toast.analyze detail crashes
mix toast.analyze detail sanitizer

# Overview of diagnostics file contents
mix toast.analyze info

# Performance analysis (module/test timing breakdown)
mix toast.analyze perf
```

Detail view options:

```bash
# Include server log excerpts around issues
mix toast.analyze detail all --logs

# Filter log servers and time window
mix toast.analyze detail all --logs --log-servers dbserver-0 --log-window -20000,5000

# Control coredump backtrace output
mix toast.analyze detail all --threads all --backtrace-frames 30
```

## Test Bucketing

For CI parallelization, Toast can split test modules into balanced buckets so
each CI job runs a subset:

```bash
# Split into 4 buckets, run bucket 0
mix toast --build-dir /path/to/build --test-buckets 4/0

# Run bucket 1 of 4
mix toast --build-dir /path/to/build --test-buckets 4/1
```

The index is 0-based. Each bucket gets a roughly equal share of total test
weight while minimizing the number of suites per bucket (since each suite
requires its own server deployment).

### Module Weights

By default every test module has weight 1. For modules that take significantly
longer, declare a higher weight:

```elixir
defmodule Smoke.HeavyTest do
  use Smoke.Suite, weight: 5

  # ...tests...
end
```

The bucketing algorithm uses these weights to balance load across buckets.
Weights don't need to be precise -- the goal is to distinguish fast modules
from slow ones so they aren't all placed in the same bucket.

### Suggesting Weights

After a test run, use `mix toast.analyze weights` to get weight suggestions
based on actual runtimes:

```bash
mix toast.analyze weights
mix toast.analyze weights /path/to/toast-results
```

This compares each module's runtime against the median and suggests a weight
proportional to its relative duration. Only modules where the suggested weight
differs from the current weight are shown.

## Interactive Mode

`ToastTest.Interactive` lets you run individual test modules or complete suites
against a manually-started deployment, useful for debugging. A single `run/2`
function infers the target type automatically -- pass a file, directory, test
module, or suite module.

Start an interactive interactive session with `TOAST_BUILD_DIR=path/to/build-dir iex -S mix`.
Alternatively one can use a [Local Config File](#local-config-file) with a predefined build directory
to avoid having to set the environment variable.

Inside the session:
```elixir
{:ok, deployment} = Toast.Deployment.start_cluster("/path/to/work-dir")

# Run a single test file
ToastTest.Interactive.run("suites/smoke/test_version.exs", deployment: deployment)

# Run a single test module
ToastTest.Interactive.run(Smoke.VersionTest, deployment: deployment)

# Run a complete suite by directory
ToastTest.Interactive.run("suites/smoke/", deployment: deployment)

# Run a complete suite by module
ToastTest.Interactive.run(Smoke.Suite, deployment: deployment)

# Filter by test name substring (works for both single modules and suites)
ToastTest.Interactive.run(Smoke.Suite, deployment: deployment, test: "arango")

# When done
Toast.Deployment.stop(deployment)
```

All files in the suite directory are always compiled, so cross-module
dependencies work regardless of which entry point you use. When a suite module
is passed, `setup_deployment/1` and `teardown_deployment/1` callbacks are called
automatically. Compared to `mix toast`, interactive mode does not run
between-tests health checks, enforce timeouts, collect diagnostics, or produce
result files.

Be aware that when you are in an interactive session, .exs and .ex files in the
suites folder are automatically recompiled when changed, but not .ex files of toast
itself. Execute `recompile` in the interactive shell to recompile these as well.


## Generating Suites

Scaffold a new suite with:

```bash
mix toast.gen.suite my_tests
mix toast.gen.suite cluster_tests --mode cluster
```

This creates `suites/<name>/suite.ex` and `suites/<name>/test_example.exs`.
Run the new suite with:

```bash
mix toast --build-dir /path/to/build my_tests
```

## Configuration Reference

Configuration values are resolved in this order (highest priority first):

1. CLI arguments
2. Environment variables
3. `.toast.local.exs`
4. Defaults

All CLI options are listed in [Toast Options](#toast-options) above. The
following table shows the corresponding environment variables, which are useful
for CI pipelines or shell aliases:

### Environment Variables

| Variable | CLI Equivalent | Description |
|---|---|---|
| `TOAST_BUILD_DIR` | `--build-dir` | Path to ArangoDB build directory |
| `TOAST_BASE_DIR` | `--base-dir` | Base directory for server data/logs |
| `TOAST_RESULT_DIR` | `--result-dir` | Output directory for test results |
| `TOAST_DEPLOYMENT_MODE` | `--cluster` / `--single` | `single_server` or `cluster` |
| `TOAST_SHOW_SERVER_LOGS` | `--show-server-logs` | Print arangod output to stdout |
| `TOAST_GLOBAL_TIMEOUT` | `--global-timeout` | Global timeout in ms |
| `TOAST_TEST_TIMEOUT` | `--test-timeout` | Per-test timeout in ms |
| `TOAST_STARTUP_TIMEOUT` | `--startup-timeout` | Server startup timeout in ms |
| `TOAST_SHUTDOWN_TIMEOUT` | `--shutdown-timeout` | Server shutdown timeout in ms |
| `TOAST_TIMEOUT_FACTOR` | `--timeout-factor` | Multiplier applied to all timeouts |
| `TOAST_KEEP_DATA` | `--keep-data` | Keep work dir on success |
| `TOAST_SANITIZER` | `--sanitizer` | Sanitizer type (`tsan` or `alubsan`) |
| `TOAST_MEMORY_BUDGET` | `--memory-budget` | Memory budget in bytes |
| `TOAST_PROTOCOL` | `--http2` | `http1` or `http2` |
| `TOAST_SSL` | -- | Enable SSL (`true` or `false`) |
| `TOAST_RR` | `--rr` | rr recording: `default`, `all`, or comma-separated roles |
| `TOAST_ATTACH_DEBUGGER` | `--attach-debugger` | Pause for debugger attachment (disables test timeouts) |
| `TOAST_CLUSTER_AGENTS` | `--cluster-agents` | Number of agency nodes |
| `TOAST_CLUSTER_DBSERVERS` | `--cluster-dbservers` | Number of DB servers |
| `TOAST_CLUSTER_COORDINATORS` | `--cluster-coordinators` | Number of coordinators |
| `TOAST_CLUSTER_REPLICATION_FACTOR` | `--replication-factor` | Default replication factor |
| `TOAST_CI` | `--ci` | Enable CI result packaging |
| `TOAST_FORCE_ALL_TIERS` | `--force-all-tiers` | Force packaging all tiers (CI only) |
| `TOAST_API_VERSION` | -- | API version prefix (e.g., `1`) |
| `TOAST_DEBUGGER` | -- | Core dump debugger: `gdb`, `lldb`, `auto`, `none` |
| `TOAST_DUMP_AGENCY` | `--no-agency-dump` | Dump agency state on error (cluster mode) |
| `TOAST_COREDUMP_TIMEOUT` | -- | Timeout for coredump analysis in ms |
| `TOAST_COREDUMP_DIR` | -- | Directory to search for core dumps |

<a id="local-config-file"></a>
### Local Config File

For development convenience, you can create a `.toast.local.exs` file in the
toast project root. It is evaluated as Elixir code and must return a map:

```elixir
%{
  build_dir: "/home/user/dev/arangodb/build-clang",
  base_dir: "/tmp/toast-dev",
  deployment_mode: :single_server
}
```

This file is ignored when `TOAST_CI=true`. It should not be checked into
version control.
