// Tests for the /_admin/actions prefix (MaintenanceRestHandler).
//
// All four HTTP methods are covered.  The endpoint is marked AUTHEN in the
// permission table — any authenticated user can reach it regardless of their
// _system access level.  Functionally these endpoints are most relevant on
// DBServers, but the MaintenanceRestHandler is mounted on every server type
// so the tests run on coordinators and single servers as well.
//
// Expected status codes:
//   GET    /_admin/actions        → 200  (read-only registry dump)
//   POST   /_admin/actions        → 200  (proceed / pause commands)
//   PUT    /_admin/actions        → 400  (empty body rejected before
//                                         the action engine is invoked)
//   DELETE /_admin/actions/{id}   → 400  (non-existent action ID)
//
// For the PUT and DELETE cases a 400 is still proof that the caller was
// authenticated and the request reached the handler — an auth failure would
// produce 401 or 403 instead.

export default [
  {
    // GET returns the current maintenance status ("running" or "paused") and
    // the full action registry.  It is a pure read and has no side effects.
    name: "List maintenance actions (GET /_admin/actions)",
    type: "admin",
    method: "GET",
    path: "/_admin/actions",
  },

  {
    // "proceed" is idempotent: resuming an already-running maintenance
    // feature is a no-op that always succeeds with 200.
    name: "Resume maintenance feature (POST /_admin/actions – proceed)",
    type: "admin",
    method: "POST",
    path: "/_admin/actions",
    body: { execute: "proceed" },
  },

  {
    // Pause the maintenance feature for 1 second (the minimum allowed by the
    // handler; the maximum is 300 s).  The teardown hook immediately resumes
    // maintenance so subsequent test rows are not affected.
    name: "Pause maintenance feature (POST /_admin/actions – pause)",
    type: "admin",
    method: "POST",
    path: "/_admin/actions",
    body: { execute: "pause", duration: 1 },
    teardown: async (ctx) => {
      await ctx.request('POST', '/_admin/actions', { execute: "proceed" });
    },
  },

  {
    // PUT with an empty JSON body is rejected by the handler before the
    // action engine is ever invoked:
    //   if (good && _request->payload().isEmptyObject()) →
    //     generateError(BAD, TRI_ERROR_HTTP_CORRUPTED_JSON)
    // All authenticated users therefore receive 400.
    name: "Submit maintenance action – empty body (PUT /_admin/actions)",
    type: "admin",
    method: "PUT",
    path: "/_admin/actions",
    body: {},
  },

  {
    // DELETE requires exactly one numeric suffix that identifies an existing
    // action.  Action ID 999999 will never exist in a normal test run, so
    // the handler returns 400 "deleteAction could not find action to delete".
    // All authenticated users receive 400, proving the endpoint is reachable.
    name: "Cancel non-existent maintenance action (DELETE /_admin/actions/999999)",
    type: "admin",
    method: "DELETE",
    path: "/_admin/actions/999999",
  },
];
