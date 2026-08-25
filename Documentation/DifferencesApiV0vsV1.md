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
  **not** stored: the request keeps its default API version and the unstripped
  prefix stays part of the path. No route matches such a path, so the client
  receives HTTP `404` / `TRI_ERROR_HTTP_NOT_FOUND` with the message
  `unknown path '<path>'`. (Under V0 the catch-all `/` handler answers instead,
  normally with the same `404`.) Note that
  `RestHandlerFactory::createHandler` also contains an
  `unknown API version <n> for path '<path>'` error, but it cannot be reached:
  `GeneralRequest::detectAndStripApiVersion` never stores an unsupported
  version, so the value the factory sees is always valid.
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
| `/_admin/version` | `RestVersionHandler` | use `/_api/version` |
| `/_admin/job` | `RestJobHandler` | use `/_api/job` |
| `/_api/endpoint` | `RestEndpointHandler` | deprecated since 3.4 |
| `/_api/upload` | `RestUploadHandler` | Foxx-related |
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

## Authentication/authorization error shape: `handleAuthorizationChecks`

This is one of the most consequential differences, since it affects **every**
authenticated REST call, regardless of endpoint (`arangod/GeneralServer/RestHandler.cpp`).

The RestHandler has three methods that guard access. They all have a default 
implementation but each RestHandler can override each one of these:
* `checkUserAuthentication()`: check that user authentication is required that 
  the request is authenticated
* `checkApiVersionAccess()`: check that user is allowed to access the requested 
  api version
* `checkDatabaseAccess()`: check that user can read the requested database

All these methods are called in `RestHandler::handleAuthorizationChecks()`:

* first checks `checkUserAuthentication()` which can grant access early
  without checking the subsequent checks or deny access with `UNAUTHORIZED`.
* then executes `checkApiVersionAccess()` which can fail early with an
  error coming from the permission system
* then `checkDatabaseAccess()` which can fail with an error which is different
  per api version:
  * **V0**: in case of an error it always gives `UNAUTHORIZED` to preserve
    backwards compatibility. If used with classic authentication the error
    message is always "No read access to database.".
  * **V1**: in case of an error it returns any error code returned by the
    permission system

## Permission-check differences (`arangod/Auth/AuthMode.cpp`)

`AuthMode.cpp` implements permission checks. Several of the checks in
`AuthMode::Classic` return different results (and hence different HTTP codes)
between V0 and V1, mostly to avoid leaking the existence of
databases/collections/views that a user is not allowed to see at all (an
information-disclosure hardening introduced with V1):

> Note that `AuthMode::Rbac` — used when an external RBAC service is
> configured via `--server.external-rbac-service` — does **not** inspect the
> requested API version at all, so none of the differences in this section
> apply when RBAC is enabled. RBAC answers every denial with
> `TRI_ERROR_FORBIDDEN` (403) regardless of API version.

* **`UseDatabase` check** (line ~365): if the requested access level exceeds
  what the user has, and the user has *no* access at all
  (`effectiveLevel == auth::Level::NONE`):
  * **V1**: returns `TRI_ERROR_ARANGO_DATABASE_NOT_FOUND` (HTTP 404,
    errorNum 1228) instead of revealing that the database exists but access
    is forbidden.
  * **V0**: always returns `TRI_ERROR_FORBIDDEN` (HTTP 403, errorNum 11),
    with a descriptive access-level-mismatch message — the database's
    existence is implicitly confirmed.

* **`UseCollection` check** (line ~429): analogous logic for collections.
  If access is insufficient and the user has no access at all:
  * **V1**: returns `TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND` (HTTP 404,
    errorNum 1203) — with message `"collection not found"` in cluster mode,
    or no extra message in single-server mode.
  * **V0**: falls through to the generic mismatch handling, returning either
    `TRI_ERROR_ARANGO_READ_ONLY` (HTTP 403, errorNum 1004, when RW was
    requested but only RO is held) or `TRI_ERROR_FORBIDDEN` (HTTP 403,
    errorNum 11) otherwise — the collection's existence is implicitly
    confirmed in both cases.

