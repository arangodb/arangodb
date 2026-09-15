*AI generated docs.*

# RBAC scenarios for rta-makedata

Runs the **unmodified** rta-makedata workload under a series of RBAC configurations and checks that each behaves the way the role definition says it should.

RBAC is the variable; rta-makedata is the payload. Nothing in `3rdParty/rta-makedata` is changed, and no test code is added to it.

## Why drive rta-makedata rather than probe the APIs

`makedata` / `checkdata` / `cleardata` already create and drop databases, collections, indexes, views, analyzers and graphs, write and read documents, and run AQL. That makes them a much broader probe of arangod's RBAC enforcement than a hand-written list of endpoint checks — and, because the workload is maintained elsewhere, it keeps pace with the product instead of drifting from it.

So the question each scenario asks is not "does endpoint X return 403" but **"given permission set X, does the real workload behave as the role promises?"** A scenario failing where it should pass means the documented action set for that role is missing something arangod actually checks.

## Layout

```
tests/api/rbac/
  scripts/            stack setup, reused as-is (see "Provenance" below)
  mkjwt.py            mints ArangoDB-compatible HS256 JWTs
  rta/
    run_all.sh        every layer, cheapest first - start here
    scenarios.py      the catalogs: RBAC configs and classic grants, + outcomes
    run_scenarios.py  the driver
    selfcheck.py      validates the catalogs against arangod's own vocabulary
    README.md         this file
```

## Prerequisites

| Need | Where from |
|---|---|
| `arangod`, `arangosh` | this checkout, `build/bin/` |
| `arangodb_operator` (sidecar) | kube-arangodb, `make bin-all` |
| `python3` | — |
| unprivileged user namespaces | kernel default on most distributions — see "Topology" |

Point the setup scripts at the operator binary with `OPERATOR` (see `../scripts/env.sh`; every path there is overridable).

## Two supported configurations

arangod authorizes one of two ways, and **both** are valid deployments that the
workload has to survive:

| | started with | authorization | catalog |
|---|---|---|---|
| **RBAC** | `--server.external-rbac-service=<sidecar>` | policies + roles + scoped bindings, in the sidecar | `scenarios.build()` |
| **classic** | *without* that option | `_users` grants (`rw` / `ro` / `none`), in arangod | `scenarios.build_classic()` |

Classic is not a broken RBAC setup — it is the default, and the regression baseline the RBAC work must not break. `--auth-mode {auto,rbac,classic}` picks the catalog; `auto` probes the server.

The probe is a user with **no classic grants** and an allow-all RBAC binding: only an arangod consulting the RBAC service can let that user in. It matters because guessing wrong produces a *misleading half-green run* rather than an obvious failure — the negative scenarios still report `deny`, since a user with no classic grants has no access either. Measured, running the RBAC catalog against a classic server:

```
superuser-control      3 steps  ok       (superuser bypasses both models)
coredb-admin-in-scope  pass -> deny  MISMATCH
no-binding             deny -> deny  ok   <- passing for the wrong reason
```

Four of five steps look fine with the feature switched off. Hence the probe.

Differences worth knowing when comparing the catalogs:

- Classic has no scope. The database grant *is* the boundary, so "out of scope" becomes "granted on a different database".
- Classic gates the server version on `--server.harden` + `rw` on `_system`; RBAC always gates it, on `db:AdminMonitoringInternal`. `start_arangod_classic.sh` does not harden, so classic keeps the `version` field for everyone. See "Reading the server version needs an admin action" below.
- **Every classic scenario needs `_system` read access**, for the same reason the RBAC scopes need `db:database:_system`: arangosh's connect handshake and makedata's startup probes carry no `/_db/` prefix. Without it a scenario is refused at the handshake, which makes the deny scenarios pass for the wrong reason and the positive ones fail for an unrelated one.
- **Creating or dropping a database is gated on `_system` rw** in classic, where RBAC maps it to `db:Create` / `db:Drop` on the database resource. The classic full-cycle scenario therefore needs a broader grant than its RBAC counterpart — a real model difference, not a harness quirk.
- A **collection-level** grant requires its database to already exist (`404 database not found` otherwise), while a database-level grant on an absent database is accepted and stored. The runner pre-creates the database for scenarios whose grants name a collection.

## Topology (RBAC configuration)

