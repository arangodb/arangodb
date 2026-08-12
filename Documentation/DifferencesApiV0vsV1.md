# Behavioural Differences Between REST API Version 0 and Version 1

This document is a comprehensive overview of all behavioural differences
between REST API version 0 (`V0`, the classic/default API, addressed either
without a version prefix or via `/_arango/v0/...`) and REST API version 1
(`V1`, addressed via `/_arango/v1/...`) that currently exist in the ArangoDB
code base.

It was compiled by searching the whole code base for calls to
`GeneralRequest::requestedApiVersion()`, which is the (sole) method used
throughout the server to distinguish which API version a request was made
with, and by inspecting the route registration table in
`arangod/GeneralServer/GeneralServerFeature.cpp`, which determines which
endpoints exist at all under a given API version.

> **Status note:** As of this writing, `lib/Rest/ApiVersion.h` only lists
> `{0}` in `supportedApiVersions`. API version 1 is not yet an officially
> supported/released version; it can currently only be reached in builds
> compiled with `ARANGODB_ENABLE_FAILURE_TESTS` by activating the failure
> point `ApiVersion::treatVersion1AsSupported` (see
> `lib/Rest/GeneralRequest.cpp` and `arangod/GeneralServer/RestHandlerFactory.cpp`).
> Everything described below reflects the behaviour that is implemented in
> the code for when V1 *is* enabled/reachable.

## How the API version is determined

* A client selects an API version by prefixing the request path with
  `/_arango/vX/...` (e.g. `/_arango/v1/_api/version`). If no such prefix is
  present, `api_version::defaultApiVersion` (= 0) is used
  (`lib/Rest/GeneralRequest.cpp`, `GeneralRequest::detectAndStripApiVersion`).
* A special prefix `/_arango/experimental/...` selects the "experimental"
  API version (`api_version::experimentalApiVersion`, currently value `2`),
  which is used for the OpenAPI spec only (see below) and is otherwise out
  of scope for this document.
* If the requested version number is not in `supportedApiVersions` (and is
  not the experimental version), the version prefix is **not** stripped and
  **not** stored; this causes routing to fail and the server to answer with
  HTTP `404` and `errorNum` `404`, with an error message
  `unknown API version <n> for path '<path>'`
  (`arangod/GeneralServer/RestHandlerFactory.cpp`).
* Leading zeros in the version number (e.g. `/_arango/v01/`) are rejected
  with HTTP `400` (`TRI_ERROR_HTTP_BAD_PARAMETER`).

## Endpoints that exist only under V0 (removed/not registered under V1)

The route table in `GeneralServerFeature::defineRemainingHandlers` /
`defineInitialHandlers` registers most handlers for API versions `{0, 1}`,
but a number of handlers are registered **only** for version `{0}`. Under
V1 these paths are simply unknown routes and produce a `404 Not Found`
(`unknown API version` only applies to unsupported version numbers; for a
route that is not registered under an otherwise-valid version, the normal
"not found" handling in `RestHandlerFactory::createHandler` applies, i.e.
HTTP `404`, `errorNum` `TRI_ERROR_HTTP_NOT_FOUND` (404) with message
`unknown path '<path>'`).

The following endpoints are V0-only:

| Path | Handler | Notes |
|---|---|---|
| `/_api/simple/all` | `RestSimpleQueryHandler` | deprecated "simple queries" API |
| `/_api/simple/all-keys` | `RestSimpleQueryHandler` | deprecated "simple queries" API |
| `/_api/simple/by-example` | `RestSimpleQueryHandler` | deprecated "simple queries" API |
| `/_api/simple/lookup-by-keys` | `RestSimpleHandler` | deprecated "simple queries" API |
| `/_api/simple/remove-by-keys` | `RestSimpleHandler` | deprecated "simple queries" API |
| `/_api/tasks` | `RestTasksHandler` | only registered at all if V8/JavaScript is enabled |
| `/_api/aqlfunction` | `RestAqlUserFunctionsHandler` | user-defined AQL functions (V8-dependent) |
| `/_admin/execute` | `RestAdminExecuteHandler` | only registered at all if V8/JavaScript is enabled |
| `/_admin/database/target-version` | `RestAdminDatabaseHandler` | |
| `/_admin/routing` | `RestAdminRoutingHandler` | only registered at all if V8/JavaScript is enabled |
| `/_admin/statistics` | `RestAdminStatisticsHandler` | |
| `/_admin/statistics-description` | `RestAdminStatisticsHandler` | |
| `/` (catch-all prefix) | `RestActionHandler` | generic/legacy action dispatch |

