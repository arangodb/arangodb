Good, I have a clear picture of the format. Now I can write the section content.

# Section 08: Diagnostics

## Overview

This section implements two new diagnostic modules and integrates them into the deployment shutdown lifecycle:

1. **Coredump Analysis** (`Toast.Diagnostics.Coredump`) -- discovers core dump files after a server crash, runs a debugger (GDB or LLDB) to extract stack traces, and produces structured crash reports.
2. **Agency Dump** (`Toast.Diagnostics.AgencyDump`) -- captures agency state from a live agent before cluster shutdown, providing cluster topology and plan information essential for diagnosing cluster test failures.

Both modules integrate into `stop_and_collect/1` via a multi-step protocol: agency dump (pre-shutdown, from living agents) then shutdown then log/sanitizer collection then coredump analysis (post-shutdown, with its own timeout).

**Depends on**: section-02-library-extraction (library/test-framework boundary, Deployment struct, controller architecture, Config module with `.toast.local.exs` support)

**Blocks**: section-09-ci-packaging (tiered result packaging consumes coredump reports and agency dumps)

---

## Prerequisites

After section-02, the project has:

- `lib/toast/diagnostics/` containing existing modules: `CrashLogParser`, `Sanitizer`, `SanitizerMatcher`, `CrashMatcher`, `Matcher`, `ServerLog`, `Summary`
- `lib/toast/deployment.ex` with `stop_and_collect/1` that calls `controller.shutdown(pid, timeout)` then reads diagnostics from controller state
- `Toast.Config` with `.toast.local.exs` support and keyword opts > env vars > local config > defaults precedence
- Controllers (`SingleServerController`, `ClusterController`) as GenServers that collect diagnostics during shutdown via `collect_diagnostics/1`
- `Toast.Deployment.Supervisor` and `Toast.Process.Supervisor` for process management

The existing `collect_diagnostics/1` in `SingleServerController` gathers sanitizer errors, server log scans, and crash log parser output. This section extends that with coredump analysis (post-shutdown) and agency dump (pre-shutdown for clusters).

---

## Tests

Write these tests BEFORE implementing. All tests use ExUnit, live in `test/toast/diagnostics/`, and mock external dependencies (filesystem operations, process execution). No running ArangoDB or debugger required.

### 6.1 Coredump Analysis Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/diagnostics/coredump_test.exs`

These tests verify core file discovery, debugger command construction, stack trace parsing, and graceful degradation when debuggers are unavailable.

```elixir
# --- Discovery ---
# Test: Coredump.discover/1 finds core files in server work directory
# Test: Coredump.discover/1 finds core files in /tmp filtered by PID
# Test: Coredump.discover/1 handles pipe-based core_pattern (systemd-coredump/coredumpctl)
#
# --- Debugger command construction ---
# Test: GDB debugger constructs correct batch command
#       (gdb -batch -ex "thread apply all bt full" -ex "quit" <binary> <core>)
# Test: LLDB debugger constructs correct command with -c and -o flags
#       (lldb -c <core> -o "thread backtrace all" -o "quit" -- <binary>)
#
# --- Stack trace parsing ---
# Test: stack trace extraction parses frames into common struct
# Test: frame filtering removes glibc/libstdc++ internal frames
#
# --- Auto-detection and fallbacks ---
# Test: debugger auto-detection prefers LLDB, falls back to GDB
# Test: missing debugger -> skip with warning, still collect core files
# Test: debugger timeout does not hang collection
# Test: binary mismatch handled gracefully (logged and skipped)
```

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/diagnostics/coredump/gdb_test.exs`

```elixir
# Test: GDB.command/2 returns correct argument list for batch execution
# Test: GDB.parse_output/1 extracts thread list and frame data from GDB bt output
# Test: GDB.parse_output/1 identifies crash thread and signal info
# Test: GDB.parse_output/1 handles truncated or malformed output gracefully
```

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/diagnostics/coredump/lldb_test.exs`