Two processes, not three:

```
arangosh ──JWT──> arangod :8529
                    │  --server.external-rbac-service
                    ▼
                  sidecar :8108  HTTP gateway
                                   /_management/permissions/*      management API
                                   /_integration/authn/v1/*        authentication.v1
                                   /_integration/authorization/v1/* authorization.v1
                          :8109  gRPC (pool)
                          :8107  health
```

`arangodb_operator_integration` is **not** part of this. The sidecar serves authn.v1 and authz.v1 on its own gateway (`pkg/sidecar/register.go`), so `--server.external-rbac-service` points at the sidecar. `--management` and `--integration` are consequently the same address.

Three things are needed to run the sidecar outside a Pod, all handled by `../scripts/start_sidecar.sh`:

- **A writable `/var/run/sidecar`.** The sidecar opens a unix socket there for internal service-to-service calls and exits if it cannot create it. The path is hardcoded (`pkg/util/constants/sidecar.go`). The script runs it in a user+mount namespace with a writable directory bind-mounted over `/run` — no root required, but unprivileged user namespaces must be enabled.
- **`CENTRAL_INTEGRATION_SERVICE_ADDRESS`** → the sidecar's own gRPC port, so the authorization.v1 pool client finds the pool to sync from. Without it authorization.v1 stays `degraded` and arangod sees every check fail.
- **`INTEGRATION_ARANGO_JWT_FOLDER`** → the JWT folder, so that same client can authenticate. The connection carries no credentials otherwise and the sidecar answers its own client with `Unauthenticated: Unauthorized`.

Both env vars mirror what the operator injects in `pkg/deployment/resources/internal_sidecar.go`.

Readiness is `authorization.v1=healthy` in the sidecar log, not merely the gateway port being bound — a bound gateway with a degraded pool client denies everything.

## Running everything

```bash
export OPERATOR=/path/to/kube-arangodb/bin/linux/amd64/arangodb_operator
tests/api/rbac/rta/run_all.sh
```

That runs four layers, cheapest first, and prints a PASS/FAIL summary:

| layer | what | needs | time |
|---|---|---|---|
| `offline` | self-check, both catalog listings, both dry runs | nothing | seconds |
| `rbac` | brings up arangod + sidecar, runs the RBAC matrix | `OPERATOR` | ~20 min |
| `role-modelling` | what the documented role set alone can do | the `rbac` layer's stack | ~3 min |
| `classic` | arangod with no RBAC service, runs the grant matrix | — | ~15 min |

Subsets and overrides:

```bash
tests/api/rbac/rta/run_all.sh --layers offline          # CI without binaries
tests/api/rbac/rta/run_all.sh --layers rbac,classic     # both configurations
tests/api/rbac/rta/run_all.sh --test 050,400,500        # override the suite filter
tests/api/rbac/rta/run_all.sh --help
```

Most of the wall time is denials: a step that is *meant* to be refused still costs about two minutes, because rta-makedata's `createSafe()` retries a failing create 50 times before giving up.

## Running through the standard test harness

```bash
# 1. plain workload under RBAC. Starts utils/rbac_dummy.py, an allow-everything
#    stub. Proves the RBAC code path does not break the workload - and nothing
#    more (see the warning below).
./scripts/unittest rta_makedata --rbac true

# 2. the same, plus the scenario matrix, against a real authorization sidecar.
#    The sidecar must validate tokens with the harness JWT secret ('haxxmann').
#    env.sh generates a *random* secret when the key file is absent, so write it
#    yourself before bringing the stack up:
export RBAC_WORK=/tmp/rbac-ut
mkdir -p $RBAC_WORK/jwt && printf haxxmann > $RBAC_WORK/jwt/-
tests/api/rbac/scripts/start_arangod.sh http://127.0.0.1:8108
OPERATOR=/path/to/kube-arangodb/bin/linux/amd64/arangodb_operator \
  tests/api/rbac/scripts/start_sidecar.sh central

./scripts/unittest rta_makedata --rbac http://127.0.0.1:8108
```

A stack whose key file holds a random secret works fine for `run_all.sh` — the
runner mints its own tokens with that same secret — but the harness signs with
`haxxmann` and cannot be told otherwise, so its tokens are rejected. The
symptom is every phase failing to authenticate, not a policy denial.

