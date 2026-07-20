# Authorization Behaviour Comparison: RBAC branch (`Classic` mode) vs. `devel`

## Task description

This repository implements a new Role-Based-Access-Control (RBAC) system for
authorization. RBAC is **opt-in**: it is only active if it has been
explicitly configured/enabled. When it is switched off, the `AuthMode` member
of `ExecContext` is set to `AuthMode::Classic` (or, when authentication itself
is disabled, `AuthMode::Disabled`), and the authorization behaviour of the
server is supposed to be **byte-for-byte identical** to the behaviour of the
`devel` branch (i.e. the branch this feature branch was forked from), which
has no notion of RBAC or `ExecContext`/`AuthMode` at all.

The purpose of this ongoing investigation is to go through the REST API
`RestHandler` by `RestHandler`, compare the authorization-relevant code paths
of this branch (`feature/cor-213-implement-rbac-authorization`) in `Classic`
mode against the corresponding code in `devel`, and document any behavioural
differences that are found — whether they are outright regressions, subtle
edge cases, or deliberate/acceptable changes introduced by the broader
refactoring that this branch performs (e.g. the move of authorization checks
out of `CommTask` and into `RestHandler`/`ExecContext`, or the new API
versioning scheme).

This file is a living document. Each session covers one (or a small number
of) `RestHandler`(s). New sections are appended as the investigation
progresses. The first `RestHandler` analyzed is `RestDatabaseHandler`.

Methodology used for each handler:

1. Read the current implementation of the handler and any helper code
   it calls into (`methods::Databases`, `ExecContext`, `AuthMode::Classic`,
   ...).
2. Fetch the corresponding code from `devel` (via `git show devel:<path>`)
   and diff it against the current branch, both for the handler itself and
   for all helper/authorization code it depends on. Note: the `enterprise/`
   subdirectory is itself a full git checkout of the closed-source
   Enterprise-Edition repository, with the same branch names
   (`feature/cor-213-implement-rbac-authorization` and `devel`) — so any
   EE-only counterpart of a handler (e.g. `*EE.cpp` files under
   `enterprise/Enterprise/RestHandler/`) can and should be diffed the same
   way via `git -C enterprise show devel:<path>`, rather than treated as
   out of scope.
3. Trace, for every code path/branch of the handler, the exact sequence of
   authorization checks in both branches and compare the resulting
   ALLOW/DENY decisions (and, secondarily, the error codes/messages) for
   all relevant combinations (authenticated/unauthenticated, admin/
   non-admin, read-only mode on/off, superuser, auth disabled, etc).
4. Document any divergence found, classified as:
   - **Regression**: `Classic` mode behaves differently from `devel` in a
     case that can be triggered through normal REST API usage.
   - **Cosmetic**: only the wording of an error message differs, but the
     HTTP status code / ALLOW-DENY decision is unchanged.
   - **New, gated, deliberate**: a genuinely new behaviour was introduced
     that is not part of `devel` at all, but it is orthogonal to the
     RBAC-toggle and only observable through a new opt-in mechanism (e.g.
     the new `/_arango/v1/...` API versioning), so it does not affect the
     classic `/_api/...` routes.
   - **Code-quality / latent risk**: implementation detail that currently
     produces identical behaviour, but is fragile or could diverge under
     circumstances not covered by today's call sites.

The general architectural background needed to understand these findings:

- In `devel`, most authorization for "is the caller allowed to even reach
  this route" is done centrally in `CommTask::canAccessPath()`
  (`arangod/GeneralServer/CommTask.cpp`, `devel:787-876`), which runs before
  a `RestHandler` object is even created. It checks authentication, then
  checks `VocbaseContext::databaseAuthLevel() == NONE`, and finally applies
  a number of hard-coded **path-based exceptions** that allow certain
  routes to be reached without authentication (or with a downgraded
  "read-only"/"superuser" context): `"/"`, `/_open/*`, `/_admin/aardvark/*`,
  `/_admin/server/availability`, `/_api/cluster/endpoints` (if
  authenticated), `/_api/user/<self>/` (POST, to allow password checks),
  `/_api/user/*` (if authenticated), `/_api/token/*` (if authenticated),
  and a bypass for UNIX domain socket connections when
  `--server.authentication-unix-sockets=false`.
- In this branch, `CommTask::canAccessPath()`
  (`arangod/GeneralServer/CommTask.cpp:773-790`) has been reduced to just
  the `allowedPaths()` check from the JWT token; **all the rest of the
  logic was moved** into a new virtual/coroutine method
  `RestHandler::checkUserCanAccess()`
  (`arangod/GeneralServer/RestHandler.cpp:705-764`), which now runs on the
  `RestHandler` (via `handleAuthorizationChecks()`,
  `arangod/GeneralServer/RestHandler.cpp:766-772`, invoked from
  `runHandlerStateMachine()`, `arangod/GeneralServer/RestHandler.cpp:439`).
  Importantly, **not all of the old path-based exceptions were carried
  over** — only the UNIX-socket bypass and the
  `authenticationSystemOnly()`/Foxx-app exception remain; the exceptions
  for `"/"`, `/_open/*`, `/_admin/aardvark/*`,
  `/_admin/server/availability`, `/_api/cluster/endpoints`,
  `/_api/user/*`, and `/_api/token/*` are gone from this generic place.
  (This may or may not matter for those specific routes/handlers — that
  will need to be checked when those handlers are analyzed in a future
  session. It is called out here because it directly explains one of the
  findings below for `RestDatabaseHandler`.)
- `ExecContext`/`AuthMode` (`arangod/Utils/ExecContext.{h,cpp}`,
  `arangod/Auth/AuthMode.{h,cpp}`) replace `devel`'s
  `ExecContext`/`VocbaseContext` (`arangod/RestServer/VocbaseContext.{h,cpp}`
  in `devel`). The `Classic` variant of `AuthMode` is meant to reproduce the
  exact `devel` semantics through the classic `auth::UserManager`.


## `RestDatabaseHandler` (`arangod/RestHandler/RestDatabaseHandler.cpp`)

Routes handled: `GET /_api/database`, `GET /_api/database/user`,
`GET /_api/database/current`, `GET /_api/database/shardStatistics`,
`POST /_api/database`, `DELETE /_api/database/{name}`.

The diff of the handler file itself
(`arangod/RestHandler/RestDatabaseHandler.cpp` vs.
`devel:arangod/RestHandler/RestDatabaseHandler.cpp`) is small; the routing
logic, the `_vocbase.isSystem()` guards for POST/DELETE, and the bodies for
the `current`/`shardStatistics` suffixes are unchanged. Two real changes
were found, discussed below. All other authorization-relevant logic is
delegated to `methods::Databases::{list,create,drop}`
(`arangod/VocBase/Methods/Databases.cpp`) and to `ExecContext`/`AuthMode`.

### Finding 1 (Regression, narrow): missing extra authentication check for `GET /_api/database/user`

`devel` (`devel:arangod/RestHandler/RestDatabaseHandler.cpp:85-89`, as it
was before this branch):

```cpp
} else if (suffixes[0] == "user") {
  if (!_request->authenticated() && ExecContext::isAuthEnabled()) {
    res.reset(TRI_ERROR_FORBIDDEN);
  } else {
    names = methods::Databases::list(server(), _request->user());
  }
}
```

Current branch (`arangod/RestHandler/RestDatabaseHandler.cpp:85-89`):

```cpp
} else if (suffixes[0] == "user") {
  // When we get here, we are either authenticated or authentication
  // is disabled, so no need to check further.
  names = methods::Databases::list(server(), _request->user());
}
```

The comment's assumption ("we are either authenticated or authentication is
disabled") is correct for the overwhelming majority of configurations,
because `RestHandler::checkUserCanAccess()` now runs before `execute()` and
returns `TRI_ERROR_HTTP_UNAUTHORIZED` for any unauthenticated request,
**except** for two hard-coded exceptions (see architectural background
above): the Foxx/`authenticationSystemOnly()` exception (does not apply,
`/_api/...` always starts with `/_`) and the **UNIX domain socket
exception**, which applies when the server is started with
`--server.authentication-unix-sockets=false` (default: `true`, see
`arangod/GeneralServer/AuthenticationOptions.h:33`). In that (non-default,
but real and configurable) case, an unauthenticated request coming in over
a UNIX domain socket reaches `checkUserCanAccess()`
(`arangod/GeneralServer/RestHandler.cpp:728-739`), which sets
`canAccess = true` **without** authenticating the request or upgrading the
`ExecContext` to superuser. So `getDatabases()` is reached with
`_request->authenticated() == false` and `_request->user() == ""`.

Consequently, in this branch `methods::Databases::list(server(), "")` is
called (`arangod/VocBase/Methods/Databases.cpp:81-97`), which — because the
user string is empty — takes the "list *all* databases, completely
unfiltered" branch, exactly like the plain `GET /_api/database` route
(which additionally requires being in the `_system` database; the `/user`
suffix branch does not have that requirement). In other words: for this one
non-default configuration, an unauthenticated request to
`GET /_api/database/user` now returns the full, unfiltered list of *all*
databases on the server (even from a non-`_system` database context),
whereas in `devel` the same request would be rejected with
`TRI_ERROR_FORBIDDEN` due to the now-missing explicit check.

This is a genuine, if narrow, behavioural regression relative to `devel`.
It should either be restored as an explicit check in
`RestDatabaseHandler::getDatabases()`, or (better, from an architectural
point of view) the missing exceptions from `devel`'s
`CommTask::canAccessPath()` should be revisited holistically when the
generic `RestHandler::checkUserCanAccess()` is reviewed, since this handler
is unlikely to be the only place affected by the incomplete migration.

### Finding 2 (Cosmetic): reordering of checks / different error text for `POST /_api/database` and `DELETE /_api/database/{name}`

`devel`'s permission checks were hand-written directly in
`methods::Databases::create`/`drop`:

- `create` (`devel:arangod/VocBase/Methods/Databases.cpp:357-369`):
  ```cpp
  if (!exec.isAdminUser()) {
    return res.reset(TRI_ERROR_FORBIDDEN);                      // no message
  }
  if (ServerState::readOnly() && !exec.isSuperuser()) {
    return res.reset(TRI_ERROR_FORBIDDEN, "server is in read-only mode");
  }
  ```
- `drop` (`devel:arangod/VocBase/Methods/Databases.cpp:503-509`):
  ```cpp
  if (exec.systemAuthLevel() != auth::Level::RW) {
    events::DropDatabase(dbName, Result(TRI_ERROR_FORBIDDEN), exec);
    return TRI_ERROR_FORBIDDEN;                                 // no message
  }
  ```
  Note this uses `systemAuthLevel()` (the runtime-effective, read-only-mode
  capped level), not `isAdminUser()` — so, unlike `create`, `drop` denies
  admins in read-only mode with a generic `FORBIDDEN`, not the
  "server is in read-only mode" message.

Current branch routes both through `ExecContext`:

- `create` (`arangod/VocBase/Methods/Databases.cpp:363`) calls
  `exec.canCreateDatabase(dbName)`
  (`arangod/Utils/ExecContext.cpp:173-179`):
  ```cpp
  Result ExecContext::canCreateDatabase(std::string_view db) const {
    if (!isSuperuser() && ServerState::readOnly()) {
      return {TRI_ERROR_FORBIDDEN, "Server is in read-only mode."};
    }
    return can(CreateDatabase{.name{db}});
  }
  ```
  which, for `Classic` mode, calls
  `AuthMode::Classic::check(CreateDatabase)` →
  `isAdmin()` (`arangod/Auth/AuthMode.cpp:372-375,574-577`), i.e.
  `check(UseDatabase{_system, Write})`, returning
  `"insufficient database access level for '_system'"` on failure.
- `drop` (`arangod/VocBase/Methods/Databases.cpp:503`) calls
  `exec.canDropDatabase(dbName)`
  (`arangod/Utils/ExecContext.cpp:181-187`), structurally identical to
  `canCreateDatabase` (read-only check first, then `isAdmin()` via `can()`).

I traced all combinations of {admin/non-admin} × {read-only mode on/off} ×
{superuser/not} for both `create` and `drop` and found that **the final
ALLOW/DENY decision always matches `devel`** in `Classic` mode. What
differs is:
- **Order of checks**: `devel`'s `create` checks admin-status first, then
  read-only mode; the current branch's `canCreateDatabase`/`canDropDatabase`
  check read-only mode first, then admin-status (via `isAdmin()`). Since a
  read-only-mode deny and a not-admin deny both resolve to
  `TRI_ERROR_FORBIDDEN` either way, the externally visible result (HTTP
  403) is the same, but which of the two possible error messages is
  returned can differ from `devel` when *both* conditions are true (e.g. a
  non-admin user issuing `POST /_api/database` while the server happens to
  be in read-only mode: `devel` reports the (message-less) "not admin"
  reason first, the current branch reports "Server is in read-only mode."
  first).
- **Message text**: `"server is in read-only mode"` (`devel`) vs.
  `"Server is in read-only mode."` (current) — trivial capitalization/
  punctuation difference.
- `drop` in `devel` used the raw `systemAuthLevel()` (i.e. denies admins in
  read-only mode, just like `create`, but via a slightly different code
  path with a different, generic message), while the current branch's
  `canDropDatabase` is structurally identical to `canCreateDatabase`
  (explicit read-only short-circuit + `isAdmin()`). The end result (deny in
  read-only mode unless superuser) is unchanged, but the specific error
  message for `drop` in read-only mode changed from a bare `FORBIDDEN`
  (`devel`) to `"Server is in read-only mode."` (current branch) — arguably
  an improvement, but technically a text change that tests asserting exact
  error messages could pick up on.

None of this affects the actual authorization decision (allow vs. deny) in
`Classic` mode; it is purely a matter of which of two possible `FORBIDDEN`
reasons is surfaced. Flagging it here in case there are integration tests
that assert on exact error message text for these routes.

### Finding 3 (New, gated, deliberate — not a `Classic`-mode regression): "hide as NOT_FOUND" for the new API versioning scheme

`AuthMode::Classic::check()` for `UseDatabase` (used by both
`canCreateDatabase`'s/`canDropDatabase`'s `isAdmin()` helper, and by
`canUseDatabase()` in general) contains this new branch
(`arangod/Auth/AuthMode.cpp:157-175`):

```cpp
[&](p::UseDatabase const& database) -> Result {
  auto const effectiveLevel = effectiveDatabaseAuthLevel(database.name);
  auto const requestedLevel = accessLevelToAuthLevel(database.level);
  if (requestedLevel <= effectiveLevel) {
    return {};
  } else if (_request.requestedApiVersion() > 0 &&
             effectiveLevel == auth::Level::NONE) {
    // User has no access to the database at all: report as not found
    // to avoid revealing its existence.
    return {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND, ...};
  } else {
    return {TRI_ERROR_FORBIDDEN, ...};
  }
}
```

This has no equivalent at all in `devel` — `devel` always denies with
`TRI_ERROR_FORBIDDEN` in this situation. However, `requestedApiVersion()`
(`lib/Rest/GeneralRequest.h:214`, populated only by
`GeneralRequest::detectAndStripApiVersion`,
`lib/Rest/GeneralRequest.cpp:408-489`) defaults to `0` and is only set to a
non-zero value for requests made through the brand-new
`/_arango/v<N>/...` (or `/_arango/experimental/...`) API-versioning
prefix — a feature that does not exist in `devel` at all and is unrelated
to the RBAC on/off toggle. For the classic, non-prefixed routes
(`/_api/database`, `/_api/database/{name}`, ...), `requestedApiVersion()`
is always `0`, so this branch never triggers and behaviour for those routes
remains identical to `devel`. This is included here purely for completeness
/ awareness, not as a `Classic`-mode regression for `RestDatabaseHandler`'s
primary routes.

### Finding 4 (Code-quality / latent risk, not observably different today): `getDatabaseNamesForUser` ignores its explicit `username` parameter

`GET /_api/database/user` (when `_request->user()` is non-empty) calls
`methods::Databases::list(server(), _request->user())` →
`DatabaseFeature::getDatabaseNamesForUser(username)`.

`devel` (`devel:arangod/RestServer/DatabaseFeature.cpp:1002-1032`) uses the
passed-in `username` argument directly:

```cpp
AuthenticationFeature* af = AuthenticationFeature::instance();
...
if (af->isActive() && af->userManager() != nullptr) {
  auto level = af->userManager()->databaseAuthLevel(username, vocbase->name(), false);
  if (level == auth::Level::NONE) { continue; }   // hide dbs without access
}
names.emplace_back(vocbase->name());
```

Current branch (`arangod/RestServer/DatabaseFeature.cpp:911-937`) instead
completely ignores the `username` parameter and consults the thread-local
`ExecContext::current()`:

```cpp
std::vector<std::string> DatabaseFeature::getDatabaseNamesForUser(
    std::string const& username) {
  ...
  auto& exec = ExecContext::current();
  ...
  if (exec.canSeeDatabase(vocbase->name()).fail()) {
    continue;
  }
  names.emplace_back(vocbase->name());
  ...
}
```

Because `RestHandler::executeEngine()`
(`arangod/GeneralServer/RestHandler.cpp:531-534`) installs an
`ExecContextScope` around the request's own `ExecContext`
(`_request->requestContext()`) before `execute()` runs,
`ExecContext::current().user()` and the `username` parameter
(`_request->user()`) are guaranteed to be the same value for the normal,
synchronous `RestDatabaseHandler` call path examined here. I also verified
that, for the purpose of the NONE-vs-not-NONE visibility check, using the
raw/configured level (as `AuthMode::Classic` does internally, see below) vs.
the runtime-effective/read-only-capped level (as `devel` does) makes no
observable difference, since read-only mode only ever caps `RW` down to
`RO`, never down to `NONE`.

So, for `RestDatabaseHandler` specifically, this refactoring does **not**
currently produce a behavioural difference from `devel`. It is flagged as a
latent risk / code-quality issue because:
- The function signature still takes an explicit `username` parameter that
  is silently unused for the actual permission decision, which is
  surprising and could easily be broken again by future refactoring (e.g.
  if this method is ever called for a user other than the "current" one,
  or from a context/thread where `ExecContext::current()` is not correctly
  set up — the same helper is also called from
  `arangod/V8Server/v8-vocbase.cpp:1739` with an explicit `user` argument
  coming from JavaScript).
- More generally, `AuthMode::Classic`'s internal helper
  `effectiveDatabaseAuthLevel()` (`arangod/Auth/AuthMode.cpp:99-106`) always
  calls `_userManager.databaseAuthLevel(username(), db, /*configured=*/true)`,
  i.e. it always uses the **raw configured** level, never the
  runtime-effective (read-only-capped) one that `devel` used almost
  everywhere (`devel:arangod/Utils/ExecContext.cpp:100,117-137`). This is
  safe today only because every `ExecContext` method that can result in a
  *write* action (`canCreateDatabase`, `canDropDatabase`, `canUseDatabase`
  for `Write`, `canCreateCollection`, ... — see
  `arangod/Utils/ExecContext.cpp:173-462`) re-implements the read-only-mode
  short-circuit explicitly before delegating to `can()`/`AuthMode::Classic`.
  This is a correct, but fragile, invariant: any newly added
  permission-check method that forgets to add this explicit read-only guard
  would silently permit write operations while the server is in read-only
  mode. Worth keeping in mind for future `RestHandler` reviews.

### Summary for `RestDatabaseHandler`

| Route | Verdict |
|---|---|
| `GET /_api/database` (list all, requires `_system`) | Identical to `devel` |
| `GET /_api/database/user` | **Regression** in the narrow, non-default case of an unauthenticated request over a UNIX domain socket with `--server.authentication-unix-sockets=false` (Finding 1); otherwise identical |
| `GET /_api/database/current` | Identical to `devel` |
| `GET /_api/database/shardStatistics` | Identical to `devel` |
| `POST /_api/database` (create) | Same ALLOW/DENY outcome as `devel` in all cases checked; error message/ordering differs in some read-only/non-admin corner cases (Finding 2, cosmetic); new "hide as NOT_FOUND" behaviour is gated behind the unrelated, not-yet-active-for-this-route API versioning scheme (Finding 3) |
| `DELETE /_api/database/{name}` (drop) | Same as above (Finding 2, Finding 3) |
| (all routes, indirectly) | `getDatabaseNamesForUser` ignores its `username` parameter in favour of thread-local `ExecContext::current()`; no observable difference today, but a latent risk (Finding 4) |

**Action items / recommendations:**
1. Restore an explicit authentication check in
   `RestDatabaseHandler::getDatabases()` for the `user` suffix (Finding 1),
   or fix the underlying incomplete migration of `devel`'s
   `CommTask::canAccessPath()` exceptions into
   `RestHandler::checkUserCanAccess()`.
2. No functional change strictly required for Finding 2, but consider
   aligning the check order (`isAdmin()`-like check before the read-only
   check) if exact error-message parity with `devel` is desired/tested.
3. No action required for Finding 3 (deliberate, gated, unrelated to RBAC
   toggle).
4. Consider passing the `username`/`ExecContext` explicitly into
   `getDatabaseNamesForUser` rather than relying on thread-local state, to
   remove the latent risk described in Finding 4 (low priority, no observed
   bug today).


## `RestCollectionHandler` (`arangod/RestHandler/RestCollectionHandler.cpp`)

Routes handled: `GET /_api/collection[/<name>[/checksum|figures|count|
properties|revision|shards]]`, `POST /_api/collection`,
`PUT /_api/collection/<name>/{load|unload|compact|responsibleShard|
truncate|properties|rename|loadIndexesIntoMemory|recalculateCount}`,
`DELETE /_api/collection/<name>`.

The handler file itself diffs only slightly from
`devel:arangod/RestHandler/RestCollectionHandler.cpp` (907 vs. 872 lines);
`RestCollectionHandler.h` is unchanged apart from a removed `@author`
doc-comment. Authorization logic is a mix of a few checks written directly
in the handler and checks delegated to `methods::Collections::{lookup,
create,drop,updateProperties,rename,warmup,properties}`
(`arangod/VocBase/Methods/Collections.cpp`), which in turn call into
`ExecContext`/`AuthMode::Classic`. Two real behavioural differences were
found (Findings 1 and 2 below); everything else that looked suspicious at
first turned out to be an equivalent reformulation of the same `devel`
logic (documented as Finding 4, for completeness/future reference).

Like `RestReplicationHandler`, `RestCollectionHandler` is not used
directly: `RestCollectionHandler::handleExtraCommandPut()`
(`arangod/RestHandler/RestCollectionHandler.h:65-67`) is a pure-virtual
extension point, and there are exactly two engine-/role-specific
subclasses that each implement it for the single `recalculateCount`
sub-command (the only `PUT` action not handled directly in the base
class — see `arangod/RestHandler/RestCollectionHandler.cpp:695-707`):
- `RocksDBRestCollectionHandler`
  (`arangod/RocksDBEngine/RocksDBRestCollectionHandler.cpp`) — registered
  for `/_api/collection` on single-server and DBServer
  (`arangod/RocksDBEngine/RocksDBRestHandlers.cpp:38`).
- `ClusterRestCollectionHandler`
  (`arangod/ClusterEngine/ClusterRestCollectionHandler.cpp`) — registered
  for `/_api/collection` on the coordinator
  (`arangod/ClusterEngine/ClusterRestHandlers.cpp`).

Both are analyzed below as Finding 5.

### Finding 1 (Regression): `GET /_api/collection` (top-level listing) uses `canSeeCollection`, which — unlike `devel`'s per-collection level check — always succeeds in `Classic` mode, leaking existence of collections that should be hidden

`devel` (`devel:arangod/RestHandler/RestCollectionHandler.cpp:120-134`):

```cpp
for (auto const& collection : methods::Collections::getNotDeleted(_vocbase)) {
  bool const canUse = ExecContext::current().canUseCollection(
      collection->name(), auth::Level::RO);
  if (canUse && (!excludeSystem || !collection->system())) {
    ...
  }
}
```

Current branch (`arangod/RestHandler/RestCollectionHandler.cpp:118-134`):

```cpp
for (auto const& collection : methods::Collections::getNotDeleted(_vocbase)) {
  bool const canSee =
      ExecContext::current()
          .canSeeCollection(_vocbase.name(), collection->name())
          .ok();
  if (canSee && (!excludeSystem || !collection->system())) {
    ...
  }
}
```

`devel`'s `canUseCollection(name, RO)` resolves to
`ExecContext::collectionAuthLevel()`
(`devel:arangod/Utils/ExecContext.cpp:140-179`) →
`auth::UserManager::collectionAuthLevel()`
(`arangod/Auth/UserManagerBase.cpp:251-...`, unchanged between branches) →
`auth::User::collectionAuthLevel()` (`arangod/Auth/User.cpp:728-748`,
unchanged between branches), which computes the **actual, possibly
collection-specific** access level: it consults `_collectionAccess`, a
per-`(database, collection)` grant table that a `_system`/db admin can set
independently of (and overriding, in either direction) the user's
database-level grant (e.g. `grantCollection(user, db, coll, "none")` to
hide one sensitive collection from a user who otherwise has `RW` on the
whole database) — plus hard-coded special cases: `_users` is always
`NONE` for everyone, `_queues` is always `RO`, `_frontend` is always `RW`.

The current branch's `canSeeCollection(db, coll)` instead calls
`can(SeeCollection{db, coll})`, whose `Classic`-mode implementation
(`arangod/Auth/AuthMode.cpp:380-384`) is:

```cpp
[&](p::SeeCollection const& /*collection*/) -> Result {
  // Database RO access is the only prerequisite and has already been
  // checked; a collection is always visible if the database is.
  return {};
},
```

i.e. it **unconditionally returns success**, regardless of any
per-collection grant. (The comment's premise — that database-level `RO`
access was "already... checked" — only holds for the coarse, database-wide
gate in `RestHandler::checkUserCanAccess()`
(`arangod/GeneralServer/RestHandler.cpp:718-724`), which runs once per
request before the handler's `handleCommandGet()` even starts; it says
nothing about the specific collection being iterated over here.)

Consequence: as long as a user has at least database-level `RO` access
(required just to reach this route at all), `GET /_api/collection` (no
suffix, the default `excludeSystem=false`) will now list **every**
collection in the database — including:
- Any collection the admin has explicitly set to `Level::NONE` for that
  user via a specific grant, even though the user has `RW`/`RO` on the
  database as a whole. In `devel` such a collection is filtered out of the
  list entirely; in the current branch it appears (`id`, `name`, `status`,
  `type` are all shown, though attempting to actually read/write it — via
  `GET /_api/collection/<name>` or any data route — is still correctly
  denied by `methods::Collections::lookup`, see Finding 4 below, since that
  code path still uses the real, level-based `canUseCollection`, not
  `canSeeCollection`).
- The `_users` system collection, which `devel` **always** hides from this
  listing (its `collectionAuthLevel` is hard-coded to `NONE` for everyone,
  so `canUse(RO)` always fails for it, independent of `excludeSystem`). The
  current branch will include `_users` in the listing whenever the caller
  passes `excludeSystem=false` (the default!), since `system()` collections
  are only excluded via the `excludeSystem` request parameter, not via the
  (always-succeeding) `canSeeCollection` check.

This is a genuine information-disclosure regression versus `devel`: the
existence (id/name/type/status, not the contents) of collections that are
supposed to be completely hidden from a given user is now revealed through
the collection-listing endpoint in `Classic` mode.

### Finding 2 (Regression): `DELETE /_api/collection/<name>` now returns `403 FORBIDDEN` instead of `404 NOT_FOUND` for a non-existent collection when the caller lacks write access

`devel` (`devel:arangod/RestHandler/RestCollectionHandler.cpp:693-704`)
performs the lookup **first**, with no separate, up-front permission check:

```cpp
std::shared_ptr<LogicalCollection> coll;
Result res = methods::Collections::lookup(_vocbase, name, coll);
if (res.fail()) {
  events::DropCollection(_vocbase.name(), name, res.errorNumber());
  generateError(res);
  co_return;
}
...
res = methods::Collections::drop(*coll, dropOptions);
```

`methods::Collections::lookup()` (identical logic in both branches,
`arangod/VocBase/Methods/Collections.cpp:556-578` /
`devel:arangod/VocBase/Methods/Collections.cpp:558-580`) checks
**existence first**, and only checks `canUseCollection(..., Read)`
*after* confirming the collection physically exists:

```cpp
auto coll = vocbase.lookupCollection(name);
if (coll != nullptr) {
  if (auto r = ExecContext::current().canUseCollection(
          vocbase.name(), coll->name(), AccessLevel::Read); r.fail()) {
    return r;                                    // FORBIDDEN
  }
  ...
  return Result();
}
return Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);   // <-- reached
                                                           //     regardless
                                                           //     of rights
                                                           //     if not found
```

So in `devel`, requesting `DELETE` on a **non-existent** collection always
yields `404 NOT_FOUND`, no matter what access level the caller has (`RW`,
`RO`, or effectively none beyond what's needed to reach the handler); only
for an *existing* collection does insufficient write access surface as
`403 FORBIDDEN` (raised inside `Collections::drop()`, which independently
re-checks `canUseDatabase(RW) && canUseCollection(RW)`,
`devel:arangod/VocBase/Methods/Collections.cpp:1233-1245`).

Current branch (`arangod/RestHandler/RestCollectionHandler.cpp:725-731`)
adds a **new, up-front** permission check *before* calling `lookup()`:

```cpp
// Check if we are allowed to drop the collection:
if (auto r = ExecContext::current().canDropCollection(_vocbase.name(), name);
    r.fail()) {
  events::DropCollection(_vocbase.name(), name, TRI_ERROR_FORBIDDEN);
  generateError(r);
  co_return;
}

std::shared_ptr<LogicalCollection> coll;
Result res = methods::Collections::lookup(_vocbase, name, coll);
```

`canDropCollection()` (`arangod/Utils/ExecContext.cpp:214-221`) is a pure
permission-level check — it has no notion of whether the named collection
actually exists — requiring database-level `Write` and collection-level
`WriteMeta`
(`arangod/Auth/AuthMode.cpp:391-411`, container principle). Since this
check now runs *before* any existence check, a caller who only has
database-level `RO` (or no per-collection write grant) now gets
`403 FORBIDDEN` for `DELETE /_api/collection/<anything>`, **including
collection names that don't exist at all** — where `devel` would have
returned `404 NOT_FOUND` uniformly for non-existent names regardless of the
caller's permission level.

Net effect for a read-only user attempting to delete a *non-existent*
collection: `devel` → `404 NOT_FOUND`; current branch (`Classic` mode) →
`403 FORBIDDEN`. For an *existing* collection the caller isn't allowed to
drop, both branches agree on `403 FORBIDDEN` (only the message text
differs, see Finding 4). Note also that `Collections::drop()` in the
current branch performs the *exact same* `canDropCollection()` check again
internally (`arangod/VocBase/Methods/Collections.cpp:1219-1230`) — so for
requests that *do* pass the new up-front check, the permission is
evaluated twice; harmless (idempotent), but redundant.

### Finding 3 (New, gated, deliberate — not a `Classic`-mode regression): `WriteMeta` check for `PUT .../compact`

`arangod/RestHandler/RestCollectionHandler.cpp:482-492` adds a permission
check that doesn't exist in `devel` at all for the `compact` sub-action:

```cpp
// We only enforce WriteMeta access here if the API version is higher
// than 0 for backwards compatibility:
if (_request.get()->requestedApiVersion() > 0) {
  if (auto r = ExecContext::current().canUseCollection(
          _vocbase.name(), name, AccessLevel::WriteMeta);
      r.fail()) {
    generateError(r);
    co_return;
  }
}
```

As with the equivalent finding for `RestDatabaseHandler` (Finding 3 above),
this is explicitly gated behind `requestedApiVersion() > 0`, which is only
non-zero for requests through the new `/_arango/v<N>/...` prefix — a
feature absent from `devel` and orthogonal to the RBAC toggle. For the
classic, unprefixed `/_api/collection/<name>/compact` route,
`requestedApiVersion()` is always `0`, so this new check never fires and
behaviour remains identical to `devel` (where `compact` previously required
only the `Read` access already enforced by `methods::Collections::lookup`).

### Finding 4 (Verified equivalent / Cosmetic — documented to avoid re-investigating): several call sites that *looked* different but aren't

While diffing `arangod/VocBase/Methods/Collections.cpp` line-by-line
against `devel:arangod/VocBase/Methods/Collections.cpp`, several functions
appeared to check authorization differently, but were confirmed to produce
the same ALLOW/DENY decisions in `Classic` mode as `devel`:

- **`Collections::create`**: `devel` checks a single
  `exec.canUseDatabase(vocbase.name(), auth::Level::RW)` for the whole
  batch (`devel:...Collections.cpp:604-615`); the current branch instead
  loops and calls `exec.canCreateCollection(vocbase.name(), col.name)` per
  collection to be created (`arangod/VocBase/Methods/Collections.cpp:602-608`).
  `AuthMode::Classic::check(CreateCollection)` (`arangod/Auth/AuthMode.cpp:385-390`)
  is `return check(UseDatabase{db, Write})` — i.e. also purely a
  database-level `RW` check, independent of the (not-yet-existing)
  collection's name. Equivalent.
- **`Collections::drop`**: `devel` checks
  `canUseDatabase(RW) && canUseCollection(name, RW)` directly
  (`devel:...Collections.cpp:1233-1245`); current calls
  `canDropCollection(db, name)` (`arangod/VocBase/Methods/Collections.cpp:1219-1224`),
  whose `Classic` implementation (`arangod/Auth/AuthMode.cpp:391-411`)
  checks `UseDatabase{db, Write}` then `UseCollection{db, name, WriteMeta}`
  (`WriteMeta`/`WriteData` both map to `auth::Level::RW`, so this is the
  same pair of checks, just funneled through the permission-vocabulary
  API. The error code is explicitly normalized to `TRI_ERROR_FORBIDDEN` in
  both branches, discarding the more specific `TRI_ERROR_ARANGO_READ_ONLY`
  that `check()` might otherwise produce, "for API compatibility" per an
  explicit code comment.) Equivalent.
- **`Collections::updateProperties`/`rename`**: `devel` checks
  `canUseCollection(name, RW) && canUseDatabase(RW)` explicitly
  (`devel:...Collections.cpp:1148-1150,1204-1206` roughly); current checks
  only `canUseCollection(db, name, WriteMeta)`
  (`arangod/VocBase/Methods/Collections.cpp:1023-1027`,
  `arangod/VocBase/Methods/Collections.cpp:1141-1145`), but
  `AuthMode::Classic::check(UseCollection)` has an explicit
  "container principle" clause for `WriteMeta`
  (`arangod/Auth/AuthMode.cpp:240-249`) that additionally requires
  database-level `RW`. Equivalent.
- **`Collections::properties`**: `devel` checks
  `canUseCollection(name, RO) && (databaseAuthLevel() != NONE)`
  (`devel:...Collections.cpp:1152-1157` roughly); current checks
  `canUseDatabase(db, Read)` then `canUseCollection(db, name, Read)`
  (`arangod/VocBase/Methods/Collections.cpp:1075-1084`). `RO`/not-`NONE`
  and "`canUseDatabase(Read)`" both mean "at least `RO`" — equivalent, just
  reordered (database check first instead of second).
- **`Collections::warmup`**: both check `Read`-level collection access
  only, unchanged.

The only actual difference across all of the above is **error message
text** in a few places (e.g. `Collections::lookup`'s `"No access to
collection '<name>'"` in `devel` vs. `"insufficient collection access
level for '<name>' in database '<db>'"` in the current branch,
`arangod/VocBase/Methods/Collections.cpp:560-564` vs.
`devel:...Collections.cpp:562-566`) — same `TRI_ERROR_FORBIDDEN` code in
both cases, consistent with the "Cosmetic" category defined at the top of
this document.

One unrelated-to-`Classic`-mode change also observed in
`Collections::create`: the guard for auto-granting the creating user `RW`
access on their own newly-created collection changed from
`!exec.isSuperuser()` (`devel`) to `!exec.isSuperuserOrDisabled()` (current,
`arangod/VocBase/Methods/Collections.cpp:703`). Since `Classic` mode
implies authentication is active (`isDisabled()` is only true for the
separate, fully-auth-disabled mode, out of scope for this comparison, per
the task description), `isSuperuserOrDisabled()` reduces to `isSuperuser()`
under `Classic` mode — no observable difference for the mode under
investigation here.

### Finding 5: the two `handleExtraCommandPut()` subclasses — `RocksDBRestCollectionHandler` and `ClusterRestCollectionHandler` (`PUT /_api/collection/<name>/recalculateCount`)

**`ClusterRestCollectionHandler`** (`arangod/ClusterEngine/ClusterRestCollectionHandler.cpp`)
is **byte-for-byte identical** to
`devel:arangod/ClusterEngine/ClusterRestCollectionHandler.cpp` (empty
diff, not even the usual `@author`-line cosmetic difference) — no finding,
no divergence whatsoever. However, its content is worth stating plainly,
since it is relevant to understanding the overall authorization picture:
```cpp
futures::Future<Result> ClusterRestCollectionHandler::handleExtraCommandPut(
    std::shared_ptr<LogicalCollection> coll, std::string const& suffix,
    velocypack::Builder& builder) {
  if (suffix == "recalculateCount") {
    Result res = recalculateCountsOnAllDBServers(
        server().getFeature<ClusterFeature>(), _vocbase.name(), coll->name());
    ...
  }
  return {TRI_ERROR_NOT_IMPLEMENTED};
}
```
This performs **no permission check of any kind** before triggering a
cluster-wide recount on every DBServer holding a shard of the collection.
Since `RestCollectionHandler::handleCommandPut()`
(`arangod/RestHandler/RestCollectionHandler.cpp:443-458`) only guarantees
**Read**-level access via `methods::Collections::lookup()` before
dispatching to `handleExtraCommandPut()` (as its own comment states: "All
permission checks for reading the collection are done in the
`methods::Collections::lookup` method... The checks for writing... are
typically done here in the RestHandler" — but for `recalculateCount`,
no such check exists at the `RestHandler` level, and none is added by
`ClusterRestCollectionHandler` either), **any user with mere `RO` access
to a collection can trigger `PUT .../recalculateCount` on the coordinator**
and force a cluster-wide, potentially expensive, recount operation across
all DBServers. This is a real, latent authorization gap — but since it is
**identical, unchanged, in both `devel` and the current branch**, it is
**not a divergence** within the scope of this document (which tracks
current-branch-vs-`devel` behavioural differences under Classic mode) and
is therefore not assigned its own regression/fix verdict; it is recorded
here purely for completeness/awareness, since it was uncovered directly by
this investigation and is easy to miss.

**`RocksDBRestCollectionHandler`**
(`arangod/RocksDBEngine/RocksDBRestCollectionHandler.cpp`), by contrast,
**does** diverge from `devel`, and this is a genuine, newly-found
regression (in the stricter/safer direction). `devel`
(`devel:arangod/RocksDBEngine/RocksDBRestCollectionHandler.cpp:41-44`):
```cpp
if (!ExecContext::current().canUseCollection(coll->name(), auth::Level::RW)) {
  co_return Result(TRI_ERROR_FORBIDDEN);
}
```
Current branch (`arangod/RocksDBEngine/RocksDBRestCollectionHandler.cpp:42-46`):
```cpp
if (auto r = ExecContext::current().canUseCollection(
        coll->vocbase().name(), coll->name(), AccessLevel::WriteMeta);
    !r.ok()) {
  co_return r;
}
```
At first glance this looks like the same cosmetic `auth::Level::RW` →
`AccessLevel::WriteMeta` / single-arg → two-arg `canUseCollection` API
migration already proven equivalent multiple times in this document (e.g.
Finding 4's `Collections::updateProperties`/`rename` case) — the single-arg
overload is confirmed to simply forward to the two-arg one using the
`ExecContext`'s own `_database`
(`devel:arangod/Utils/ExecContext.h:136-139`), which is the same database
as `coll->vocbase()` here, so that part is indeed a no-op change. **But
this specific instance is *not* equivalent**, because
`AuthMode::Classic::check(UseCollection{..., WriteMeta})`
(`arangod/Auth/AuthMode.cpp:237-249`) has an additional "container
principle" clause not present in the old check:
```cpp
// WriteMeta additionally requires RW access to the database
// (container principle: modifying a collection's meta-data
// requires write permission on the containing database).
if (collection.level == CollectionAccessLevel::WriteMeta) {
  auto const dbLevel = effectiveDatabaseAuthLevel(collection.db);
  if (dbLevel < auth::Level::RW) {
    return {TRI_ERROR_FORBIDDEN, ...};
  }
}
```
Unlike the `updateProperties`/`rename` case (where `devel`'s *old* code
already performed an explicit, separate `canUseDatabase(RW)` check in
addition to `canUseCollection(RW)` — so the new container-principle clause
merely folds an already-present check into the permission-vocabulary API),
`devel`'s old `recalculateCount` check was **only**
`canUseCollection(name, RW)` — a purely collection-level check, with
**no** accompanying database-level check. Since `auth::User::
collectionAuthLevel()`/`effectiveCollectionAuthLevel()` can return `RW` for
a specific collection via a per-`(database, collection)` grant override
even when the user's database-level access is only `RO` (the same
override mechanism documented in Finding 1 above), there is a concrete
scenario where behaviour differs:

- A user granted database-level `RO` on `db1`, but an explicit
  per-collection override of `RW` on `db1/coll1` (e.g.
  `grantCollection(user, "db1", "coll1", "rw")` while `grantDatabase(user,
  "db1", "ro")`).
- In `devel`: `canUseCollection("coll1", RW)` succeeds (the per-collection
  override applies) → `PUT /_api/collection/coll1/recalculateCount`
  **succeeds**.
- In the current branch: the same per-collection check succeeds, but the
  new container-principle clause additionally requires
  `effectiveDatabaseAuthLevel("db1") >= RW`, which is `RO` here → **fails
  with `403 FORBIDDEN`**.

This is a narrow (requires a specific, somewhat unusual grant
configuration — collection-level `RW` combined with database-level `RO`)
but real behavioural regression, in the stricter/safer direction (denying
something `devel` allowed), affecting only single-server and DBServer
(`ClusterRestCollectionHandler`'s coordinator path is, as shown above,
completely unaffected — and indeed unchanged — by this container-principle
tightening, since it performs no check at all).

### Summary for `RestCollectionHandler`

| Route | Verdict |
|---|---|
| `GET /_api/collection` (list all) | **Regression** (Finding 1): `canSeeCollection` always succeeds in `Classic` mode, so per-collection deny grants and the hard-coded `_users` hiding rule from `devel` are no longer honored in the listing (existence-only leak; actual read/write remains correctly denied elsewhere) |
| `GET /_api/collection/<name>[/...]` | Identical to `devel` (goes through `methods::Collections::lookup`, which still uses the real, level-based `canUseCollection`) |
| `POST /_api/collection` (create) | Identical to `devel` (Finding 4) |
| `PUT .../{load,unload,truncate,properties,rename,loadIndexesIntoMemory,responsibleShard}` | Identical to `devel` (Finding 4) |
| `PUT .../compact` | Identical to `devel` for the classic route; a new `WriteMeta` check exists but is gated behind the unrelated API-versioning scheme (Finding 3) |
| `DELETE /_api/collection/<name>` | **Regression** (Finding 2): returns `403 FORBIDDEN` instead of `404 NOT_FOUND` for a *non-existent* collection when the caller lacks write access, due to a new up-front `canDropCollection` check that runs before the existence check |
| `PUT .../recalculateCount` (coordinator, `ClusterRestCollectionHandler`) | Identical to `devel` — byte-for-byte unchanged; **both** branches perform no permission check at all (Finding 5, not a divergence, recorded for awareness) |
| `PUT .../recalculateCount` (single-server/DBServer, `RocksDBRestCollectionHandler`) | **Regression** (Finding 5, narrow, stricter/safer direction): a user with per-collection `RW` override but only database-level `RO` could recalculate in `devel`; now denied with `403 FORBIDDEN` due to the new `WriteMeta` container-principle check |

**Action items / recommendations:**
1. Fix Finding 1: either make `AuthMode::Classic::check(SeeCollection)`
   perform the real, level-based check (mirroring
   `auth::User::collectionAuthLevel`/the old `canUseCollection(RO)`
   semantics) instead of unconditionally returning success, or have
   `RestCollectionHandler::handleCommandGet()` call
   `canUseCollection(db, name, AccessLevel::Read)` instead of
   `canSeeCollection` for the top-level listing (as it still effectively
   needs to, for parity with `devel`).
2. Fix Finding 2: move the `canDropCollection` pre-check in
   `RestCollectionHandler::handleCommandDelete()` to *after* the
   `methods::Collections::lookup()` existence check (or drop the
   redundant up-front check entirely and rely on the existing check inside
   `methods::Collections::drop()`, as `devel` does), to restore `404`-before-`403`
   precedence for non-existent collections.
3. No action required for Finding 3 (deliberate, gated, unrelated to RBAC
   toggle) or Finding 4 (already equivalent; documented to save
   re-investigation effort in the future).
4. Finding 5 (`recalculateCount`): the `RocksDBRestCollectionHandler`
   narrowing is low-risk (an unusual grant combination) and arguably a
   reasonable side-effect of standardizing on the `WriteMeta`
   container-principle check elsewhere — worth a one-line release-note
   mention rather than a code fix. Separately, and out of scope for a
   current-branch-vs-`devel` fix (since it is unchanged in both): the
   complete absence of any permission check in
   `ClusterRestCollectionHandler::handleExtraCommandPut()` for
   `recalculateCount` is worth flagging to the team as a pre-existing gap
   worth closing in `devel` directly (e.g. by adding the same `WriteMeta`
   check there), independent of this comparison exercise.

**Addendum (found while investigating `RestDocumentHandler`, see below):**
the `PUT /_api/collection/<name>/truncate` route listed above as "Identical
to `devel`" needs a caveat. `truncate` is executed through a real,
write-mode transaction (`arangod/RestHandler/RestCollectionHandler.cpp:596-600`,
`trx->truncateAsync(...)`), which is gated by the very same
`TransactionState::checkCollectionPermission()` machinery discussed in
Finding 1 of the `RestDocumentHandler` section below. It is therefore
subject to that finding's read-only-mode regression as well (`load`,
`unload`, `properties`, `rename`, `responsibleShard` do not open a
write-mode transaction on the collection's data and remain unaffected).

## `RestDocumentHandler` (`arangod/RestHandler/RestDocumentHandler.cpp`)

`RestDocumentHandler` implements the classic document CRUD API:
`POST/GET/HEAD/PUT/PATCH/DELETE /_api/document[/<collection>[/<key>]]`.
Unlike `RestDatabaseHandler`/`RestCollectionHandler`, this handler contains
essentially **no authorization logic of its own**. Every operation
(`insertDocument`, `readSingleDocument`, `readManyDocuments`,
`modifyDocument`, `removeDocument`) immediately calls
`RestVocbaseBaseHandler::createTransaction()`
(`arangod/RestHandler/RestVocbaseBaseHandler.cpp:487-601`) to obtain a
`transaction::Methods`, then `co_await trx->beginAsync()`
(e.g. `arangod/RestHandler/RestDocumentHandler.cpp:251,394,605,760,851`).
All authorization for document access happens *inside* that shared
transaction machinery — specifically when the collection is added to the
transaction — not in `RestDocumentHandler.cpp` itself.

Diffing `arangod/RestHandler/RestDocumentHandler.cpp` directly against
`devel:arangod/RestHandler/RestDocumentHandler.cpp` confirms this: the
*only* differences are (a) copyright year and a removed `@author` comment,
(b) an added explanatory comment, (c) `OperationOptions opOptions(_context)`
(`devel`) → `OperationOptions opOptions;` (current branch, `_context` member
removed from `OperationOptions`, see below), and (d) a refactor of how the
storage engine is looked up in `handleFillIndexCachesValue()`
(`_vocbase.server().getFeature<EngineSelectorFeature>().engine()` in `devel`
vs. `_vocbase.engine()` in the current branch) — an unrelated internal API
simplification. None of these affect authorization.

Regarding (c): `OperationOptions::context()` (`devel` only,
`devel:arangod/Utils/OperationOptions.h:198`,`devel:arangod/Utils/OperationOptions.cpp:100-105`)
falls back to `ExecContext::current()` whenever no explicit context was
passed in, and a repository-wide search
(`git grep -n "\.context()" devel`) shows the *only* call site that ever
uses the explicitly-passed context is `Collections::create`
(`devel:arangod/VocBase/Methods/Collections.cpp:604`, already covered in
the `RestCollectionHandler` section above). Since `RestDocumentHandler`
never calls `Collections::create`, passing vs. not passing `_context` here
is a no-op: `ExecContext::current()` is always equal to `_context` for the
plain, synchronous request-handling path examined throughout this
document. Confirmed not a behavioural difference.

Because the handler code itself is authorization-free, the real comparison
has to trace the shared collection-locking path used by every document
operation:
`transaction::Methods::beginAsync()` → `TransactionState::useCollections()`/
`addCollection()` (`arangod/StorageEngine/TransactionState.cpp:427-571`) →
`TransactionState::checkCollectionPermission()`
(`arangod/StorageEngine/TransactionState.cpp:713-754`). This is the code
that actually decides, for every `INSERT`/`GET`/`PUT`/`PATCH`/`DELETE` on
`/_api/document/...`, whether the requested collection access is granted.

### Finding 1 (Regression): global read-only-mode short-circuit in `ExecContext::canUseCollection`/`canUseDatabase` reports the wrong error code (`FORBIDDEN` instead of `ARANGO_READ_ONLY`) for otherwise-permitted write requests

Side-by-side comparison of `checkCollectionPermission`:

`devel` (`devel:arangod/StorageEngine/TransactionState.cpp:724-768`):
```cpp
auto level = exec.collectionAuthLevel(_vocbase.name(), cname);
if (level == auth::Level::NONE) {
  return {TRI_ERROR_FORBIDDEN, ...};
} else {
  bool collectionWillWrite = AccessMode::isWriteOrExclusive(accessType);
  if (level == auth::Level::RO && collectionWillWrite) {
    return {TRI_ERROR_ARANGO_READ_ONLY, ...};   // note: specific code!
  }
}
return {};
```

Current branch (`arangod/StorageEngine/TransactionState.cpp:713-754`):
```cpp
if (accessType == AccessMode::Type::READ) {
  if (auto r = exec.canUseCollection(_vocbase.name(), cname, AccessLevel::Read); r.fail())
    return {TRI_ERROR_FORBIDDEN, ...};
  return {};
}
if (auto r = exec.canUseCollection(_vocbase.name(), cname, AccessLevel::WriteData); r.fail())
  return r;   // <-- forwards whatever `r` says
return {};
```

At first glance these look equivalent (and mostly are — see below), because
`ExecContext::canUseCollection()` → `AuthMode::Classic::check(UseCollection)`
(`arangod/Auth/AuthMode.cpp:176-251`) reproduces `devel`'s
`RW`-requested-vs-`RO`-granted → `TRI_ERROR_ARANGO_READ_ONLY` distinction
almost verbatim (`arangod/Auth/AuthMode.cpp:223-228`). **The divergence is
specifically in how the two branches account for the server being
globally in read-only mode** (`--server.read-only` / hot-backup /
maintenance mode toggled via `ServerState::readOnly()`):

- `devel` bakes the read-only-mode downgrade *into the level itself*,
  inside `UserManagerImpl::collectionAuthLevel()`/`databaseAuthLevel()`
  when called with `configured=false` (the default/normal way `devel`'s
  `ExecContext::collectionAuthLevel()` always calls it,
  `devel:arangod/Utils/ExecContext.cpp:140-179`, esp. line 178):
  ```cpp
  // devel:arangod/Auth/UserManagerImpl.cpp:1016-1021 (collectionAuthLevel)
  // and analogously :978-982 (databaseAuthLevel)
  if (!configured) {
    if (level > Level::RO && ServerState::readOnly()) {
      return Level::RO;               // RW silently capped to RO
    }
  }
  ```
  A configured `RW` grant is therefore *silently downgraded to `RO`*
  whenever the server is read-only, **before** `checkCollectionPermission`
  ever sees it. The subsequent `level == auth::Level::RO && collectionWillWrite`
  branch then correctly reports the specific `TRI_ERROR_ARANGO_READ_ONLY` —
  exactly the same code path/outcome as for a user who was only ever
  granted plain `RO` on that collection.

- The current branch instead added an *explicit, separate* short-circuit in
  each of `ExecContext`'s write-capable wrapper methods, e.g.
  `canUseCollection` (`arangod/Utils/ExecContext.cpp:223-231`):
  ```cpp
  Result ExecContext::canUseCollection(std::string_view db, std::string_view coll,
                                       CollectionAccessLevel level) const {
    if (!isSuperuser() && ServerState::readOnly() &&
        level >= CollectionAccessLevel::WriteData) {
      return {TRI_ERROR_FORBIDDEN, "Server is in read-only mode."};
    }
    return can(UseCollection{.db{db}, .name{coll}, .level = level});
  }
  ```
  and analogously `canUseDatabase`, `canCreateCollection`,
  `canDropCollection`, `canCreateDatabase`, `canDropDatabase`, `canCreateIndex`,
  `canDropIndex`, `canUseView`, `canModifyView`, ... (all in
  `arangod/Utils/ExecContext.cpp:173-437`). This check fires **unconditionally**
  whenever the server is read-only and the requested level is `WriteData`
  or higher — completely independent of whether the calling user's actual
  configured collection permission is `RW`, `RO`, or even `NONE`. It always
  returns the generic `TRI_ERROR_FORBIDDEN` (11), never
  `TRI_ERROR_ARANGO_READ_ONLY` (1004).

**Net, observable effect** for `INSERT`/`UPDATE`/`REPLACE`/`REMOVE` via
`RestDocumentHandler` (and any other write transaction going through
`TransactionState::checkCollectionPermission`, e.g. `RestCollectionHandler`'s
`truncate`, see addendum above) while the server is globally in read-only
mode, for a user who is *not* fully blocked (i.e. has at least `RO` access
to the collection, which includes the overwhelmingly common case of a user
who normally has full `RW` access):

| | `devel` | current branch (`Classic`) |
|---|---|---|
| error code | `TRI_ERROR_ARANGO_READ_ONLY` (1004) | `TRI_ERROR_FORBIDDEN` (11) |
| error message | `"read-only collection access level for '<name>' in database '<db>'"` | `"Server is in read-only mode."` |
| HTTP status | 403 (`lib/Rest/GeneralResponse.cpp:383,387`) | 403 (`lib/Rest/GeneralResponse.cpp:381,387`) |

The HTTP status code is unchanged (both `TRI_ERROR_ARANGO_READ_ONLY` and
`TRI_ERROR_FORBIDDEN` map to `ResponseCode::FORBIDDEN`,
`lib/Rest/GeneralResponse.cpp:381-387`), so this is **not** merely a
message-text/cosmetic issue as classified for similar-looking findings in
earlier sections of this document: the JSON response body's `errorNum`
field genuinely changes from `1004` to `11`. `TRI_ERROR_ARANGO_READ_ONLY`
is a semantically distinct, documented error code that client drivers,
`arangosh`, cluster failover/retry logic and test suites can reasonably
key off of to distinguish "temporarily read-only, perhaps retry against
the leader/later" from a genuine, permanent, permission-based denial. This
qualifies as a **Regression**.

If the calling user has no access at all (`Level::NONE`) to the collection,
both branches agree on plain `TRI_ERROR_FORBIDDEN` regardless of server
read-only mode — no divergence in that sub-case (in `devel` the downgrade
`if (level > Level::RO ...)` never applies to `NONE`; in the current branch
the wrapper's short-circuit also yields `FORBIDDEN`, coincidentally the
same code).

Interestingly, the low-level, commit-time safety net that both branches
also have is unaffected and still correct in the current branch — it is
simply now unreachable for ordinary REST-triggered document writes:
- `arangod/Transaction/Methods.cpp:3754-3771` (`Methods::commitInternal`):
  `bool cancelRW = ServerState::readOnly() && !exec.isSuperuserOrDisabled(); ... return Result(TRI_ERROR_ARANGO_READ_ONLY, ...)`
- `arangod/RocksDBEngine/Methods/RocksDBTrxBaseMethods.cpp:452-457`: same
  pattern, same correct `TRI_ERROR_ARANGO_READ_ONLY`.

Both are byte-for-byte equivalent to their `devel` counterparts
(`devel:arangod/Transaction/Methods.cpp:3766-3769`,
`devel:` the analogous RocksDB commit path) — but since
`TransactionState::checkCollectionPermission()` now rejects the write
*earlier*, during `addCollection()`/`beginAsync()`, execution never reaches
this correct commit-time code in the scenario described above.

This defect lives entirely in the shared `arangod/Utils/ExecContext.cpp`
helpers, not in `RestDocumentHandler` itself, so it transitively affects
every `RestHandler` that performs a plain, transaction-based collection
write (confirmed here for `RestDocumentHandler` and, per the addendum
above, `RestCollectionHandler`'s `truncate`). It was previously flagged as
a "fragile invariant" in the `RestDatabaseHandler` section (Finding 4
there) but had not yet been shown to actually change the reported error
*code* — the handlers reviewed so far (`Databases::create/drop`,
`Collections::create/drop/rename/updateProperties`) all happen to use
hand-rolled, boolean-returning permission checks that hardcode
`TRI_ERROR_FORBIDDEN` regardless of reason in *both* `devel` and the
current branch, which masks this discrepancy. `RestDocumentHandler` is the
first handler examined whose authorization path flows through the
generic, `Result`-returning, level-comparison-based
`TransactionState::checkCollectionPermission()`, which is where the
discrepancy becomes externally observable.

### Summary for `RestDocumentHandler`

| Route | Verdict |
|---|---|
| `POST /_api/document[/<collection>]` (insert) | Identical to `devel`, **except** Finding 1 (server-wide read-only mode) |
| `GET /_api/document/<collection>/<key>` (read single) | Identical to `devel` (read-only mode does not affect `Read`-level checks) |
| `HEAD /_api/document/<collection>/<key>` (check existence) | Identical to `devel` |
| `PUT /_api/document/<collection>[/<key>]` (replace) | Identical to `devel`, **except** Finding 1 |
| `PUT /_api/document/<collection>?onlyget=true` (read many) | Identical to `devel` |
| `PATCH /_api/document/<collection>[/<key>]` (update) | Identical to `devel`, **except** Finding 1 |
| `DELETE /_api/document/<collection>[/<key>]` (remove) | Identical to `devel`, **except** Finding 1 |

**Action items / recommendations:**
1. Fix Finding 1: change `ExecContext::canUseCollection`/`canUseDatabase`
   (and the analogous `canCreateCollection`/`canDropCollection`/
   `canCreateIndex`/`canDropIndex`/`canCreateDatabase`/`canDropDatabase`/
   `canUseView`/`canModifyView`/... helpers in `arangod/Utils/ExecContext.cpp`)
   to mirror `devel`'s approach: only *cap* an effective `RW` grant down to
   `RO` when `ServerState::readOnly()` is true (rather than
   unconditionally returning a hardcoded `FORBIDDEN`), and let the normal,
   already-correct `AuthMode::Classic::check(UseCollection)` level-comparison
   logic (`arangod/Auth/AuthMode.cpp:223-228`) decide between
   `TRI_ERROR_ARANGO_READ_ONLY` and `TRI_ERROR_FORBIDDEN` based on the
   (possibly capped) effective level, exactly as `devel` does. This would
   also make the now-unreachable commit-time safety net
   (`arangod/Transaction/Methods.cpp:3765-3770`) reachable again as a
   true last-resort fallback, restoring parity with `devel`.
2. No action required for the rest of `RestDocumentHandler`: the handler
   itself is authorization-free and behaves identically to `devel` in
   every other respect checked.

## `RestMetricsHandler` (`arangod/RestHandler/RestMetricsHandler.cpp`)

Mounted at `GET /_admin/metrics[/v2]` (prefix route, `RestMetricsHandler.h:36-42`
declares no path suffixes are parsed for authorization purposes). This handler
returns the Prometheus/VelocyPack metrics payload and has exactly **one**
authorization check, right at the top of `executeAsync()`; the remainder of
the handler (parameter parsing, cluster redirection, metrics serialization)
is unchanged between branches and irrelevant to authorization.

Diffing `arangod/RestHandler/RestMetricsHandler.cpp` against
`devel:arangod/RestHandler/RestMetricsHandler.cpp` confirms the auth check is
the *only* behaviourally relevant change; everything else is either
identical or an unrelated internal refactor (caching
`metrics::ClusterMetricsFeature&` as a member instead of looking it up via
`server().getFeature<...>()` at each call site).

`devel` (`devel:arangod/RestHandler/RestMetricsHandler.cpp:91-98`):

```cpp
auto& security = server().getFeature<ServerSecurityFeature>();
if (!security.canAccessHardenedApi()) {
  // don't leak information about server internals here
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
  co_return;
}
```

`ServerSecurityFeature::canAccessHardenedApi()`
(`devel:arangod/GeneralServer/ServerSecurityFeature.cpp:94-106`):

```cpp
bool ServerSecurityFeature::canAccessHardenedApi() const noexcept {
  bool allowAccess = !isRestApiHardened();
  if (!allowAccess) {
    ExecContext const& exec = ExecContext::current();
    if (exec.isAdminUser()) {
      allowAccess = true;
    }
  }
  return allowAccess;
}
```

Current branch (`arangod/RestHandler/RestMetricsHandler.cpp:94-101`):

```cpp
if (auto r = ExecContext::current().canUseHardenedAction(
        auth::perms::AdminMonitoring{});
    r.fail()) {
  // don't leak information about server internals here
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                r.errorMessage());
  co_return;
}
```

`ExecContext::canUseHardenedAction()` (`arangod/Utils/ExecContext.h:147-156`):

```cpp
Result canUseHardenedAction(auth::perms::AnyAdmin auto action) const {
  if (!_isRestApiHardened) {
    return {};
  }
  return can(std::move(action));
}
```

I traced every branch of both implementations and they are **fully
equivalent** in `Classic` mode:

- **Hardened-off gate**: `devel`'s `!isRestApiHardened()` and the current
  branch's `if (!_isRestApiHardened) return {};` both short-circuit to
  "always allow" when `--server.harden` (default: `false`, same option
  name/description/default in both branches,
  `arangod/GeneralServer/ServerSecurityOptionsProvider.cpp:35-39` vs.
  `devel:arangod/GeneralServer/ServerSecurityFeature.cpp:44-48`) is off.
  `_isRestApiHardened` is captured once into `ExecContext` at creation time
  from `securityFeature.isRestApiHardened()`
  (`arangod/Utils/ExecContext.cpp:113-115`), i.e. the same underlying
  option as `devel`'s `security.canAccessHardenedApi()` call reads live.
- **Hardened-on gate — admin check**: `devel`'s `exec.isAdminUser()` is a
  boolean **precomputed once** at `ExecContext::create()` time
  (`devel:arangod/Utils/ExecContext.cpp:90-114`):
  ```cpp
  dbLvl = sysLvl = um->databaseAuthLevel(user, dbname, false);
  if (dbname != StaticStrings::SystemDatabase)
    sysLvl = um->databaseAuthLevel(user, StaticStrings::SystemDatabase, false);
  isAdminUser = (sysLvl == auth::Level::RW);
  if (!isAdminUser && ServerState::readOnly()) {
    isAdminUser = um->databaseAuthLevel(user, StaticStrings::SystemDatabase,
                                        true) == auth::Level::RW;
  }
  ```
  I worked through all four combinations of {server read-only mode on/off}
  × {raw configured `_system` level RW or not} and confirmed this
  simplifies, in every case, to exactly:
  `isAdminUser == (databaseAuthLevel(user, "_system", /*configured=*/true) == RW)`
  — i.e. "does the user have a **raw, configured** `RW` grant on `_system`,
  ignoring server-wide read-only mode entirely" (the two-step dance exists
  only to make that true regardless of whether the server happens to be
  read-only right now).
  The current branch's `can(AdminMonitoring{})` →
  `AuthMode::Classic::check()` dispatches `AdminMonitoring` (one of the
  `auth::perms::AnyAdmin` alternatives, `arangod/Auth/Permissions.h:85,112,122`)
  through the generic case (`arangod/Auth/AuthMode.cpp:366-367`):
  ```cpp
  [&](p::AnyAdmin auto const&) -> Result { return isAdmin(); },
  ```
  and `isAdmin()` (`arangod/Auth/AuthMode.cpp:574-577`) is
  `check(UseDatabase{_system, Write})`, whose `UseDatabase` branch
  (`arangod/Auth/AuthMode.cpp:157-174`) computes
  `effectiveDatabaseAuthLevel(_system)` via
  `_userManager.databaseAuthLevel(username(), _system, /*configured=*/true)`
  (`arangod/Auth/AuthMode.cpp:104-106`) — **exactly** the same raw,
  configured, read-only-mode-*ignoring* level check `devel` ultimately
  reduces to. Crucially, this check is reached via `can()` directly, i.e.
  it does **not** go through any of the `ExecContext` wrapper methods
  (`canUseDatabase`, `canUseCollection`, ...) that were shown in the
  `RestDocumentHandler` section (Finding 1 there) to have an extra,
  unconditional `ServerState::readOnly()` short-circuit — so
  `RestMetricsHandler` is **not** affected by that regression: admin
  status for the hardened-API gate is correctly computed from the raw
  grant in both branches, independent of read-only mode, exactly as
  `devel` does.
- **Auth-disabled case**: with authentication fully disabled, `devel`'s
  `isAdminUser` defaults to `true` unconditionally
  (`devel:arangod/Utils/ExecContext.cpp:92,110`, the whole
  `af->isActive()` block is skipped). The current branch's
  `AuthMode::Disabled::check()` (`arangod/Auth/AuthMode.cpp:659-661`)
  likewise returns `{}` (success) for *any* permission unconditionally.
  Same result: hardened mode is a no-op when auth is disabled, in both
  branches.

So, for every combination of {`--server.harden` on/off} × {admin/non-admin
user} × {read-only mode on/off} × {auth enabled/disabled}, the **ALLOW/DENY
decision of `RestMetricsHandler` in `Classic` mode is identical to
`devel`**. This is also independently confirmed by the pre-existing
expectation table in `tests/api/apitests/server.mjs:100-110`, which
documents exactly this behaviour (`canUseHard(Monitoring)`:
`--server.harden=false` → all authenticated users pass;
`--server.harden=true` → only `RW` on `_system` passes).

### Finding 1 (Cosmetic, but arguably undermines the code's own stated intent): error message text differs, in the hardened-and-denied case

The only observable difference is the response body's error *message*
(never the `errorNum`/HTTP status, which stay `TRI_ERROR_FORBIDDEN`/403 in
both branches):

- `devel` calls `generateError(FORBIDDEN, TRI_ERROR_FORBIDDEN)` with no
  message argument at all, producing the generic, static "forbidden"
  message — deliberately, per the adjacent comment "don't leak information
  about server internals here".
- The current branch keeps the identical comment, but then passes
  `r.errorMessage()` as the third argument to `generateError`
  (`arangod/RestHandler/RestMetricsHandler.cpp:98-99`). Since `isAdmin()`
  fails via the `UseDatabase` branch of `AuthMode::Classic::check()`, that
  message is `"insufficient database access level for '_system'"`
  (`arangod/Auth/AuthMode.cpp:171-173`) — which mildly contradicts the very
  comment sitting right above it, by revealing that the admin gate is
  implemented as a check against the `_system` database specifically. This
  is a minor internal-detail disclosure (not a security-relevant one — any
  operator/reader of the documentation already knows "admin" means "`RW`
  on `_system`" in `Classic` mode), not a change in the ALLOW/DENY outcome,
  and is consistent with the "Cosmetic" category used throughout this
  document.

### Summary for `RestMetricsHandler`

| Route | Verdict |
|---|---|
| `GET /_admin/metrics[/v2]` (all parameter combinations: `serverId`, `type`, `mode`) | Identical to `devel` in every combination of harden on/off, admin/non-admin, read-only mode, auth disabled checked; **not** affected by the read-only-mode `ExecContext` regression found for `RestDocumentHandler`, since the admin check bypasses those wrapper methods entirely. Only the exact wording of the `403` error message differs when `--server.harden=true` and access is denied (Finding 1, cosmetic) |

**Action items / recommendations:**
1. No functional action required. Optionally, for defense-in-depth /
   message-parity with `devel` and the handler's own stated intent, drop
   the `r.errorMessage()` argument from the `generateError(...)` call in
   `RestMetricsHandler::executeAsync()` (or replace it with a fixed,
   non-revealing string) so the "don't leak information about server
   internals here" comment is actually honored, matching `devel`'s
   behaviour exactly.

## `RestCompactHandler` (`arangod/RestHandler/RestCompactHandler.cpp`)

Mounted at `PUT /_admin/compact` (exact route, no suffixes). This is one of
the simplest handlers examined so far: a single, hard-coded authorization
check, a method-type check, then a direct call into
`StorageEngine::compactAll()`.

Diffing `arangod/RestHandler/RestCompactHandler.cpp` against
`devel:arangod/RestHandler/RestCompactHandler.cpp` shows the *only*
behaviourally relevant change is the authorization check itself; the rest
of the diff is an unrelated internal refactor (the `StorageEngine&` is now
cached as a constructor-initialized member, obtained via
`DatabaseFeature::engine()`, instead of being looked up fresh via
`server().getFeature<EngineSelectorFeature>().engine()` on every call —
confirmed to return the exact same object, `arangod/RestServer/DatabaseFeature.h:191-194`).

`devel` (`devel:arangod/RestHandler/RestCompactHandler.cpp:29-34`):

```cpp
RestStatus RestCompactHandler::execute() {
  if (ExecContext::isAuthEnabled() && !ExecContext::current().isSuperuser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  "compaction is only allowed for superusers");
    return RestStatus::DONE;
  }
```

Current branch (`arangod/RestHandler/RestCompactHandler.cpp:45-50`):

```cpp
RestStatus RestCompactHandler::execute() {
  if (!ExecContext::current().isSuperuserOrDisabled()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                  "compaction is only allowed for superusers");
    return RestStatus::DONE;
  }
```

These two conditions are **logically equivalent**, and I confirmed this by
tracing both sides down to their primitive definitions rather than taking
the naming similarity at face value:

- `devel`'s reject condition is `isAuthEnabled() && !isSuperuser()`. If
  auth is globally disabled (`isAuthEnabled() == false`), the `&&`
  short-circuits to `false` (never reject); otherwise it rejects unless
  `isSuperuser()`.
- `devel`'s `ExecContext::isSuperuser()`
  (`devel:arangod/Utils/ExecContext.h:85-88`) is
  `isInternal() && systemAuthLevel()==RW && databaseAuthLevel()==RW` — note
  this requires `Type::Internal`, which an ordinary HTTP request (even from
  a fully-privileged admin user authenticated the normal way) never has
  (`Type::Default`). In practice this is only ever `true` for: the static
  `ExecContext::Superuser` singleton, or a request explicitly recognized as
  a superuser JWT (empty `preferred_username`) at `ExecContext::create()`
  time. A regular admin user (`RW` on `_system`, logged in with their own
  username) does **not** satisfy `isSuperuser()` in `devel` — this handler
  intentionally excludes even admins, matching its own error message
  literally ("...only allowed for **superusers**", not "...admins").
- Current branch's `ExecContext::isSuperuserOrDisabled()`
  (`arangod/Utils/ExecContext.h:87-90`) is
  `_authMode.isSuperuser() || _authMode.isDisabled()`.
  `AuthMode::isSuperuser()` (`arangod/Auth/AuthMode.cpp:52-54`) is
  `std::holds_alternative<Superuser>(authMode)`; the `Superuser` variant is
  constructed at `ExecContext::create()` time under the *exact* same
  condition as `devel`'s special-case (`req.authenticated() &&
  req.user().empty() && ... == JWT`, `arangod/Utils/ExecContext.cpp:82-90`),
  or via the static `ExecContext::Superuser` singleton / an explicit
  `forceSuperuser()`/`ExecContextSuperuserScope` escalation. A regular
  `Classic`-mode admin user is represented by `AuthMode::Classic`, for
  which `isSuperuser()` is `false` — same exclusion as `devel`.
  `AuthMode::isDisabled()` is `true` exactly when
  `!authenticationFeature.isActive()` (`arangod/Utils/ExecContext.cpp:92-93`),
  the same condition as `devel`'s `!isAuthEnabled()`.
- So `!isSuperuserOrDisabled()` = `!isSuperuser() && !isDisabled()` =
  (rewriting `isDisabled()` as `!isAuthEnabled()`) `!isSuperuser() &&
  isAuthEnabled()` = `isAuthEnabled() && !isSuperuser()` — **identical** to
  `devel`'s reject condition, term for term.

I also checked that `/_admin/compact` is not among any of the path-based
exceptions in either branch's pre-handler access-control layer (`devel`'s
`CommTask::canAccessPath()` / the current branch's
`RestHandler::checkUserCanAccess()`), so an unauthenticated request is
rejected with `401` before this check is ever reached in both branches, and
`RestCompactHandler` itself has no `checkUserCanAccess()` override — the
comparison above is exhaustive.

The subsequent method-type check (`PUT` only →
`TRI_ERROR_HTTP_METHOD_NOT_ALLOWED` otherwise) and the call into
`engine.compactAll(changeLevel, compactBottomMostLevel)` are byte-for-byte
identical in both branches, including the exact error message text on
failure.

### Summary for `RestCompactHandler`

| Route | Verdict |
|---|---|
| `PUT /_admin/compact` | **Identical to `devel`** — same reject condition (`isAuthEnabled() && !isSuperuser()`, restated but logically unchanged as `!isSuperuserOrDisabled()`), same error code/message, same method-type check, same underlying `compactAll()` call. No differences of any kind (not even cosmetic) found. |

**Action items / recommendations:**
None. This handler required no changes; it is a clean, faithful
reformulation of the exact same authorization logic as `devel`.

## `RestAdminServerHandler` (`arangod/RestHandler/RestAdminServerHandler.cpp`)

Mounted at `/_admin/server` (prefix), dispatching on a single suffix:
`mode`, `id`, `role`, `availability`, `databaseDefaults`, `tls`, `jwt`,
`encryption`, `api-calls`, `aql-queries`. Each sub-route has its own,
independent authorization logic (or none beyond the generic
"must be authenticated" gate). `arangod/RestHandler/RestAdminServerHandler.h`
diffs only in a removed `@author` comment and two new cached members
(`_engine`, `_apiRecordingFeature` — an unrelated internal refactor, same
as seen in `RestCompactHandler`/`RestMetricsHandler`) plus the new
`checkUserCanAccess()` override discussed in Finding 1.

### Finding 1 (New, deliberate, verified equivalent): re-implementation of `devel`'s `/_admin/server/availability` "OPEN access" path exception

This is the first handler encountered so far that plugs the gap flagged in
this document's architectural background section ("not all of `devel`'s
path-based exceptions were carried over into the generic
`RestHandler::checkUserCanAccess()`") — and it turns out to have been
addressed correctly, at the handler level.

`devel` grants unauthenticated access to `/_admin/server/availability`
centrally, in `CommTask::canAccessPath()`
(`devel:arangod/GeneralServer/CommTask.cpp:848-852`):

```cpp
if (path == "/" || path.starts_with(::pathPrefixOpen) ||
    path.starts_with(::pathPrefixAdminAardvark) ||
    path == "/_admin/server/availability") {
  // mop: these paths are always callable...
  result = Flow::Continue;
  vc->forceSuperuser();
}
```

The current branch has no such path-based logic left in `CommTask`, but
`RestAdminServerHandler` now overrides `checkUserCanAccess()`
(`arangod/RestHandler/RestAdminServerHandler.cpp:82-92`) to reproduce
exactly this exception, locally:

```cpp
async<Result> RestAdminServerHandler::checkUserCanAccess() const {
  auto const& suffixes = _request->suffixes();
  if (suffixes.size() == 1 && suffixes[0] == "availability") {
    auto ec = _request->requestContext();
    TRI_ASSERT(ec != nullptr);
    ec->forceSuperuser();
    co_return Result{};
  }
  co_return co_await RestBaseHandler::checkUserCanAccess();
}
```

This matches `devel` in every relevant respect: it fires only for the
exact, single-suffix `/_admin/server/availability` path (mirroring
`devel`'s exact-string `path ==` comparison — a request to e.g.
`/_admin/server/availability/x`, if such a suffix were ever accepted, would
*not* match `suffixes.size() == 1`, falling through to the normal
authenticated path in both branches), it returns success **unconditionally**
regardless of whether the request carries valid credentials (matching
`devel`'s `Flow::Continue` before any authentication check is performed),
and it calls `forceSuperuser()` to upgrade the request's execution context
so that the handler body — which performs no further authorization checks
of its own — can run to completion. This is confirmed correct by the
pre-existing test-suite comment
(`tests/api/apitests/server.mjs:189-195`): "Auth: OPEN – no authentication
required at all", with expected results `AU→200 or 503` for all identity
columns including the fully-unauthenticated one.

One latent (currently inconsequential) difference worth flagging for
future sessions: `devel`'s `VocbaseContext::forceSuperuser()`
(`devel:arangod/RestServer/VocbaseContext.cpp:136-146`) special-cases
global read-only mode by degrading to `forceReadOnly()` (`Type::Internal`,
`RO`/`RO` levels) instead of granting full `RW`/`RW` superuser, whereas the
current branch's `ExecContext::forceSuperuser()`
(`arangod/Utils/ExecContext.cpp:118-128`) unconditionally resets to full
`AuthMode::Superuser` (which always answers every permission check with
success, `arangod/Auth/AuthMode.cpp:64-66`) regardless of
`ServerState::readOnly()`. For `/_admin/server/availability` specifically
this makes no observable difference, because `handleAvailability()`
performs no further authorization decision that could distinguish "full
superuser" from "read-only superuser" — both simply let the handler run to
completion and report the server's health/mode. It could matter for some
other, not-yet-reviewed caller of `forceSuperuser()`/`ec->forceSuperuser()`
that relies on the resulting context still being *read-only* rather than
fully read-write after the call; worth keeping in mind for future sessions
(no concrete instance of this mattering has been found yet).

### Finding 2 (Cosmetic): `PUT /_admin/server/mode` — refactored admin check, same decision, different `errorNum`/message on failure

`devel` (`devel:arangod/RestHandler/RestAdminServerHandler.cpp:191-206`)
hand-rolls the admin check:

```cpp
AuthenticationFeature* af = AuthenticationFeature::instance();
if (af->isActive() && !_request->user().empty()) {
  auth::Level lvl;
  if (af->userManager() != nullptr) {
    lvl = af->userManager()->databaseAuthLevel(
        _request->user(), StaticStrings::SystemDatabase, /*configured*/ true);
  } else {
    lvl = auth::Level::RW;
  }
  if (lvl < auth::Level::RW) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);  // no message, errorNum 11
    return;
  }
}
```

Current branch (`arangod/RestHandler/RestAdminServerHandler.cpp:200-207`):

```cpp
if (auto r = ExecContext::current().canUseAdminAction(
        auth::perms::AdminMaintenance{});
    r.fail()) {
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                r.errorMessage());  // errorNum 403, with message
  return;
}
```

I confirmed these produce the **same ALLOW/DENY decision** in every case:
`devel`'s hand-rolled check reads
`databaseAuthLevel(user, _system, /*configured=*/true)` directly — the
raw, configured level, ignoring `ServerState::readOnly()` — and skips the
check entirely when `!af->isActive()` (auth disabled) or `_request->user()`
is empty (superuser JWT). `canUseAdminAction(AdminMaintenance{})` → `can()`
→ `AuthMode::Classic::check()`'s generic `AnyAdmin` branch
(`AdminMaintenance` is in the `AdminList`, `arangod/Auth/Permissions.h:98,111-118`)
→ `isAdmin()` (`arangod/Auth/AuthMode.cpp:574-577`) → the very same
`databaseAuthLevel(user, _system, /*configured=*/true)` call
(`arangod/Auth/AuthMode.cpp:104-106`); the auth-disabled and superuser-JWT
bypasses are likewise reproduced by `AuthMode::Disabled::check()`
(always `{}`) and `AuthMode::Superuser::check()` (always `{}`)
respectively. Since this goes through `canUseAdminAction()`, not one of the
`ExecContext` wrapper methods with the read-only-mode short-circuit bug
documented in the `RestDocumentHandler` section, it is also **not**
affected by that regression — same as the `RestMetricsHandler` admin check.

The only observable difference is in the failure response body:
- `errorNum`: `TRI_ERROR_FORBIDDEN` (`11`, `devel`) vs.
  `TRI_ERROR_HTTP_FORBIDDEN` (`403`, current branch) — note this is a
  genuine numeric change in the JSON body's `errorNum` field, not merely a
  message-wording change. However, unlike the `TRI_ERROR_ARANGO_READ_ONLY`
  vs. `TRI_ERROR_FORBIDDEN` case found for `RestDocumentHandler` (Finding 1
  there), neither `11` nor `403` carries any distinguishing semantic
  meaning beyond "generic forbidden" (both error definitions literally say
  `"forbidden"` in `lib/Basics/errors.dat:16,49`) — there is no
  actionable distinction a client could reasonably make between them.
  Classified as **Cosmetic** per this document's convention, but the exact
  numeric change is called out explicitly here in case some test asserts
  on it.
- Message text: none (`devel`) vs.
  `"insufficient database access level for '_system'"` (current,
  `arangod/Auth/AuthMode.cpp:171-173`) — same pattern as the
  `RestMetricsHandler` Finding 1 (mildly reveals that the admin gate is a
  `_system`-database check, but not security-relevant).
- HTTP status code is unchanged: `403` (`rest::ResponseCode::FORBIDDEN`) in
  both branches, passed explicitly to `generateError()`.

### Finding 3 (Verified equivalent): `PUT /_admin/server/tls` (reload) and the `onlySuperUser` branches of `api-calls`/`aql-queries` — `isSuperuser()` vs. `isSuperuserOrDisabled()`

Same pattern already fully analyzed in the `RestCompactHandler` section
above, appearing three more times in this handler:

- `handleTLS()`, `POST` (`arangod/RestHandler/RestAdminServerHandler.cpp:279`
  vs. `devel:...RestAdminServerHandler.cpp:279`): `devel`'s
  `ExecContext::isAuthEnabled() && !ExecContext::current().isSuperuser()`
  → current's `!ExecContext::current().isSuperuserOrDisabled()`.
- `handleApiCalls()`/`handleAqlRecordedQueries()`, `onlySuperUser()` branch
  (`arangod/RestHandler/RestAdminServerHandler.cpp:325,380`): `devel`'s
  bare `!ExecContext::current().isSuperuser()` (**without** an
  `isAuthEnabled()` guard this time) → current's
  `!ExecContext::current().isSuperuserOrDisabled()`.

As established for `RestCompactHandler`, `AuthMode::isSuperuser()` and
`devel`'s `ExecContext::isSuperuser()` (`isInternal() && systemLevel==RW &&
dbLevel==RW`, effectively "is this a genuine superuser-JWT/internal
request") are constructed under identical conditions in both branches. The
two call sites above that lack `devel`'s `isAuthEnabled()` guard are not a
concern for the `Classic`-mode comparison in scope here: `AuthMode::Classic`
is only ever selected when authentication is active in the first place
(`arangod/Utils/ExecContext.cpp:92-110`), so within `Classic` mode
`isDisabled()` is always `false` and `isSuperuserOrDisabled()` reduces to
plain `isSuperuser()` — matching `devel`'s bare check exactly. (The
auth-fully-disabled configuration is out of scope for this document, per
the task description — see also the equivalent note in the
`RestCollectionHandler` section.) Verified equivalent, no regression.

### Finding 4 (Cosmetic): `api-calls`/`aql-queries`, non-`onlySuperUser` branch — `isAdminUser()` boolean replaced by `canUseAdminAction()` Result

`devel` (`devel:arangod/RestHandler/RestAdminServerHandler.cpp:333-337,388-392`):

```cpp
if (!ExecContext::current().isAdminUser()) {
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                "You need admin rights for recording API operations");
  return;
}
```

Current branch (`arangod/RestHandler/RestAdminServerHandler.cpp:331-337,386-392`):

```cpp
if (auto r = ExecContext::current().canUseAdminAction(
        auth::perms::AdminApiCalls{} /* or AdminAqlQueries{} */);
    r.fail()) {
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                r.errorMessage());
  return;
}
```

`devel`'s `isAdminUser()` and the current branch's `canUseAdminAction()` →
`isAdmin()` were already shown (Finding 2 above, and previously for
`RestMetricsHandler`) to reduce to the exact same "raw, configured `RW` on
`_system`" check. The only difference is, again, the message text (a fixed
string in `devel` vs. `r.errorMessage()` = `"insufficient database access
level for '_system'"` in the current branch) — the `errorNum`
(`TRI_ERROR_HTTP_FORBIDDEN`, `403`) is identical in both branches this
time (both already used it, unlike Finding 2's `mode` route). Purely
cosmetic; no change in outcome.

### Finding 5 (Revised — verified equivalent): Enterprise-only `handleJWTSecretsReload`/`handleEncryptionKeyRotation`

**Correction to earlier session:** the `enterprise/` directory turns out to
be a full checkout of the Enterprise-Edition repository (own git history,
same branch names), so the EE implementations *can* be compared after all.
`arangod/RestHandler/RestAdminServerHandler.cpp:299-307` provides only the
Community-Edition stub (`404 NOT_FOUND`) when `USE_ENTERPRISE` is not
defined — identical in both branches, not interesting on its own. The real
logic lives in
`enterprise/Enterprise/RestHandler/RestAdminServerHandlerEE.cpp`, compiled
in instead of the stub when building the EE server.

Diffing `enterprise/Enterprise/RestHandler/RestAdminServerHandlerEE.cpp`
against `enterprise` repo's own `devel:Enterprise/RestHandler/RestAdminServerHandlerEE.cpp`
shows exactly two authorization-relevant lines changed, both following the
same pattern already proven equivalent for `RestCompactHandler` and
`RestAdminServerHandler`'s `handleTLS()`/`api-calls`/`aql-queries` routes:

`devel` (`POST /_admin/server/jwt` and `POST /_admin/server/encryption`,
`enterprise/devel:Enterprise/RestHandler/RestAdminServerHandlerEE.cpp:96-97,148-149`):
```cpp
if (_request->requestType() == RequestType::POST &&
    ExecContext::isAuthEnabled() && !ExecContext::current().isSuperuser()) {
```

Current branch (`enterprise/Enterprise/RestHandler/RestAdminServerHandlerEE.cpp:96-97,148-149`):
```cpp
auto const& execContext = ExecContext::current();
if (_request->requestType() == RequestType::POST &&
    !execContext.isSuperuserOrDisabled()) {
```

As established previously, `!isSuperuserOrDisabled()` ≡ `isAuthEnabled() &&
!isSuperuser()` term-for-term (`isSuperuserOrDisabled()` =
`isSuperuser() || isDisabled()`, and `isDisabled()` ≡ `!isAuthEnabled()`).
So both the JWT-secrets-reload and encryption-key-rotation POST routes make
the identical ALLOW/DENY decision as `devel` in `Classic` mode: only a
superuser (or an auth-disabled deployment) may `POST`; any authenticated
user (including a full admin) may still `GET` both routes to read hashes/
metadata, exactly as in `devel`.

The remaining diff hunks (`RestServer/DatabaseFeature.h` include instead of
`StorageEngine/EngineSelectorFeature.h`, and
`server().getFeature<DatabaseFeature>().engine()` instead of
`server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>()` for
`handleEncryptionKeyRotation()`) are the same unrelated engine-lookup
refactor already seen and confirmed equivalent in `RestCompactHandler` and
`RestAdminServerHandler.cpp`'s own constructor — both resolve to the same
underlying `RocksDBEngine&`/`nullptr` outcome, just obtained via a
different accessor.

**Verdict: fully equivalent to `devel`, no regression.** This supersedes
the "out of scope" classification from the previous session.

### Summary for `RestAdminServerHandler`

| Route | Verdict |
|---|---|
| `GET /_admin/server/mode` | Identical to `devel` (`AUTHEN`, no per-user check) |
| `PUT /_admin/server/mode` | Same ALLOW/DENY decision as `devel`; `errorNum`/message differ on failure (Finding 2, cosmetic) |
| `GET /_admin/server/id` | Identical to `devel` |
| `GET /_admin/server/role` | Identical to `devel` |
| `GET /_admin/server/availability` | Identical to `devel` — `devel`'s central, path-based "OPEN access" exception is faithfully reproduced via a new handler-level `checkUserCanAccess()` override (Finding 1) |
| `GET /_admin/server/databaseDefaults` | Identical to `devel` (`AUTHEN`) |
| `GET /_admin/server/tls` | Identical to `devel` (`AUTHEN`) |
| `POST /_admin/server/tls` | Identical to `devel` (Finding 3) |
| `GET/POST /_admin/server/jwt`, `/_admin/server/encryption` | Identical to `devel` — EE implementation verified equivalent (Finding 5) |
| `GET /_admin/server/api-calls` | Identical to `devel` in both the `onlySuperUser` (Finding 3) and admin-check (Finding 4, cosmetic) modes |
| `GET /_admin/server/aql-queries` | Same as `api-calls` (Findings 3, 4) |

**Action items / recommendations:**
1. No functional action required anywhere in this handler — every route
   produces the same ALLOW/DENY decision as `devel`.
2. Optional, low-priority, for message parity with `devel`: consider
   omitting `r.errorMessage()` (Findings 2 and 4) if exact error-message
   parity with `devel` is desired, and/or aligning the `mode` route's
   `errorNum` back to `TRI_ERROR_FORBIDDEN` (Finding 2) for consistency
   with the other routes in the same handler that already use
   `TRI_ERROR_HTTP_FORBIDDEN`.
3. Keep the `forceSuperuser()` read-only-mode nuance (Finding 1) in mind
   for future sessions: the current branch's version is strictly more
   permissive (`RW` instead of `devel`'s `RO`-during-global-read-only) in
   any future caller that both (a) relies on `forceSuperuser()` and (b)
   makes a decision that distinguishes `RO` from `RW`. No such caller has
   been identified yet.

## `RestWalAccessHandler` (`arangod/RestHandler/RestWalAccessHandler.cpp`)

Mounted at `/_api/wal` (prefix; sub-routes `range`, `lastTick`, `tail`,
`open-transactions`). This handler exposes the raw write-ahead-log to
callers, primarily for replication tailing (leader/follower shard sync,
`arangosync`, external backup/replication tools).

### Single server vs. cluster

`RestWalAccessHandler::execute()` (`arangod/RestHandler/RestWalAccessHandler.cpp:186-191`)
starts with:
```cpp
if (ServerState::instance()->isCoordinator()) {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED,
                TRI_ERROR_CLUSTER_UNSUPPORTED,
                "'/_api/wal' is not yet supported in a cluster");
  return RestStatus::DONE;
}
```
This is **byte-for-byte identical** in both branches (confirmed via diff —
it is untouched code). Consequently:
- On a **Coordinator**, `/_api/wal/*` always fails with `501
  NOT_IMPLEMENTED` before any authorization check runs at all — identical
  behavior in both branches, and there is **no request forwarding** to a
  DB-Server for this particular route (unlike, e.g., the `dump`
  batch-management routes of `RestReplicationHandler`, which *do* support a
  `DBserver` query-parameter forwarding mechanism — see the `DBserver`
  forwarding notes in `OpenAPI/0-openapi.json:28202` and
  `Documentation/path_permissions.md:1071-1076`, which explicitly marks
  `/_api/wal/*` as "only DBServer/Single, not on coord").
- On a **single server or a DB-Server**, execution falls through to the
  same code, unconditionally — the handler makes no further distinction
  between the two deployment modes. So everything below applies identically
  to both.
- **Cluster-internal replication traffic** (leader→follower shard
  synchronization, `arangod/Cluster/SynchronizeShard.cpp:1722-1725`) reaches
  a DB-Server's `/_api/wal/tail` directly (not via the Coordinator) and
  authenticates using the server's own internal JWT secret
  (`AuthenticationFeature::tokenCache().jwtToken()`), with **no
  `preferred_username`/user field**. `ExecContext::create()`
  (`arangod/Utils/ExecContext.cpp:82-90`) recognizes exactly this pattern
  (`req.authenticated() && req.user().empty() && ... == JWT`) and
  constructs an `AuthMode::Superuser` context for it — which trivially
  passes every check in `AuthMode::Superuser::check()`
  (`arangod/Auth/AuthMode.cpp:64-66`, unconditionally returns `{}`). This
  part is unaffected by anything below, in both branches.

### Finding 1 (Cosmetic): top-level admin gate

`devel` (`devel:arangod/RestHandler/RestWalAccessHandler.cpp:193-196`):
```cpp
if (!_context.isAdminUser()) {
  generateError(ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
  return RestStatus::DONE;
}
```
Current branch (`arangod/RestHandler/RestWalAccessHandler.cpp:193-198`):
```cpp
if (auto r = ExecContext::current().canUseAdminAction(
        auth::perms::AdminWalAccess{});
    r.fail()) {
  generateError(r);
  return RestStatus::DONE;
}
```
`canUseAdminAction(AnyAdmin)` dispatches to `AuthMode::Classic::check()`'s
`[&](p::AnyAdmin auto const&) { return isAdmin(); }` branch
(`arangod/Auth/AuthMode.cpp:367`), and `isAdmin()` checks `UseDatabase{
_system, Write}` (`arangod/Auth/AuthMode.cpp:574-577`) — the same "RW on
`_system`" test as `devel`'s precomputed `_isAdminUser` flag (already
established equivalent in the `RestMetricsHandler`/`RestAdminServerHandler`
sessions). Same ALLOW/DENY decision. On failure, `devel` returns bare
`TRI_ERROR_FORBIDDEN` with no message; the current branch returns the same
`errorNum` (`TRI_ERROR_FORBIDDEN`, since `requestedApiVersion() == 0` for
this classic route) but with an added message (e.g. `"insufficient
database access level for '_system'"`). Cosmetic only.

### Finding 2 (Regression): missing superuser escalation during `handleCommandTail`

This is the significant finding for this handler. `devel`
(`devel:arangod/RestHandler/RestWalAccessHandler.cpp:316`) contains, right
before the actual WAL-tailing call:
```cpp
ExecContextSuperuserScope escope(ExecContext::current().isAdminUser());
```
Because `execute()` already required `_context.isAdminUser()` to be `true`
to even reach `handleCommandTail()`, this condition is always `true` at
this point — so `devel` **unconditionally escalates `ExecContext::CURRENT`
to a full superuser** for the remainder of the tailing operation. The
current branch's `handleCommandTail()`
(`arangod/RestHandler/RestWalAccessHandler.cpp:274-412`) **does not contain
this line at all** — it was dropped without replacement during the
refactor.

This matters because `wal->tail(...)` (`RocksDBWalAccess`, driven by
`WalAccessContext`) filters every WAL marker through
`WalAccessContext::shouldHandleCollection()`
(`arangod/StorageEngine/WalAccess.cpp:53-69`), which — for every
collection-scoped marker — calls `loadCollection(dbid, cid)`
(`arangod/StorageEngine/WalAccess.cpp:87-101`). That, in turn, constructs a
`CollectionGuard(vocbase, cid)` (`arangod/Utils/CollectionGuard.h:49-54`),
whose constructor calls `vocbase->useCollection(cid, /*checkPermissions*/
true)`, which flows into `Database::loadCollection(collection,
checkPermissions=true)` (`arangod/VocBase/vocbase.cpp:387-403` in current
branch, `devel:arangod/VocBase/vocbase.cpp:396-410` — **this file is
identical in both branches**, confirmed by diff):
```cpp
if (checkPermissions) {
  std::string const& dbName = _info.getName();
  if (auto r = ExecContext::current().canUseCollection(
          dbName, collection.name(), AccessLevel::Read);
      !r.ok()) {
    return r;
  }
}
```
This calls straight into `AuthMode::Classic::check()`'s `UseCollection`
branch (`arangod/Auth/AuthMode.cpp:176-251`), which — unlike
`DumpCollection`/`RestoreCollection`/`RestoreCreateIndex`/etc. in the very
same function — has **no `isAdmin()` bypass**. It purely consults
`UserManagerBase::collectionAuthLevel(user, db, coll, /*configured*/
true)`, i.e. the caller's actual, specifically-granted permissions for that
exact database/collection pair.

Consequence:
- In `devel`, because of the unconditional superuser escalation, **every**
  collection in **every** database is visible during `/_api/wal/tail`
  streaming to any user who passed the initial `isAdminUser()` gate,
  regardless of that user's per-database/per-collection grants elsewhere.
  This matches the documented design intent recorded in
  `Documentation/path_permissions.md:1071-1076`, which lists all
  `/_api/wal/*` routes as requiring `SUPER`-level access with an empty
  "Changes to before RBAC" column (i.e. **no behavioral change was
  intended** for this handler by the RBAC migration).
- In the current branch, without the escalation, `ExecContext::current()`
  inside `Database::loadCollection` is the **real, unescalated** admin
  identity. An admin (RW on `_system`) who does **not** additionally hold
  an explicit (or wildcard `*`) grant on a collection in some *other*
  database will have `canUseCollection(..., Read)` **fail** for that
  collection during tailing. Critically, this failure is **not
  surfaced as an error** to the client: `WalAccessContext::loadCollection()`
  swallows the resulting exception in a bare `catch (...) { // weglaecheln
  }` (`arangod/StorageEngine/WalAccess.cpp:93-99`) and returns `nullptr`,
  which causes `shouldHandleCollection()` to simply skip that marker. The
  practical effect is **silent, partial data loss**: the tailing/dump
  response is missing entries for collections the admin doesn't explicitly
  have rights on, with no indication to the client that anything was
  filtered.
- Database-level markers (database create/drop) and view-level markers are
  **not** affected by this regression: `shouldHandleDB()`
  (`arangod/StorageEngine/WalAccess.cpp:33-35`) and the view lookup path
  perform no permission check at all in either branch (`loadVocbase()` /
  `vocbase->lookupView()` are auth-agnostic). Only *collection-scoped*
  markers are impacted.
- As noted above, **cluster-internal shard-replication traffic is
  unaffected** by this regression, because it already authenticates as
  `AuthMode::Superuser` via the internal JWT — the missing escalation is a
  no-op for callers who are already superuser. The regression's practical
  blast radius is therefore limited to **direct, named-user calls** to
  `/_api/wal/tail` (e.g. an external backup/replication tool, or manual
  administrative use) on a single server or directly against a DB-Server's
  endpoint — on both of those deployment modes identically, since (as
  established above) the handler code does not distinguish between them.

This is best classified as an unintentional **Regression**: the project's
own migration-tracking document (`Documentation/path_permissions.md`)
explicitly records "no change" as the target for this route, and the
codebase elsewhere (`ExecContext::canDumpCollection`,
`arangod/Utils/ExecContext.h:169-174`) demonstrates the established
convention for this exact "admin should see everything, even
without per-collection grants, for infra/backup-style operations"
use case — but that convention was never wired up to replace the
deleted `ExecContextSuperuserScope` in this handler.

### Finding 3 (Cosmetic): unrelated engine/feature-lookup refactor

The remaining diff between branches is entirely non-authorization:
- `RestWalAccessHandler` now caches `DatabaseFeature&` as a member
  (`_databaseFeature`, set in the constructor) instead of looking it up via
  `server().getFeature<DatabaseFeature>()` at each call site in
  `handleCommandTail()`.
- `StorageEngine& engine = _vocbase.engine();` replaces `server().getFeature<
  EngineSelectorFeature>().engine()` in `execute()` — same underlying engine
  instance, different accessor (same pattern already confirmed equivalent
  for `RestAdminServerHandler`/`RestCompactHandler`).

No behavioral impact.

### Summary for `RestWalAccessHandler`

| Route | Verdict |
|---|---|
| Any route, on a Coordinator | Identical to `devel`: unconditional `501 NOT_IMPLEMENTED`, no forwarding, no auth check reached |
| `GET /_api/wal/range` | Identical to `devel` (top-level admin gate only, Finding 1 cosmetic) |
| `GET /_api/wal/lastTick` | Identical to `devel` (Finding 1 cosmetic) |
| `GET /_api/wal/open-transactions` | Identical to `devel` (Finding 1 cosmetic; no per-collection filtering in this deprecated route) |
| `DELETE /_api/wal/tail`, `PUT /_api/wal/tail?trackOnly=true` | Identical to `devel` (client (un)registration only, no `wal->tail()` call, unaffected by Finding 2) |
| `GET/PUT /_api/wal/tail` (actual tailing) | **Regression** (Finding 2): admins without explicit per-database/per-collection grants elsewhere now silently miss WAL entries for those collections; internal cluster shard-replication traffic is unaffected |

**Action items / recommendations:**
1. **Fix Finding 2**: reinstate an equivalent bypass for the duration of
   `handleCommandTail()`'s `wal->tail(...)` call — either restore
   `ExecContextSuperuserScope` (currently `[[deprecated]]`,
   `arangod/Utils/ExecContext.h:272-287`, but still functionally correct),
   or — preferably, to avoid the deprecated API — change
   `Database::loadCollection()`'s permission check (or add an
   admin/dump-style bypass reachable from `WalAccessContext::loadCollection()`)
   to use the same "RW-on-`_system`-bypasses-everything" convention already
   established for `canDumpCollection()`
   (`arangod/Utils/ExecContext.h:169-174`), since `/_api/wal/tail` is
   conceptually the same kind of infra/backup operation.
2. No action required for Finding 1 (cosmetic) or Finding 3 (cosmetic,
   unrelated refactor).

### Addendum: `RocksDBRestWalHandler` and `ClusterRestWalHandler`

These two are **not** subclasses of `RestWalAccessHandler` — they are
independent handlers (both extend `RestBaseHandler` directly) mounted at
the completely different path `/_admin/wal` (prefix route, registered in
`arangod/RocksDBEngine/RocksDBRestHandlers.cpp:44` for single-server/
DBServer, and `arangod/ClusterEngine/ClusterRestHandlers.cpp:41` for the
Coordinator). They expose four sub-operations:
`transactions` (GET), `flush` (PUT), `properties` (GET/PUT), and
`wait_for_estimator_sync` (PUT). Confirmed via
`Documentation/path_permissions.md:857-866` (a design doc present only on
this branch, added as new documentation — it does not exist on `devel` at
all, `git show devel:Documentation/path_permissions.md` fails).

Both `RocksDBRestWalHandler::execute()`
(`arangod/RocksDBEngine/RocksDBRestWalHandler.cpp:46-101`) and
`ClusterRestWalHandler::execute()`
(`arangod/ClusterEngine/ClusterRestWalHandler.cpp:46-101`) are otherwise
byte-for-byte identical to `devel` except for:

1. Two added comments (`// Mounted at /_admin/wal ...`) — cosmetic.
2. `ClusterRestWalHandler.cpp` drops an unused `#include
   "Cluster/ServerState.h"` — cosmetic, confirmed via diff that
   `ServerState` is not referenced anywhere in this file in either branch.
3. The `wait_for_estimator_sync` admin gate, but **only** inside the
   `#else` branch of `#ifndef ARANGODB_ENABLE_MAINTAINER_MODE` (i.e. only
   reachable in maintainer builds) changed from:
   ```cpp
   if (!ExecContext::current().isAdminUser()) { ... }
   ```
   to:
   ```cpp
   if (auto r = ExecContext::current().canUseAdminAction(
           auth::perms::AdminWalAccess{});
       r.fail()) { ... }
   ```
   This is the **exact same `isAdminUser()` → `canUseAdminAction(AnyAdmin)`
   pattern already analyzed and classified as cosmetic-only in Finding 1
   above** — `canUseAdminAction(AnyAdmin)` dispatches to
   `AuthMode::Classic::check()`'s `isAdmin()` branch
   (`arangod/Auth/AuthMode.cpp:367`), which is the same "RW on `_system`"
   test as `devel`'s `_isAdminUser` flag. Same ALLOW/DENY decision; only
   the error message text differs. The `#ifndef
   ARANGODB_ENABLE_MAINTAINER_MODE` branch (`!isSuperuser()` → full
   superuser required) is untouched in both files, in both branches.
4. `flushWalOnAllDBServers()`/`ClusterAdminOperations.cpp` (the function
   backing both `flush()` implementations' cluster-wide fan-out) is
   byte-for-byte identical between branches (confirmed via diff) and
   performs cluster-internal RPCs to DB-Servers via the network pool —
   the same superuser-JWT internal-authentication pattern already
   established for `compactOnAllDBServers()`/`RestCompactHandler`. No new
   finding.

**The three remaining sub-routes — `transactions`, `flush`, and
`properties` — have zero handler-local authorization code in either
branch** (confirmed: no `ExecContext`/`canUse*`/`auth::` reference
anywhere near these three functions in either `devel` or the current
branch). This matches `Documentation/path_permissions.md:857-869`, which
records their designed access level as merely `AUTHEN` (any authenticated
user) rather than `SUPER`/`ADMIN` — i.e. this is **documented, intentional,
unchanged-since-`devel` behavior**, not a divergence introduced by this
branch. (Whether "any authenticated user can trigger a cluster-wide WAL
flush or introspect running-transaction counts" is itself a *good* design
choice is outside this document's stated scope, since it is identical in
both branches.)

**Conclusion:** no findings distinct from the parent `RestWalAccessHandler`
session — the one behavioral change present (`wait_for_estimator_sync`'s
gate) is the same already-classified-cosmetic `isAdminUser()` →
`canUseAdminAction(AnyAdmin)` refactor pattern seen throughout this
codebase. No action items.


## `RestOptions*` handler family

This session covers all four `RestHandler`s built around program-options
introspection, since three of them share a common base class and the fourth
is its sibling:

- `RestOptionsBaseHandler` (`arangod/RestHandler/RestOptionsBaseHandler.cpp`)
  — abstract base, provides `checkAuthentication()`.
- `RestOptionsHandler` (`arangod/RestHandler/RestOptionsHandler.cpp`) —
  `GET /_admin/options` (full option values, including sensitive ones).
- `RestOptionsDescriptionHandler`
  (`arangod/RestHandler/RestOptionsDescriptionHandler.cpp`) —
  `GET /_admin/options-description` (option metadata/schema).
- `RestPublicOptionsHandler`
  (`arangod/RestHandler/RestPublicOptionsHandler.cpp`) —
  `GET /_admin/options-public` (filtered, non-sensitive option values;
  always registered regardless of `--server.options-api` policy).

`RestOptionsHandler` and `RestOptionsDescriptionHandler` are structurally
trivial (method check, call `checkAuthentication()`, produce a
`VPackBuilder`); all of their authorization logic lives in the shared
`RestOptionsBaseHandler::checkAuthentication()`. `RestPublicOptionsHandler`
does **not** call `checkAuthentication()` at all (by design, in both
branches) since it's meant to be reachable regardless of the
`--server.options-api` policy setting.

Diffing all four `.cpp`/`.h` files against `devel` shows only comment-only
changes (removal of `@author` lines, addition of `// Mounted at ...`
annotations) plus the two functional diffs discussed below.

### Background: the generic pre-handler gate applies here too

As established in the `RestDatabaseHandler` session, **every** request (in
both branches) passes through a generic, handler-independent authorization
gate before the handler's `execute()`/`executeAsync()` ever runs, unless the
handler overrides it. None of the four `RestOptions*` handlers override this
gate (no `checkUserCanAccess()` override is declared in any of their
headers), so the following applies uniformly to `/_admin/options`,
`/_admin/options-description`, and `/_admin/options-public` alike, in both
branches:

- `devel`: `CommTask::canAccessPath()` (`/tmp/devel_CommTask.cpp:787-876`)
  requires `req.authenticated()` and rejects if
  `vc->databaseAuthLevel() == auth::Level::NONE` for the request's target
  database — none of the path-based exceptions further down in that function
  (`/`, `pathPrefixOpen`, `pathPrefixAdminAardvark`,
  `/_admin/server/availability`, `/_api/cluster/endpoints`,
  `pathPrefixApiUser`, `pathPrefixApiToken`) match any `/_admin/options*`
  path, so none of them apply here.
- Current branch: `RestHandler::checkUserCanAccess()`
  (`arangod/GeneralServer/RestHandler.cpp:705-764`) requires
  `request()->authenticated()` and rejects if
  `ec->canUseDatabase(request()->databaseName(), DatabaseAccessLevel::Read).fail()`
  — algebraically the same condition (`Read` requires `>= RO`, i.e. blocks
  only `NONE`), and again none of its own exceptions (UNIX-socket bypass,
  `authenticationSystemOnly()` Foxx-app bypass) apply to these three paths.

In other words: **any request to any of these three routes must already be
authenticated with at least `RO` access to its target database before the
handler-specific code below is ever reached**, identically in both branches.
This matters directly for Finding 1 below.

### Finding 1 (Verified NOT a regression — dead-code removal):
`RestPublicOptionsHandler`'s in-handler database-access check was deleted

```diff
   VPackBuilder builder =
       server().options(options::ProgramOptions::defaultPublicOptionsFilter);
```

was, in `devel` (`/tmp/devel_RestPublicOptionsHandler.cpp:49-55`), preceded
by:

```cpp
  // available to any user with at least read access to the database
  if (ExecContext::isAuthEnabled() &&
      !ExecContext::current().canUseDatabase(auth::Level::RO)) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "insufficient permissions");
    co_return;
  }
```

This check has been deleted outright in the current branch (with no
replacement), and `#include "Utils/ExecContext.h"` is now an unused leftover
include in the current file (minor code-quality nit, not a behavioural
issue).

At first glance this looks like a real regression: any authenticated user,
regardless of their access level on the current database, would now be able
to read `/_admin/options-public`. However, tracing the condition
(`ExecContext::current().canUseDatabase(auth::Level::RO)`, i.e.
`RO <= _databaseAuthLevel`, i.e. `_databaseAuthLevel != NONE`) shows it is
**exactly** the same condition already enforced by the generic pre-handler
gate described above (`vc->databaseAuthLevel() == auth::Level::NONE` in
`devel` / `ec->canUseDatabase(db, Read).fail()` in the current branch) —
which by construction has *already* run, and *already* rejected any request
that would have failed this check, before `executeAsync()` is ever entered.
Consequently, in `devel`, this code was **dead**: the `FORBIDDEN` branch
could never be reached in practice, because `CommTask::canAccessPath()`
would already have aborted the request with `401`/`TRI_ERROR_FORBIDDEN`
beforehand for the exact same condition. Deleting it in the current branch
removes dead code but changes no observable behaviour.

This is fully consistent with `Documentation/path_permissions.md:828`, which
documents `/_admin/options-public` as `AUTHEN` — per the legend at
`Documentation/path_permissions.md:722-723`, `AUTHEN` already means "some
existing user (or SUPERUSER) has to be authenticated... must have read
access to the used database from `/_db/<dbname`" — i.e. exactly the generic
gate's semantics — with an empty "Changes to before RBAC" column, confirming
no behavioural change was intended or introduced.

### Finding 2 (Narrow, verified — not a security regression): `checkAuthentication()`'s `"jwt"` policy check

`RestOptionsBaseHandler::checkAuthentication()`
(`arangod/RestHandler/RestOptionsBaseHandler.cpp:44-50`), which gates
`/_admin/options` and `/_admin/options-description` when
`--server.options-api=jwt`, changed from:

```cpp
// devel (/tmp/devel_RestOptionsBaseHandler.cpp:44-50)
if (apiPolicy == "jwt") {
  if (!ExecContext::current().isSuperuser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "insufficient permissions");
    return false;
  }
}
```

to:

```cpp
if (apiPolicy == "jwt") {
  if (!ExecContext::current().isSuperuserOrDisabled()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  "insufficient permissions");
    return false;
  }
}
```

Unlike every previous occurrence of this `isSuperuser()` →
`isSuperuserOrDisabled()` pattern seen in earlier sessions (`RestCompactHandler`,
`RestAdminServerHandler`), `devel`'s condition here is **not** wrapped in an
`ExecContext::isAuthEnabled() &&` guard — it is plain `!isSuperuser()`. That
matters because `isSuperuser()` and `isSuperuserOrDisabled()` are *not*
algebraically equivalent when authentication is globally disabled
(`--server.authentication false`):

- In `devel`, `VocbaseContext::create()` (`/tmp/devel_VocbaseContext.cpp:78-91`),
  when `!auth->isActive()`, sets `ExecContext::Type::Internal` (→
  `isSuperuser() == true`) **only if `req.user().empty()`**; if the incoming
  request happens to carry a non-empty username (e.g. a client still sends
  HTTP Basic credentials even though the server ignores them for
  authentication purposes), the type is `Default` with `RW`/`RW` levels, so
  `isSuperuser()` is `false` — and `checkAuthentication()` would incorrectly
  reject the request with `403 FORBIDDEN` despite authentication being
  completely disabled.
- In the current branch, `ExecContext::create()`
  (`arangod/Utils/ExecContext.cpp:92-94`) always constructs
  `AuthMode::Disabled(req.user(), req)` when `!authenticationFeature.isActive()`,
  **regardless of `req.user()`** — so `isSuperuserOrDisabled()` is
  unconditionally `true` whenever auth is disabled, and the check always
  passes.

So there is a genuine, narrow behavioural difference: with
`--server.authentication false` **and** `--server.options-api jwt` **and** a
client that still supplies a non-empty username (e.g. stale Basic-Auth
credentials, or a reverse proxy that always injects one), `devel` would
reject the request with `403`, while the current branch allows it. This is
the *opposite* of a security regression — the current branch is strictly
*more permissive* in this one corner case, and arguably fixes what looks
like an oversight in `devel` (nobody would reasonably expect
`--server.authentication false` to still gate access based on an ignored
username). In the much more common case — no credentials sent at all when
auth is disabled — both branches behave identically (`req.user()` empty →
`ALLOW` in both). When authentication *is* active, both conditions require
the identity to be a genuine superuser (empty username, authenticated via
JWT) and are equivalent.

The `"admin"` policy check right below it
(`arangod/RestHandler/RestOptionsBaseHandler.cpp:52-58`) was also refactored,
from `apiPolicy == "admin" && !ExecContext::current().isAdminUser()` to
`apiPolicy == "admin" && r.fail()` where
`r = ExecContext::current().canUseAdminAction(auth::perms::AdminOptions{})`.
Tracing this through `AuthMode::Classic::check()`
(`arangod/Auth/AuthMode.cpp:367`: the generic `AnyAdmin` case dispatches to
`isAdmin()`, which is `check(UseDatabase{_system, Write})` at
`arangod/Auth/AuthMode.cpp:574-577`, using the "configured" (raw,
readonly-mode-ignoring) auth level) shows this reduces to precisely the same
"raw, configured `RW` on `_system`" test as `devel`'s
`isAdminUser()`/`_isAdminUser` field computation
(`/tmp/devel_ExecContext.cpp:105-109`, `/tmp/devel_VocbaseContext.cpp`) —
**identical ALLOW/DENY decision** in every case, including under global
read-only mode. The only observable difference is cosmetic: the error
message changes from the hardcoded `"insufficient permissions"` to
`r.errorMessage()` (`"insufficient database access level for '_system'"`);
the error code (`TRI_ERROR_HTTP_FORBIDDEN`/403) is unchanged in both
branches (unlike the analogous change in `RestAdminServerHandler`, which
also changed the `errorNum`).

The final check in `checkAuthentication()` — requiring
`_request->databaseName() == StaticStrings::SystemDatabase` — is completely
unchanged between branches.

### Summary for `RestOptions*` handlers

| Route | Verdict |
|---|---|
| `GET /_admin/options` | Identical to `devel` for the `"jwt"` policy in the normal (auth-enabled) case; narrow, more-permissive divergence only when auth is fully disabled *and* a stray username is present (Finding 2, not a security regression). `"admin"` policy: identical ALLOW/DENY, cosmetic message-text change only. `"disabled"` policy: route not even registered, identical in both branches. |
| `GET /_admin/options-description` | Same as above (shares `checkAuthentication()`) |
| `GET /_admin/options-public` | Identical to `devel` — the deleted in-handler check was provably dead code, fully subsumed by the generic pre-handler gate (Finding 1) |

**Action items / recommendations:**
1. No functional fix required for Finding 1; optionally remove the now-dead
   `#include "Utils/ExecContext.h"` in
   `arangod/RestHandler/RestPublicOptionsHandler.cpp` for cleanliness.
2. No fix required for Finding 2 — the current branch's behaviour in the
   auth-disabled edge case is arguably more correct than `devel`'s. Flagging
   only for awareness in case any test asserts on `devel`'s specific (and
   likely accidental) rejection behaviour in that corner case.
3. No action required for the `"admin"`-policy message-text change (cosmetic).


## `RestCursorHandler` (`arangod/RestHandler/RestCursorHandler.cpp`)

Mounted at `/_api/cursor` (prefix). Handles AQL query execution
(`POST /_api/cursor[/json]`), cursor continuation
(`PUT /_api/cursor/<id>`, and the legacy `POST /_api/cursor/<id>`), batch
re-fetch (`POST /_api/cursor/<id>/<batch-id>`), and cursor disposal
(`DELETE /_api/cursor/<id>`).

### Overview: no in-handler authorization logic

Just like `RestDocumentHandler`, `RestCursorHandler.cpp` itself contains
**no authorization logic at all** — confirmed by diffing it against `devel`
(`diff -u /tmp/devel_RestCursorHandler.cpp arangod/RestHandler/RestCursorHandler.cpp`):
the only differences are an added `activities::Registry::
ScopedCurrentlyExecutingActivity` guard (unrelated activity-tracking
instrumentation) and some `#include`/comment churn. `RestCursorHandler.h`
has no diff beyond a removed `@author` comment. All authorization happens
in two shared, lower-level components that this handler calls into:

1. **Collection-level permissions during query execution**, via
   `RestVocbaseBaseHandler::createTransactionContext()` →
   `AqlTransaction`/`transaction::Methods::addCollection()` →
   `TransactionState::checkCollectionPermission()`
   (`arangod/StorageEngine/TransactionState.cpp:713-754`) — the exact same
   central gate already investigated in the `RestDocumentHandler` session.
   `registerQueryOrCursor()` (`arangod/RestHandler/RestCursorHandler.cpp:192-193`)
   always creates its transaction context with `AccessMode::Type::WRITE`
   (comment: `"access mode can always be write on the coordinator"`) — but
   this only controls what the *transaction* is opened as; the actual
   per-collection access level checked for each collection referenced by
   the query is determined from the AQL AST during planning
   (`arangod/Aql/ExecutionPlan.cpp:194-247`, unchanged between branches) and
   passed down via `aql::Collection::accessType()`
   (`AqlTransaction::processCollection`,
   `arangod/Aql/AqlTransaction.cpp:84-97`, itself unchanged apart from a
   cosmetic namespace-wrapping refactor — confirmed by diff). This means:
   - **Read-only AQL queries** request `AccessType::READ` per collection,
     so they are unaffected by the previously-documented server-wide
     read-only-mode regression (that regression only triggers for
     `WriteData`-or-above requests).
   - **AQL queries containing `INSERT`/`UPDATE`/`REMOVE`/`UPSERT`/
     `REPLACE`** (or graph modifications) request `AccessType::WRITE` for
     the affected collection(s), and therefore **do inherit** the
     already-documented regression from the `RestDocumentHandler` session:
     while the server is globally in read-only mode
     (`ServerState::readOnly()`), `ExecContext::canUseCollection()`
     (`arangod/Utils/ExecContext.cpp:223-231`) short-circuits to a generic
     `TRI_ERROR_FORBIDDEN` ("Server is in read-only mode.") instead of the
     more specific `TRI_ERROR_ARANGO_READ_ONLY` (1004) that `devel`'s
     level-capping approach produces. Both map to HTTP 403, so this is the
     same cosmetic-at-the-status-code-level/real-at-the-errorNum-level
     divergence already tracked for `RestDocumentHandler` and
     `RestCollectionHandler::truncate` — no new finding number is assigned
     here, this is just confirmation that AQL write queries are also in
     scope for that existing regression.
   - EE's `IgnoreNoAccessAqlTransaction`/`skipInaccessibleCollections`
     mechanism (`arangod/Aql/AqlTransaction.cpp:42-48`) is a
     Coordinator-side-computed, opt-in mechanism unrelated to `Classic`
     mode's own permission checks (the set of "inaccessible" collections is
     computed and serialized by the Coordinator before the query ever
     reaches this code); it is unchanged between branches and out of scope
     for a `Classic`-mode-vs-`devel` comparison.

2. **Cursor ownership**, via `CursorRepository` — this is where a genuine,
   new finding was made (Finding 1 below).

### Finding 1 (Regression): cursor-ownership check bypassed when authentication is disabled

`CursorRepository.cpp` stores, alongside each cursor, the username of the
identity that created it (`arangod/Utils/CursorRepository.cpp:139-145`,
`addCursor()`: `auto user = ExecContext::current().user();`), and both
`find()` (continuation/batch fetch) and `remove()` (deletion) gate every
lookup through an `authorized()` predicate
(`arangod/Utils/CursorRepository.cpp:223`, `:264`):

`devel` (`/tmp/devel_CursorRepository.cpp:47-53`):
```cpp
bool authorized(std::pair<arangodb::Cursor*, std::string> const& cursor) {
  auto const& exec = arangodb::ExecContext::current();
  if (exec.isSuperuser()) {
    return true;
  }
  return (cursor.second == exec.user());
}
```

Current branch (`arangod/Utils/CursorRepository.cpp:46-52`):
```cpp
bool authorized(std::pair<arangodb::Cursor*, std::string> const& cursor) {
  auto const& exec = arangodb::ExecContext::current();
  if (exec.isSuperuserOrDisabled()) {
    return true;
  }
  return (cursor.second == exec.user());
}
```

The change from `isSuperuser()` to `isSuperuserOrDisabled()` looks like the
same, already-established-safe pattern from `RestCompactHandler`/
`RestAdminServerHandler` (`!isSuperuserOrDisabled()` ⟺ `isAuthEnabled() &&
!isSuperuser()`) — but here the logic is used the *other way round*: as an
early-`true` short-circuit rather than an early-rejection guard, and that
changes what it actually does:

- I traced the origin of both `cursor.second` (the stored owner, set at
  `addCursor()` time) and `exec.user()` (the current caller, read at
  `find()`/`remove()` time) all the way back to
  `CommTask::checkAuthHeader()` (`arangod/GeneralServer/CommTask.cpp:868-937`,
  confirmed **byte-for-byte unchanged** between branches by diff) and
  `auth::TokenCache::checkAuthenticationBasic()`
  (`arangod/Auth/TokenCache.cpp:140-202`, also unchanged). Crucially,
  `AuthenticationFeature::prepare()`
  (`arangod/GeneralServer/AuthenticationFeature.cpp:169-184`) constructs a
  real `UserManager` on single servers/Coordinators **regardless of
  whether `--server.authentication` is `true` or `false`** — only
  `isActive()` (`:240-242`) is gated by the option. This means that even
  with authentication fully disabled, if a client sends an `Authorization:
  Basic <...>` header, `checkAuthenticationBasic()` still runs a *real*
  credential check against the user database and sets `req.user()`
  accordingly (to the validated username on success, or to the raw,
  unvalidated username parsed from the header on failure) — this part of
  the pipeline is identical in both branches.
- Both branches' `ExecContext`/`VocbaseContext` construction for the
  "authentication disabled" case use this same `req.user()` value
  directly, without re-checking `req.authenticated()`: `devel`'s
  `VocbaseContext::create()` (`/tmp/devel_VocbaseContext.cpp:78-92`) passes
  `req.user()` into the constructor whenever `!auth->isActive()`
  (yielding `ExecContext::Type::Default` whenever `req.user()` is
  non-empty, `Type::Internal` only when it's empty); the current branch's
  `ExecContext::create()` (`arangod/Utils/ExecContext.cpp:92-94`)
  equivalently constructs `AuthMode::Disabled(req.user(), req)`. So **the
  stored/compared usernames are populated identically in both branches** —
  this is not where the divergence comes from.
- The divergence is purely in the `authorized()` predicate itself. Given
  two different (or absent-vs-present) client-supplied usernames while
  `--server.authentication false`:
  - `devel`: `exec.isSuperuser()` is `false` for any non-empty-username
    `Type::Default` context (only a genuine JWT-superuser or the truly
    anonymous/`Type::Internal` case with empty username short-circuits),
    so the code falls through to the username-equality check. A cursor
    created under username `"alice"` and later accessed under username
    `"bob"` (e.g. two different HTTP clients that happen to send different
    `Authorization: Basic` headers, or one client that sends a header and
    another that sends none at all) is correctly **rejected** with `404
    CURSOR_NOT_FOUND` (from `find()`) or a no-op `remove()` — exactly as it
    would be if authentication were enabled.
  - Current branch: `exec.isSuperuserOrDisabled()` is `true` for **every**
    caller whenever authentication is disabled, completely bypassing the
    username-equality check. **Any** caller can continue, re-fetch a batch
    of, or delete **any other user's** streaming cursor while
    `--server.authentication false`, regardless of what username (if any)
    it supplies.

This is a genuine behavioral regression, not merely cosmetic: the
ALLOW/DENY decision itself changes for this specific combination of
inputs. Its practical security impact is limited, though not zero — when
authentication is fully disabled, `AuthMode::Disabled::check()` already
grants blanket access to essentially everything (any user can read/write
any document, list any database, etc.), so cross-user cursor access does
not, by itself, expose data that wasn't already fully accessible via a
fresh, equivalent query. The concrete, observable difference is narrower:
loss of the (admittedly vestigial) per-"user"-label isolation of
in-flight streaming-cursor *state* — e.g. one nominal user's script
being able to accidentally or intentionally continue/dispose another
nominal user's in-progress cursor purely by guessing/enumerating cursor
IDs, something `devel` prevented via the username tag even with auth off.
Given `--server.authentication false` is itself a deliberately
insecure/testing-oriented configuration, this is a low-severity but
genuine finding.

### Summary for `RestCursorHandler`

| Route | Verdict |
|---|---|
| `POST /_api/cursor[/json]` (create, read-only AQL) | Identical to `devel` — no in-handler auth logic; per-collection `Read`-level checks via the shared, unmodified `TransactionState::checkCollectionPermission()` path |
| `POST /_api/cursor[/json]` (create, AQL with writes) | Identical to `devel`, **except** it inherits the previously-documented server-wide-read-only-mode `errorNum` divergence (`TRI_ERROR_ARANGO_READ_ONLY` vs. generic `TRI_ERROR_FORBIDDEN`) already tracked under `RestDocumentHandler` Finding 1 — not a new finding |
| `PUT /_api/cursor/<id>`, `POST /_api/cursor/<id>[/<batch-id>]` (continuation/batch fetch) | **Regression** (Finding 1) when `--server.authentication false`: cursor-ownership check is bypassed for all callers |
| `DELETE /_api/cursor/<id>` | Same regression (Finding 1) applies — any caller can delete any cursor when authentication is disabled |

**Action items / recommendations:**
1. **Fix Finding 1**: change `isSuperuserOrDisabled()` back to
   `isSuperuser()` in `CursorRepository.cpp`'s anonymous `authorized()`
   helper (`arangod/Utils/CursorRepository.cpp:48`), restoring the
   username-equality fallback for the auth-disabled case. This is a
   one-line, low-risk fix that exactly restores `devel`'s behavior.
2. No action needed for the read-only-mode observation under
   `POST /_api/cursor` — already covered by the existing action item
   against `ExecContext::canUseCollection()`/`canUseDatabase()` recorded
   under `RestDocumentHandler`.


## `RestAqlHandler` (`arangod/Aql/RestAqlHandler.cpp`)

Mounted at `/_api/aql` (prefix). This is a **cluster-internal-only**
handler: it implements the wire protocol a Coordinator uses to set up
(`POST /_api/aql/setup`), drive (`PUT /_api/aql/<op>/<queryId>`), and tear
down (`DELETE /_api/aql/finish/<queryId>`) the per-shard AQL execution
snippets running on a DB-Server. It is never meant to be called by
ordinary end-user clients, and is not listed at all in
`Documentation/path_permissions.md` (unlike every other handler examined so
far), confirming its purely-internal status in both branches.

### Overview: no authorization logic in the handler at all

`RestAqlHandler.cpp`/`.h` are, again, essentially **untouched** between
branches — confirmed by diff
(`diff -u /tmp/devel_RestAqlHandler.cpp arangod/Aql/RestAqlHandler.cpp`):
the only differences are a moved `#include "VocBase/vocbase.h"`, one added
comment (`// Mounted at /_api/aql (prefix)`), and `@author` comment
removal in the header. There is no `ExecContext`/`AuthMode` reference
anywhere in this file, nor in `arangod/Aql/ClusterQuery.cpp` or
`arangod/Aql/QueryRegistry.cpp` (both confirmed unchanged between branches
by diff, and neither references `ExecContext`/`auth::` at all in either
branch). The handler does not override `checkUserCanAccess()` either
(no such declaration in `arangod/Aql/RestAqlHandler.h`). So — exactly as
the task framing anticipated — this handler relies **entirely** on the
generic, handler-independent pre-execution gates.

### Single server vs. cluster

`executeAsync()` (`arangod/Aql/RestAqlHandler.cpp:460-466`) starts with:
```cpp
if (ServerState::instance()->isSingleServer()) {
  generateError(rest::ResponseCode::NOT_IMPLEMENTED, ...
                "this endpoint is only available in clusters");
  co_return;
}
```
identical in both branches. `setupClusterQuery()` additionally asserts/
rejects (`arangod/Aql/RestAqlHandler.cpp:102-107`,
`TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER`) unless running on a DB-Server —
also unchanged. So:
- On a **single server**: always `501 NOT_IMPLEMENTED`, no auth check
  reached, identical in both branches.
- On a **Coordinator**: `POST /_api/aql/setup` is rejected with `405
  METHOD_NOT_ALLOWED` (identical in both branches) — but only *after*
  passing the generic pre-handler gates below, since a Coordinator is
  user-facing and a normal authenticated user's request can reach this far
  as long as they have at least `RO` on the target database (a low bar).
  `useQuery()`/`handleFinishQuery()` have no server-role guard at all in
  either branch, but harmlessly fail with `QUERY_NOT_FOUND` unless the
  caller happens to know a live, cluster-internal numeric query-engine ID
  — `QueryRegistry`'s id-based lookup has no per-user ownership concept in
  either branch (confirmed identical by diff of `QueryRegistry.cpp`); this
  predates and is orthogonal to the RBAC work, so it is not a regression.
- On a **DB-Server**: this is the actual, intended use of the handler —
  driven exclusively by Coordinator-to-DB-Server internal cluster traffic,
  authenticated via the shared internal JWT secret.

### Finding 1 (Code-quality / latent risk, not exploitable): different rejection path for a forged non-superuser JWT on a DB-Server

This is the one genuine difference found, and it matches the "only
Superuser checks, as before" expectation in spirit — both branches
ultimately **reject** the scenario below, just via different code paths
and with different HTTP status codes.

Background: `AuthenticationFeature::prepare()`
(`arangod/GeneralServer/AuthenticationFeature.cpp:169-184`) only
constructs a `UserManager` on single-server/Coordinator roles; on
DB-Servers (and Agents), `userManager()` is `nullptr`, confirmed identical
in both branches (this file is part of the untouched core). The
legitimate way to reach `/_api/aql` on a DB-Server is via the **internal
superuser JWT** (empty `preferred_username`), which both branches
recognize identically:
- Current: `ExecContext::create()` (`arangod/Utils/ExecContext.cpp:82-90`)
  detects `req.authenticated() && req.user().empty() && ... == JWT` and
  constructs `AuthMode::Superuser`.
- `devel`: `VocbaseContext::create()`
  (`/tmp/devel_VocbaseContext.cpp:67-74`) detects the identical condition
  and constructs `ExecContext::Type::Internal` with `RW`/`RW`.

Both grant unconditional access via `AuthMode::Superuser::check()`
(`arangod/Auth/AuthMode.cpp:64-66`, unconditional `{}`) / `isInternal()`
(devel) respectively — **identical behavior** for the actual, intended
cluster-internal traffic.

The divergence only appears for a hypothetical forged request: a JWT that
is *validly signed* with the correct shared cluster secret (so
`req.authenticated() == true`) but carries a **non-empty** username claim
(so it is *not* recognized as the superuser-JWT special case). On a
DB-Server, since `userManager() == nullptr`:
- Current branch: `ExecContext::create()`
  (`arangod/Utils/ExecContext.cpp:96-102`) hits
  `if (!req.authenticated() || userManager == nullptr) { return
  AuthMode::Unauthenticated(req.user(), req); }` — this function **always**
  returns a valid (non-null) `ExecContext`. Request processing continues
  normally into `RestHandler::checkUserCanAccess()`
  (`arangod/GeneralServer/RestHandler.cpp:705-763`), which calls
  `ec->canUseDatabase(dbname, DatabaseAccessLevel::Read)`. This dispatches
  to `AuthMode::Unauthenticated::check()`
  (`arangod/Auth/AuthMode.cpp:607-618`), whose `UseDatabase` branch denies
  anything above `None`, returning `{TRI_ERROR_FORBIDDEN, "not
  authenticated"}`. `checkUserCanAccess()` then rejects with **`401
  UNAUTHORIZED`, `"No read access to database."`** before the handler's
  `execute()` ever runs.
- `devel`: `VocbaseContext::create()`
  (`/tmp/devel_VocbaseContext.cpp:107-112`) hits the equivalent branch:
  ```cpp
  auth::UserManager* um = auth->userManager();
  if (um == nullptr) {
    LOG_TOPIC(...) << "users are not supported on this server";
    return nullptr;
  }
  ```
  Returning `nullptr` here makes `resolveRequestContext()`
  (`arangod/GeneralServer/CommTask.cpp:83-107`, this exact function is
  unchanged between branches) return `false`. The caller
  (`arangod/GeneralServer/CommTask.cpp:283-311`, also unchanged) then
  re-derives an auth level from scratch for the sole purpose of picking an
  error response: since `_auth->userManager() != nullptr && !req.user
  ().empty()` is false (because `userManager()` is null), it takes the
  `else` branch and sets `lvl = auth::Level::RW` — **not** `NONE` — so the
  `lvl == NONE` early-rejection (which would produce `401 FORBIDDEN`) is
  skipped, and the function falls through to the unconditional
  `sendErrorResponse(NOT_FOUND, ..., TRI_ERROR_ARANGO_DATABASE_NOT_FOUND)`
  a few lines further down. So `devel` responds with **`404 NOT_FOUND`,
  `TRI_ERROR_ARANGO_DATABASE_NOT_FOUND`** for the exact same forged
  request — a database-existence-masking response, not an
  authentication-specific one.

Both branches reject the request; **there is no privilege-escalation or
access-control regression** — only the HTTP status code (`401` vs. `404`)
and error code/message differ. I am not classifying this as a security
regression for two reasons: (1) the ultimate ALLOW/DENY outcome is
identical (deny), and (2) exploiting the difference requires already
possessing the cluster's internal JWT secret — the same secret that would
let an attacker trivially forge the *actual* superuser token (empty
username) and bypass every check in both branches entirely, making the
distinction moot in practice. It is recorded here as a **code-quality /
latent-risk** observation because `ExecContext::create()`'s signature
(`[[nodiscard]] static std::shared_ptr<ExecContext> create(...)`, never
`nullptr`) has silently absorbed a case that `devel`'s
`VocbaseContext::create()` treated as fatal/unrepresentable (returning
`nullptr` with a warning log message), replacing it with a normal,
"successfully denied" `Unauthenticated` context. This same effect (dead
`!context` branch in `resolveRequestContext`'s caller,
`arangod/GeneralServer/CommTask.cpp:285`) was already noted in passing
during the very first (`RestDatabaseHandler`) session as a general
architectural observation; this is the first handler where it was traced
all the way through to a concrete, reachable (if inconsequential)
behavioral difference.

### Summary for `RestAqlHandler`

| Route / scenario | Verdict |
|---|---|
| Any route, on a single server | Identical to `devel`: unconditional `501 NOT_IMPLEMENTED`, no auth check reached |
| `POST /_api/aql/setup`, on a Coordinator | Identical to `devel`: `405 METHOD_NOT_ALLOWED`, reached identically via the generic per-database `RO` gate |
| Any route, on a DB-Server, authenticated via the real internal superuser JWT (the actual, intended use) | Identical to `devel` — unconditional superuser access in both branches |
| Any route, on a DB-Server, with a validly-signed but non-superuser (non-empty-username) JWT | Both branches **deny** the request (no regression); cosmetic-but-notable divergence in HTTP status/error code (`401 UNAUTHORIZED` current vs. `404 ARANGO_DATABASE_NOT_FOUND` `devel`) — Finding 1, not exploitable beyond what already having the JWT secret grants |
| `PUT/DELETE /_api/aql/...` referencing a non-existent/foreign query-engine ID | Identical to `devel`: `404`/`NOT_FOUND`-class errors from `QueryRegistry`, no ownership concept in either branch (pre-existing, unrelated to RBAC) |

**Action items / recommendations:**
1. No functional fix required — both branches deny access in the
   scenario described in Finding 1. Optionally, for consistency/defense in
   depth, `ExecContext::create()` could special-case "authenticated,
   non-empty username, but no `UserManager` available" to produce a
   `404`-style outcome matching `devel`'s masking behavior, but this is
   low priority given the precondition already implies possession of the
   internal JWT secret.
2. No other action items for this handler — it is otherwise a clean,
   fully delegate-to-generic-gates handler with no distinct authorization
   logic of its own, matching the task's expectation.

## `RestClusterHandler` (`arangod/Cluster/RestClusterHandler.cpp`)

Mounted as a prefix handler at `/_api/cluster`, **only when
`ClusterFeature::isEnabled()`** (`arangod/GeneralServer/GeneralServerFeature.cpp:750-754`,
identical registration in `devel`, `/tmp/devel_GeneralServerFeature.cpp:797-800`)
— i.e. it does not exist at all on a single server, satisfying the
single-server/cluster distinction trivially: there is nothing to compare on
a single server, and on a cluster deployment this handler is registered
identically on every role (Coordinator, DB-Server, Agent), with individual
sub-routes internally restricting themselves to Coordinator-only via
`ServerState::instance()->isCoordinator()` checks (`handleAgencyDump`,
`handleCommandEndpoints`) — both unchanged between branches. Diffed
against `devel` in full (`arangod/Cluster/RestClusterHandler.cpp:1-615`
vs. `/tmp/devel_RestClusterHandler.cpp`); as the user predicted, this
handler is "mostly admin operations" and turned out to be clean.

**Result: fully equivalent to `devel` — no regressions found.** Three
admin-style checks were refactored to the new permission vocabulary, one
new handler-level `checkUserCanAccess()` override was added, and one
unrelated bug fix (off-by-one) was found; all traced to identical
ALLOW/DENY outcomes.

- **Finding 1 (verified equivalent) — `cluster-info` subtree admin gate**
  (`arangod/Cluster/RestClusterHandler.cpp:54-61`): `devel`'s
  `!ExecContext::current().isAdminUser()` became
  `ExecContext::current().canUseAdminAction(auth::perms::AdminClusterInfo{})`.
  `AdminClusterInfo` is a member of the `AnyAdmin` concept
  (`arangod/Auth/Permissions.h:97,111-118`), and `AuthMode::Classic::check()`
  maps every `AnyAdmin` permission to `isAdmin()`
  (`arangod/Auth/AuthMode.cpp:367`), i.e. `check(UseDatabase{_system, Write})`
  (`arangod/Auth/AuthMode.cpp:574-577`) — exactly "raw, configured `RW` grant
  on `_system`, ignoring read-only mode", which is precisely what `devel`'s
  `isAdminUser()` reduces to (already proven algebraically equivalent in the
  `RestMetricsHandler` session, `/tmp/devel_ExecContext.cpp:92-110`). This
  gate protects the entire `cluster-info` sub-tree (`flush`,
  `get_collection_info`, `get_collection_info_current`,
  `get_responsible_servers`, `get_responsible_shard`,
  `get_analyzers_revision`, `wait_for_plan_version`,
  `get_max_number_of_shards`, `get_max_replication_factor`,
  `get_min_replication_factor`) in both branches identically. The
  `#ifndef ARANGODB_ENABLE_MAINTAINER_MODE` block immediately below it,
  which further restricts `cluster-info/<subroute>` (everything except the
  bare `cluster-info` dump) to `ExecContext::current().isSuperuser()` in
  non-maintainer builds, is **byte-for-byte unchanged** between branches
  (not part of the diff at all) — confirmed identical in both by inspection.

- **Finding 2 (verified equivalent) — `handleAgencyDump`/`handleAgencyCache`
  admin gate** (`arangod/Cluster/RestClusterHandler.cpp:155-161,175-181`):
  `devel`'s hand-rolled check —
  ```cpp
  if (af->isActive() && !_request->user().empty()) {
    auth::Level lvl = af->userManager() != nullptr
        ? af->userManager()->databaseAuthLevel(_request->user(), "_system", true)
        : auth::Level::RW;
    if (lvl < auth::Level::RW) { /* FORBIDDEN */ }
  }
  ```
  became `exec.canUseAdminAction(auth::perms::AdminReadAgency{})`, again an
  `AnyAdmin` permission reducing to the same `isAdmin()` check as Finding 1.
  I verified the two conditions under which `devel`'s manual check is
  *skipped* (auth inactive → always allow; `_request->user().empty()` →
  always allow) are exactly reproduced on the current side:
  - Auth inactive → `ExecContext::create()` builds `AuthMode::Disabled`
    (`arangod/Utils/ExecContext.cpp:92-93`), whose `check()` unconditionally
    returns success (`arangod/Auth/AuthMode.cpp:659-661`) — matches.
  - Empty username while authenticated via JWT → recognized as the
    superuser special-case in **both** branches
    (`arangod/Utils/ExecContext.cpp:83-90` current;
    `/tmp/devel_VocbaseContext.cpp:67-68` `devel`, same condition
    verbatim) — `AuthMode::Superuser::check()` unconditionally succeeds
    (`arangod/Auth/AuthMode.cpp:64-66`) — matches.
  - The remaining branch — `devel`'s `af->userManager() == nullptr` fallback
    to a hardcoded `auth::Level::RW` (i.e., grant access even to a named,
    non-superuser, authenticated user when no `UserManager` exists, which
    only happens on DB-Servers/Agents) — looked like a possible divergence,
    since the current branch's equivalent path (no `UserManager` →
    `AuthMode::Unauthenticated`, `arangod/Utils/ExecContext.cpp:100-101`)
    would instead **reject** such a caller
    (`AuthMode::Unauthenticated::check()`'s catch-all branch,
    `arangod/Auth/AuthMode.cpp:640-642`). However, I traced this down to
    `auth::TokenCache::validateJwtBody()`
    (`arangod/Auth/TokenCache.cpp:362-371`): a JWT carrying a non-empty
    `preferred_username` claim is rejected outright
    (`return auth::TokenCache::Entry::Unauthenticated();`) whenever
    `_userManager == nullptr` — i.e. it is **impossible** to ever reach
    request handling as an authenticated, non-empty-username caller on a
    node without a `UserManager` in the first place; only `server_id`
    (empty-username) tokens succeed there, and those are always the
    superuser case already covered above. So `devel`'s
    `af->userManager() == nullptr` branch is **unreachable dead code** in
    `devel` too (this is the same "removing unreachable devel code changes
    nothing observable" pattern already documented for `RestPublicOptionsHandler`
    in the `RestOptions*` session) — not a regression.

- **Finding 3 (verified equivalent, faithful gap-fill) — new
  `checkUserCanAccess()` override for `/_api/cluster/endpoints`**
  (`arangod/Cluster/RestClusterHandler.cpp:136-145`,
  `arangod/Cluster/RestClusterHandler.h:38-39`, both new in this branch):
  ```cpp
  async<Result> RestClusterHandler::checkUserCanAccess() const {
    auto const& suffixes = _request->suffixes();
    if (_request->authenticated() && suffixes.size() == 1 &&
        suffixes[0] == "endpoints") {
      co_return Result{};
    }
    co_return co_await RestBaseHandler::checkUserCanAccess();
  }
  ```
  This reproduces a path-based exception that lived in `devel`'s
  `CommTask::canAccessPath()`:
  ```cpp
  } else if (userAuthenticated && path == "/_api/cluster/endpoints") {
    // allow authenticated users to access cluster/endpoints
    result = Flow::Continue;
    // vc->forceReadOnly();
  }
  ```
  (`/tmp/devel_CommTask.cpp:856-859`, only reached when the generic
  per-database gate already failed, i.e. `vc->databaseAuthLevel() == NONE`).
  This is the same architectural pattern already seen for
  `/_admin/server/availability` in the `RestAdminServerHandler` session and
  for `/_admin/options-public` in the `RestOptions*` session: `devel`'s
  monolithic `CommTask::canAccessPath()` path-exception list has been
  migrated, route by route, into individual handlers'
  `checkUserCanAccess()` overrides. I compared the two conditions
  term-for-term: `suffixes.size() == 1 && suffixes[0] == "endpoints"` is the
  exact suffix-decomposition of an exact-match `path == "/_api/cluster/endpoints"`
  for a handler mounted at the `/_api/cluster` prefix (no trailing segments
  allowed in either), `_request->authenticated()` is the same
  `userAuthenticated` flag, and — importantly — **neither** branch escalates
  to superuser for this exception (`devel`'s commented-out
  `vc->forceReadOnly()` confirms the level is deliberately left as-is; the
  current override likewise just returns a bare success, leaving
  `ExecContext` untouched). `handleCommandEndpoints()` itself performs no
  further permission checks in either branch, only a
  `ServerState::instance()->isCoordinator()` guard producing `501` on
  non-Coordinators (unchanged). **Exact parity confirmed.**

- **Finding 4 (unrelated, not an auth issue) — off-by-one bounds check
  fixed**: `handleCI_getResponsibleShard`'s guard changed from
  `suffixes.size() < 4` (`devel`) to `suffixes.size() < 5`
  (`arangod/Cluster/RestClusterHandler.cpp:446`). The handler reads
  `suffixes[2]`, `suffixes[3]`, **and `suffixes[4]`**
  (`arangod/Cluster/RestClusterHandler.cpp:458-460`), so `devel`'s `< 4`
  bound under-checked by one and could read one element past the
  minimum guaranteed size when exactly 4 suffixes were supplied — a
  latent out-of-bounds-read bug in `devel`, fixed (not introduced) on this
  branch. Purely a robustness fix, unrelated to authorization; noted for
  completeness since it appeared in the diff.

### Summary for `RestClusterHandler`

| Route / scenario | Verdict |
|---|---|
| Single server (any route) | N/A — handler not registered at all in either branch |
| `GET /_api/cluster/cluster-info` and sub-routes, admin vs. non-admin, read-only mode on/off | Identical to `devel` (Finding 1) |
| `GET /_api/cluster/agency-dump` (Coordinator only), `GET /_api/cluster/agency-cache` | Identical to `devel`, including the `UserManager == nullptr` edge case, proven unreachable in both (Finding 2) |
| `GET /_api/cluster/endpoints`, authenticated but no DB access to current database | Identical to `devel` — bypasses the generic DB-access gate in both, no superuser escalation in either (Finding 3) |
| `GET /_api/cluster/endpoints`, unauthenticated | Identical to `devel` — generic gate applies, request denied in both |
| `POST /_api/cluster/cluster-info/get_responsible_shard/...` with exactly 4 suffixes | Both branches: current fixes a latent OOB-read bug present in `devel`; unrelated to auth (Finding 4) |

**Action items / recommendations:** None. This handler requires no code
changes — every route produces the same ALLOW/DENY decision as `devel`,
confirming the user's expectation that "mostly admin operations" would be
unproblematic.

## `RestAgencyCallbacksHandler` (`arangod/Cluster/RestAgencyCallbacksHandler.cpp`)

Mounted (prefix) at `/_api/agency/agency-callbacks`
(`arangod/Cluster/ClusterFeature.h:95-97`), only registered when the cluster
feature is enabled (`arangod/GeneralServer/GeneralServerFeature.cpp:743-749`),
i.e. never on a genuine single server. It has no in-handler authorization
logic whatsoever in either branch, and no handler-specific
`checkUserCanAccess()` override
(`arangod/Cluster/RestAgencyCallbacksHandler.h:36-53`), so the entire
ALLOW/DENY decision for this route is made exclusively by the generic
pre-handler gate already documented in earlier sessions (`CommTask::
canAccessPath()` in `devel` / `RestHandler::checkUserCanAccess()` on this
branch) — the same "background" mechanism previously traced in detail for
the `RestOptions*` family and `RestClusterHandler`'s `endpoints` route.

### Diff result: purely cosmetic

```
diff -u /tmp/devel_RestAgencyCallbacksHandler.cpp arangod/Cluster/RestAgencyCallbacksHandler.cpp
diff -u /tmp/devel_RestAgencyCallbacksHandler.h   arangod/Cluster/RestAgencyCallbacksHandler.h
```

The only differences are a removed `@author` doxygen comment and an added
one-line mount-point comment (`arangod/Cluster/RestAgencyCallbacksHandler.cpp:42-43`).
The handler body (`arangod/Cluster/RestAgencyCallbacksHandler.cpp:44-88`) —
suffix-count/method/body validation, `AgencyCallbackRegistry::getCallback()`
lookup by numeric id, `cb->refetchAndUpdate()`, `404`/`202` response — is
byte-for-byte identical between branches.

### Authorization path: identical to `devel`

Since the route has no `/_db/...` prefix, `CommTask::setDefault()` assigns
it the default database `_system`
(`arangod/GeneralServer/CommTask.cpp:76`) in both branches — same as every
other un-prefixed admin path already investigated. With no path-based
exception and no handler override, both branches therefore require the
calling identity to hold at least `auth::Level::RO` on `_system` (or be a
superuser, or have auth disabled) to reach the handler body at all; this is
the exact same generic-gate condition proven equivalent between
`CommTask::canAccessPath()` and `RestHandler::checkUserCanAccess()` in the
`RestOptions*` session. No handler-specific privilege (e.g. admin) is
required beyond that in either branch — any authenticated user with mere
read access to `_system` can trigger a refetch of an arbitrary
still-registered callback id.

### Background note: the endpoint appears to be legacy/unreachable in practice (unrelated to auth, identical in both branches)

While tracing who actually calls this endpoint (to determine which
identity's permissions matter in practice), I found that
`AgencyCallbackRegistry::getEndpointUrl()` — the only code in the tree that
assembles a `<node-endpoint>/_api/agency/agency-callbacks/<id>` URL
(`arangod/Cluster/AgencyCallbackRegistry.cpp:167-170`) — has **no callers
anywhere** in either branch (`git grep getEndpointUrl` returns only its own
declaration/definition on both `devel` and this branch). Callback delivery
today is handled entirely locally by `AgencyCache`'s polling mechanism
(`AgencyCallbackRegistry::registerCallback()` →
`_clusterFeature.agencyCache().registerCallback(...)`,
`arangod/Cluster/AgencyCallbackRegistry.cpp:69-110`), not by the agency
pushing an HTTP notification to this route. This isn't a `Classic`-vs-RBAC
difference — the dead code predates this branch — but it's worth recording
because it means the authorization question above is largely academic
under current architecture: nothing in the normal code paths of either
branch is known to actually issue requests to this handler today (it may
still be reachable by a hand-crafted request, by very old/mixed-version
agents, or by code outside this repository, so the equivalence proof above
still matters for defense in depth).

### Summary for `RestAgencyCallbacksHandler`

| Route / scenario | Verdict |
|---|---|
| Single server (any route) | N/A — handler not registered at all in either branch |
| `POST /_api/agency/agency-callbacks/<id>`, authenticated with `RO`+ on `_system` | Identical to `devel` — generic gate passes, handler body byte-for-byte identical |
| `POST /_api/agency/agency-callbacks/<id>`, authenticated but no access to `_system` | Identical to `devel` — generic gate rejects in both |
| `POST /_api/agency/agency-callbacks/<id>`, auth disabled | Identical to `devel` — generic gate passes unconditionally in both |
| Real-world reachability of this endpoint | Same in both branches — appears to be dead/legacy (push-URL builder unused), unrelated to auth |

**Action items / recommendations:** None. No differences of any kind
(functional or cosmetic) were found beyond comments; this is the second
fully clean handler after `RestCompactHandler`.

## `RestAnalyzerHandler` (`arangod/RestHandler/RestAnalyzerHandler.cpp`)

Mounted at `/_api/analyzer` (prefix). `GET` with no suffix lists all
analyzers visible from the current database plus the system database
(plus built-in static analyzers); `GET /<name>` fetches a single analyzer;
`POST` (no suffix) creates an analyzer; `DELETE /<name>[?force=true]`
removes one. All four operations ultimately funnel through
`IResearchAnalyzerFeature::canUse()`
(`arangod/IResearch/IResearchAnalyzerFeature.cpp:1149-1178`, devel:
`devel:arangod/IResearch/IResearchAnalyzerFeature.cpp:1155-1177`), the only
authorization-relevant function in that file (confirmed by grepping the
whole file for `canUse|ExecContext|auth::Level` — no other function
touches authorization; `emplace()`/`remove()` trust the caller
completely).

### Diff overview

`RestAnalyzerHandler.h` is byte-for-byte identical apart from the removed
`@author` doxygen lines. `RestAnalyzerHandler.cpp` differs only in the four
call sites of `IResearchAnalyzerFeature::canUse()` (bool → `Result`, see
Finding 1) and in the listing gate of `getAnalyzers()` (see Finding 2);
`createAnalyzer()`, `execute()`, `getAnalyzer()` (aside from its `canUse`
call), and `removeAnalyzer()` are otherwise unchanged. The permission-name
normalization/validation helpers they call into —
`splitAnalyzerName()`, `normalize()`, `extractVocbaseName()`, and, most
importantly, `analyzerReachableFromDb()`
(`arangod/IResearch/IResearchAnalyzerFeature.cpp:2354-2372`, byte-for-byte
identical to `devel:arangod/IResearch/IResearchAnalyzerFeature.cpp:2370-2388`)
— are untouched. `analyzerReachableFromDb()` is what actually restricts
*which* database an analyzer name may refer to relative to the database
the request was sent to (`_vocbase.name()`):
- `createAnalyzer`/`removeAnalyzer` (non-getter mode): the analyzer's
  db-prefix must either be absent (implicitly the current database) or
  exactly equal to the current database — i.e. **you can only
  create/remove analyzers in the database the request targets**, never in
  some unrelated third database, regardless of privilege level.
- `getAnalyzer` (getter mode, `forGetters=true`): additionally allows a
  `_system`-prefixed name to be read from any database (a deliberate
  cross-db read exception for the system database only).

This restriction is identical in both branches and bounds the practical
impact of Finding 2 below.

### Finding 1 (Cosmetic): `bool` → `Result` return type of `canUse()`

`devel` (e.g. `devel:arangod/RestHandler/RestAnalyzerHandler.cpp:169`):
```cpp
if (!IResearchAnalyzerFeature::canUse(name, auth::Level::RW)) {
  generateError(arangodb::rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                std::string("insufficient rights while creating analyzer: ") +
                    body.toString());
  return;
}
```
Current branch (`arangod/RestHandler/RestAnalyzerHandler.cpp:168-173`):
```cpp
if (auto r =
        IResearchAnalyzerFeature::canUse(name, AnalyzerAccessLevel::Modify);
    r.fail()) {
  generateError(r);
  return;
}
```
Same pattern applies to `getAnalyzer()`
(`arangod/RestHandler/RestAnalyzerHandler.cpp:285-290` vs.
`devel:arangod/RestHandler/RestAnalyzerHandler.cpp:285-290`) and
`removeAnalyzer()` (`arangod/RestHandler/RestAnalyzerHandler.cpp:402-407`
vs. `devel:arangod/RestHandler/RestAnalyzerHandler.cpp:386-391`). In all
three call sites, denial still maps to `TRI_ERROR_FORBIDDEN` / HTTP `403`
in `Classic` mode: `canUse()` → `ExecContext::canUseAnalyzer()`
(`arangod/Utils/ExecContext.cpp:389-397`) → `can(UseAnalyzer{...})` →
`AuthMode::Classic::check()`'s `UseAnalyzer` branch
(`arangod/Auth/AuthMode.cpp:348-365`) → `check(UseDatabase{...})`
(`arangod/Auth/AuthMode.cpp:157-174`), whose failure branch (since
`requestedApiVersion() == 0` for this classic route, so the "report as not
found" masking added for the new API version never triggers here) always
returns `{TRI_ERROR_FORBIDDEN, "insufficient database access level for
'<db>'"}`. Only the error message text changes (from e.g. `"insufficient
rights while creating analyzer: {...body...}"` to `"insufficient database
access level for '<db>'"`); the HTTP status code and `errorNum` are
unchanged. Purely cosmetic.

### Finding 2 (Regression): missing "admin bypass" in `IResearchAnalyzerFeature::canUse(name, level)` — affects create / get-single / remove

This is the significant finding for this handler. `devel`'s `canUse()`
(`devel:arangod/IResearch/IResearchAnalyzerFeature.cpp:1155-1177`):
```cpp
bool IResearchAnalyzerFeature::canUse(std::string_view name,
                                      auth::Level const& level) {
  auto& ctx = ExecContext::current();

  if (ctx.isAdminUser()) {
    return true;  // authentication not enabled
  }

  auto& staticAnalyzers = getStaticAnalyzers();
  if (staticAnalyzers.contains(irs::hashed_string_view{name})) {
    return true;  // special case for singleton static analyzers
  }

  auto split = splitAnalyzerName(name);
  auto const vocbaseName = static_cast<std::string>(split.first);
  return irs::IsNull(split.first)
         || (ctx.canUseDatabase(vocbaseName, level)
             && ctx.canUseCollection(
                    vocbaseName, arangodb::StaticStrings::AnalyzersCollection,
                    level));
}
```
Current branch (`arangod/IResearch/IResearchAnalyzerFeature.cpp:1149-1178`):
```cpp
Result IResearchAnalyzerFeature::canUse(std::string_view name,
                                        AnalyzerAccessLevel const& level) {
  auto& staticAnalyzers = getStaticAnalyzers();
  if (staticAnalyzers.contains(irs::hashed_string_view{name})) {
    return {};  // special case for singleton static analyzers
  }

  auto split = splitAnalyzerName(name);
  ...
  auto& ctx = ExecContext::current();
  auto const vocbaseName = static_cast<std::string>(split.first);
  return ctx.canUseAnalyzer(vocbaseName, static_cast<std::string>(split.second),
                            level);
}
```
There is **no `isAdminUser()`/`isAdmin()` bypass anywhere in the current
call chain**: `ExecContext::canUseAnalyzer()`
(`arangod/Utils/ExecContext.cpp:389-397`) forwards straight into
`can(UseAnalyzer{...})`, and `AuthMode::Classic::check()`'s `UseAnalyzer`
branch (`arangod/Auth/AuthMode.cpp:348-365`) computes a required
`DatabaseAccessLevel` from `analyzer.level` and calls `check(UseDatabase{
analyzer.db, dbLevel})` with **no preceding `if (isAdmin().ok()) return
{};`** — unlike numerous sibling branches in the very same function
(`RestoreCollection`, `RestoreCreateIndex`, `RestoreCreateView`,
`RestoreDropView`, `RestoreWriteData`, `CreateDatabase`, `DropDatabase`,
`WriteUser`, `AnyAdmin`, all of which do have exactly this
`if (isAdmin().ok()) return {};` bypass, see e.g.
`arangod/Auth/AuthMode.cpp:257,268,292,302,312,322,374,378,563,367`).
`isAdmin()` (`arangod/Auth/AuthMode.cpp:574-577`) is
`check(UseDatabase{_system, Write})`, which was already proven equivalent
to `devel`'s precomputed `ctx.isAdminUser()` flag in the
`RestMetricsHandler`/`RestAdminServerHandler` sessions above (line
1047-1063): `isAdminUser() == (databaseAuthLevel(user, "_system",
configured=true) == RW)`.

**Consequence:** in `devel`, an admin (any identity holding `RW` on
`_system`) can create, read, or remove **any** analyzer reachable through
`analyzerReachableFromDb()` (i.e. one belonging to the database the
request targets, or — for reads only — the system database), *regardless
of that admin's actual configured access level on that specific target
database*. For example, an admin with `RW` on `_system` but explicitly
`NONE`/no grant at all on database `foo` could, in `devel`, still `POST
/_db/foo/_api/analyzer` to create an analyzer there, or `DELETE
/_db/foo/_api/analyzer/<name>` to remove one, or `GET
/_db/foo/_api/analyzer/<name>` to read one. In the current branch, that
same admin is now **denied** (`403 FORBIDDEN`, "insufficient database
access level for 'foo'") unless they separately hold `RW`
(create/remove)/`RO` (read) on `foo` itself. This is a genuine behavioral
divergence from `devel` triggerable through normal REST API usage, so it
is classified as a **Regression** per the methodology above — even though,
unlike most other regressions found in this investigation, this one goes
in the *safer* direction (current branch is *more* restrictive than
`devel`, not less). It could nonetheless break existing deployments/scripts
that rely on the `devel` convention (used consistently elsewhere for
restore/admin-style operations, see the bypass list above) that a `_system`
admin has implicit, unconditional analyzer-management rights over every
database.

Two sub-cases worth calling out explicitly:
- The listing endpoint `GET /_api/analyzer` (no suffix) is **not** affected
  by this particular finding — see Finding 3, it uses a different, already
  bypass-free gate in both branches.
- Because of `analyzerReachableFromDb()`'s restriction (see "Diff
  overview" above), the blast radius is narrower than, say, the
  `RestWalAccessHandler` regression: an admin can only be locked out of
  managing analyzers in (a) the database the request itself targets, or
  (b) (for reads only) the system database — never some unrelated third
  database, since the analyzer name syntax itself makes that unreachable
  in both branches.

### Finding 3 (Code-quality / latent risk, proven currently equivalent): listing gate no longer calls `canUseCollection(..., AnalyzersCollection, ...)`

`devel`'s `getAnalyzers()` gates access to a database's analyzers via
`canUse(vocbase, level)` → `canUseVocbase()`
(`devel:arangod/IResearch/IResearchAnalyzerFeature.cpp:1139-1148`):
```cpp
bool IResearchAnalyzerFeature::canUseVocbase(std::string_view vocbaseName,
                                             auth::Level const& level) {
  auto& ctx = ExecContext::current();
  return ctx.canUseDatabase(nameStr, level) &&
         ctx.canUseCollection(nameStr, StaticStrings::AnalyzersCollection,
                              level);
}
```
called as `IResearchAnalyzerFeature::canUse(_vocbase, auth::Level::RO)` /
`canUse(*sysVocbase, auth::Level::RO)`
(`devel:arangod/RestHandler/RestAnalyzerHandler.cpp:333,345`). The current
branch removed both the `canUse(TRI_vocbase_t const&, auth::Level)` and
`canUseVocbase()` overloads entirely (only the single-name overload
remains, `arangod/IResearch/IResearchAnalyzerFeature.h:313`) and inlined
just the database-level half of the check directly in the handler
(`arangod/RestHandler/RestAnalyzerHandler.cpp:344-346,358-361`):
```cpp
if (execContext
        .canUseDatabase(_vocbase.name(), arangodb::DatabaseAccessLevel::Read)
        .ok()) {
  analyzers.visit(visitor, &_vocbase, ...);
}
```
i.e. the `canUseCollection(..., AnalyzersCollection, ...)` half was
dropped. This looks like a narrowing at first glance, but it is provably a
no-op: `auth::User::collectionAuthLevel()`
(`arangod/Auth/User.cpp:728-748`, unchanged between branches) special-cases
*every* collection name starting with `_` (`isSystem` branch, line 736) to
skip the per-collection `_collectionAccess` grant table entirely — the
table lookup only happens in the non-system `else` branch
(`arangod/Auth/User.cpp:750-...`). Since `_analyzers` is not one of the
three hard-coded exceptions (`_users` → `NONE`, `_queues` → `RO`,
`_frontend` → `RW`, `arangod/Auth/User.cpp:739-746`), it always falls
through to `return databaseAuthLevel(dbname);` — i.e.
`collectionAuthLevel(db, "_analyzers", level)` is **always identical** to
`databaseAuthLevel(db)` compared against `level`, in both branches, and no
administrator can override this via a per-collection grant on
`_analyzers` (the code path to honor such a grant is unreachable for any
system-collection name). Consequently `canUseVocbase(db, level) ==
canUseDatabase(db, level)` always holds, and the current branch's inlined
check is behaviorally identical to `devel`'s `canUse(vocbase, level)` in
every case — today. This is recorded as a **code-quality / latent-risk**
item rather than a "clean" equivalence, however, because the
equivalence depends on an implementation detail elsewhere
(`User::collectionAuthLevel()`'s blanket exclusion of system-collection
names from the per-collection grant table) that is not enforced by any
invariant local to this handler; if that exclusion is ever narrowed
(e.g. system collections other than the three hard-coded ones become
independently grantable), the listing gate would silently start
diverging from a hypothetical `canUseCollection`-based check without any
compile-time or handler-level signal.

The per-analyzer filter added to the listing's visitor callback
(`arangod/RestHandler/RestAnalyzerHandler.cpp:324-330`,
`execContext.canSeeAnalyzer(split.first, split.second)`) is likewise a
no-op in practice: `canSeeAnalyzer()` maps to `AuthMode::Classic::check()`'s
`SeeAnalyzer` branch (`arangod/Auth/AuthMode.cpp:482-488`), which itself
just re-checks `UseDatabase{analyzer.db, Read}` — but `split.first` for
every analyzer actually visited by this loop is either `_vocbase.name()`
or `sysVocbase->name()`, both of which were already gated by the identical
`canUseDatabase(..., Read)` check one level up (lines 344-346, 358-361)
before `analyzers.visit()` was even called. `devel` has no equivalent
per-analyzer filter at all in its visitor
(`devel:arangod/RestHandler/RestAnalyzerHandler.cpp:317-323`) and doesn't
need one, for the same reason. No behavioral difference in either
direction.

### Finding 4 (Cosmetic): early read-only-mode short-circuit added to `canUseAnalyzer()`

`ExecContext::canUseAnalyzer()` (`arangod/Utils/ExecContext.cpp:389-397`):
```cpp
Result ExecContext::canUseAnalyzer(std::string_view db, std::string_view analyzer,
                                   AnalyzerAccessLevel level) const {
  if (!isSuperuser() && ServerState::readOnly() &&
      level == AnalyzerAccessLevel::Modify) {
    return {TRI_ERROR_FORBIDDEN, "Server is in read-only mode."};
  }
  return can(UseAnalyzer{.db{db}, .name{analyzer}, .level = level});
}
```
`devel`'s `IResearchAnalyzerFeature.cpp` has no equivalent
`ServerState::readOnly()`/`isSuperuser()` check anywhere (confirmed by
grep — no matches in the whole file). This does not constitute a new
restriction in practice: any actual write to the `_analyzers` system
collection still has to go through the normal RocksDB
transaction/storage-engine layer (`arangod/Transaction/Methods.cpp`,
`arangod/RocksDBEngine/Methods/RocksDBTrxBaseMethods.cpp`), which already
enforces `--server.read-only` independently of this handler in both
branches. The current branch's addition merely fails fast, earlier and
with a clearer message ("Server is in read-only mode.") instead of letting
the request reach the storage layer and fail there with a more generic
error. Both branches ultimately deny the write; only the exact point of
failure and error message differ. Cosmetic only.

### Summary for `RestAnalyzerHandler`

| Route / scenario | Verdict |
|---|---|
| `GET /_api/analyzer` (list) | Identical to `devel` (Finding 3: inlined/dropped `canUseCollection` half and the added per-analyzer `canSeeAnalyzer` filter are both no-ops given current `User::collectionAuthLevel()` semantics) |
| `GET /_api/analyzer/<name>`, `POST /_api/analyzer`, `DELETE /_api/analyzer/<name>` — non-admin caller with correct database-level access | Identical to `devel` (Finding 1: error message wording only, on denial) |
| `GET /_api/analyzer/<name>`, `POST /_api/analyzer`, `DELETE /_api/analyzer/<name>` — admin (`RW` on `_system`) without explicit access to the analyzer's own target database | **Regression** (Finding 2): `devel` allows this via the `isAdminUser()` bypass; current branch denies with `403 FORBIDDEN` |
| `POST`/`DELETE` while server is in `--server.read-only` mode | Identical end result (write denied) in both branches; current branch just fails earlier/clearer (Finding 4, cosmetic) |

**Action items / recommendations:**
1. **Fix Finding 2**: add an `if (isAdmin().ok()) return {}; ` bypass to
   `AuthMode::Classic::check()`'s `UseAnalyzer` branch
   (`arangod/Auth/AuthMode.cpp:348-365`), matching the convention already
   used by `RestoreCollection`/`RestoreCreateIndex`/`RestoreCreateView`/
   `RestoreDropView`/`RestoreWriteData`/`CreateDatabase`/`DropDatabase`/
   `WriteUser`/`AnyAdmin` in the same function, and mirroring `devel`'s
   `ctx.isAdminUser()` short-circuit at the top of
   `IResearchAnalyzerFeature::canUse()`. This is a purely `Classic`-mode
   fix (adding it to `AuthMode::Classic::check()` rather than to
   `IResearchAnalyzerFeature::canUse()` itself) and does not affect the
   new RBAC mode.
2. No action required for Finding 1, Finding 3, or Finding 4 (all
   cosmetic/no-op today); Finding 3 is flagged for awareness only, in case
   `User::collectionAuthLevel()`'s system-collection exclusion is ever
   revisited.

## `RestImportHandler` (`arangod/RestHandler/RestImportHandler.cpp`)

Mounted at `/_api/import` (prefix, `POST` only). Implements bulk document
import in three body formats (`type=documents`/`array`/`list`/`auto`
via `createFromJson()`, raw VelocyPack via `createFromVPack()`, and
headless CSV-style key/value lines via `createFromKeyValueList()`), all of
which funnel into the shared `performImport()` (a batched
`trx.insertAsync()`) and, when `overwrite=true` is given, a preceding
`trx.truncateAsync()`. Like `RestDocumentHandler`, this handler contains
**no authorization logic of its own at all** — no `ExecContext`/`canUse*`
call appears anywhere in `RestImportHandler.cpp` (confirmed by grep: zero
matches for `ExecContext|canUse|auth::`). Every operation constructs a
`SingleCollectionTransaction` directly (`arangod/RestHandler/RestImportHandler.cpp:349-350,558-559,761-762`,
`AccessMode::Type::WRITE`) and calls `co_await trx.beginAsync()`; all
authorization happens inside that shared transaction machinery — the same
`TransactionState::checkCollectionPermission()` path already fully
analyzed in the `RestDocumentHandler` section above — not in this file.

### Diff overview

`RestImportHandler.h` is byte-for-byte identical apart from the removed
`@author` line. `RestImportHandler.cpp` differs only in: (a) the removed
`@author` line, (b) an added mount-point comment
(`arangod/RestHandler/RestImportHandler.cpp:56`), and (c) three call sites
where `OperationOptions truncateOpts(_context);`
(`devel:arangod/RestHandler/RestImportHandler.cpp:378,790`) /
`OperationOptions opOptions(_context);`
(`devel:arangod/RestHandler/RestImportHandler.cpp:1096`) became
`OperationOptions truncateOpts;` (`arangod/RestHandler/RestImportHandler.cpp:378,790`) /
`OperationOptions opOptions;` (`arangod/RestHandler/RestImportHandler.cpp:1096`)
— the exact same `_context`-member-removed-from-`OperationOptions` change
already investigated for `RestDocumentHandler` (see there). Every other
line of `createFromJson()`, `createFromVPack()`, `createFromKeyValueList()`,
`handleSingleDocument()`, `performImport()`, and the CSV/VelocyPack parsing
helpers is untouched.

Regarding (c): as established in the `RestDocumentHandler` section, the
only call site anywhere in `devel` that ever reads
`OperationOptions::context()` — as opposed to falling back to
`ExecContext::current()` — is `Collections::create`
(`devel:arangod/VocBase/Methods/Collections.cpp:604`, re-confirmed here
via `git grep -n "\.context()" devel` across the whole tree, ignoring
unrelated 3rd-party/boost hits: the *only* production call site is that
one line). `RestImportHandler` never calls `Collections::create` (it only
ever operates on a pre-existing collection resolved by
`trx.resolver()->getCollection(collectionName)` — there is no
"auto-create collection" option anywhere in this handler, in either
branch). So, exactly as for `RestDocumentHandler`, dropping the explicit
`_context` argument is a confirmed no-op here: `ExecContext::current()` is
always the same context object for the synchronous request-handling path.

### Addendum: this handler is also subject to Finding 1 of `RestDocumentHandler` (read-only-mode error-code regression)

All three import variants perform a real, write-mode transaction via
`trx.insertAsync()` (through `performImport()`,
`arangod/RestHandler/RestImportHandler.cpp:898-975`) and, when
`overwrite=true`, additionally a `trx.truncateAsync()`
(`arangod/RestHandler/RestImportHandler.cpp:377-382,586-591,789-794`)
beforehand. Both operations are gated by the identical
`TransactionState::checkCollectionPermission()` machinery already analyzed
in Finding 1 of the `RestDocumentHandler` section above (line 788-935):
while the server is globally in `--server.read-only` mode, a caller whose
*configured* grant on the target collection is `RW` (i.e. the ordinary
case for anyone actually allowed to import data) receives
`TRI_ERROR_FORBIDDEN` (11) from the current branch instead of `devel`'s
more specific `TRI_ERROR_ARANGO_READ_ONLY` (1004); the HTTP status code
(`403`) is unchanged in both. This is not a new, `/_api/import`-specific
defect — it is the same cross-cutting regression, reachable here through
one more call site — so it is not counted as a separate finding, but is
recorded for completeness (as was already done for `RestCollectionHandler`'s
`truncate` route in the addendum to that section).

### Summary for `RestImportHandler`

| Route / scenario | Verdict |
|---|---|
| `POST /_api/import?type=documents\|array\|list\|auto` (JSON body) | Identical to `devel` (no authorization logic in the handler itself; gated entirely by the generic database-access gate plus `TransactionState::checkCollectionPermission()`) |
| `POST /_api/import` (VelocyPack body) | Identical to `devel` |
| `POST /_api/import` (headless CSV/key-value body) | Identical to `devel` |
| Any of the above with `overwrite=true`, or any insert, while the server is in `--server.read-only` mode | Same cross-cutting regression as `RestDocumentHandler` Finding 1 (`TRI_ERROR_FORBIDDEN` instead of `TRI_ERROR_ARANGO_READ_ONLY`; same HTTP status) — not a new, handler-specific finding |
| `OperationOptions(_context)` → `OperationOptions()` (3 call sites) | Confirmed no-op, same reasoning as `RestDocumentHandler` |

**Action items / recommendations:** None specific to this handler. It
inherits Finding 1 from the `RestDocumentHandler` section (already tracked
there with its own fix recommendation) and is otherwise a clean handler —
the third one, after `RestCompactHandler` and `RestAgencyCallbacksHandler`,
found to contain no handler-local authorization code or divergence of its
own.

## `RestUsageMetricsHandler`, `RestEngineHandler` and `RestSupportInfoHandler`

Three small, monitoring/introspection-style handlers, covered together
since each turns out to reuse an authorization pattern already fully
proven equivalent in earlier sessions of this document — confirming the
expectation that they would be low-risk.

- `RestUsageMetricsHandler` (`arangod/RestHandler/RestUsageMetricsHandler.cpp`)
  — mounted at `/_admin/usage-metrics` (prefix, `GET` only); exports
  dynamic (per-shard) Prometheus metrics, with an optional
  cluster-internal `serverId` redirection.
- `RestEngineHandler` (`arangod/RestHandler/RestEngineHandler.cpp`) —
  mounted at `/_api/engine` (prefix, `GET` only); `GET /_api/engine`
  returns storage-engine capabilities (unauthenticated-safe, no
  suffix-specific check), `GET /_api/engine/stats` returns storage-engine
  statistics (hardened-gated).
- `RestSupportInfoHandler` (`arangod/RestHandler/RestSupportInfoHandler.cpp`)
  — mounted at `/_admin/support-info` (exact, `GET` only); returns a
  deployment/version/host support-info bundle, gated by the
  `--server.support-info-api` policy (`jwt`/`admin`/`public`/`disabled`).

### Finding 1 (Cosmetic): `RestUsageMetricsHandler`/`RestEngineHandler` — `ServerSecurityFeature::canAccessHardenedApi()` → `canUseHardenedAction(AdminMonitoringInternal{})`

Both handlers changed identically. `devel`
(`devel:arangod/RestHandler/RestUsageMetricsHandler.cpp:47-56`,
`devel:arangod/RestHandler/RestEngineHandler.cpp:71-78`):
```cpp
auto& security = server().getFeature<ServerSecurityFeature>();
if (!security.canAccessHardenedApi()) {
  // don't leak information about server internals here
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
  co_return; // or return;
}
```
Current branch (`arangod/RestHandler/RestUsageMetricsHandler.cpp:48-55`,
`arangod/RestHandler/RestEngineHandler.cpp:71-78`):
```cpp
if (auto r = ExecContext::current().canUseHardenedAction(
        auth::perms::AdminMonitoringInternal{});
    r.fail()) {
  // don't leak information about server internals here
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                r.errorMessage());
  co_return; // or return;
}
```
This is the exact same `ServerSecurityFeature::canAccessHardenedApi()` →
`ExecContext::canUseHardenedAction()` migration already fully traced and
proven equivalent for `RestMetricsHandler` above (line 994-1063): the
"hardened-off" gate (`--server.harden`, default `false`) short-circuits to
always-allow identically in both branches, and the "hardened-on" gate
reduces, in both branches, to exactly `databaseAuthLevel(user, "_system",
configured=true) == RW` (`devel`'s precomputed `isAdminUser()` vs. the
current branch's `isAdmin()`/`AnyAdmin` dispatch in
`AuthMode::Classic::check()`, `arangod/Auth/AuthMode.cpp:367,574-577`).
`AdminMonitoringInternal` is one of the plain `AnyAdmin`-category
permissions (`arangod/Auth/Permissions.h:86,112-118`, alongside
`AdminMonitoring`/`AdminOptions`/`AdminApiCalls`/etc., all already shown to
map to the same `isAdmin()` check), so no new reasoning is needed here —
the equivalence proof carries over verbatim. The only observable
difference is, again, the error message: `devel` returns bare
`TRI_ERROR_FORBIDDEN` with no message text; the current branch adds
`r.errorMessage()` (`"insufficient database access level for '_system'"`).
The HTTP status (`403`) and `errorNum` (`TRI_ERROR_FORBIDDEN`) are
identical in both branches. Purely cosmetic.

Note that `RestEngineHandler`'s `GET /_api/engine` (no suffix,
`getCapabilities()`) never reaches this check in either branch — only the
`/stats` suffix (`getStats()`) is hardened-gated — and that branching logic
(`arangod/RestHandler/RestEngineHandler.cpp:57-82`) is byte-for-byte
identical to `devel` (`devel:arangod/RestHandler/RestEngineHandler.cpp:56-82`).

### Finding 2 (Cosmetic, unrelated): `RestEngineHandler` — `EngineSelectorFeature` lookup replaced by cached `DatabaseFeature::engine()` reference

```diff
- StorageEngine& engine = server().getFeature<EngineSelectorFeature>().engine();
- engine.getCapabilities(result);
+ _engine.getCapabilities(result);   // _engine cached in the constructor from
+                                     // server.getFeature<DatabaseFeature>().engine()
```
Same underlying `StorageEngine` singleton, just obtained once at
construction time via `DatabaseFeature` instead of looked up per-call via
`EngineSelectorFeature` — the identical internal-API simplification already
seen for `RestDocumentHandler`/`RestAdminServerHandler`/`RestCompactHandler`
in earlier sessions. No behavioral or authorization impact.

### Finding 3 (Verified equivalent / narrow non-security divergence): `RestSupportInfoHandler`'s `apiPolicy` gate

`RestSupportInfoHandler::execute()`
(`arangod/RestHandler/RestSupportInfoHandler.cpp:43-73`) implements
essentially the same three-way `"jwt"`/`"admin"`/`"public"` policy switch
as `RestOptionsBaseHandler::checkAuthentication()`, already fully analyzed
in the `RestOptions*` handler family session above (Finding 2, line
1886-1978) — and the diff confirms the refactor is the very same
transformation, applied to a near-identical code block:

- `"jwt"` policy (`arangod/RestHandler/RestSupportInfoHandler.cpp:48-54`):
  `devel`'s bare `!ExecContext::current().isSuperuser()`
  (`devel:arangod/RestHandler/RestSupportInfoHandler.cpp:48-49`, **not**
  guarded by `isAuthEnabled()`, exactly like the `RestOptions*` case) →
  current's `!ExecContext::current().isSuperuserOrDisabled()`. By the same
  reasoning already established for `RestOptions*` Finding 2: identical
  ALLOW/DENY whenever authentication is active; a narrow, *more permissive*
  (not a security regression) divergence exists only when
  `--server.authentication false` **and** the client still supplies a
  non-empty username, in which case `devel` would incorrectly reject with
  `403` while the current branch allows it.
- `"admin"` policy (`arangod/RestHandler/RestSupportInfoHandler.cpp:56-64`):
  `devel`'s `apiPolicy == "admin" && !ExecContext::current().isAdminUser()`
  (`devel:arangod/RestHandler/RestSupportInfoHandler.cpp:56`) → current's
  `apiPolicy == "admin" && r.fail()` where `r =
  ExecContext::current().canUseAdminAction(auth::perms::AdminMonitoring{})`.
  `AdminMonitoring` is again a plain `AnyAdmin` permission, dispatching to
  `isAdmin()` — identical ALLOW/DENY decision as `devel`'s `isAdminUser()`
  in every case, cosmetic message-text change only (`"insufficient
  permissions"` → `r.errorMessage()`); the `errorNum`
  (`TRI_ERROR_HTTP_FORBIDDEN`/`403`) is unchanged in both branches, exactly
  as for the `RestOptions*` `"admin"`-policy case.
- `"public"` policy: no checks in either branch (both fall through to the
  system-database-only restriction below).
- The final `_request->databaseName() != StaticStrings::SystemDatabase`
  restriction (`arangod/RestHandler/RestSupportInfoHandler.cpp:68-73`) is
  byte-for-byte unchanged.

The one remaining diff — `SupportInfoBuilder::buildInfoMessage(result,
dbName, server, isLocal, false)` (`devel`, 5 args) →
`SupportInfoBuilder::buildInfoMessage(result, dbName, server, isLocal)`
(current, 4 args) — is not an authorization change: `devel`'s trailing
`isTelemetricsReq` parameter defaults to `false`
(`devel:arangod/Utils/SupportInfoBuilder.h:41`) and `RestSupportInfoHandler`
always explicitly passed `false` anyway (only `RestTelemetricsHandler`
would have passed `true`); the current branch's `SupportInfoBuilder`
dropped the parameter entirely (`arangod/Utils/SupportInfoBuilder.h:41-44`,
out of scope here — this affects `RestTelemetricsHandler`, not this
handler). Since the value used by `RestSupportInfoHandler` was already
`false` in both branches, this is a no-op for this specific handler.

### Summary

| Handler / Route | Verdict |
|---|---|
| `GET /_admin/usage-metrics` | Identical to `devel` (Finding 1, cosmetic message-text only) |
| `GET /_api/engine` (capabilities) | Identical to `devel` — no auth check in either branch |
| `GET /_api/engine/stats` | Identical to `devel` (Finding 1, cosmetic message-text only) |
| `GET /_admin/support-info`, `"jwt"` policy | Identical to `devel` when auth is enabled; narrow, more-permissive-only divergence when auth is fully disabled and a stray username is supplied (same as `RestOptions*` Finding 2, not a security regression) |
| `GET /_admin/support-info`, `"admin"` policy | Identical ALLOW/DENY to `devel`, cosmetic message-text change only |
| `GET /_admin/support-info`, `"public"` policy | Identical to `devel` (no checks in either branch) |

**Action items / recommendations:** None. All three handlers reuse
authorization patterns already established as equivalent (or,
for the one narrow divergence, already assessed as a safe, non-security
change and not requiring a fix) in earlier sessions; no new regressions
were found.

## `RestAqlFunctionsHandler`, `RestEndpointHandler` and `RestAccessTokenHandler`

Three more small handlers, covered together. The first two turned out to
have zero authorization-relevant diff at all; the third
(`RestAccessTokenHandler`) has several real, worthwhile findings.

- `RestAqlFunctionsHandler` (`arangod/RestHandler/RestAqlFunctionsHandler.cpp`)
  — mounted at `/_api/aql-builtin` (prefix, `GET` only); dumps the built-in
  AQL function inventory. **No authorization code of any kind, in either
  branch.** The diff against `devel` is purely cosmetic (`@author` line
  removed, one comment added,
  `arangod/RestHandler/RestAqlFunctionsHandler.cpp:37`). No finding.
- `RestEndpointHandler` (`arangod/RestHandler/RestEndpointHandler.cpp`) —
  mounted at `/_api/endpoint` (prefix, `GET` only); lists configured HTTP
  endpoints, gated only by `!_vocbase.isSystem()` →
  `TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE`
  (`arangod/RestHandler/RestEndpointHandler.cpp:63-67`), byte-for-byte
  identical to `devel`. Same cosmetic-only diff pattern (`@author` line,
  one comment). No finding.
- `RestAccessTokenHandler` (`arangod/RestHandler/RestAccessTokenHandler.cpp`)
  — mounted at `/_api/token` (prefix); lets a user list/create/delete their
  own (or, if admin, another user's) API access tokens. Unlike the two
  handlers above, this one has real authorization-logic changes, detailed
  below.

### Finding 1 (Verified equivalent, faithful gap-fill, one benign edge case): new `checkUserCanAccess()` override reproduces devel's `/_api/token/` path exception

`arangod/RestHandler/RestAccessTokenHandler.h:41-42` and
`arangod/RestHandler/RestAccessTokenHandler.cpp:97-104` (both new in this
branch):
```cpp
async<Result> RestAccessTokenHandler::checkUserCanAccess() const {
  // This API only requires the user to be authenticated
  if (request()->authenticated()) {
    co_return Result{};
  }
  co_return Result{TRI_ERROR_HTTP_UNAUTHORIZED, "Not authenticated."};
}
```
This reproduces the same `devel`-era `CommTask::canAccessPath()` path
exception already documented for `RestClusterHandler`/`RestUsersHandler` in
earlier sessions:
```cpp
constexpr std::string_view pathPrefixApiToken("/_api/token/");
...
} else if (userAuthenticated && path.starts_with(::pathPrefixApiToken)) {
  result = Flow::Continue;
}
```
(`/tmp/devel_CommTask.cpp:68,869-870`, only reached when the generic
per-database gate already failed, i.e. `vc->databaseAuthLevel() == NONE` —
exactly mirroring the architectural background section above). Neither
branch escalates to superuser for this exception, matching exactly.

Unlike the `RestUsersHandler`/`RestClusterHandler` overrides, which
explicitly re-check the path prefix (`arangod/RestHandler/RestUsersHandler.cpp:111-121`,
`.../RestClusterHandler.cpp:136-145`), this override skips the path check
entirely and just tests `request()->authenticated()`. Since this handler is
only ever reached for requests already routed to the `/_api/token` mount
point, this makes no difference for any request carrying a `{user}` path
segment (`suffixes.size() >= 1`, checked at
`arangod/RestHandler/RestAccessTokenHandler.cpp:60-64`), because such a
request's path always literally starts with `/_api/token/` — the condition
`devel` checks is unconditionally true whenever a suffix is present. The
only theoretical divergence is a **bare, suffix-less** authenticated
request (`GET`/`POST`/`DELETE /_api/token`, no trailing segment) coming
from a user who additionally has *no* access to the `_system` database
(the only case where `devel`'s generic per-database gate doesn't already
grant access on its own, forcing a fall-through to the path-exception
list): `devel` would reject such a request with **`401
TRI_ERROR_HTTP_UNAUTHORIZED`** at the framework level (path
`"/_api/token"` does not start with `"/_api/token/"`, so the exception
does not fire, and there is no other path-independent way in for this
user), whereas the current branch's override returns success unconditionally
for any authenticated caller, reaches `execute()`, and is rejected there
with **`400 TRI_ERROR_HTTP_BAD_PARAMETER`** ("path parameter 'user' is
missing", `arangod/RestHandler/RestAccessTokenHandler.cpp:60-64`). Both
branches reject the request and return no data in either case — this is a
change in error status/code for a degenerate, practically-unreachable
request shape (no normal client ever omits the `{user}` segment), not a
security-relevant divergence. **Cosmetic/benign**, flagged only for
completeness.

### Finding 2 (Verified equivalent): per-verb split of `devel`'s single `canAccessUser()` gate into `canReadUser()`/`canWriteUser()`

`devel` (`/tmp/devel_RestAccessTokenHandler.cpp:66-70`) ran a single,
verb-independent check before dispatching:
```cpp
if (!canAccessUser(user)) {
  generateError(ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
  return RestStatus::DONE;
}
auto const type = _request->requestType();
switch (type) { ... }
```
with (`/tmp/devel_RestBaseHandler.cpp:67-72`):
```cpp
bool RestBaseHandler::canAccessUser(std::string const& user) const {
  if (_request->authenticated() && user == _request->user()) {
    return true;
  }
  return isAdminUser();
}
```
The current branch instead calls a verb-specific check per case
(`arangod/RestHandler/RestAccessTokenHandler.cpp:71-89`): `GET` →
`exec.canReadUser(user)`, `POST`/`DELETE` → `exec.canWriteUser(user)`. Both
(`arangod/Utils/ExecContext.cpp:441-462`) special-case "acting on oneself"
first, exactly like `devel`'s `user == _request->user()` branch:
```cpp
Result ExecContext::canReadUser(std::string_view userName) const {
  if (userName == user()) return {};
  return can(ReadUser{.name{userName}});
}
Result ExecContext::canWriteUser(std::string_view userName) const {
  if (!isSuperuser() && ServerState::readOnly()) {
    return {TRI_ERROR_FORBIDDEN, "Server is in read-only mode."};
  }
  if (userName == user()) return {};
  return can(WriteUser{.name{userName}});
}
```
and, for any other target user, both `ReadUser`/`WriteUser` permissions
reduce, in `AuthMode::Classic::check()`
(`arangod/Auth/AuthMode.cpp:560-569`), to `isAdmin()`
(`arangod/Auth/AuthMode.cpp:574-577`: `check(UseDatabase{"_system",
Write})`) — precisely `devel`'s `isAdminUser()` semantics (RW access to
`_system`). So for the "self" case and the "admin acting on another user"
case, the ALLOW/DENY decision is exactly equivalent between branches for
both the `GET` and `POST`/`DELETE` routes. The only substantive change is
the read-only-mode gate added to `canWriteUser()`, discussed next as its
own finding since it *is* a genuine behavioral difference, not merely a
verb split.

### Finding 3 (Regression, narrow but real and confirmed): `canWriteUser()`'s new read-only-mode gate actually changes behavior here, unlike the read-only-mode "duplicate guard" pattern seen elsewhere

This is *not* another instance of the "harmless, redundant, pre-emptive
read-only check" pattern already documented for other handlers (e.g.
`RestAnalyzerHandler` Finding 4, `RestImportHandler`'s addendum): here the
extra guard **does** change the observable outcome, because of how
`RestAccessTokenHandler`'s underlying token storage write is implemented.

- `POST`/`DELETE /_api/token/{user}` ultimately call
  `auth::UserManagerBase::createAccessToken()`/`deleteAccessToken()`
  (`arangod/Auth/UserManagerBase.cpp:296-315`), which both go through
  `UserManagerImpl::updateUser()` and finish in
  `UserManagerImpl::storeUserInternal()`
  (`arangod/Auth/UserManagerImpl.cpp:362-...`), which explicitly says:
  ```cpp
  // we cannot set this execution context, otherwise the transaction
  // will ask us again for permissions and we get a deadlock
  ExecContextSuperuserScope scope;
  ```
  (`arangod/Auth/UserManagerImpl.cpp:377`) — i.e. the actual write to the
  `_users` system collection is deliberately performed with the
  `ExecContext` **forced to superuser** for the duration of the write
  transaction.
- The storage-engine-level read-only-mode enforcement that normally blocks
  writes in `--server.read-only` mode
  (`arangod/Transaction/Methods.cpp:3763-3769`,
  `arangod/RocksDBEngine/Methods/RocksDBTrxBaseMethods.cpp:454`) is itself
  gated by `!exec.isSuperuserOrDisabled()`:
  ```cpp
  bool cancelRW = ServerState::readOnly() && !exec.isSuperuserOrDisabled();
  ```
  Because of the `ExecContextSuperuserScope` above, this check evaluates to
  `cancelRW == false` for the token write — meaning the storage layer does
  **not** block it, in either branch (this code path is unchanged and
  common to both).
- Consequently, in `devel`, `canAccessUser()` has no read-only-mode check
  of its own (see Finding 2), and the actual write bypasses the
  storage-engine's read-only gate via the forced-superuser scope — so a
  `POST`/`DELETE /_api/token/{user}` request **succeeds even while the
  server is running with `--server.read-only=true`**, for both the "self"
  and "admin acting on another user" cases.
- In the current branch, `ExecContext::canWriteUser()`
  (`arangod/Utils/ExecContext.cpp:452-462`) now checks
  `!isSuperuser() && ServerState::readOnly()` **before** anything else,
  evaluated against the *caller's own*, not-yet-superuser `ExecContext`
  (`ExecContext::current()` at
  `arangod/RestHandler/RestAccessTokenHandler.cpp:70,79,85`). For any
  non-superuser caller (i.e. essentially all normal HTTP requests,
  including the `_system` root user acting on themselves or on another
  user), this now returns `{TRI_ERROR_FORBIDDEN, "Server is in read-only
  mode."}` and the request is rejected with `generateError(r)`
  (`arangod/RestHandler/RestAccessTokenHandler.cpp:79-82,85-88`) — **before**
  ever reaching `um->createAccessToken()`/`deleteAccessToken()`.

This is a genuine, confirmed new restriction: management of one's own (or,
for admins, another user's) API access tokens — an operation that does not
touch application data and was always available in `devel` regardless of
`--server.read-only` — is now blocked while the server is in read-only
mode. It is *safer* (more restrictive), not a security hole, but it is a
real behavioral divergence that could break operational workflows (e.g. an
admin trying to revoke a compromised token during a read-only maintenance
window). `GET` (`canReadUser()`) is unaffected — no read-only check was
added there, matching `devel`.

### Summary

| Handler / Route | Verdict |
|---|---|
| `GET /_api/aql-builtin` | Identical to `devel` — no auth check in either branch |
| `GET /_api/endpoint` | Identical to `devel` — unchanged `isSystem()` guard |
| `GET/POST/DELETE /_api/token/*`, framework-level gate | Verified equivalent to `devel`'s path-based exception (Finding 1); one benign edge-case status-code difference (401→400) for a degenerate, suffix-less request that is rejected either way |
| `GET /_api/token/{user}` | Identical ALLOW/DENY to `devel` (Finding 2) |
| `POST`/`DELETE /_api/token/{user}[/{id}]`, normal operation | Identical ALLOW/DENY to `devel` (Finding 2) |
| `POST`/`DELETE /_api/token/{user}[/{id}]`, server in `--server.read-only` mode | **Regression**: `devel` allows it (bypasses read-only via forced-superuser write scope); current branch now rejects with `403 TRI_ERROR_FORBIDDEN` (Finding 3) |

**Action items / recommendations:** Finding 3 is the one actionable item.
If read-only mode is intended to block user/token management too, this is
a deliberate and acceptable hardening — but it should be a conscious
decision, and ideally applied consistently to `RestUsersHandler` as well
(not analyzed in this session; `RestUsersHandler` also uses
`canReadUser`/`canWriteUser`/`canReadUsers`, per
`Documentation/path_permissions.md:1037-1114`, so it likely has the exact
same new restriction — worth confirming in a future session). If, instead,
parity with `devel` is desired, the read-only check should be removed from
`ExecContext::canWriteUser()` (`arangod/Utils/ExecContext.cpp:454-456`),
mirroring the fact that the underlying storage write already deliberately
opts out of the read-only gate via `ExecContextSuperuserScope`.

## `RestReplicationHandler` (+ `RocksDBRestReplicationHandler` / `ClusterRestReplicationHandler`)

This is by far the most complex handler examined so far, and — unusually —
the outcome is **not** "current branch regressed relative to `devel`", but
the opposite: this session found that `devel` itself has (or had) a set of
genuine, significant authorization gaps in this handler, which the current
branch has fixed. Per the task's methodology, these are documented as
findings even though they are improvements rather than regressions, since
they represent real behavioural divergence from `devel`.

> **Correction (post-review):** the first pass of this analysis was made
> against a local `devel` checkout that turned out to be stale (behind
> `origin/devel`). After updating the local `devel` branch to the current
> tip of `origin/devel` and re-diffing every file involved
> (`RestReplicationHandler.{cpp,h}`, `RocksDBRestReplicationHandler.{cpp,h}`,
> `RocksDBReplicationContext.cpp`, `ClusterRestReplicationHandler.{cpp,h}`),
> **Finding 3 below (the `DBserver`-forwarding restriction) turned out to
> already be present, byte-for-byte identical, in the up-to-date `devel`** —
> it was fixed upstream in `devel` independently of, and prior to, this
> branch's refactor, so it is **not** a divergence between the current
> branch and `devel` after all. It has been struck accordingly (kept in the
> document, marked as retracted, for an honest record). **Findings 1, 2, 4,
> 5 and 6 were all re-verified line-by-line against the up-to-date `devel`
> and are confirmed to still hold exactly as originally described** — in
> particular, the headline **Finding 2 (missing `restore-indexes`/
> `restore-view` permission checks in `devel`) is still a genuine, current
> gap in `devel`**, not something already fixed upstream.

Three files/classes are in play:

- `RestReplicationHandler` (`arangod/RestHandler/RestReplicationHandler.cpp`)
  — the shared base class, used directly on single servers and as the base
  of the other two. Mounted at `/_api/replication` (prefix), dispatching
  ~30 different sub-commands via the first path suffix.
- `RocksDBRestReplicationHandler`
  (`arangod/RocksDBEngine/RocksDBRestReplicationHandler.cpp`) — overrides
  the storage-engine-specific commands (`batch`, `logger-follow`,
  `inventory`, `keys/*`, `dump`, `revision-tree`) for single-server/DBServer.
- `ClusterRestReplicationHandler`
  (`arangod/ClusterEngine/ClusterRestReplicationHandler.cpp`) — the
  coordinator override, which simply stubs out the same engine-specific
  commands with `TRI_ERROR_NOT_IMPLEMENTED`
  (`arangod/ClusterEngine/ClusterRestReplicationHandler.cpp:37-83`), since
  coordinators have no local storage engine/WAL. This file's diff against
  `devel` is **purely cosmetic** (`@author` line removed only) — no finding.

### Finding 1 (Cosmetic, confirmed no-op): `testPermissions()` cleanup

`Result RestReplicationHandler::testPermissions()`
(`arangod/RestHandler/RestReplicationHandler.cpp:809-896`) had two changes
relative to `devel`, both confirmed to be behaviourally inert:

- The `if (!_request->authenticated()) return TRI_ERROR_NO_ERROR;` early
  return at the top of `devel`'s version was removed. As already
  established for other handlers, this is unreachable dead code: any
  request that is not `authenticated()` is aborted by
  `CommTask::canAccessPath()` before a handler is even created for any path
  that isn't one of the small set of documented, hard-coded exceptions
  (`/tmp/devel_CommTask.cpp:803`, `Flow result = userAuthenticated ?
  Flow::Continue : Flow::Abort;`), and `/_api/replication/*` is not among
  those exceptions. When authentication is disabled entirely,
  `req.authenticated()` is unconditionally `true` (`Entry::Superuser()`,
  `arangod/Auth/TokenCache.h:63`), so the check never fires either way.
- `devel`'s outer condition also matched `command == Batch` and
  `command == Inventory && GET`, but neither of those branches was ever
  connected to any inner action — matching them changed nothing (dead,
  vestigial conditions from an earlier version of the function). The
  current branch dropped them; this is a no-op cleanup, not a behavioural
  change.

### Finding 2 (Devel security gap, now fixed): `restore-indexes` and `restore-view` had **no permission check at all** in `devel`

This is the headline finding for this handler. In `devel`:

- `testPermissions()` (`/tmp/devel_RestReplicationHandler.cpp:803-903`) only
  ever validates four cases: `Batch` (a no-op match, see Finding 1),
  `Inventory` GET (ditto), `Dump` GET, and `RestoreCollection` PUT. The
  `RestoreIndexes` and `RestoreView` commands are **not mentioned at all**.
- `RestReplicationHandler::processRestoreIndexes()`
  (`/tmp/devel_RestReplicationHandler.cpp:1809-...`) and
  `processRestoreIndexesCoordinator()` create indexes by calling
  `physical->createIndex(idxDef, /*restore*/ true, created)` /
  `ClusterIndexMethods::ensureIndexCoordinator(...)` **directly**, with no
  `SingleCollectionTransaction` or other permission-checked code path in
  between (unlike document writes, which go through
  `TransactionState::checkCollectionPermission()`, see the
  `RestDocumentHandler`/`RestImportHandler` sessions). The only
  authorization-adjacent code present is
  `ExecContextSuperuserScope escope(ExecContext::current().isSuperuser() ||
  (ExecContext::current().isAdminUser() && !ServerState::readOnly()));`
  (`/tmp/devel_RestReplicationHandler.cpp:1917-1919` old numbering) — but
  this only *conditionally escalates admins to superuser*; for a
  **non-admin**, the condition is `false`, no escalation happens, and
  execution proceeds to `createIndex()` regardless, with **no rejection of
  any kind**.
- `RestReplicationHandler::handleCommandRestoreView()`
  (`/tmp/devel_RestReplicationHandler.cpp:2085-2166`) is worse still: it
  calls `view->drop()` and `LogicalView::create(...)` **unconditionally,
  with zero authorization code whatsoever** — no check, no
  superuser-escalation-if-admin pattern, nothing.
- Net effect in `devel`: **any authenticated user with at least
  database-level access other than `NONE`** (the only generic gate applied
  before a handler is reached at all, `CommTask::canAccessPath()`,
  `/tmp/devel_CommTask.cpp:803-810`) could call
  `PUT /_api/replication/restore-indexes` or
  `PUT /_api/replication/restore-view` on a database they only have `RO`
  access to (or even no explicit collection/view grants at all beyond the
  database default) and create/overwrite arbitrary indexes or views on any
  collection in that database — a real privilege-escalation gap.

The current branch fixes this with explicit checks added directly in the
processing functions, for both the single-server/DBServer and coordinator
variants:
- `processRestoreIndexes()`:
  `if (auto r = exec.canRestoreCreateIndex(_vocbase.name(), name); r.fail())
  return r;` (`arangod/RestHandler/RestReplicationHandler.cpp:1901-1905`).
- `processRestoreIndexesCoordinator()`: the same check
  (`arangod/RestHandler/RestReplicationHandler.cpp:2035-2039`).
- `processRestoreView()` (called from `handleCommandRestoreView()`): a
  `canRestoreDropView()` check before dropping an existing view
  (`arangod/RestHandler/RestReplicationHandler.cpp:2189-2192`) and a
  `canRestoreCreateView()` check before creating the replacement
  (`arangod/RestHandler/RestReplicationHandler.cpp:2208-2214`).

All four new checks reduce, in Classic mode
(`arangod/Auth/AuthMode.cpp:287-327`), to "is `_system`-DB admin, OR has
the specific `WriteMeta`/`CreateView`/`DropView` access needed" — i.e.
exactly the access level that should always have been required. This is a
genuine, confirmed security fix relative to `devel`, not a regression.

(By contrast, restoring data into the `_users`/`_analyzers` system
collections, via `processRestoreData()`'s special-cased
`processRestoreUsersBatch()`/`processRestoreCoordinatorAnalyzersBatch()`
paths, was **not** actually a hole in `devel` despite the same
"escalate-only-if-admin, no explicit check" pattern
(`/tmp/devel_RestReplicationHandler.cpp:1317-1319`): both go through an AQL
query / `IResearchAnalyzerFeature` write that is gated by the standard
collection-permission machinery, and — as already established for
`RestAnalyzerHandler` (Finding 3) — system collections such as `_users`
and `_analyzers` always fall back to database-level access in
`auth::User::collectionAuthLevel()`, so "has `RW` on `_users`" and "is
`_system`-DB admin" are one and the same condition in Classic mode. No
finding there.)

### ~~Finding 3~~ (RETRACTED — not a divergence; already fixed upstream in `devel`): unrestricted `DBserver`-parameter forwarding with stripped authorization header

**This finding is retracted.** It was originally based on a stale local
`devel` checkout. After updating `devel` to the current `origin/devel` tip,
`RestReplicationHandler::isDBserverForwardingAllowed()` was found to
already exist there, **byte-for-byte identical** to the current branch's
version — confirmed via full-file diff of `RestReplicationHandler.cpp`
between the stale and up-to-date `devel` snapshots, which shows this exact
function (plus the `forwardingTarget()` call site and log message) being
added as a `devel`-only commit, unrelated to the current branch. In other
words: `origin/devel` fixed this gap on its own, independently of (and, by
the look of the surrounding code/comments, probably around the same time
as or before) the current branch's refactor. There is therefore **no
behavioural difference between the current branch and `devel`** for
`DBserver`-parameter forwarding — both restrict it identically to
`dump`(GET)/`batch`, and both reject any other command carrying a
`DBserver` parameter with `403 TRI_ERROR_FORBIDDEN`.

The original write-up is preserved below, struck through in spirit, purely
as a historical record of what the *stale* comparison incorrectly
attributed to the current branch:

<details>
<summary>Original (incorrect) write-up, kept for the record</summary>

`RestReplicationHandler::forwardingTarget()` implements a mechanism, used
by `arangodump`/`arangorestore` on a coordinator, where a client can pass a
`?DBserver=<id>` query parameter to have the *whole request* transparently
forwarded to a specific DBServer — and, critically, **the caller's own
`Authorization` header is stripped** before forwarding, so the forwarded
request executes on the target DBServer under cluster-internal,
effectively superuser, trust. It was (incorrectly) believed that a *stale*
`devel` checkout applied this forwarding unconditionally to any command,
and that only the current branch added the `dump`/`batch` restriction.
Re-verification against an up-to-date `devel` showed this restriction was
already present there too — this was never actually a difference between
the two branches being compared; it only appeared to be one because of a
stale reference checkout.

</details>

Note: Finding 2 (the actually-still-valid gap: `restore-indexes`/
`restore-view` lacking any permission check in `devel`) is **not**
amplified by this retraction in any new way — it remains exactly as
severe as described there on its own; it just isn't compounded by an
*additional*, unrestricted forwarding gap, since that part turned out to
already be fixed in `devel`.

### Finding 4 (Correctness fix, stricter than `devel`): `restore-collection`'s "overwrite" path now also honours per-collection access overrides

`devel`'s `testPermissions()` handled the three `RestoreCollection`
sub-cases with two separate, coarse checks
(`/tmp/devel_RestReplicationHandler.cpp:862-882` old numbering):
```cpp
if (overwriteCollection == "true" || not existing) {
  // (1) recreate OR (2) new collection
  if (!exec.isAdminUser() && !exec.canUseDatabase(dbName, auth::Level::RW))
    return FORBIDDEN;
} else {
  // (3) existing collection, no overwrite
  if (!exec.isAdminUser() && !exec.canUseCollection(collectionName, auth::Level::RW))
    return FORBIDDEN;
}
```
i.e. for sub-case (1) — actually overwriting an **existing** collection —
`devel` checked **only database-level `RW`**, completely ignoring any
collection-specific access override (`grantCollection(user, db, coll,
"none")`, see the per-`(db,collection)` grant table documented in the
`RestCollectionHandler` session, `auth_comparison_with_devel.md:457-465`).
A user with database-wide `RW` but an explicit collection-level
restriction on one particular collection could still drop-and-recreate
that very collection via `restore-collection?overwrite=true`.

The current branch's `canRestoreCollection(db, name, overwrite)`
(`arangod/Utils/ExecContext.cpp:239-247`) maps, in Classic mode
(`arangod/Auth/AuthMode.cpp:263-286`), the `overwrite == true` case to:
```cpp
if (isAdmin().ok()) return {};
if (collection.overwrite) {
  if (auto r = check(p::DropCollection(db, name)); r.fail()) return r;
  if (auto r = check(p::CreateCollection(db, name)); r.fail()) return r;
  return {};
}
```
`DropCollection` (`arangod/Auth/AuthMode.cpp:391-...`) requires **both**
database-level `RW` **and** collection-level `WriteMeta` on the specific
collection. For a genuinely non-existing collection this makes no
difference (there is no per-collection override to look up, so the
effective level trivially falls back to the database level, matching
`devel`). But for an **existing** collection being overwritten, the new
code correctly enforces any collection-specific restriction that
`devel` silently bypassed. This is a narrow, low-probability-of-being-hit
but real behavioural change — stricter/more correct than `devel`, not a
regression, and worth being aware of if any test or workflow relies on
overwrite-via-restore bypassing a per-collection deny override.

### Finding 5 (Verified equivalent): admin-bypass refactors in `handleCommandClusterInventory` and RocksDB `handleCommandDump`

Two more spots replace `devel`'s "conditionally escalate to superuser via
`ExecContextSuperuserScope`, then run an `!isAdminUser() && !canUse...`
check" pattern with an explicit up-front `Result`-returning check, with no
escalation for the check itself:

- `handleCommandClusterInventory()`
  (`arangod/RestHandler/RestReplicationHandler.cpp:1058-1065`):
  ```cpp
  if (exec.canUseAdminAction(auth::perms::AdminClusterInfo{}).fail() &&
      exec.canUseCollection(dbName, c->name(), AccessLevel::Read).fail()) {
    continue;  // skip collection
  }
  ```
  is exactly `devel`'s `if (!exec.isAdminUser() &&
  !exec.canUseCollection(...))` by De Morgan's law, since
  `canUseAdminAction(AdminClusterInfo{})` reduces to `isAdmin()` in Classic
  mode (`arangod/Auth/AuthMode.cpp:367`, the generic `AnyAdmin` case) —
  identical to `devel`'s `isAdminUser()`. The rest of the function (view
  enumeration in particular) never consulted `ExecContext` in either
  branch, so removing the surrounding `ExecContextSuperuserScope` here has
  no further effect. No finding.
- RocksDB `handleCommandDump()`
  (`arangod/RocksDBEngine/RocksDBRestReplicationHandler.cpp:736-744`):
  ```cpp
  if (auto r = ExecContext::current().canDumpCollection(_vocbase.name(), cname);
      r.fail()) {
    generateError(r);
    return;
  }
  ExecContextSuperuserScope superUser;  // needed to actually read the data
  ```
  `canDumpCollection()` (`arangod/Utils/ExecContext.cpp:233-237` →
  `arangod/Auth/AuthMode.cpp:253-261`) is `isAdmin() OR
  UseCollection(Read)`, matching `devel`'s
  `ExecContextSuperuserScope escope(isAdminUser()); if
  (!canUseCollection(..., RO)) FORBIDDEN;` exactly (for a `devel` admin,
  the pre-check escalation makes `canUseCollection` trivially pass; for a
  non-admin, no escalation happens and the real check applies) — just
  reordered (check first on the real identity, then escalate
  unconditionally afterwards, needed for the actual read to succeed on a
  single server). Verified equivalent, not a finding.

### Finding 6 (Narrow, low-impact regression): RocksDB `handleCommandInventory`'s single-collection branch lost its admin bypass

`devel`'s `RocksDBRestReplicationHandler::handleCommandInventory()`
wrapped the entire non-global branch — both the "all collections" and the
"single collection" (`ctx->getInventory(_vocbase, collection, builder)`)
sub-cases — in `ExecContextSuperuserScope escope(exec.isAdminUser())`
(`/tmp/devel_RocksDBRestReplicationHandler.cpp:427`). The current branch
dropped this escalation entirely
(`arangod/RocksDBEngine/RocksDBRestReplicationHandler.cpp:416-427`).

This makes no difference for the "all collections" sub-case (`collection`
empty): `RocksDBReplicationContext::getInventory(vocbase, includeSystem,
includeFoxxQueues, global, result)`
(`arangod/RocksDBEngine/RocksDBReplicationContext.cpp:363-461`) never
consults `ExecContext` at all, in either branch — it always returns the
full inventory of all collections regardless of caller identity (this is
itself a latent, pre-existing characteristic shared identically by both
branches, not a new difference, so not flagged as its own finding here).

It does matter for the "single collection" sub-case (`collection`
non-empty, used for DBServer shard synchronization): that overload,
`RocksDBReplicationContext::getInventory(vocbase, collectionName, result)`
(`arangod/RocksDBEngine/RocksDBReplicationContext.cpp:465-499`), explicitly
checks `ExecContext::current().canUseCollection(vocbase.name(),
collectionName, AccessLevel::Read)` and silently omits the collection's
metadata (still returning an empty `"collections"` array, not an error) if
the check fails. In `devel`, a Classic-mode admin without direct
database-level access to the target database would still see full
metadata (due to the escalation); in the current branch, they now see an
empty result. In practice this branch is documented as being used "in the
DB server case" for shard synchronization
(`arangod/RocksDBEngine/RocksDBRestReplicationHandler.cpp:400-401`), which
normally runs under an already-superuser, cluster-internal execution
context — making this a low-impact, edge-case-only regression (it would
only matter for a direct, external call to
`GET /_api/replication/inventory?collection=X&batchId=...` by a Classic
admin lacking specific access to that database).

Similarly, `devel`'s `ExecContextSuperuserScope
escope(ExecContext::current().isAdminUser());` in
`handleCommandLoggerFollow()`
(`/tmp/devel_RocksDBRestReplicationHandler.cpp:265`) was also removed with
no replacement. This one is confirmed to be a pure no-op: `tailWal()`
(`arangod/RocksDBEngine/RocksDBReplicationTailing.cpp`) never references
`ExecContext` in either branch, so the escalation never affected its
behaviour to begin with (this endpoint has no per-collection filtering of
WAL entries at all, in either branch — again a latent, shared
characteristic, not a new divergence).

### Summary

| Handler / Route | Verdict |
|---|---|
| `testPermissions()` dead-condition/early-return cleanup | Confirmed no-op (Finding 1) |
| `PUT /_api/replication/restore-indexes` (single-server & coordinator) | **`devel` had no permission check at all** (real gap); current branch adds `canRestoreCreateIndex()` (Finding 2, security fix) |
| `PUT /_api/replication/restore-view` | **`devel` had no permission check at all** (real gap); current branch adds `canRestoreDropView()`/`canRestoreCreateView()` (Finding 2, security fix) |
| `restore-data` into `_users`/`_analyzers` | Not actually a gap in `devel`; both branches equivalent via system-collection auth-level fallback |
| Any command + `?DBserver=<id>` (auth-header-stripped forwarding) | ~~Real gap, current branch fixes it~~ **RETRACTED**: up-to-date `devel` already restricts this to `dump`/`batch` identically via its own `isDBserverForwardingAllowed()`; no divergence (former Finding 3, now retracted — was based on a stale `devel` checkout) |
| `PUT /_api/replication/restore-collection?overwrite=true` on an **existing** collection | Stricter than `devel`: now also enforces the target collection's own access override, not just database-level `RW` (Finding 4) |
| `handleCommandClusterInventory()`, RocksDB `handleCommandDump()` | Verified equivalent to `devel` (Finding 5) |
| `GET /_api/replication/inventory?collection=X` (single collection, non-global) | Narrow regression: Classic admin without direct DB access now sees empty result instead of full metadata; low real-world impact (Finding 6) |
| `GET /_api/replication/logger-follow` | Confirmed no-op removal; endpoint has no per-collection auth filtering in either branch |
| `ClusterRestReplicationHandler` | Purely cosmetic diff; no authorization code |

**Action items / recommendations:**
1. Finding 2 (the `restore-indexes`/`restore-view` gap) looks like it may
   already have been the motivation for this refactor; no further action
   needed beyond confirming it is an intentional, deliberate fix (it
   appears to be, given the explanatory comments already present in the
   code). Note: the `DBserver`-forwarding restriction (formerly Finding 3)
   is **not** part of this branch's contribution — it was independently
   fixed upstream in `devel` and is identical in both branches; see the
   retraction note above.
2. Finding 4 (stricter overwrite-restore semantics) and Finding 6 (narrow
   single-collection-inventory regression for non-superuser admins) are
   low-risk but worth a short mention in release notes / migration notes,
   since they could theoretically break an existing workflow that
   (knowingly or not) relied on the looser `devel` behaviour.
3. No code changes are proposed as part of this analysis session, per the
   established methodology for this document.
4. **Process note:** this session's initial pass used a local `devel`
   checkout that was behind `origin/devel`; after updating it and
   re-diffing all files, only Finding 3 needed retraction (see the
   correction note at the top of this section) — Findings 1, 2, 4, 5 and 6
   were all independently re-confirmed against the up-to-date `devel` and
   remain valid as stated. As a spot-check, the other recently-analyzed
   handlers in this document (`RestAnalyzerHandler`, `RestImportHandler`,
   `RestUsageMetricsHandler`/`RestEngineHandler`/`RestSupportInfoHandler`,
   `RestAqlFunctionsHandler`/`RestEndpointHandler`/`RestAccessTokenHandler`)
   were also re-diffed against the up-to-date `devel`; their underlying
   source files are either byte-for-byte unchanged or differ only in
   already-accounted-for cosmetic ways, so no corrections were needed for
   those sections.

## `MaintenanceRestHandler` (`arangod/Cluster/MaintenanceRestHandler.cpp`)

Mounted at the exact path `/_admin/actions` (registered unconditionally for
**every** server role — Coordinator, DBServer, single server, and Agent —
in `arangod/GeneralServer/GeneralServerFeature.cpp:762-764`; there is no
`ServerState::instance()->isDBServer()` guard around the registration
itself, confirmed identical in both branches via diff). It exposes four
operations, dispatched purely on HTTP verb in
`MaintenanceRestHandler::execute()`
(`arangod/Cluster/MaintenanceRestHandler.cpp:48-79`):
- `GET` → `getAction()`: dumps the full maintenance action registry plus
  pause/running status.
- `POST` → `postAction()`: pauses (`{"execute": "pause", "duration": N}`)
  or resumes (`{"execute": "proceed"}`) the maintenance feature.
- `PUT` → `putAction()`: adds an arbitrary maintenance action to the
  worklist (or executes it directly, depending on the parsed
  `ActionDescription`).
- `DELETE /_admin/actions/{id}` → `deleteAction()`: cancels/removes a
  pending action.

**Diff vs. `devel`:** a single added comment
(`// Mounted at /_admin/actions (exact)`) — otherwise byte-for-byte
identical in both the `.cpp` and `.h` files. The handler registration code
in `GeneralServerFeature.cpp` is likewise unchanged.

**No authorization code exists anywhere in this handler, in either
branch** — confirmed by inspection: there is no `ExecContext`, `canUse*`,
`auth::`, or `isAdminUser`/`isSuperuser` reference anywhere in
`MaintenanceRestHandler.cpp`. Any authenticated user (in Classic mode: any
user holding a valid, non-anonymous session — no specific database or
collection grant is even consulted) can list, pause/resume, add, or delete
cluster maintenance actions on whichever server the request happens to
land on.

Regarding the user's premise that this handler is "only active on
DBServers": this is **half right**. The **registration is not
role-gated** — the handler is reachable via HTTP on every role, and
`MaintenanceFeature` itself is instantiated as an `ApplicationFeature` on
all roles too. However, `MaintenanceFeature::start()`
(`arangod/Cluster/MaintenanceFeature.cpp:281-289`) explicitly disables its
background worker threads for single-server and Agent roles, and
`arangod/Cluster/MaintenanceFeature.cpp:296-300` similarly skips thread
startup on a Coordinator ("no need for maintenance on a coordinator") —
so on those roles `getAction()` simply returns an always-empty registry,
and `putAction()`/`postAction()` are effectively inert (no worker consumes
the queued action; pausing/resuming a feature with no active workers has
no observable effect). This matches
`Documentation/path_permissions.md:748-751`, which records the intended
access level as merely `AUTHEN` for all four routes, annotated "Only
really relevant on DBServers" — i.e., the lack of both a role guard and an
authorization check is **documented, pre-existing, unchanged-since-`devel`
behavior**, not something introduced or altered by this branch.

**Conclusion:** the user's assessment is correct in substance — this
handler introduces **no new divergence** from `devel` (the code is
identical modulo one comment), and its practical impact is confined to
DBServers where the maintenance worker actually runs. It joins
`RestCompactHandler`, `RestAqlFunctionsHandler`, `RestEndpointHandler`, and
`RestImportHandler` as handlers with a clean bill of health in this
comparison. No findings, no action items.

## `RestGraphHandler` (`arangod/RestHandler/RestGraphHandler.cpp`)

Mounted at `/_api/gharial` (prefix); implements the "General Graph" CRUD
API (graph create/read/drop, edge-definition and orphan-collection
management, vertex/edge CRUD scoped to a graph). `RestGraphHandler.h` is
byte-for-byte identical to `devel`; `RestGraphHandler.cpp` itself contains
**no authorization code whatsoever** (confirmed by grep for
`ExecContext|canUse|canSee|canCreate|canDrop|auth::` — zero matches) and
its diff against `devel` is trivial: one added comment
(`// Mounted at /_api/gharial (prefix)`) and four call sites where
`OperationOptions(_context)` became `OperationOptions()` — the same
proven-no-op pattern already established for `RestDocumentHandler` and
`RestImportHandler` (the only real consumer of that constructor argument,
`Collections::create`, is never reached from a plain `OperationOptions`
passed to `trx.update()`/`trx.document()` calls here).

All authorization logic lives one layer down, in `GraphOperations.cpp` and
`GraphManager.cpp` (the classes `RestGraphHandler` delegates every
operation to), so this session focuses there. This turned out to be a
substantial, genuine refactor — not merely cosmetic — with one confirmed
narrow regression (Finding 3) that matches the user's expectation that
"we might have introduced some changes intentionally."

### Finding 1 (Confirmed no-op): removal of the `!ExecContext::isAuthEnabled()` early-return shortcut

`devel`'s `GraphOperations::hasPermissionsFor()`
(`devel:arangod/Graph/GraphOperations.cpp:1180-1185`),
`GraphOperations::checkEdgeDefinitionPermissions` (analogous check at
`devel:arangod/Graph/GraphOperations.cpp:1205-1210`), and
`GraphManager::checkCreationRequirements()` /
`GraphManager::checkDropRequirements()`
(`devel:arangod/Graph/GraphManager.cpp:795-800,1069-1074`) all began with:
```cpp
if (!ExecContext::isAuthEnabled()) {
  LOG_TOPIC(...) << "Permissions are turned off.";
  return true; // or TRI_ERROR_NO_ERROR
}
```
`ExecContext::isAuthEnabled()` (`devel:arangod/Utils/ExecContext.cpp:79-83`)
simply returns `AuthenticationFeature::instance()->isActive()` — i.e. this
shortcut only fires when authentication is switched off server-wide
(`--server.authentication false`), a case that is out of scope for this
Classic-mode-with-auth-enabled comparison. The whole `ExecContext::isAuthEnabled()`
static method was removed in the current branch (confirmed absent from
`arangod/Utils/ExecContext.h`), and these four call sites were simply
deleted rather than replaced.

This is a confirmed no-op, not a behavior change, even in the
auth-disabled case: when authentication is inactive,
`ExecContext::create()` (`arangod/Utils/ExecContext.cpp:72-110`) builds an
`AuthMode::Disabled` context instead of `AuthMode::Classic`, and
`AuthMode::Disabled::check()` (`arangod/Auth/AuthMode.cpp:659-661`)
unconditionally returns `Result{}` (success) **for every permission**,
with no exceptions. So whether the `isAuthEnabled()` bypass is checked
explicitly beforehand or the request simply falls through to
`canUseCollection()`/`canUseDatabase()`/`canUseGraph()` and hits
`AuthMode::Disabled::check()` at the bottom, the outcome is identical:
unconditional ALLOW. Purely a code-cleanup consequence of moving the
"is auth disabled" special-casing from scattered call sites into the
`AuthMode` variant hierarchy itself — a pattern already established for
several previous handlers in this document (e.g. `RestCompactHandler`'s
`isSuperuserOrDisabled()` findings).

### Finding 2 (Confirmed equivalent, "fail-fast" refactor): new explicit `checkCanModifyGraphStructure()` guard in `GraphOperations.cpp`

The current branch adds a new private helper
(`arangod/Graph/GraphOperations.cpp:70-74`):
```cpp
Result GraphOperations::checkCanModifyGraphStructure() const {
  auto& exec = ExecContext::current();
  return exec.canUseGraph(_vocbase.name(), _graph.name(),
                          GraphAccessLevel::Modify);
}
```
and calls it as the very first statement in five structure-mutating
functions that had **no permission check of their own** in `devel`:
`eraseEdgeDefinition` (`arangod/Graph/GraphOperations.cpp:110`),
`editEdgeDefinition` (`:250`), `addOrphanCollection` (`:342`),
`eraseOrphanCollection` (`:499`), and `addEdgeDefinition` (`:571`). In
`devel`, only two of these five had *any* explicit check at all —
`eraseEdgeDefinition` and `eraseOrphanCollection`, and only conditionally,
gating the collection-drop sub-case via `hasRWPermissionsFor()`
(`devel:arangod/Graph/GraphOperations.cpp:109,468`, preserved unchanged in
the current branch at `arangod/Graph/GraphOperations.cpp:121,494`).

`canUseGraph(db, graph, GraphAccessLevel::Modify)`
(`arangod/Utils/ExecContext.cpp:430-437`) resolves in `Classic` mode
(`arangod/Auth/AuthMode.cpp:544-558`) to exactly
`check(UseDatabase{db, DatabaseAccessLevel::Write})` — i.e. plain
database-level RW, nothing more, nothing less.

The reason this is a confirmed no-op rather than a new restriction: all
five functions unconditionally end by writing the updated graph
definition to the `_graphs` collection (either directly via
`SingleCollectionTransaction`/`trx.update()`, e.g.
`arangod/Graph/GraphOperations.cpp:100,144-150`, or via
`GraphManager::storeGraph()`, `arangod/Graph/GraphOperations.cpp:610`).
Since `_graphs` is a system collection, `User::collectionAuthLevel()`
unconditionally resolves its auth level to the database's own level
(established already in the `RestAnalyzerHandler` session for `_analyzers`
— the same short-circuit applies to every collection name starting with
`_`), so that final write **already required database-level RW** in
`devel`, implicitly, via the ordinary transaction/collection-permission
machinery — the exact same requirement the new explicit check now tests
upfront. The only observable difference is *when* the rejection happens
(before vs. after `checkEdgeCollectionAvailability()`/collection creation
side effects), which only affects error-message/error-code ordering for
an already-unauthorized caller — not the final ALLOW/DENY outcome. This
matches the "fail fast for what was already implicitly enforced" pattern
seen previously for `RestCollectionHandler`/`RestReplicationHandler`.

### Finding 3 (Regression, more permissive): `GraphManager::createGraph()`'s permission check drops the per-collection read check when the caller already has database-level RW

This is the headline finding — a genuine behavioral divergence, not a
no-op, and the kind of change the user anticipated.

`devel`'s inline permission logic inside `GraphManager::createGraph()`
(`devel:arangod/Graph/GraphManager.cpp:819-878`, prior to being refactored
out into `ExecContext::canCreateGraph()`) worked as follows:
1. If the caller **lacks** database-level RW: check every edge/vertex
   collection referenced by the graph — each must both already exist
   *and* be individually readable
   (`execContext.canUseCollection(col, auth::Level::RO)`); if any
   collection is missing or unreadable, fail with `TRI_ERROR_FORBIDDEN`.
   Otherwise (everything exists and is readable, but the `_graphs` write
   itself is still impossible), fail with `TRI_ERROR_ARANGO_READ_ONLY`.
   **Either way, lacking database RW always results in failure.**
2. If the caller **has** database-level RW: *unconditionally* loop over
   every edge/vertex collection again and require
   `execContext.canUseCollection(col, auth::Level::RO)` on each one —
   this second loop ran regardless of whether the collections already
   existed, and regardless of the database-RW check having already
   passed.

Step 2 matters because, in the Classic permission model, a per-collection
grant can be set *more restrictively* than the database default (e.g. a
database-level `rw` default with an explicit `none`/`ro` override on one
specific collection) — `User::collectionAuthLevel()` honors such
overrides for ordinary (non-system) collections. So `devel` genuinely
enforced: "you need database RW **and** individual read access to every
collection the graph will reference, even ones you're not creating."

The current branch's refactor moved this logic into
`ExecContext::canCreateGraph()`
(`arangod/Utils/ExecContext.cpp:406-419`) and the corresponding `Classic`
case (`arangod/Auth/AuthMode.cpp:504-528`):
```cpp
[&](p::CreateGraph const& graph) -> Result {
  if (auto r =
          check(p::UseDatabase{graph.db, DatabaseAccessLevel::Write});
      r.ok()) {
    return r;                                   // <-- early success!
  }
  for (auto const& coll : graph.collectionNamesToCreate) {
    if (auto r = check(p::CreateCollection{graph.db, coll}); r.fail()) {
      return r;
    }
  }
  for (auto const& coll : graph.collectionNamesToRead) {
    if (auto r = check(p::UseCollection{graph.db, coll,
                                        CollectionAccessLevel::Read});
        r.fail()) {
      return r;
    }
  }
  return {TRI_ERROR_ARANGO_READ_ONLY, "Cannot write to database."};
},
```
called from `GraphManager::createGraph()`
(`arangod/Graph/GraphManager.cpp:844-852`), which now partitions the
graph's collections into `collectionsToCreate` (don't yet exist) and
`collectionsToRead` (already exist) before calling `canCreateGraph()`.

The "no database RW" branch is a faithful, verified-equivalent
reimplementation of `devel`'s step 1 (`CreateCollection`
(`arangod/Auth/AuthMode.cpp:385-390`) itself requires database RW via the
"container principle", so it always fails here too, matching `devel`'s
unconditional rejection whenever a not-yet-existing collection is
involved; the `collectionNamesToRead` loop reproduces `devel`'s per-
collection RO check for existing collections one-for-one).

**But the "has database RW" branch returns success immediately —
`devel`'s step 2 (the unconditional per-collection RO re-check) has no
counterpart at all.** Concretely: a user who holds database-level RW but
has an explicit collection-level override denying (or merely not
granting) access to some specific existing collection could **not**
create a graph referencing that collection in `devel` (rejected with
`FORBIDDEN` by the second loop), but **can** in the current branch (the
database-RW check alone short-circuits to success, and
`collectionNamesToRead` is never even inspected in this branch). This is
a narrow but real widening of what's permitted — the opposite direction
from most findings in this document, and squarely a case of "an
intentional-looking refactor that quietly dropped a check", worth a
deliberate decision rather than being dismissed as equivalent.

`GraphManager::checkDropRequirements()` → `ExecContext::canDropGraph()`
(`arangod/Utils/ExecContext.cpp:420-428`) →
`AuthMode::Classic`'s `DropGraph` case
(`arangod/Auth/AuthMode.cpp:529-543`) was checked for the same pattern and
is **not** affected: it unconditionally requires database RW
(`arangod/Auth/AuthMode.cpp:532-536`) *and* `DropCollection` permission
(which itself layers database RW plus collection-level `WriteMeta`,
`arangod/Auth/AuthMode.cpp:391-404`) on every collection actually being
dropped — reproducing `devel`'s
`devel:arangod/Graph/GraphManager.cpp:1091-1141` logic faithfully in both
the "have DB RW" and "lack DB RW" cases (`devel`'s final pair of
`canUseCollection(StaticStrings::GraphsCollection, RO/RW)` checks are
exactly `canUseDatabase(RO)`/`canUseDatabase(RW)` in disguise, per the
system-collection short-circuit rule).

### Finding 4 (Confirmed no-op): new per-item `canSeeGraph()` visibility filter in `GraphManager::readGraphs()` / `lookupGraphByName()`

`GraphManager::readGraphs()` (list graphs, backing `GET /_api/gharial`)
gained a per-entry filter
(`arangod/Graph/GraphManager.cpp:783-802`):
```cpp
for (auto const& g : VPackArrayIterator(graphsSlice)) {
  ...
  if (exec.canSeeGraph(ctx()->vocbase().name(), <name>).ok()) {
    builder.add(g);
  }
}
```
that `devel` lacked entirely (`devel:arangod/Graph/GraphManager.cpp:749`
just appended the whole `graphsSlice` unfiltered). Likewise,
`GraphManager::lookupGraphByName()` (backing `GET /_api/gharial/<name>`,
called from `arangod/RestHandler/RestGraphHandler.cpp:602`) gained an
up-front `canSeeGraph()` guard (`arangod/Graph/GraphManager.cpp:304-309`)
that `devel` also lacked.

`canSeeGraph()` (`arangod/Utils/ExecContext.cpp:400-404`) resolves in
`Classic` mode to `check(UseDatabase{db, DatabaseAccessLevel::Read})`
(`arangod/Auth/AuthMode.cpp:499-503`) — plain database-level RO, with no
per-graph or per-collection override capability (unlike collections,
graphs have no independent ACL entity in the Classic permission model).
Crucially, `RestHandler::checkUserCanAccess()`
(`arangod/GeneralServer/RestHandler.cpp:705-724`) already unconditionally
requires `DatabaseAccessLevel::Read` on the target database for **every**
request before any handler-specific code runs at all — so any caller who
can reach `RestGraphHandler` in the first place, by definition, already
satisfies `canSeeGraph()` for every graph in that database. The new
filter is an all-or-nothing gate that can never actually remove an
individual entry in Classic mode; it only matters for RBAC mode (out of
scope here). This is the same "list-filtering that's a confirmed no-op
under Classic mode's database-level-only visibility model" pattern
already documented for `RestAnalyzerHandler` (Finding 3) and
`RestCollectionHandler`.

### Summary

| Operation | Verdict |
|---|---|
| `GET /_api/gharial` (list graphs) | Confirmed equivalent — new `canSeeGraph()` filter is a no-op under Classic mode (Finding 4). |
| `GET /_api/gharial/<name>` (read graph) | Confirmed equivalent — new `canSeeGraph()` guard is a no-op under Classic mode (Finding 4). |
| `POST /_api/gharial` (create graph) | **Regression (more permissive)** — a database-RW user with a denying per-collection override on an already-existing referenced collection is now allowed to create the graph, where `devel` would reject it (Finding 3). |
| `DELETE /_api/gharial/<name>` (drop graph) | Confirmed equivalent — `canDropGraph()` reproduces `devel`'s combined database-RW + per-collection-drop logic faithfully. |
| Edge-definition add/edit/remove, orphan-collection add/remove | Confirmed equivalent — new upfront `checkCanModifyGraphStructure()` check (database RW) merely fails fast on a requirement `devel` already enforced implicitly via the unconditional `_graphs` write (Finding 2). |
| Vertex/edge CRUD within a graph (`getVertex`/`getEdge`/`create*`/`update*`/`replace*`/`remove*`) | Unaffected — these functions are untouched by the diff apart from the no-op `OperationOptions(_context)` → `OperationOptions()` change; they rely on `checkVertexCollectionAvailability()`/`checkEdgeCollectionAvailability()` plus the standard transaction-level collection-permission machinery already covered by the `RestDocumentHandler` session. |

**Action items / recommendations:** Finding 3 is the one item worth a
deliberate decision. Either (a) treat it as intentional — the
`collectionNamesToRead` check was arguably redundant defense-in-depth
that most deployments don't rely on (per-collection overrides *below* the
database default are an unusual configuration) — and document it as a
minor, accepted behavior change, or (b) restore parity with `devel` by
adding an unconditional loop over `collectionNamesToRead` requiring
`UseCollection{Read}` in the `CreateGraph` Classic-mode case
(`arangod/Auth/AuthMode.cpp:504-528`), independent of whether the
database-RW check already succeeded. Given this is a narrow permissiveness
widening (not a lockout) and requires a specific, uncommon permission
configuration to matter, it is low urgency but should not be dismissed as
a pure refactor artifact.

## `RestSimpleQueryHandler` (`arangod/RestHandler/RestSimpleQueryHandler.cpp`)

Mounted at `/_api/simple/all`, `/_api/simple/all-keys`, and
`/_api/simple/by-example` (prefix); a thin, legacy convenience layer over
the cursor API, subclassing `RestCursorHandler`. All three operations
(`allDocuments()`, `allDocumentKeys()`, `byExample()`) do the same thing:
parse the request body, build a corresponding AQL query string
(`FOR doc IN @@collection ...`) plus bind variables, and hand it off to
`registerQueryOrCursor()` — the exact same entry point used by
`POST /_api/cursor`, already fully analyzed in the `RestCursorHandler`
session (`auth_comparison_with_devel.md:2217-2408`).

### Diff overview and authorization surface

`RestSimpleQueryHandler.h` is byte-for-byte identical to `devel`.
`RestSimpleQueryHandler.cpp` differs from `devel` by exactly one added
comment (`// Mounted at /_api/simple/all, ...`,
`arangod/RestHandler/RestSimpleQueryHandler.cpp:44-45`) — confirmed via
`diff -u` showing a single three-line hunk. A grep for
`ExecContext|canUse|canSee|auth::` across the file returns zero matches:
this handler, like `RestDocumentHandler` and `RestCursorHandler` before
it, contains **no authorization logic of its own**. The only
collection-name handling here (`_vocbase.lookupCollection(collectionName)`
in each of the three functions, e.g.
`arangod/RestHandler/RestSimpleQueryHandler.cpp:93,184,300`) is a plain,
unchanged, non-authorizing name-normalization lookup (resolving a numeric
collection ID to its current name) — it existed identically in `devel`
and performs no permission check.

Since every request here is routed through `registerQueryOrCursor()` as a
freshly-built read-only `FOR doc IN @@collection ...` AQL query (none of
the three operations ever mutates data or accepts a raw AQL string from
the caller — the query text is always one of the three fixed templates
built in this file), the collection-level permission check that applies
is unconditionally the `AccessType::READ` path already covered in the
`RestCursorHandler` session: **not** subject to the server-wide
read-only-mode regression tracked there (that regression is scoped to
`WriteData`-or-above accesses only). The cursor-ownership regression
documented as `RestCursorHandler` Finding 1 (`auth_comparison_with_devel.md:2283-2387`)
does not apply either — these three endpoints only *create* new cursors
via `registerQueryOrCursor()`; they never look up or delete an existing
cursor by ID (that's exclusively `PUT`/`DELETE /_api/cursor/<id>`, not
reachable through `/_api/simple/*`).

### Summary

No findings distinct from the already-documented `RestCursorHandler`
session apply beyond what's noted above (and even those are narrowed to
"not applicable" for this handler, since it only ever issues read-only
queries and never touches cursor ownership). `RestSimpleQueryHandler`
joins `RestCompactHandler`, `RestAqlFunctionsHandler`,
`RestEndpointHandler`, `RestImportHandler`, and `MaintenanceRestHandler`
as a handler with a clean bill of health — no code changes beyond a
comment, no authorization logic of its own, and its sole dependency
(`registerQueryOrCursor()`'s read-only-query path) was already verified
unaffected.

**Action items / recommendations:** None.

## `RestAuthReloadHandler`, `RestDebugHandler`, `RestStatusHandler` and `RestAdminLogHandler`

Four small, single-purpose admin handlers, mounted at `/_admin/auth/reload`
(exact), `/_admin/debug` (prefix, only compiled in when
`ARANGODB_ENABLE_FAILURE_TESTS` is defined), `/_admin/status` (exact), and
`/_admin/log` (prefix), respectively.

### `RestAuthReloadHandler` — Finding 1 (Cosmetic)

`devel` (`devel:arangod/RestHandler/RestAuthReloadHandler.cpp:41-44`):

```cpp
RestStatus RestAuthReloadHandler::execute() {
  if (!ExecContext::current().isAdminUser()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
    return RestStatus::DONE;
  }
```

Current branch (`arangod/RestHandler/RestAuthReloadHandler.cpp:43-50`):

```cpp
RestStatus RestAuthReloadHandler::execute() {
  if (auto r = ExecContext::current().canUseAdminAction(
          auth::perms::AdminAuthReload{});
      r.fail()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  r.errorMessage());
    return RestStatus::DONE;
  }
```

`canUseAdminAction(AdminAuthReload{})` dispatches through
`AuthMode::Classic::check()`'s generic catch-all for any
`auth::perms::AnyAdmin` alternative that has no dedicated `case`
(`arangod/Auth/AuthMode.cpp:366-367`):

```cpp
// Classic admin action requires RW access to the _system database.
[&](p::AnyAdmin auto const&) -> Result { return isAdmin(); },
```

`AdminAuthReload` has no dedicated branch anywhere in `AuthMode.cpp`
(confirmed by grep), so it falls through to this generic case —
`isAdmin()` is precisely `devel`'s `isAdminUser()` ("RW on `_system`"),
already established as equivalent throughout this document (e.g.
`RestAnalyzerHandler`, `RestWalAccessHandler` sessions). Same pattern,
same verdict: **cosmetic only** — the `bool`→`Result` signature change
only affects error-message wording (`r.errorMessage()` vs. the generic
`TRI_ERROR_HTTP_FORBIDDEN` text); the ALLOW/DENY outcome and HTTP status
(`403`) are unchanged. No behavioral difference in Classic mode.

### `RestDebugHandler` and `RestStatusHandler` — Finding 2 (the headline result: devel had an unauthenticated-access gap during the STARTUP→MAINTENANCE boot window, now closed)

Both handlers gained an identical new `checkUserCanAccess()` override
(`arangod/RestHandler/RestDebugHandler.cpp:39-52`,
`arangod/RestHandler/RestStatusHandler.cpp:90-103`):

```cpp
async<Result> RestDebugHandler::checkUserCanAccess() const {
  // Note that this particular RestHandler might be called during startup (or
  // in maintenance mode). The AuthenticationFeature might not yet be available
  // for authorization, and must not be consulted.
  if (auto const mode = ServerState::instance()->mode();
      mode == ServerState::Mode::STARTUP ||
      mode == ServerState::Mode::MAINTENANCE) {
    co_return request()->authenticated()
        ? Result{}
        : Result{TRI_ERROR_HTTP_UNAUTHORIZED, "Not authenticated."};
  }

  co_return co_await RestBaseHandler::checkUserCanAccess();
}
```

`RestVersionHandler` (`/_api/version`, `/_admin/version`) carries the
*exact same* override, byte-for-byte
(`arangod/RestHandler/RestVersionHandler.cpp:103-116`) — this is a
deliberate, consistently-applied pattern across all three handlers that
`CommTask`'s mode switch admits during boot
(`arangod/GeneralServer/CommTask.cpp:212-274`, unchanged vs. `devel:arangod/GeneralServer/CommTask.cpp:206-269`
byte-for-byte):

```cpp
case ServerState::Mode::STARTUP: {
  if (!allowEarlyConnections || (_auth->isActive() && !req.authenticated())) {
    ... return Flow::Abort;
  }
  // passed authentication!
  if (path == "/_api/version" || path == "/_admin/version" ||
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
      path.starts_with("/_admin/debug/") ||
#endif
      path == "/_admin/status") {
    return Flow::Continue;      // <-- Step 3/4 (context creation + permission
  }                             //     check) never runs for these paths!
  ...
}
case ServerState::Mode::MAINTENANCE: {
  if (allowEarlyConnections &&
      (path == "/_api/version" || path == "/_admin/version" ||
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
       path.starts_with("/_admin/debug/") ||
#endif
       path == "/_admin/status")) {
    return Flow::Continue;      // <-- no authentication check at all here!
  }
  ...
}
```

**Why this override exists at all (a necessary, correct compensating fix
caused by an unrelated refactor, not a Classic/RBAC policy decision):** in
`devel`, the permission check for every request (`CommTask::canAccessPath()`)
lived entirely inline in `CommTask`, *after* the mode-switch above, so it
was structurally skipped for these three paths during `STARTUP`/`MAINTENANCE`
(the switch `return`s before ever reaching it) — `devel`'s
`RestDebugHandler`/`RestStatusHandler`/`RestVersionHandler` had **no**
per-request context at all during this window (`req.requestContext()` was
never populated), and were never expected to consult one. In the current
branch, this check was refactored out of `CommTask` into the virtual
`RestHandler::checkUserCanAccess()`
(`arangod/GeneralServer/RestHandler.cpp:705-763`), invoked unconditionally
by `handleAuthorizationChecks()`
(`arangod/GeneralServer/RestHandler.cpp:766-772`) from
`runHandlerStateMachine()`
(`arangod/GeneralServer/RestHandler.cpp:439`) — a call site that runs for
*every* handler execution, `STARTUP`/`MAINTENANCE` included (both go
through the same `handler->runHandler(...)` as normal requests; see
`arangod/GeneralServer/CommTask.cpp:626-661`,
`handleRequestStartup()`). Without this override, the base
`checkUserCanAccess()` would call
`ec->canUseDatabase(...)` on a null `request()->requestContext()`
(`TRI_ASSERT(ec != nullptr)` at
`arangod/GeneralServer/RestHandler.cpp:716`, or a null dereference in a
release build) for exactly these three routes during this window — a
crash bug that these three matching overrides deliberately avoid by
short-circuiting on `authenticated()` alone, without ever touching `ec`.
This is best classified as a **confirmed necessary fix, not a policy
change**: the underlying internal-facing permission check
(`canUseHardenedAction(AdminMonitoring{})` inside
`RestStatusHandler::execute()`, `arangod/RestHandler/RestStatusHandler.cpp:70-77`)
is a no-op in both branches during this window anyway, since
`ExecContextScope scope(_request->requestContext())`
(`arangod/GeneralServer/RestHandler.cpp:245`, unchanged vs. `devel`) sets
the thread-local `ExecContext::CURRENT` to `nullptr` when the request
context is null, and `ExecContext::current()`
(`arangod/Utils/ExecContext.cpp:46-50`) falls back to the
`Superuser` singleton whenever `CURRENT == nullptr` — so any in-handler
check reached during this window trivially passes in *both* branches.

**The one genuine, confirmed behavioral difference — and it favors the
current branch:** for `ServerState::Mode::MAINTENANCE` specifically,
`devel`'s early-`Continue` has **no authentication requirement at all**
(unlike the `STARTUP` case, which does gate on
`_auth->isActive() && !req.authenticated()`
at `arangod/GeneralServer/CommTask.cpp:213-214`). Combined with the
context-creation skip and the `Superuser` fallback described above, this
means that in `devel`, during the `MAINTENANCE` phase every ArangoDB
instance passes through on every single startup
(entered unconditionally at
`arangod/GeneralServer/GeneralServerFeature.cpp:418`, right after the
handler factory is sealed, before the bootstrap/upgrade phase completes
and the mode flips to `DEFAULT`), **`/_admin/status` and
`/_admin/debug/*` (the latter only in failure-test builds) are reachable
by a completely unauthenticated caller**, with every internal check
resolving to the `Superuser` fallback. The current branch's override
closes this: it requires `request()->authenticated()` to be `true` for
both `STARTUP` and `MAINTENANCE`, uniformly. This is a narrow-window,
low-severity gap in practice (typically leaks only version/health info
via `/_admin/status`, since `canUseHardenedAction` defaults to allow when
`--server.harden` is off anyway; `/_admin/debug/*` is compiled out of
production builds by default), but it is a genuine, verifiable
improvement over `devel`, not a regression.

### `RestAdminLogHandler` — Finding 3 (Verified equivalent, reusing the established `isSuperuser()`/`isSuperuserOrDisabled()` precedent) and Finding 4 (Cosmetic)

`verifyPermitted()` gained a `RequestType const type` parameter
(`arangod/RestHandler/RestAdminLogHandler.h:62`,
`arangod/RestHandler/RestAdminLogHandler.cpp:61`) and now branches three
ways instead of two (`arangod/RestHandler/RestAdminLogHandler.cpp:61-93`):

```cpp
if (loggerFeature.onlySuperUser()) {
  if (!ExecContext::current().isSuperuserOrDisabled()) {
    return arangodb::Result(TRI_ERROR_HTTP_FORBIDDEN,
                            "you need super user rights for log operations");
  }
} else {
  if (type == RequestType::GET) {
    if (auto r = ExecContext::current().canUseAdminAction(
            auth::perms::AdminReadLogs{});
        r.fail()) {
      return r;
    }
  } else {
    // Please note that this means that both `clearLogs` as well as
    // setting logs levels is allowed by AdminSetLogLevel!
    if (auto r = ExecContext::current().canUseAdminAction(
            auth::perms::AdminSetLogLevel{});
        r.fail()) {
      return r;
    }
  }
}
```

- **`--log.api-jwt-policy=jwt` branch** (`loggerFeature.onlySuperUser()`,
  `lib/Logger/LoggerFeature.h:63`): `devel`'s bare
  `!ExecContext::current().isSuperuser()`
  (`devel:arangod/RestHandler/RestAdminLogHandler.cpp:72`) became
  `!ExecContext::current().isSuperuserOrDisabled()`. This is the exact
  same transformation already fully traced and verified equivalent in the
  `RestCompactHandler` section (`auth_comparison_with_devel.md:1310-1370`)
  and reconfirmed for `RestAdminServerHandler`
  (`auth_comparison_with_devel.md:1556-1583`): within `AuthMode::Classic`
  (the scope of this document), `isDisabled()` is always `false`
  (`Classic` is only ever selected while authentication is active,
  `arangod/Utils/ExecContext.cpp:92-110`), so
  `isSuperuserOrDisabled()` reduces to plain `isSuperuser()`, term-for-term
  identical to `devel`'s check. The fully-auth-disabled configuration
  (where the two diverge) is explicitly out of scope per the task
  description and the equivalent note already recorded in the
  `RestCollectionHandler` and `RestAdminServerHandler` sessions.
  **Verified equivalent, no regression.**
- **Non-`onlySuperUser` branch, GET vs. write split**: `devel` used one
  unconditional `!ExecContext::current().isAdminUser()` check for *all*
  request types (`devel:arangod/RestHandler/RestAdminLogHandler.cpp:76`).
  The current branch splits this into `AdminReadLogs` (`GET`) and
  `AdminSetLogLevel` (`PUT`/`DELETE`, i.e. `clearLogs()` and setting log
  levels — as the added comment explicitly flags). Neither permission has
  a dedicated `case` in `AuthMode::Classic::check()` (confirmed by grep),
  so both fall through to the same generic `AnyAdmin` catch-all
  (`arangod/Auth/AuthMode.cpp:366-367`) and resolve to the identical
  `isAdmin()` check `devel` used for both verbs. This split only matters
  under RBAC (where `AdminReadLogs` and `AdminSetLogLevel` can be granted
  independently); in Classic mode the two verbs remain equally gated as
  before. **Cosmetic only, no behavioral difference.**

### Summary

| Route | Verdict |
|---|---|
| `POST /_admin/auth/reload` | Identical to `devel` (Finding 1, cosmetic) |
| `GET/POST/... /_admin/debug/*` (only in failure-test builds) | **Fixes** an unauthenticated-access gap present in `devel` during the `STARTUP`/`MAINTENANCE` boot window (Finding 2) |
| `GET /_admin/status` | Same as above (Finding 2) |
| `GET /_admin/log`, `GET /_admin/log/level` | Identical to `devel` (Finding 3 verified-equivalent / Finding 4 cosmetic) |
| `PUT /_admin/log/level`, `DELETE /_admin/log/entries` | Identical to `devel` (Finding 3 / Finding 4) |

**Action items / recommendations:** None required — Finding 1, 3, and 4
are confirmed no-ops, and Finding 2 is a fix, not a regression, already
correctly and consistently implemented across all three affected handlers
(`RestDebugHandler`, `RestStatusHandler`, `RestVersionHandler`). Worth
noting for release notes as a security hardening item if such notes are
being compiled for this refactor.

## `RestAdminRoutingHandler`, `RestUploadHandler` and `RestJobHandler`

Three more small handlers, mounted at `/_admin/routing` (prefix, requires
V8), `/_api/upload` (prefix), and `/_api/job` + `/_admin/job` (both
prefix), respectively.

- **`RestAdminRoutingHandler`** (`arangod/RestHandler/RestAdminRoutingHandler.cpp`)
  reloads the V8 routing table (`reload` sub-command only). Subclasses
  `RestVocbaseBaseHandler`, has no `checkUserCanAccess()` override and no
  `ExecContext`/`canUse*`/`auth::` references anywhere (confirmed by
  grep). Diff vs. `devel` is a single added comment
  (`arangod/RestHandler/RestAdminRoutingHandler.cpp:37`); `.h` is
  byte-for-byte identical.
- **`RestUploadHandler`** (`arangod/RestHandler/RestUploadHandler.cpp`)
  writes the raw request body to a server-generated temp file (via
  `TRI_GetTempName("uploads", ...)`, not attacker-controlled, so no path-
  traversal concern) and returns its name; used internally by
  arangoimport/UI as a staging step before a subsequent import call.
  Subclasses `RestVocbaseBaseHandler`, likewise no authorization code and
  no override. Diff vs. `devel` is a single added comment
  (`arangod/RestHandler/RestUploadHandler.cpp:46`); `.h` identical.
- **`RestJobHandler`** (`arangod/RestHandler/RestJobHandler.cpp`) manages
  async job results (`getJob`/`putJob`/`deleteJob`, plus `forwardingTarget()`
  for routing to the coordinator that owns the job). Subclasses
  `RestBaseHandler` directly, no authorization code, no override. Diff vs.
  `devel` is a single added comment
  (`arangod/RestHandler/RestJobHandler.cpp:48`); `.h` and
  `forwardingTarget()` byte-for-byte identical.

All three therefore rely entirely on the shared, already-analyzed base
`RestHandler::checkUserCanAccess()` gate (database-level `RO`, per the
`RestDocumentHandler`/many other sessions) — no handler-specific
divergence of any kind was found. Note: `RestJobHandler` returning another
user's async job result to any caller with mere database `RO` access
(job IDs are just sequential numeric ticks, no ownership check tying a
job to the user who created it) is a pre-existing characteristic shared
identically by `devel` and the current branch — not a regression, and out
of scope for a devel-vs-current comparison, but flagged here for
completeness since it was noticed during the review.

### Summary

| Route | Verdict |
|---|---|
| `POST /_admin/routing/reload` | Identical to `devel` — comment-only diff |
| `POST /_api/upload` | Identical to `devel` — comment-only diff |
| `GET/PUT/DELETE /_api/job/*`, `/_admin/job/*` | Identical to `devel` — comment-only diff |

**Action items / recommendations:** None. All three handlers are clean;
no authorization logic exists in any of them in either branch.

## `RestAdminDatabaseHandler`, `RestLogInternalHandler` and `RestAdminStatisticsHandler`

Three more small handlers, mounted at `/_admin/database/target-version`
(exact), `/_api/log-internal` (prefix, replication2 clusters only), and
`/_admin/statistics` + `/_admin/statistics-description` (both exact),
respectively.

- **`RestAdminDatabaseHandler`** (`arangod/RestHandler/RestAdminDatabaseHandler.cpp:40-53`)
  just reports the numeric server version. It has no authorization code
  at all in either branch — confirmed by grep for
  `ExecContext|canUse|auth::` — and no `checkUserCanAccess()` override, so
  it relies solely on the shared base-`RestHandler` requirement of a valid
  authenticated request, matching its documented `AUTHEN`-only access
  level (`Documentation/path_permissions.md:790`). Diff vs. `devel` is a
  single added comment (`arangod/RestHandler/RestAdminDatabaseHandler.cpp:39`);
  `.h` is byte-for-byte identical.
- **`RestAdminStatisticsHandler`** (`arangod/RestHandler/RestAdminStatisticsHandler.cpp:44-77`)
  serves runtime server/HTTP statistics and their description. Diff vs.
  `devel` is a single added comment plus one substantive change
  (Finding 1 below); `.h` is byte-for-byte identical.
- **`RestLogInternalHandler`** (`arangod/RestHandler/RestLogInternalHandler.cpp:43-74`)
  is the internal replication2-log RPC endpoint (`append-entries`,
  `update-snapshot-status`), reachable only between cluster members. Diff
  vs. `devel` is a single added comment plus one substantive change
  (Finding 2 below); `.h` is byte-for-byte identical.

### Finding 1 (Cosmetic): `RestAdminStatisticsHandler` — `ServerSecurityFeature::canAccessHardenedApi()` → `canUseHardenedAction(AdminMonitoring{})`

`devel` (`devel:arangod/RestHandler/RestAdminStatisticsHandler.cpp:50-58`):
```cpp
ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();
if (!security.canAccessHardenedApi()) {
  // dont leak information about server internals here
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN);
  return RestStatus::DONE;
}
```
Current branch (`arangod/RestHandler/RestAdminStatisticsHandler.cpp:53-60`):
```cpp
if (auto r = ExecContext::current().canUseHardenedAction(
        auth::perms::AdminMonitoring{});
    r.fail()) {
  // dont leak information about server internals here
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_FORBIDDEN,
                r.errorMessage());
  return RestStatus::DONE;
}
```
This is the identical `ServerSecurityFeature::canAccessHardenedApi()` →
`ExecContext::canUseHardenedAction()` migration already fully traced and
proven equivalent for `RestMetricsHandler` (line 994-1063) and
`RestUsageMetricsHandler`/`RestEngineHandler` (line 3245-3288) above —
`AdminMonitoring` is the same plain `AnyAdmin`-category permission cited
there (`arangod/Auth/Permissions.h:85,112`, alongside
`AdminMonitoringInternal`), reducing in both branches to exactly
`databaseAuthLevel(user, "_system") == RW`. Only the error message text
changes (`devel`: bare `TRI_ERROR_FORBIDDEN`, no text; current: adds
`r.errorMessage()`); HTTP status (`403`) and `errorNum` are identical.
Purely cosmetic — no new reasoning required, the equivalence proof
carries over verbatim for the third time.

### Finding 2 (Cosmetic): `RestLogInternalHandler` — `isSuperuser()` → `isSuperuserOrDisabled()`

`devel` (`devel:arangod/RestHandler/RestLogInternalHandler.cpp:46-49`):
```cpp
// for now required admin access to the database
if (!ExecContext::current().isSuperuser()) {
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
  co_return;
}
```
Current branch (`arangod/RestHandler/RestLogInternalHandler.cpp:47-50`)
adds `OrDisabled`. This is the same `isSuperuser()` →
`isSuperuserOrDisabled()` widening already established and proven
equivalent-within-Classic-mode in the `RestCompactHandler`,
`RestAdminServerHandler`, and `RestAuthReloadHandler`/`RestAdminLogHandler`
sessions: the two predicates differ only when authentication is globally
disabled (`--server.authentication false`), which is out of scope for
this Classic-mode-focused comparison; whenever authentication is active,
`isSuperuser()` and `isSuperuserOrDisabled()` return identically. No
behavioral change for RBAC/Classic-mode callers.

Note: this handler requires the caller to be a genuine **superuser**
(cluster-internal JWT), not merely a `_system`-admin human user — stricter
than the `isAdmin()`/`AnyAdmin` pattern seen in most other handlers in
this document, and unchanged between branches in that respect.

### Summary

| Route | Verdict |
|---|---|
| `GET /_admin/database/target-version` | Identical to `devel` — no auth check in either branch, comment-only diff |
| `POST /_api/log-internal/<id>/append-entries`, `.../update-snapshot-status` | Identical ALLOW/DENY to `devel` (Finding 2, cosmetic) |
| `GET /_admin/statistics`, `/_admin/statistics-description` | Identical ALLOW/DENY to `devel` (Finding 1, cosmetic message-text only) |

**Action items / recommendations:** None. Both substantive diffs reuse
authorization-refactor patterns already fully proven equivalent in
multiple earlier sessions; no new regressions were found.

## `RestVersionHandler`, `RestAdminDeploymentHandler` and `RestDumpHandler`

Mounted at `/_api/version` + `/_admin/version` (both exact), `/_admin/deployment`
(prefix, coordinator/single-server only), and `/_api/dump` (prefix,
DBServer/single-server only), respectively.

### `RestVersionHandler` — no new findings, both diffs already covered

`RestVersionHandler` has two diffs vs. `devel`, and both were already
fully analyzed in earlier sessions of this document:

- Its `checkUserCanAccess()` override
  (`arangod/RestHandler/RestVersionHandler.cpp:103-116`) is the *exact
  same*, byte-for-byte `STARTUP`/`MAINTENANCE` compensating-fix override
  already fully traced in the `RestAuthReloadHandler`/`RestDebugHandler`/
  `RestStatusHandler`/`RestAdminLogHandler` session (`auth_comparison_with_devel.md:4446-4448,4488`)
  — that session explicitly names `RestVersionHandler` as the third
  handler carrying this override, with the full root-cause analysis
  (necessary fix for a refactor-induced crash risk, plus the genuine
  `devel`-side `MAINTENANCE`-mode unauthenticated-access gap it closes)
  given there. No new reasoning needed.
- Its `execute()` body's `ServerSecurityFeature::canAccessHardenedApi()` →
  `ExecContext::canUseHardenedAction(AdminMonitoringInternal{})` change
  (`arangod/RestHandler/RestVersionHandler.cpp:152-154`) is the identical
  migration already proven equivalent for `RestMetricsHandler`,
  `RestUsageMetricsHandler`/`RestEngineHandler`, and
  `RestAdminStatisticsHandler` (Finding 1, above) — `AdminMonitoringInternal`
  is the same plain `AnyAdmin`-category permission, reducing to
  `databaseAuthLevel(user, "_system") == RW` in both branches. Note this
  one has no observable difference at all (not even a message-text
  change), since `allowInfo` is merely used to decide whether to include
  extra version detail in a still-`200 OK` response, not to reject the
  request.

### `RestAdminDeploymentHandler` — clean, no findings

No authorization code exists in this handler in either branch (confirmed
by grep for `ExecContext|canUse|auth::`); it relies solely on the shared
base-`RestVocbaseBaseHandler` database-level `RO` gate. The diff vs.
`devel` is purely cosmetic: one added comment
(`arangod/RestHandler/RestAdminDeploymentHandler.cpp:43`) and the removal
of four now-unused `#include`s (`Basics/StaticStrings.h`,
`GeneralServer/GeneralServerFeature.h`, `Logger/LogMacros.h`,
`Logger/LoggerStream.h`, `Utils/ExecContext.h`) — dead-code cleanup with
no functional effect; `.h` is byte-for-byte identical.

### Finding 1 (Cosmetic): `RestDumpHandler::handleCommandDumpStart()` — `isAdminUser()` → `canUseAdminAction(AdminDump{}).ok()`

`devel` (`devel:arangod/RestHandler/RestDumpHandler.cpp:166-168`):
```cpp
// adjust permissions in single server case, so that the behavior
// is identical to non-parallel dumps
ExecContextSuperuserScope escope(ExecContext::current().isAdminUser() &&
                                 ServerState::instance()->isSingleServer());
```
Current branch (`arangod/RestHandler/RestDumpHandler.cpp:166-173`) renames
this to `ExecContext::current().canUseAdminAction(auth::perms::AdminDump{}).ok()`.
`AdminDump` has no dedicated case in `AuthMode::Classic::check()` and falls
through to the generic `[&](p::AnyAdmin auto const&) -> Result { return
isAdmin(); }` catch-all (`arangod/Auth/AuthMode.cpp:367`) — the identical
`_system` RW test as `devel`'s `isAdminUser()`. Purely a naming/clarity
improvement (the added `TODO` comment about what permission this *should*
check is a forward-looking design question for RBAC, out of scope for
this Classic-mode comparison); no behavioral difference.

### Finding 2 (Verified equivalent): `RestDumpHandler::validateRequest()`'s per-shard check — same `canDumpCollection()` refactor already proven equivalent

`devel` (`devel:arangod/RestHandler/RestDumpHandler.cpp:303-320`):
```cpp
// make this version of dump compatible with the previous version of
// arangodump. the previous version assumed that as long as you are
// an admin user, you can dump every collection
ExecContextSuperuserScope escope(ExecContext::current().isAdminUser());
// validate permissions for all participating shards
...
if (!ExecContext::current().canUseCollection(
        _request->databaseName(), collectionName, auth::Level::RO)) {
  return {TRI_ERROR_FORBIDDEN, ...};
}
```
Current branch (`arangod/RestHandler/RestDumpHandler.cpp:309-334`) drops
the escalation scope and instead calls
`ExecContext::current().canDumpCollection(_request->databaseName(), collectionName)`
per shard. This is the *exact same* `ExecContextSuperuserScope(isAdminUser());
canUseCollection(RO)` → `canDumpCollection()` refactor already fully
proven equivalent for `RocksDBRestReplicationHandler::handleCommandDump()`
in the `RestReplicationHandler` session (`auth_comparison_with_devel.md:3863-3882`):
`canDumpCollection()` (`arangod/Utils/ExecContext.cpp:233-237` →
`arangod/Auth/AuthMode.cpp:253-261`) is `isAdmin() OR UseCollection(Read)`,
which is exactly what the reordered `devel` logic computes (for an admin,
the pre-check escalation makes the subsequent `canUseCollection` call
trivially pass; for a non-admin, no escalation happens and the real
per-collection `RO` check applies). Verified equivalent — same reasoning
carries over verbatim, no new finding.

The two added `// permission checked in DumpManager` comments in
`execute()` (`arangod/RestHandler/RestDumpHandler.cpp:89,99`, covering the
`DELETE .../<id>` and `POST .../next/<id>` routes) are purely descriptive
— they document the pre-existing (and, per a direct diff check,
byte-for-byte unchanged apart from one typo fix) ownership check inside
`RocksDBDumpManager::find()`/`remove()`, which compares the requesting
user (`getAuthorizedUser()`) against the user who created the dump
context. No code or behavior changed here.

### Summary

| Route | Verdict |
|---|---|
| `GET /_api/version`, `/_admin/version` | Identical to `devel` — both diffs already fully covered in earlier sessions |
| `GET/POST /_admin/deployment/id` | Identical to `devel` — no auth check in either branch, comment/include-cleanup only |
| `POST /_api/dump/start` | Identical ALLOW/DENY to `devel` (Finding 1 cosmetic; Finding 2 verified equivalent) |
| `POST /_api/dump/next/<id>`, `DELETE /_api/dump/<id>` | Identical to `devel` — ownership check in `RocksDBDumpManager` unchanged |

**Action items / recommendations:** None. Every substantive diff across
these three handlers reuses an authorization-refactor pattern already
fully proven equivalent in a prior session of this document; no new
regressions were found.

## `RestSupervisionStateHandler`, `RestTransactionHandler` and `RestAdminClusterHandler`

Mounted at `/_admin/supervisionState` (exact), `/_api/transaction` (prefix),
and `/_admin/cluster` (prefix), respectively. The last one is by far the
largest and most consequential of the three — as anticipated, it produced
genuine, security-relevant findings in *both* directions.

### `RestSupervisionStateHandler` — clean, one cosmetic diff

`executeAsync()`'s sole gate,
`isAdminUser()` → `canUseAdminAction(auth::perms::AdminSupervisionState{})`
(`arangod/RestHandler/RestSupervisionStateHandler.cpp:44-50`), falls
through to the generic `[&](p::AnyAdmin auto const&) -> Result { return
isAdmin(); }` catch-all (`arangod/Auth/AuthMode.cpp:367`) — confirmed no
dedicated `AdminSupervisionState` case exists in `AuthMode::Classic::check()`.
Identical `_system` RW semantics to `devel`'s `isAdminUser()`. The rest of
the diff is dead-`#include` cleanup only; `.h` is byte-for-byte identical.

### `RestTransactionHandler` — no findings

The main transaction lifecycle (`POST` begin, `PUT` commit, `DELETE` abort,
`GET` status) has **no handler-local authorization code at all** (confirmed
by grep); it relies entirely on `SingleCollectionTransaction`/
`TransactionState::checkCollectionPermission()`, the same shared machinery
already fully analyzed for `RestDocumentHandler` and `RestImportHandler` —
including the previously-documented, cross-cutting read-only-mode
`TRI_ERROR_FORBIDDEN`-vs-`TRI_ERROR_ARANGO_READ_ONLY` addendum, which
applies here identically and is not repeated as a new finding.

The only diff is in the maintainer-mode-only, explicitly-unofficial
`GET/DELETE .../history` debug routes
(`arangod/RestHandler/RestTransactionHandler.cpp:178,332`):
```cpp
// devel:
auto auth = AuthenticationFeature::instance();
if ((auth == nullptr || !auth->isActive()) ||
    (auth->isActive() && ExecContext::current().isSuperuser())) { ... }
// current:
if (ExecContext::current().isSuperuserOrDisabled()) { ... }
```
This is the same `isSuperuser()`/auth-disabled-check → `isSuperuserOrDisabled()`
collapse already proven equivalent multiple times in this document (e.g.
`RestCompactHandler`, `RestAdminLogHandler`) — identical semantics within
Classic mode; only differs when authentication is globally disabled
(out of this document's scope). Verified equivalent, no new finding.

### Finding 1 (Cosmetic, batch): `RestAdminClusterHandler` — widespread `isAdminUser()` → `canUseAdminAction(X)` migrations

The top-level JWT-policy gate in `executeAsync()`
(`arangod/RestHandler/RestAdminClusterHandler.cpp:366-395`) changed
`isSuperuser()` → `isSuperuserOrDisabled()` — the standard, already-proven
auth-disabled-widening pattern, out of Classic-mode scope.

Beyond that, no fewer than **eleven** call sites across this handler
migrated a bare `isAdminUser()` check to `canUseAdminAction(X)` for one of
five different permission tags: `AdminRemoveServer` (`handleRemoveServer`),
`AdminClusterInfo` (`handleShardStatistics`, `handleShardDistribution`,
`handleCollectionShardDistribution`), `AdminMoveShards`
(`handleQueryJobStatus`, `handleCancelJob`, `handleSingleServerJob`),
`AdminMaintenance` (`handleMaintenance`, `handleDBServerMaintenance`,
`handlePutNumberOfServers`, `handleUniqId`), and `AdminRebalance`
(`handleRebalance`). I confirmed via `grep` on `arangod/Auth/AuthMode.cpp`
that **none** of these five permission tags has a dedicated case in
`AuthMode::Classic::check()` — all five fall through to the same generic
`[&](p::AnyAdmin auto const&) -> Result { return isAdmin(); }` catch-all
(`arangod/Auth/AuthMode.cpp:367`), which is exactly `devel`'s
`isAdminUser()` test (`_system` RW). Purely a naming/clarity refactor in
preparation for RBAC's fine-grained per-action permissions; zero behavioral
difference in Classic mode across all eleven sites. Only the error-message
text now varies (`r.errorMessage()` instead of the empty default), matching
the pattern established for essentially every other handler in this
document.

### Finding 2 (Confirmed devel-side gap, now fixed): `handleRebalanceShards()` — database-scoped check widened to a proper admin check

`devel` (`devel:arangod/RestHandler/RestAdminClusterHandler.cpp:2505-2510`):
```cpp
ExecContext const& exec = ExecContext::current();
if (!exec.canUseDatabase(auth::Level::RW)) {
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                "insufficient permissions");
  co_return;
}
```
Current branch (`arangod/RestHandler/RestAdminClusterHandler.cpp:2545-2551`):
```cpp
ExecContext const& exec = ExecContext::current();
if (auto r = exec.canUseAdminAction(auth::perms::AdminRebalance{});
    r.fail()) {
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                r.errorMessage());
  co_return;
}
```
`devel`'s no-argument `canUseDatabase(auth::Level requested)` overload
(`devel:arangod/Utils/ExecContext.h:123-125`) checks RW against
`_databaseAuthLevel` — the auth level for **whichever database is in the
request's URL path**, not necessarily `_system`. Since
`RestAdminClusterHandler` is registered as an ordinary database-scoped
prefix handler (`f.addPrefixHandler("/_admin/cluster", ...)`,
`arangod/GeneralServer/GeneralServerFeature.cpp:818`), it is reachable via
`/_db/<any-db>/_admin/cluster/rebalanceShards`. This means in `devel`, a
completely non-admin user who merely holds ordinary `RW` access to their
own personal, non-system database could trigger a genuine **cluster-wide**
shard-rebalancing operation — a serious, confused-deputy-style privilege
gap, since shard rebalancing affects the whole cluster's data placement,
not just the caller's own database. The current branch's
`canUseAdminAction(AdminRebalance{})` correctly requires `_system` RW
(`isAdmin()`) regardless of which database is named in the URL, closing
this gap. This is the second confirmed instance in this document (after
`RestReplicationHandler`'s restore-indexes/restore-view finding) where the
comparison uncovered a genuine, exploitable `devel`-side authorization gap
that the current branch has fixed.

Note `handleRebalance()` (`GET`/`PUT /_admin/cluster/rebalance[/execute]`,
`arangod/RestHandler/RestAdminClusterHandler.cpp:2877-2883`) already used
`isAdminUser()` in `devel` and is unaffected — only the sibling
`handleRebalanceShards()` had the weaker, database-scoped check.

### Finding 3 (Narrow regression, safer direction; same pattern as `RestCollectionHandler` Finding 5): `handleMoveShard()`'s collection-level fallback now also requires database `RW` (container principle)

`devel` (`devel:arangod/RestHandler/RestAdminClusterHandler.cpp:803-808`):
```cpp
auto const& exec = ExecContext::current();
bool canAccess = exec.isAdminUser() ||
                 exec.collectionAuthLevel(ctx->database, ctx->collection) ==
                     auth::Level::RW;
```
Current branch (`arangod/RestHandler/RestAdminClusterHandler.cpp:806-811`):
```cpp
auto const& exec = ExecContext::current();
bool canAccess =
    exec.canUseAdminAction(auth::perms::AdminMoveShards{}).ok() ||
    exec.canUseCollection(ctx->database, ctx->collection,
                          CollectionAccessLevel::WriteMeta)
        .ok();
```
`devel`'s fallback branch was a bare per-collection check:
`collectionAuthLevel(db, coll) == RW`, with no accompanying database-level
requirement. The current branch's `canUseCollection(db, coll, WriteMeta)`
routes through `AuthMode::Classic`'s `UseCollection` case, which for
`CollectionAccessLevel::WriteMeta` carries an explicit container-principle
clause (`arangod/Auth/AuthMode.cpp:236-247`): it additionally requires
`effectiveDatabaseAuthLevel(db) >= RW`. This is the *exact same*
transformation already documented as Finding 5 in the `RestCollectionHandler`
section (`auth_comparison_with_devel.md:769-800`, for `recalculateCount`):
a user granted a per-collection `RW` override on a specific shard's
collection, but only database-level `RO` overall, could `handleMoveShard()`
that collection's shard in `devel` but now gets `403 FORBIDDEN`. Narrow
(requires an unusual grant combination) but real, in the stricter/safer
direction — not a security concern, but a genuine behavioral change.

### Finding 4 (Genuine regression, more permissive — security-relevant): `handleNumberOfServers()`'s `GET` path is no longer gated by `--server.harden`

`devel` (`devel:arangod/RestHandler/RestAdminClusterHandler.cpp:2086-2097`):
```cpp
// GET requests are allowed for everyone, unless --server.harden is used.
// in this case admin privileges are required.
// PUT requests always require admin privileges
ServerSecurityFeature& security = server().getFeature<ServerSecurityFeature>();
bool const needsAdminPrivileges =
    (request()->requestType() != rest::RequestType::GET ||
     security.isRestApiHardened());
if (needsAdminPrivileges && !ExecContext::current().isAdminUser()) {
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
  co_return;
}
```
Current branch (`arangod/RestHandler/RestAdminClusterHandler.cpp:2089-2100`):
```cpp
// GET requests are allowed for everyone, PUT, too, unless
// --server.harden is used. In this case admin privileges are
// required. with RBAC, db:AdminMaintenance is needed for PUT
if (request()->requestType() != rest::RequestType::GET) {
  if (auto r = ExecContext::current().canUseHardenedAction(
          auth::perms::AdminMaintenance{});
      r.fail()) {
    generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN,
                  r.errorMessage());
    co_return;
  }
}
```
The new gate only ever runs for non-`GET` requests — for `GET`, the `if`
block is skipped entirely, so **no check of any kind is performed**,
regardless of the `--server.harden` setting. This diverges from `devel` in
exactly one case: when `--server.harden=true`, `devel` required `_system`
RW (`isAdminUser()`) to `GET /_admin/cluster/numberOfServers`, but the
current branch now allows **any authenticated user** to read the cluster's
coordinator/DBServer counts — silently bypassing the hardening feature for
this specific piece of server-internal information. (With the default
`--server.harden=false`, both branches already allowed this for everyone,
so this only manifests on hardened installations.)

Note that `PUT` is **not** actually affected in practice, despite the
entry-level gate now being conditional on hardening: `handlePutNumberOfServers()`
itself (called from the `PUT` case) carries its own, unconditional
`canUseAdminAction(auth::perms::AdminMaintenance{})` check
(`arangod/RestHandler/RestAdminClusterHandler.cpp:1972-1978`, migrated
1:1 from `devel`'s unconditional `isAdminUser()`), so `PUT` still always
requires admin privileges in both branches, matching `devel`'s
`needsAdminPrivileges` being unconditionally `true` for non-`GET` requests.
Only the `GET` path's hardening protection was lost.

### Summary

| Route / check | Verdict |
|---|---|
| `GET /_admin/supervisionState` | Identical to `devel` (cosmetic renaming only) |
| `POST/PUT/DELETE/GET /_api/transaction/*` (main lifecycle) | Identical to `devel` — no handler-local auth code; same read-only-mode addendum as `RestDocumentHandler`/`RestImportHandler` applies |
| `GET/DELETE /_api/transaction/history` (maintainer-mode only) | Identical to `devel` (verified-equivalent `isSuperuserOrDisabled()` collapse) |
| Eleven `isAdminUser()` → `canUseAdminAction(X)` sites in `RestAdminClusterHandler` | Identical to `devel` (Finding 1, all fall to the same `isAdmin()` catch-all) |
| `POST /_admin/cluster/rebalanceShards` | **Fixed vs. `devel`**: now correctly requires `_system` admin instead of merely `RW` on whatever database happened to be in the URL (Finding 2 — closes a real, exploitable gap) |
| `PUT /_admin/cluster/moveShard` (collection-level fallback) | **Stricter than `devel`** in one narrow grant configuration (Finding 3 — container-principle parity with `RestCollectionHandler` Finding 5) |
| `GET /_admin/cluster/numberOfServers` with `--server.harden=true` | **More permissive than `devel`**: hardening no longer restricts this `GET` to admins (Finding 4) |
| `PUT /_admin/cluster/numberOfServers` | Identical to `devel` — always requires admin via `handlePutNumberOfServers()`'s own unconditional check |

**Action items / recommendations:**
1. **Finding 4 is the actionable item from this session**: restore the
   hardening protection for `GET /_admin/cluster/numberOfServers` by moving
   (or duplicating) the `canUseHardenedAction(AdminMaintenance{})` check in
   `handleNumberOfServers()` (`arangod/RestHandler/RestAdminClusterHandler.cpp:2089-2100`)
   outside the `requestType() != GET` guard, so it also applies to `GET`
   (mirroring `devel`'s `needsAdminPrivileges` formula, which OR'd in
   `security.isRestApiHardened()` regardless of request type).
2. Findings 2 and 3 require no action: Finding 2 is a confirmed fix of a
   genuine `devel` privilege-escalation gap (recommend highlighting in
   release notes as a security hardening); Finding 3 is a narrow,
   safer-direction tightening consistent with an already-accepted pattern
   elsewhere in the codebase (`RestCollectionHandler` Finding 5) and needs
   no correction, only optional release-note awareness.

## `RestTtlHandler`, `RestOpenApiHandler` and `RestViewHandler`

Mounted at `/_api/ttl` (prefix, `_system` database only), `/openapi.json`
(exact, unauthenticated-reachable static spec), and `/_api/view` (prefix),
respectively. The first two are trivial; `RestViewHandler`, as anticipated,
turned out to be the most substantial finding of this session — the
current branch appears to have finally closed a long-standing, explicitly
acknowledged `devel` TODO around per-view/per-collection authorization.

### `RestTtlHandler` — clean, one cosmetic diff

No authorization code exists in this handler in either branch (confirmed
by grep); it relies solely on the shared base-`RestVocbaseBaseHandler`
database-level gate (`RO` for `GET`, `RW` for `PUT`, enforced generically
via `checkUserCanAccess()`), plus its own `!_vocbase.isSystem()` guard
(`arangod/RestHandler/RestTtlHandler.cpp:45-49`), unchanged from `devel`.
The only diff is one added comment; `.h` is byte-for-byte identical.

### `RestOpenApiHandler` — clean, one cosmetic diff

Serves a static, compile-time-embedded OpenAPI spec with **no
authorization code whatsoever** in either branch — matching its documented
intent as a publicly reachable, unauthenticated discovery endpoint. Only
diff is one added comment; `.h` is byte-for-byte identical.

### Finding 1 (Confirmed equivalent): `getView()`, `deleteView()`, per-view listing filter, and rename — same database-level semantics as `devel`

Four of `RestViewHandler`'s checks carry over `devel`'s semantics exactly,
just renamed/relocated from the old free-standing `canUse()` helper /
`LogicalView::canUse()` member into dedicated `ExecContext` methods:

| Operation | `devel` | Current | Classic-mode semantics |
|---|---|---|---|
| `getView()` (`arangod/RestHandler/RestViewHandler.cpp:63-69`) | `view->canUse(RO)` | `canUseView(db, name, ViewAccessLevel::Read)` | Both reduce to `effectiveDatabaseAuthLevel(db) >= RO` — see below |
| `deleteView()` (`:404-411`) | `view->canUse(RW)` | `canDropView(db, name)` | Both reduce to `effectiveDatabaseAuthLevel(db) >= RW` |
| rename branch of `modifyView()` (`:306-311`) | `view->canUse(RW)` | `canRenameView(db, oldName, newName)` | Both reduce to `effectiveDatabaseAuthLevel(db) >= RW` |
| per-view filter in `getViews()` (`:489-494`) | `view->canUse(RO)` | `canSeeView(db, name)` | Both reduce to `effectiveDatabaseAuthLevel(db) >= RO` |

I confirmed `devel:arangod/VocBase/LogicalView.cpp:134-141`'s
`LogicalView::canUse(level)` is a **pure database-level check** —
`ExecContext::current().canUseDatabase(vocbase().name(), level)` — with an
explicit comment stating "per-view authentication checks disabled as per
[backlog#459]" (view-specific auth overrides were never actually
evaluated). The current branch's `AuthMode::Classic` cases for `UseView`
(`arangod/Auth/AuthMode.cpp:328-347`) and `SeeView`
(`arangod/Auth/AuthMode.cpp:412-423`) preserve this exact limitation
verbatim, with an explicit comment: "In the classic system views delegate
to database-level access (per-view collection-level auth is not used for
views)." `DropView` (`arangod/Auth/AuthMode.cpp:478-481`) and `RenameView`
(`arangod/Auth/AuthMode.cpp:465-477`) likewise just check database `RW`.
Verified byte-for-byte equivalent ALLOW/DENY outcomes in Classic mode for
all four operations; only cosmetic differences (helper renaming, more
specific error messages via `Result` instead of a fixed generic message).

Also confirmed **no-op**, per the same reasoning already established for
`RestGraphHandler`'s Finding 4: the top-level `canUse(RO, _vocbase)` gate
that `devel` ran before enumerating views in `getViews()`
(`devel:arangod/RestHandler/RestViewHandler.cpp:439-444`) was removed
(replaced with a commented-out `// TODO check access right per view`
block) — but this check is strictly redundant with the base
`RestHandler::checkUserCanAccess()` gate that already requires database
`RO` before any handler code runs at all, so its removal has zero
observable effect.

### Finding 2 (Genuine devel-side gap, now fixed — the headline result): `createView()` and `modifyView()` now require read access to every linked collection

`devel` (`devel:arangod/RestHandler/RestViewHandler.cpp:198-204`, `createView()`):
```cpp
if (!canUse(auth::Level::RW, _vocbase)) {
  generateError(
      Result(TRI_ERROR_FORBIDDEN, "insufficient rights to create view"));
  ...
}
```
Current branch (`arangod/RestHandler/RestViewHandler.cpp:189-206`):
```cpp
auto const& execContext = ExecContext::current();
std::vector<std::string> linkedCollections;
if (auto linksSlice = body.get("links"); linksSlice.isObject()) {
  for (auto const& pair : VPackObjectIterator(linksSlice)) {
    linkedCollections.push_back(pair.key.copyString());
  }
}
if (auto r = execContext.canCreateView(
        _vocbase.name(), nameSlice.stringView(), linkedCollections);
    !r.ok()) {
  generateError(r);
  ...
}
```
`ExecContext::canCreateView()` (`arangod/Utils/ExecContext.cpp:314-323`)
routes to `AuthMode::Classic`'s `CreateView` case
(`arangod/Auth/AuthMode.cpp:425-447`), which — beyond the same database
`RW` check `devel` already performed — **additionally requires `UseCollection(Read)`
on every collection named in the request body's `links` object**
(`arangod/Auth/AuthMode.cpp:433-444`). The exact same additional
requirement was added to `modifyView()`'s non-rename branch via
`canModifyView()` (`arangod/RestHandler/RestViewHandler.cpp:312-324` →
`arangod/Auth/AuthMode.cpp:448-464`), checking read access to every
collection in the (possibly partial, for `PATCH`) updated `links` object
from the request body.

This directly closes the gap left open by `devel`'s disabled per-view/
per-collection check (see Finding 1): in `devel`, any user with database
`RW` could create or update an ArangoSearch/search-alias view linking to
**any** collection in the database — including one they had been
explicitly denied access to via a per-collection override — since only
the blanket database-level `RW` check applied. A search view built on
such a collection would expose its indexed field values through view
queries, bypassing the collection-level deny. The current branch closes
this: creating/modifying a view that links to a collection the caller
cannot read is now rejected with `403 FORBIDDEN`.

This is a real, narrow-but-security-relevant behavioral change (it only
manifests when a per-collection access override diverges from the
database default — the same "unusual grant configuration" precondition
seen in several other findings in this document), and — like
`RestReplicationHandler`'s restore-indexes/restore-view finding and
`RestAdminClusterHandler`'s `rebalanceShards` finding — is best understood
as the current branch **fixing** a genuine, explicitly-acknowledged
`devel` gap (the `backlog#459` TODO) rather than introducing a regression.

### Summary

| Route | Verdict |
|---|---|
| `GET/PUT /_api/ttl/properties`, `GET /_api/ttl/statistics` | Identical to `devel` — no auth code, base-handler gate only |
| `GET /openapi.json` | Identical to `devel` — no auth code, intentionally public |
| `GET /_api/view[/<name>[/properties]]` | Identical to `devel` (Finding 1) |
| `DELETE /_api/view/<name>` | Identical to `devel` (Finding 1) |
| `PUT /_api/view/<name>/rename` | Identical to `devel` (Finding 1) |
| `POST /_api/view` (create) | **Fixed vs. `devel`**: now also requires read access to every linked collection (Finding 2) |
| `PUT/PATCH /_api/view/<name>/properties` (modify) | **Fixed vs. `devel`**: same new linked-collection check (Finding 2) |

**Action items / recommendations:** None required — Finding 2 is a
deliberate, welcome closure of a long-standing, explicitly-documented
`devel` authorization gap (`backlog#459`); worth a release note
highlighting that view creation/modification now validates access to
linked collections, since applications relying on the old (lax) behavior
with explicitly-denied collections would need those denials revisited.

## `RestKeyGeneratorsHandler`, `RestQueryPlanCacheHandler`, `RestShutdownHandler` and `RestLicenseHandler`

### `RestKeyGeneratorsHandler` (`arangod/RestHandler/RestKeyGeneratorsHandler.cpp`)

Mounted at `GET /_api/key-generators` (prefix), it returns the static list
of registered key generator names (`arangod/RestHandler/RestKeyGeneratorsHandler.cpp:38-61`).
No authorization code exists in either branch (confirmed by grep); the
diff vs. `devel` is a single added comment
(`arangod/RestHandler/RestKeyGeneratorsHandler.cpp:37`). Relies solely on
the shared base-`RestHandler` authentication requirement, matching its
documented `AUTHEN`-only level (`Documentation/path_permissions.md:967`).
No findings.

### `RestQueryPlanCacheHandler` (`arangod/RestHandler/RestQueryPlanCacheHandler.cpp`)

Mounted at `/_api/query-plan-cache` (prefix), with `GET` (`readPlans()`)
listing cached query plans and `DELETE` (`clearCache()`) invalidating the
whole cache for the current database.

**Finding 1 (cosmetic).** `clearCache()`'s permission check
(`arangod/RestHandler/RestQueryPlanCacheHandler.cpp:58-69`) changed from
`devel`'s bare `bool ExecContext::canUseDatabase(auth::Level::RW)`
(implicit current database, boolean return) to
`Result ExecContext::canUseDatabase(_vocbase.name(), DatabaseAccessLevel::Write)`
(explicit database, `Result` return). Since `_vocbase.name()` is always
the same database the old implicit-current-database overload resolved
against (`arangod/Utils/ExecContext.h:123-128`, devel), the permission
semantics are identical — only the error message text changes (now
includes `r.errorMessage()`).

**Finding 2 (narrow regression, more restrictive — the one worth
flagging).** The new `Result ExecContext::canUseDatabase(std::string_view db, DatabaseAccessLevel level)`
overload (`arangod/Utils/ExecContext.cpp:189-197`) is not a pure
permission check: it adds an unconditional
`if (!isSuperuser() && ServerState::readOnly() && level >= DatabaseAccessLevel::Write) return {TRI_ERROR_FORBIDDEN, "Server is in read-only mode."}`
guard before consulting the auth backend at all. `devel`'s plain
`auth::Level`-based `canUseDatabase()` never performed any such
server-read-only-mode check — it looked at stored permissions only.

This matters here in a way it did not for the previously-documented
read-only-mode findings (`RestDocumentHandler` Finding 1,
`RestCollectionHandler`'s `truncate` addendum): in those cases the
explicit handler-level check was provably redundant, because the actual
write always went through `TransactionState`/the storage engine, which
independently enforces the exact same read-only-mode rule
(`arangod/Transaction/Methods.cpp:3763-3769`) — so removing or adding the
early check never changed the final outcome. Here, `clearCache()`'s
actual effect, `_vocbase.queryPlanCache().invalidateAll()`
(`arangod/RestHandler/RestQueryPlanCacheHandler.cpp:71`), is a **pure
in-memory, non-persistent maintenance operation** — it never touches the
storage engine and was never subject to any read-only-mode check of its
own, in either branch. So this is a genuine, standalone new restriction:
an operator with legitimate database `RW` permissions could clear stale
query plan cache entries in `devel` even while the server runs in
`--server.read-only` mode (a plausible maintenance action, since it
mutates no persistent data); in the current branch, the same request now
fails with `403 FORBIDDEN "Server is in read-only mode."` before ever
reaching the cache.

This is narrow (only matters for servers in read-only mode) and strictly
more restrictive (no security concern), but is a real, user-visible
behavioral difference distinct from the "provably no-op" pattern
established elsewhere in this document, so it is recorded as its own
finding rather than folded into an existing addendum. Note also the
pre-existing `// TODO Should this get a separate admin action/permission?`
comment (`arangod/RestHandler/RestQueryPlanCacheHandler.cpp:59`), which
mirrors `Documentation/path_permissions.md:985`'s own
`needs an RBAC solution? FIXME` annotation — this is an acknowledged,
pre-existing open design question in both branches, not something this
comparison newly uncovered.

**Finding 3 (confirmed equivalent).** `readPlans()`
(`arangod/RestHandler/RestQueryPlanCacheHandler.cpp:82-105`) dropped
`devel`'s explicit `canUseDatabase(auth::Level::RO)` upfront check. This
is a no-op: the shared base `RestHandler::checkUserCanAccess()`
(`arangod/GeneralServer/RestHandler.cpp:705-724`) already gates every
request on database-level `RO` before any handler's `execute()` runs —
the same established fact already relied upon for `RestGraphHandler`
Finding 4 and `RestCursorHandler`.

**Finding 4 (confirmed equivalent).** The per-cache-entry filter's
condition changed from `devel`'s
`ExecContext::isAuthEnabled() && !ExecContext::current().isSuperuser()`
to `!context.isSuperuserOrDisabled()` — the same collapsed-condition
refactor already proven equivalent multiple times in this document
(`RestCompactHandler`, `RestAdminServerHandler`, `RestAuthReloadHandler`
sessions): `isSuperuserOrDisabled()` returns `true` exactly when auth is
disabled *or* the caller is a superuser, so negating it is identical to
`devel`'s two-part condition. Similarly, `canUseCollection(name, RO)`
(implicit current database) became
`canUseCollection(databaseName, name, AccessLevel::Read).ok()` with
`databaseName = _vocbase.name()` captured explicitly — the same database,
so no behavioral change.

### `RestShutdownHandler` (`arangod/RestHandler/RestShutdownHandler.cpp`)

Mounted at `/_admin/shutdown` (prefix; `GET` for soft-shutdown status on
coordinators, `DELETE` to trigger shutdown).

**Finding 5 (confirmed equivalent).** `devel`'s inline permission check
(manually reading `AuthenticationFeature::instance()` and
`userManager()->databaseAuthLevel(user, "_system", true)`) was replaced
by `ExecContext::current().canUseAdminAction(auth::perms::AdminShutdown{})`
(`arangod/RestHandler/RestShutdownHandler.cpp:59-65`). `AdminShutdown` is
part of `detail::AdminList` (`arangod/Auth/Permissions.h:114`), so it
falls through the generic `[&](p::AnyAdmin auto const&) -> Result { return isAdmin(); }`
catch-all (`arangod/Auth/AuthMode.cpp:367`), and
`AuthMode::Classic::isAdmin()` (`arangod/Auth/AuthMode.cpp:574-577`) is
defined as exactly `check(UseDatabase{"_system", Write})` —
i.e. `databaseAuthLevel(user, "_system") >= RW`, the identical test
`devel` performed by hand. Tracing every branch of `devel`'s condition
confirms full equivalence:
- **Auth disabled** (`!af->isActive()`): `devel` skips the check entirely
  (allow). Current: `ExecContext::create()` selects `AuthMode::Disabled`
  whenever `!authenticationFeature.isActive()`
  (`arangod/Utils/ExecContext.cpp:90-92`), and
  `AuthMode::Disabled::check()` unconditionally returns `{}`
  (`arangod/Auth/AuthMode.cpp:659-661`) — same allow.
- **Superuser JWT (empty `user()`)**: `devel` skips the check
  (`!_request->user().empty()` is false, allow). Current:
  `ExecContext::create()` detects `req.authenticated() && req.user().empty() && JWT`
  and selects the dynamic `AuthMode::Superuser` context
  (`arangod/Utils/ExecContext.cpp:82-89`), whose `check()` also
  unconditionally returns `{}` (`arangod/Auth/AuthMode.cpp:64-66`) — same
  allow.
- **Regular authenticated user**: both branches require `_system` `RW`
  — identical outcome.
- **`userManager() == nullptr` with a non-empty authenticated user**
  (the DBServer/Agent edge case `devel` specially fell back to
  `lvl = auth::Level::RW`, i.e. unconditional allow): this exact scenario
  was already investigated and proven **unreachable in practice** in an
  earlier session of this document
  (`auth_comparison_with_devel.md:2659-2666`) — `auth::TokenCache::validateJwtBody()`
  returns `Entry::Unauthenticated()` whenever `_userManager == nullptr`
  for any JWT that isn't the empty-username superuser token, so a
  non-empty, authenticated user can never coexist with a null
  `UserManager`. Since it cannot occur on either branch, this edge case
  contributes no behavioral difference.

No findings — full equivalence confirmed for every reachable case.

### `RestLicenseHandler` (`arangod/RestHandler/RestLicenseHandler.cpp`, community build)

**Finding 6 (cosmetic).** `ServerSecurityFeature::canAccessHardenedApi()`
→ `ExecContext::current().canUseHardenedAction(AdminLicense{})`
(`arangod/RestHandler/RestLicenseHandler.cpp:46-53`) is the same migration
already fully proven equivalent multiple times in this document
(`RestMetricsHandler`, `RestUsageMetricsHandler`/`RestEngineHandler`,
`RestAdminStatisticsHandler`, `RestVersionHandler`). `AdminLicense` is
also part of `detail::AdminList`, so it resolves through the same
`isAdmin()` catch-all when the REST API is hardened, and is a no-op check
(always allowed) when it is not — identical to `devel`'s
`canAccessHardenedApi()` semantics. Only the error path changes (now
includes `r.errorMessage()`, still generic enough to "not leak
information about server internals").

**Finding 7 (confirmed dead-code cleanup, zero impact).** `devel`'s
`RestLicenseHandler::verifyPermitted()` (declared in the header, defined
in the `.cpp` guarded by `#ifdef USE_ENTERPRISE`) was removed along with
its header declaration. Grepping both branches confirms this method was
never called anywhere in the community tree. Since the actual Enterprise
build lives in a separate submodule (`enterprise/`, its own git
repository with its own `devel` branch), I additionally checked
`enterprise/Enterprise/RestHandler/RestLicenseHandlerEE.cpp` — which
supplies the real `RestLicenseHandler::execute()` for Enterprise builds —
directly in that repository. It confirms: (a) `verifyPermitted()` was
never called there either (the only `verifyPermitted()` calls anywhere in
the enterprise tree belong to the unrelated `RestHotBackupHandler`
class); and (b) the Enterprise `execute()` independently underwent the
exact same two refactors as the community file
(`canAccessHardenedApi()` → `canUseHardenedAction(AdminLicense{})`, and
`!isAuthEnabled() || isSuperuser()` → `isSuperuserOrDisabled()`),
confirming consistency across both build variants. No behavioral impact.

### Summary

| Route | Verdict |
|---|---|
| `GET /_api/key-generators` | Identical to `devel` — no auth code, base-handler gate only |
| `GET /_api/query-plan-cache` | Confirmed equivalent (Finding 3, Finding 4) |
| `DELETE /_api/query-plan-cache` | **Regression (safer direction)**: now blocked while the server is in read-only mode, even though the operation is purely in-memory (Finding 2); signature/message-only cosmetic change otherwise (Finding 1) |
| `GET/DELETE /_admin/shutdown` | Confirmed equivalent for every reachable case (Finding 5) |
| `GET/PUT /_admin/license` (community + Enterprise) | Confirmed equivalent (Finding 6); dead-code cleanup only (Finding 7) |

**Action items / recommendations:** Finding 2 is optional to address —
consider whether `RestQueryPlanCacheHandler::clearCache()` should call the
plain `canUseDatabase(auth::Level)`-style check (or a dedicated
non-read-only-gated variant) instead of the generic
`ExecContext::canUseDatabase(db, level)`, since invalidating an in-memory
cache is not a persistent write and arguably should remain available
during read-only-mode maintenance windows. No other action items.

## `RestExplainHandler`, `RestAqlUserFunctionsHandler`, `RestCrashHandler` and `RestIndexHandler`

### `RestExplainHandler` (`arangod/RestHandler/RestExplainHandler.cpp`)

Diff vs. `devel` is a single added comment
(`arangod/RestHandler/RestExplainHandler.cpp:41`, `// Mounted at
/_api/explain (prefix)`); `.h` is byte-for-byte identical. No authorization
code exists in this handler in either branch (confirmed by grep) — it
relies entirely on the shared base-`RestHandler` database-level gate.

No findings.

### `RestAqlUserFunctionsHandler` (`arangod/RestHandler/RestAqlUserFunctionsHandler.cpp`)

Diff vs. `devel` is a single added comment
(`arangod/RestHandler/RestAqlUserFunctionsHandler.cpp:44`); `.h` is
byte-for-byte identical. No authorization code in the handler itself; its
delegate, `arangod/VocBase/Methods/AqlUserFunctions.cpp`, is also
byte-for-byte identical between branches (full diff is empty).

No findings.

### `RestCrashHandler` (`arangod/RestHandler/RestCrashHandler.cpp`, mounted at `/_admin/crashes`)

**Finding 1 (cosmetic).** `ExecContext::current().isAdminUser()` →
`canUseAdminAction(AdminCrashHandler{})`
(`arangod/RestHandler/RestCrashHandler.cpp:42-48`). `AdminCrashHandler` is
part of `detail::AdminList`/`auth::perms::AnyAdmin`
(`arangod/Auth/Permissions.h:88,113`, matching
`Documentation/path_permissions.md:787-789`), so this dispatches to the
same generic `isAdmin()` catch-all already proven equivalent throughout
this document. Only the error message text changes (`r.errorMessage()`
vs. the old hard-coded string); HTTP status (`403`) and `errorNum`
(`TRI_ERROR_HTTP_FORBIDDEN`) are unchanged.

**Finding 2 (genuine regression — real, admin-gated path-traversal exposure).**
The handler-level `DumpManager::isValidCrashId(crashId)` check
(`devel:arangod/RestHandler/RestCrashHandler.cpp:69-73`) was removed
without replacement
(`arangod/RestHandler/RestCrashHandler.cpp:64-79`). Tracing this further
into `lib/CrashHandler/DumpManager.cpp` reveals this isn't merely
redundant defense-in-depth being cleaned up: in `devel`,
`DumpManager::getCrashContents()` and (implicitly, via the same pattern)
`deleteCrash()` route through a private `resolveCrashDirectory()` helper
that itself calls `isValidCrashId()` again
(`/tmp/devel_DumpManager.cpp:51-58`, `87-93`) — a strict UUID-format check
via `boost::uuids::string_generator` — so `devel` validates the crash ID
as a well-formed UUID at **two independent layers** before ever
constructing a filesystem path from client-supplied input.

In the current branch, `lib/CrashHandler/DumpManager.cpp:67-106` builds
`crashDir = _crashesDirectory / std::string(crashId)` **directly from the
raw, unvalidated suffix** in both `getCrashContents()` (used by `GET
/_admin/crashes/{id}`, which returns the contents of every regular file
found under the resolved directory in the JSON response) and
`deleteCrash()` (used by `DELETE /_admin/crashes/{id}`, which calls
`std::filesystem::remove_all(crashDir, ec)`). Since `crashId` is taken
verbatim from `_request->suffixes()[0]`
(`arangod/RestHandler/RestCrashHandler.cpp:70`), a caller can supply a
value such as `..` as the single path suffix — no URL-encoding of a slash
is even required, since `..` is a single, ordinary path segment — causing
`_crashesDirectory / ".."` to resolve one level up (e.g. to the server's
data directory). This lets an admin-authorized-but-not-meant-to-have-
filesystem-access caller read arbitrary regular files reachable via
directory traversal from the crashes directory (via `GET`), or recursively
delete arbitrary directories reachable the same way (via `DELETE`). This
requires passing Finding 1's admin check first, so it is not a privilege
escalation from anonymous access, but it is a genuine widening of what an
"admin, crash-management-only" caller can do — from managing crash dumps
to arbitrary filesystem read/delete with the arangod process's
privileges. This is unrelated to the RBAC/Classic-mode toggle (it
reproduces identically whether RBAC is on or off) but is a real,
security-relevant regression worth fixing regardless.

### `RestIndexHandler` (`arangod/RestHandler/RestIndexHandler.cpp`)

Diff vs. `devel` is: one removed unused `#include`, one added comment, and
two functional additions in `arangod/RestHandler/RestIndexHandler.cpp`;
`.h` is byte-for-byte identical.

**Finding 3 (new, gated, deliberate — partial/incomplete fix for a
pre-existing, still-latent gap shared with `devel`).**
`RestIndexHandler::collection()` (`arangod/RestHandler/RestIndexHandler.cpp:226-244`)
gains a new branch, present only on the coordinator and only when
`_request->requestedApiVersion() > 0`:

```cpp
if (ServerState::instance()->isCoordinator()) {
  // Restrict access properly from API version 1 on:
  if (_request->requestedApiVersion() > 0) {
    if (auto r = ExecContext::current().canUseCollection(
            _vocbase.name(), cName, AccessLevel::Read);
        r.fail()) {
      return nullptr;
    }
  }
  return _clusterFeature.clusterInfo().getCollectionNT(_vocbase.name(), cName);
}
```

As with the identically-shaped findings already documented for
`RestDatabaseHandler` and `RestCollectionHandler`'s `compact` route
(`auth_comparison_with_devel.md:274-297`, `620-645`),
`requestedApiVersion()` defaults to `0` for the classic, unprefixed
`/_api/index/...` routes used by every current driver and by `devel`
entirely — so for all traffic seen today this branch never fires, and
behaviour is byte-for-byte identical to `devel`. This is **not** a
Classic-mode regression.

However, tracing what this new, currently-dormant check is actually
guarding against surfaces a genuine, still-open, `devel`-shared gap worth
recording: `RestIndexHandler::getIndexes()` (list/get one or all indexes
of a collection, `GET /_api/index[/{id}]`) calls
`methods::Indexes::getAll()`/`getIndex()`
(`arangod/VocBase/Methods/Indexes.cpp:205`, `146`) directly on the
`LogicalCollection` object — with **no transaction**, and therefore
without ever going through the `TransactionState::checkCollectionPermission()`
machinery that gates ordinary document/CRUD access elsewhere in this
document. Grepping `arangod/VocBase/Methods/Indexes.cpp` confirms neither
`getAll()` nor `getIndex()` perform any `ExecContext`/`canUse*` check at
all, in **either** branch (only `ensureIndex()` and the two `drop()`
overloads do, at lines 487-501 and 753-799). This means: a caller with
mere database-level `RO` — even one holding an explicit collection-level
*deny* override on a specific collection — can list/inspect that
collection's indexes via `/_api/index`, in `devel` and in the current
branch alike, on **every** server role (the single-server/DBServer branch,
`_vocbase.lookupCollection(cName)`, never gained any check at all — only
the coordinator branch did, and only for `requestedApiVersion() > 0`).
So the new check is a **narrow, incomplete, forward-looking** first step
(coordinator-only, opt-in-only) toward closing a gap that continues to
exist unconditionally today, on every role, in both branches.

The unrelated `RestIndexHandler::syncCaches()` addition
(`arangod/RestHandler/RestIndexHandler.cpp:1000-1007`, returning `501 NOT
IMPLEMENTED` on the coordinator for `requestedApiVersion() > 0`) carries no
authorization semantics — it is a plain feature-availability gate, not an
access-control check — and is noted only for completeness.

### Summary

| Route | Verdict |
|---|---|
| `/_api/explain` (all methods) | Identical to `devel` — no auth code |
| `/_api/aqlfunction` (all methods) | Identical to `devel` — no auth code, delegate unchanged |
| `GET /_admin/crashes`, `GET/DELETE /_admin/crashes/{id}` | Admin check confirmed equivalent (Finding 1); **removed crash-ID validation reopens a real path-traversal exposure for an already-admin caller** (Finding 2) |
| `GET /_api/index`, `GET /_api/index/{id}` | No auth-relevant change for `requestedApiVersion() == 0` (today's default, identical to `devel`); new coordinator+v1-only read check only partially/optionally closes a pre-existing, still-latent, `devel`-shared gap (Finding 3) |
| `POST /_api/index`, `DELETE /_api/index/{id}` | Unaffected — always fully checked downstream via `Indexes::ensureIndex()`/`Indexes::drop()`, unchanged in both branches |

**Action items / recommendations:** Finding 2 (`RestCrashHandler`) is the
one item here that warrants prompt attention regardless of the RBAC
comparison's scope — restore ID validation, ideally inside
`DumpManager::getCrashContents()`/`deleteCrash()` themselves (as `devel`
did via `resolveCrashDirectory()`/`isValidCrashId()`), not only in the
REST handler, so every caller of the manager is protected. Finding 3
(`RestIndexHandler`) needs no immediate action — it is intentionally
gated and forward-looking — but is worth keeping in mind if/when the new
API-version scheme becomes the default: at that point, the single-
server/DBServer branch of `collection()` and the `requestedApiVersion() ==
0` path should receive the same `AccessLevel::Read` check for full
coverage.

## `RestQueryHandler`, `RestLogHandler`, `RestAdminExecuteHandler` and `RestSystemReportHandler`

### `RestQueryHandler` (`arangod/RestHandler/RestQueryHandler.cpp`, mounted at `/_api/query` prefix)

Handles AQL query introspection/management: `GET .../current`,
`GET .../slow`, `GET .../properties`, `GET .../rules`, `GET .../registry`,
`PUT .../properties`, `POST /_api/query` (parse), `DELETE .../<id>` (kill),
`DELETE .../slow` (clear slow log).

Only one of these sub-operations, `dumpQueryRegistry()`
(`arangod/RestHandler/RestQueryHandler.cpp:96-100`), has any handler-local
authorization check; all others (`readQuery()`, `replaceProperties()`,
`killQuery()`, `deleteQuerySlow()`, `parseQuery()`,
`handleAvailableOptimizerRules()`) rely solely on the shared base-handler
database-level gate — unchanged in both branches, confirmed by grep.

**Finding 1 (cosmetic, established pattern).** `dumpQueryRegistry()`'s guard
changed from `!ExecContext::current().isSuperuser()` to
`!ExecContext::current().isSuperuserOrDisabled()`
(`arangod/RestHandler/RestQueryHandler.cpp:97`). This is the same widening
already proven equivalent within the scope of Classic mode across several
earlier sessions (`RestCompactHandler`, `RestAdminServerHandler`,
`RestAuthReloadHandler`/`RestDebugHandler`/`RestStatusHandler`/`RestAdminLogHandler`,
`RestAdminStatisticsHandler`/`RestLogInternalHandler`): the two predicates
only diverge when authentication is globally disabled, which is out of
this document's RBAC-vs-Classic-mode scope. No behavioral change for any
authenticated Classic-mode request.

### `RestLogHandler` (`arangod/RestHandler/RestLogHandler.cpp`, mounted at `/_api/log` prefix, only registered when replication2 is enabled in cluster mode)

Manages replicated-log operations (status, head/tail/entries, compaction,
etc. via `ReplicatedLogMethods`).

**Finding 2 (cosmetic, established pattern).** `devel`'s single,
verb-independent guard,

```cpp
if (!ExecContext::current().isAdminUser()) {
  generateError(rest::ResponseCode::FORBIDDEN, TRI_ERROR_HTTP_FORBIDDEN);
  co_return;
}
```

was split by request type
(`arangod/RestHandler/RestLogHandler.cpp:53-69`) into a `GET`-only branch
requiring `auth::perms::AdminReadReplicatedLog{}` and a non-`GET` branch
requiring `auth::perms::AdminWriteReplicatedLog{}`. Both permission tags
are members of the `AnyAdmin` type list
(`arangod/Auth/Permissions.h:102-103,111-118`), so `AuthMode::Classic`
dispatches both through the exact same generic catch-all `isAdmin()` check
(`_system` RW) that `devel`'s `isAdminUser()` already performed
unconditionally for every verb. The split is purely a forward-looking
scaffold for a future finer-grained RBAC policy (confirmed by the
handler's own added comment, `arangod/RestHandler/RestLogHandler.h:34`:
*"TODO Add (rbac) permission checks, or error-out for now"*) — in Classic
mode, GET and non-GET require identical access today, exactly as in
`devel`. No behavioral change.

### `RestAdminExecuteHandler` (`arangod/RestHandler/RestAdminExecuteHandler.cpp`, mounted at `/_admin/execute`, requires V8)

No authorization code in either branch (confirmed by grep) — this
handler executes arbitrary JavaScript supplied in the request body and
relies entirely on the shared base-handler database-level gate, matching
`Documentation/path_permissions.md`'s documented access level for this
well-known, inherently dangerous, opt-in-only (`--javascript.allow-admin-execute`)
endpoint. Diff vs. `devel` is a single added comment
(`arangod/RestHandler/RestAdminExecuteHandler.cpp:53`). No findings.

### `RestSystemReportHandler` (`arangod/RestHandler/RestSystemReportHandler.cpp`, mounted at `/_admin/system-report`, exact)

**Finding 3 (cosmetic, established pattern).** `devel`'s handler-local
`isAdminUser()` helper —

```cpp
bool RestSystemReportHandler::isAdminUser() const {
  if (!ExecContext::isAuthEnabled()) {
    return true;
  } else {
    return ExecContext::current().isAdminUser();
  }
}
```

— together with the separate `ServerSecurityFeature::canAccessHardenedApi()`
gate in `execute()`, was consolidated into a single
`ExecContext::current().canUseHardenedAction(AdminMonitoringInternal{})`
call (`arangod/RestHandler/RestSystemReportHandler.cpp:77-84`). This is
the same `canUseHardenedAction()`/`AdminMonitoringInternal` migration
already fully proven equivalent for `RestMetricsHandler`,
`RestUsageMetricsHandler`/`RestEngineHandler`,
`RestAdminStatisticsHandler`, and `RestVersionHandler` in earlier
sessions — `--server.harden` semantics plus the generic `AnyAdmin`
catch-all reproduce `devel`'s combined check exactly. The now-unused
`isAdminUser()` declaration was correctly removed from the header
alongside it. No behavioral change.

### Summary

| Route | Verdict |
|---|---|
| `GET/PUT/POST/DELETE /_api/query/*` (all sub-ops except `registry`) | Identical to `devel` — no auth code, unchanged base-handler gate |
| `GET /_api/query/registry` | Superuser check confirmed equivalent (Finding 1) |
| `/_api/log/*` (all verbs, replication2 clusters only) | Verb-split admin check confirmed equivalent — both GET and non-GET still resolve to the same `_system`-RW test as `devel` (Finding 2) |
| `/_admin/execute` (POST) | Identical to `devel` — no auth code |
| `GET /_admin/system-report` | Combined hardened-admin check confirmed equivalent (Finding 3) |

**Action items / recommendations:** none. Every diff across all four
handlers in this session reuses an authorization-refactor pattern already
fully proven equivalent in a prior session of this document — no new
regressions, no gaps, no action required.

## `RestQueryCacheHandler`, `RestAuthHandler`, `RestUsersHandler` and `RestEdgesHandler`

### `RestQueryCacheHandler` (`arangod/RestHandler/RestQueryCacheHandler.cpp`)

Both `clearCache()` and `properties()` (its `PUT` counterpart) gained an
identical, `requestedApiVersion() > 0`-gated block
(`arangod/RestHandler/RestQueryCacheHandler.cpp:62-74,129-141`):

```cpp
if (_request->requestedApiVersion() > 0) {
  if (!_vocbase.isSystem()) {
    generateError(rest::ResponseCode::FORBIDDEN,
                  TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE);
    return;
  }
  if (auto r = ExecContext::current().canUseAdminAction(
          auth::perms::AdminQueryCache{});
      r.fail()) {
    generateError(r);
    return;
  }
}
```

This is the same, already-established `requestedApiVersion()`-gated
scaffolding pattern seen for `RestDatabaseHandler`, `RestCollectionHandler`,
and `RestIndexHandler`: since no client sends an API version greater than
`0` today, this block is dead code in practice — a **no-op**, not a
Classic-mode regression. `AdminQueryCache` is part of the generic
`AnyAdmin` list (`arangod/Auth/Permissions.h:87,112`), so once opted-in
clients do reach it, it will resolve to the same `_system`-RW test used
throughout this document. The rest of the diff is a single added comment.

### `RestAuthHandler` (`arangod/RestHandler/RestAuthHandler.cpp`)

`devel`'s special-case handling of `/_open/auth` lived entirely in
`CommTask::canAccessPath()`
(`/tmp/devel_CommTask.cpp:849-855`, effectively unchanged from earlier
sessions):

```cpp
if (path == "/" || path.starts_with(::pathPrefixOpen) ||
    path.starts_with(::pathPrefixAdminAardvark) ||
    path == "/_admin/server/availability") {
  // mop: these paths are always callable...they will be able to check
  // req.user when it could be validated
  result = Flow::Continue;
  vc->forceSuperuser();
}
```

i.e. any request (even fully unauthenticated) to `/_open/*` is let through
and the request's `ExecContext` is force-escalated to superuser — needed
because this is precisely the login/token endpoint, which by definition
must be reachable before the caller has a token. The current branch moves
this into a handler-local override
(`arangod/RestHandler/RestAuthHandler.cpp:204-209`):

```cpp
async<Result> RestAuthHandler::checkUserCanAccess() const {
  auto ec = _request->requestContext();
  TRI_ASSERT(ec != nullptr);
  ec->forceSuperuser();
  co_return Result{};
}
```

Confirmed equivalent: unconditional success plus `forceSuperuser()`,
exactly reproducing `devel`'s path-based exception (this handler is only
ever mounted at `/_open/auth` and `/_open/auth/renew`, so the check is
unconditionally correct here, unlike a generic path-prefix test). No
behavioral difference; the rest of the diff is a single added comment and
an added `#include`.

### `RestUsersHandler` (`arangod/RestHandler/RestUsersHandler.cpp`)

This handler is the substantial one this session, and the diff is large
(managing users, their passwords, their per-database/per-collection
access-level grants, and per-user config data at `/_api/user/*`). It
carries over the same `pathPrefixApiUser` exception already covered for
`RestAccessTokenHandler`'s `/_api/token/` prefix, but reimplements it as a
handler-local `checkUserCanAccess()` override
(`arangod/RestHandler/RestUsersHandler.cpp:111-121`):

```cpp
async<Result> RestUsersHandler::checkUserCanAccess() const {
  constexpr std::string_view pathPrefixApiUser("/_api/user/");
  auto const& path = _request->requestPath();
  if (_request->authenticated() && path.starts_with(pathPrefixApiUser)) {
    co_return Result{};
  }
  co_return co_await RestBaseHandler::checkUserCanAccess();
}
```

#### Finding 1 (Confirmed equivalent): the `pathPrefixApiUser` override matches `devel`'s `CommTask` exception

`devel`'s `CommTask::canAccessPath()`
(`/tmp/devel_CommTask.cpp:860-868`) has:

```cpp
} else if (req.requestType() == RequestType::POST && !username.empty() &&
           path.starts_with(std::string{::pathPrefixApiUser} + username +
                            '/')) {
  // simon: unauthorized users should be able to call
  // `/_api/user/<name>` to check their passwords
  result = Flow::Continue;
  vc->forceReadOnly();
} else if (userAuthenticated && path.starts_with(::pathPrefixApiUser)) {
  result = Flow::Continue;
}
```

The first clause (an unauthenticated credential-check POST) requires a
trailing `/` immediately after the username in the path, so it never
actually matches the real `POST /_api/user/{user}` credential-check route
(whose path has no further segments) — it is dead code in `devel` too,
both before and after this branch's refactor, and out of scope for this
comparison. The second clause — `userAuthenticated &&
path.starts_with(pathPrefixApiUser)` — is exactly what the new override
reproduces (`_request->authenticated() && path.starts_with(...)`). Bare
`/_api/user` (no trailing slash, i.e. the `list`/`create` routes) matches
neither `devel`'s nor the current branch's prefix test, so both fall
through to the ordinary database-level gate in both branches. No
divergence.

#### Finding 2 (Confirmed equivalent): listing and reading users

`getRequest()`'s empty-suffix branch (list all users,
`arangod/RestHandler/RestUsersHandler.cpp:138-165`) replaces `devel`'s
`if (isAdminUser()) { ...all users... } else { 403 }` with:

```cpp
if (auto r = exec.canUseAdminAction(auth::perms::AdminReadUsers{});
    r.fail()) {
  generateError(r);
  return RestStatus::DONE;
}
... build userList ...
std::vector<bool> allowed = exec.canReadUsers(userList);
... only include users where allowed[i] is true ...
```

`AdminReadUsers` is part of the generic `AnyAdmin` list
(`arangod/Auth/Permissions.h:83,112`), reducing to the same `isAdmin()`
(`_system` RW) test `devel` used — so a non-admin caller is rejected
before the per-user filter ever runs, exactly like `devel`. For an admin
caller, the per-user filter (`ExecContext::canReadUsers()`,
`arangod/Utils/ExecContext.cpp:465-472`) evaluates `can(ReadUser{name})`
for each entry, which (`arangod/Auth/AuthMode.cpp:560-563`) is again
`isAdmin()` — already known `true` for this caller — so every user passes
the filter and the full list is returned, identical to `devel`. This
two-layer construction is forward-looking RBAC scaffolding (where the
per-user filter will eventually matter); it is a no-op in Classic mode.

The single-user `GET` route (`arangod/RestHandler/RestUsersHandler.cpp:166-179`)
replaces `devel`'s `canAccessUser(user)` with `exec.canReadUser(user)`
(`arangod/Utils/ExecContext.cpp:441-448`):

```cpp
Result ExecContext::canReadUser(std::string_view userName) const {
  if (userName == user()) return {};
  return can(ReadUser{.name{userName}});
}
```

This is byte-for-byte the same self-or-admin logic as `devel`'s
`canAccessUser()` (`/tmp/devel_RestBaseHandler.cpp:67-72`: `user ==
_request->user()` **or** `isAdminUser()`), since `can(ReadUser{})` reduces
to `isAdmin()` as shown above. Confirmed equivalent.

#### Finding 3 (Regression, real and serious — privilege escalation): granting/revoking a user's own database or collection access level no longer requires admin rights

This is the headline result of this session. `devel` had **two distinct**
permission rules for user-management sub-operations:

1. Editing your **own** basic account fields (password, `active` flag,
   dashboard `config` blob) — allowed via the self-or-admin
   `canAccessUser()` check, i.e. a normal user may always edit *their own*
   record.
2. Granting or revoking a **database or collection access-level override**
   for a user (`PUT`/`DELETE /_api/user/{user}/database/{db}[/{coll}]`,
   which sets/clears an explicit `auth::Level` for that user on that
   database/collection) — required **strict, non-bypassable admin rights**
   (`isAdminUser()`, i.e. `_system` RW), with **no self-exception**, even
   though the URL's `{user}` could name the caller themselves. This is a
   deliberately stricter rule: you can edit your own profile, but you
   cannot change your own (or anyone else's) access-level grants unless
   you are already an admin.

The diff shows both `PUT` (`arangod/RestHandler/RestUsersHandler.cpp:410-422`)
and `DELETE` (`arangod/RestHandler/RestUsersHandler.cpp:593-601`)
collapsed rule 2 into the *same* `canWriteUser()` helper used for rule 1:

```cpp
// PUT /_api/user/{name}/database/{db}[/{coll}]  (grant access)
if (auto r = exec.canWriteUser(name); r.fail()) {   // was: if (!isAdminUser())
  generateError(r);
  return RestStatus::DONE;
}
...
VPackSlice grant = body.get("grant");
auth::Level lvl = auth::convertToAuthLevel(grant);
Result r = um->updateUser(name, [&](auth::User& entry) {
  entry.grantDatabase(db, lvl);         // or entry.grantCollection(db, coll, lvl)
  ...
});
```

```cpp
// DELETE /_api/user/{user}/database/{db}[/{coll}]  (revoke access)
if (auto r = exec.canWriteUser(user); r.fail()) {   // was: if (!isAdminUser())
  generateError(r);
  return RestStatus::DONE;
}
```

And `ExecContext::canWriteUser()` (`arangod/Utils/ExecContext.cpp:452-462`)
**does** include the self-bypass:

```cpp
Result ExecContext::canWriteUser(std::string_view userName) const {
  if (!isSuperuser() && ServerState::readOnly()) { ... }
  if (userName == user()) {
    return {};
  }
  return can(WriteUser{.name{userName}});
}
```

Consequence: any authenticated, completely unprivileged user `alice` (with
`NONE` access everywhere) can now issue

```
PUT /_api/user/alice/database/_system
{ "grant": "rw" }
```

and, because `name == exec.user()` short-circuits to success **before**
`can(WriteUser{...})`/`isAdmin()` is ever evaluated, the request succeeds
and grants `alice` `RW` on the `_system` database — which is *exactly*
this codebase's definition of "being an admin"
(`AuthMode::Classic::isAdmin()`, `arangod/Auth/AuthMode.cpp:574-577`:
`check(UseDatabase{"_system", Write})`). The very next request `alice`
makes is evaluated as a full system administrator. This is a genuine,
directly-exploitable **self privilege-escalation vulnerability**,
independent of RBAC — every authenticated user can promote themselves to
admin with a single API call. The symmetric `DELETE` path (self-revoke) is
comparatively harmless (a user can only reduce their own access), but
confirms the same underlying logic error.

I found no compensating check elsewhere: `grantDatabase()`/`grantCollection()`
(`arangod/Auth/User.cpp`) perform no validation against the caller's own
existing privilege level, and the existing test suite
(`tests/RestHandler/RestUsersHandlerTest.cpp`) does not exercise this grant
route at all (its one test, `test_collection_auth`, only checks
`canUseCollection()` given a pre-configured admin grant, never the
`PUT`/`DELETE database` routes), so this was not caught by existing
coverage.

By contrast, the "config" (dashboard) sub-routes
(`arangod/RestHandler/RestUsersHandler.cpp:471-474,638`) and the plain
account-replace routes (`PUT`/`PATCH /_api/user/{user}`,
`arangod/RestHandler/RestUsersHandler.cpp:398-401,534`) already used
`canAccessUser()`/self-bypass in `devel` too — those are unchanged,
intentional self-service operations and are **not** part of this finding.

#### Finding 4 (Confirmed, addendum — already predicted): the `canWriteUser()` read-only-mode gate applies here too

The `RestAccessTokenHandler` session (see above) predicted that
`RestUsersHandler`, sharing the same `canReadUser`/`canWriteUser`
primitives, would carry the identical new read-only-mode restriction. This
is now confirmed: every write route in this handler (`POST`/`PUT`/`PATCH`/
`DELETE`, including the create-user, replace-user, grant/revoke, and
config routes) now rejects with `403 TRI_ERROR_FORBIDDEN` while the server
runs `--server.read-only=true`, for **any** caller, including one editing
their own record — a restriction `devel` never had (its checks,
`isAdminUser()`/`canAccessUser()`, contain no read-only test), and, exactly
as reasoned for `RestAccessTokenHandler`, the actual underlying write
(`UserManagerImpl::storeUserInternal()`, reached via `updateUser()`/
`storeUser()`) deliberately runs under a forced `ExecContextSuperuserScope`
(`arangod/Auth/UserManagerImpl.cpp:377`), bypassing the storage engine's
own read-only gate — so in `devel`, user/permission management continues
to work even in read-only mode. This is the same, already-documented
Finding 3 from the `RestAccessTokenHandler` section, now confirmed to
apply identically here; it is not treated as a new, separate finding.

### `RestEdgesHandler` (`arangod/RestHandler/RestEdgesHandler.cpp`)

Clean — no authorization code in either branch; the diff is a single added
comment (`// Mounted at /_api/edges (prefix)`). Edge traversal relies
entirely on the standard collection-level transaction permission
machinery already fully analyzed for `RestDocumentHandler`.

### Summary

| Handler / Route | Verdict |
|---|---|
| `PUT /_api/query-cache`, `POST /_api/query-cache/properties` | No-op `requestedApiVersion()`-gated scaffolding (unreachable today) |
| `POST /_open/auth`, `/_open/auth/renew` | Confirmed equivalent to `devel`'s `forceSuperuser()` path exception (Finding 1) |
| `GET /_api/user`, `GET /_api/user/{user}` | Confirmed equivalent — admin/self gate unchanged in effect (Finding 2) |
| `PUT`/`PATCH /_api/user/{user}` (own profile), `*/config` routes | Unchanged, intentional self-service — not part of this session's findings |
| `PUT`/`DELETE /_api/user/{user}/database/{db}[/{coll}]` | **Critical regression**: strict admin-only check weakened to self-bypassing `canWriteUser()` — any user can grant themselves arbitrary access, including full admin (Finding 3) |
| All `POST`/`PUT`/`PATCH`/`DELETE /_api/user/*` routes, server in `--server.read-only` mode | **Regression** (safer direction): now rejected with `403`, `devel` allowed it (Finding 4, same root cause as `RestAccessTokenHandler` Finding 3) |
| `/_api/edges/*` | Identical to `devel` — no auth code |

**Action items / recommendations:** Finding 3 needs a prompt, deliberate
fix — remove the self-bypass from the `WriteUser`/grant-revoke code path
(e.g. have `RestUsersHandler`'s `PUT`/`DELETE .../database/...` branches
call a strict `can(WriteUser{name})`/`isAdmin()`-only check instead of
`exec.canWriteUser(name)`, restoring `devel`'s no-self-exception rule for
this specific operation only, while continuing to use `canWriteUser()`
with its self-bypass for the legitimate self-service routes). This is a
real, exploitable privilege-escalation bug and should be prioritized ahead
of the read-only-mode Finding 4, which — as with `RestAccessTokenHandler`
— is a policy decision rather than a security defect.


## `RestDocumentStateHandler`, `RestTasksHandler`, `RestTimeHandler`, `RestSimpleHandler`, `RestActionHandler`, `async_registry::RestHandler`, `activities::RestHandler`, `RestHotBackupHandler` (Enterprise)

This session covers the remaining, previously-unanalyzed handlers,
completing the full sweep described at the top of this document.

### `RestDocumentStateHandler` (`arangod/RestHandler/RestDocumentStateHandler.cpp`)

Mounted at `/_api/document-state` (prefix, only reachable when
replication2 is enabled and the server is in cluster mode). Internal-only
API for replication2 document-state snapshot transfer.

**Finding 1 (cosmetic)**: `devel`'s single, verb-independent
`ExecContext::current().isAdminUser()` check was split by verb
(`arangod/RestHandler/RestDocumentStateHandler.cpp:74-90`) into
`canUseAdminAction(AdminReadReplicatedLog{})` (GET) and
`canUseAdminAction(AdminWriteReplicatedLog{})` (POST/DELETE). Both
permission tags are members of the `AnyAdmin` catch-all
(`arangod/Auth/Permissions.h:102-118`), which `AuthMode::Classic::check()`
resolves via the same generic `isAdmin()` fallthrough already established
for `RestLogHandler`'s identical split (this document's
`RestQueryHandler`/`RestLogHandler`/`RestAdminExecuteHandler`/
`RestSystemReportHandler` session). No behavioral difference in Classic
mode today — confirmed to be, like that session, forward-looking RBAC
scaffolding rather than an active policy change.

### `RestTasksHandler` (`arangod/RestHandler/RestTasksHandler.cpp`)

Mounted at `/_api/tasks` (prefix, requires the V8 subsystem). Registers,
lists, and deletes periodic/one-off server-side V8 tasks. Notably, this
handler is already flagged in `Documentation/path_permissions.md:1034-1036`
with "NO FIX, tasks gone soon" — i.e. its imperfections are a known,
accepted, pre-existing condition, not something introduced by this
comparison.

**Finding 2 (confirmed no-op, refactor only)**: three related changes,
all traced to be behaviorally equivalent:

1. `registerTask()`/`deleteTask()`'s permission check
   (`arangod/RestHandler/RestTasksHandler.cpp:182-190,337-345`) changed
   from a bare `exec.databaseAuthLevel() != auth::Level::RW` comparison to
   `exec.canUseDatabase(_request->databaseName(), DatabaseAccessLevel::Write)`.
   This *does* newly add the standard read-only-mode short-circuit found
   in `ExecContext::canUseDatabase()` (`arangod/Utils/ExecContext.cpp:189-196`)
   — see Finding 3 below for the one genuine consequence of this.
2. The unused/dead `runAsUser` handling (`devel`'s `task->setUser(runAsUser)`
   call and the `Task::_user` member/`setUser()` method it fed) was
   removed. The code's own new comment
   (`arangod/RestHandler/RestTasksHandler.cpp:221-226`) documents why this
   is safe: the "run as a different user" option never actually worked in
   `devel` either — `runAsUser` could only ever end up equal to
   `exec.user()` (any mismatch triggered an immediate `403`), so
   `task->setUser(runAsUser)` was always setting the task's owner to the
   creator's own username, which is exactly what happens implicitly now.
3. `Task` (`arangod/VocBase/Methods/Tasks.h/.cpp`) was refactored from
   storing a bare `std::string _user` (re-resolved into a fresh
   `ExecContext::create(_user, dbname)` on every periodic tick,
   `/tmp/devel_Tasks.cpp:314-320`) to storing a full
   `std::shared_ptr<ExecContext const> _execContext` snapshot captured at
   creation time (`RestTasksHandler.cpp:293-298`;
   `Tasks.cpp:304-307,341-343`). I verified this is **not** a
   permission-freezing regression: `AuthMode::Classic::check()`
   (`arangod/Auth/AuthMode.cpp:91-106`) always performs a **live** lookup
   against the shared `auth::UserManager` singleton keyed by username
   string (`_userManager.databaseAuthLevel(username(), db, true)`) — it
   holds no cached grant data of its own. So calling
   `_execContext->canUseDatabase(...)` on the captured snapshot object
   produces exactly the same live, up-to-the-moment result as `devel`'s
   pattern of constructing a brand-new `ExecContext` each tick; only the
   superuser-vs-regular-user branch selection is now baked in at creation
   time (matching `devel`'s equivalent `_user.empty()` branch, which was
   also fixed at creation time). Confirmed no functional difference.

**Finding 3 (narrow regression, safer direction, low risk)**: the
read-only-mode gate newly bundled into `canUseDatabase()` (Finding 2,
point 1) has two real, if minor, consequences that `devel` did not have:
- `POST`/`PUT /_api/tasks[/{id}]` (registering a task) and
  `DELETE /_api/tasks/{id}` (deleting a task) now fail with `403
  TRI_ERROR_FORBIDDEN` while the server runs in `--server.read-only` mode,
  even for a fully RW-permitted caller. `devel` allowed both — this is the
  same recurring "helper bundles in a blanket read-only check" pattern
  already documented for `RestQueryPlanCacheHandler` (Finding 2),
  `RestUsersHandler` (Finding 4), and `RestAccessTokenHandler` (Finding 3),
  applied here to a purely in-memory, non-persisted resource (a scheduled
  V8 callback), so there is no storage-engine backstop making it a no-op.
- More subtly, `Task::callbackFunction()`'s **periodic** re-check
  (`arangod/VocBase/Methods/Tasks.cpp:304-307`) now also inherits this
  gate. If the server enters `--server.read-only` mode *after* a periodic
  task was already scheduled, the task will be silently unregistered on
  its very next tick (since `canUseDatabase(..., Write)` now unconditionally
  fails in read-only mode, regardless of the task owner's actual
  permissions) — whereas in `devel` the periodic task kept running
  (constrained only by the owner's actual database grant, exactly as
  before). Arguably this is more correct behavior (a periodic
  write-oriented task probably *should* pause during planned read-only
  maintenance windows), but it is a genuine, previously-undocumented
  behavioral change specific to this handler's background-task machinery,
  distinct from the simple "REST call now rejected" pattern seen
  elsewhere.

`RestTasksHandler.h` has no diff versus `devel`.

### `RestTimeHandler` and `RestSimpleHandler` — clean

- **`RestTimeHandler`** (`arangod/RestHandler/RestTimeHandler.cpp`, mounted
  at `/_admin/time`, exact) — no authorization code in either branch; diff
  is a single added comment.
- **`RestSimpleHandler`** (`arangod/RestHandler/RestSimpleHandler.cpp`,
  mounted at `/_api/simple/lookup-by-keys` and
  `/_api/simple/remove-by-keys`) — inherits from `RestCursorHandler` and
  delegates entirely to `registerQueryOrCursor()`, the same entry point
  fully analyzed in the `RestCursorHandler` session
  (`auth_comparison_with_devel.md:2217-2408`). Diff is a single added
  comment. Unlike `RestSimpleQueryHandler` (which only ever issues
  read-only queries), `remove-by-keys` builds a `REMOVE`-based AQL query —
  a genuine write — so it **does** fall under that session's
  already-documented read-only-mode `errorNum` divergence (generic
  `TRI_ERROR_FORBIDDEN` vs. `devel`'s `TRI_ERROR_ARANGO_READ_ONLY`, both
  HTTP 403); this is an addendum confirming applicability, not a new
  finding. Neither route ever looks up an existing cursor by ID, so the
  `RestCursorHandler` Finding 1 (cursor-ownership bypass under disabled
  authentication) does not apply here.

### `RestActionHandler` (`arangod/Actions/RestActionHandler.cpp`)

Mounted at `/` (prefix, catch-all for legacy/Foxx actions and the web UI).

**Finding 4 (confirmed equivalent)**: a new `checkUserCanAccess()`
override (`arangod/Actions/RestActionHandler.cpp:115-121`) unconditionally
allows any request whose path starts with `/_admin/aardvark/`. This
faithfully reproduces `devel`'s `CommTask`-level path exception
(`/tmp/devel_CommTask.cpp:850`: `path.starts_with(pathPrefixAdminAardvark)`
→ `Flow::Continue` + `forceSuperuser()`), and is the same
relocated-from-`CommTask`-into-per-handler-override pattern already
established for `RestAuthHandler`'s `/_open/` exemption and
`RestAccessTokenHandler`'s `/_api/token/` exemption. No other diff besides
one added comment.

### `async_registry::RestHandler` and `activities::RestHandler` — cosmetic only

- **`async_registry::RestHandler`**
  (`arangod/SystemMonitor/AsyncRegistry/RestHandler.cpp:36-43`, mounted at
  `/_admin/async-registry`) — `isAdminUser()` →
  `canUseAdminAction(AdminMonitoringInternal{})`, the same
  already-established `AnyAdmin`-catch-all-equivalent migration seen
  repeatedly (`RestVersionHandler`, `RestSystemReportHandler`, etc.).
- **`activities::RestHandler`**
  (`arangod/SystemMonitor/Activities/RestHandler.cpp:118-133`, mounted at
  `/_admin/activities`) — two changes, both already-established
  equivalents: `isSuperuser()` → `isSuperuserOrDisabled()` (the
  auth-disabled-only widening proven safe multiple times in this
  document), and `isAdminUser()` →
  `canUseAdminAction(AdminMonitoringInternal{})` (same as above).

Both `.h` files have no diff versus `devel`.

### `RestHotBackupHandler` (Enterprise, `enterprise/Enterprise/RestHandler/RestHotBackupHandler.cpp`)

Mounted at `/_admin/backup` (prefix, Enterprise-only). Diffed directly
against the enterprise submodule's own `devel` branch (not the community
`devel`).

**Finding 5 (confirmed equivalent)**:
`verifyPermitted()` (`enterprise/Enterprise/RestHandler/RestHotBackupHandler.cpp:130-149`)
carries the same two already-established equivalent migrations seen for
every other superuser/admin-gated handler in this document:
`isSuperuser()` → `isSuperuserOrDisabled()`, and `isAdminUser()` →
`canUseAdminAction(AdminBackup{})` (`AdminBackup` confirmed to be a member
of the `AnyAdmin` catch-all, `arangod/Auth/Permissions.h:101,116`). No
other diff besides two added `@author` comment lines (moved, not removed)
and one mounting comment.

This handler was cross-checked against `RestAdminServerHandlerEE`
(already fully covered at `auth_comparison_with_devel.md:1628-1644`) and
`RestLicenseHandlerEE` (already covered at
`auth_comparison_with_devel.md:5431`) — both confirmed to already be
present in this document from prior sessions; no further action needed
for either.

### Summary

| Handler / Route | Verdict |
|---|---|
| `GET`/`POST`/`DELETE /_api/document-state` | Cosmetic verb-split admin check (Finding 1) |
| `POST`/`PUT /_api/tasks[/{id}]`, `DELETE /_api/tasks/{id}` | Confirmed-equivalent refactor (Finding 2); new read-only-mode rejection, both for the REST call and for already-running periodic tasks (Finding 3) |
| `GET /_api/tasks`, `GET /_api/tasks/{id}` | Unchanged (`isSuperuser`/self-check untouched by this diff) |
| `GET /_admin/time` | Clean, no auth code |
| `PUT /_api/simple/lookup-by-keys` | Clean — delegates to already-analyzed `RestCursorHandler` read path |
| `PUT /_api/simple/remove-by-keys` | Delegates to `RestCursorHandler`'s already-documented write-path read-only-mode divergence (addendum, not new) |
| `/` (catch-all actions, incl. `/_admin/aardvark/*`) | Confirmed equivalent to `devel`'s `CommTask` path exception (Finding 4) |
| `GET /_admin/async-registry`, `GET /_admin/activities` | Cosmetic `AnyAdmin`/`isSuperuserOrDisabled()` migrations, no behavioral change |
| `POST /_admin/backup/*` (Enterprise) | Cosmetic, same established migrations (Finding 5) |

**Action items / recommendations:** only Finding 3
(`RestTasksHandler`'s read-only-mode interaction) merits a decision: if
periodic V8 tasks pausing during `--server.read-only` mode is unintended,
`Task::callbackFunction()`'s permission re-check
(`arangod/VocBase/Methods/Tasks.cpp:304-307`) should bypass the
read-only-mode component of `canUseDatabase()` while keeping the
permission-level check itself. Given this handler's documented
deprecation ("tasks gone soon"), this is low priority. No other action
items from this session.

# Overall Summary

This concludes the full handler-by-handler sweep. The sections below
collect the results across all sessions into two views: which handlers
are done and need no further attention, and which concrete items still
need a decision or a code fix (cosmetic-only, dead-code, and
"verified-equivalent" findings are omitted from both, since by
definition they require no action).

## Handlers confirmed to need no further work

For every handler/route listed below, all differences found against
`devel` were either non-existent, purely cosmetic (message text/error-code
wording only, same ALLOW/DENY decision), confirmed no-ops, or — where a
real behavioural difference existed — already assessed in its own section
as not requiring any code change. No open action item remains for any of
these:

- `RestMetricsHandler`
- `RestCompactHandler`
- `RestAdminServerHandler` (incl. Enterprise-only `RestAdminServerHandlerEE`)
- `RestOptions*` handler family (e.g. `RestPublicOptionsHandler`)
- `RestAqlHandler`
- `RestClusterHandler`
- `RestAgencyCallbacksHandler`
- `RestImportHandler`
- `RestUsageMetricsHandler`
- `RestEngineHandler`
- `RestSupportInfoHandler`
- `RestAqlFunctionsHandler`
- `RestEndpointHandler`
- `RestReplicationHandler`, `RocksDBRestReplicationHandler`,
  `ClusterRestReplicationHandler` (see note below — real, low-risk
  behavioural differences exist here, but the session concluded no code
  change is warranted, only optional release-note mentions)
- `MaintenanceRestHandler`
- `RestSimpleQueryHandler`
- `RestAuthReloadHandler`
- `RestDebugHandler`
- `RestStatusHandler`
- `RestAdminLogHandler`
- `RestAdminRoutingHandler`
- `RestUploadHandler`
- `RestJobHandler`
- `RestAdminDatabaseHandler`
- `RestLogInternalHandler`
- `RestAdminStatisticsHandler`
- `RestVersionHandler`
- `RestAdminDeploymentHandler`
- `RestDumpHandler`
- `RestSupervisionStateHandler`
- `RestTransactionHandler`
- `RestTtlHandler`
- `RestOpenApiHandler`
- `RestViewHandler`
- `RestKeyGeneratorsHandler`
- `RestShutdownHandler`
- `RestLicenseHandler` (incl. Enterprise-only `RestLicenseHandlerEE`)
- `RestExplainHandler`
- `RestAqlUserFunctionsHandler`
- `RestIndexHandler` (its one new check is deliberately gated/dormant; no
  immediate action needed)
- `RestQueryHandler`
- `RestLogHandler`
- `RestAdminExecuteHandler`
- `RestSystemReportHandler`
- `RestQueryCacheHandler`
- `RestAuthHandler`
- `RestEdgesHandler`
- `RestDocumentStateHandler`
- `RestTimeHandler`
- `RestSimpleHandler`
- `RestActionHandler`
- `async_registry::RestHandler`
- `activities::RestHandler`
- `RestHotBackupHandler` (Enterprise)

## Urgent / outstanding items

The table below lists every remaining finding across the whole document
that still calls for either a concrete code fix or a deliberate policy
decision. Purely cosmetic findings, confirmed no-ops, and findings whose
own section already concluded "no action required" are excluded.

| Severity | Type of issue | `RestHandler`(s) | What needs doing |
|---|---|---|---|
| **Critical** | Security vulnerability (privilege escalation, independent of RBAC) | `RestUsersHandler` | `PUT`/`DELETE /_api/user/{user}/database/{db}[/{coll}]` (grant/revoke access) now goes through the self-bypassing `ExecContext::canWriteUser()` (`arangod/Utils/ExecContext.cpp:452-462`) instead of `devel`'s strict, non-bypassable admin-only check. Any authenticated user can grant themselves `RW` on `_system` (`arangod/RestHandler/RestUsersHandler.cpp:410-422,593-601`) — i.e. become a full admin — with one API call. **Fix:** use a strict `can(WriteUser{name})`/admin-only check (no self-exception) for these two routes specifically; keep `canWriteUser()`'s self-bypass for the legitimate self-service routes (password/config). |
| **Critical** | Security vulnerability (path traversal, independent of RBAC) | `RestCrashHandler` | The handler-level `DumpManager::isValidCrashId(crashId)` UUID check was removed with no replacement (`arangod/RestHandler/RestCrashHandler.cpp:64-79`); `lib/CrashHandler/DumpManager.cpp:67-106` now builds a filesystem path directly from the raw, client-supplied suffix in `getCrashContents()`/`deleteCrash()`. An already-admin-authorized caller can supply `..` to read/delete arbitrary files/directories outside the crashes directory. **Fix:** restore UUID validation inside `DumpManager` itself (not only in the REST layer), as `devel` did via `resolveCrashDirectory()`. |
| **High** | Regression — missing admin bypass | `RestAnalyzerHandler` | `AuthMode::Classic::check()`'s `UseAnalyzer` branch (`arangod/Auth/AuthMode.cpp:348-365`) lacks the `isAdmin()` bypass that `devel`'s `IResearchAnalyzerFeature::canUse()` had. A `_system` admin without explicit access to a given database can no longer create/read/remove analyzers there. **Fix:** add `if (isAdmin().ok()) return {};` to that branch. |
| **High** | Regression — missing superuser escalation | `RestWalAccessHandler` | `handleCommandTail()` (`arangod/RestHandler/RestWalAccessHandler.cpp:274`) lost the `ExecContextSuperuserScope` bypass `devel` wrapped around `wal->tail(...)`, so collection-loading permission checks reached during tailing can now fail where they previously didn't. **Fix:** reinstate an equivalent bypass (e.g. via `arangod/Utils/ExecContext.h:272-287`, or a `canDumpCollection()`-style admin bypass, `arangod/Utils/ExecContext.h:169-174`). |
| **High** | Regression — missing authentication check | `RestDatabaseHandler` | `getDatabases()`'s `user` suffix route (`arangod/RestHandler/RestDatabaseHandler.cpp:68`) lost the explicit authentication check `devel` performed centrally in `CommTask::canAccessPath()` before this logic was moved into `RestHandler::checkUserCanAccess()`. **Fix:** restore an explicit authentication check for `GET /_api/database/user`. |
| **High** | Regression — existence leak / wrong error code | `RestCollectionHandler` | (a) `GET /_api/collection` listing uses `canSeeCollection()` (`arangod/RestHandler/RestCollectionHandler.cpp:124`), which always succeeds in Classic mode, leaking the existence of collections a caller should not even see; (b) `DELETE /_api/collection/<name>` on a non-existent collection now returns `403` instead of `404` because the `canDropCollection()` pre-check (`arangod/RestHandler/RestCollectionHandler.cpp:726`) runs before the existence check. **Fix:** make `SeeCollection` a real level-based check (or call `canUseCollection(..., Read)` for the listing instead), and move the drop permission check to after the existence lookup. |
| **High** | Regression — hardening bypass | `RestAdminClusterHandler` | `handleNumberOfServers()`'s admin/hardening gate (`arangod/RestHandler/RestAdminClusterHandler.cpp:2089-2100`) only runs for non-`GET` requests, so `GET /_admin/cluster/numberOfServers` is no longer gated by `--server.harden` — any authenticated user can now read cluster server counts on hardened installations. **Fix:** move the `canUseHardenedAction(AdminMaintenance{})` check outside the `requestType() != GET` guard. |
| **Medium** | Regression — wrong error code (cross-cutting) | `RestDocumentHandler` (and everything sharing `ExecContext::canUseCollection`/`canUseDatabase`: `RestImportHandler`, `RestCollectionHandler`'s `truncate`, `RestSimpleHandler`'s `remove-by-keys`, `RestTransactionHandler`, `RestGraphHandler` writes, etc.) | `canUseCollection()` (`arangod/Utils/ExecContext.cpp:223`) and `canUseDatabase()` (`arangod/Utils/ExecContext.cpp:189`) unconditionally return `TRI_ERROR_FORBIDDEN` instead of `devel`'s `TRI_ERROR_ARANGO_READ_ONLY` for an otherwise-permitted write while `--server.read-only` is set. HTTP status (`403`) is unaffected, but `errorNum` differs, which could break clients/tests branching on it. **Fix:** have these helpers only *cap* an effective `RW` grant down to `RO` in read-only mode and let `AuthMode::Classic::check()`'s normal level comparison produce the correct error. |
| **Medium** | Regression — more permissive | `RestGraphHandler` | `GraphManager::createGraph()`'s Classic-mode check (`arangod/Auth/AuthMode.cpp:504-528`) no longer re-checks per-collection read access once database-level `RW` is already confirmed. A caller holding an explicit deny override on a referenced, already-existing collection can now create a graph referencing it, where `devel` would reject it. **Needs a decision:** accept as intentional, or restore the unconditional `collectionNamesToRead` loop. |
| **Medium** | Policy decision — read-only mode blocks operations `devel` allowed | `RestAccessTokenHandler`, `RestUsersHandler`, `RestQueryPlanCacheHandler`, `RestTasksHandler` | The new/refactored `canWriteUser()` (`arangod/Utils/ExecContext.cpp:452-462`) and generic `canUseDatabase()` (`arangod/Utils/ExecContext.cpp:189`) helpers bundle a read-only-mode gate that `devel`'s equivalent checks never had. As a result, token/user management (whose actual write runs under a forced superuser scope and bypasses the storage engine's own read-only check in `devel`), clearing the in-memory query-plan cache, and registering/running periodic V8 tasks are all now rejected during `--server.read-only` mode. **Needs one consistent, deliberate decision** across all four handlers: keep as intentional hardening, or exempt these non-persistent/superuser-backed operations from the read-only gate. |
| **Low** | Narrow regression, safer direction (release-note only) | `RestCollectionHandler` (`RocksDBRestCollectionHandler`'s `recalculateCount`), `RestAdminClusterHandler` (`moveShard`'s collection-level fallback) | Both now additionally require database-level `RW` alongside the collection-level grant ("container principle"), so a narrow permission combination (collection-level `RW` override + database-level `RO`) that worked in `devel` now gets `403`. No fix proposed; worth a release-note mention only. |
| **Low** | Narrow regression (release-note only) | `RestReplicationHandler` / `RocksDBRestReplicationHandler` | `restore-collection?overwrite=true` on an existing collection now also enforces that collection's own access override (stricter than `devel`); `handleCommandInventory()`'s single-collection branch lost an admin-bypass escalation (`devel`'s `ExecContextSuperuserScope`), relevant only to internal shard-sync callers. No fix proposed; release-note mention only. |
