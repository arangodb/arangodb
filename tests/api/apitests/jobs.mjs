// Tests for the async-job API endpoints.
//
// These routes are mounted at TWO prefixes that share the same handler
// (RestJobHandler):
//
//   /_admin/job/{id|type}          same handler as /_api/job
//   /_api/job/{id|type}
//
// The handler itself has no per-user authorization logic.  Auth is checked
// by the general server layer, which requires at least read access to the
// _system database for every request that reaches this handler.
//
// Authorization per user column
// ─────────────────────────────
//   AU (_sys undef) → 401  No _system grant at all; rejected before the
//                           handler runs.
//   AN (_sys none)  → 401  Explicitly denied _system; rejected before the
//                           handler runs.
//   AR (_sys ro)    → authenticated; see per-test notes below.
//   AW (_sys rw)    → authenticated; see per-test notes below.
//   superuser       → authenticated, isSuperuser() == true.
//
// Ownership check in AsyncJobManager
// ────────────────────────────────────
// Every job stored by the manager is tagged with the username of the
// request that created it (empty string for a superuser JWT).  The
// `authorized()` helper inside AsyncJobManager allows access only when
// exec.isSuperuser() == true OR exec.user() == job.owner.
//
// Because setup() runs as the superuser, all jobs created in setup are
// tagged with owner == "".  Consequently:
//
//   AR / AW  → authenticated, but owner mismatch → 404 (JOB_UNDEFINED)
//   superuser → isSuperuser() == true → full access → appropriate 2xx
//
// This applies to all ID-specific operations (GET/{id}, PUT/{id},
// PUT/{id}/cancel, DELETE/{id}).  Bulk operations (GET/done, GET/pending,
// DELETE/all, DELETE/expired) operate only on the caller's own jobs and
// return 200 with an empty result set for AR/AW.
//
// Mount-point note
// ─────────────────
// The last test uses /_admin/job to confirm that the alternative mount
// point behaves identically.  All per-operation auth semantics are
// identical between the two prefixes.

// ── Shared setup helper ───────────────────────────────────────────────────────
//
// Creates one async job by making a GET /_api/version request with the
// x-arango-async: store header (superuser context).  Returns the job ID
// string extracted from the x-arango-async-id response header.
//
// /_api/version is an OPEN endpoint, so it completes almost immediately
// and the job is typically in JOB_DONE state by the time the test request
// is issued.

async function createJob(ctx) {
  const r = await ctx.request('GET', '/_api/version', null, {
    'x-arango-async': 'store',
  });
  const jobId = r.headers['x-arango-async-id'];
  if (!jobId) {
    throw new Error(
      `x-arango-async-id header missing; response status was ${r.status}`
    );
  }
  return { jobId };
}

// Shared teardown: delete the job (idempotent; 404 is silently accepted).
async function deleteJob(ctx) {
  await ctx.request('DELETE', `/_api/job/${ctx.data.jobId}`);
  // ctx.request never throws on HTTP error codes, so 404 is safely ignored.
}

