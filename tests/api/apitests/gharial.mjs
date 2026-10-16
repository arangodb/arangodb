// Tests for the /_api/gharial endpoint family (Named Graph / Gharial API).
//
// Handler: RestGraphHandler
// Mounted at: /_db/<name>/_api/gharial (prefix)
//
// The global setup provides:
//   database 'd'
//   document collection 'c'  with 100 docs keyed k1–k100
//   edge collection    'e'  with 100 circular edges (c/k1→c/k2 … c/k100→c/k1)
//   named graph        'g'  edge-def: e (c → c)
//   64 permission-matrix users; each has the same grant on 'e' as on 'c'
//
// Test strategy
// ─────────────
// • Tests that only READ the existing graph 'g' or its data are type "all"
//   (collection × database × admin matrices).
// • Tests that INSERT/REPLACE/UPDATE/DELETE individual edge or vertex
//   *documents* inside 'g' are also type "all"; they use a dedicated key
//   'e_apitest' or 'v_apitest' so that the global setup data is not disturbed.
// • Tests that modify the STRUCTURE of a graph (add/remove edge definitions
//   or vertex collections, create/delete a graph) operate on a temporary
//   graph 'g_apitest' and are type ["admin", "database"].
//
// Temporary names used across tests
// ───────────────────────────────────
//   g_apitest         – temp graph (same db d)
//   e_apitest         – temp edge collection used as edge def in g_apitest
//   e2_apitest        – second temp edge collection (add-edge-def test)
//   c_orphan_apitest  – temp document collection added as orphan vertex
//   e_apitest (key)   – key used for temp edge documents inside g/e
//   v_apitest (key)   – key used for temp vertex documents inside g/c

// ── constants ────────────────────────────────────────────────────────────────

const G            = 'g';
const G_APITEST    = 'g_apitest';
const E_APITEST    = 'e_apitest';
const E2_APITEST   = 'e2_apitest';
const C_ORPHAN     = 'c_orphan_apitest';

// Key used for temporary documents inside g/e and g/c.
const EDGE_KEY   = 'e_apitest_doc';
const VERTEX_KEY = 'v_apitest_doc';

// ── helpers ──────────────────────────────────────────────────────────────────

// ── edge documents inside the global graph g/e ────────────────────────────

async function insertTestEdge(ctx) {
  // Delete stale, then insert a known edge.
  await ctx.request('DELETE', `/_db/d/_api/document/e/${EDGE_KEY}`);
  const r = await ctx.request('POST', '/_db/d/_api/document/e',
    { _key: EDGE_KEY, _from: 'c/k1', _to: 'c/k2' });
  if (r.status !== 201 && r.status !== 202) {
    throw new Error(`setup: insert test edge failed: ${r.status} ${JSON.stringify(r.body)}`);
  }
}

async function deleteTestEdge(ctx) {
  await ctx.request('DELETE', `/_db/d/_api/document/e/${EDGE_KEY}`);
}

// ── vertex documents inside the global graph g/c ─────────────────────────

async function insertTestVertex(ctx) {
  await ctx.request('DELETE', `/_db/d/_api/document/c/${VERTEX_KEY}`);
  const r = await ctx.request('POST', '/_db/d/_api/document/c',
    { _key: VERTEX_KEY, value: 9999 });
  if (r.status !== 201 && r.status !== 202) {
    throw new Error(`setup: insert test vertex failed: ${r.status} ${JSON.stringify(r.body)}`);
  }
}

async function deleteTestVertex(ctx) {
  await ctx.request('DELETE', `/_db/d/_api/document/c/${VERTEX_KEY}`);
}

// ── temporary graph g_apitest (structural tests) ──────────────────────────

