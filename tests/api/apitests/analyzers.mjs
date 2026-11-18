// Tests for the /_api/analyzer endpoint.
//
// Handler: RestAnalyzerHandler (RestVocbaseBaseHandler, iresearch namespace)
// Mounted at: /_api/analyzer (prefix)
//
// When the iresearch/search feature is not compiled in, all routes return 404.
//
// Auth model (RestVocbaseBaseHandler + IResearchAnalyzerFeature::canUse):
//   Requests without a /_db/<name>/ prefix are dispatched in the _system
//   database context.  The routing layer enforces at least some DB access
//   before the handler is reached:
//
//   AU (_sys undef) → 401  No _system grant; rejected at the routing layer.
//   AN (_sys none)  → 401  Explicitly denied; rejected at the routing layer.
//   AR (_sys ro)    → GET list: 200  (static + system-db analyzers if readable)
//                  → GET by name: 200  (built-in / accessible analyzer)
//                  → POST / DELETE: 403  (canUse WriteData fails for ro user)
//   AW (_sys rw)    → GET / POST / DELETE: 200 / 201
//   SU              → same as AW
//
//   When accessed via /_db/d/... the same logic applies to database d:
//   UUU (db undef)  → 401
//   NUU (db none)   → 401
//   RUU (db ro)     → GET: 200  /  POST / DELETE: 403
//   WUU (db rw)     → GET / POST / DELETE: 200 / 201

// Name used for setup/teardown. We keep it simple; the handler normalizes it
// to the fully-qualified form <dbname>::apitest_analyzer internally.
const TEST_ANALYZER_NAME = "apitest_analyzer";

export default [

  // ── GET /_db/d/_api/analyzer ─────────────────────────────────────────────
  // Lists analyzers visible from database d (built-ins + d's own analyzers,
  // and _system analyzers if accessible).
  // Expected: UUU→401, NUU→401, RUU→200, WUU→200
  {
    name: "List analyzers in database d (GET /_db/d/_api/analyzer)",
    type: "database",
    method: "GET",
    path: "/_db/d/_api/analyzer",
  },

  // ── GET /_db/d/_api/analyzer/{name} ──────────────────────────────────────
  // Gets the built-in 'identity' analyzer from the context of database d.
  // Expected: UUU→401, NUU→401, RUU→200, WUU→200
  {
    name: "Get built-in 'identity' analyzer from database d (GET /_db/d/_api/analyzer/identity)",
    type: "database",
    method: "GET",
    path: "/_db/d/_api/analyzer/identity",
  },

  // ── POST /_db/d/_api/analyzer ────────────────────────────────────────────
  // Creates a user-defined analyzer in database d.
  // Requires WriteData access to the _analyzers system collection in d.
  // Expected: UUU→401, NUU→401, RUU→403, WUU→201
  {
    name: "Create analyzer in database d (POST /_db/d/_api/analyzer)",
    type: "database",
    method: "POST",
    path: "/_db/d/_api/analyzer",
    body: { name: TEST_ANALYZER_NAME, type: "identity" },
    setup: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/analyzer/${TEST_ANALYZER_NAME}`);
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/analyzer/${TEST_ANALYZER_NAME}`);
    },
  },

  // ── DELETE /_db/d/_api/analyzer/{name} ───────────────────────────────────
  // Deletes a user-defined analyzer from database d.
  // Requires WriteData access to the _analyzers system collection in d.
  // Expected: UUU→401, NUU→401, RUU→403, WUU→200
  {
    name: "Delete analyzer from database d (DELETE /_db/d/_api/analyzer/{name})",
    type: "database",
    method: "DELETE",
    path: `/_db/d/_api/analyzer/${TEST_ANALYZER_NAME}`,
    setup: async (ctx) => {
      await ctx.request('POST', '/_db/d/_api/analyzer',
        { name: TEST_ANALYZER_NAME, type: "identity" });
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/analyzer/${TEST_ANALYZER_NAME}`);
    },
  },

  // ── GET /_api/analyzer ───────────────────────────────────────────────────
  // Lists all analyzers visible from the _system database context
  // (static built-ins + user-defined analyzers for _system).
  // The handler always returns 200; if the user lacks read on the vocbase,
  // the vocbase's own analyzers are simply omitted from the result.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "List analyzers in _system (GET /_api/analyzer)",
    type: "admin",
    method: "GET",
    path: "/_api/analyzer",
  },

  // ── GET /_api/analyzer/{name} ─────────────────────────────────────────────
  // Gets the definition of the built-in 'identity' analyzer.
  // Built-in analyzers are globally accessible; canUse returns true for any
  // authenticated user with at least ro access to the current database.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Get built-in 'identity' analyzer (GET /_api/analyzer/identity)",
    type: "admin",
    method: "GET",
    path: "/_api/analyzer/identity",
  },

  // ── POST /_api/analyzer ──────────────────────────────────────────────────
  // Creates a user-defined analyzer in the _system database.
  // Requires WriteData access to the _analyzers system collection.
  // Expected: AU→401, AN→401, AR→403, AW→201, SU→201
  {
    name: "Create analyzer in _system (POST /_api/analyzer)",
    type: "admin",
    method: "POST",
    path: "/_api/analyzer",
    body: { name: TEST_ANALYZER_NAME, type: "identity" },
    setup: async (ctx) => {
      // Remove any stale analyzer so each user cell starts clean.
      await ctx.request('DELETE', `/_api/analyzer/${TEST_ANALYZER_NAME}`);
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE', `/_api/analyzer/${TEST_ANALYZER_NAME}`);
    },
  },

  // ── DELETE /_api/analyzer/{name} ─────────────────────────────────────────
  // Deletes a user-defined analyzer from the _system database.
  // Requires WriteData access to the _analyzers system collection.
  // Expected: AU→401, AN→401, AR→403, AW→200, SU→200
  {
    name: "Delete analyzer from _system (DELETE /_api/analyzer/{name})",
    type: "admin",
    method: "DELETE",
    path: `/_api/analyzer/${TEST_ANALYZER_NAME}`,
    setup: async (ctx) => {
      // Ensure the analyzer exists before the test user tries to delete it.
      await ctx.request('POST', '/_api/analyzer',
        { name: TEST_ANALYZER_NAME, type: "identity" });
    },
    teardown: async (ctx) => {
      // Remove the analyzer if the test user lacked permission to do so.
      await ctx.request('DELETE', `/_api/analyzer/${TEST_ANALYZER_NAME}`);
    },
  },

];
