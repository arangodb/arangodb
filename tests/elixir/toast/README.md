# Toast

**T**est **O**rchestration for **A**rangoDB **S**ystem **T**esting — an Elixir framework that manages ArangoDB server deployments for integration tests.

Toast handles the full lifecycle: starting arangod processes, waiting for health, running ExUnit tests against a live server, collecting diagnostics (crash logs, sanitizer output), and exporting structured results.

## Project Structure

This is an Elixir umbrella project with two apps:

```
apps/
  toast/          # Core framework (library)
  smoke_test/     # Example test suite using Toast
```

### Core Modules (`apps/toast/`)

**Deployment layer** — starts and manages arangod processes:

| Module | Purpose |
|--------|---------|
| `Toast.Deployment` | High-level API to start/stop deployments (single server or cluster) |
| `Toast.Deployment.SingleServerController` | GenServer managing a single arangod process lifecycle |
| `Toast.Deployment.ClusterController` | GenServer managing a full cluster (agents, dbservers, coordinators) |
| `Toast.Deployment.ServerInstance` | Struct holding all runtime state of a server instance (id, role, port, endpoint, OS PID, log path, erlang PIDs) |
| `Toast.Deployment.Factory` | Builds launch specs (executable path, args, env, directories) |
| `Toast.Deployment.CommandBuilder` | Constructs arangod CLI arguments per role |
| `Toast.Deployment.Health` | HTTP health checks and agency readiness polling |

**Process layer** — OS process management and monitoring:

| Module | Purpose |
|--------|---------|
| `Toast.Process.ServerProcess` | GenServer wrapping erlexec for OS process management |
| `Toast.Process.HealthMonitor` | Continuous HTTP health monitoring per server, notifies controller on failure |
| `Toast.Process.Supervisor` | DynamicSupervisor for server processes and health monitors |

**Test execution** — running tests and collecting results:

| Module | Purpose |
|--------|---------|
| `Toast.TestCase` | ExUnit.CaseTemplate — provides `deployment`, `endpoint`, `client` to tests; health-checks between test cases |
| `Toast.Runner` | Custom test runner with suite abort support (replaces ExUnit.Runner) |
| `Toast.Client` | Thin REST client for ArangoDB (version, AQL, collections, documents) |
| `Toast.ResultFormatter` | ExUnit formatter collecting test events |
| `Toast.CLIFormatter` | Custom CLI formatter (replaces ExUnit.CLIFormatter) |

**Diagnostics** — crash detection and log analysis:

| Module | Purpose |
|--------|---------|
| `Toast.Diagnostics.CrashLogParser` | Parses arangod crash logs (signals, stack traces, crash output) |
| `Toast.Diagnostics.ServerLog` | Scans server log for assertion failures and FATAL/WARNING messages |
| `Toast.Diagnostics.Sanitizer` | ASAN/LSAN/UBSAN/TSAN env setup and log collection |
| `Toast.Diagnostics.Summary` | Formats crash diagnostics into human-readable CLI output |

**Result export** — structured output for CI:

| Module | Purpose |
|--------|---------|
| `Toast.ResultExporter` | Orchestrates writing results.json, results.xml, toast.log |
| `Toast.ResultExporter.JSON` | Builds JSON result structure with test suites, diagnostics, server health |
| `Toast.ResultExporter.JUnitXML` | JUnit XML format for CI integration |

**Infrastructure:**

| Module | Purpose |
|--------|---------|
| `Toast.Config` | Configuration from `TOAST_*` environment variables |
| `Toast.PortAllocator` | Dynamic TCP port allocation |
| `Toast.LogFormatter` | Custom log formatter with automatic module name inclusion |
| `Mix.Tasks.Toast` | `mix toast` — runs test suites with Toast runner |
| `Mix.Tasks.Toast.Gen.Suite` | `mix toast.gen.suite` — scaffolds a new test suite app |

### Key Data Structures