All other REST endpoints (the large majority) are registered for both `{0,
1}` and behave identically at the routing level; any behavioural
differences for those come from explicit `requestedApiVersion()` checks in
the handler code, documented below.

`/openapi.json` is special: it is registered for `{0, 1, 2}` and serves a
different, version-specific static OpenAPI specification document
depending on the requested API version (`kOpenApiV0`/`kOpenApiV1`/`kOpenApiV2`,
compiled in from `openapi-v0.csx`/`openapi-v1.csx`/`openapi-v2.csx`) — see
`arangod/RestHandler/RestOpenApiHandler.cpp`. If a version has no compiled-in
spec, the handler answers with HTTP `404` / `TRI_ERROR_HTTP_NOT_FOUND`.

## `GET /_api/version` and `GET /_admin/version`

`RestVersionHandler` (`arangod/RestHandler/RestVersionHandler.cpp`) includes
a `requestedApiVersion` field in its JSON response (e.g. `"v0"` or `"v1"`),
reflecting whichever API version was used to call it. This is purely
informational and not a behavioural difference in status codes, but it does
mean the **response body shape differs** in that this field always mirrors
the caller's chosen version.

## Authentication/authorization error shape: `RestHandler::checkUserCanAccess` / `handleAuthorizationChecks`

This is one of the most consequential differences, since it affects **every**
authenticated REST call, regardless of endpoint (`arangod/GeneralServer/RestHandler.cpp`).

