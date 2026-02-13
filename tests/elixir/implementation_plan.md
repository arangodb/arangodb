# Toast — Elixir Implementation Plan

## Context

Toast is an integration/system testing framework for ArangoDB, replacing the legacy JS framework (`js/client/modules/@arangodb/testutils/`). The framework's core job is to **manage and monitor ArangoDB deployments** and **provide detailed analysis when things go wrong** (server crashes, sanitizer reports, assertion failures). ExUnit already handles the test execution and assertion side — Toast adds deployment lifecycle, crash detection, and diagnostic integration on top.

The legacy JS framework (analyzed in `tests/python/plan/`) and the partial Python reimplementation define the **baseline functionality** — what needs to be supported — but neither dictates how the Elixir code should be structured. The Elixir implementation should be idiomatic Elixir/OTP, not a transliteration of Python classes or JS patterns. The existing code is a reference for domain knowledge (arangod CLI args, health check endpoints, agency consensus protocol, etc.), not architectural templates.

Not everything from the JS framework is in scope: network sniffing, memory profiling, and similar advanced monitoring are explicitly out of scope. The focus is on a **stable, well-tested core** that makes it easy to write new integration tests.

### Design Principles

- **Core over features**: Deployment management, crash detection, sanitizer analysis, result reporting. Everything else is later.
- **Framework quality matters**: The framework itself must be thoroughly tested. It's infrastructure that developers depend on daily.
- **Simple test authoring**: Writing a new test should require minimal boilerplate and no framework internals knowledge.
- **Leverage OTP**: Process supervision, crash cascading, and concurrent operations should use OTP primitives directly, not reimplementations.

## Architecture: Umbrella Project

The framework is an Elixir umbrella project. Each JS "test suite" becomes its own umbrella app. The framework itself is the `toast` app that all suite apps depend on.

```
tests/elixir/toast/                  # Umbrella root
├── mix.exs                          # Umbrella config
├── config/
│   ├── config.exs
│   └── runtime.exs                  # TOAST_* env var loading
├── apps/
│   ├── toast/                       # Framework library (OTP app)
│   │   ├── mix.exs
│   │   ├── lib/toast/...
│   │   └── test/...                 # Framework unit tests
│   │
│   ├── shell_api/                   # Test suite: shell API tests
│   │   ├── mix.exs                  # deps: [{:toast, in_umbrella: true}]
│   │   ├── test/
│   │   │   ├── test_helper.exs      # Starts deployment, configures ExUnit
│   │   │   ├── collections_test.exs
│   │   │   ├── documents_test.exs
│   │   │   └── queries_test.exs
│   │   └── lib/                     # Suite-specific helpers (optional)
│   │
│   ├── replication/                 # Test suite: replication tests
│   │   ├── mix.exs
│   │   ├── test/
│   │   │   ├── test_helper.exs      # Starts cluster deployment
│   │   │   └── ...
│   │   └── lib/
│   │
│   └── ...                          # More test suites
│
└── completion/
    └── toast.bash                   # Bash completion script
```

### Why Umbrella?

This solves the group-sequential execution problem without any ExUnit hacking:

1. `mix test` at the umbrella root runs each app's tests **as a separate ExUnit session**, sequentially
2. Each suite's `test_helper.exs` starts its deployment **before** tests run
3. `ExUnit.after_suite/1` stops the deployment **after** all tests in that suite complete
4. The next suite starts fresh — no deployment overlap, no ordering tricks

### Two Responsibilities

1. **Running tests**: `mix test` — standard Mix, works out of the box with the umbrella structure. Each suite app's `test_helper.exs` manages its own deployment lifecycle.
2. **Analyzing results**: `mix toast.analyze` — reads JSON result files, produces reports (slow tests, crash summaries, sanitizer issues, regressions). This is a separate tool that operates on result data, not a test runner concern. Important, but comes after the test execution foundation is solid.