* **`ReadView` check** (line ~558): identical pattern to `UseDatabase` for
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

* **`AdminQueryCache` check** (line ~587): unlike every other admin action,
  which requires RW access to `_system`:
  * **V0**: RO access to the `_system` database is sufficient.
  * **V1**: requires RW access to `_system` (`isAdmin()`), i.e. the same as
    all other admin actions.

* **`CreateGraph` check** (line ~804): if the user lacks write access to the
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

* **`syncCaches` (`POST /_api/index/sync-caches`)** (line ~1020): this
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

`DELETE /_api/query-cache`
* **V0** requires read-access to `_system`
* **V1** requires write-access to `_system`

`PUT /_api/query-cache/properties`
* **V0** accepts any database given; requires read-access to `_system`
* **V1** fails with `TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE` if given database 
  is not `_system`; requires write-access to `_system`

## `RestCollectionHandler` — `PUT /_api/collection/<name>/compact` (`arangod/RestHandler/RestCollectionHandler.cpp`, line ~485)

* **V0**: no explicit per-collection permission check is performed before
  compacting a collection (only whatever generic access control applies
  earlier in the request pipeline).
* **V1**: an explicit check for `WriteMeta` access to the collection
  (`ExecContext::current().canUseCollection(..., AccessLevel::WriteMeta)`)
  is enforced; on failure, the request is rejected with the resulting
  `Result` (typically HTTP 403 / `TRI_ERROR_FORBIDDEN`), whereas under V0
  the same request would proceed to actually compact the collection.

## `RestAdminClusterHandler` — `POST /_admin/cluster/removeServer` (`arangod/RestHandler/RestAdminClusterHandler.cpp`, line ~674)

* **V1**: if the request is not being handled on a coordinator, it is
  rejected immediately with HTTP `403 Forbidden` /
  `TRI_ERROR_HTTP_FORBIDDEN`, message `"only allowed on coordinators"`.
* **V0**: this coordinator-only restriction is not enforced at all; the
  request proceeds to attempt removing the server regardless of the
  current server's role.

## Endpoints and HTTP methods rejected under V1

These routes are registered for both versions, but the handler itself refuses
the request under V1.

* **`GET /_admin/log` and `DELETE /_admin/log` without a suffix**
  (`RestAdminLogHandler.cpp`, lines ~107 and ~127):
  * **V0**: `GET` returns the pre-3.8 log format (attributes split into
    parallel arrays); `DELETE` clears the log.
  * **V1**: HTTP `410 Gone` / `TRI_ERROR_HTTP_GONE`, pointing at
    `/_admin/log/entries`, which returns the same data as `total` plus a
    `messages` array.

* **`POST /_api/transaction` without a suffix** (legacy JavaScript
  transactions, `RestTransactionHandler.cpp`, line ~117):
  * **V0**: executes the JS transaction (V8 required).
  * **V1**: HTTP `404` / `TRI_ERROR_HTTP_NOT_FOUND`, "JavaScript transactions
    are no longer supported. Use streaming transactions
    (POST /_api/transaction/begin)".

* **`PUT /_api/cursor/<cursor-id>`** (`RestCursorHandler.cpp`, line ~99):
  * **V0**: `modifyQueryCursor` — fetches the next batch.
  * **V1**: the method is not handled, so the request ends in HTTP `405`
    `TRI_ERROR_HTTP_METHOD_NOT_ALLOWED`. Use
    `POST /_api/cursor/<cursor-id>` instead.

* **`PUT /_api/collection/<name>/load` and `/unload`**
  (`RestCollectionHandler.cpp`, lines ~460 and ~469):
  * **V0**: both are no-ops (since 3.9) returning `200` with a collection
    representation; `unload` optionally flushes the WAL.
  * **V1**: the sub-command is not recognised, so the request falls through
    to the "expecting one of the actions …" error, which is
    `TRI_ERROR_HTTP_NOT_FOUND` — HTTP `404`.