```elixir
# Test: LLDB.command/2 returns correct argument list
# Test: LLDB.parse_output/1 extracts thread list and frame data from LLDB bt output
# Test: LLDB.parse_output/1 identifies crash thread and signal info
# Test: LLDB.parse_output/1 handles truncated or malformed output gracefully
```

### 6.2 Agency Dump Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/diagnostics/agency_dump_test.exs`

These tests verify agency state capture from a responsive agent. The REST client calls should be mocked.

```elixir
# --- Core behavior ---
# Test: AgencyDump queries single responsive agent (not all agents)
# Test: AgencyDump fetches /_api/agency/config
# Test: AgencyDump fetches /_api/agency/state
# Test: AgencyDump fetches /_api/agency/read with [["/arango"]] body
#
# --- Edge cases ---
# Test: AgencyDump skips with warning if no agents are alive
# Test: dump_agency_on_error: false disables the dump
#
# --- Lifecycle integration ---
# Test: dump_agency/1 runs before agent shutdown in stop_and_collect lifecycle
# Test: stop_and_collect uses multi-step protocol (dump_agency -> shutdown -> collect)
```

### Configuration Tests

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/test/toast/config_test.exs` (extend existing)

```elixir
# Test: TOAST_DEBUGGER env var sets debugger preference
# Test: debugger configurable via .toast.local.exs
# Test: debugger auto-detection when not configured
```

---

## Implementation

### Module 1: Debugger Behaviour

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/diagnostics/coredump/debugger.ex`

Define a behaviour that abstracts debugger-specific command construction and output parsing. Two implementations (GDB and LLDB) implement this behaviour.

```elixir
defmodule Toast.Diagnostics.Coredump.Debugger do
  @moduledoc "Behaviour for coredump debugger backends."

  @type frame :: %{
          function: String.t(),
          file: String.t() | nil,
          line: integer() | nil
        }

  @type thread :: %{
          id: integer(),
          frames: [frame()]
        }

  @type result :: %{
          signal: String.t() | nil,
          faulting_address: String.t() | nil,
          threads: [thread()],
          crash_thread: integer() | nil
        }

  @doc "Return the debugger executable name (e.g., \"gdb\", \"lldb\")."
  @callback executable() :: String.t()

  @doc "Build the command-line arguments for non-interactive stack trace extraction."
  @callback command(binary_path :: Path.t(), core_path :: Path.t()) :: [String.t()]

  @doc "Parse debugger output into a structured result."
  @callback parse_output(output :: String.t()) :: result()
end
```

### Module 2: GDB Implementation

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/diagnostics/coredump/gdb.ex`

```elixir
defmodule Toast.Diagnostics.Coredump.GDB do
  @moduledoc "GDB debugger backend for coredump analysis."
  @behaviour Toast.Diagnostics.Coredump.Debugger

  @doc "Returns \"gdb\"."
  @impl true
  def executable, do: "gdb"

  @doc """
  Build GDB batch-mode command:
  gdb -batch -ex "thread apply all bt full" -ex "quit" <binary> <core>
  """
  @impl true
  def command(binary_path, core_path)
    # ...construct argument list...
  end

  @doc "Parse GDB backtrace output into threads, frames, and crash info."
  @impl true
  def parse_output(output)
    # Parse thread headers (e.g., "Thread N (LWP pid):")
    # Parse frame lines (e.g., "#N  0xaddr in func (args) at file:line")
    # Identify crash signal from "Program terminated with signal ..." line
    # Filter out glibc/libstdc++ internal frames
  end
end
```

GDB output patterns to match:
- Thread header: `Thread N (LWP <pid>):` or `Thread N (Thread 0x... (LWP ...)):`
- Frame: `#N  0x<addr> in <function> (<args>) at <file>:<line>`
- Frame (no debug info): `#N  0x<addr> in <function> ()`
- Signal: `Program terminated with signal SIG<NAME>, <description>.`
- Faulting address: From signal info line

