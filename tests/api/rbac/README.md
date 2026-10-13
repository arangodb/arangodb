# RBAC vs. Classic authorization — API-matrix comparison

`../apitester.js` fires every ArangoDB HTTP endpoint (the 29 `../apitests/*.mjs`
files) as each user in a fixed permission matrix and records the HTTP status
code in a table. A patched copy of that runner was used here to answer:

> Does the external-RBAC path reach the same authorization decisions as the
> classic `_users` permission system?

A **classic baseline** was captured, the classic grant matrix translated into
equivalent **RBAC policies**, the identical probes run against an RBAC server,
and the two diffed.

**Result:** after a faithful translation, **7213 / 7263 status-code cells
(99.3%) are identical**; the remaining **50 cells (0.7%)** are a small set of
genuine classic-vs-RBAC differences plus a few approximations in the hand-built
admin/user mapping.

---

What *is* maintained lives in `rta/` - the scenario matrix that runs in CI and
under `scripts/unittest rta_makedata --rbac` - and in `scripts/`, which brings
up the stack those use. See `rta/README.md`.

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