* **`GET /_api/wal/open-transactions`** (`RestWalAccessHandler.cpp`, line
  ~240):
  * **V0**: served.
  * **V1**: falls through to the usage error, HTTP `400`. Note the usage
    message itself is version-dependent: `open-transactions` is only listed
    under V0.

* **`GET /_admin/cluster/nodeStatistics` and `/_admin/cluster/statistics`**
  (`RestAdminClusterHandler.cpp`, lines ~425 and ~428):
  * **V0**: proxied to `/_admin/statistics` on the requested server.
  * **V1**: the command is not recognised → HTTP `400`. (Consistent with
    `/_admin/statistics` itself being V0-only.)

* **Non-`GET` requests to `/_api/version`, `/_admin/time` and
  `/_admin/support-info`** (`RestVersionHandler.cpp` line ~165,
  `RestTimeHandler.cpp` line ~41, `RestSupportInfoHandler.cpp` line ~44):
  * **V0**: the handler answers regardless of the HTTP method.
  * **V1**: only `GET` is accepted; anything else yields HTTP `405` /
    `TRI_ERROR_HTTP_METHOD_NOT_ALLOWED`.

## Request parameters and body attributes rejected under V1

* **`POST /_api/document` — the `collection` query parameter**
  (`RestDocumentHandler.cpp`, line ~165): under **V1** the collection must be
  given as a path suffix (`POST /_api/document/<collection>`); omitting it
  fails with HTTP `400` /
  `TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING`. **V0** still accepts
  `?collection=<name>`.

* **Numeric collection IDs in the path** (`RestHandler::rejectNumericCollectionId`):
  under **V1** a path segment consisting only of digits is rejected with HTTP
  `400`, "Numeric collection IDs are not allowed; please use the collection
  name instead". **V0** resolves them. Enforced at these entry points:
  * `RestDocumentHandler.cpp`: `insertDocument`, `readSingleDocument` (also
    covers `HEAD`), `modifyDocument` (`PUT`/`PATCH`), `readManyDocuments`
    (`PUT ...?onlyget`) and `removeDocument`.
  * `RestCollectionHandler.cpp`: `handleCommandGet`, `handleCommandPut` and
    `handleCommandDelete`.

  Together these cover the surface deprecated in 3.4, which named exactly
  `/_api/collection/<collection-id>`, `/_api/document/<collection-id>` and
  `/_api/simple`. The third needs no check: the whole simple-queries API is
  V0-only (see the route table above), so it cannot be reached under V1 at
  all.

* **The `overwrite` query parameter on document inserts**
  (`RestDocumentHandler.cpp`, line ~219): under **V1** it is rejected with
  HTTP `400`, "the 'overwrite' option has been removed, use 'overwriteMode'
  instead". Under **V0** it is still honoured and means
  `overwriteMode=replace`.

* **`minReplicationFactor` in collection properties**
  (`RestCollectionHandler.cpp`, line ~640): the deprecated attribute is kept
  in the accepted property set under **V0** only; under **V1** it is dropped
  from the request body before the update is applied.

* **`includeFoxxQueues` on `GET /_api/wal/tail`**
  (`RestWalAccessHandler.cpp`, line ~150): evaluated under **V0** only; under
  **V1** the parameter has no effect.

* **Deprecated index types on `POST /_api/index`**
  (`RestIndexHandler.cpp`, line ~909): under **V1**, creating an index of
  type `geo1`, `geo2`, `hash`, `skiplist` or `fulltext` fails with HTTP `400`
  / `TRI_ERROR_BAD_PARAMETER`, "index type '<type>' is not supported in API
  version 1 or higher". Under **V0** these aliases are still accepted.

## Response attributes omitted under V1