### Module 3: LLDB Implementation

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/diagnostics/coredump/lldb.ex`

```elixir
defmodule Toast.Diagnostics.Coredump.LLDB do
  @moduledoc "LLDB debugger backend for coredump analysis."
  @behaviour Toast.Diagnostics.Coredump.Debugger

  @doc "Returns \"lldb\"."
  @impl true
  def executable, do: "lldb"

  @doc """
  Build LLDB command:
  lldb -c <core> -o "thread backtrace all" -o "quit" -- <binary>
  """
  @impl true
  def command(binary_path, core_path)
    # ...construct argument list...
  end

  @doc "Parse LLDB backtrace output into threads, frames, and crash info."
  @impl true
  def parse_output(output)
    # Parse thread headers (e.g., "* thread #N, ...")
    # Parse frame lines (e.g., "  * frame #N: 0xaddr module`func at file:line")
    # Identify crash signal from "stop reason = signal SIG<NAME>" line
    # Filter out glibc/libstdc++ internal frames
  end
end
```

LLDB output patterns to match:
- Thread header: `* thread #N, name = '...'` or `  thread #N, ...`
- Frame: `    frame #N: 0x<addr> <module>\`<function> at <file>:<line>`
- Frame (no debug info): `    frame #N: 0x<addr> <module>\`<function>`
- Signal: `stop reason = signal SIG<NAME>`

### Module 4: Coredump Discovery and Analysis

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/diagnostics/coredump.ex`

This is the main module that orchestrates core file discovery, debugger selection, and analysis.

```elixir
defmodule Toast.Diagnostics.Coredump do
  @moduledoc """
  Discover and analyze core dump files from crashed ArangoDB server processes.

  Supports GDB and LLDB via pluggable debugger backends. Auto-detects available
  debugger at startup. Configured via Toast.Config (TOAST_DEBUGGER env var or
  .toast.local.exs).
  """

  alias Toast.Diagnostics.Coredump.{Debugger, GDB, LLDB}

  @type report :: %__MODULE__.Report{
          core_path: Path.t(),
          binary_path: Path.t(),
          debugger: :gdb | :lldb,
          signal: String.t() | nil,
          faulting_address: String.t() | nil,
          threads: [Debugger.thread()],
          crash_thread: integer() | nil
        }

  @doc "Discover core files for a server process."
  @spec discover(keyword()) :: [Path.t()]
  def discover(opts)
    # opts: server_dir, os_pid, work_dir
    # Search locations:
    #   0. TOAST_COREDUMP_DIR env var override (if set, search only this dir)
    #   1. {server_dir}/core*
    #   2. /tmp/core* filtered by PID
    #   3. Read /proc/sys/kernel/core_pattern:
    #      - If starts with "|" -> pipe handler (systemd-coredump)
    #        -> use coredumpctl to list/locate core files by PID
    #      - Otherwise -> use pattern to locate core files
  end

  @doc "Analyze a core file and extract stack traces."
  @spec analyze(Path.t(), Path.t(), keyword()) :: {:ok, report()} | {:error, term()}
  def analyze(core_path, binary_path, opts \\ [])
    # opts: debugger (module), timeout (default 60_000)
    # 1. Select debugger backend
    # 2. Build command via debugger.command/2
    # 3. Execute with timeout via System.cmd or :exec
    # 4. Parse output via debugger.parse_output/1
    # 5. Return structured report
  end

  @doc "Detect available debugger. Prefer LLDB, fall back to GDB."
  @spec detect_debugger() :: {:ok, module()} | :none
  def detect_debugger
    # Check System.find_executable for "lldb" then "gdb"
  end

  @doc """
  Discover and analyze all core files for a set of servers.

  Called post-shutdown with its own timeout. Returns list of reports.
  """
  @spec collect(keyword()) :: [report()]
  def collect(opts)
    # opts: servers (list of %{id, os_pid, server_dir, binary_path}), timeout, debugger
    # For each server with a known os_pid:
    #   1. discover core files
    #   2. analyze each with timeout budget
    # Return all reports
  end
