# 🥑 TOAST 🍞

**TO**olkit for **A**rango **S**ystem **T**oasting, ah, Testing is an Elixir-based
integration testing framework for ArangoDB. It manages ArangoDB server deployments
(single-server and cluster), runs tests against them, collects diagnostics (crash
logs, sanitizer errors, core dumps), and produces CI-friendly reports (JSON, JUnit XML).

## Quick Start

```bash
# Run all suites against a local build
mix toast --build-dir /path/to/build

# Run a specific suite
mix toast --build-dir /path/to/build smoke

# Run in cluster mode
mix toast --build-dir /path/to/build --cluster

# Run with verbose output
mix toast --build-dir /path/to/build --trace
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
    assert {:ok, body} = Client.Admin.version(client)
    assert body["server"] == "arango"
  end
end
```

Note: the module names of all files belonging to a suite must be namespaced with the suite name.

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

Use ExUnit filter options to include/exclude by tag:

```bash
mix toast --only cluster_only
mix toast --exclude slow
mix toast --include edge_case
```

### Toast Options

| Option | Description |
|---|---|
| `--build-dir PATH` / `-b` | Path to ArangoDB build directory |
| `--work-dir PATH` | Temporary directory for server data and logs |
| `--result-dir PATH` | Output directory for test results (default: `toast-results`) |
| `--cluster` | Use cluster deployment |
| `--single` | Use single-server deployment (default) |
| `--show-server-logs` | Print arangod output to stdout |
| `--global-timeout MS` | Global timeout in milliseconds (default: 3600000) |
| `--test-timeout MS` | Per-test timeout in milliseconds (default: 300000) |
| `--startup-timeout MS` | Server startup timeout in milliseconds (default: 60000) |
| `--shutdown-timeout MS` | Server shutdown timeout in milliseconds (default: 60000) |
| `--timeout-factor N` | Timeout multiplier (default: 1, auto-set to 3 for sanitizer builds) |
| `--keep-work-dir` | Keep server data and logs even on success |
| `--sanitizer TYPE` | Sanitizer type: `tsan` or `alubsan` (auto-detected from build dir) |
| `--cluster-agents N` | Number of agency nodes (default: 3) |
| `--cluster-dbservers N` | Number of DB servers (default: 3) |
| `--cluster-coordinators N` | Number of coordinators (default: 1) |
| `--replication-factor N` | Default replication factor (default: 2) |
| `--ci` | Enable CI mode (packages results for upload) |
| `--no-agency-dump` | Skip agency state dump on error |

### ExUnit Options

| Option | Description |
|---|---|
| `--include` / `-i` | Include tests matching the filter |
| `--exclude` / `-e` | Exclude tests matching the filter |
| `--only` | Run only tests matching the filter |
| `--trace` / `-t` | Enable verbose per-test output |
| `--timeout MS` | Per-test timeout (ExUnit level) |
| `--max-failures N` | Stop after N failures |
| `--color` / `--no-color` | Enable/disable ANSI coloring |
| `--no-compile` | Skip project compilation |
| `--no-start` | Skip application startup |

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
| `mode` | `:auto` | Deployment mode: `:single_server`, `:cluster`, or `:auto` (follows CLI) |
| `timeout` | `3_600_000` | Suite-level timeout in milliseconds |
| `server_args` | `[]` | Extra arangod CLI arguments for single-server mode |
| `coordinator_args` | `[]` | Extra arguments for coordinators (cluster mode) |
| `dbserver_args` | `[]` | Extra arguments for DB servers (cluster mode) |
| `agent_args` | `[]` | Extra arguments for agents (cluster mode) |
| `between_tests` | `:default` | Health check behavior between tests (`:default` or `false`) |

Example with explicit cluster configuration:

```elixir
defmodule Resilience.Suite do
  use ToastTest.Suite,
    mode: :cluster,
    cluster_dbservers: 3,
    cluster_coordinators: 2
end
```

### Suite Callbacks

Suites can implement optional callbacks for lifecycle hooks:

```elixir
defmodule MyApp.Suite do
  use ToastTest.Suite

  @impl ToastTest.Suite
  def setup_deployment(deployment) do
    # Called once after the deployment starts, before any tests run.
    # Use this to create databases, collections, or seed data.
    client = Toast.Client.new(deployment.endpoint)
    {:ok, _} = Toast.Client.Collection.create(client, "shared_data")
    {:ok, %{shared_collection: "shared_data"}}
  end

  @impl ToastTest.Suite
  def teardown_deployment(deployment) do
    # Called once after all tests complete, before the deployment stops.
    :ok
  end

  @impl ToastTest.Suite
  def between_tests(deployment, prev_test) do
    # Called between each test. Return :ok or {:error, reason}.
    # The default behavior checks deployment health.
    Toast.Deployment.check_health(deployment, prev_test)
  end
end
```