**ServerInstance** — all runtime state for one arangod process:
```elixir
%ServerInstance{
  id: "dbserver-1",         # internal identifier
  role: :dbserver,          # :single | :agent | :dbserver | :coordinator
  port: 8530,              # TCP port
  endpoint: "http://...",   # full URL
  pid: 54321,              # OS PID (from erlexec)
  log_file: "/tmp/.../log", # server log path
  server_dir: "/tmp/...",   # data directory
  server_pid: #PID<...>,   # Erlang PID of ServerProcess
  health_monitor: #PID<...> # Erlang PID of HealthMonitor
}
```

**Diagnostics** — collected per server during shutdown:
```elixir
%{
  sanitizer_errors: [%{content: "...", sanitizer_type: :alubsan, ...}],
  server_log: %{assertion_failures: [...], warnings: [...]},
  crash_report: %{signal_name: "SIGSEGV", crash_output: [...], ...},
  server_error: {:server_crashed, %{exit_status: 139, signal: 11}} | nil,
  server: %ServerInstance{...}
}
```

For cluster deployments, diagnostics is a map of `%{server_id => diagnostics}`.

## Prerequisites

- Elixir 1.19+ / OTP 28+
- A built arangod (the `arangod` target from the ArangoDB build)
- erlexec (fetched automatically by Mix)

## Quick Start

```bash
cd tests/elixir/toast

# Install dependencies
mix deps.get

# Run the smoke tests against a local build
mix toast --build-dir /path/to/build-dir

# Or via environment variable
TOAST_BUILD_DIR=/path/to/build-dir mix test
```

The build dir should point to the CMake build directory containing the `arangod` binary (or a `bin/` subdirectory with it).

## Configuration

All configuration is via environment variables or `mix toast` CLI flags:

| Variable | CLI Flag | Default | Description |
|----------|----------|---------|-------------|
| `TOAST_BUILD_DIR` | `--build-dir` | — | Path to ArangoDB build directory (required) |
| `TOAST_WORK_DIR` | `--work-dir` | `/tmp/toast/run_<N>` | Temporary directory for server data |
| `TOAST_RESULT_DIR` | `--result-dir` | `toast-results` | Output directory for results and logs |
| `TOAST_DEPLOYMENT_MODE` | `--cluster` / `--single` | `single_server` | `single_server` or `cluster` |
| `TOAST_SANITIZER` | `--sanitizer` | auto-detected | `alubsan` or `tsan` — forces sanitizer env vars |
| `TOAST_GLOBAL_TIMEOUT` | `--global-timeout` | `3600000` | Global timeout (ms) — entire lifecycle |
| `TOAST_TEST_TIMEOUT` | `--test-timeout` | `300000` | Per-test timeout (ms) — overridable with `@tag timeout:` |
| `TOAST_STARTUP_TIMEOUT` | `--startup-timeout` | `60000` | Server startup timeout (ms) |
| `TOAST_SHUTDOWN_TIMEOUT` | `--shutdown-timeout` | `60000` | Server shutdown timeout (ms) |
| `TOAST_TIMEOUT_FACTOR` | `--timeout-factor` | `1` (auto `3` for sanitizer) | Multiplier applied to all timeouts including `@tag timeout:` |
| `TOAST_KEEP_WORK_DIR` | `--keep-work-dir` | `false` | Keep server data/logs even on success |
| `TOAST_SHOW_SERVER_LOGS` | `--show-server-logs` | `false` | If `true`, arangod logs go to stdout |
| `TOAST_CLUSTER_AGENTS` | `--cluster-agents` | `3` | Number of agency nodes |
| `TOAST_CLUSTER_DBSERVERS` | `--cluster-dbservers` | `3` | Number of DB servers |
| `TOAST_CLUSTER_COORDINATORS` | `--cluster-coordinators` | `1` | Number of coordinators |
| `TOAST_CLUSTER_REPLICATION_FACTOR` | `--replication-factor` | `2` | Default replication factor |