// Creates g_apitest with a single edge definition e_apitest (c → c).
// e_apitest is created explicitly so we control it.
async function createGapitest(ctx) {
  // Clean up stale artefacts from a previous failed run.
  await ctx.request('DELETE', `/_db/d/_api/gharial/${G_APITEST}?dropCollections=false`);
  await ctx.request('DELETE', `/_db/d/_api/collection/${E_APITEST}`);
  // Create the temporary edge collection.
  const rc = await ctx.request('POST', '/_db/d/_api/collection',
    { name: E_APITEST, type: 3 });
  if (rc.status !== 200 && rc.status !== 201) {
    throw new Error(`setup: create ${E_APITEST} failed: ${rc.status} ${JSON.stringify(rc.body)}`);
  }
  // Create the graph.
  const rg = await ctx.request('POST', '/_db/d/_api/gharial', {
    name: G_APITEST,
    edgeDefinitions: [{ collection: E_APITEST, from: ['c'], to: ['c'] }],
  });
  if (rg.status !== 201 && rg.status !== 202) {
    throw new Error(`setup: create ${G_APITEST} failed: ${rg.status} ${JSON.stringify(rg.body)}`);
  }
}

// Deletes g_apitest and the associated temp collections.
async function deleteGapitest(ctx) {
  await ctx.request('DELETE', `/_db/d/_api/gharial/${G_APITEST}?dropCollections=false`);
  await ctx.request('DELETE', `/_db/d/_api/collection/${E_APITEST}`);
}

// Like createGapitest but also creates e2_apitest (used by add-edge-def test).
async function createGapitestWithE2(ctx) {
  await createGapitest(ctx);
  await ctx.request('DELETE', `/_db/d/_api/collection/${E2_APITEST}`);
  const r = await ctx.request('POST', '/_db/d/_api/collection',
    { name: E2_APITEST, type: 3 });
  if (r.status !== 200 && r.status !== 201) {
    throw new Error(`setup: create ${E2_APITEST} failed: ${r.status} ${JSON.stringify(r.body)}`);
  }
}

async function deleteGapitestWithE2(ctx) {
  await ctx.request('DELETE', `/_db/d/_api/gharial/${G_APITEST}?dropCollections=false`);
  await ctx.request('DELETE', `/_db/d/_api/collection/${E_APITEST}`);
  await ctx.request('DELETE', `/_db/d/_api/collection/${E2_APITEST}`);
}

// Like createGapitest but also creates and registers C_ORPHAN as orphan vertex.
async function createGapitestWithOrphan(ctx) {
  await createGapitest(ctx);
  await ctx.request('DELETE', `/_db/d/_api/collection/${C_ORPHAN}`);
  const rc = await ctx.request('POST', '/_db/d/_api/collection',
    { name: C_ORPHAN, type: 2 });
  if (rc.status !== 200 && rc.status !== 201) {
    throw new Error(`setup: create ${C_ORPHAN} failed: ${rc.status} ${JSON.stringify(rc.body)}`);
  }
  // Register as orphan vertex collection in g_apitest.
  const rv = await ctx.request('POST', `/_db/d/_api/gharial/${G_APITEST}/vertex`,
    { collection: C_ORPHAN });
  if (rv.status !== 200 && rv.status !== 201 && rv.status !== 202) {
    throw new Error(`setup: add orphan ${C_ORPHAN} failed: ${rv.status} ${JSON.stringify(rv.body)}`);
  }
}

// Like createGapitest but also creates C_ORPHAN without registering it yet
// (used by the "add orphan vertex collection" test).
async function createGapitestForAddOrphan(ctx) {
  await createGapitest(ctx);
  await ctx.request('DELETE', `/_db/d/_api/collection/${C_ORPHAN}`);
  const rc = await ctx.request('POST', '/_db/d/_api/collection',
    { name: C_ORPHAN, type: 2 });
  if (rc.status !== 200 && rc.status !== 201) {
    throw new Error(`setup: create ${C_ORPHAN} failed: ${rc.status} ${JSON.stringify(rc.body)}`);
  }
}

async function deleteGapitestWithOrphan(ctx) {
  await ctx.request('DELETE', `/_db/d/_api/gharial/${G_APITEST}?dropCollections=false`);
  await ctx.request('DELETE', `/_db/d/_api/collection/${E_APITEST}`);
  await ctx.request('DELETE', `/_db/d/_api/collection/${C_ORPHAN}`);
}

// ── test entries ─────────────────────────────────────────────────────────────

