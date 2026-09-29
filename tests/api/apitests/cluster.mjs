// Tests for the /_api/cluster endpoint family.
//
// Handler: RestClusterHandler (RestBaseHandler)
// Mounted at: /_api/cluster (prefix, ONLY when cluster mode is enabled)
//
// IMPORTANT: On a single-server deployment (no cluster) the entire
// /_api/cluster prefix is not registered → every request returns 404.
// The per-user expected outcomes described in comments below reflect the
// behaviour on a coordinator.  When run against a single server all rows
// will show 404.
//
// Auth overview
// ─────────────
//  RestBaseHandler does NOT require a database context.  Authenticated users
//  (including AU and AN) reach the handler.  Per-route permission checks:
//
//  /_api/cluster/agency-cache      canUseAdminAction(AdminReadAgency)
//    → AU→403, AN→403, AR→403, AW→200, SU→200
//
//  /_api/cluster/agency-dump       canUseAdminAction(AdminReadAgency)
//    → AU→403, AN→403, AR→403, AW→501(coord-only), SU→501(coord-only)
//      (agency-dump checks isCoordinator() FIRST, so 501 even before auth)
//
//  /_api/cluster/cluster-info      canUseAdminAction(AdminClusterInfo)
//    → AU→403, AN→403, AR→403, AW→200, SU→200
//
//  /_api/cluster/cluster-info/<sub>  canUseAdminAction(AdminClusterInfo)
//                                    THEN isSuperuser (prod) / always (maint)
//    → AU→403, AN→403, AR→403, AW→403, SU→200
//
//  /_api/cluster/endpoints         CommTask special-case: any authenticated
//                                  user is allowed through; handler returns
//                                  200 on coordinator, 501 on non-coordinator.
//    → AU→200/501, AN→200/501, AR→200/501, AW→200/501, SU→200/501