With a URL, `rta_makedata` runs the scenario matrix as a final phase, reported as
`rta_RbacScenarios`. It is skipped, with a reason, when `--rbac` is just `true`:
the dummy serves no management API, so there is nothing to seed scenarios
through. `--rtaRbacDir` overrides where the runner is looked for.

> **`--rbac true` on its own is not a test of authorization.** The dummy answers
> `Allow` to everything, so the plain workload only proves the RBAC code path
> does not break it. Use the sidecar URL form for anything that has to be
> sensitive to policy.

### What the harness has to do for RBAC to be real

Three things, all of them load-bearing and all of them verified by removing them
again and watching the run fail:

1. **Authentication must be on.** `ExecContext::create` returns
   `AuthMode::Disabled` when the authentication feature is off, and then no
   policy is ever evaluated, whatever `--server.external-rbac-service` says.
   `rta_makedata` merges `tu.testServerAuthInfo` into `options` only, which
   tells the *client* to authenticate but never reaches arangod's own arguments,
   so before this was fixed the server ran unauthenticated and `--rbac` was
   silently a no-op. Measured with the fix reverted: every scenario reports
   `MISMATCH`, including the deny ones.
2. **The workload's own account needs a binding.** A real sidecar denies by
   default, so `root` cannot even complete arangosh's connect handshake until it
   is bound. The suite calls `run_scenarios.py --bootstrap-user <username>`
   before the workload phases and `--remove-bootstrap-user` after. Production
   does the same thing: the operator binds `root` to
   `managed:predefined:super-admin` on bootstrap.
3. **Request compression must be off.** See the defect below.

Measured falsifications of the integrated path:

| Experiment | Result |
|---|---|
| `--rbac http://127.0.0.1:1` (dead PDP) | `Fail` - bootstrap cannot reach the sidecar |
| authentication fix reverted | `Fail` - 7 scenario `MISMATCH`es |
| both fixes in place | `Success` |

## Port ownership

`start_sidecar.sh` kills only the sidecar recorded in `$RBAC_WORK/sidecar.pid`.
A sidecar started from a different `RBAC_WORK` - for instance the long-lived one
you point `--rbac <url>` at - still holds `:8107`, `:8108` and `:8109`, and
`run_all.sh` cannot then bring up its own. The script now says so explicitly and
names the holding process; stop it first, or set `RBAC_WORK` to the directory
that owns it.

## Running individual pieces

```bash
# offline sanity, no server needed
tests/api/rbac/rta/run_scenarios.py --self-check
tests/api/rbac/rta/run_scenarios.py --list
tests/api/rbac/rta/run_scenarios.py --dry-run --verbose

# bring up a local stack and run everything
export OPERATOR=~/kube-arangodb/bin/linux/amd64/arangodb_operator
tests/api/rbac/rta/run_scenarios.py --setup

# against an already-running stack
tests/api/rbac/rta/run_scenarios.py \
    --endpoint tcp://127.0.0.1:8529 \
    --management http://127.0.0.1:8108 \
    --integration http://127.0.0.1:8108 \
    --jwt-secret-file /tmp/arangodb-rbac-test/jwt/-

# one scenario, full output
tests/api/rbac/rta/run_scenarios.py --scenario coredb-admin-in-scope --verbose

# the classic configuration: an arangod started WITHOUT
# --server.external-rbac-service. ../scripts/start_arangod_classic.sh puts one
# on :8530. The catalog is selected by probing, so --auth-mode is only needed to
# override the probe.
bash tests/api/rbac/scripts/start_arangod_classic.sh
tests/api/rbac/rta/run_scenarios.py --endpoint tcp://127.0.0.1:8530 --test 050,100,400
```

Exit code is 0 only if every step matched its expectation.

## The scenarios

`--list` prints the live catalog. In outline:

