// Tests for the /_api/dump endpoint family.
//
// Handler: RestDumpHandler
// Mounted at: /_db/<name>/_api/dump (prefix)
//
// The dump API is available on single servers and on DBServers (in a cluster
// the coordinator proxies the request to a DBServer via the ?dbserver=
// query parameter).
//
// Auth model
// ──────────
//   POST /_api/dump/start      – canUseCollection(Read): AUTHEN + COLL RO
//   POST /_api/dump/next/{id}  – SAME USER (the user who called dump/start)
//   DELETE /_api/dump/{id}     – SAME USER
//
// Replication batch (required for dump/next)
// ──────────────────────────────────────────
// POST /_api/dump/next/{id} requires a ?batchId=<id> query parameter.
// A replication batch is created beforehand via:
//   POST /_api/replication/batch          body: { ttl: 600 }
// and cleaned up afterwards via:
//   DELETE /_api/replication/batch/{id}
// In cluster mode both calls carry ?DBserver=<leader> (note the capital B).
// The batch id is returned in the response body field 'id'.
//
// Query-parameter conventions
// ────────────────────────────
//   dump API  (start / next / delete)  → ?dbserver=<leader>  (lowercase d)
//   batch API (create / delete)        → ?DBserver=<leader>  (capital B)
//
// getDeployInfo(ctx) returns:
//   {
//     shardName,   – "c" (single) or the shard id (cluster)
//     dumpQS,      – "" or "?dbserver=<leader>"
//     batchQS,     – "" or "?DBserver=<leader>"
//   }
//
// Notes on SAME USER tests (dump/next and dump/delete)
// ─────────────────────────────────────────────────────
// The setup starts a dump session using the superuser JWT.  The "SAME USER"
// check restricts access to the dump session to that same principal.  In the
// test matrix:
//   • All Basic-auth users (the 64 matrix users, AU/AN/AR/AW) are a different
//     principal → they receive 401/403/404.
//   • The superuser column (JWT, same token as setup) is the same principal
//     → it receives 200/204.
// This clearly demonstrates the SAME USER restriction.

// ── helpers ──────────────────────────────────────────────────────────────────

/**
 * Determine deployment mode and shard/dbserver info for collection 'c' in 'd'.
 *
 * Returns { shardName, dumpQS, batchQS } where:
 *   shardName  "c" on single server; first shard ID of 'c' on a cluster.
 *   dumpQS     "" on single server; "?dbserver=<leader>" on a cluster.
 *   batchQS    "" on single server; "?DBserver=<leader>" on a cluster.
 */
async function getDeployInfo(ctx) {
  const roleResp = await ctx.request('GET', '/_admin/server/role');
  if (roleResp.status !== 200) {
    throw new Error(
      `getDeployInfo: GET /_admin/server/role failed: ${roleResp.status} ${JSON.stringify(roleResp.body)}`
    );
  }

  const role = roleResp.body.role;   // "SINGLE" | "COORDINATOR" | "DBSERVER" | …

  if (role === 'SINGLE') {
    return { shardName: 'c', dumpQS: '', batchQS: '' };
  }

  // Cluster: ask the coordinator for the authoritative inventory of database d.
  const invResp = await ctx.request('GET', '/_db/d/_api/replication/clusterInventory');
  if (invResp.status !== 200) {
    throw new Error(
      `getDeployInfo: clusterInventory failed: ${invResp.status} ${JSON.stringify(invResp.body)}`
    );
  }

  const collections = invResp.body.collections || [];
  const cEntry = collections.find(
    x => x.parameters && x.parameters.name === 'c'
  );
  if (!cEntry) {
    throw new Error('getDeployInfo: collection c not found in clusterInventory response');
  }

  const shardMap = cEntry.parameters.shards || {};
  const shardEntries = Object.entries(shardMap);
  if (shardEntries.length === 0) {
    throw new Error('getDeployInfo: no shards found for collection c');
  }

  const [shardName, servers] = shardEntries[0];
  if (!servers || servers.length === 0) {
    throw new Error('getDeployInfo: no servers found for shard of collection c');
  }

  const leader = servers[0];   // first entry is the current leader
  return {
    shardName,
    dumpQS:  `?dbserver=${encodeURIComponent(leader)}`,
    batchQS: `?DBserver=${encodeURIComponent(leader)}`,
  };
}

/**
 * Start a dump session as superuser.
 * Returns { dumpId, dumpQS, batchQS }.
 * The dump ID is taken from the response header 'x-arango-dump-id'.
 */
async function startDump(ctx) {
  const info = await getDeployInfo(ctx);
  const r = await ctx.request(
    'POST',
    `/_db/d/_api/dump/start${info.dumpQS}`,
    { shards: [info.shardName] }
  );
  if (r.status !== 200 && r.status !== 201) {
    throw new Error(
      `startDump: POST dump/start failed: ${r.status} ${JSON.stringify(r.body)}`
    );
  }
  const dumpId = r.headers['x-arango-dump-id'];
  if (!dumpId) {
    throw new Error(
      `startDump: response contained no 'x-arango-dump-id' header (headers: ${JSON.stringify(r.headers)})`
    );
  }
  return { dumpId, dumpQS: info.dumpQS, batchQS: info.batchQS };
}

/**
 * Create a replication batch and return its id.
 * batchQS is "" (single) or "?DBserver=<leader>" (cluster).
 */
