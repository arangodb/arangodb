// Tests for the /_api/replication endpoint family.
//
// Handler: RestReplicationHandler (base)
//          RocksDBRestReplicationHandler (single-server / DBServer)
//          ClusterRestReplicationHandler  (coordinator)
//
// All tests use type "admin" (5 columns: AU, AN, AR, AW, superuser).
// Paths are anchored to /_db/_system/.
//
// Auth model
// ──────────
// Most replication endpoints are internal APIs used by the replication
// framework with a superuser JWT.  The handlers themselves do not always
// contain an explicit isSuperuser() check; the table marks them "SUPER"
// because they are only ever called with superuser credentials in practice.
//
// A global auth check (CommTask) still requires a valid JWT before any
// handler is invoked, so unauthenticated (AU) requests are rejected early.
//
// Batch / trampoline behaviour on coordinator
// ────────────────────────────────────────────
// The batch command uses a trampoline on coordinator (forwarded to a DBServer).
// The ?DBserver=DBServer0001 query parameter is required on coordinator so the
// trampoline knows where to forward the request.  It is harmless on a single
// server or a DBServer, so we include it unconditionally.

export default [

  // ── batch (POST / PUT / DELETE) ───────────────────────────────────────────

  {
    // POST /_api/replication/batch?DBserver=DBServer0001
    // Creates a replication snapshot context.  Returns {"id": "<numeric-id>"}.
    // teardown: if the batch was created (status 200), discard it so snapshot
    //           contexts do not accumulate.
    // Expected: AU→401/403, AN/AR/AW/SU→200 (single-server / DBServer)
    name: "Replication create batch (POST /_api/replication/batch)",
    type: "admin",
    method: "POST",
    path: "/_db/_system/_api/replication/batch?DBserver=DBServer0001",
    body: { ttl: 30 },
    teardown: async (ctx) => {
      if (ctx.response && ctx.response.status === 200 &&
          ctx.response.body && ctx.response.body.id) {
        await ctx.request('DELETE',
          `/_db/_system/_api/replication/batch/${ctx.response.body.id}?DBserver=DBServer0001`);
      }
    },
  },

  {
    // PUT /_api/replication/batch/{id}?DBserver=DBServer0001
    // Extends (refreshes the TTL of) an existing batch context.
    // setup:    superuser creates a fresh batch and stores its id.
    // teardown: superuser discards the batch regardless of test outcome.
    // Expected: AU→401/403, AN/AR/AW/SU→204
    name: "Replication extend batch (PUT /_api/replication/batch/<id>)",
    type: "admin",
    method: "PUT",
    path: "/_db/_system/_api/replication/batch/${ctx.data.id}?DBserver=DBServer0001",
    body: { ttl: 30 },
    setup: async (ctx) => {
      const resp = await ctx.request('POST',
        '/_db/_system/_api/replication/batch?DBserver=DBServer0001', { ttl: 60 });
      if (!resp.body || !resp.body.id) {
        throw new Error(`setup: failed to create batch: ${resp.status} ${JSON.stringify(resp.body)}`);
      }
      return { id: resp.body.id };
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE',
        `/_db/_system/_api/replication/batch/${ctx.data.id}?DBserver=DBServer0001`);
    },
  },

  {
    // DELETE /_api/replication/batch/{id}?DBserver=DBServer0001
    // Discards an existing batch context.
    // setup:    superuser creates a fresh batch and stores its id.
    // teardown: superuser cleans up if the test user could not delete it.
    // Expected: AU→401/403, AN/AR/AW/SU→204
    name: "Replication delete batch (DELETE /_api/replication/batch/<id>)",
    type: "admin",
    method: "DELETE",
    path: "/_db/_system/_api/replication/batch/${ctx.data.id}?DBserver=DBServer0001",
    setup: async (ctx) => {
      const resp = await ctx.request('POST',
        '/_db/_system/_api/replication/batch?DBserver=DBServer0001', { ttl: 60 });
      if (!resp.body || !resp.body.id) {
        throw new Error(`setup: failed to create batch: ${resp.status} ${JSON.stringify(resp.body)}`);
      }
      return { id: resp.body.id };
    },
    teardown: async (ctx) => {
      // No-op if the test already deleted it; silently ignore 404.
      if (!ctx.response || ctx.response.status !== 200) {
        await ctx.request('DELETE',
          `/_db/_system/_api/replication/batch/${ctx.data.id}?DBserver=DBServer0001`);
      }
    },
  },

  // ── clusterInventory ──────────────────────────────────────────────────────

  {
    // GET /_api/replication/clusterInventory
    // Auth: AdminClusterInfo OR COLL RO (auth check inside handler).
    // Only meaningful on coordinator; single-server → 403 CLUSTER_ONLY_ON_COORDINATOR.
    // Expected on coordinator: AU→403, AN→403, AR→403, AW→200, SU→200
    name: "Replication clusterInventory (GET /_api/replication/clusterInventory)",
    type: "admin",
    method: "GET",
    path: "/_db/_system/_api/replication/clusterInventory",
  },

];