The map returned from `setup_deployment/1` is merged into the test context,
making its keys available to all tests in the suite.

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
    assert {:ok, _} = Client.Collection.create(client, name)
    assert {:ok, collections} = Client.Collection.list(client, exclude_system: true)
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

The `use Smoke.Suite` line automatically aliases `Toast.Client` as `Client`, so
you can write `Client.Admin.version(client)` instead of the fully qualified form.

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
   cleaned up (unless `--keep-work-dir` is set)

## Client API

`Toast.Client` is a thin REST client for ArangoDB. Tests receive a
pre-configured client in their context.

### Creating Clients

```elixir
# Client from test context (most common)
test "example", %{client: client} do
  # ...
end

# Client for a specific database
db_client = Client.with_database(client, "mydb")

# Client for a specific server in a cluster
{:ok, dbserver_client} = Toast.Deployment.client(deployment, role: :dbserver, index: 0)
{:ok, server_client} = Toast.Deployment.client(deployment, "dbserver-0")
```

### Collections

```elixir
# Create a document collection
{:ok, _} = Client.Collection.create(client, "users")

# Create an edge collection
{:ok, _} = Client.Collection.create(client, "edges", edge: true)

# List collections (excluding system collections)
{:ok, collections} = Client.Collection.list(client, exclude_system: true)

# Drop a collection (no error if it doesn't exist)
:ok = Client.Collection.drop(client, "users")
```

### Documents

```elixir
# Insert a document
{:ok, meta} = Client.Document.insert(client, "users", %{"name" => "Alice"})
key = meta["_key"]

# Read a document
{:ok, doc} = Client.Document.get(client, "users", key)

# Remove a document
:ok = Client.Document.remove(client, "users", key)
```

### AQL Queries

```elixir
# Simple query
{:ok, [1]} = Client.AQL.execute(client, "RETURN 1")

# With bind variables
{:ok, results} = Client.AQL.execute(client, "FOR u IN users FILTER u.age > @min RETURN u", %{"min" => 18})

# Bang variant (raises on error)
results = Client.AQL.execute!(client, "FOR i IN 1..10 RETURN i")

# Handles cursor pagination automatically
{:ok, all_rows} = Client.AQL.execute(client, "FOR doc IN large_collection RETURN doc")
```

### Indexes

```elixir
# Create an index
{:ok, _} = Client.Index.create(client, "users", %{
  "type" => "persistent",
  "fields" => ["name"]
})

# List indexes on a collection
{:ok, indexes} = Client.Index.list(client, "users")

# Drop an index by handle
:ok = Client.Index.drop(client, "users/12345")
```

### Admin

```elixir
# Server version
{:ok, %{"server" => "arango", "version" => version}} = Client.Admin.version(client)

# Server status
{:ok, status} = Client.Admin.status(client)
```

### Raw HTTP

For endpoints not covered by the client modules, use the base client directly:

```elixir
{:ok, response} = Client.get(client, "/_api/engine")
{:ok, body} = Client.unwrap({:ok, response})

{:ok, response} = Client.post(client, "/_api/explain", %{"query" => "RETURN 1"})
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

# Target by cluster-internal ID
Deployment.stop_server(deployment, cluster_id: "PRMR-abc123")
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

# Health check
:ok = Deployment.check_health(deployment)
```

After modifying the deployment (stopping/killing servers), tests must restore
all servers before the test finishes. The between-tests health check will
detect degraded deployments and abort the suite.

### Failure Points

Failure points trigger debug-mode behaviors in ArangoDB (debug builds only).

```elixir
# Set a failure point on a specific server
:ok = Deployment.set_failure_point(deployment, "dbserver-0", "crash-after-commit")

# Set a failure point on all servers of a role
:ok = Deployment.set_failure_point(deployment, [role: :dbserver], "crash-after-commit")

# Clear a specific failure point
:ok = Deployment.clear_failure_point(deployment, "dbserver-0", "crash-after-commit")

# Clear all failure points on all servers
:ok = Deployment.clear_all_failure_points(deployment)
```

## Crash Testing

For tests that intentionally crash a server, use the expect/verify protocol to
prevent the crash monitor from aborting the suite.

