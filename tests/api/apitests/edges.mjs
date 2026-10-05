// Tests for the /_api/edges endpoint family.
//
// Handler: RestEdgesHandler
// Mounted at: /_db/<name>/_api/edges (prefix)
//
// Both endpoints return the set of edges in a given edge collection that are
// connected to a specified vertex.  They require at least read access to the
// edge collection (canUseCollection(Read) via transaction).
//
// Auth model (without RBAC)
// ─────────────────────────
//   DB=undef (U) → 401   DB=none (N) → 401
//   DB=ro   (R) → COLL RO required (i.e. COLL=none → 403, COLL=ro/rw → 200)
//   DB=rw   (W) → same as DB=ro for this handler
//   (admin users AU/AN have no access to d → 401; AR/AW/SU use their d-access)
//
// The global setup provides database 'd', document collection 'c' (100 docs,
// keys k1–k100), edge collection 'e' (100 circular edges c/k1→c/k2 … c/k100→
// c/k1), and named graph 'g'.  No test-specific setup or teardown is needed.

export default [

  // ── GET /_db/d/_api/edges/e ───────────────────────────────────────────────
  {
    // Return all edges connected to vertex c/k1 in collection e (GET form).
    // The vertex ID is supplied as the query parameter ?vertex=c/k1.
    // Auth: canUseCollection(Read) on e → COLL RO.
    // Expected: DB-undef→401, DB-none→401,
    //           DB-ro + COLL-none→403, DB-ro + COLL-ro/rw→200,
    //           DB-rw + COLL-none→403, DB-rw + COLL-ro/rw→200
    name: 'Return edges for a vertex – GET form (GET /_db/d/_api/edges/e?vertex=c/k1)',
    type: 'all',
    method: 'GET',
    path: '/_db/d/_api/edges/e?vertex=c%2Fk1&direction=any',
  },

  // ── POST /_db/d/_api/edges/e ──────────────────────────────────────────────
  {
    // Return all edges connected to vertex c/k1 in collection e (POST form).
    // The vertex IDs are supplied in the request body.
    // Auth: canUseCollection(Read) on e → COLL RO.
    // Expected: same as GET variant above.
    name: 'Return edges for a vertex – POST form (POST /_db/d/_api/edges/e)',
    type: 'all',
    method: 'POST',
    path: '/_db/d/_api/edges/e',
    body: [ 'c/k1', 'c/k2' ],
  },

];