### Deployment Lifecycle per Suite

```elixir
# apps/shell_api/test/test_helper.exs
Application.ensure_all_started(:toast)

{:ok, deployment} = Toast.Deployment.start(:single_server,
  server_args: %{"server.authentication" => "false"}
)

Application.put_env(:toast, :deployment, deployment)

ExUnit.after_suite(fn _results ->
  Toast.Deployment.stop(deployment)
end)

ExUnit.start()
```

### Test Authoring

Tests access the deployment through a single `client` — a connection to the entrypoint (the single server, or a coordinator in cluster mode). Most tests only need this and are deployment-mode agnostic:

```elixir
defmodule ShellApi.CollectionsTest do
  use Toast.TestCase

  test "create and drop collection", %{client: client} do
    assert {:ok, _} = Toast.Client.create_collection(client, "test_coll")
    assert {:ok, _} = Toast.Client.drop_collection(client, "test_coll")
  end
end
```

Cluster-specific tests that need to talk to individual servers access the full deployment:

```elixir
defmodule Replication.ShardDistributionTest do
  use Toast.TestCase

  test "shards are distributed across dbservers", %{deployment: deployment} do
    for {_id, dbserver} <- deployment.dbservers do
      db_client = Toast.Client.new(dbserver.endpoint)
      # query shard distribution on this specific dbserver
    end
  end
end
```

The CaseTemplate is minimal — it reads the deployment from app env and provides both `client` (entrypoint) and `deployment` (full topology):

```elixir
defmodule Toast.TestCase do
  use ExUnit.CaseTemplate

  setup do
    deployment = Application.fetch_env!(:toast, :deployment)
    client = Toast.Client.new(deployment.endpoint)
    %{client: client, deployment: deployment}
  end
end
```

## Framework App Structure (`apps/toast/`)

```
apps/toast/
├── mix.exs
├── lib/
│   └── toast/
│       ├── application.ex          # OTP Application + supervision tree
│       ├── config.ex               # Configuration
│       │
│       ├── process/                # OS process management (core)
│       │   ├── server_process.ex   # GenServer wrapping arangod via Port
│       │   ├── process_sup.ex      # DynamicSupervisor (max_restarts: 0)
│       │   └── signal.ex           # SIGTERM/SIGKILL helpers
│       │
│       ├── deployment/             # Deployment orchestration (core)
│       │   ├── deployment.ex       # Deployment struct + start/stop facade
│       │   ├── controller.ex       # GenServer: lifecycle of one deployment
│       │   ├── factory.ex          # Pure fns: build server configs
│       │   ├── command_builder.ex  # Build arangod CLI args per role
│       │   ├── cluster.ex          # Cluster bootstrap (agents→dbs→coords)
│       │   └── health.ex           # HTTP health checks + agency consensus
│       │
│       ├── testing/                # ExUnit integration (core)
│       │   ├── test_case.ex        # CaseTemplate (client injection)
│       │   └── client.ex           # Thin ArangoDB REST API wrapper
│       │
│       ├── diagnostics/            # Post-mortem analysis (core)
│       │   ├── crash_analyzer.ex   # Parse crash info from arangod logs
│       │   ├── sanitizer.ex        # ASAN/TSAN env vars, log scanning, test matching
│       │   └── server_log.ex       # Important message detection, assertion failures
│       │
│       └── utils/
│           ├── auth.ex             # JWT HS256 (Joken)
│           ├── port_allocator.ex   # GenServer: network port allocation
│           ├── filesystem.ex       # Temp dirs, build detection
│           └── codec.ex            # Encoding abstraction (JSON now, VelocyPack later)
│
└── test/                           # Framework unit tests
    ├── test_helper.exs
    ├── support/
    │   └── fake_server.sh          # Shell script simulating arangod
    ├── process/
    │   └── server_process_test.exs
    ├── deployment/
    │   ├── command_builder_test.exs
    │   └── factory_test.exs
    ├── diagnostics/
    │   ├── crash_analyzer_test.exs
    │   └── sanitizer_test.exs
    └── utils/
        ├── auth_test.exs
        ├── port_allocator_test.exs
        └── codec_test.exs
```