```elixir
test "handles server crash gracefully", %{deployment: d, client: client} do
  [dbserver | _] = Deployment.servers(d, role: :dbserver)

  # 1. Set up the failure point that will cause the crash
  :ok = Deployment.set_failure_point(d, dbserver.id, "crash-after-commit")

  # 2. Tell Toast to expect a crash (suppresses abort)
  :ok = Deployment.expect_crash(d, dbserver.id)

  # 3. Trigger the crash (e.g., by performing a write)
  Client.Document.insert(client, "test_coll", %{"trigger" => true})

  # 4. Verify the crash happened
  {:ok, crash_info} = Deployment.verify_crash(d, dbserver.id, timeout: 10_000)

  # 5. Clean up: clear failure points and restart
  Deployment.clear_all_failure_points(d)
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

### mix toast.analyze

Post-run analysis of exported results:

```bash
# Summary overview (default)
mix toast.analyze toast-results/results.json

# Detailed failure info with stack traces
mix toast.analyze toast-results/results.json --failures

# Crash diagnostics, sanitizer errors, coredump traces
mix toast.analyze toast-results/results.json --crashes

# Slowest tests
mix toast.analyze toast-results/results.json --slow 20
```

### Exit Codes

| Code | Meaning |
|---|---|
| 0 | All tests passed |
| 1 | Test failures |
| 2 | Sanitizer errors detected |
| 3 | Infrastructure failure (deployment failed to start, etc.) |
| 4 | Server crash |

### CI Mode

With `--ci`, Toast packages results into tiers for upload:

- **Tier 1** (always published): `results.json`, `results.xml`, `toast.log`
- **Tier 2** (compressed archive): server logs, sanitizer reports, crash
  reports, agency dumps -- bundled into `toast-logs.tar.gz`
- **Tier 3** (individually compressed): core dump files, compressed with zstd
  (falling back to gzip)

## Interactive Mode

`ToastTest.Interactive` lets you run individual test modules against a
manually-started deployment, useful for debugging.

```elixir
# In an IEx session:
{:ok, deployment} = Toast.Deployment.start(:single_server, build_dir: "/path/to/build")

# Run a test file
ToastTest.Interactive.run("suites/smoke/test_version.exs", deployment: deployment)

# Run a specific test by name
ToastTest.Interactive.run(Smoke.VersionTest,
  deployment: deployment,
  test: "returns arango server info"
)

# When done
Toast.Deployment.stop(deployment)
```

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
| `TOAST_WORK_DIR` | `--work-dir` | Temp directory for server data/logs |
| `TOAST_RESULT_DIR` | `--result-dir` | Output directory for test results |
| `TOAST_DEPLOYMENT_MODE` | `--cluster` / `--single` | `single_server` or `cluster` |
| `TOAST_SHOW_SERVER_LOGS` | `--show-server-logs` | Print arangod output to stdout |
| `TOAST_GLOBAL_TIMEOUT` | `--global-timeout` | Global timeout in ms |
| `TOAST_TEST_TIMEOUT` | `--test-timeout` | Per-test timeout in ms |
| `TOAST_STARTUP_TIMEOUT` | `--startup-timeout` | Server startup timeout in ms |
| `TOAST_SHUTDOWN_TIMEOUT` | `--shutdown-timeout` | Server shutdown timeout in ms |
| `TOAST_TIMEOUT_FACTOR` | `--timeout-factor` | Multiplier applied to all timeouts |
| `TOAST_KEEP_WORK_DIR` | `--keep-work-dir` | Keep work dir on success |
| `TOAST_SANITIZER` | `--sanitizer` | Sanitizer type (`tsan` or `alubsan`) |
| `TOAST_CLUSTER_AGENTS` | `--cluster-agents` | Number of agency nodes |
| `TOAST_CLUSTER_DBSERVERS` | `--cluster-dbservers` | Number of DB servers |
| `TOAST_CLUSTER_COORDINATORS` | `--cluster-coordinators` | Number of coordinators |
| `TOAST_CLUSTER_REPLICATION_FACTOR` | `--replication-factor` | Default replication factor |
| `TOAST_CI` | `--ci` | Enable CI result packaging |
| `TOAST_API_VERSION` | -- | API version prefix (e.g., `1`) |
| `TOAST_DEBUGGER` | -- | Core dump debugger: `gdb`, `lldb`, `auto`, `none` |
| `TOAST_DUMP_AGENCY` | `--no-agency-dump` | Dump agency state on error (cluster mode) |
| `TOAST_COREDUMP_TIMEOUT` | -- | Timeout for coredump analysis in ms |

### Local Config File

For development convenience, you can create a `.toast.local.exs` file in the
toast project root. It is evaluated as Elixir code and must return a map:

```elixir
%{
  build_dir: "/home/user/dev/arangodb/build-clang",
  work_dir: "/tmp/toast-dev",
  deployment_mode: :single_server
}
```

This file is ignored when `TOAST_CI=true`. It should not be checked into
version control.