end
```

#### Core File Discovery Strategy

The discovery function searches multiple locations because core file placement varies by system configuration:

1. **Server work directory**: `{server_dir}/core*` -- the most common location when `core_pattern` is just `core` or the process runs in its work dir.

2. **`/tmp` directory**: `/tmp/core*` filtered by PID -- some systems dump cores to `/tmp`. Filter by PID to avoid picking up unrelated cores.

3. **`/proc/sys/kernel/core_pattern`**: Read the kernel's core pattern to determine where cores actually go.
   - If the pattern starts with `|` (pipe character), it means cores are handled by a userspace program like `systemd-coredump` or `apport`. In this case, use `coredumpctl list --since <timestamp> PID` to locate core files. This is common on CI Docker images running under systemd.
   - Otherwise, interpret the pattern (which may contain `%p` for PID, `%e` for executable name, etc.) to construct the expected core file path.

4. **Match core files to servers by PID**: Core filenames typically contain the PID (`core.12345`). The `ProcessHistory` (from section-04) records OS PIDs for all server instances, enabling correlation.

#### Debugger Execution

The debugger runs non-interactively with a timeout (default 60 seconds). The command is executed via `System.cmd/3` with a `:timeout` option (or via `:exec` if finer control is needed). If the debugger times out, the analysis for that core file is skipped and the core file is still included in the result package for manual inspection.

Binary mismatch (debugger reports symbols do not match the core) is detected by checking for specific error patterns in the debugger output. These are logged as warnings and the core file is skipped for analysis but still collected.

#### Report Struct

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/diagnostics/coredump/report.ex`

```elixir
defmodule Toast.Diagnostics.Coredump.Report do
  @moduledoc "Structured report from coredump analysis."

  @type t :: %__MODULE__{
          core_path: Path.t(),
          binary_path: Path.t(),
          debugger: :gdb | :lldb,
          signal: String.t() | nil,
          faulting_address: String.t() | nil,
          threads: [map()],
          crash_thread: integer() | nil
        }

  defstruct [:core_path, :binary_path, :debugger, :signal,
             :faulting_address, :crash_thread, threads: []]
end
```

### Module 5: Agency Dump

**File**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/diagnostics/agency_dump.ex`

Captures agency state from a living agent in a cluster deployment. Since agents replicate a single globally shared state, querying one responsive agent is sufficient.

```elixir
defmodule Toast.Diagnostics.AgencyDump do
  @moduledoc """
  Capture agency state from a live ArangoDB agent for cluster diagnostics.

  Queries a single responsive agent for configuration, full state, and the
  /arango plan tree. This information is essential for diagnosing cluster
  test failures (shard leadership changes, replication issues, etc.).
  """

  @type t :: %__MODULE__{
          agent_id: String.t(),
          config: map() | nil,
          state: map() | nil,
          plan: map() | nil,
          error: String.t() | nil
        }

  defstruct [:agent_id, :config, :state, :plan, :error]

  @doc """
  Capture agency dump from the first responsive agent.

  Takes a list of agent endpoints (or a deployment handle) and queries the
  first one that responds. Returns the dump struct or nil if no agents are
  available.
  """
  @spec capture(keyword()) :: t() | nil
  def capture(opts)
    # opts: agents (list of %{id, endpoint}), client_opts, timeout
    # For each agent (try in order):
    #   1. GET /_api/agency/config -> store as config
    #   2. GET /_api/agency/state -> store as state
    #   3. POST /_api/agency/read with body [["/arango"]] -> store as plan
    #   If all three succeed, return the dump
    #   If agent unresponsive, try next agent
    # If no agents respond, return nil with warning logged
  end
end
```

#### REST Endpoints

The agency dump queries three endpoints on a single responsive agent:

| Endpoint | Method | Body | Output File |
|----------|--------|------|-------------|
| `/_api/agency/config` | GET | none | `agencyConfig_{agent_id}.json` |
| `/_api/agency/state` | GET | none | `agencyState_{agent_id}.json` |
| `/_api/agency/read` | POST | `[["/arango"]]` | `agencyPlan_{agent_id}.json` |

The `/_api/agency/read` endpoint with `[["/arango"]]` returns the full plan tree, which contains shard distributions, server registrations, and planned topology changes.

#### Agent Selection

The function iterates through the list of agents and queries the first one that responds. Since all agents in a healthy Raft group share the same state, any agent can provide the dump. If no agents are alive (all crashed), the dump is skipped with a warning -- this is a diagnostic best-effort, not a hard requirement.

#### Configuration

Enabled by default for cluster deployments. Controlled by:
- `dump_agency_on_error: true | false` in `Toast.Config` (default: `true`)
- CLI flag `--no-agency-dump` in `mix toast`
- `TOAST_DUMP_AGENCY` env var

### Integration: stop_and_collect Lifecycle

**File to modify**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment.ex`