| Scenario | Config | Expectation |
|---|---|---|
| `superuser-control` | RBAC bypassed | everything passes — proves the stack and workload are healthy before blaming permissions |
| `coredb-admin-in-scope` | documented coredb-admin action set, scope on the target db | everything passes |
| `coredb-reader-in-scope` | coredb-reader action set | `checkdata` passes, `makedata` is denied |
| `coredb-developer-in-scope` | coredb-developer action set | `makedata` denied — no `db:Create` |
| `coredb-admin-out-of-scope` | admin policy, scope on *another* db | denied; the scope alone makes the difference |
| `no-binding` | policy and role, never bound | denied (default deny) |
| `binding-without-scope` | bound, but scope absent | the management API **rejects** the binding (`Scope cannot be empty`); no workload runs |
| `admin-without-api-version` | full action set minus `db:UseApiVersion` | denied — the gate runs before every handler |
| `admin-with-explicit-deny` | admin policy + `Deny` on one collection | denied — `Deny` beats `Allow` in the same policy |
| `reader-permissive-mode` | insufficient policy, `central-permissive` | passes — denials logged, not enforced |
| `documented-admin-set-only` | the documented coredb-admin set *without* `db:AdminMonitoringInternal` | the workload cannot read the server version, so version-dependent suites break before any permission decision. Intended behaviour; opt-in via `--group role-modelling` |

### Why the roles are emulated rather than bound

The scenarios build their own policies using the action sets documented in `documents/DesignDocuments/03_IN_PROGRESS/RbacPredefinedRolesInformationFlow.md`, instead of binding `managed:predefined:coredb-*`.

kube-arangodb's `design/rbac/predefined_roles.md` states that in the MVP only `super-admin` ships a bundled policy and the rest are **empty containers**. Binding them would grant nothing and every scenario would fail identically, testing nothing. Emulating the documented policy means the *definition* is under test: if the intended action set for a role is wrong or incomplete, a scenario fails and says so.

When the predefined roles gain real bundled policies, the natural follow-up is a second pass that binds them directly and expects the same outcomes.

## Notes and limitations

**Authentication must be JWT.** arangod forwards `req.jwtToken()` to the policy decision point, and that is empty for Basic auth, so a username/password login is rejected before any policy is consulted. The driver always uses `--server.jwt-token`.

**`--server.harden true` is mandatory.** `ExecContext.h:164` asserts `!isRbac() || _isRestApiHardened` and aborts the process — release builds included — on the first authenticated hardened-endpoint hit. `start_arangod.sh` passes it.

**Suite filter.** Default `--test 050,100,400,500,580,607,612` — databases; collections, indexes and documents; views; graphs; analyzers. Every suite in that list has been verified to run under both authorization models.

Suite `100` gates itself on `semver.coerce(db._version())`, which needs `db:AdminMonitoringInternal` under RBAC; the scenarios grant it, so it is included. This requirement will change some time soon.

Pass `--test ''` for everything, bearing the Foxx blocker in mind.

A single-suite filter such as `--test 050` is passed on doubled (`050,050`).
makedata parses its own argv with `internal.parseArgv`, which turns a purely
numeric value into a Number and then calls `.split(',')` on it; repeating the
value keeps it a string without changing which suites match, since each entry
is an independent substring filter. rta-makedata stays unmodified, so the
workaround lives in `phase_argv()` and in the `rta_makedata` suite.

**Narrow filters disable some scenarios.** Two of them - and `classic-ro` -
work by letting a superuser populate the data and then re-running `makedata` as
the restricted user, so they only mean something if the selected suites attempt
a write even when their objects already exist. Suite `050` does not: it creates
databases, finds them present and does nothing. Those scenarios carry
`needs_write_on_rerun`, and the runner skips them, naming the filter, unless it
contains `100` or `400`. Measured: `--test 050` made both report `pass`;
`--test 050,400` made both deny.

**Deny scenarios are slow.** rta-makedata's `createSafe()` retries a failing create 50 times with a growing sleep before giving up, so a step that is *meant* to be denied still takes roughly two minutes. `--phase-timeout` defaults to 1800s to accommodate it.

**How a denial is recognised.** `deny` requires a non-zero exit **and** evidence of a permission decision in the output — in practice `One of the requests has been denied`, the policy decision point's batch message propagated verbatim by arangod. Anything else that merely exited non-zero is reported as `error`, so a crash cannot satisfy a `deny` expectation.

A refusal has no single wording. These are the ones actually observed, each with the source that produces it:

