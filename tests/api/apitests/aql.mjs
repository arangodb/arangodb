// Tests for the /_api/aql-builtin endpoint.
//
// Handler: RestAqlFunctionsHandler (RestVocbaseBaseHandler)
// Mounted at: /_api/aql-builtin (prefix)
//
// RestAqlFunctionsHandler extends RestVocbaseBaseHandler, which requires a
// database context.  Requests without an explicit /_db/<name>/ prefix are
// handled in the _system database context.  The general routing layer
// therefore enforces _system DB access before the handler runs:
//
//   AU (_sys undef) → 401   No _system grant; rejected before handler.
//   AN (_sys none)  → 401   Explicitly denied; rejected before handler.
//
// The handler itself has no additional authorization check: it simply
// serializes the list of built-in AQL functions and returns it with HTTP 200.
//
// Auth: AUTHEN (no in-handler permission check beyond the DB-context check)
// Expected: AU→401, AN→401, AR→200, AW→200, SU→200

export default [

  // ── GET /_api/aql-builtin ─────────────────────────────────────────────────
  // Returns a JSON object with a "functions" array listing all built-in AQL
  // functions together with their signatures and descriptions.
  // Safe, read-only, no side effects.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "List built-in AQL functions (GET /_api/aql-builtin)",
    type: "admin",
    method: "GET",
    path: "/_api/aql-builtin",
  },

];