* **`GET /_admin/status`** (`RestStatusHandler.cpp`, lines ~122, ~162 and
  ~205): **V0** additionally reports `mode` and `operationMode` (both the
  server operation mode), `foxxApi`, `serverInfo.writeOpsEnabled`, and — on a
  Coordinator — a `coordinator` object with `foxxmaster` and `isFoxxmaster`.
  None of these are present under **V1**.

* **`GET /_api/version?details=true`** (`RestVersionHandler.cpp`, line ~83):
  `details.mode` is reported under **V0** only.

* **`GET /_api/database/current`** (`arangod/VocBase/vocbase.cpp`,
  `Database::toVelocyPack`): the `path` attribute (the filesystem path of the
  database, or `"none"` outside a Coordinator) is reported under **V0** only.

* **`GET /_api/replication/clusterInventory`**
  (`RestReplicationHandler.cpp` line ~861): serialises the same
  `Database::toVelocyPack`, so its `properties.path` attribute is likewise
  reported under **V0** only. `arangodump` is the only in-tree consumer and
  does not read the attribute.

* **`GET /_admin/cluster/health`** (`RestAdminClusterHandler.cpp`, line
  ~2353): the deprecated per-node `Timestamp` attribute is reported under
  **V0** only. `LastAckedTime` carries the same information in both versions.

* **`GET /_api/engine`** (`RestHandler/RestEngineHandler.cpp` line ~85 →
  `StorageEngine::getCapabilities`): two separate parts of the response drop
  the deprecated types under **V1**:
  * `supports.aliases.indexes` (`IndexFactory::indexAliases`): the aliases
    `hash` → `persistent` and `skiplist` → `persistent` are reported under
    **V0** only; `zkd` → `mdi` is reported under both.
  * `supports.indexes` (`IndexFactory::supportedIndexes`): `hash`, `skiplist`
    and `fulltext` are appended under **V0** only, so they are absent from the
    **V1** list. Array order is not part of the contract.

## Summary table of HTTP status / errorNum differences