The existing `stop_and_collect/1` function is refactored to use a multi-step protocol on the controller:

```
stop_and_collect(deployment):
  1. If cluster: dump_agency(controller)     # pre-shutdown, living agents
  2. shutdown(controller, timeout)            # stops all server processes
  3. collect_diagnostics(controller)          # logs, sanitizer files (existing)
  4. coredump_analysis(deployment)            # post-shutdown, own timeout
  5. merge all diagnostics and return
```

The key constraint is that agency dump MUST happen before agent processes are shut down (step 1 before step 2), while coredump analysis MUST happen after shutdown (step 4 after step 2) because the core files are written when the process exits.

#### Controller Changes for Agency Dump

**Files to modify**:
- `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/deployment/cluster_controller.ex`

Add a `dump_agency/1` GenServer call to the ClusterController. This call:
1. Identifies living agent server instances from the controller's state
2. Calls `Toast.Diagnostics.AgencyDump.capture/1` with the agent endpoints
3. Stores the result in the controller's state (alongside existing diagnostics)

The SingleServerController does not need this -- agency dumps are cluster-only.

```elixir
# In ClusterController, add:
def handle_call(:dump_agency, _from, state) do
  agents = get_living_agents(state)
  dump = Toast.Diagnostics.AgencyDump.capture(agents: agents)
  {:reply, dump, %{state | agency_dump: dump}}
end
```

#### Deployment Module Changes

Modify `stop_and_collect/1` in `Toast.Deployment`:

```elixir
def stop_and_collect(%__MODULE__{} = deployment, opts \\ []) do
  mod = controller_module(deployment)
  timeout = Keyword.get(opts, :timeout, default_shutdown_timeout(deployment))

  # Step 1: Agency dump (cluster only, pre-shutdown)
  agency_dump =
    if deployment.mode == :cluster and agency_dump_enabled?(deployment) do
      mod.dump_agency(deployment.controller)
    end

  # Step 2: Shutdown all servers
  with :ok <- mod.shutdown(deployment.controller, timeout) do
    # Step 3: Collect existing diagnostics (logs, sanitizer)
    base_diagnostics = mod.get_info(deployment.controller)[:diagnostics]

    # Step 4: Coredump analysis (post-shutdown, own timeout)
    coredump_reports = collect_coredumps(deployment, opts)

    # Step 5: Merge and return
    merge_diagnostics(base_diagnostics, agency_dump, coredump_reports)
  end
end
```

#### Coredump Collection Integration

The coredump analysis runs after shutdown with its own timeout (separate from the shutdown timeout). It needs access to the server process info (OS PIDs, binary paths, work directories) which is available from the controller's state before termination.

```elixir
defp collect_coredumps(deployment, opts) do
  debugger = resolve_debugger(opts)

  case debugger do
    :none ->
      Logger.warning("No debugger available, skipping coredump analysis")
      []

    {:ok, debugger_module} ->
      coredump_timeout = Keyword.get(opts, :coredump_timeout, 120_000)
      servers = get_server_info(deployment)
      Toast.Diagnostics.Coredump.collect(
        servers: servers,
        debugger: debugger_module,
        timeout: coredump_timeout
      )
  end
end
```

### Configuration Extensions

**File to modify**: `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/lib/toast/config.ex`

Add new configuration keys to `Toast.Config`:

```elixir
# In the Config struct, add:
debugger: :auto | :gdb | :lldb | :none   # default: :auto
dump_agency_on_error: boolean()           # default: true
coredump_timeout: pos_integer()           # default: 120_000 (2 minutes)
```

