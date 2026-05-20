// Tests for the /_api/transaction endpoint family.
//
// Handler: RestTransactionHandler
// Mounted at: /_db/d/_api/transaction (prefix)
//
// All tests use database 'd' (created by global setup) and collection 'c'
// (which contains 100 documents {"Hallo": 1} … {"Hallo": 100}).
//
// Auth model
// ──────────
//  GET    /_api/transaction          – list ongoing transactions        → AUTHEN (own user or SUPER)
//  GET    /_api/transaction/{id}     – get transaction state            → AUTHEN
//  GET    /_api/transaction/history  – transaction history              → SUPER (maintainer builds only)
//  POST   /_api/transaction          – run JS transaction               → AUTHEN (V8 required)
//  POST   /_api/transaction/begin    – begin stream transaction         → AUTHEN
//  PUT    /_api/transaction/{id}     – commit stream transaction        → AUTHEN (same user or SUPER)
//  DELETE /_api/transaction/{id}     – abort stream transaction         → AUTHEN (same user or SUPER)
//  DELETE /_api/transaction/write    – abort all write transactions     → AUTHEN (same user or SUPER)
//  DELETE /_api/transaction/history  – clear transaction history        → SUPER (maintainer builds only)

export default [

  // ── GET /_db/d/_api/transaction (list all) ───────────────────────────────
  // Lists ongoing stream transactions visible to the calling user.
  // A non-superuser sees only their own transactions; a superuser sees all.
  // No setup or teardown needed — the result may simply be an empty list.
  {
    name: "List ongoing transactions (GET /_db/d/_api/transaction)",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/transaction",
  },

  // ── GET /_db/d/_api/transaction/{id} (get state) ─────────────────────────
  // Returns the status of a specific stream transaction.
  // setup:    superuser begins a read-only transaction on collection 'c'
  //           and stores its id so the path can be interpolated.
  // teardown: superuser aborts the transaction (always, in case the test
  //           user could not commit it or it was never modified).
  {
    name: "Get transaction state (GET /_db/d/_api/transaction/{id})",
    type: "all",
    method: "GET",
    path: "/_db/d/_api/transaction/${ctx.data.id}",
    setup: async (ctx) => {
      const resp = await ctx.request('POST', '/_db/d/_api/transaction/begin',
        { collections: { read: ['c'] } });
      if (!resp.body || !resp.body.result || !resp.body.result.id) {
        throw new Error(`Failed to begin transaction: ${resp.status} - ${JSON.stringify(resp.body)}`);
      }
      return { id: resp.body.result.id };
    },
    teardown: async (ctx) => {
      // Abort unconditionally — a GET does not change transaction state.
      await ctx.request('DELETE', `/_db/d/_api/transaction/${ctx.data.id}`);
    },
  },

  // ── POST /_db/d/_api/transaction (JS transaction) ────────────────────────
  // Executes a JavaScript transaction.  Requires a V8 context to be available.
  // On deployments without V8 the server returns 503; that still demonstrates
  // correct auth behaviour before reaching V8.
  // The action function simply reads from collection 'c', so no cleanup is
  // needed.
  {
    name: "Run JS transaction, read from c (POST /_db/d/_api/transaction)",
    type: "all",
    method: "POST",
    path: "/_db/d/_api/transaction",
    body: {
      collections: { read: ['c'] },
      action: "function () { return 1; }",
    },
  },

  // ── POST /_db/d/_api/transaction/begin (begin stream transaction) ─────────
  // Begins a new stream transaction that reads from collection 'c'.
  // teardown: if the request succeeded (status 201) the transaction is still
  //           open; abort it so it does not linger.
  {
    name: "Begin read stream transaction (POST /_db/d/_api/transaction/begin)",
    type: "all",
    method: "POST",
    path: "/_db/d/_api/transaction/begin",
    body: { collections: { read: ['c'] } },
    teardown: async (ctx) => {
      if (ctx.response.status === 201 &&
          ctx.response.body &&
          ctx.response.body.result &&
          ctx.response.body.result.id) {
        await ctx.request('DELETE',
          `/_db/d/_api/transaction/${ctx.response.body.result.id}`);
      }
    },
  },

  // ── PUT /_db/d/_api/transaction/{id} (commit) ────────────────────────────
  // Commits an open stream transaction.
  // setup:    superuser begins a write transaction on 'c' and writes one
  //           document with a fixed key so we can clean it up deterministically.
  // teardown: if the test committed (status 200) the document now exists —
  //           delete it.  If the test did not commit (e.g. insufficient
  //           permission) the transaction is still open — abort it.
  {
    name: "Commit write stream transaction (PUT /_db/d/_api/transaction/{id})",
    type: "all",
    method: "PUT",
    path: "/_db/d/_api/transaction/${ctx.data.id}",
    setup: async (ctx) => {
      const beginResp = await ctx.request('POST', '/_db/d/_api/transaction/begin',
        { collections: { write: ['c'] } });
      if (!beginResp.body || !beginResp.body.result || !beginResp.body.result.id) {
        throw new Error(`Failed to begin write transaction: ${beginResp.status} - ${JSON.stringify(beginResp.body)}`);
      }
      const txId = beginResp.body.result.id;
      // Write a document with a prescribed key inside the transaction.
      const writeResp = await ctx.request('POST', '/_db/d/_api/document/c',
        { _key: 'apitester-trx-doc', Hallo: 999 },
        { 'x-arango-trx-id': txId });
      if (writeResp.status !== 201 && writeResp.status !== 202) {
        // Roll back and abort — don't leave the transaction dangling.
        await ctx.request('DELETE', `/_db/d/_api/transaction/${txId}`);
        throw new Error(`Failed to write document in transaction: ${writeResp.status} - ${JSON.stringify(writeResp.body)}`);
      }
      return { id: txId };
    },
    teardown: async (ctx) => {
      if (ctx.response.status === 200) {
        // Transaction was committed — the document now exists, delete it.
        await ctx.request('DELETE', '/_db/d/_api/document/c/apitester-trx-doc');
      } else {
        // Transaction was not committed — abort it (may already be gone on error).
        await ctx.request('DELETE', `/_db/d/_api/transaction/${ctx.data.id}`);
      }
    },
  },

  // ── DELETE /_db/d/_api/transaction/{id} (abort) ───────────────────────────
  // Aborts an open stream transaction.
  // setup:    superuser begins a write transaction on 'c' and stores its id.
  //           No document is written, so no document cleanup is ever needed.
  // teardown: if the test user lacked permission (status != 200), the
  //           transaction is still open — abort it now.
  {
    name: "Abort write stream transaction (DELETE /_db/d/_api/transaction/{id})",
    type: "all",
    method: "DELETE",
    path: "/_db/d/_api/transaction/${ctx.data.id}",
    setup: async (ctx) => {
      const resp = await ctx.request('POST', '/_db/d/_api/transaction/begin',
        { collections: { write: ['c'] } });
      if (!resp.body || !resp.body.result || !resp.body.result.id) {
        throw new Error(`Failed to begin write transaction: ${resp.status} - ${JSON.stringify(resp.body)}`);
      }
      return { id: resp.body.result.id };
    },
    teardown: async (ctx) => {
      if (ctx.response.status !== 200) {
        // Test user could not abort — do it as superuser now.
        await ctx.request('DELETE', `/_db/d/_api/transaction/${ctx.data.id}`);
      }
    },
  },

  // ── DELETE /_db/d/_api/transaction/write (abort all write transactions) ───
  // Aborts all write transactions of the calling user (or all users if
  // superuser).  Idempotent — safe to run even when there are none.
  // No setup or teardown needed.
  {
    name: "Abort all write transactions (DELETE /_db/d/_api/transaction/write)",
    type: "all",
    method: "DELETE",
    path: "/_db/d/_api/transaction/write",
  },

];
