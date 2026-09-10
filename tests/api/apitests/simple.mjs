// Tests for the /_api/simple endpoint family.
//
// Handler: RestSimpleQueryHandler / RestSimpleHandler
// Mounted at: /_db/d/_api/simple (prefix)
//
// All tests use database 'd' (created by global setup) and collection 'c'
// (which contains 100 documents {"Hallo": 1} … {"Hallo": 100}).
//
// Auth model
// ──────────
// All simple-query endpoints require at least collection read access.
// remove-by-keys additionally requires collection write-data access.
//
//  PUT /_api/simple/all           – return all documents         → COLL RO
//  PUT /_api/simple/all-keys      – return all keys/IDs/paths    → COLL RO
//  PUT /_api/simple/by-example    – return docs matching example → COLL RO
//  PUT /_api/simple/lookup-by-keys – return docs by key list     → COLL RO
//  PUT /_api/simple/remove-by-keys – remove docs by key list     → COLL RWDATA

export default [

  {
    // PUT /_api/simple/all
    // Returns all documents in the collection.
    // limit:1 keeps the result set tiny and avoids unnecessary data transfer.
    // Expected: all authenticated users with COLL RO → 201 (result cursor).
    name: "Simple all (PUT /_api/simple/all)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/simple/all",
    body: { collection: "c", limit: 1 },
  },

  {
    // PUT /_api/simple/all-keys
    // Returns all document keys (or IDs/paths, depending on ?type=).
    // limit:1 keeps the result set tiny.
    // Expected: all authenticated users with COLL RO → 201 (result cursor).
    name: "Simple all-keys (PUT /_api/simple/all-keys)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/simple/all-keys",
    body: { collection: "c", limit: 1 },
  },

  {
    // PUT /_api/simple/by-example
    // Returns documents matching a given example object.
    // {"Hallo": 1} matches exactly one document in the collection.
    // Expected: all authenticated users with COLL RO → 201 (result cursor).
    name: "Simple by-example (PUT /_api/simple/by-example)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/simple/by-example",
    body: { collection: "c", example: { Hallo: 1 } },
  },

  {
    // PUT /_api/simple/lookup-by-keys
    // Looks up documents by their _key values.
    // Using a nonexistent key produces an empty result — no documents are
    // modified and no teardown is needed.
    // Expected: all authenticated users with COLL RO → 200 (document array).
    name: "Simple lookup-by-keys (PUT /_api/simple/lookup-by-keys)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/simple/lookup-by-keys",
    body: { collection: "c", keys: ["nonexistent-key-apitester-99999"] },
  },

  {
    // PUT /_api/simple/remove-by-keys
    // Removes documents identified by their _key values.
    // Using a nonexistent key is a no-op — the collection remains intact.
    // Expected: all authenticated users with COLL RWDATA → 200.
    name: "Simple remove-by-keys (PUT /_api/simple/remove-by-keys)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/simple/remove-by-keys",
    body: { collection: "c", keys: ["nonexistent-key-apitester-99999"] },
  },

];
