# RBAC vs. Classic authorization — API-matrix comparison

`../apitester.js` fires every ArangoDB HTTP endpoint (the 29 `../apitests/*.mjs`
files) as each user in a fixed permission matrix and records the HTTP status
code in a table. This directory reuses that runner to answer:

> Does the external-RBAC path reach the same authorization decisions as the
> classic `_users` permission system?

We capture a **classic baseline**, translate the classic grant matrix into
equivalent **RBAC policies**, run the identical probes against an RBAC server,
and diff.

**Result:** after a faithful translation, **7213 / 7263 status-code cells
(99.3%) are identical**; the remaining **50 cells (0.7%)** are a small set of
genuine classic-vs-RBAC differences plus a few approximations in the hand-built
admin/user mapping (see §4). Captured runs are in `results/`.

---

## Layout

```
tests/api/rbac/
  README.md                 this file
  apitester-rbac.js         patched runner: per-user JWT auth + superuser-JWT setup
  rbac_policies.py          classic effective-level -> RBAC policy/role/binding translator
  diffstat.py               status-code transition tally
  mkjwt.py                  mint superuser / per-user JWTs
  results/
    classic_baseline.txt    reference tables (classic server)
    rbac.txt                RBAC-server tables
  scripts/
    env.sh                  shared paths/ports (override via env vars)
    start_arangod.sh        RBAC arangod (:8529, --server.harden)
    start_arangod_classic.sh classic arangod (:8530)
    start_integration.sh    authorization.v1 PDP [always|never|central|central-permissive]
    start_sidecar.sh        RBAC policy store (writes _system in arangod)
    seed_rbac.sh            demo single-user policy (manual testing)
    setup_all.sh            bring up the whole RBAC stack + demo policy
```

Runtime data (JWT secret, server data dirs, logs) lives outside the source tree
in `$RBAC_WORK` (default `/tmp/arangodb-rbac-test`). Binary locations and ports
are configured in `scripts/env.sh` and overridable via environment variables
(`ARANGOD`, `OPERATOR`, `OPERATOR_INT`, `RBAC_WORK`, ...).

Prerequisites: a built `arangod`, the built `arangodb_operator` +
`arangodb_operator_integration` (kube-arangodb), Node.js >= 18, and `python3`.

---

## Reproduce

```bash
cd tests/api
npm install                         # once: undici
R=rbac/scripts

# A. CLASSIC BASELINE
bash $R/start_arangod_classic.sh    # classic arangod :8530
SEC=${RBAC_WORK:-/tmp/arangodb-rbac-test}/jwt/-
node apitester.js -e http://127.0.0.1:8530 -p '' setup
node apitester.js -e http://127.0.0.1:8530 -j $SEC test apitests/ > rbac/results/classic_baseline.txt

# B. RBAC STACK  (arangod:8529 + sidecar:8108 + integration central:9192)
bash $R/setup_all.sh

# C. RBAC RUN
export RBAC_JWT=1 RBAC_JWT_SECRET_FILE=$SEC
node rbac/apitester-rbac.js -e http://127.0.0.1:8529 -j $SEC teardown
node rbac/apitester-rbac.js -e http://127.0.0.1:8529 -j $SEC setup   # users + fixtures + classic grants (superuser JWT)
RBAC_JWT_SECRET_FILE=$SEC python3 rbac/rbac_policies.py              # translate -> sidecar policies
bash $R/start_integration.sh central ; sleep 12                     # let the central cache sync
node rbac/apitester-rbac.js -e http://127.0.0.1:8529 -j $SEC test apitests/ > rbac/results/rbac.txt

# D. COMPARE
diff rbac/results/classic_baseline.txt rbac/results/rbac.txt
python3 rbac/diffstat.py rbac/results/classic_baseline.txt rbac/results/rbac.txt --by-endpoint
```

---

## How classic config is translated to RBAC

The matrix has 68 users: 64 `<DB><WC><COLL>` (each ∈ `U/N/R/W` =
undefined/none/ro/rw; `DB`=grant on database `d`, `WC`=wildcard `d/*`,
`COLL`=specific `d/c`+`d/e`) and 4 admin users `AU/AN/AR/AW` with a `_system`
grant. RBAC ignores classic grants, so we rebuild them as sidecar policies.