Configuration precedence for debugger:
1. Keyword opt `debugger:` passed to `stop_and_collect/1`
2. `TOAST_DEBUGGER` env var (`"gdb"`, `"lldb"`, `"auto"`, `"none"`)
3. `.toast.local.exs` setting (`debugger: :lldb`)
4. Default: `:auto` (auto-detect, prefer LLDB, fall back to GDB)

This follows the established precedence chain: keyword opts > env vars > `.toast.local.exs` > defaults.

---

## File Summary

### New Files

| File | Purpose |
|------|---------|
| `lib/toast/diagnostics/coredump.ex` | Main coredump discovery and analysis orchestrator |
| `lib/toast/diagnostics/coredump/debugger.ex` | Behaviour for debugger backends |
| `lib/toast/diagnostics/coredump/gdb.ex` | GDB implementation |
| `lib/toast/diagnostics/coredump/lldb.ex` | LLDB implementation |
| `lib/toast/diagnostics/coredump/report.ex` | CoredumpReport struct |
| `lib/toast/diagnostics/agency_dump.ex` | Agency state capture for cluster diagnostics |
| `test/toast/diagnostics/coredump_test.exs` | Tests for discovery, analysis, fallbacks |
| `test/toast/diagnostics/coredump/gdb_test.exs` | GDB command and parse tests |
| `test/toast/diagnostics/coredump/lldb_test.exs` | LLDB command and parse tests |
| `test/toast/diagnostics/agency_dump_test.exs` | Agency dump tests |

All file paths are relative to `/home/mpoeter/dev/arangodb/arango_next4/tests/elixir/toast/`.

### Modified Files

| File | Changes |
|------|---------|
| `lib/toast/deployment.ex` | Refactor `stop_and_collect/1` to multi-step protocol with agency dump and coredump analysis |
| `lib/toast/deployment/cluster_controller.ex` | Add `dump_agency/1` GenServer call |
| `lib/toast/config.ex` | Add `debugger`, `dump_agency_on_error`, `coredump_timeout` config keys |

---

## Implementation Order

1. **Debugger behaviour and implementations** (`debugger.ex`, `gdb.ex`, `lldb.ex`, `report.ex`) -- these are pure data transformation modules with no dependencies on the rest of Toast. Write and test in isolation.

2. **Coredump discovery and orchestrator** (`coredump.ex`) -- depends on the debugger modules. Discovery logic is filesystem-based; analysis delegates to the debugger backend.

3. **Agency dump module** (`agency_dump.ex`) -- independent of coredump modules. Uses `Toast.Client` (from section-03 if available, or raw HTTP via `Req` directly) to query agent endpoints.

4. **Config extensions** (`config.ex`) -- add the new config keys. Small change.

5. **Lifecycle integration** (`deployment.ex`, `cluster_controller.ex`) -- wire the new modules into `stop_and_collect/1`. This is the final integration step that connects everything.

---

## Design Notes

### Frame Filtering

Both GDB and LLDB parsers should filter out internal runtime frames that add noise to crash reports. Filter frames where the function matches patterns like:
- `__libc_*`, `__GI_*` (glibc internals)
- `std::*` (libstdc++ internals, though some may be relevant)
- `_start`, `__libc_start_main` (process entry)
- `clone`, `start_thread` (thread creation)

Keep all frames with `arangodb` or `arangod` in the module/binary path, plus any frames in the vicinity of the crash (first N frames of the crashing thread are always kept).

### Graceful Degradation

Every external dependency (debugger binary, filesystem paths, REST endpoints) must fail gracefully:
- No debugger installed: log warning, skip analysis, still collect raw core files for packaging
- Debugger times out: log warning, skip that core file's analysis
- Binary mismatch: log warning, skip that core file's analysis
- No agents alive: log warning, return nil agency dump
- Agency endpoint returns error: log warning, store error in dump struct

This ensures diagnostics collection never blocks or crashes the shutdown path.

### Coredump Timeout

Coredump analysis gets its own timeout (default 2 minutes, configurable) separate from the shutdown timeout. This is because debugger execution on large core files can be slow, especially with full backtraces. The timeout is applied as a budget across all core files for the deployment, not per-file.