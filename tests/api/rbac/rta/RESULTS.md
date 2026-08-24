# RBAC / rta-makedata test results

| Item | Value |
|---|---|
| Date | 2026-08-25 |
| Branch | `feature/rbac-api-tester-rta` @ `237abff0aae` |
| arangod / arangosh | 3.12.11-devel, built from `refs/feature/rbac-api-tester-rta` `c5a404c41b8` |
| Sidecar | `kube-arangodb/bin/linux/amd64/arangodb_operator`, built 2026-08-24 |
| rta-makedata | `6cea1d3` (unmodified) |
| Suite filter | `050,100,400` (databases; collections, indexes, documents; views) |
| Deployment | single server |

## Summary

| Layer | Configuration | Steps | Mismatches | Result |
|---|---|---:|---:|---|
| offline | none required | 3 checks | 0 | PASS |
| rbac | arangod `:8529` **with** `--server.external-rbac-service` | 21 | 0 | PASS |
| classic | arangod `:8530` **without** it | 14 | 0 | PASS |
| role-modelling | arangod `:8529`, opt-in | 1 | 0 | PASS |
| **Total** | | **39** | **0** | **PASS** |

## RBAC configuration

| Scenario | Phase | As | Expected | Actual | Verdict |
|---|---|---|---|---|---|
| superuser-control | makedata | superuser | pass | pass | ok |
| superuser-control | checkdata | superuser | pass | pass | ok |
| superuser-control | cleardata | superuser | pass | pass | ok |
| coredb-admin-in-scope | makedata | scenario | pass | pass | ok |
| coredb-admin-in-scope | checkdata | scenario | pass | pass | ok |
| coredb-admin-in-scope | cleardata | scenario | pass | pass | ok |
| coredb-reader-in-scope | makedata | superuser | pass | pass | ok |
| coredb-reader-in-scope | checkdata | scenario | pass | pass | ok |
| coredb-reader-in-scope | makedata | scenario | deny | deny | ok |
| coredb-reader-in-scope | cleardata | superuser | pass | pass | ok |
| coredb-developer-in-scope | makedata | superuser | pass | pass | ok |
| coredb-developer-in-scope | checkdata | scenario | pass | pass | ok |
| coredb-developer-in-scope | makedata | scenario | deny | deny | ok |
| coredb-developer-in-scope | cleardata | superuser | pass | pass | ok |
| coredb-admin-out-of-scope | makedata | scenario | deny | deny | ok |
| no-binding | makedata | scenario | deny | deny | ok |
| binding-without-scope | setup | — | rejected | rejected | ok |
| admin-without-api-version | makedata | scenario | deny | deny | ok |
| admin-with-explicit-deny | makedata | scenario | deny | deny | ok |
| reader-permissive-mode | makedata | scenario | pass | pass | ok |
| reader-permissive-mode | cleardata | scenario | pass | pass | ok |

## Classic configuration

| Scenario | Grants | Phase | As | Expected | Actual | Verdict |
|---|---|---|---|---|---|---|
| classic-superuser-control | — | makedata | superuser | pass | pass | ok |
| classic-superuser-control | — | checkdata | superuser | pass | pass | ok |
| classic-superuser-control | — | cleardata | superuser | pass | pass | ok |
| classic-rw | `_system=rw`, `db=rw` | makedata | scenario | pass | pass | ok |
| classic-rw | `_system=rw`, `db=rw` | checkdata | scenario | pass | pass | ok |
| classic-rw | `_system=rw`, `db=rw` | cleardata | scenario | pass | pass | ok |
| classic-ro | `_system=ro`, `db=ro` | makedata | superuser | pass | pass | ok |
| classic-ro | `_system=ro`, `db=ro` | checkdata | scenario | pass | pass | ok |
| classic-ro | `_system=ro`, `db=ro` | makedata | scenario | deny | deny | ok |
| classic-ro | `_system=ro`, `db=ro` | cleardata | superuser | pass | pass | ok |
| classic-none | `_system=ro`, `db=none` | makedata | scenario | deny | deny | ok |
| classic-no-grant | `_system=ro` | makedata | scenario | deny | deny | ok |
| classic-other-database | `_system=ro`, `other=rw` | makedata | scenario | deny | deny | ok |
| classic-collection-denied | `_system=rw`, `db=rw`, `db/cgeo_0=none` | makedata | scenario | deny | deny | ok |

## Role-modelling (opt-in)

| Scenario | Phase | As | Expected | Actual | Verdict |
|---|---|---|---|---|---|
| documented-admin-set-only | makedata | scenario | error | error | ok |

## Offline checks

| Check | Scope | Result |
|---|---|---|
| Catalog self-check | 11 RBAC + 7 classic scenarios validated against `arangod/Auth/Rbac/ServiceImpl.cpp` | PASS |
| Catalog listings | RBAC and classic | PASS |
| Dry runs | RBAC (21 steps) and classic (14 steps) | PASS |

## Denial evidence

| Configuration | Signature that proved the denial | Denials |
|---|---:|---:|
| RBAC | `has been denied` | 5 |
| RBAC | `not connected`, resolved by superuser liveness probe | 1 |
| Classic | `authentication level` | 3 |
| Classic | `arangoerror 11:` | 2 |

## Not covered

| Area | Status |
|---|---|
| Cluster deployment | not run — single server only |
| Hardened classic (`--server.harden`) | not run — `start_arangod_classic.sh` does not harden |
| Suites 070/071 (Foxx) | excluded from the filter |
| Suite 700 (users, `db:user:*`) | excluded — `AdminReadUsers` returns 501 under RBAC |
| Analyzers, graphs (suites 5xx/6xx) | not run |
| Predefined roles bound directly (`managed:predefined:coredb-*`) | not run — empty containers in the MVP |
