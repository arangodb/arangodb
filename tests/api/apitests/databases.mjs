// Tests for the /_api/database endpoint family.
//
// Handler: RestDatabaseHandler
// Mounted at: /_api/database (prefix)
//
// GET tests use type ["admin", "database"] so the result matrix covers both
// the admin users (AU/AN/AR/AW with varying _system access) and the
// database-level permission matrix (users with varying access to database 'd').
//
// POST and DELETE tests are admin-only: creating and dropping databases is a
// server-administration operation that requires _system RW access.  They
// operate on a temporary database 'd2' so that 'd' (used by all other tests)
// is never touched.
//
// Auth model
// ──────────
//  GET /_api/database                    – list all databases (_system only)  → _sys RO
//  GET /_api/database/current            – info about the current database    → DB  RO
//  GET /_api/database/user               – databases accessible to this user  → (any)
//  GET /_api/database/shardStatistics    – shard statistics (cluster-only)    → DB  RO
//  POST /_api/database                   – create a new database              → _sys RW
//  DELETE /_api/database/{name}          – drop a database                    → _sys RW

export default [

  // ── GET /_db/_system/_api/database ───────────────────────────────────────
  // Lists all databases on the server.  Only accessible from the _system
  // context; requires at least RO access to _system.
  // Database-level users (who only have access to 'd', not '_system') will
  // receive 401 across the board – that is the expected and informative result.
  {
    name: "List all databases (GET /_db/_system/_api/database)",
    type: ["admin"],
    method: "GET",
    path: "/_db/_system/_api/database",
  },

  // ── GET /_db/d/_api/database/current ─────────────────────────────────────
  // Returns metadata about the current database ('d').
  // Requires at least DB read access.
  {
    name: "Get current database info (GET /_db/d/_api/database/current)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/database/current",
  },

  // ── GET /_db/d/_api/database/user ────────────────────────────────────────
  // Returns the list of databases accessible to the authenticated user.
  // Available from any database context; does not require elevated access.
  {
    name: "List databases accessible to current user (GET /_db/d/_api/database/user)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/database/user",
  },

  // ── GET /_db/d/_api/database/shardStatistics ──────────────────────────────
  // Returns shard statistics for the current database.
  // On a single server this endpoint returns 501 (Not Implemented); on a
  // cluster coordinator it requires at least DB read access.
  {
    name: "Get shard statistics (GET /_db/d/_api/database/shardStatistics)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/database/shardStatistics",
  },

  // ── POST /_db/_system/_api/database (create database) ────────────────────
  // Creates a new database 'd2'.  Requires _system RW access (admin only).
  // setup:    remove any stale 'd2' so each matrix cell starts clean.
  // teardown: drop 'd2' whether or not the test user succeeded, so subsequent
  //           cells find a consistent state.
  {
    name: "Create database d2 (POST /_db/_system/_api/database)",
    type: "admin",
    method: "POST",
    path: "/_db/_system/_api/database",
    body: { name: "d2" },
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/_system/_api/database/d2');
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/_system/_api/database/d2');
    },
  },

  // ── DELETE /_db/_system/_api/database/d2 (drop database) ─────────────────
  // Drops database 'd2'.  Requires _system RW access (admin only).
  // setup:    ensure 'd2' exists so the test user has something to drop.
  // teardown: clean up 'd2' in case the test user lacked permission to drop it.
  {
    name: "Drop database d2 (DELETE /_db/_system/_api/database/d2)",
    type: "admin",
    method: "DELETE",
    path: "/_db/_system/_api/database/d2",
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/_system/_api/database/d2');
      const r = await ctx.request('POST', '/_db/_system/_api/database', { name: 'd2' });
      if (r.status < 200 || r.status >= 300) {
        throw new Error(`setup: failed to create d2: ${r.status} - ${JSON.stringify(r.body)}`);
      }
    },
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/_system/_api/database/d2');
    },
  },

];
