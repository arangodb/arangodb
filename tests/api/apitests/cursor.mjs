// Tests for the /_api/cursor endpoint family.
//
// Handler: RestCursorHandler
// Mounted at: /_db/d/_api/cursor (prefix)
//
// All tests use database 'd' (created by global setup) and collection 'c'
// (which contains 100 documents {"Hallo": 1} … {"Hallo": 100}).
//
// Auth model
// ──────────
// Creating and advancing cursors requires at least DB read access and
// collection read access.
//
//  POST /_api/cursor          – execute AQL query, returns results   → COLL RO
//  POST /_api/cursor/<id>     – advance cursor (fetch next batch)    → COLL RO
//  PUT  /_api/cursor/<id>     – advance cursor (fetch next batch)    → COLL RO
//  DELETE /_api/cursor/<id>   – discard open cursor                  → COLL RO

export default [

  // ── POST /_db/d/_api/cursor (single batch) ───────────────────────────────
  // Executes a simple AQL query that returns all 100 documents in one batch.
  // With the default batchSize (≥ 100) hasMore is false and no cursor is left
  // open, so no setup or teardown is needed.
  {
    name: "Execute AQL query, single batch (POST /_db/d/_api/cursor)",
    type: "all",
    method: "POST",
    path: "/_db/d/_api/cursor",
    body: { query: "FOR d IN c RETURN d" },
  },

  // ── POST /_db/d/_api/cursor (multi-batch, batchSize 10) ──────────────────
  // Creates a cursor by limiting the batch size to 10 (well below the 100
  // documents in collection c).  hasMore is true and the response contains
  // a cursor id.
  // teardown: if the request succeeded (201) the cursor is still open; discard
  //           it via ctx.response.body.id so no cursors are leaked.
  {
    name: "Execute AQL query, create cursor with batchSize 10 (POST /_db/d/_api/cursor)",
    type: "all",
    method: "POST",
    path: "/_db/d/_api/cursor",
    body: { query: "FOR d IN c RETURN d", batchSize: 10 },
    teardown: async (ctx) => {
      if (ctx.response.status === 201 && ctx.response.body && ctx.response.body.id) {
        await ctx.request('DELETE', `/_db/d/_api/cursor/${ctx.response.body.id}`);
      }
    },
  },

  // ── POST /_db/d/_api/cursor/<id> (advance cursor) ────────────────────────
  // Fetches the next batch from an open cursor.
  // setup:    superuser creates a fresh cursor (batchSize 10) and stores
  //           its id so the path can be interpolated.
  // teardown: superuser discards the cursor; this is a no-op if the test
  //           user already exhausted or the cursor no longer exists.
  {
    name: "Advance cursor – next batch (POST /_db/d/_api/cursor/<id>)",
    type: "all",
    method: "POST",
    path: "/_db/d/_api/cursor/${ctx.data.id}",
    setup: async (ctx) => {
      const resp = await ctx.request('POST', '/_db/d/_api/cursor',
        { query: 'FOR d IN c RETURN d', batchSize: 10 });
      if (!resp.body || !resp.body.id) {
        throw new Error(`Failed to create cursor: ${resp.status} - ${JSON.stringify(resp.body)}`);
      }
      return { id: resp.body.id };
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/cursor/${ctx.data.id}`);
    },
  },

  // ── PUT /_db/d/_api/cursor/<id> (advance cursor) ─────────────────────────
  // Same as POST /<id> but using the PUT verb (both verbs are supported by
  // ArangoDB for advancing a cursor).
  // setup:    superuser creates a fresh cursor (batchSize 10) and stores
  //           its id.
  // teardown: superuser discards the cursor.
  {
    name: "Advance cursor – next batch (PUT /_db/d/_api/cursor/<id>)",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/cursor/${ctx.data.id}",
    setup: async (ctx) => {
      const resp = await ctx.request('POST', '/_db/d/_api/cursor',
        { query: 'FOR d IN c RETURN d', batchSize: 10 });
      if (!resp.body || !resp.body.id) {
        throw new Error(`Failed to create cursor: ${resp.status} - ${JSON.stringify(resp.body)}`);
      }
      return { id: resp.body.id };
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/cursor/${ctx.data.id}`);
    },
  },

  // ── DELETE /_db/d/_api/cursor/<id> (discard cursor) ──────────────────────
  // Discards an open cursor before it is fully consumed.
  // setup:    superuser creates a fresh cursor (batchSize 10) and stores
  //           its id.
  // teardown: superuser attempts to discard the cursor; errors are ignored
  //           because a successful test user will have already deleted it.
  {
    name: "Delete cursor (DELETE /_db/d/_api/cursor/<id>)",
    type: "all",
    method: "DELETE",
    path: "/_db/d/_api/cursor/${ctx.data.id}",
    setup: async (ctx) => {
      const resp = await ctx.request('POST', '/_db/d/_api/cursor',
        { query: 'FOR d IN c RETURN d', batchSize: 10 });
      if (!resp.body || !resp.body.id) {
        throw new Error(`Failed to create cursor: ${resp.status} - ${JSON.stringify(resp.body)}`);
      }
      return { id: resp.body.id };
    },
    teardown: async (ctx) => {
      // Ignore errors – the cursor may already be gone if the test succeeded.
      await ctx.request('DELETE', `/_db/d/_api/cursor/${ctx.data.id}`);
    },
  },

];