export default [

  // ── GET /_db/d/_api/gharial ───────────────────────────────────────────────
  {
    // List all graphs visible to the requesting user.
    // Auth: canSeeGraph → DB RO (response is filtered to visible graphs).
    // Expected: DB-undef→401, DB-none→401, DB-ro→200, DB-rw→200
    name: 'List graphs (GET /_db/d/_api/gharial)',
    type: 'all',
    method: 'GET',
    path: '/_db/d/_api/gharial',
  },

  // ── POST /_db/d/_api/gharial ──────────────────────────────────────────────
  {
    // Create a new named graph.
    // Auth: canCreateGraph → DB RW.
    // Uses a temporary graph g_apitest to avoid modifying the persistent 'g'.
    // setup:    remove g_apitest and e_apitest if stale.
    // teardown: remove g_apitest (and e_apitest) regardless of outcome.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→201/202
    name: 'Create graph (POST /_db/d/_api/gharial)',
    type: ['admin', 'database'],
    method: 'POST',
    path: '/_db/d/_api/gharial',
    body: {
      name: G_APITEST,
      edgeDefinitions: [{ collection: E_APITEST, from: ['c'], to: ['c'] }],
    },
    setup: async (ctx) => {
      // Ensure a clean slate; also pre-create e_apitest so the test body
      // uses an already-existing collection (avoids needing createCollection).
      await ctx.request('DELETE', `/_db/d/_api/gharial/${G_APITEST}?dropCollections=false`);
      await ctx.request('DELETE', `/_db/d/_api/collection/${E_APITEST}`);
      await ctx.request('POST', '/_db/d/_api/collection',
        { name: E_APITEST, type: 3 });
    },
    teardown: async (ctx) => {
      await deleteGapitest(ctx);
    },
  },

  // ── GET /_db/d/_api/gharial/{graph} ───────────────────────────────────────
  {
    // Get the graph descriptor for 'g'.
    // Auth: canUseGraph(RO) → DB RO.
    // Expected: DB-undef→401, DB-none→401, DB-ro→200, DB-rw→200
    name: `Get graph descriptor (GET /_db/d/_api/gharial/${G})`,
    type: 'all',
    method: 'GET',
    path: `/_db/d/_api/gharial/${G}`,
  },

  // ── DELETE /_db/d/_api/gharial/{graph} ────────────────────────────────────
  {
    // Delete a named graph.
    // Auth: canDropGraph → DB RW.
    // Uses g_apitest; teardown re-creates the graph if the test succeeded,
    // then removes it (i.e., teardown is idempotent via deleteGapitest).
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200/202
    name: `Delete graph (DELETE /_db/d/_api/gharial/${G_APITEST})`,
    type: ['admin', 'database'],
    method: 'DELETE',
    path: `/_db/d/_api/gharial/${G_APITEST}?dropCollections=false`,
    setup: async (ctx) => {
      await createGapitest(ctx);
    },
    teardown: async (ctx) => {
      // The test may have deleted g_apitest; clean up unconditionally.
      await deleteGapitest(ctx);
    },
  },

  // ── GET /_db/d/_api/gharial/{graph}/edge ──────────────────────────────────
  {
    // List all edge definitions of graph 'g'.
    // Auth: canUseGraph(RO) → DB RO.
    // Expected: DB-undef→401, DB-none→401, DB-ro→200, DB-rw→200
    name: `List edge definitions (GET /_db/d/_api/gharial/${G}/edge)`,
    type: 'all',
    method: 'GET',
    path: `/_db/d/_api/gharial/${G}/edge`,
  },

  // ── POST /_db/d/_api/gharial/{graph}/edge ─────────────────────────────────
  {
    // Add an edge definition to graph g_apitest.
    // Auth: canUseGraph(RW) → DB RW.
    // setup:    create g_apitest (with e_apitest) + e2_apitest collection.
    // teardown: delete g_apitest + e_apitest + e2_apitest.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→201/202
    name: `Add edge definition (POST /_db/d/_api/gharial/${G_APITEST}/edge)`,
    type: ['admin', 'database'],
    method: 'POST',
    path: `/_db/d/_api/gharial/${G_APITEST}/edge`,
    body: { collection: E2_APITEST, from: ['c'], to: ['c'] },
    setup: async (ctx) => {
      await createGapitestWithE2(ctx);
    },
    teardown: async (ctx) => {
      await deleteGapitestWithE2(ctx);
    },
  },

  // ── GET /_db/d/_api/gharial/{graph}/edge/{definition}/{key} ───────────────
  {
    // Read a single edge document from g/e.
    // Auth: canUseGraph(RO) → DB RO.
    // setup:    insert a test edge with key EDGE_KEY.
    // teardown: delete the test edge.
    // Expected: DB-undef→401, DB-none→401, DB-ro→200, DB-rw→200
    name: `Read edge document (GET /_db/d/_api/gharial/${G}/edge/e/${EDGE_KEY})`,
    type: 'all',
    method: 'GET',
    path: `/_db/d/_api/gharial/${G}/edge/e/${EDGE_KEY}`,
    setup: async (ctx) => {
      await insertTestEdge(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestEdge(ctx);
    },
  },

  // ── POST /_db/d/_api/gharial/{graph}/edge/{definition} ───────────────────
  {
    // Insert a new edge document into g/e.
    // Auth: canUseGraph(RO) + canUseColl(RWDATA) → DB RO + COLL RWDATA on e.
    // setup:    delete stale EDGE_KEY so there is nothing to conflict with.
    // teardown: delete the edge regardless of whether the test succeeded.
    // Expected (no RBAC): DB-none→401, DB-ro + COLL-rw-data→201,
    //                     DB-ro + COLL-none/ro→403, …
    name: `Insert edge document (POST /_db/d/_api/gharial/${G}/edge/e)`,
    type: 'all',
    method: 'POST',
    path: `/_db/d/_api/gharial/${G}/edge/e`,
    body: { _key: EDGE_KEY, _from: 'c/k1', _to: 'c/k2' },
    setup: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/document/e/${EDGE_KEY}`);
    },
    teardown: async (ctx) => {
      await deleteTestEdge(ctx);
    },
  },

  // ── PUT /_db/d/_api/gharial/{graph}/edge/{definition} ────────────────────
  {
    // Replace an edge definition in graph g_apitest.
    // Auth: canUseGraph(RW) → DB RW.
    // Sends the same definition back (no-op replacement) so structure is
    // preserved regardless of whether the test user had write access.
    // setup:    create g_apitest with e_apitest edge def.
    // teardown: delete g_apitest + e_apitest.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200/202
    name: `Replace edge definition (PUT /_db/d/_api/gharial/${G_APITEST}/edge/${E_APITEST})`,
    type: ['admin', 'database'],
    method: 'PUT',
    path: `/_db/d/_api/gharial/${G_APITEST}/edge/${E_APITEST}`,
    body: { collection: E_APITEST, from: ['c'], to: ['c'] },
    setup: async (ctx) => {
      await createGapitest(ctx);
    },
    teardown: async (ctx) => {
      await deleteGapitest(ctx);
    },
  },

  // ── DELETE /_db/d/_api/gharial/{graph}/edge/{definition} ─────────────────
  {
    // Remove an edge definition from graph g_apitest.
    // Auth: canUseGraph(RW) → DB RW.
    // Removes e_apitest from the graph (dropCollection=false, so the
    // collection is demoted to an orphan vertex collection, not dropped).
    // teardown cleans up graph + collection in all cases.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200/202
    name: `Remove edge definition (DELETE /_db/d/_api/gharial/${G_APITEST}/edge/${E_APITEST})`,
    type: ['admin', 'database'],
    method: 'DELETE',
    path: `/_db/d/_api/gharial/${G_APITEST}/edge/${E_APITEST}?dropCollection=false`,
    setup: async (ctx) => {
      await createGapitest(ctx);
    },
    teardown: async (ctx) => {
      await deleteGapitest(ctx);
    },
  },

  // ── PUT /_db/d/_api/gharial/{graph}/edge/{definition}/{key} ──────────────
  {
    // Replace a single edge document in g/e.
    // Auth: canUseGraph(RO) + canUseColl(RWDATA) → DB RO + COLL RWDATA on e.
    // setup:    insert test edge EDGE_KEY.
    // teardown: delete EDGE_KEY.
    // Expected (no RBAC): DB-ro + COLL rw-data → 200; others → 4xx
    name: `Replace edge document (PUT /_db/d/_api/gharial/${G}/edge/e/${EDGE_KEY})`,
    type: 'all',
    method: 'PUT',
    path: `/_db/d/_api/gharial/${G}/edge/e/${EDGE_KEY}`,
    body: { _from: 'c/k2', _to: 'c/k3' },
    setup: async (ctx) => {
      await insertTestEdge(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestEdge(ctx);
    },
  },

  // ── PATCH /_db/d/_api/gharial/{graph}/edge/{definition}/{key} ────────────
  {
    // Partially update a single edge document in g/e.
    // Auth: canUseGraph(RO) + canUseColl(RWDATA) → DB RO + COLL RWDATA on e.
    // setup:    insert test edge EDGE_KEY.
    // teardown: delete EDGE_KEY.
    name: `Update edge document (PATCH /_db/d/_api/gharial/${G}/edge/e/${EDGE_KEY})`,
    type: 'all',
    method: 'PATCH',
    path: `/_db/d/_api/gharial/${G}/edge/e/${EDGE_KEY}`,
    body: { extra: 1 },
    setup: async (ctx) => {
      await insertTestEdge(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestEdge(ctx);
    },
  },

  // ── DELETE /_db/d/_api/gharial/{graph}/edge/{definition}/{key} ───────────
  {
    // Delete a single edge document from g/e.
    // Auth: canUseGraph(RO) + canUseColl(RWDATA) → DB RO + COLL RWDATA on e.
    // setup:    insert test edge EDGE_KEY.
    // teardown: delete EDGE_KEY (idempotent; 404 is acceptable).
    name: `Delete edge document (DELETE /_db/d/_api/gharial/${G}/edge/e/${EDGE_KEY})`,
    type: 'all',
    method: 'DELETE',
    path: `/_db/d/_api/gharial/${G}/edge/e/${EDGE_KEY}`,
    setup: async (ctx) => {
      await insertTestEdge(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestEdge(ctx);
    },
  },

  // ── GET /_db/d/_api/gharial/{graph}/vertex ────────────────────────────────
  {
    // List all vertex collections (incl. orphans) of graph 'g'.
    // Auth: canUseGraph(RO) → DB RO.
    // Expected: DB-undef→401, DB-none→401, DB-ro→200, DB-rw→200
    name: `List vertex collections (GET /_db/d/_api/gharial/${G}/vertex)`,
    type: 'all',
    method: 'GET',
    path: `/_db/d/_api/gharial/${G}/vertex`,
  },

  // ── POST /_db/d/_api/gharial/{graph}/vertex ───────────────────────────────
  {
    // Add an orphan vertex collection to graph g_apitest.
    // Auth: canUseGraph(RW) → DB RW.
    // setup:    create g_apitest + c_orphan_apitest collection (not yet in graph).
    // teardown: delete g_apitest + e_apitest + c_orphan_apitest.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200/202
    name: `Add orphan vertex collection (POST /_db/d/_api/gharial/${G_APITEST}/vertex)`,
    type: ['admin', 'database'],
    method: 'POST',
    path: `/_db/d/_api/gharial/${G_APITEST}/vertex`,
    body: { collection: C_ORPHAN },
    setup: async (ctx) => {
      await createGapitestForAddOrphan(ctx);
    },
    teardown: async (ctx) => {
      await deleteGapitestWithOrphan(ctx);
    },
  },

  // ── GET /_db/d/_api/gharial/{graph}/vertex/{collection}/{key} ────────────
  {
    // Read a single vertex document from g/c.
    // Auth: canUseGraph(RO) → DB RO.
    // Uses the existing document k1 inserted by global setup.
    // Expected: DB-undef→401, DB-none→401, DB-ro→200, DB-rw→200
    name: `Read vertex document (GET /_db/d/_api/gharial/${G}/vertex/c/k1)`,
    type: 'all',
    method: 'GET',
    path: `/_db/d/_api/gharial/${G}/vertex/c/k1`,
  },

  // ── POST /_db/d/_api/gharial/{graph}/vertex/{collection} ─────────────────
  {
    // Insert a new vertex document into g/c.
    // Auth: canUseGraph(RO) + canUseColl(RWDATA) → DB RO + COLL RWDATA on c.
    // setup:    delete stale VERTEX_KEY.
    // teardown: delete VERTEX_KEY (idempotent).
    // Expected (no RBAC): DB-ro + COLL rw-data → 201; others → 4xx
    name: `Insert vertex document (POST /_db/d/_api/gharial/${G}/vertex/c)`,
    type: 'all',
    method: 'POST',
    path: `/_db/d/_api/gharial/${G}/vertex/c`,
    body: { _key: VERTEX_KEY, value: 9999 },
    setup: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/document/c/${VERTEX_KEY}`);
    },
    teardown: async (ctx) => {
      await deleteTestVertex(ctx);
    },
  },

  // ── DELETE /_db/d/_api/gharial/{graph}/vertex/{collection} ───────────────
  {
    // Remove an orphan vertex collection from graph g_apitest.
    // Auth: canUseGraph(RW) → DB RW.
    // setup:    create g_apitest with c_orphan_apitest registered as orphan.
    // teardown: delete g_apitest + e_apitest + c_orphan_apitest.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200/202
    name: `Remove orphan vertex collection (DELETE /_db/d/_api/gharial/${G_APITEST}/vertex/${C_ORPHAN})`,
    type: ['admin', 'database'],
    method: 'DELETE',
    path: `/_db/d/_api/gharial/${G_APITEST}/vertex/${C_ORPHAN}?dropCollection=false`,
    setup: async (ctx) => {
      await createGapitestWithOrphan(ctx);
    },
    teardown: async (ctx) => {
      await deleteGapitestWithOrphan(ctx);
    },
  },

  // ── PUT /_db/d/_api/gharial/{graph}/vertex/{collection}/{key} ────────────
  {
    // Replace a single vertex document in g/c.
    // Auth: canUseGraph(RO) + canUseColl(RWDATA) → DB RO + COLL RWDATA on c.
    // setup:    insert test vertex VERTEX_KEY.
    // teardown: delete VERTEX_KEY.
    name: `Replace vertex document (PUT /_db/d/_api/gharial/${G}/vertex/c/${VERTEX_KEY})`,
    type: 'all',
    method: 'PUT',
    path: `/_db/d/_api/gharial/${G}/vertex/c/${VERTEX_KEY}`,
    body: { value: 10000 },
    setup: async (ctx) => {
      await insertTestVertex(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestVertex(ctx);
    },
  },

  // ── PATCH /_db/d/_api/gharial/{graph}/vertex/{collection}/{key} ──────────
  {
    // Partially update a single vertex document in g/c.
    // Auth: canUseGraph(RO) + canUseColl(RWDATA) → DB RO + COLL RWDATA on c.
    // setup:    insert test vertex VERTEX_KEY.
    // teardown: delete VERTEX_KEY.
    name: `Update vertex document (PATCH /_db/d/_api/gharial/${G}/vertex/c/${VERTEX_KEY})`,
    type: 'all',
    method: 'PATCH',
    path: `/_db/d/_api/gharial/${G}/vertex/c/${VERTEX_KEY}`,
    body: { extra: 42 },
    setup: async (ctx) => {
      await insertTestVertex(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestVertex(ctx);
    },
  },

  // ── DELETE /_db/d/_api/gharial/{graph}/vertex/{collection}/{key} ─────────
  {
    // Delete a single vertex document from g/c.
    // Auth: canUseGraph(RO) + canUseColl(RWDATA) → DB RO + COLL RWDATA on c.
    // setup:    insert test vertex VERTEX_KEY.
    // teardown: delete VERTEX_KEY (idempotent; 404 acceptable).
    name: `Delete vertex document (DELETE /_db/d/_api/gharial/${G}/vertex/c/${VERTEX_KEY})`,
    type: 'all',
    method: 'DELETE',
    path: `/_db/d/_api/gharial/${G}/vertex/c/${VERTEX_KEY}`,
    setup: async (ctx) => {
      await insertTestVertex(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestVertex(ctx);
    },
  },

];