export default [

  // ── GET /_api/cluster/agency-cache ───────────────────────────────────────
  // Returns a dump of the local agency cache.  No coordinator check, so the
  // auth check fires before the request is served.
  // Coordinator: AU→401, AN→401, AR→403, AW→200, SU→200
  // Single-server (route not registered): all→401, SU-200
  {
    name: "Read agency cache (GET /_api/cluster/agency-cache)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/agency-cache",
  },

  // ── GET /_api/cluster/agency-dump ────────────────────────────────────────
  // Returns a full agency dump.  Coordinator check fires before auth check,
  // so on a non-coordinator all users get 501 regardless of permissions.
  // Coordinator: AU→401, AN→401, AR→403, AW→200, SU→200
  // Non-coordinator: all→401 or 501  /  Single-server: all→401 or 501
  {
    name: "Get agency dump (GET /_api/cluster/agency-dump)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/agency-dump",
  },

  // ── GET /_api/cluster/cluster-info ───────────────────────────────────────
  // Returns ClusterInfo state.  Requires canUseAdminAction(AdminClusterInfo).
  // Coordinator: AU→401, AN→401, AR→403, AW→200, SU→200
  // Single-server: all→401/200
  {
    name: "Get cluster info (GET /_api/cluster/cluster-info)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/cluster-info",
  },

  // ── PUT /_api/cluster/cluster-info/flush ─────────────────────────────────
  // Flushes the ClusterInfo cache.
  // Requires canUseAdminAction(AdminClusterInfo) THEN isSuperuser (prod).
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200
  // Single-server: skipped (because it blocks)
  {
    name: "Flush cluster info cache (PUT /_api/cluster/cluster-info/flush)",
    type: "admin",
    method: "PUT",
    path: "/_api/cluster/cluster-info/flush",
    setup: async (ctx) => {
      const r = await ctx.request('GET', '/_admin/server/role');
      if (r.body.role === "SINGLE") {
        return {skipTest: true};
      }
    }, 
  },

  // ── GET /_api/cluster/cluster-info/get_collection_info/{db}/{coll} ───────
  // Returns shard/server info for a collection.  Dummy db/collection names
  // used; on a coordinator the isSuperuser gate fires before the lookup, so
  // non-superusers get 403 regardless of whether the collection exists.
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200
  // Single-server: all→401 or 404
  {
    name: "Get collection shard info (GET /_api/cluster/cluster-info/get_collection_info/d/c)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/cluster-info/get_collection_info/d/c",
  },

  // ── GET /_api/cluster/cluster-info/get_collection_info_current/{db}/{coll}/{shard} ──
  // Returns current shard state.  Same superuser gate as above.
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200
  // Single-server: all→401 or 404
  {
    name: "Get collection shard current info (GET /_api/cluster/cluster-info/get_collection_info_current/d/c/s1)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/cluster-info/get_collection_info_current/d/c/s1",
  },

  // ── POST /_api/cluster/cluster-info/get_responsible_servers ──────────────
  // Returns responsible servers for the given shard IDs (POST body: array of
  // shard-ID strings).  Same superuser gate.
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→400 (empty/bad body)
  // Single-server: all→401 or 404
  {
    name: "Get responsible servers (POST /_api/cluster/cluster-info/get_responsible_servers)",
    type: "admin",
    method: "POST",
    path: "/_api/cluster/cluster-info/get_responsible_servers",
    body: [],
  },

  // ── POST /_api/cluster/cluster-info/get_responsible_shard/{db}/{coll} ────
  // Returns the shard responsible for the given document.  Same superuser gate.
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200
  // Single-server: all→401 or 400
  {
    name: "Get responsible shard (POST /_api/cluster/cluster-info/get_responsible_shard/d/c)",
    type: "admin",
    method: "POST",
    path: "/_api/cluster/cluster-info/get_responsible_shard/d/c/true",
    body: { _key: "testkey" },
  },

  // ── GET /_api/cluster/cluster-info/get_analyzers_revision/{db} ───────────
  // Returns the analyzer revision for a database.  Same superuser gate.
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200 (db may exist)
  // Single-server: all→401 or 400
  {
    name: "Get analyzers revision (GET /_api/cluster/cluster-info/get_analyzers_revision/_system)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/cluster-info/get_analyzers_revision/_system",
  },

  // ── GET /_api/cluster/cluster-info/wait_for_plan_version/{version} ────────
  // Waits until the plan version reaches the given value.  Same superuser gate.
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200 (already at version)
  // Single-server: all→401 or 200
  {
    name: "Wait for plan version (GET /_api/cluster/cluster-info/wait_for_plan_version/1)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/cluster-info/wait_for_plan_version/1",
  },

  // ── GET /_api/cluster/cluster-info/get_max_number_of_shards ──────────────
  // Returns the configured maximum number of shards.  Same superuser gate.
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200
  // Single-server: all→401 or 200
  {
    name: "Get max number of shards (GET /_api/cluster/cluster-info/get_max_number_of_shards)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/cluster-info/get_max_number_of_shards",
  },

  // ── GET /_api/cluster/cluster-info/get_max_replication_factor ────────────
  // Returns the configured maximum replication factor.  Same superuser gate.
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200
  // Single-server: all→401 or 200
  {
    name: "Get max replication factor (GET /_api/cluster/cluster-info/get_max_replication_factor)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/cluster-info/get_max_replication_factor",
  },

  // ── GET /_api/cluster/cluster-info/get_min_replication_factor ────────────
  // Returns the configured minimum replication factor.  Same superuser gate.
  // Coordinator: AU→401, AN→401, AR→403, AW→403, SU→200
  // Single-server: all→401 or 200
  {
    name: "Get min replication factor (GET /_api/cluster/cluster-info/get_min_replication_factor)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/cluster-info/get_min_replication_factor",
  },

  // ── GET /_api/cluster/endpoints ──────────────────────────────────────────
  // Returns the list of coordinator endpoints.  CommTask special-case: any
  // authenticated user is allowed through (no admin RBAC check needed).
  // The handler still checks isCoordinator() → 501 on non-coordinator.
  // Coordinator: AU→200, AN→200, AR→200, AW→200, SU→200
  // Non-coordinator: all→401 or 501  /  Single-server: all→401 or 504
  {
    name: "List cluster endpoints (GET /_api/cluster/endpoints)",
    type: "admin",
    method: "GET",
    path: "/_api/cluster/endpoints",
  },

];