Configuration can also be passed programmatically as keyword options to `Toast.TestCase.setup_suite/2`.

## Writing Tests

### test_helper.exs

```elixir
ExUnit.start()

case Toast.TestCase.setup_suite() do
  {:ok, _} -> :ok
  {:error, _} -> ExUnit.configure(exclude: [:toast_suite])
end
```

Use `setup_suite!/2` instead to raise on failure (aborting the suite).

### Test modules

```elixir
defmodule MyApp.SomeTest do
  use Toast.TestCase

  test "server is reachable", %{client: client} do
    assert {:ok, body} = Client.version(client)
    assert body["server"] == "arango"
  end

  test "AQL works", %{client: client} do
    assert [3] = Client.aql!(client, "RETURN 1 + 2")
  end

  test "collection CRUD", %{client: client} do
    assert {:ok, _} = Client.create_collection(client, "test_col")

    on_exit(fn -> Client.drop_collection(client, "test_col") end)

    assert {:ok, _} = Client.insert_document(client, "test_col", %{"value" => 42})
  end
end
```

The `use Toast.TestCase` macro provides:
- `%{client: client}` — a `Toast.Client` struct for REST API calls
- `%{endpoint: endpoint}` — the base URL (e.g., `http://127.0.0.1:8529`)
- `%{deployment: deployment}` — the deployment struct

## Test Lifecycle

1. **Application boot** — `Toast.Application` starts the supervision tree (PortAllocator, Process.Supervisor, Deployment.Supervisor) and sets up the file logger
2. **`setup_suite`** — starts an arangod deployment (single or cluster), waits for health, registers an `after_suite` callback
3. **Test execution** — ExUnit runs test modules via `Toast.Runner`; deployment health is checked between test cases; server crashes abort the suite immediately
4. **`after_suite`** — shuts down the deployment, collects diagnostics, prints crash summary to CLI, exports results

## Deployment Modes

### Single Server

Starts one arangod in single-server mode. Fastest for most integration tests.

### Cluster

Starts a full cluster: agency nodes, DB servers, and coordinators. Deploy sequence:
1. Start all OS processes (via erlexec)
2. Launch agents, wait for agency consensus
3. Launch DB servers with health checks
4. Launch coordinators with health checks
5. Start per-server health monitors

The test endpoint points to the first coordinator.

## Crash Handling

If an arangod process crashes during testing:
- The `HealthMonitor` or erlexec port detects the failure
- The controller is notified and sets status to `:failed`
- The crash monitor calls `Toast.Runner.abort!/1` to stop the suite
- All linked test processes are killed
- The CLI displays a "CRASHED SERVERS" section with signal info, crash output, fatal log lines, and the server log path

## Sanitizer Support

Toast auto-detects sanitizer configuration:
1. From the build directory path (`asan` → ASAN/LSAN/UBSAN, `tsan` → TSAN)
2. From existing `ASAN_OPTIONS` / `TSAN_OPTIONS` env vars
3. From explicit `TOAST_SANITIZER=alubsan|tsan`

Sanitizer log files are written per-server and collected during shutdown. Errors appear in both the CLI output and the exported results.

## Output

Results are written to `TOAST_RESULT_DIR` (default: `toast-results/`):

| File | Format | Content |
|------|--------|---------|
| `results.json` | JSON | Full test results, server health, diagnostics |
| `results.xml` | JUnit XML | For CI integration |
| `toast.log` | Text | All log messages (debug level) |

## Adding a New Test Suite

```bash
mix toast.gen.suite my_suite
mix toast.gen.suite cluster_tests --mode cluster
```

Or manually:
1. Create a new app: `cd apps && mix new my_suite --sup`
2. Add `toast` as a dependency in the new app's `mix.exs`
3. Create `test/test_helper.exs` with `Toast.TestCase.setup_suite()`
4. Write test modules with `use Toast.TestCase`