**1. Auth: Basic → per-user JWT.** The stock runner uses Basic auth
(`user:user`). Under RBAC arangod forwards `req.jwtToken()`, which is *empty for
Basic auth* → the PDP returns "400 bad parameter" → everything denied.
`apitester-rbac.js` therefore mints a per-user JWT
(`{"iss":"arangodb","preferred_username":"<user>"}`) for every matrix request
(`RBAC_JWT=1`); setup/teardown use a superuser JWT (`RBAC_JWT_SECRET_FILE`).

**2. Grants: effective level → concrete resource** (`rbac_policies.py`). For each
user we read the *effective* level arangod itself resolves
(`GET /_api/user/<u>/database/{d, d/c, d/e, d/<probe>, _system}` — this API
applies classic precedence *specific > wildcard > db-level* and the
`_system`→other-database cascade), then grant matching RBAC actions on the
concrete resources arangod checks:

- level→actions: `rw → [db:Read,WriteData,WriteMeta,Create,Drop,UseApiVersion]`,
  `ro → [db:Read,UseApiVersion]`, `none/undefined → implicit deny`.
- resources: `db:database:d`; `db:collection:d:*` + `db:graph:d:*` +
  `db:view:d:*` + `db:analyzer:d:*` (wildcard level); `db:collection:d:c` / `:e`;
  `db:database:_system` + `db:collection:_system:*`.
- admin users: `_system rw` → all `db:Admin*` on `*`; `_system ro` → read-ish
  admin actions.
- one policy + role + binding (allow-all scope) per user.

**3. Three model differences we had to encode** (took the diff from ~14% to 0.7%):

1. **No specific>wildcard precedence in RBAC** (any-Allow-wins): emit an explicit
   **Deny** on `db:collection:d:c`/`:e` for actions the wildcard grants but the
   specific level must not have.
2. **Graph/view/analyzer are separate RBAC resource types** (classic derives them
   from collections): grant them at the wildcard level.
3. **`_system` cascades to all databases in classic**: compute `d/c/e` grants for
   admin users too (`eff_level()` already reflects the cascade).

---

## Differences that remain (50 cells / 0.7%)

### A. Genuine RBAC-vs-classic divergences (candidate findings)
- **Collection-metadata & index ops accept collection-level rw where classic
  required database-level rw.** `PUT .../collection/c/properties` and
  `POST .../index?collection=c`, DB=ro + COLL=rw: classic **403**, RBAC
  **200/201** (10 cells). RBAC is *more permissive* here — confirm intent.
- **Listing users is unimplemented under RBAC**: `GET /_api/user` → **501**
  (matches the deliberate fail-closed `AdminReadUsers` stub in `AuthMode::Rbac`).

### B. Approximations in our admin/user mapping (not arangod issues)
- **User-management endpoints** (`/_api/user/testuser/...`, col AW): classic
  200/201/202 → RBAC 403 (~20 cells). These check the `db:user:<name>` resource,
  which the translator does not grant to admin users. Granting `db:user:*` would
  close most; left as a known gap (user-admin RBAC was out of scope).
- **Admin read/write split** (col AR = `_system ro`): a few `/_admin/log*`,
  `support-info`, `license`, `agency-cache`, `async-registry` cells flip 403↔200
  because the `ADMIN_RO` guess doesn't exactly match classic's ro/rw boundary.

### C. Noise
- `POST /_open/auth/renew` 404→200 and a couple analyzer/database create edge
  cases — not authorization-decision differences.

---

## Two blocking arangod bugs this work uncovered

1. **`resolveDestination` rejected `http://` → RBAC entirely non-functional.**
   `--server.external-rbac-service` requires `http(s)://`, but
   `network::sendRequest → resolveDestination` only accepted
   `tcp://`/`ssl://`/`http+tcp://`/`http+ssl://`/`server:`/`shard:`; a plain
   `http://` returned `TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE` (1478) → every
   check failed → every request denied. The unit/Smocker tests missed it because
   they bypass `resolveDestination` via `pool.leaseConnection()`. Fixed in
   `arangod/Network/Utils.cpp`.
2. **RBAC without `--server.harden=true` crashes arangod** —
   `ExecContext.h:148` `ADB_PROD_ASSERT(!isRbac() || _isRestApiHardened)` fires
   on the first authenticated hardened-endpoint hit (e.g. `/_api/version`, which
   arangosh calls on connect) → SIGABRT, release builds included. Recommend a
   startup validation instead of a per-request abort.
