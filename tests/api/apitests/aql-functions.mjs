// Tests for the /_api/aqlfunction endpoint.
//
// Handler: RestAqlUserFunctionsHandler (RestVocbaseBaseHandler)
// Mounted at: /_api/aqlfunction (prefix, requires USE_V8)
//
// When V8 is not compiled in, all routes return 404.
//
// Auth model (RestVocbaseBaseHandler + internal collection-level check):
//   Requests without a /_db/<name>/ prefix are dispatched in the _system
//   database context.  The routing layer enforces at least some DB access
//   before the handler is reached:
//
//   AU (_sys undef) → 401  No _system grant; rejected at the routing layer.
//   AN (_sys none)  → 401  Explicitly denied; rejected at the routing layer.
//   AR (_sys ro)    → GET: 200  (can read _aqlfunctions)
//                  → POST / DELETE: 403  (need rw on _aqlfunctions)
//   AW (_sys rw)    → GET / POST / DELETE: 200 / 201
//   SU              → same as AW
//
//   When accessed via /_db/d/... the same logic applies to database d:
//   UUU (db undef)  → 401
//   NUU (db none)   → 401
//   RUU (db ro)     → GET: 200  /  POST / DELETE: 403
//   WUU (db rw)     → GET / POST / DELETE: 200 / 201

const TEST_FN_NAME = "APITESTNS::APITESTFUNC";
const TEST_FN_CODE = "function (a, b) { return a + b; }";

export default [

  // ── GET /_db/d/_api/aqlfunction ──────────────────────────────────────────
  // Lists all user-defined AQL functions in database d.
  // Expected: UUU→401, NUU→401, RUU→200, WUU→200
  {
    name: "List user-defined AQL functions in database d (GET /_db/d/_api/aqlfunction)",
    type: "database",
    method: "GET",
    path: "/_db/d/_api/aqlfunction",
  },

  // ── GET /_db/d/_api/aqlfunction/{namespace} ──────────────────────────────
  // Lists user-defined AQL functions filtered by namespace in database d.
  // Expected: UUU→401, NUU→401, RUU→200, WUU→200
  {
    name: "List user-defined AQL functions by namespace in database d (GET /_db/d/_api/aqlfunction/APITESTNS)",
    type: "database",
    method: "GET",
    path: "/_db/d/_api/aqlfunction/APITESTNS",
  },

  // ── POST /_db/d/_api/aqlfunction ─────────────────────────────────────────
  // Creates or replaces a user-defined AQL function in database d.
  // Requires rw access to the _aqlfunctions system collection in d.
  // Expected: UUU→401, NUU→401, RUU→403, WUU→201
  {
    name: "Create user-defined AQL function in database d (POST /_db/d/_api/aqlfunction)",
    type: "database",
    method: "POST",
    path: "/_db/d/_api/aqlfunction",
    body: { name: TEST_FN_NAME, code: TEST_FN_CODE, isDeterministic: true },
    setup: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/aqlfunction/${TEST_FN_NAME}`);
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/aqlfunction/${TEST_FN_NAME}`);
    },
  },

  // ── DELETE /_db/d/_api/aqlfunction/{name} ────────────────────────────────
  // Deletes a user-defined AQL function from database d.
  // Requires rw access to the _aqlfunctions system collection in d.
  // Expected: UUU→401, NUU→401, RUU→403, WUU→200
  {
    name: "Delete user-defined AQL function from database d (DELETE /_db/d/_api/aqlfunction/{name})",
    type: "database",
    method: "DELETE",
    path: `/_db/d/_api/aqlfunction/${TEST_FN_NAME}`,
    setup: async (ctx) => {
      await ctx.request('POST', '/_db/d/_api/aqlfunction',
        { name: TEST_FN_NAME, code: TEST_FN_CODE, isDeterministic: true });
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/aqlfunction/${TEST_FN_NAME}`);
    },
  },

  // ── GET /_api/aqlfunction ─────────────────────────────────────────────────
  // Lists all user-defined AQL functions registered in the _system database.
  // Read-only, no side effects.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "List user-defined AQL functions in _system (GET /_api/aqlfunction)",
    type: "admin",
    method: "GET",
    path: "/_api/aqlfunction",
  },

  // ── GET /_api/aqlfunction/{namespace} ────────────────────────────────────
  // Lists user-defined AQL functions filtered by namespace in _system.
  // Read-only, no side effects.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "List user-defined AQL functions by namespace in _system (GET /_api/aqlfunction/APITESTNS)",
    type: "admin",
    method: "GET",
    path: "/_api/aqlfunction/APITESTNS",
  },

  // ── POST /_api/aqlfunction ────────────────────────────────────────────────
  // Creates or replaces a user-defined AQL function in the _system database.
  // Requires rw access to the _aqlfunctions system collection.
  // Expected: AU→401, AN→401, AR→403, AW→201, SU→201
  {
    name: "Create user-defined AQL function in _system (POST /_api/aqlfunction)",
    type: "admin",
    method: "POST",
    path: "/_api/aqlfunction",
    body: { name: TEST_FN_NAME, code: TEST_FN_CODE, isDeterministic: true },
    setup: async (ctx) => {
      // Remove any stale function so each user cell starts clean.
      await ctx.request('DELETE', `/_api/aqlfunction/${TEST_FN_NAME}`);
    },
    teardown: async (ctx) => {
      // Clean up whether or not the test user succeeded.
      await ctx.request('DELETE', `/_api/aqlfunction/${TEST_FN_NAME}`);
    },
  },

  // ── DELETE /_api/aqlfunction/{name} ──────────────────────────────────────
  // Deletes a user-defined AQL function from the _system database.
  // Requires rw access to the _aqlfunctions system collection.
  // Expected: AU→401, AN→401, AR→403, AW→200, SU→200
  {
    name: "Delete user-defined AQL function from _system (DELETE /_api/aqlfunction/{name})",
    type: "admin",
    method: "DELETE",
    path: `/_api/aqlfunction/${TEST_FN_NAME}`,
    setup: async (ctx) => {
      // Ensure the function exists before the test user tries to delete it.
      await ctx.request('POST', '/_api/aqlfunction',
        { name: TEST_FN_NAME, code: TEST_FN_CODE, isDeterministic: true });
    },
    teardown: async (ctx) => {
      // Remove the function if the test user lacked permission to do so.
      await ctx.request('DELETE', `/_api/aqlfunction/${TEST_FN_NAME}`);
    },
  },

];