| message | where from | signature |
|---|---|---|
| `One of the requests has been denied` | the policy decision point's batch verdict, propagated verbatim by `Auth/Rbac/ServiceImpl.cpp` | `has been denied` |
| `Failed to use database 'x'. Request requires database authentication level 'rw' but it has only level 'ro'.` | `accessLevelMismatchReason()`, `Auth/AuthMode.cpp:251` | `authentication level` |
| `ArangoError 11: No read access to database.` | `RestHandler::checkDatabaseAccess()` — the classic database gate on API version 0 | `no read access to database`, `arangoerror 11:` |
| `User not authenticated` | the *same* gate under RBAC — misleading wording, but an authorization failure | `user not authenticated` |
| `forbidden: secret [read]` | collection-level refusal | `forbidden` |

None of the classic wordings contain "forbidden", "denied" or a status code, which is why they initially classified as `error`. Each signature was checked against the source before being added — `authentication level`, for instance, is the only occurrence of that phrase in the whole tree. `arangoerror 11:` keeps its trailing colon so it cannot match unrelated error numbers that merely begin with 11.

The lesson worth keeping: classification is per-signature and evidence-based, never "non-zero exit means denied" and never "this status code means denied". Both shortcuts were tried and both produced wrong verdicts.

Two further cases need more than text, and both are resolved the same way: by asking an identity that is never denied.

- **`not connected`.** A user denied their *first* request never completes arangosh's connect handshake, so the failure reads `ArangoError 2001: not connected` and mentions no permissions at all — indistinguishable from a dead server. The runner probes arangod as superuser: healthy server plus refused user is a denial; unreachable for the superuser too is a real error.
- **`not found` (404).** An authorization failure can be reported as 404 rather than 403, to avoid revealing that an object exists. The wording is then identical to a genuinely missing object. 404 is therefore *never* a denial signal on its own — a missing fixture or a real bug would score as a successful `deny`. Instead, when the object is named (`collection or view not found: foo`) the runner asks whether the superuser can see it: if yes the 404 was hiding it (denial), if no it is genuinely absent (error). An unnamed 404 stays an error, because nothing can distinguish it.

### Measured status codes

What this build actually returns, for a user scoped to `rbac_visible` but not to the collection `secret`:

| Request | Result |
|---|---|
| in-scope database / collection / AQL | `200`/`201` |
| out-of-scope **database** (exists) | `401` `has been denied` |
| database that does not exist | `401` `not authorized to execute this request` — absence hidden behind a denial |
| out-of-scope **collection** (properties, document read, document write) | `403` `has been denied` / `forbidden: secret [read]` |
| AQL over an out-of-scope collection | `403` `forbidden: secret [read]` |
| out-of-scope **view** / **graph** | `403` `has been denied` |
| collection / view that does not exist | `404` errorNum `1203` |
| AQL over a collection that does not exist | `404` errorNum `1203` |
| **graph** that does not exist | `403` — again, absence hidden behind a denial |

So on this build denials are `403`, the database gate is `401` (the API-version-0 backwards-compatibility path in `RestHandler::handleAuthorizationChecks`), and `404` means genuine absence. The mapping is not uniform across resource types, though — a missing database or graph comes back as a denial while a missing collection or view comes back as `404` — so the runner does not depend on any particular code, only on evidence.

Two traps worth knowing, both of which produced wrong readings while this was being written:

- A `404` observed for a denied collection turned out to be a genuine absence: the probe sequence had dropped the collection moments earlier. Always check what state the object is actually in before concluding that a code means "denied".
- Reusing one user and *changing* its scope reads the old permissions until the cache flips, and a readiness probe that waits for something both the old and the new scope allow will not notice. That is why every scenario here gets its **own** user, so the wait is always a genuine nothing-to-Allow transition.

**Reader vs developer is not positively separable.** makedata has no write-without-create phase, so both roles pass `checkdata` and fail `makedata`. The developer scenario therefore establishes only that the developer action set is insufficient for the workload (i.e. `db:Create` really is required and enforced), not that writing works where creating does not. Separating them would need a workload phase that writes into existing collections.

**Reading the server version needs an admin action.** This is **intended behaviour**, not a defect. On a hardened server the `version` field of `/_api/version` sits behind an admin permission:

| model | what the user needs | when |
|---|---|---|
| RBAC | `db:AdminMonitoringInternal` = Allow | always — RBAC forces `--server.harden` (`ExecContext.h:160` asserts it) |
| classic | `rw` on `_system` (i.e. admin) | only when started with `--server.harden` |

