# Event Vocabulary

The canonical reference for every event recorded in `ToastTest.EventStore`.

Events are emitted exclusively through per-layer constructor modules — no
caller builds event maps by hand:

- **`Toast.Deployment.Events`** — deployment lifecycle events, emitted through
  the `Toast.Deployment.EventListener` behaviour (the port between the
  reusable `Toast` infrastructure and the `ToastTest` runner).
- **`ToastTest.Runner.Events`** — test lifecycle and diagnostics events,
  recorded directly in the EventStore.
- **`ToastTest.Events`** — the public API for custom events from test code.

When adding an event or a field, add it to the constructor and to this
document. Consumers pattern-match on event maps; a field that is not produced
by the constructor does not exist, no matter what a consumer hopes to read.

## Envelope

Every event carries:

| Field | Type | Notes |
|---|---|---|
| `event` | atom | event type |
| `timestamp` | integer | Unix microseconds (`Toast.get_timestamp/0`); set at emission for deployment events, at `EventStore.notify/1` for runner events unless passed explicitly (JS after-the-fact reporting) |

Deployment events additionally carry `deployment_id` (String).

## Deployment events (`Toast.Deployment.Events`)

| Event | Producer(s) | Payload | Consumers |
|---|---|---|---|
| `:deployment_starting` | DeployPipeline | `mode` (`:cluster`/`:single_server`), `stacktrace`, `specs` — list of `%{id, role, port, endpoint, log_file, server_dir}` (the complete static birth record; `endpoint` is derived once in `init_servers_from_specs`) | Projections: deployment metadata + server birth records |
| `:deployment_started` | DeployPipeline | — (pure status transition; all server metadata is on `:deployment_starting`) | Projections: backfills deployment metadata only when no `:deployment_starting` preceded it |
| `:deployment_stopped` | ShutdownPipeline | — | Projections: `stopped_at` |
| `:server_started` | DeployPipeline (initial launch), Controller (restart/start ops) | `server_id`, `pid` (OS pid) | Projections: `pids_by_server`, opens an incarnation record |
| `:server_stopped` | Controller (stop op, restart), ShutdownPipeline | `server_id`, `pid`, `reason` (currently always `nil`) | Projections: closes the incarnation |
| `:server_killed` | Controller (kill op) | `server_id`, `pid` | Projections: closes the incarnation (matched by `pid`) |
| `:server_unhealthy` | Controller (health-monitor verdict, before SIGABRT) | `server_id` | Projections: appends to the server's `unhealthy_verdicts` — explains the subsequent signal-6 `:server_crashed` |
| `:server_paused` | Controller (pause op) | `server_id` | Timeline display only — pause windows are deliberately not modeled on incarnations (a paused incarnation reads as running; revisit if upgrade/pause analysis needs it) |
| `:server_resumed` | Controller (resume op) | `server_id` | Timeline display only — see `:server_paused` |
| `:server_crashed` | Controller | `server_id`, `pid`, `crash_info` (`Toast.Process.CrashInfo` — carries the `executable` of the crashed incarnation, stamped by its `ServerProcess` at crash time), `expected` (boolean) | Projections: `unexpected_crashes` (when `expected: false`), closes the incarnation; `ManagedDeploymentListener` aborts the run on unexpected crashes; `crash_info.executable` drives coredump analysis |
| `:server_identified` | DeployPipeline (post-deploy) | `server_id`, `arango_id` | Projections: `arango_id` on server metadata |
| `:timeout_kill` | Controller (`source: :startup`), ShutdownPipeline (`source: :shutdown`) | `source`, `reason` (String), `servers` — list of `%{server_id, os_pid, log_file}` | Projections → `Attribution.timeout_issues` |

## Runner events (`ToastTest.Runner.Events`)

Test lifecycle events are additionally broadcast to the ExUnit event manager
(ResultCollector, CLI formatter); diagnostics events go to the EventStore only.

| Event | Producer(s) | Payload | Consumers |
|---|---|---|---|
| `:module_started` / `:module_finished` | Runner (test execution) | `module` | Attribution time windows (`TimeWindows.build/1`); log interleaving (`toast.analyze detail`) |
| `:test_started` | Runner (test execution) | `module`, `name` | Attribution time windows; log interleaving |
| `:test_finished` | Runner (test execution) | `module`, `name`, `outcome`, `duration_us` | Attribution time windows; log interleaving |
| `:between_tests_finished` | Runner (after the between-tests barrier) | `module`, `name` (the test that just ran) | Attribution time windows — extends the test's window so crashes detected during the barrier still attribute to it |
| `:netstat_snapshot` | Runner (between-tests check, baselines) | `total` (socket count), `label` (`:pre_deployment`, `:deployment_ready`, or `nil`) | Netstat trajectory in `toast.analyze detail` |
| `:infrastructure_issue` | Runner (between-tests check) | `subtype` (e.g. `:port_exhaustion`), `detail` (map) | Projections → PostExecution infrastructure issues |
| `:timeout_kill` | Runner.Timeout (`source: :global` or per-test) | `source`, `reason`, `servers` (same shape as the deployment variant; `[]` when unknown) | Projections → `Attribution.timeout_issues` |

Note: `:timeout_kill` has producers in both layers. The runner variant carries
no `deployment_id`; consumers must not rely on it.

## Custom events (`ToastTest.Events`)

| Event | Producer | Payload | Consumers |
|---|---|---|---|
| `:custom` | test code via `ToastTest.Events.custom/2` | `kind` (atom), `payload` (map, must not contain `:event`/`:timestamp`/`:kind`) | Log interleaving in `toast.analyze detail` |
