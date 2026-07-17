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
truncate|properties|rename|loadIndexesIntoMemory}`,
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

### Summary for `RestCollectionHandler`

| Route | Verdict |
|---|---|
| `GET /_api/collection` (list all) | **Regression** (Finding 1): `canSeeCollection` always succeeds in `Classic` mode, so per-collection deny grants and the hard-coded `_users` hiding rule from `devel` are no longer honored in the listing (existence-only leak; actual read/write remains correctly denied elsewhere) |
| `GET /_api/collection/<name>[/...]` | Identical to `devel` (goes through `methods::Collections::lookup`, which still uses the real, level-based `canUseCollection`) |
| `POST /_api/collection` (create) | Identical to `devel` (Finding 4) |
| `PUT .../{load,unload,truncate,properties,rename,loadIndexesIntoMemory,responsibleShard}` | Identical to `devel` (Finding 4) |
| `PUT .../compact` | Identical to `devel` for the classic route; a new `WriteMeta` check exists but is gated behind the unrelated API-versioning scheme (Finding 3) |
| `DELETE /_api/collection/<name>` | **Regression** (Finding 2): returns `403 FORBIDDEN` instead of `404 NOT_FOUND` for a *non-existent* collection when the caller lacks write access, due to a new up-front `canDropCollection` check that runs before the existence check |

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
