// Tests for the /_api/collection endpoint family.
//
// Handler: RestCollectionHandler (RestVocbaseBaseHandler)
// Mounted at: /_db/d/_api/collection (prefix)
//
// All tests use database 'd' (created by global setup) and collection 'c'
// (created by global setup).  Destructive operations (create, drop, rename)
// use a temporary collection 'c_apitest' so that 'c' is never removed or
// renamed by a test cell.
//
// Auth model
// ──────────
// RestVocbaseBaseHandler requires at least some DB access; requests from
// users with DB=U or DB=N reach the router but are rejected at the vocbase
// context level (401).
//
// Per-operation access levels (all operate on database 'd', collection 'c'
// unless stated otherwise):
//
//  GET /_api/collection                canSeeCollection  → AUTHEN (DB R/W)
//  GET /_api/collection/{name}         canUseCollection(Read)        → COLL RO
//  GET /_api/collection/{name}/checksum            "                 → COLL RO
//  GET /_api/collection/{name}/count              "                  → COLL RO
//  GET /_api/collection/{name}/figures            "                  → COLL RO
//  GET /_api/collection/{name}/properties         "                  → COLL RO
//  GET /_api/collection/{name}/revision           "                  → COLL RO
//  GET /_api/collection/{name}/shards             "  + cluster-only  → COLL RO
//  PUT /_api/collection/{name}/compact   canUseCollection(WriteMeta) → COLL RW
//  PUT /_api/collection/{name}/load      canUseCollection(Read)      → COLL RO
//  PUT /_api/collection/{name}/loadIndexesIntoMemory  (Read)         → COLL RO
//  PUT /_api/collection/{name}/properties  canUseCollection(WriteMeta)→ COLL RW
//  PUT /_api/collection/{name}/rename      canUseCollection(WriteMeta)→ COLL RW
//  PUT /_api/collection/{name}/responsibleShard  (Read) + coord-only → COLL RO
//  PUT /_api/collection/{name}/truncate  canUseCollection(WriteData) → COLL RWDATA
//  PUT /_api/collection/{name}/unload    canUseCollection(Read)      → COLL RO
//  POST /_api/collection               canCreateCollection           → COLL RW
//  DELETE /_api/collection/{name}      canDropCollection             → COLL RW

