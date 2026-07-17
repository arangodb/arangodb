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
   for all helper/authorization code it depends on.
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