| Area | V0 behaviour | V1 behaviour |
|---|---|---|
| Generic authorization failure (`RestHandler::handleAuthorizationChecks`) | Always HTTP 401 + errorNum 11 (`TRI_ERROR_FORBIDDEN`), regardless of actual cause | HTTP/errorNum reflect the actual `Result` (401/403/404 as appropriate) |
| No DB access at all (`AuthMode::UseDatabase`) | HTTP 403 / errorNum 11 | HTTP 404 / errorNum 1228 (`DATABASE_NOT_FOUND`) — hides existence |
| No collection access at all (`AuthMode::UseCollection`) | HTTP 403 / errorNum 1004 or 11 | HTTP 404 / errorNum 1203 (`DATA_SOURCE_NOT_FOUND`) — hides existence |
| No view/db access at all (`AuthMode::ReadView`) | HTTP 403 / errorNum 11 | HTTP 404 / errorNum 1203 — hides existence |
| Drop collection, insufficient DB/collection access (`AuthMode::DropCollection`) | HTTP 403 / errorNum 11 (forced) | HTTP 403 / errorNum 11 or 1004, depending on actual cause |
| Create graph, no DB write access (`AuthMode::CreateGraph`) | HTTP 403 / errorNum 1004 | HTTP 403 / errorNum 11 |
| `/_api/index` collection lookup on coordinator | No per-collection read check | Requires `Read` access; otherwise collection "not found" |
| `POST /_api/index/sync-caches` on coordinator | 200 OK (no-op) | 501 / `TRI_ERROR_NOT_IMPLEMENTED` |
| `/_api/query-cache` clear/replace-properties | `AdminQueryCache` with RO on `_system`; no `_system` DB requirement for `properties` | `AdminQueryCache` with RW on `_system`; `properties` additionally requires the `_system` DB |
| Every other admin action (`AuthMode::AdminQueryCache` aside) | RW on `_system` | unchanged |
| `PUT /_api/collection/<name>/compact` | No explicit permission check | Requires `WriteMeta` on the collection |
| `POST /_admin/cluster/removeServer` off-coordinator | Proceeds anyway | HTTP 403 / `TRI_ERROR_HTTP_FORBIDDEN` |
| Simple-queries API, tasks, user AQL functions, several `/_admin/*` endpoints | Available | Route removed entirely (HTTP 404) |
| `/openapi.json` | Serves `openapi-v0.csx` | Serves `openapi-v1.csx` |
| `/_api/version`, `/_admin/version` response body | `requestedApiVersion: "v0"` | `requestedApiVersion: "v1"` |
| `/_admin/log`, `/_admin/log` DELETE (no suffix) | Old log format / clears log | HTTP 410 `TRI_ERROR_HTTP_GONE` → use `/_admin/log/entries` |
| `POST /_api/transaction` (JS transactions) | Executes the JS transaction | HTTP 404 → use `/_api/transaction/begin` |
| `PUT /_api/cursor/<id>` | Fetches next batch | HTTP 405 → use `POST` |
| `PUT /_api/collection/<name>/load`, `/unload` | 200 (no-op) | HTTP 404 (unknown action) |
| `GET /_api/wal/open-transactions` | Served | HTTP 400 (unknown suffix) |
| `/_admin/cluster/nodeStatistics`, `/_admin/cluster/statistics` | Proxied to `/_admin/statistics` | HTTP 400 (unknown command) |
| Non-GET on `/_api/version`, `/_admin/time`, `/_admin/support-info` | Handled | HTTP 405 |
| `POST /_api/document?collection=<name>` | Accepted | HTTP 400 — collection must be a path suffix |
| Numeric collection ID in path | Resolved | HTTP 400 |
| `overwrite` query parameter on document inserts | Honoured | HTTP 400 → use `overwriteMode` |
| `minReplicationFactor` collection property | Accepted | Ignored (dropped from the body) |
| `includeFoxxQueues` on `GET /_api/wal/tail` | Honoured | Ignored |
| `POST /_api/index` with `geo1`/`geo2`/`hash`/`skiplist`/`fulltext` | Accepted | HTTP 400 `TRI_ERROR_BAD_PARAMETER` |
| `GET /_admin/status` response | Includes `mode`, `operationMode`, `foxxApi`, `serverInfo.writeOpsEnabled`, `coordinator` | Those attributes omitted |
| `GET /_api/version?details=true` response | Includes `details.mode` | Omitted |
| `GET /_api/database/current` response | Includes `path` | Omitted |
| `GET /_api/replication/clusterInventory` response | `properties` includes `path` | Omitted |
| `GET /_admin/cluster/health` response | Includes per-node `Timestamp` | Omitted |
| `GET /_api/engine` response | `supports.indexes` includes `hash`, `skiplist`, `fulltext`; aliases include `hash`, `skiplist` | Those dropped; only the `zkd` → `mdi` alias remains |
| Unsupported/unknown version prefix in URL | n/a | HTTP 404 / `TRI_ERROR_HTTP_NOT_FOUND`, `"unknown path"` |

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
* `arangod/RestHandler/RestAdminLogHandler.cpp`
* `arangod/RestHandler/RestTransactionHandler.cpp`
* `arangod/RestHandler/RestCursorHandler.cpp`
* `arangod/RestHandler/RestDocumentHandler.cpp`
* `arangod/RestHandler/RestWalAccessHandler.cpp`
* `arangod/RestHandler/RestStatusHandler.cpp`
* `arangod/RestHandler/RestEngineHandler.cpp`
* `arangod/RestHandler/{RestTimeHandler,RestSupportInfoHandler}.cpp`
* `arangod/VocBase/vocbase.cpp` (`Database::toVelocyPack`)
* `arangod/Indexes/IndexFactory.cpp`, `arangod/RocksDBEngine/RocksDBIndexFactory.cpp`
* `arangod/Auth/AuthMode.cpp`