export default [

  // ── GET /_db/d/_api/collection ───────────────────────────────────────────
  // Lists all collections visible to the current user in database d.
  // The handler filters the list by canSeeCollection() – it never rejects
  // the request, just returns a smaller list.  Any user with DB access gets
  // 200; users with no DB access (DB=U or DB=N) get 401.
  {
    name: "List all collections in database d (GET /_db/d/_api/collection)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/collection",
  },

  // ── GET /_db/d/_api/collection/c ─────────────────────────────────────────
  // Returns metadata for collection c.
  // Requires canUseCollection(Read) → COLL RO.
  {
    name: "Get collection metadata (GET /_db/d/_api/collection/c)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/collection/c",
  },

  // ── GET /_db/d/_api/collection/c/checksum ────────────────────────────────
  // Returns a checksum for collection c.
  // Requires canUseCollection(Read) → COLL RO.
  {
    name: "Get collection checksum (GET /_db/d/_api/collection/c/checksum)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/collection/c/checksum",
  },

  // ── GET /_db/d/_api/collection/c/count ───────────────────────────────────
  // Returns the document count for collection c.
  // Requires canUseCollection(Read) → COLL RO.
  {
    name: "Get collection document count (GET /_db/d/_api/collection/c/count)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/collection/c/count",
  },

  // ── GET /_db/d/_api/collection/c/figures ─────────────────────────────────
  // Returns storage engine statistics for collection c.
  // Requires canUseCollection(Read) → COLL RO.
  {
    name: "Get collection figures (GET /_db/d/_api/collection/c/figures)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/collection/c/figures",
  },

  // ── GET /_db/d/_api/collection/c/properties ──────────────────────────────
  // Returns collection properties (replication factor, wait-for-sync, …).
  // Requires canUseCollection(Read) → COLL RO.
  {
    name: "Get collection properties (GET /_db/d/_api/collection/c/properties)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/collection/c/properties",
  },

  // ── GET /_db/d/_api/collection/c/revision ────────────────────────────────
  // Returns the current revision of collection c.
  // Requires canUseCollection(Read) → COLL RO.
  {
    name: "Get collection revision (GET /_db/d/_api/collection/c/revision)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/collection/c/revision",
  },

  // ── GET /_db/d/_api/collection/c/shards ──────────────────────────────────
  // Returns shard-to-server mapping for collection c.
  // Requires canUseCollection(Read) → COLL RO.
  // On single-server / non-cluster: returns 501 for users with read access;
  // users without access still get 401/403 first.
  {
    name: "Get collection shards (GET /_db/d/_api/collection/c/shards)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/collection/c/shards",
  },

  // ── PUT /_db/d/_api/collection/c/load ────────────────────────────────────
  // No-op since ArangoDB 3.9; still enforces canUseCollection(Read).
  // Requires COLL RO.
  {
    name: "Load collection into memory (PUT /_db/d/_api/collection/c/load)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/collection/c/load",
    body: {},
  },

  // ── PUT /_db/d/_api/collection/c/unload ──────────────────────────────────
  // No-op since ArangoDB 3.9; still enforces canUseCollection(Read).
  // Requires COLL RO.
  {
    name: "Unload collection from memory (PUT /_db/d/_api/collection/c/unload)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/collection/c/unload",
    body: {},
  },

  // ── PUT /_db/d/_api/collection/c/loadIndexesIntoMemory ───────────────────
  // Warms up indexes.  Requires canUseCollection(Read) → COLL RO.
  {
    name: "Load indexes into memory (PUT /_db/d/_api/collection/c/loadIndexesIntoMemory)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/collection/c/loadIndexesIntoMemory",
    body: {},
  },

  // ── PUT /_db/d/_api/collection/c/compact ─────────────────────────────────
  // Triggers a compaction run for collection c.
  // Requires canUseCollection(WriteMeta) → COLL RW.
  {
    name: "Compact collection (PUT /_db/d/_api/collection/c/compact)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/collection/c/compact",
    body: {},
  },

  // ── PUT /_db/d/_api/collection/c/properties ──────────────────────────────
  // Updates collection properties.  Body uses a no-effect change (sets
  // waitForSync to its current/default value) so repeated calls are safe.
  // Requires canUseCollection(WriteMeta) → COLL RW.
  {
    name: "Update collection properties (PUT /_db/d/_api/collection/c/properties)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/collection/c/properties",
    body: { waitForSync: false },
  },

  // ── PUT /_db/d/_api/collection/c/responsibleShard ────────────────────────
  // Returns the shard responsible for the given document key.
  // Requires canUseCollection(Read) → COLL RO.
  // On single-server / non-coordinator: returns 403/501 (cluster-only check
  // fires after lookup, so auth is checked first).
  {
    name: "Get responsible shard (PUT /_db/d/_api/collection/c/responsibleShard)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/collection/c/responsibleShard",
    body: { _key: "testkey" },
  },

  // ── PUT /_db/d/_api/collection/c_apitest/truncate ────────────────────────
  // Truncates (empties) collection c_apitest.
  // Uses c_apitest to avoid destroying documents in c.
  // Requires canUseCollection(WriteData) → COLL RWDATA.
  {
    name: "Truncate collection (PUT /_db/d/_api/collection/c/truncate)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/collection/c/truncate",
    body: {},
  },

  // ── POST /_db/d/_api/collection ──────────────────────────────────────────
  // Creates a new collection c_apitest.
  // Requires canCreateCollection → DB RW.
  {
    name: "Create collection (POST /_db/d/_api/collection)",
    type: ["admin", "database"],
    method: "POST",
    path: "/_db/d/_api/collection",
    body: { name: "c_apitest" },
    setup: async (ctx) => {
      // Remove any stale collection so each cell starts clean.
      await ctx.request('DELETE', '/_db/d/_api/collection/c_apitest');
    },
    teardown: async (ctx) => {
      // Remove the collection whether or not the test user succeeded.
      await ctx.request('DELETE', '/_db/d/_api/collection/c_apitest');
    },
  },

  // ── DELETE /_db/d/_api/collection/c_apitest ──────────────────────────────
  // Drops collection c_apitest.
  // Uses c_apitest to avoid dropping c (which other tests depend on).
  // Requires canDropCollection → COLL RW.
  {
    name: "Drop collection (DELETE /_db/d/_api/collection/c_apitest)",
    type: ["admin", "database"],
    method: "DELETE",
    path: "/_db/d/_api/collection/c_apitest",
    setup: async (ctx) => {
      // Ensure c_apitest exists before the test user tries to drop it.
      await ctx.request('DELETE', '/_db/d/_api/collection/c_apitest');
      await ctx.request('POST',   '/_db/d/_api/collection', { name: 'c_apitest' });
    },
    teardown: async (ctx) => {
      // Clean up if the test user lacked permission to drop.
      await ctx.request('DELETE', '/_db/d/_api/collection/c_apitest');
    },
  },

];