async function createBatch(ctx, batchQS) {
  const r = await ctx.request(
    'POST',
    `/_api/replication/batch${batchQS}`,
    { ttl: 600 }
  );
  if (r.status !== 200 && r.status !== 201) {
    throw new Error(
      `createBatch: POST replication/batch failed: ${r.status} ${JSON.stringify(r.body)}`
    );
  }
  const batchId = r.body.id;
  if (!batchId) {
    throw new Error(
      `createBatch: response contained no 'id' field: ${JSON.stringify(r.body)}`
    );
  }
  return batchId;
}

/**
 * Delete a replication batch (best-effort; errors are silently ignored).
 */
async function deleteBatch(ctx, batchId, batchQS) {
  await ctx.request(
    'DELETE',
    `/_api/replication/batch/${batchId}${batchQS}`
  );
}

// ── test entries ─────────────────────────────────────────────────────────────

export default [

  // ── POST /_db/d/_api/dump/start ───────────────────────────────────────────
  {
    // Start a new dump session for collection 'c' in database 'd'.
    // Auth: canUseCollection(Read) → AUTHEN + COLL RO.
    //
    // setup:    detect deployment mode and shard info (no dump is started yet;
    //           the test request itself starts the dump).
    // teardown: if the test request created a dump (status 200/201 and the
    //           x-arango-dump-id response header is set), abort it.
    //           The abort uses the superuser JWT.  If the SAME USER check
    //           prevents the superuser from aborting a dump started by a
    //           different user the DELETE may return 4xx; that is silently
    //           tolerated here since the sessions are short-lived.
    //
    // Expected (no RBAC, no explicit COLL grant):
    //   DB-undef → 401, DB-none → 401,
    //   DB-ro + COLL-undef / COLL-none → 403,
    //   DB-ro + COLL-ro / COLL-rw     → 200,
    //   DB-rw + COLL-*                → similar to DB-ro
    name: 'Start dump of collection c (POST /_db/d/_api/dump/start)',
    type: 'all',
    method: 'POST',
    path: '/_db/d/_api/dump/start${ctx.data.dumpQS}',
    body: { shards: ['${ctx.data.shardName}'] },
    setup: async (ctx) => {
      return await getDeployInfo(ctx);
    },
    teardown: async (ctx) => {
      // Try to abort the dump that the test user may have started.
      const resp = ctx.response;
      const dumpId = resp && resp.headers && resp.headers['x-arango-dump-id'];
      if (dumpId) {
        await ctx.request(
          'DELETE',
          `/_db/d/_api/dump/${dumpId}${ctx.data.dumpQS}`
        );
      }
    },
  },

  // ── POST /_db/d/_api/dump/next/{id} ──────────────────────────────────────
  {
    // Fetch the next batch of a running dump session.
    // Auth: SAME USER – only the user who called dump/start may call this.
    //
    // The call requires a ?batchId=<id> query parameter.  A replication batch
    // is created in setup (POST /_api/replication/batch) and deleted in
    // teardown (DELETE /_api/replication/batch/{id}).  In cluster mode both
    // batch calls carry ?DBserver=<leader>.
    //
    // setup:    start dump session + create replication batch as superuser.
    //           Returns { dumpId, batchId, nextQS, dumpQS, batchQS }.
    // teardown: abort dump + delete batch (both as superuser, idempotent).
    //
    // Expected:
    //   All Basic-auth users (matrix + admin AU/AN/AR/AW) → 401/403/404
    //     (not the principal that started the dump).
    //   Superuser column (same JWT as setup) → 200 or 204 (no-more-data).
    name: 'Fetch next dump batch (POST /_db/d/_api/dump/next/{id})',
    type: 'all',
    method: 'POST',
    path: '/_db/d/_api/dump/next/${ctx.data.dumpId}${ctx.data.nextQS}',
    setup: async (ctx) => {
      const { dumpId, dumpQS, batchQS } = await startDump(ctx);
      const batchId = await createBatch(ctx, batchQS);
      // Build the full query string for the dump/next path:
      //   single:  ?batchId=<id>
      //   cluster: ?batchId=<id>&dbserver=<leader>
      const nextQS = `?batchId=${encodeURIComponent(batchId)}${dumpQS ? `&${dumpQS.slice(1)}` : ''}`;
      return { dumpId, batchId, nextQS, dumpQS, batchQS };
    },
    teardown: async (ctx) => {
      if (ctx.data) {
        const { dumpId, batchId, dumpQS, batchQS } = ctx.data;
        if (dumpId) {
          await ctx.request('DELETE', `/_db/d/_api/dump/${dumpId}${dumpQS}`);
        }
        if (batchId) {
          await deleteBatch(ctx, batchId, batchQS);
        }
      }
    },
  },

  // ── DELETE /_db/d/_api/dump/{id} ─────────────────────────────────────────
  {
    // Abort / finish a running dump session.
    // Auth: SAME USER – only the user who called dump/start may call this.
    //
    // setup:    start a dump session as superuser; return { dumpId, dumpQS }.
    // teardown: attempt to abort the dump session as superuser; this is a
    //           no-op if the test request (superuser column) already deleted it.
    //
    // Expected:
    //   All Basic-auth users → 401/403/404 (not the owner of the session).
    //   Superuser column     → 200 (session aborted successfully).
    name: 'Abort dump session (DELETE /_db/d/_api/dump/{id})',
    type: 'all',
    method: 'DELETE',
    path: '/_db/d/_api/dump/${ctx.data.dumpId}${ctx.data.dumpQS}',
    setup: async (ctx) => {
      return await startDump(ctx);
    },
    teardown: async (ctx) => {
      if (ctx.data && ctx.data.dumpId) {
        // Ignore any error; the test request may already have deleted the session.
        await ctx.request(
          'DELETE',
          `/_db/d/_api/dump/${ctx.data.dumpId}${ctx.data.dumpQS}`
        );
      }
    },
  },

];