* `checkUserCanAccess()`:
  * If the user is not authenticated at all, this always fails with
    `TRI_ERROR_HTTP_UNAUTHORIZED` (401), independent of API version.
  * If the user is authenticated but has no read access to the database
    (`ec->canUseDatabase(...)` fails):
    * **V0** (and only when the execution context is in "classic"
      authorization mode, `ec->isClassic()`): the failure is always
      normalized to `TRI_ERROR_HTTP_UNAUTHORIZED` ("No read access to
      database."), i.e. HTTP `401`, no matter what the underlying access
      check actually returned.
    * **V1** (or non-classic/RBAC mode): the original `Result` from
      `canUseDatabase()` is passed through unmodified. Depending on RBAC
      policy this can be `TRI_ERROR_FORBIDDEN` (403) or, if the user has
      absolutely no access to the database, `TRI_ERROR_ARANGO_DATABASE_NOT_FOUND`
      (404, see `AuthMode.cpp` below) rather than always 401.

* `handleAuthorizationChecks()`, once `checkUserCanAccess()` has failed:
  * **V0**: the code contains an explicit, commented "special" branch:
    regardless of what the actual error was, the response is *always*
    generated as HTTP status `401 UNAUTHORIZED` combined with `errorNum`
    `TRI_ERROR_FORBIDDEN` (`11`), using the underlying error's message text.
    This exact combination (401 status + errorNum 11) is preserved
    deliberately for backwards compatibility and cannot be produced by the
    generic `generateError(Result)` overload.
  * **V1**: `generateError(res)` is called, which derives the HTTP status
    code from the actual error via `GeneralResponse::responseCode()`. This
    means the HTTP status and errorNum now vary correctly with the actual
    cause, e.g.:
    * not authenticated → HTTP 401 / errorNum 401
    * forbidden (RBAC denies access, resource exists) → HTTP 403 / errorNum 11 (`TRI_ERROR_FORBIDDEN`)
    * resource intentionally hidden (RBAC, resource does not exist for this user) → HTTP 404 / errorNum 1203 or 1228 (see below)

## RBAC-specific "hide existence of resource" behaviour (`arangod/Auth/AuthMode.cpp`)

`AuthMode.cpp` implements permission checks. Several of the checks return
different results (and hence different HTTP codes) between V0 and V1
specifically to avoid leaking the existence of databases/collections/views
that a user is not allowed to see at all (an information-disclosure
hardening that is introduced with V1's RBAC semantics):

* **`UseDatabase` check** (line ~385): if the requested access level exceeds
  what the user has, and the user has *no* access at all
  (`effectiveLevel == auth::Level::NONE`):
  * **V1**: returns `TRI_ERROR_ARANGO_DATABASE_NOT_FOUND` (HTTP 404,
    errorNum 1228) instead of revealing that the database exists but access
    is forbidden.
  * **V0**: always returns `TRI_ERROR_FORBIDDEN` (HTTP 403, errorNum 11),
    with a descriptive access-level-mismatch message — the database's
    existence is implicitly confirmed.

* **`UseCollection` check** (line ~449): analogous logic for collections.
  If access is insufficient and the user has no access at all:
  * **V1**: returns `TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND` (HTTP 404,
    errorNum 1203) — with message `"collection not found"` in cluster mode,
    or no extra message in single-server mode.
  * **V0**: falls through to the generic mismatch handling, returning either
    `TRI_ERROR_ARANGO_READ_ONLY` (HTTP 403, errorNum 1004, when RW was
    requested but only RO is held) or `TRI_ERROR_FORBIDDEN` (HTTP 403,
    errorNum 11) otherwise — the collection's existence is implicitly
    confirmed in both cases.

* **`UseView` check** (line ~579): identical pattern to `UseDatabase` for
  views: **V1** returns `TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND` (404) when
  the user has no database access at all; **V0** returns `TRI_ERROR_FORBIDDEN`
  (403).

* **`DropCollection` check** (line ~640, two occurrences): when the
  underlying `UseDatabase`/`UseCollection` check fails, the code explicitly
  overrides the errorNum for **V0** to always be `TRI_ERROR_FORBIDDEN` (403,
  errorNum 11), even in cases where the underlying result was
  `TRI_ERROR_ARANGO_READ_ONLY` (1004). This is called out in the code
  comments as required "for API compatibility" with V0. **V1** passes the
  underlying `Result` through unchanged, so callers may see
  `TRI_ERROR_ARANGO_READ_ONLY` (403, errorNum 1004) instead of the generic
  forbidden error (403, errorNum 11) — same HTTP status, different errorNum.

* **`CreateGraph` check** (line ~814): if the user lacks write access to the
  database needed to create the graph's `_graphs` entry:
  * **V1**: returns `TRI_ERROR_FORBIDDEN` (HTTP 403, errorNum 11).
  * **V0**: returns `TRI_ERROR_ARANGO_READ_ONLY` (HTTP 403, errorNum 1004)
    instead — same HTTP status, different errorNum, preserved for backwards
    compatibility.

## `RestIndexHandler` (`arangod/RestHandler/RestIndexHandler.cpp`)

* **Collection lookup access restriction** (`RestIndexHandler::collection`,
  line ~231): when running on a coordinator, starting with **V1**, looking
  up a collection by name additionally enforces a `Read` access check via
  `ExecContext::current().canUseCollection(...)`; if this fails, the
  collection is treated as not found (`nullptr`), which downstream
  typically produces a `404`/`TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND`
  response. Under **V0**, this extra access restriction is skipped
  entirely — any authenticated user reaching this code path can look up the
  collection regardless of their per-collection read access, relying only
  on the database-level check performed earlier.

* **`syncCaches` (`POST /_api/index/sync-caches`)** (line ~1000): this
  unofficial/internal endpoint, when running on a coordinator:
  * **V1**: returns HTTP `501 Not Implemented` /
    `TRI_ERROR_NOT_IMPLEMENTED` with message `"Not implemented on
    coordinators!"`.
  * **V0**: falls through and executes `engine.syncIndexCaches()` on the
    coordinator's local storage engine (which is generally a no-op there),
    then returns a normal `200 OK`. So V0 silently "succeeds" without
    doing anything useful on a coordinator, while V1 makes the
    unsupported-on-coordinator nature explicit via an error.

## `RestQueryCacheHandler` (`arangod/RestHandler/RestQueryCacheHandler.cpp`)

Both `DELETE /_api/query-cache` (`clearCache`) and
`PUT /_api/query-cache/properties` (`replaceProperties`) have an additional
authorization check that only applies starting with **V1**:

* **V0**: the only requirement (introduced in 3.12.10) is that the request
  targets the `_system` database — no admin permission is required to clear
  the query cache or change its properties (beyond the general "must be
  authenticated on `_system`" implicit requirement). If not on `_system`,
  it fails with HTTP 403 / `TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE`.
* **V1**: in addition to the `_system`-database requirement, the caller
  must also pass `ExecContext::current().canUseAdminAction(auth::perms::AdminQueryCache{})`.
  If this RBAC check fails, the request is rejected with whatever `Result`
  that check returns (typically HTTP 403 / `TRI_ERROR_FORBIDDEN`), which is
  an additional way for V1 requests to be rejected compared to V0.

## `RestCollectionHandler` — `PUT /_api/collection/<name>/compact` (`arangod/RestHandler/RestCollectionHandler.cpp`, line ~485)

* **V0**: no explicit per-collection permission check is performed before
  compacting a collection (only whatever generic access control applies
  earlier in the request pipeline).
* **V1**: an explicit check for `WriteMeta` access to the collection
  (`ExecContext::current().canUseCollection(..., AccessLevel::WriteMeta)`)
  is enforced; on failure, the request is rejected with the resulting
  `Result` (typically HTTP 403 / `TRI_ERROR_FORBIDDEN`), whereas under V0
  the same request would proceed to actually compact the collection.

## `RestAdminClusterHandler` — `POST /_admin/cluster/removeServer` (`arangod/RestHandler/RestAdminClusterHandler.cpp`, line ~664)

* **V1**: if the request is not being handled on a coordinator, it is
  rejected immediately with HTTP `403 Forbidden` /
  `TRI_ERROR_HTTP_FORBIDDEN`, message `"only allowed on coordinators"`.
* **V0**: this coordinator-only restriction is not enforced at all; the
  request proceeds to attempt removing the server regardless of the
  current server's role.

## Summary table of HTTP status / errorNum differences

| Area | V0 behaviour | V1 behaviour |
|---|---|---|
| Generic authorization failure (`RestHandler::handleAuthorizationChecks`) | Always HTTP 401 + errorNum 11 (`TRI_ERROR_FORBIDDEN`), regardless of actual cause | HTTP/errorNum reflect the actual `Result` (401/403/404 as appropriate) |
| No DB access at all (`AuthMode::UseDatabase`) | HTTP 403 / errorNum 11 | HTTP 404 / errorNum 1228 (`DATABASE_NOT_FOUND`) — hides existence |
| No collection access at all (`AuthMode::UseCollection`) | HTTP 403 / errorNum 1004 or 11 | HTTP 404 / errorNum 1203 (`DATA_SOURCE_NOT_FOUND`) — hides existence |
| No view/db access at all (`AuthMode::UseView`) | HTTP 403 / errorNum 11 | HTTP 404 / errorNum 1203 — hides existence |
| Drop collection, insufficient DB/collection access (`AuthMode::DropCollection`) | HTTP 403 / errorNum 11 (forced) | HTTP 403 / errorNum 11 or 1004, depending on actual cause |
| Create graph, no DB write access (`AuthMode::CreateGraph`) | HTTP 403 / errorNum 1004 | HTTP 403 / errorNum 11 |
| `/_api/index` collection lookup on coordinator | No per-collection read check | Requires `Read` access; otherwise collection "not found" |
| `POST /_api/index/sync-caches` on coordinator | 200 OK (no-op) | 501 / `TRI_ERROR_NOT_IMPLEMENTED` |
| `/_api/query-cache` clear/replace-properties | Requires only `_system` DB | Also requires `AdminQueryCache` admin permission |
| `PUT /_api/collection/<name>/compact` | No explicit permission check | Requires `WriteMeta` on the collection |
| `POST /_admin/cluster/removeServer` off-coordinator | Proceeds anyway | HTTP 403 / `TRI_ERROR_HTTP_FORBIDDEN` |
| Simple-queries API, tasks, user AQL functions, several `/_admin/*` endpoints | Available | Route removed entirely (HTTP 404) |
| `/openapi.json` | Serves `openapi-v0.csx` | Serves `openapi-v1.csx` |
| `/_api/version`, `/_admin/version` response body | `requestedApiVersion: "v0"` | `requestedApiVersion: "v1"` |
| Unsupported/unknown version prefix in URL | n/a | HTTP 404 / errorNum 404, `"unknown API version"` |

## Source locations referenced

* `lib/Rest/ApiVersion.h`
* `lib/Rest/GeneralRequest.cpp` / `lib/Rest/GeneralRequest.h`
* `lib/Rest/GeneralResponse.cpp` (error-to-HTTP-status mapping)
* `arangod/GeneralServer/RestHandlerFactory.cpp` / `.h`
* `arangod/GeneralServer/GeneralServerFeature.cpp` (route registration table)
* `arangod/GeneralServer/RestHandler.cpp`
* `arangod/GeneralServer/CommTask.cpp` (logging only, no behavioural difference)
* `arangod/RestHandler/RestVersionHandler.cpp` / `.h`
* `arangod/RestHandler/RestOpenApiHandler.cpp`
* `arangod/RestHandler/RestAdminClusterHandler.cpp`
* `arangod/RestHandler/RestIndexHandler.cpp`
* `arangod/RestHandler/RestQueryCacheHandler.cpp`
* `arangod/RestHandler/RestCollectionHandler.cpp`
* `arangod/Auth/AuthMode.cpp`