The gate is `canUseHardenedAction()`, which returns early — allowing everyone — unless the server is hardened, and otherwise requires the admin action.

In `RestVersionHandler.cpp::getVersion()`, `result.add("version", ...)` sits inside `if (allowInfo)`, so it is the **`version` field itself** that is withheld, not merely the details block:

```
$ curl -H "authorization: bearer $SUPERUSER" .../_api/version
{"server":"arango","license":"enterprise","version":"3.12.11-devel",...}

$ curl -H "authorization: bearer $SCOPED_ADMIN" .../_api/version
{"server":"arango","license":"enterprise","apiVersions":["v0"],...}     # no version
```

`db._version()` returns `requestResult.version`, so it yields `undefined`, and every version-dependent client path breaks. arangosh itself prints `Client/server version mismatch detected. arangosh version: 3.12.11-devel, server version: ` with an empty value. In rta-makedata it surfaces as a `TypeError: Invalid version. Must be a string.` from `semver` inside `100_collections.js::isSupported` — **before any permission decision is reached**, which is why it classifies as `error` rather than `deny`.

The scenarios therefore grant `db:AdminMonitoringInternal` explicitly (see `MONITORING` in `scenarios.py`), which is what lets suite `100` run under RBAC.

**Two consequences worth passing to whoever owns the role catalog.** Neither is a bug; both are things a deployment has to get right.

1. `db:AdminMonitoringInternal` is not part of any documented `coredb-*` action set, so a user holding only those roles cannot run arangosh-based tooling that reads the server version. The `documented-admin-set-only` scenario (`--group role-modelling`) demonstrates exactly that.
2. An admin action carries **no resource** — `resourceToWireString()` maps `NoResource` to the empty string, and only the bare `*` pattern matches it. So a binding scope has to permit admin actions in a statement of their own. The scope example in `RbacPredefinedRolesInformationFlow.md` (`actions: ["*"]`, `resources: ["db:database:xyz", "db:collection:xyz:*"]`) cannot grant any admin action at all. The catalog keeps that allowance in a separate statement so resource scoping still applies to every data action — putting `*` in the main statement would silently unscope everything.

**Propagation.** A permission change travels operator → sidecar pool → streaming cache before arangod sees it; the documented worst case is ~30s. The driver polls the PDP until the new binding is visible, up to `--propagation-timeout` (90s).

## Running against more than one arangod revision

The RBAC implementation is still moving, so the catalog adapts instead of pinning a branch. `selfcheck.capabilities()` detects which optional parts of the model the arangod under test has, and `scenarios.build()` takes the result:

| Capability | `devel` | `feature/rbac-api-tester` |
|---|---|---|
| `db:apiversion:v<n>` resource + `checkApiVersionAccess` call site | yes | **no** |
| `db:AdminReadUsers` action | yes | **no** |
| total actions | 37 | 36 |

Where there is no API-version gate, the `db:apiversion:*` scope entry is dropped (it could never match) and `admin-without-api-version` is left out of the catalog — asserting a denial there would be asserting behaviour the build does not have. That is why the same catalog yields 10 scenarios / 21 steps on `devel` and 9 / 20 on the branch.

Detection prefers **`build/bin/arangod` over the source**, because the binary is what serves the requests. A checkout whose build is stale is easy to end up with, and trusting the source would then tune the catalog to code that is not running. When the two disagree, `--self-check` says so:

```
WARNING build/bin/arangod and the checked-out source disagree about the RBAC
model - the build is stale. The binary wins, since it is what runs, but
rebuild before trusting a result.
```

Detection falls back to the source when no binary is present.

## Provenance

`../scripts/`, `../mkjwt.py` and the rest of `tests/api/rbac/` come from the `feature/rbac-api-tester` branch, which is **not merged into `devel`** — it is a separate line of development, and its `arangod/Auth/Rbac` is older than devel's. `setup_all.sh` is intentionally *not* used: it also seeds a demo user and policy, which would contaminate the scenarios. The driver calls `start_arangod.sh`, `start_sidecar.sh` and `start_integration.sh` directly.

Everything under `rta/` is new and branch-independent — it detects the arangod it is pointed at, so it works on `devel` too, where the `scripts/` it depends on would have to be brought in.

Note also that this branch leaves `LOG_DEVEL` tracing in `Auth/Rbac/ServiceImpl.cpp` (`[RBAC-TRACE] ...` on every check). Useful while debugging a run, but it is debug output, not something to build on.

