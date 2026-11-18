// Tests for the /_admin/log/* endpoints.
//
// Handler: RestAdminLogHandler (prefix /_admin/log)
//
// The log API has a two-level guard (see RestAdminLogHandler::verifyPermitted):
//
//   1. isAPIEnabled()  – controlled by --log.api-enabled (default: true).
//      If the API is disabled the handler immediately returns 403 for every
//      user, including the superuser.
//
//   2. onlySuperUser() – true when --log.api-jwt-policy == "jwt" (default:
//      "true", i.e. NOT superuser-only).
//      In "jwt" mode only a superuser token passes; all named users get 403.
//
//   3. In the default configuration ("true" / admin mode):
//        GET requests   → canUseAdminAction(AdminReadLogs)
//        non-GET        → canUseAdminAction(AdminSetLogLevel)
//      Without RBAC, canUseAdminAction maps to RW access on the _system
//      database, so only AW and superuser pass.
//
// Table annotation: `?/S/A`
//   `?` = API disabled (--log.api-enabled=false)  → 403 for everyone
//   `S` = superuser-only (--log.api-jwt-policy=jwt) → only superuser passes
//   `A` = admin mode (default)                     → AW + superuser pass
//
// All tests below assume the default configuration:
//   --log.api-enabled=true, --log.api-jwt-policy=true (i.e. "admin" mode)
//
// Expected for GET (AdminReadLogs):
//   AU (_sys undef) → 401
//   AN (_sys none)  → 401
//   AR (_sys ro)    → 403
//   AW (_sys rw)    → 200
//   superuser       → 200
//
// Expected for PUT / DELETE (AdminSetLogLevel):
//   AU (_sys undef) → 401
//   AN (_sys none)  → 401
//   AR (_sys ro)    → 403
//   AW (_sys rw)    → 200
//   superuser       → 200

export default [

  // ── GET /_admin/log ──────────────────────────────────────────────────────
  // Returns buffered log messages in the legacy array format.  Pure read.
  {
    name: "Get buffered log messages, legacy format (GET /_admin/log)",
    type: "admin",
    method: "GET",
    path: "/_admin/log",
  },

  // ── GET /_admin/log/entries ──────────────────────────────────────────────
  // Returns buffered log messages in the newer object-per-entry format.
  {
    name: "Get buffered log messages, new format (GET /_admin/log/entries)",
    type: "admin",
    method: "GET",
    path: "/_admin/log/entries",
  },

  // ── GET /_admin/log/level ────────────────────────────────────────────────
  // Returns the current per-topic log levels as a JSON object.  Pure read.
  {
    name: "Get current log levels (GET /_admin/log/level)",
    type: "admin",
    method: "GET",
    path: "/_admin/log/level",
  },

  // ── GET /_admin/log/structured ───────────────────────────────────────────
  // Returns the currently active structured-logging parameters.  Pure read.
  {
    name: "Get structured-log parameters (GET /_admin/log/structured)",
    type: "admin",
    method: "GET",
    path: "/_admin/log/structured",
  },

  // ── PUT /_admin/log/level ────────────────────────────────────────────────
  // Sets per-topic log levels.  Sending an empty object {} is a valid
  // no-op request: the handler deserialises it as a LogLevels config with
  // no fields set, applies nothing, and returns the unchanged levels.
  // Auth: AdminSetLogLevel
  {
    name: "Set log levels, no-op empty body (PUT /_admin/log/level)",
    type: "admin",
    method: "PUT",
    path: "/_admin/log/level",
    body: {},
  },

  // ── PUT /_admin/log/structured ───────────────────────────────────────────
  // Sets structured-logging parameters.  Sending {} leaves all parameters
  // at their current values (no boolean values → nothing toggled).
  // Auth: AdminSetLogLevel
  {
    name: "Set structured-log parameters, no-op body (PUT /_admin/log/structured)",
    type: "admin",
    method: "PUT",
    path: "/_admin/log/structured",
    body: {},
  },

  // ── DELETE /_admin/log ───────────────────────────────────────────────────
  // Clears the in-memory log buffer.  This is a harmless side-effect in a
  // test environment.  Auth: AdminSetLogLevel
  {
    name: "Clear log buffer (DELETE /_admin/log)",
    type: "admin",
    method: "DELETE",
    path: "/_admin/log",
  },

  // ── DELETE /_admin/log/entries ───────────────────────────────────────────
  // Alias for DELETE /_admin/log — also clears the in-memory log buffer.
  // Auth: AdminSetLogLevel
  {
    name: "Clear log buffer via /entries suffix (DELETE /_admin/log/entries)",
    type: "admin",
    method: "DELETE",
    path: "/_admin/log/entries",
  },

  // ── DELETE /_admin/log/level ─────────────────────────────────────────────
  // Resets all per-topic log levels back to their compiled-in defaults.
  // Potentially disruptive to other logging-sensitive tests but acceptable
  // in a dedicated authorization-test environment.
  // Auth: AdminSetLogLevel
  {
    name: "Reset log levels to defaults (DELETE /_admin/log/level)",
    type: "admin",
    method: "DELETE",
    path: "/_admin/log/level",
  },

];
