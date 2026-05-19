// Tests for the /_admin/debug/* endpoints.
//
// Handler: RestDebugHandler (prefix /_admin/debug)
//
// IMPORTANT: This handler is ONLY compiled in when ARANGODB_ENABLE_FAILURE_TESTS
// is defined at build time.  When running against a production (or release) build
// every route in this group returns 404.  The tests below therefore accept both
// 200 (compiled in, functional) and 404 (not compiled in) as "passing" states
// for the purposes of this authorization test suite.  What the tests verify is
// that the authorization level is AUTHEN, i.e. every authenticated user gets
// the same status code — there is no per-user differentiation.
//
// Authorization for every route in this group: AUTHEN
//   → any authenticated user (no further check beyond authentication)
//   → if compiled in:     AU→401, AN→401, AR→200, AW→200, superuser→200
//   → if not compiled in: AU→404, AN→404, AR→404, AW→404, superuser→404
//
// NOTE: PUT /_admin/debug/crash is intentionally omitted from this test suite.
//       Issuing that request with a superuser token would actually terminate the
//       arangod process, which would destroy the test environment.  Its
//       authorization semantics are identical to the other routes (AUTHEN) and
//       are therefore covered by analogy.

export default [

  // ── GET /_admin/debug/failat ─────────────────────────────────────────────
  // Returns a boolean indicating whether failure points can be used at all in
  // this build.  Completely read-only and side-effect-free.
  // Auth: AUTHEN (no further check)
  // Expected (compiled in):     AU→401, AN→401, AR→200, AW→200, superuser→200
  // Expected (not compiled in): AU→404, AN→404, AR→404, AW→404, superuser→404
  {
    name: "Query failure-points availability (GET /_admin/debug/failat)",
    type: "admin",
    method: "GET",
    path: "/_admin/debug/failat",
  },

  // ── GET /_admin/debug/failat/all ─────────────────────────────────────────
  // Returns the list of all currently active failure points.  Read-only.
  // Auth: AUTHEN
  // Expected (compiled in):     AU→401, AN→401, AR→200, AW→200, superuser→200
  // Expected (not compiled in): AU→404, AN→404, AR→404, AW→404, superuser→404
  {
    name: "List all active failure points (GET /_admin/debug/failat/all)",
    type: "admin",
    method: "GET",
    path: "/_admin/debug/failat/all",
  },

  // ── PUT /_admin/debug/failat/{name} ──────────────────────────────────────
  // Activates a named failure point.  We use a test-only name that has no
  // effect on the running server.  teardown removes it unconditionally so
  // later tests are not affected.
  // Auth: AUTHEN
  // Expected (compiled in):     AU→401, AN→401, AR→200, AW→200, superuser→200
  // Expected (not compiled in): AU→404, AN→404, AR→404, AW→404, superuser→404
  {
    name: "Activate failure point (PUT /_admin/debug/failat/apitest-dummy)",
    type: "admin",
    method: "PUT",
    path: "/_admin/debug/failat/apitest-dummy",

    // Ensure no stale failure point from a previous interrupted run.
    setup: async (ctx) => {
      await ctx.request("DELETE", "/_admin/debug/failat/apitest-dummy");
      // Ignore the result — the point may not exist, and 404 is expected then.
    },

    // Remove the failure point after each matrix cell so it does not
    // accumulate across the 64+ iterations.
    teardown: async (ctx) => {
      await ctx.request("DELETE", "/_admin/debug/failat/apitest-dummy");
    },
  },

  // ── DELETE /_admin/debug/failat/{name} ───────────────────────────────────
  // Removes a single named failure point.  setup adds it first (as superuser)
  // so the DELETE has something to remove.  After the request teardown cleans
  // up any remnant (e.g. if the DELETE returned 404 because the endpoint is
  // not compiled in).
  // Auth: AUTHEN
  // Expected (compiled in):     AU→401, AN→401, AR→200, AW→200, superuser→200
  // Expected (not compiled in): AU→404, AN→404, AR→404, AW→404, superuser→404
  {
    name: "Remove single failure point (DELETE /_admin/debug/failat/apitest-dummy)",
    type: "admin",
    method: "DELETE",
    path: "/_admin/debug/failat/apitest-dummy",

    // Add the failure point as superuser so every matrix cell has something
    // to delete.
    setup: async (ctx) => {
      await ctx.request("PUT", "/_admin/debug/failat/apitest-dummy");
    },

    // Best-effort cleanup in case the DELETE under test did not fire (e.g.
    // the endpoint is not compiled in).
    teardown: async (ctx) => {
      await ctx.request("DELETE", "/_admin/debug/failat/apitest-dummy");
    },
  },

  // ── DELETE /_admin/debug/failat ──────────────────────────────────────────
  // Clears ALL active failure points at once.  Safe to call even when there
  // are no failure points; the call is idempotent.
  // Auth: AUTHEN
  // Expected (compiled in):     AU→401, AN→401, AR→200, AW→200, superuser→200
  // Expected (not compiled in): AU→404, AN→404, AR→404, AW→404, superuser→404
  {
    name: "Clear all failure points (DELETE /_admin/debug/failat)",
    type: "admin",
    method: "DELETE",
    path: "/_admin/debug/failat",
  },

  // ── DELETE /_admin/debug/raceControl ─────────────────────────────────────
  // Resets the DebugRaceController singleton.  Only effective in maintainer
  // builds (ARANGODB_ENABLE_MAINTAINER_MODE); in failure-tests-only builds it
  // returns 501 NOT_IMPLEMENTED.  When the entire handler is absent (no
  // failure tests) it returns 404.
  // Auth: AUTHEN (no further check)
  // Expected (maintainer build):       AU→401, AN→401, AR→200, AW→200, superuser→200
  // Expected (failure-tests-only):     AU→501, AN→501, AR→501, AW→501, superuser→501
  // Expected (not compiled in at all): AU→404, AN→404, AR→404, AW→404, superuser→404
  {
    name: "Reset race controller (DELETE /_admin/debug/raceControl)",
    type: "admin",
    method: "DELETE",
    path: "/_admin/debug/raceControl",
  },

];