## Known product defects

### arangod compresses its authorization requests with an encoding the sidecar cannot read

arangod reaches the external RBAC service through `network::sendRequest`
(`arangod/Auth/Rbac/BackendImpl.cpp`), which applies arangod's **internal**
request compression to a third-party HTTP endpoint that never negotiated it.
When `--network.compression-method` is not `none`, and the authorization request
body compresses well enough to be worth sending compressed, arangod sends it as

```
content-encoding: x-arango-lz4
```

The sidecar cannot decode that proprietary encoding and answers

```
400 {"code":3, "message":"invalid character '\x01' looking for beginning of value"}
```

which arangod maps to `TRI_ERROR_BAD_PARAMETER`, surfacing to the client as
HTTP 500 / `errorNum` 400 `bad parameter`.

* arangod's own default is `network.compression-method = none`, so this is
  **latent in production** and only bites deployments that turn the documented
  option on.
* `etc/testing/arangod-common.conf` sets `compression-method = auto`, so every
  `scripts/unittest` run does turn it on.
* It is intermittent by nature: it needs a body that compresses. Single-resource
  authorization requests (~320 bytes, mostly an incompressible JWT) went through
  uncompressed; the multi-resource request that arangosearch link validation
  emits did not. That is why it showed up as suite `400_views` failing at
  `view.properties({links: ...})` while everything else passed.
* Reproduced end to end by putting a logging HTTP proxy between arangod and the
  sidecar; the request/response pair above is the proxy's transcript.

Because the testing config, not the product default, is what triggers it, the
suite pins `network.compression-method = none` in `serverOptions` under `--rbac`
so the run tests RBAC rather than this defect. Remove that line to reproduce.

## Status

Last full run 2026-09-02, arangod from this checkout plus the sidecar from
`kube-arangodb/bin/linux/amd64/`. Suite filter
`050,100,400,500,580,607,612` throughout — databases; collections, indexes and
documents; views; graphs; analyzers.

| layer | how | result |
|---|---|---|
| offline: self-check, listings, dry runs | `run_all.sh` | **PASS** |
| RBAC (`:8529`, with the service) | `run_all.sh` | **21 steps, 0 mismatches** |
| `role-modelling` (opt-in) | `run_all.sh` | **1 step, 0 mismatches** |
| classic (`:8530`, without the service) | `run_all.sh` | **14 steps, 0 mismatches** |
| RBAC, inside the test harness | `unittest rta_makedata --rbac <url>` | **Success; 19 steps, 0 mismatches** |

The harness path runs 19 of the 21 steps: `reader-permissive-mode` needs the
sidecar started in a different authorization mode, and there the sidecar is
externally managed and must not be restarted.

Denials were proved by four distinct signatures, none of which is the word
"forbidden": `has been denied` (5), `authentication level` (3),
`arangoerror 11:` (2), and one `no-binding` case where the user cannot connect
at all and the verdict comes from the superuser liveness probe described above.
Classic and RBAC word their refusals completely differently, which is why the
runner matches a vocabulary rather than a status code.

All eleven RBAC scenarios and all seven classic scenarios behave as the role
definitions say they should. Notably:

- The documented **coredb-admin** action set, plus
  `db:AdminMonitoringInternal`, is sufficient for every suite in the filter —
  `makedata`, `checkdata` and `cleardata` all pass over databases,
  collections, documents, indexes, views, graphs and analyzers. It is *not* yet
  established for the full resource set: Foxx (`070`/`071`) and users (`700`)
  have not been run.
- **coredb-reader** passes `checkdata` and is refused `makedata` — the
  discriminating result.
- **Scope is genuinely the boundary**: `coredb-admin-out-of-scope` uses the
  identical policy to the passing admin scenario and is denied.
- **Permissive mode** was verified to be doing real work rather than
  accidentally permitting: the sidecar logged 317 `Permissive.Evaluate`
  overrides during that scenario.
- Reading the server version needs an admin action under both models —
  `db:AdminMonitoringInternal` under RBAC, `rw` on `_system` under classic.
  This is intended behaviour, not a defect; see "Reading the server version
  needs an admin action" above.

Caveat: single-server only. Cluster remains untested, and so do Foxx and users.
