// Miscellaneous API tests covering a wide range of /_api/* endpoints.
//
// Auth abbreviations used in comments below:
//   canUseHard(X)   → without --server.harden: AUTHEN; with harden: RW on _system
//   canUseAdmin(X)  → canUseAdminAction(AdminX) → without RBAC: RW on _system
//   isSuperuser     → JWT token with empty preferred_username
//   AUTHEN          → any authenticated user, no further check
//   _system         → must be called in the _system database context
//   OPEN            → no authentication required at all
//
// Type guide:
//   "all"                  → collection, database, and admin test columns
//   ["admin","database"]   → database and admin columns (e.g. needs _system ctx)
//   "admin"                → admin column only (admin permission or SUPER)
//   "database"             → database and admin columns (needs DB write)

export default [

  // ── /_api/document-state ─────────────────────────────────────────────────
  // Handler: RestDocumentStateHandler
  // Only registered when replication2 is enabled and running in cluster mode;
  // on a single server or coordinator without replication2 the route returns 404.
  // Auth: GET → AdminReadReplicatedLog; POST/DELETE → AdminWriteReplicatedLog.
  // A bogus numeric state-id (99999) causes a logical error (404/400) after
  // auth passes — safe and requires no setup/teardown.

  {
    // GET /_api/document-state/<state-id>/shards
    // Lists shards associated with the replicated-state machine.
    // Expected: AU→401/403, AN/AR/AW→403, SU→200 or 4xx (state not found)
    name: "Document-state get shards (GET /_api/document-state/99999/shards)",
    type: "admin",
    method: "GET",
    path: "/_db/d/_api/document-state/99999/shards",
  },

  {
    // POST /_api/document-state/<state-id>/snapshot/start
    // Starts a document-state snapshot.  Body {} is accepted; logical error
    // fires after auth if the state-id does not exist.
    // Expected: AU→401/403, AN/AR/AW→403, SU→200 or 4xx (state not found)
    name: "Document-state start snapshot (POST /_api/document-state/99999/snapshot/start)",
    type: "admin",
    method: "POST",
    path: "/_db/d/_api/document-state/99999/snapshot/start",
    body: {},
  },

  {
    // DELETE /_api/document-state/<state-id>/snapshot/finish/<snapshot-id>
    // Finishes (closes) a document-state snapshot.
    // Expected: AU→401/403, AN/AR/AW→403, SU→200 or 4xx (state not found)
    name: "Document-state finish snapshot (DELETE /_api/document-state/99999/snapshot/finish/99999)",
    type: "admin",
    method: "DELETE",
    path: "/_db/d/_api/document-state/99999/snapshot/finish/99999",
  },

  // ── /_api/endpoint ──────────────────────────────────────────────────────

  {
    // GET /_api/endpoint
    // Handler: RestEndpointHandler
    // Auth: AUTHEN, _system — any authenticated user, but only in _system
    // context.  Collection-level users without _system access will see 403.
    // Expected: AU→403, AN→403 (no _system), AR/AW→200 or 403, SU→200
    name: "List endpoints (GET /_api/endpoint)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/_system/_api/endpoint",
  },

  // ── /_api/engine ────────────────────────────────────────────────────────

  {
    // GET /_api/engine
    // Handler: RestEngineHandler
    // Auth: canUseHard(MonitoringInternal) — without --server.harden: AUTHEN;
    // with --server.harden: requires RW on _system.
    // On a standard (non-hardened) server all authenticated users → 200.
    name: "Engine info (GET /_api/engine)",
    type: "admin",
    method: "GET",
    path: "/_db/d/_api/engine",
  },

  {
    // GET /_api/engine/stats
    // Handler: RestEngineHandler
    // Auth: canUseHard(MonitoringInternal) — same as /_api/engine above.
    name: "Engine stats (GET /_api/engine/stats)",
    type: "admin",
    method: "GET",
    path: "/_db/d/_api/engine/stats",
  },

  // ── /_api/explain ────────────────────────────────────────────────────────

  {
    // POST /_api/explain
    // Handler: RestExplainHandler
    // Auth: canUseCollection(Read) via trx — AUTHEN + COLL RO.
    // Uses collection 'c' in database 'd' (created by global setup).
    // The explain call only produces a query plan; it does not execute.
    // Expected: all authenticated users with at least RO on 'c' → 200.
    name: "Explain AQL query (POST /_api/explain)",
    type: "all",
    method: "POST",
    path: "/_db/d/_api/explain",
    body: { query: "FOR d IN c RETURN d" },
  },

  // ── /_api/key-generators ─────────────────────────────────────────────────

  {
    // GET /_api/key-generators
    // Handler: RestKeyGeneratorsHandler
    // Auth: AUTHEN — any authenticated user.
    // Returns the list of available key generator types.
    name: "Key generators (GET /_api/key-generators)",
    type: ["database", "admin"],
    method: "GET",
    path: "/_db/d/_api/key-generators",
  },

  // ── /_api/log ────────────────────────────────────────────────────────────
  // All three verbs are replication2 + cluster only.  On a single server
  // without replication2 the endpoint is not registered and all users see
  // 404 (route not found) before any auth check can fire.
  // On a cluster with replication2:
  //   GET: AdminReadReplicatedLog  → AU→403, AN→403, AR→403, AW→200, SU→200
  //   POST/DELETE: AdminWriteReplicatedLog → same pattern

  {
    // GET /_api/log — list replicated logs
    name: "List replicated logs (GET /_api/log)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/log",
  },

  {
    // POST /_api/log — create a replicated log
    // Body {} is intentionally minimal and will fail body validation after
    // the auth check, producing a 400 for authorised users.
    name: "Create replicated log (POST /_api/log)",
    type: ["admin", "database"],
    method: "POST",
    path: "/_db/d/_api/log",
    body: {},
  },

  {
    // DELETE /_api/log — delete a replicated log
    // Sending no body causes the handler to return 400 for authorised users
    // (missing required fields), while unauthorised users see 403.
    name: "Delete replicated log (DELETE /_api/log)",
    type: ["admin", "database"],
    method: "DELETE",
    path: "/_db/d/_api/log",
  },

  // ── /_api/log-internal ───────────────────────────────────────────────────

  {
    // GET /_api/log-internal
    // Handler: RestLogInternalHandler
    // Auth: isSuperuser — SUPER only.
    // Also replication2 + cluster only; on single server all → 404.
    // On cluster: AU→401, AN→401, AR→403, AW→403, SU→200 (or 404 if
    // the feature is not enabled).
    name: "Internal log state (GET /_api/log-internal)",
    type: "admin",
    method: "GET",
    path: "/_db/d/_api/log-internal",
  },

  // ── /_api/query/* ────────────────────────────────────────────────────────

  {
    // GET /_api/query/slow
    // Handler: RestQueryHandler
    // Auth: AUTHEN (returns slow queries for the current database).
    // With ?all=true additionally requires _system + isSuperuser.
    // Expected: all authenticated users → 200.
    name: "Slow query list (GET /_api/query/slow)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/query/slow",
  },

  {
    // GET /_api/query/current
    // Handler: RestQueryHandler
    // Auth: AUTHEN (returns currently running queries for this database).
    // Expected: all authenticated users → 200.
    name: "Current queries (GET /_api/query/current)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/query/current",
  },

  {
    // GET /_api/query/properties
    // Handler: RestQueryHandler
    // Auth: AUTHEN — any authenticated user.
    // Returns AQL query tracking properties for the current database.
    name: "AQL query properties (GET /_api/query/properties)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/query/properties",
  },

  {
    // GET /_api/query/registry
    // Handler: RestQueryHandler
    // Auth: isSuperuser — SUPER only.
    // Returns the global query registry (all running queries, all DBs).
    // Expected: AU→403, AN→403, AR→403, AW→403, SU→200
    name: "Query registry (GET /_api/query/registry)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/query/registry",
  },

  {
    // GET /_api/query/rules
    // Handler: RestQueryHandler
    // Auth: AUTHEN — any authenticated user.
    // Returns the list of available AQL optimiser rules.
    name: "AQL optimiser rules (GET /_api/query/rules)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/query/rules",
  },

  {
    // POST /_api/query
    // Validates (parses) an AQL query without executing it.
    // Auth: AUTHEN — any authenticated user.
    // Expected: AU→401/403, others→200 (with parsed query info)
    name: "Validate AQL query (POST /_api/query)",
    type: "all",
    method: "POST",
    path: "/_db/d/_api/query",
    body: { query: "FOR d IN c RETURN d" },
  },

  {
    // DELETE /_api/query/{id}
    // Kills a running query by ID.
    // Auth: AUTHEN; killing another user's query requires _system + SUPER.
    // A nonexistent ID → 404 after auth check — safe and requires no setup.
    // Expected: AU→401/403, others→404 (no such query running)
    name: "Kill query by nonexistent id (DELETE /_api/query/nonexistent)",
    type: "all",
    method: "DELETE",
    path: "/_db/d/_api/query/nonexistent-query-apitester-99999",
  },

  {
    // DELETE /_api/query/slow
    // Clears the slow query log for the current database.
    // Auth: AUTHEN; clearing all DBs requires _system + SUPER.
    // Expected: AU→401/403, others→200
    name: "Clear slow query log (DELETE /_api/query/slow)",
    type: "all",
    method: "DELETE",
    path: "/_db/d/_api/query/slow",
  },

  // ── /_api/query-cache/* ──────────────────────────────────────────────────

  {
    // GET /_api/query-cache/entries
    // Handler: RestQueryCacheHandler
    // Auth: AUTHEN — any authenticated user.
    // Returns cached query results for the current database.
    name: "Query cache entries (GET /_api/query-cache/entries)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/query-cache/entries",
  },

  {
    // GET /_api/query-cache/properties
    // Handler: RestQueryCacheHandler
    // Auth: AUTHEN — any authenticated user.
    // Returns the AQL query result cache configuration.
    name: "Query cache properties (GET /_api/query-cache/properties)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/query-cache/properties",
  },

  {
    // PUT /_api/query-cache/properties
    // Handler: RestQueryCacheHandler
    // Auth: _system + canUseAdmin(AdminQueryCache) → RW on _system.
    // Sending mode "off" disables the cache — idempotent and safe.
    // Expected: AU→403, AN→403, AR→403, AW→200, SU→200
    name: "Update query cache properties (PUT /_api/query-cache/properties)",
    type: ["admin", "database"],
    method: "PUT",
    path: "/_db/_system/_api/query-cache/properties",
    body: { mode: "off" },
  },

  {
    // DELETE /_api/query-cache
    // Handler: RestQueryCacheHandler
    // Auth: _system + canUseAdmin(AdminQueryCache) → RW on _system.
    // Clears all cached query results — safe and idempotent.
    // Expected: AU→403, AN→403, AR→403, AW→200, SU→200
    name: "Clear query cache (DELETE /_api/query-cache)",
    type: ["admin", "database"],
    method: "DELETE",
    path: "/_db/_system/_api/query-cache",
  },

  // ── /_api/query-plan-cache ───────────────────────────────────────────────

  {
    // GET /_api/query-plan-cache
    // Handler: RestQueryPlanCacheHandler
    // Auth: AUTHEN — results filtered to collections the user can read (5).
    // Expected: all authenticated users → 200.
    name: "Query plan cache entries (GET /_api/query-plan-cache)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/query-plan-cache",
  },

  {
    // DELETE /_api/query-plan-cache
    // Handler: RestQueryPlanCacheHandler
    // Auth: AUTHEN + DB RW — needs database write access (FIXME: RBAC).
    // Clears the query plan cache for the current database — idempotent.
    // Expected: AR→403 (no DB write), AW→200, SU→200
    name: "Clear query plan cache (DELETE /_api/query-plan-cache)",
    type: ["admin", "database"],
    method: "DELETE",
    path: "/_db/d/_api/query-plan-cache",
  },

  // ── /_api/ttl/* ──────────────────────────────────────────────────────────
  // All TTL endpoints require _system context (any authenticated user there).

  {
    // GET /_api/ttl/properties
    // Handler: RestTtlHandler
    // Auth: AUTHEN, _system — any authenticated user in _system context.
    // Returns TTL background thread configuration.
    name: "TTL properties (GET /_api/ttl/properties)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/_system/_api/ttl/properties",
  },

  {
    // GET /_api/ttl/statistics
    // Handler: RestTtlHandler
    // Auth: AUTHEN, _system — any authenticated user in _system context.
    // Returns TTL background thread statistics.
    name: "TTL statistics (GET /_api/ttl/statistics)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/_system/_api/ttl/statistics",
  },

  {
    // PUT /_api/ttl/properties
    // Handler: RestTtlHandler
    // Auth: AUTHEN, _system — any authenticated user in _system context.
    // Sending the default values (enable:true, frequency:30000 ms) is a
    // no-op and leaves the server state unchanged.
    name: "Update TTL properties (PUT /_api/ttl/properties)",
    type: ["admin", "database"],
    method: "PUT",
    path: "/_db/_system/_api/ttl/properties",
    body: { enable: true, frequency: 30000 },
  },

  // ── /_api/upload ─────────────────────────────────────────────────────────

  {
    // POST /_api/upload
    // Handler: RestUploadHandler
    // Auth: AUTHEN — any authenticated user (deprecated, gone in 4.0).
    // Sends a minimal body; the data is stored temporarily and harmless.
    name: "Upload data (POST /_api/upload)",
    type: ["admin", "database"],
    method: "POST",
    path: "/_db/d/_api/upload",
    body: {},
  },

  // ── /_api/version ────────────────────────────────────────────────────────

  {
    // GET /_api/version
    // Handler: RestVersionHandler
    // Auth: AUTHEN (with canUseHard(MonitoringInternal) for full details (2)).
    // On a non-hardened server all authenticated users → 200.
    name: "Server version (GET /_api/version)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/version",
  },

  // ── /_api/wal/* ──────────────────────────────────────────────────────────
  // All WAL endpoints require isSuperuser (SUPER) and are only available on
  // a DBServer or single-server (not on a coordinator).
  // On coordinator: all users → 403 (server type check fires first or 404).
  // On single server: AU→401/403, AN→403, AR→403, AW→403, SU→200 (or 400
  // for PUT/DELETE without required parameters).

  {
    // GET /_api/wal/lastTick
    // Returns the last WAL tick (sequence number).
    name: "WAL last tick (GET /_api/wal/lastTick)",
    type: "admin",
    method: "GET",
    path: "/_db/_system/_api/wal/lastTick",
  },

  {
    // GET /_api/wal/open-transactions
    // Returns a list of open transactions visible in the WAL.
    name: "WAL open transactions (GET /_api/wal/open-transactions)",
    type: "admin",
    method: "GET",
    path: "/_db/_system/_api/wal/open-transactions",
  },

  {
    // GET /_api/wal/range
    // Returns the available tick range in the WAL.
    name: "WAL tick range (GET /_api/wal/range)",
    type: "admin",
    method: "GET",
    path: "/_db/_system/_api/wal/range",
  },

  {
    // GET /_api/wal/tail
    // Returns WAL entries starting from a given tick.
    // Without required parameters the handler returns 400 for SU (safe).
    name: "WAL tail read (GET /_api/wal/tail)",
    type: "admin",
    method: "GET",
    path: "/_db/_system/_api/wal/tail",
  },

  {
    // PUT /_api/wal/tail
    // Acknowledges WAL entries up to a given tick.
    // Body {} causes a 400 for SU after auth check — no data is consumed.
    name: "WAL tail acknowledge (PUT /_api/wal/tail)",
    type: "admin",
    method: "PUT",
    path: "/_db/_system/_api/wal/tail",
    body: {},
  },

  {
    // DELETE /_api/wal/tail
    // Releases WAL resources up to a given tick.
    // Without required parameters the handler returns 400 for SU (safe).
    name: "WAL tail release (DELETE /_api/wal/tail)",
    type: "admin",
    method: "DELETE",
    path: "/_db/_system/_api/wal/tail",
  },

  // ── /openapi.json ────────────────────────────────────────────────────────

  {
    // GET /openapi.json
    // Handler: RestOpenApiHandler
    // Auth: OPEN — no authentication required at all.
    // Returns the full OpenAPI specification of the server.
    // Expected: all users (including unauthenticated) → 200.
    name: "OpenAPI spec (GET /openapi.json)",
    type: "all",
    method: "GET",
    path: "/openapi.json",
  },

  // ── /_api/tasks ──────────────────────────────────────────────────────────
  // Handler: RestTasksHandler (V8 required; tasks are being phased out)
  // Auth for GET: AUTHEN — any authenticated user; superuser sees all tasks,
  //               others see only their own.
  // Auth for POST/DELETE: canUseDb(Write) — DB RW.

  {
    // GET /_api/tasks
    // Lists tasks visible to the calling user.
    // Expected: AU→401/403, others→200 (filtered list)
    name: "List tasks (GET /_api/tasks)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/tasks",
  },

  {
    // GET /_api/tasks/{id}
    // Returns a specific task; nonexistent id → 404 after auth check.
    // Expected: AU→401/403, others→404
    name: "Get task by nonexistent id (GET /_api/tasks/nonexistent)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/d/_api/tasks/nonexistent-task-apitester-99999",
  },

  {
    // POST /_api/tasks
    // Creates a one-shot JavaScript task (V8 required).
    // teardown: if the task was created (status 200), discard it.
    // Expected: AU→401/403, AN/AR→403, AW/SU→200
    name: "Create task (POST /_api/tasks)",
    type: ["admin", "database"],
    method: "POST",
    path: "/_db/d/_api/tasks",
    body: { name: "apitester-task", command: "1+1;", offset: 0 },
    teardown: async (ctx) => {
      if (ctx.response && ctx.response.status === 200 &&
          ctx.response.body && ctx.response.body.id) {
        await ctx.request('DELETE',
          `/_db/d/_api/tasks/${ctx.response.body.id}`);
      }
    },
  },

  {
    // DELETE /_api/tasks/{id}
    // Drops a task by id.
    // setup:    superuser creates a one-shot task and stores its id.
    // teardown: superuser cleans up if the test user lacked permission.
    // Expected: AU→401/403, AN/AR→403, AW/SU→200
    name: "Delete task (DELETE /_api/tasks/<id>)",
    type: ["admin", "database"],
    method: "DELETE",
    path: "/_db/d/_api/tasks/${ctx.data.id}",
    setup: async (ctx) => {
      const resp = await ctx.request('POST', '/_db/d/_api/tasks',
        { name: "apitester-task", command: "1+1;", offset: 0 });
      if (!resp.body || !resp.body.id) {
        throw new Error(`setup: failed to create task: ${resp.status} ${JSON.stringify(resp.body)}`);
      }
      return { id: resp.body.id };
    },
    teardown: async (ctx) => {
      if (!ctx.response || ctx.response.status !== 200) {
        await ctx.request('DELETE', `/_db/d/_api/tasks/${ctx.data.id}`);
      }
    },
  },

  // ── /_api/token ──────────────────────────────────────────────────────────
  // Handler: RestAccessTokenHandler
  // Auth: canReadUser(user) for GET; canWriteUser(user) for POST/DELETE.
  // In classic mode: isSuperuser || user==self || RW on _system.
  // Tests use "root" as the target user (always exists).

  {
    // GET /_api/token/root
    // Lists named access tokens belonging to the "root" user.
    // Expected: AU→401/403, AN/AR→403 (no _system RW), AW/SU→200
    name: "List access tokens for root (GET /_api/token/root)",
    type: ["admin", "database"],
    method: "GET",
    path: "/_db/_system/_api/token/root",
  },

  {
    // POST /_api/token/root
    // Creates a named access token for the "root" user.
    // teardown: if the token was created (status 200), discard it.
    // Expected: AU→401/403, AN/AR→403, AW/SU→200
    name: "Create access token for root (POST /_api/token/root)",
    type: ["admin", "database"],
    method: "POST",
    path: "/_db/_system/_api/token/root",
    body: { name: "apitester-token" },
    teardown: async (ctx) => {
      if (ctx.response && ctx.response.status === 200 &&
          ctx.response.body && ctx.response.body.id) {
        await ctx.request('DELETE',
          `/_db/_system/_api/token/root/${ctx.response.body.id}`);
      }
    },
  },

  {
    // DELETE /_api/token/root/{id}
    // Revokes a named access token belonging to "root".
    // setup:    superuser creates a token and stores its id.
    // teardown: superuser cleans up if the test user lacked permission.
    // Expected: AU→401/403, AN/AR→403, AW/SU→200
    name: "Delete access token for root (DELETE /_api/token/root/<id>)",
    type: ["admin", "database"],
    method: "DELETE",
    path: "/_db/_system/_api/token/root/${ctx.data.tokenId}",
    setup: async (ctx) => {
      const resp = await ctx.request('POST', '/_db/_system/_api/token/root',
        { name: "apitester-token" });
      if (!resp.body || !resp.body.id) {
        throw new Error(`setup: failed to create token: ${resp.status} ${JSON.stringify(resp.body)}`);
      }
      return { tokenId: resp.body.id };
    },
    teardown: async (ctx) => {
      if (!ctx.response || ctx.response.status !== 200) {
        await ctx.request('DELETE',
          `/_db/_system/_api/token/root/${ctx.data.tokenId}`);
      }
    },
  },

];