export default [

  // ── GET /_api/job/done ─────────────────────────────────────────────────────
  // Returns a JSON array of job IDs with status JOB_DONE for the current user.
  // The list is filtered by ownership, so AR/AW receive an empty array ([]).
  // No setup/teardown needed.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "List done async jobs (GET /_api/job/done)",
    type: "admin",
    method: "GET",
    path: "/_api/job/done",
  },

  // ── GET /_api/job/pending ──────────────────────────────────────────────────
  // Returns a JSON array of job IDs with status JOB_PENDING for the current user.
  // Same ownership filtering as /done.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "List pending async jobs (GET /_api/job/pending)",
    type: "admin",
    method: "GET",
    path: "/_api/job/pending",
  },

  // ── GET /_api/job/{id} ─────────────────────────────────────────────────────
  // Returns 200 (done), 204 (pending), or 404 (not found / not owned).
  // Setup creates a superuser-owned job; AR/AW are not the owner → 404.
  // Expected: AU→401, AN→401, AR→404, AW→404, SU→200 or 204
  {
    name: "Get async job status by ID (GET /_api/job/{id})",
    type: "admin",
    method: "GET",
    path: "/_api/job/${ctx.data.jobId}",
    setup: createJob,
    teardown: deleteJob,
  },

  // ── PUT /_api/job/{id} ─────────────────────────────────────────────────────
  // Fetches (and removes) the job result from the manager.
  // Setup creates a fresh superuser-owned job for every matrix cell.
  // AR/AW: not the owner → 404.
  // SU: job done → 200 (original response); pending → 204.
  //   After a successful PUT the job is consumed and teardown finds 404,
  //   which deleteJob silently ignores.
  // Expected: AU→401, AN→401, AR→404, AW→404, SU→200 or 204
  {
    name: "Fetch async job result (PUT /_api/job/{id})",
    type: "admin",
    method: "PUT",
    path: "/_api/job/${ctx.data.jobId}",
    setup: createJob,
    teardown: deleteJob,
  },

  // ── PUT /_api/job/{id}/cancel ──────────────────────────────────────────────
  // Attempts to cancel a job.  If the job is already JOB_DONE the cancel is
  // still accepted by cancelJob() and returns 200 (the handler field is simply
  // set to nullptr).  AR/AW: not the owner → 404.
  // Expected: AU→401, AN→401, AR→404, AW→404, SU→200
  {
    name: "Cancel async job (PUT /_api/job/{id}/cancel)",
    type: "admin",
    method: "PUT",
    path: "/_api/job/${ctx.data.jobId}/cancel",
    setup: createJob,
    teardown: deleteJob,
  },

  // ── DELETE /_api/job/all ───────────────────────────────────────────────────
  // Deletes all jobs owned by the current user (iterates the manager and
  // removes only those for which authorized() returns true).
  // AR/AW own no jobs → nothing deleted, but still returns 200 {result:true}.
  // SU deletes everything it owns.
  // No setup/teardown needed — the operation is idempotent and safe.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Delete all async jobs for current user (DELETE /_api/job/all)",
    type: "admin",
    method: "DELETE",
    path: "/_api/job/all",
  },

  // ── DELETE /_api/job/expired ───────────────────────────────────────────────
  // Deletes expired jobs owned by the current user (stamp = 0 selects every
  // job whose creation timestamp is before epoch, i.e. all expired jobs).
  // AR/AW own no jobs → nothing deleted, but still returns 200 {result:true}.
  // The stamp parameter is required; without it the server returns 400.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Delete expired async jobs for current user (DELETE /_api/job/expired)",
    type: "admin",
    method: "DELETE",
    path: "/_api/job/expired?stamp=0",
  },

  // ── DELETE /_api/job/{id} ──────────────────────────────────────────────────
  // Deletes one specific job result from the manager.
  // AR/AW: not the owner → deleteJobResult returns false → 404.
  // SU: owner → job deleted → 200 {result:true}.
  //   Teardown attempts a second DELETE; the job is already gone → 404,
  //   which deleteJob silently ignores.
  // Expected: AU→401, AN→401, AR→404, AW→404, SU→200
  {
    name: "Delete specific async job by ID (DELETE /_api/job/{id})",
    type: "admin",
    method: "DELETE",
    path: "/_api/job/${ctx.data.jobId}",
    setup: createJob,
    teardown: deleteJob,
  },

  // ── /_admin/job mount-point smoke test ────────────────────────────────────
  // Both /_admin/job and /_api/job are mounted to the same RestJobHandler.
  // This entry confirms the /_admin/job prefix is live and enforces identical
  // auth semantics.  A GET /done is sufficient as a mount-point check.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "List done async jobs via admin mount point (GET /_admin/job/done)",
    type: "admin",
    method: "GET",
    path: "/_admin/job/done",
  },

];