Modules like result export, SUT checkers, CLI, and client tools (arangosh/dump/restore) are not listed above — they will be added in later phases as the structure evolves. The above is the core that needs to work first.

## Supervision Tree

```
Toast.Application
├── Toast.Config (Agent)
├── Toast.PortAllocator (GenServer)
├── Toast.Results.Collector (GenServer)
└── Toast.Deployment.Supervisor (DynamicSupervisor)
    └── per deployment:
        Toast.Deployment.Controller (GenServer)
        └── Toast.Process.ProcessSup (DynamicSupervisor, max_restarts: 0)
            ├── Toast.Process.ServerProcess (GenServer, Port → arangod agent-0)
            ├── Toast.Process.ServerProcess (GenServer, Port → arangod agent-1)
            ├── ...
            └── Toast.Process.ServerProcess (GenServer, Port → arangod coordinator-0)
```

**Design rationale:**
- `ProcessSup` has `max_restarts: 0` — server crashes are NOT auto-restarted, they propagate as notifications
- `Controller` uses `Process.monitor/1` on each `ServerProcess` — crash detection via `{:DOWN, ref, :process, pid, reason}`
- On crash: Controller captures exit info, marks deployment unhealthy, logs crash details
- Tests that check deployment health before running can skip on crash (or the suite's `ExUnit.after_suite` fires cleanup)

## Key Module Designs

### ServerProcess (GenServer wrapping arangod Port)

```elixir
# Opens Port with :exit_status for crash detection
Port.open({:spawn_executable, binary}, [
  :binary, :exit_status, :use_stdio, :stderr_to_stdout,
  args: args, cd: work_dir, env: env_pairs
])

# Crash detection:
handle_info({port, {:exit_status, status}}, state)
  # status > 128 → killed by signal (status - 128)
  # status != 0 → unexpected exit → notify controller

# Graceful stop:
# 1. kill -TERM <pid>  (via System.cmd)
# 2. Wait for {:exit_status, _} with timeout
# 3. If timeout → kill -KILL <pid>
# 4. If still alive → kill -KILL -<pid> (process group)
```

### ClusterBootstrapper

```elixir
def bootstrap(servers_by_role, health_opts, timeout) do
  # 1. Start agents in parallel
  servers_by_role.agents
  |> Task.async_stream(&ServerProcess.start/1, timeout: timeout)
  |> collect_results!()

  # 2. Wait for agency consensus
  #    Poll GET /_api/agency/config on all agents
  #    Verify all agree on leaderId
  :ok = wait_for_consensus(agent_endpoints, timeout)

  # 3. Start dbservers in parallel
  servers_by_role.dbservers
  |> Task.async_stream(&ServerProcess.start/1, timeout: timeout)
  |> collect_results!()

  # 4. Start coordinators in parallel
  servers_by_role.coordinators
  |> Task.async_stream(&ServerProcess.start/1, timeout: timeout)
  |> collect_results!()

  # 5. Verify cluster ready — coordinators respond to /_api/version
  :ok = wait_for_cluster_ready(coordinator_endpoints, timeout)
end
```

### CommandBuilder

Ported from `tests/python/armadillo/armadillo/instances/command_builder.py`:

```elixir
def build_command(server_config, paths, repo_root) do
  [find_binary(server_config.bin_dir),
   "--configuration", config_file(server_config.role),
   "--define", "TOP_DIR=#{repo_root}",
   "--server.endpoint", "tcp://0.0.0.0:#{server_config.port}",
   "--database.directory", paths.data_dir,
   "--javascript.app-path", paths.app_dir,
   "--log.file", paths.log_file]
  ++ role_args(server_config.role, server_config)
  ++ flatten_custom_args(server_config.args)
end

defp config_file(:single),      do: "etc/testing/arangod-single.conf"
defp config_file(:agent),       do: "etc/testing/arangod-agent.conf"
defp config_file(:coordinator), do: "etc/testing/arangod-coordinator.conf"
defp config_file(:dbserver),    do: "etc/testing/arangod-dbserver.conf"
```

### ArangoClient (thin REST wrapper)

```elixir
defmodule Toast.Client do
  defstruct [:base_url, :auth]

  def new(endpoint, opts \\ []) do
    %__MODULE__{base_url: endpoint, auth: opts[:auth]}
  end

  def aql(client, query, bind_vars \\ %{}) do
    post(client, "/_api/cursor", %{query: query, bindVars: bind_vars})
  end

  def create_collection(client, name, opts \\ []) do
    post(client, "/_api/collection", %{name: name, type: opts[:type] || 2})
  end

  def get_version(client) do
    get(client, "/_api/version")
  end

  # ... more endpoints as needed

  defp get(client, path) do
    Req.get!(client.base_url <> path, headers: auth_headers(client))
    |> handle_response()
  end

  defp post(client, path, body) do
    Req.post!(client.base_url <> path, json: body, headers: auth_headers(client))
    |> handle_response()
  end
end
```

## Implementation Phases

### Phase 0: Skeleton + Port Validation

**Goal**: Prove Erlang Ports can reliably manage arangod-like processes.

**Deliverables**:
- Umbrella project scaffold with `apps/toast/`
- `Toast.Process.ServerProcess` — GenServer wrapping Port
- `Toast.Process.Signal` — SIGTERM/SIGKILL via `System.cmd("kill", ...)`
- `Toast.PortAllocator` — GenServer with socket-probe availability check
- `test/support/fake_server.sh` — traps SIGTERM, listens on port, simulates arangod
- Unit tests: start/stop, crash detection, SIGTERM, SIGKILL escalation

**Validates**: The foundation assumption — can we build reliable process supervision on Ports?

### Phase 1: Single Server Deployment

**Goal**: Start a real arangod, health check it, run a query, stop it.

**Deliverables**:
- `Toast.Config` — env var loading, NimbleOptions schema
- `Toast.Deployment` — facade (start/stop)
- `Toast.Deployment.Controller` — GenServer lifecycle
- `Toast.Deployment.Factory` — build server configs
- `Toast.Deployment.CommandBuilder` — arangod CLI args
- `Toast.Deployment.Health` — HTTP health checks via Req (retry + backoff)
- `Toast.Utils.Auth` — JWT HS256 via Joken
- `Toast.Utils.Filesystem` — temp dir management
- `Toast.Utils.BuildDetection` — find arangod binary
- Integration test: start → health check → `RETURN 1` via REST → stop

### Phase 2: Cluster Deployment

**Goal**: Start a full cluster (3 agents + 3 dbservers + 1 coordinator).

**Deliverables**:
- `Toast.Deployment.Cluster` — ordered startup with `Task.async_stream`
- Extended factory for cluster topology (agent/dbserver/coordinator args)
- Agency consensus polling (`/_api/agency/config`)
- Integration test: start cluster → verify healthy → AQL via coordinator → stop

### Phase 3: Test Runner + First Suite

**Goal**: Create the first test suite as an umbrella app, run it end-to-end.

**Deliverables**:
- `Toast.TestCase` CaseTemplate
- `Toast.Client` — thin REST wrapper
- First suite app (e.g., `apps/smoke_test/`) with a few basic tests
- `test_helper.exs` pattern: start deployment → run tests → stop deployment
- `mix test` at umbrella root runs the suite successfully
- Mix task `mix toast.gen.suite <name> --mode single|cluster` to scaffold new suites

### Phase 4: Crash Detection + Abort

**Goal**: Server crash during tests aborts remaining tests gracefully.

**Deliverables**:
- `ServerProcess` crash notification → `Controller` → marks deployment crashed
- `TestCase` checks deployment health in `setup` — skips if crashed
- `Toast.CrashLogParser` — extract signal, backtrace from arangod log
- Test with fake server that crashes mid-test → verify abort + next suite runs fresh

### Phase 5: Diagnostics — Sanitizer + Crash Analysis

**Goal**: Detect and report sanitizer issues, parse crash logs for post-mortem analysis.

**Deliverables**:
- `Toast.Diagnostics.Sanitizer` — ASAN/TSAN/LSAN env var propagation to arangod, log file detection after shutdown, match sanitizer errors to tests by timestamp
- `Toast.Diagnostics.CrashAnalyzer` — parse signal info and backtraces from arangod log files
- `Toast.Diagnostics.ServerLog` — scan for important messages, assertion failures
- Integration into deployment shutdown: collect diagnostics before cleanup

### Phase 6: Result Export

**Goal**: Export test results with deployment diagnostics for CI integration.

**Deliverables**:
- ExUnit formatter that captures results + embeds crash/sanitizer info
- JSON export (via `:json`)
- JUnit XML export

### Future Phases (scoped as needed)

- **SUT Checkers**: Post-test invariant verification (collection/transaction leak detection)
- **Client Tools**: arangosh, arangodump, arangorestore subprocess wrappers
- **CLI**: Standalone escript with shell completion
- **Timeouts**: Global deadline, per-test timeout, output-idle timeout
- **Resilience Testing**: Kill + restart, leader failover scenarios
- **Codec**: VelocyPack encoding via `velocy_pack` library

## Dependencies

| Dependency | Purpose | Phase |
|---|---|---|
| `req` | HTTP client (health checks, ArangoDB API) | 0 |
| `joken` | JWT HS256 token generation | 1 |

JSON is handled by Erlang/OTP 28's built-in `:json` module (no external dependency needed).

VelocyPack encoding support is planned for the future via [`velocy_pack`](https://github.com/ArangoDB-Community/velocy_pack). The client module will use a codec abstraction so the encoding can be swapped.

Target: **Elixir 1.19+, Erlang/OTP 28+**.

## Risk Areas

### Umbrella per-app test isolation
**Assumption**: `mix test` runs each app's tests as a separate ExUnit session.
**Validation**: Phase 0 — create two minimal umbrella apps, verify independent ExUnit sessions.
**Fallback**: If sessions aren't isolated, use `mix cmd --app <name> mix test` which explicitly runs per-app.

### Port-based process management
**Risk**: Erlang Ports might not provide sufficient control for signals / process groups.
**Validation**: Phase 0 validates this completely before any other work.
**Mitigation**: Signal sending via `System.cmd("kill", ...)` is reliable on Linux. For process groups, use `setsid` wrapper if needed.

### No Elixir ArangoDB client library
**Approach**: Build thin wrapper around `Req`. ArangoDB REST API is well-documented. Fewer deps = less maintenance.

### Boilerplate per suite
**Cost**: Each suite needs `mix.exs` (~15 lines) + `test_helper.exs` (~10 lines).
**Mitigation**: `mix toast.gen.suite` generator task. Templates are minimal.

## Verification Plan

Each phase produces something testable:

- **Phase 0**: `cd apps/toast && mix test` — process management unit tests pass
- **Phase 1**: Integration test starts real arangod, runs query, stops cleanly
- **Phase 2**: Integration test starts cluster, verifies all 7 servers healthy
- **Phase 3**: `mix test` at umbrella root runs smoke_test suite end-to-end
- **Phase 4**: Fake server crash → tests abort → next suite runs fresh
- **Phase 5**: Sanitizer logs detected and matched to test; crash log parsed into structured report
- **Phase 6**: JSON + JUnit XML output produced with embedded diagnostics
