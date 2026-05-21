// Tests for a collection of /_admin/* endpoints.
//
// All tests are type "admin" (5 columns: AU, AN, AR, AW, superuser).
//
// Auth abbreviations used in comments below:
//   canUseAdmin(X)  → canUseAdminAction(AdminX) → without RBAC: RW on _system
//   canUseHard(X)   → canUseHardenedAction(AdminX) → without RBAC: RW on
//                     _system if --server.harden=true, AUTHEN otherwise
//   isSuperuser     → JWT token with empty preferred_username
//   AUTHEN          → any authenticated user, no further check
//
// SA/SW/LEG note on cluster endpoints:
//   The /_admin/cluster/* handler has a global guard controlled by
//   --cluster.api-jwt-policy (default: "jwt-compat" = LEG).
//   In the default mode ("jwt-compat") each sub-handler enforces its own
//   permission.  The tests below assume the default setting.
//
// Single-server vs coordinator:
//   Many cluster endpoints perform a "coordinator-only" check that returns
//   403 before the per-user auth check.  On a single-server those endpoints
//   return 403 for every column; the auth differentiation is only visible
//   when the tests are run against a coordinator.  Endpoints where the auth
//   check fires FIRST (e.g. shardStatistics, maintenance, compact, crashes)
//   do show meaningful auth differentiation even on a single-server.

