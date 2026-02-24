# Toast: Comprehensive Specification

## Vision

Toast evolves from a test-only Elixir framework into a **deployment infrastructure library** for ArangoDB, with the test framework as one consumer. It manages the full lifecycle of ArangoDB server deployments (single server and cluster), providing both programmatic and interactive interfaces. The existing process management, health monitoring, and diagnostics layers are solid foundations to build on.

Toast replaces the Armadillo (Python) attempt. The legacy JavaScript testing framework serves as a blueprint for the *kinds of tests* needed, but not for how the framework itself should be structured.

## Current State

Toast is an Elixir umbrella project (`apps/toast/` + `apps/smoke_test/`) with ~30 modules organized in layers:

- **Deployment layer**: SingleServerController, ClusterController, Factory, CommandBuilder, Health, ServerInstance — GenServer-based state machines managing arangod lifecycle
- **Process layer**: ServerProcess (wraps erlexec), HealthMonitor (periodic HTTP checks), DynamicSupervisor
- **Test execution**: TestCase (ExUnit.CaseTemplate), custom Runner (with abort support), Client (thin REST), ResultFormatter, CLIFormatter
- **Diagnostics**: CrashLogParser, ServerLog, Sanitizer (ASAN/LSAN/UBSAN/TSAN), SanitizerMatcher, CrashMatcher, Summary
- **Result export**: JSON + JUnit XML + log file
- **Infrastructure**: Config (env vars), PortAllocator, LogFormatter, Mix task

What works well: GenServer state machines, dual crash detection (erlexec + health monitor), diagnostic correlation, timeout hierarchy, configuration simplicity.

## Planned Changes

### 1. Project Restructure

**From:** Umbrella app (`apps/toast/`, `apps/smoke_test/`)
**To:** Single Mix project with clear directory separation:

```
tests/elixir/toast/
  lib/          # Toast infrastructure library
  test/         # Unit tests for the infrastructure
  <suites>/     # Integration/system tests (folder name TBD)
```

The umbrella was originally needed to overcome ExUnit module ordering limitations. The custom mix task and runner make it unnecessary. The single-project structure provides:
- Clear separation between framework code and test suites
- Freedom to implement custom module discovery and loading (already have custom toast task)
- Simpler dependency management

**Open question:** Internal structure of the suites folder.

### 2. Infrastructure as Library

Toast's deployment management should be a standalone library usable outside ExUnit:
- **IEx REPL**: Start deployments interactively, run ad-hoc queries, inspect state, debug
- **Scripted automation**: Elixir scripts using deployment infrastructure outside test context
- **Test framework**: ExUnit integration as one consumer of the infrastructure

This means the deployment/process/health layers must not depend on ExUnit or test-specific concerns.

### 3. Suite System

The JS framework organizes tests into high-level suites, each managing its own deployment. Key characteristics:
- Each suite gets its own deployment (single server or cluster)
- Suites are explicitly specified to run (not auto-discovered)
- Most suites use a standard deployment mode, but specialized suites (resilience, replication) have custom deployment logic
- All tests within a suite share the same deployment

**Open design question:** Whether the JS-style suite organization is the right model, or whether a different approach (folder-based, tag-based, manifest-based, or hybrid) would be better. The design should be evaluated on its own merits, not replicated from the JS framework just because it exists.

Considerations:
- Suites need to declare deployment requirements (mode, custom config, custom deployment logic)
- Some suites need direct deployment control (start/stop/restart servers during tests)
- Suite-level setup/teardown for deployment lifecycle
- Clear ownership: which tests belong to which suite
- How to run a single suite, multiple suites, or filtered subsets

### 4. Resilience Testing

Tests that deliberately manipulate deployments:
- Stop/restart individual servers in a cluster
- Pause servers with SIGSTOP / resume with SIGCONT
- Cause deliberate crashes (SIGKILL, SIGABRT)
- Test failover, recovery, leader election

**Open design question:** How to handle health monitoring during deliberate actions. Options:
1. Test explicitly tells framework "I'm about to crash this server" — monitoring suppressed automatically
2. Test manually toggles monitoring for specific servers
3. Framework detects deliberate actions (called via API) vs unexpected crashes

The JS framework's approach to this is described as "very obscure" — a cleaner design is desired.

### 5. Coredump Analysis

GDB-based stack trace extraction (same as JS framework):
- Automatically extract stack traces from coredumps via non-interactive GDB
- Include stack traces in test results for analysis without downloading dumps
- Package coredumps as CI artifacts for cases requiring deeper analysis
- Stack frame filtering (prune internal frames)

### 6. Result Packaging for CI

Integration with CircleCI (ArangoDB's CI system):
- Test result packages containing: results JSON, JUnit XML, logs, coredumps, sanitizer reports
- Structured output directory for CI artifact upload
- Exit codes distinguishing test failures from infrastructure failures

### 7. Analysis Tool

CLI tool for post-run analysis of result JSON files:
- Summary view (pass/fail counts, durations, failures)
- Detailed failure analysis with stack traces
- Potentially: diff between runs, regression detection (future)

### 8. Extensible REST Client

Minimal ArangoDB REST client designed for extensibility:
- Core: CRUD operations, AQL queries, collection management, index management
- Admin endpoints: version, health, server status
- Designed so test suites can extend with domain-specific calls
- Not a full-featured client library — scoped to testing needs

### 9. More Test Suites

Beyond the current smoke tests, need integration tests covering:
- Shell server tests (collections, documents, indexes, AQL)
- Resilience tests (server failures, failover, recovery)
- Replication tests
- And many more categories from the JS framework

The specific test suites to write are a separate planning exercise — this plan focuses on the framework infrastructure needed to support them.

## Constraints

- Elixir 1.19+, OTP 28+
- No special deployment constraints — standard Elixir project
- Linux-only (no Windows support needed)
- Incremental delivery over months, no hard deadline
- Must work with CircleCI

## Key Open Design Questions

1. **Suite organization**: Folder-based vs tag-based vs manifest-based vs hybrid? How to structure the suites directory?
2. **Suite-deployment relationship**: Is one-suite-one-deployment the right model? Should suites declare deployment requirements or manage their own?
3. **Resilience monitoring**: How should health monitoring behave during deliberate server manipulation?
4. **Suite folder naming**: What should the integration test folder be called? (`suite/`, `integration/`, `system/`, other?)
