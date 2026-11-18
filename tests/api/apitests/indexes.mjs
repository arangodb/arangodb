// Tests for the /_api/index endpoint family.
//
// Handler: RestIndexHandler
// Mounted at: /_db/d/_api/index (prefix)
//
// All tests use database 'd' (created by global setup) and collection 'c'
// (which contains 100 documents {"Hallo": 1} … {"Hallo": 100}).
//
// Auth model
// ──────────
//  GET    /_api/index                   – list indexes for a collection   → COLL RO
//  GET    /_api/index/selectivity       – selectivity estimates           → COLL RO
//  POST   /_api/index                   – create a new index              → COLL RWMETA
//  POST   /_api/index/sync-caches       – sync index read caches          → AUTHEN
//  DELETE /_api/index/{collection}/{id} – drop an index                   → COLL RWMETA

export default [

  // ── GET /_api/index ───────────────────────────────────────────────────────

  {
    // GET /_api/index?collection=c
    // Returns the list of all indexes on collection 'c'.
    // Expected: all authenticated users with COLL RO → 200.
    name: "List indexes (GET /_api/index?collection=c)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/index?collection=c",
  },

  // ── GET /_api/index/selectivity ──────────────────────────────────────────

  {
    // GET /_api/index/selectivity?collection=c
    // Returns selectivity estimates used during index build decisions.
    // Expected: all authenticated users with COLL RO → 200.
    name: "Index selectivity (GET /_api/index/selectivity?collection=c)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/index/selectivity?collection=c",
  },

  // ── POST /_api/index ──────────────────────────────────────────────────────

  {
    // POST /_api/index?collection=c
    // Creates a persistent index on field 'Hallo'.
    // teardown: if the index was newly created (201), drop it so each run
    //           starts from a clean state.  If it already existed (200), it
    //           is left untouched.
    // Expected: AU/AN/AR → 403 (no COLL RWMETA); AW/SU → 201 (new) or 200.
    name: "Create index (POST /_api/index?collection=c)",
    type: "all",
    method: "POST",
    path: "/_db/d/_api/index?collection=c",
    body: { type: "persistent", fields: ["Hallo"] },
    teardown: async (ctx) => {
      if (ctx.response && ctx.response.status === 201 &&
          ctx.response.body && ctx.response.body.id) {
        await ctx.request('DELETE', `/_db/d/_api/index/${ctx.response.body.id}`);
      }
    },
  },

  // ── POST /_api/index/sync-caches ─────────────────────────────────────────

  {
    // POST /_api/index/sync-caches
    // Synchronises in-memory index read caches.  No body required; the call
    // is idempotent and safe.
    // Auth: AUTHEN — any authenticated user.
    // Expected: all authenticated users → 200.
    name: "Sync index caches (POST /_api/index/sync-caches)",
    type: "all",
    method: "POST",
    path: "/_db/d/_api/index/sync-caches",
  },

  // ── DELETE /_api/index/{collection}/{id} ─────────────────────────────────

  {
    // DELETE /_api/index/<index-handle>
    // The index handle has the form "c/<numeric-id>" so the full path becomes
    // /_db/d/_api/index/c/<numeric-id>.
    // setup:    superuser creates a fresh persistent index on field 'Hallo'
    //           and stores its handle (e.g. "c/12345") in ctx.data.id.
    // teardown: if the test user did not (or could not) delete the index, the
    //           superuser cleans it up.
    // Expected: AU/AN/AR → 403 (no COLL RWMETA); AW/SU → 200 (dropped).
    name: "Drop index (DELETE /_api/index/<index-handle>)",
    type: "all",
    method: "DELETE",
    path: "/_db/d/_api/index/${ctx.data.id}",
    setup: async (ctx) => {
      const resp = await ctx.request('POST', '/_db/d/_api/index?collection=c',
        { type: 'persistent', fields: ['Hallo'] });
      if (!resp.body || !resp.body.id) {
        throw new Error(`setup: failed to create index: ${resp.status} ${JSON.stringify(resp.body)}`);
      }
      return { id: resp.body.id };
    },
    teardown: async (ctx) => {
      // Drop the index if the test user lacked permission (or failed for any
      // other reason) so it does not accumulate across test runs.
      if (!ctx.response || ctx.response.status !== 200) {
        await ctx.request('DELETE', `/_db/d/_api/index/${ctx.data.id}`);
      }
    },
  },

];