export default [

  // ── /_admin/auth/reload ──────────────────────────────────────────────────

  {
    // POST /_admin/auth/reload
    // Auth: canUseAdmin(AuthReload) → RW on _system
    // Effect: triggers auth-cache revalidation (idempotent, safe)
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: "Reload auth cache (POST /_admin/auth/reload)",
    type: "admin",
    method: "POST",
    path: "/_admin/auth/reload",
  },

  // ── /_admin/cluster/* ────────────────────────────────────────────────────
  // All cluster endpoints share the SA/SW/LEG global guard.
  // Auth check order and coordinator restriction are noted per entry.

  {
    // GET /_admin/cluster/collectionShardDistribution
    // Auth: canUseAdmin(ClusterInfo) → RW on _system
    // Coordinator check fires FIRST; on single-server all columns → 403.
    // On coordinator: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: "Collection shard distribution (GET /_admin/cluster/collectionShardDistribution)",
    type: "admin",
    method: "GET",
    path: "/_db/d/_admin/cluster/collectionShardDistribution?collection=c",
  },

  {
    // GET /_admin/cluster/health
    // Auth: AUTHEN (no per-user check beyond authentication)
    // The handler additionally requires a live agency connection; on a
    // single-server without agency it returns 403 "not allowed on single
    // servers" for all columns.
    // On coordinator: AU→200, AN→200, AR→200, AW→200, superuser→200
    name: "Cluster health (GET /_admin/cluster/health)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/health",
  },

  {
    // GET /_admin/cluster/maintenance
    // Auth: canUseAdmin(Maintenance) → RW on _system  (check fires FIRST)
    // After auth, requires agency; on single-server without agency AW/SU
    // receive 403 "not allowed on single servers".
    // On coordinator: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: "Get cluster maintenance state (GET /_admin/cluster/maintenance)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/maintenance",
  },

  {
    // PUT /_admin/cluster/maintenance
    // Auth: canUseAdmin(Maintenance) → RW on _system  (check fires FIRST)
    // Body must be a JSON string: "on" | "off" | <numeric timeout>.
    // We send "off" (turn off maintenance) which is always a no-op when
    // maintenance is already inactive and is safe to run multiple times.
    // After auth, requires agency; on single-server without agency AW/SU
    // receive 403 "not allowed on single servers".
    // On coordinator: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: "Set cluster maintenance state (PUT /_admin/cluster/maintenance)",
    type: "admin",
    method: "PUT",
    path: "/_admin/cluster/maintenance",
    body: "off",
  },

  {
    // GET /_admin/cluster/nodeEngine
    // Auth: AUTHEN; proxies to a DB server via ?ServerID=<id>
    // Coordinator check fires FIRST; on single-server all columns → 403.
    // On coordinator (without required ServerID param): AU→400, ..., SU→400
    // On coordinator (with valid ServerID): all authenticated → 200
    name: "Node engine (GET /_admin/cluster/nodeEngine)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/nodeEngine",
  },

  {
    // GET /_admin/cluster/nodeStatistics
    // Auth: AUTHEN; proxies to a DB server via ?ServerID=<id>
    // Coordinator check fires FIRST; same behaviour as nodeEngine.
    name: "Node statistics (GET /_admin/cluster/nodeStatistics)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/nodeStatistics",
  },

  {
    // GET /_admin/cluster/nodeVersion
    // Auth: AUTHEN; proxies to a DB server via ?ServerID=<id>
    // Coordinator check fires FIRST; same behaviour as nodeEngine.
    name: "Node version (GET /_admin/cluster/nodeVersion)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/nodeVersion",
  },

  {
    // GET /_admin/cluster/numberOfServers
    // Auth: AUTHEN
    // Coordinator check fires FIRST; on single-server all columns → 403.
    // On coordinator: AU→200, AN→200, AR→200, AW→200, superuser→200
    name: "Number of servers (GET /_admin/cluster/numberOfServers)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/numberOfServers",
  },

  {
    // PUT /_admin/cluster/numberOfServers
    // Auth: canUseHard(Maintenance) → with --server.harden=false (default):
    //   AUTHEN (all authenticated pass); with --server.harden=true: RW on _system
    // Coordinator check fires FIRST; on single-server all columns → 403.
    // On coordinator (default, not hardened): all authenticated → 200 or 400
    // Body: {} – missing required fields; AW/SU receive 200 (nothing changed)
    // because the handler treats empty objects as no-ops.
    name: "Set number of servers (PUT /_admin/cluster/numberOfServers)",
    type: "admin",
    method: "PUT",
    path: "/_admin/cluster/numberOfServers",
    body: {},
  },

  {
    // GET /_admin/cluster/rebalance
    // Auth: canUseAdmin(Rebalance) → RW on _system
    // Coordinator check fires FIRST; on single-server all columns → 403.
    // On coordinator: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: "Get rebalance state (GET /_admin/cluster/rebalance)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/rebalance",
  },

  {
    // PUT /_admin/cluster/rebalance
    // Auth: canUseAdmin(Rebalance) → RW on _system
    // Coordinator check fires FIRST; on single-server all columns → 403.
    // On coordinator: AU→403, AN→403, AR→403, AW→200, superuser→200
    // Body: {} – treated as a "compute plan" request (read-only analysis),
    // which is safe and does not execute any shard moves.
    name: "Compute rebalance plan (PUT /_admin/cluster/rebalance)",
    type: "admin",
    method: "PUT",
    path: "/_admin/cluster/rebalance",
    body: {},
  },

  {
    // GET /_admin/cluster/shardDistribution
    // Auth: canUseAdmin(ClusterInfo) → RW on _system
    // Coordinator check fires FIRST; on single-server all columns → 403.
    // On coordinator: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: "Shard distribution (GET /_admin/cluster/shardDistribution)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/shardDistribution",
  },

  {
    // GET /_admin/cluster/shardStatistics
    // Auth: canUseAdmin(ClusterInfo) → RW on _system  (check fires FIRST)
    // Coordinator check fires SECOND; on single-server:
    //   AU→403, AN→403, AR→403, AW→501 (not coordinator), superuser→501
    // On coordinator: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: "Shard statistics (GET /_admin/cluster/shardStatistics)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/shardStatistics",
  },

  {
    // GET /_admin/cluster/statistics
    // Auth: AUTHEN; proxies to a DB server via ?DBserver=<id>
    // Coordinator check fires FIRST; on single-server all columns → 403.
    // On coordinator (without required DBserver param): all authenticated → 400
    name: "Cluster statistics (GET /_admin/cluster/statistics)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/statistics",
  },

  {
    // POST /_admin/cluster/cancelAgencyJob
    // Auth: canUseAdmin(MoveShards) → RW on _system  (check fires FIRST)
    // Single: AU→401, AN→401, AR→403, AW→403 (coord only), SU→403
    // Coordinator: AU→401, AN→401, AR→403, AW→404, SU→404
    name: "Cancel agency job (PUT /_admin/cluster/cancelAgencyJob)",
    type: "admin",
    method: "POST",
    path: "/_admin/cluster/cancelAgencyJob",
    body: { id: "nonexistent-job-apitester" },
  },

  {
    // POST /_admin/cluster/cleanOutServer
    // Auth: canUseAdmin(MoveShards) → RW on _system  (check fires FIRST)
    // Body {} is missing the required "server" key → 400 on coordinator, but
    // auth/coordinator checks fire before body parsing.
    // Single: AU→401, AN→401, AR→403, AW→403 (coord only), SU→403
    // Coordinator: AU→401, AN→401, AR→403, AW→400, SU→400
    name: "Clean out server (PUT /_admin/cluster/cleanOutServer)",
    type: "admin",
    method: "POST",
    path: "/_admin/cluster/cleanOutServer",
    body: {},
  },

  {
    // GET /_admin/cluster/maintenance/{serverId}
    // Auth: canUseAdmin(Maintenance) → RW on _system  (check fires FIRST)
    // Uses a bogus serverId; on coordinator auth passes for AW/SU, then the
    // agency returns an empty / 200 result for an unknown server.
    // Single: AU→401, AN→401, AR→403, AW→403 (coord only), SU→403
    // Coordinator: AU→401, AN→401, AR→403, AW→200 (or empty result), SU→200
    name: "Get DBServer maintenance state (GET /_admin/cluster/maintenance/nonexistent)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/maintenance/nonexistent",
  },

  {
    // PUT /_admin/cluster/maintenance/{serverId}
    // Auth: canUseAdmin(Maintenance) → RW on _system  (check fires FIRST)
    // Body: {"mode":"normal"} requests that maintenance is turned off — a
    // no-op for a server already in normal mode, or a 412 precondition
    // failure for an unknown server.  Either way it is harmless.
    // Single: AU→401, AN→401, AR→403, AW→403 (coord only), SU→403
    // Coordinator: AU→401, AN→401, AR→403, AW→412 or 200, SU→412 or 200
    name: "Set DBServer maintenance state (PUT /_admin/cluster/maintenance/nonexistent)",
    type: "admin",
    method: "PUT",
    path: "/_admin/cluster/maintenance/nonexistent",
    body: { mode: "normal" },
  },

  {
    // POST /_admin/cluster/moveShard
    // Auth: canUseAdmin(MoveShards) OR canUseColl(RWMETA)  (check fires THIRD)
    // Coordinator check fires FIRST; on single-server all columns → 403.
    // Body must be a valid object for auth check to be reached.  Using a
    // nonexistent collection means authorised users see 404 (safe).
    // Single: AU→401, AN→401, AR→403, AW→403 (coord only), SU→403
    // Coordinator: AU→401, AN→401, AR→403 (auth), AW→404 (coll not found), SU→404
    name: "Move shard (POST /_admin/cluster/moveShard)",
    type: "admin",
    method: "POST",
    path: "/_admin/cluster/moveShard",
    body: { database: "_system", collection: "nonexistent_apitester", shard: "s1", fromServer: "from", toServer: "to" },
  },

  {
    // GET /_admin/cluster/queryAgencyJob
    // Auth: canUseAdmin(MoveShards) → RW on _system  (check fires FIRST)
    // Uses a bogus job id; on coordinator authorised users receive 404.
    // Single: AU→401, AN→401, AR→403, AW→403 (coord only), SU→403
    // Coordinator: AU→401, AN→401, AR→403, AW→404 (job not found), SU→404
    name: "Query agency job status (GET /_admin/cluster/queryAgencyJob)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/queryAgencyJob?id=nonexistent-job-apitester-99999",
  },

  {
    // POST /_admin/cluster/rebalanceShards  (legacy endpoint)
    // Auth: canUseAdmin(Rebalance) → RW on _system  (check fires THIRD)
    // Single: AU->401, AN->401, AR->403, AW->403 (coord only), SU->403.
    // Coordinator with POST: method OK → coordinator OK → auth check →
    //   AU→401, AN→401, AR→403, AW→200, SU→200
    name: "Rebalance shards legacy (POST /_admin/cluster/rebalanceShards)",
    type: "admin",
    method: "POST",
    path: "/_admin/cluster/rebalanceShards",
    body: {},
  },

  {
    // POST /_admin/cluster/removeServer
    // Auth: canUseAdmin(RemoveServer) → RW on _system  (check fires FIRST)
    // Method check fires SECOND, coordinator check fires THIRD.
    // Body with a nonexistent server id causes the agency lookup to return 404.
    // Single: AU→401, AN→401, AR→403, AW→500 (coord only), SU→500
    // Coordinator: AU→401, AN→401, AR→403, AW→404 (server not in health), SU→404
    name: "Remove server (POST /_admin/cluster/removeServer)",
    type: "admin",
    method: "POST",
    path: "/_admin/cluster/removeServer",
    body: { server: "PRMR-nonexistent-apitester" },
  },

  {
    // POST /_admin/cluster/resignLeadership
    // Auth: canUseAdmin(MoveShards) → RW on _system  (check fires FIRST)
    // NB: the code actually requires POST; PUT yields 405 for authorised users
    // (auth check fires before the method check, so differentiation is still
    // visible).
    // Single: AU→401, AN→401, AR→403, AW→403 (coord only), SU→403
    // Coordinator: AU→401, AN→401, AR→403, AW→400, SU→400
    name: "Resign leadership (PUT /_admin/cluster/resignLeadership)",
    type: "admin",
    method: "POST",
    path: "/_admin/cluster/resignLeadership",
    body: {},
  },

  {
    // PUT /_admin/cluster/uniqId
    // Auth: canUseAdmin(Maintenance) → RW on _system  (check fires SECOND)
    // ?number=1 requests a single unique ID range — entirely harmless.
    // Single: AU→401, AN→401, AR→403, AW→403 (coord only), SU→403
    // Coordinator: AU→401, AN→401, AR→403, AW→200, SU→200
    name: "Reserve unique IDs (PUT /_admin/cluster/uniqId)",
    type: "admin",
    method: "PUT",
    path: "/_admin/cluster/uniqId?number=1",
  },

  {
    // PUT /_admin/cluster/vpackSortMigration/{serverId}
    // Auth: isSuperuser only  (check fires FIRST — no named-user override)
    // The "check" sub-command (GET) performs a read-only analysis of VPack
    // index sort order without migrating any data.  Auth fires before the
    // method check, so the auth matrix is fully visible with either verb.
    // Single/Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200
    name: "VPack sort migration check (GET /_admin/cluster/vpackSortMigration/check)",
    type: "admin",
    method: "GET",
    path: "/_admin/cluster/vpackSortMigration/check",
  },

  // ── /_admin/compact ──────────────────────────────────────────────────────

  {
    // PUT /_admin/compact
    // Auth: isSuperuser (only superuser JWT, no named-user override)
    // Triggers a full RocksDB compaction. This can be slow on large datasets
    // but is safe and idempotent; in a test environment the DB is small.
    // Expected: AU→403, AN→403, AR→403, AW→403, superuser→200
    name: "Compact all databases (PUT /_admin/compact)",
    type: "admin",
    method: "PUT",
    path: "/_admin/compact",
  },

  // ── /_admin/crashes ──────────────────────────────────────────────────────
  // RestCrashHandler is a prefix handler at /_admin/crashes.
  // After the auth check it additionally verifies that the CrashHandlerFeature
  // is enabled; if disabled it returns 503 SERVICE_UNAVAILABLE.
  // Auth check always fires first, so AU/AN/AR always receive 403.

  {
    // GET /_admin/crashes
    // Auth: canUseAdmin(CrashHandler) → RW on _system
    // Expected: AU→403, AN→403, AR→403, AW→200 or 503, superuser→200 or 503
    name: "List crash dumps (GET /_admin/crashes)",
    type: "admin",
    method: "GET",
    path: "/_admin/crashes",
  },

  {
    // GET /_admin/crashes/{id}
    // Auth: canUseAdmin(CrashHandler) → RW on _system
    // Uses a non-existent crash ID; authorised users receive 404.
    // Expected: AU→403, AN→403, AR→403, AW→404 or 503, superuser→404 or 503
    name: "Get single crash dump (GET /_admin/crashes/nonexistent)",
    type: "admin",
    method: "GET",
    path: "/_admin/crashes/nonexistent",
  },

  {
    // DELETE /_admin/crashes/{id}
    // Auth: canUseAdmin(CrashHandler) → RW on _system
    // Uses a non-existent crash ID; authorised users receive 404.
    // Expected: AU→403, AN→403, AR→403, AW→404 or 503, superuser→404 or 503
    name: "Delete single crash dump (DELETE /_admin/crashes/nonexistent)",
    type: "admin",
    method: "DELETE",
    path: "/_admin/crashes/nonexistent",
  },

  // ── /_admin/database/target-version ─────────────────────────────────────

  {
    // GET /_admin/database/target-version
    // Auth: AUTHEN — no authorization check in the handler at all; it simply
    // returns the server's compiled-in target database version.
    // Expected: AU→200, AN→200, AR→200, AW→200, superuser→200
    name: "Database target version (GET /_admin/database/target-version)",
    type: "admin",
    method: "GET",
    path: "/_admin/database/target-version",
  },

  // ── /_admin/shutdown ─────────────────────────────────────────────────────

  {
    // GET /_admin/shutdown
    // Auth: AUTHEN — no authorization check exists in the GET branch at all;
    // the handler simply returns soft-shutdown progress data.
    // Coordinator check fires FIRST (before any auth check); on a
    // On coordinator: AU→401, AN→401, AR→403, AW→200, superuser→200
    // single-server all authenticated columns receive 405 METHOD_NOT_ALLOWED.
    name: "Soft shutdown progress (GET /_admin/shutdown)",
    type: "admin",
    method: "GET",
    path: "/_admin/shutdown",
  },

  {
    // DELETE /_admin/shutdown
    // ATTENTION: We only do the cases which we know to fail, otherwise
    // we would shoot down the very server we are testing!
    // Auth: SUPER or ADMIN, should not work with the users we try!
    // Expected outcome is 401 everywhere.
    name: "Do shutdown (DELETE /_admin/shutdown)",
    type: ["collection", "database"],
    method: "DELETE",
    path: "/_admin/shutdown",
  },

];
