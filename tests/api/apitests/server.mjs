// Tests for several /_admin/* endpoints that do not have a dedicated file:
//
//   /_admin/deployment/id          RestAdminDeploymentHandler   AUTHEN
//   /_admin/execute                RestAdminExecuteHandler      AUTHEN (V8 + flag)
//   /_admin/license  GET           RestLicenseHandler           canUseHard(License)
//   /_admin/license  PUT           RestLicenseHandler           canUseHard(License)
//   /_admin/metrics  GET           RestMetricsHandler           canUseHard(Monitoring)
//   /_admin/options  GET           RestOptionsHandler           S/A/AU (default: jwt)
//   /_admin/options-description GET RestOptionsDescriptionHandler S/A/AU (default: jwt)
//   /_admin/options-public GET     RestPublicOptionsHandler     AUTHEN
//   /_admin/routing/reload POST    RestAdminRoutingHandler      AUTHEN (V8)
//   /_admin/server/api-calls GET   RestAdminServerHandler       ?/S/A AdminApiCalls
//   /_admin/server/aql-queries GET RestAdminServerHandler       ?/S/A AdminAqlQueries
//   /_admin/server/availability GET RestAdminServerHandler      OPEN
//   /_admin/server/databaseDefaults GET RestAdminServerHandler  AUTHEN
//   /_admin/server/id GET          RestAdminServerHandler       AUTHEN (cluster only)
//   /_admin/server/mode GET        RestAdminServerHandler       AUTHEN
//   /_admin/server/mode PUT        RestAdminServerHandler       AdminMaintenance
//   /_admin/server/role GET        RestAdminServerHandler       AUTHEN
//   /_admin/server/tls  GET        RestAdminServerHandler       AUTHEN
//   /_admin/server/tls  POST       RestAdminServerHandler       isSuperuser
//   /_admin/server/jwt  GET        RestAdminServerHandler       AUTHEN (EE only)
//   /_admin/server/jwt  POST       RestAdminServerHandler       isSuperuser (EE only)
//   /_admin/server/encryption GET  RestAdminServerHandler       AUTHEN (EE only)
//   /_admin/server/encryption POST RestAdminServerHandler       isSuperuser (EE only)
//
// Authorization legend
// ────────────────────
// AUTHEN          – any authenticated user, no per-user check
// OPEN            – no authentication at all required
// canUseHard(X)   – canUseHardenedAction(AdminX):
//                   · --server.harden=false (default): AUTHEN (all pass)
//                   · --server.harden=true:            RW on _system required
//                     → AU→403, AN→403, AR→403, AW→200, superuser→200
// canUseAdmin(X)  – canUseAdminAction(AdminX):
//                   without RBAC: RW on _system
//                   → AU→403, AN→403, AR→403, AW→200, superuser→200
// isSuperuser     – JWT token with no preferred_username
//                   → AU→403, AN→403, AR→403, AW→403, superuser→200
// ?/S/A           – switchable: off(?) / superuser-only(S) / admin(A=default)
//                   in default admin mode → same as canUseAdmin(X)
// S/A/AU          – switchable: superuser(S=default) / admin(A) / authen(AU)
//                   in default superuser mode → same as isSuperuser

export default [

  // ── /_admin/deployment/id ────────────────────────────────────────────────
  // Auth: AUTHEN – no per-user check beyond authentication.
  // Only available on coordinators and single servers; on a DB server the
  // handler returns 403 before the REST logic runs.
  // Expected (single-server / coordinator):
  //   AU→401, AN→401, AR→200, AW→200, superuser→200
  // Expected (DB server): all columns → 403
  {
    name: "Get deployment ID (GET /_admin/deployment/id)",
    type: "admin",
    method: "GET",
    path: "/_admin/deployment/id",
  },

  // ── /_admin/execute ──────────────────────────────────────────────────────
  // Auth: AUTHEN – no per-user check when the route is registered.
  // The endpoint is only registered when:
  //   • V8 is compiled in AND
  //   • --javascript.allow-admin-execute is true (non-default)
  // When not registered every request returns 404.
  // We send a trivial JavaScript expression so that authorised calls succeed
  // without any observable side effect.
  // Expected (route registered):     AU→401, AN→401, AR→200, AW→200, SU→200
  // Expected (route not registered): AU→401, AN→401, AR→404, AW→404, SU→404
  {
    name: "Execute JavaScript snippet (POST /_admin/execute)",
    type: "admin",
    method: "POST",
    path: "/_admin/execute",
    body: "1",
  },

  // ── /_admin/license ──────────────────────────────────────────────────────
  // Auth: canUseHardenedAction(AdminLicense)
  //   --server.harden=false (default): AUTHEN → all authenticated pass
  //   --server.harden=true:            AW→200, others→403
  {
    name: "Get license information (GET /_admin/license)",
    type: "admin",
    method: "GET",
    path: "/_admin/license",
  },

  {
    // PUT /_admin/license
    // Expect 401 if not authorized and 400 because license empty otherwise.
    name: "Set license key (PUT /_admin/license)",
    type: "admin",
    method: "PUT",
    path: "/_admin/license",
    body: {},
  },

  // ── /_admin/metrics ──────────────────────────────────────────────────────
  // Auth: canUseHardenedAction(AdminMonitoring)
  //   --server.harden=false (default): AUTHEN → all authenticated pass
  //   --server.harden=true:            AW→200, others→403
  // Returns Prometheus-format metrics text.
  {
    name: "Fetch Prometheus metrics (GET /_admin/metrics)",
    type: "admin",
    method: "GET",
    path: "/_admin/metrics",
  },

  // ── /_admin/options ──────────────────────────────────────────────────────
  // Auth: checkAuthentication() governed by --server.options-api
  //   "jwt"     (default): only superuser → 200; named users → 403
  //   "admin":             canUseAdmin(AdminOptions) → AW + SU → 200
  //   "public":            AUTHEN → all pass
  //   "disabled":          route not registered → 404
  // Expected with default policy "jwt":
  //   AU→401, AN→401, AR→403, AW→403, superuser→200
  {
    name: "Get server options (GET /_admin/options)",
    type: "admin",
    method: "GET",
    path: "/_admin/options",
  },

  // ── /_admin/options-description ──────────────────────────────────────────
  // Same auth guard as /_admin/options (uses same checkAuthentication()).
  // Returns the full option descriptions with metadata.
  // Expected with default policy "jwt":
  //   AU→401, AN→401, AR→403, AW→403, superuser→200
  {
    name: "Get server options description (GET /_admin/options-description)",
    type: "admin",
    method: "GET",
    path: "/_admin/options-description",
  },

  // ── /_admin/options-public ───────────────────────────────────────────────
  // Auth: AUTHEN – no auth check whatsoever (not even a database check).
  // The route is registered regardless of the --server.options-api policy,
  // because the platform UI depends on it.
  // Expected: AU→401, AN→401, AR→200, AW→200, superuser→200
  {
    name: "Get public server options (GET /_admin/options-public)",
    type: "admin",
    method: "GET",
    path: "/_admin/options-public",
  },

  // ── /_admin/routing/reload ───────────────────────────────────────────────
  // Auth: AUTHEN – any authenticated user may call this.
  // Requires V8; if JavaScript is disabled the handler returns 501.
  // On success it returns HTTP 204 NO_CONTENT.
  // Expected (V8 enabled):  AU→401, AN→401, AR→204, AW→204, superuser→204
  // Expected (V8 disabled): AU→401, AN→401, AR→501, AW→501, superuser→501
  {
    name: "Reload routing table (POST /_admin/routing/reload)",
    type: "admin",
    method: "POST",
    path: "/_admin/routing/reload",
  },

  // ── /_admin/server/api-calls ─────────────────────────────────────────────
  // Auth: governed by ApiRecordingFeature, same pattern as log API (?/S/A):
  //   API disabled → 403 for all
  //   "jwt" mode   → isSuperuser only
  //   default "true" (admin mode) → canUseAdmin(AdminApiCalls)
  //     → AU→401, AN→401, AR→403, AW→200, superuser→200
  {
    name: "Get recorded API calls (GET /_admin/server/api-calls)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/api-calls",
  },

  // ── /_admin/server/aql-queries ───────────────────────────────────────────
  // Auth: same ?/S/A pattern as api-calls, using canUseAdmin(AdminAqlQueries).
  // Only available on coordinator and single server; on DB server → 501.
  // Expected (default admin mode, single/coord):
  //   AU→401, AN→401, AR→403, AW→200, superuser→200
  {
    name: "Get recorded AQL queries (GET /_admin/server/aql-queries)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/aql-queries",
  },

  // ── /_admin/server/availability ──────────────────────────────────────────
  // Auth: OPEN – no authentication required at all.
  // Returns 200 when the server is healthy and in default mode,
  // 503 SERVICE_UNAVAILABLE during startup, maintenance, or when the
  // RocksDB health check fails.
  // Expected: AU→200 or 503, AN→200 or 503, AR→200 or 503,
  //           AW→200 or 503, superuser→200 or 503
  {
    name: "Server availability check (GET /_admin/server/availability)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/availability",
  },

  // ── /_admin/server/databaseDefaults ──────────────────────────────────────
  // Auth: AUTHEN – no per-user check.
  // Returns the default configuration values used when creating databases.
  // Expected: AU→401, AN→401, AR→200, AW→200, superuser→200
  {
    name: "Get database creation defaults (GET /_admin/server/databaseDefaults)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/databaseDefaults",
  },

  // ── /_admin/server/id ────────────────────────────────────────────────────
  // Auth: AUTHEN – no per-user check.
  // Only meaningful in a cluster; on a single server the handler returns
  // HTTP 500 (SERVER_ERROR) for every authenticated caller.
  // Expected (cluster coordinator): AU→401, AN→401, AR→200, AW→200, SU→200
  // Expected (single server):       AU→500, AN→500, AR→500, AW→500, SU→500
  {
    name: "Get server ID (GET /_admin/server/id)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/id",
  },

  // ── /_admin/server/mode GET ──────────────────────────────────────────────
  // Auth: AUTHEN – no per-user check.
  // Returns the current read-only / default mode.
  // Expected: AU→401, AN→401, AR→200, AW→200, superuser→200
  {
    name: "Get server mode (GET /_admin/server/mode)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/mode",
  },

  // ── /_admin/server/mode PUT ──────────────────────────────────────────────
  // Auth: canUseAdminAction(AdminMaintenance) → RW on _system
  // We send {"mode":"default"} which is a no-op when the server is already
  // in default mode, so the test is safe even when run 64+ times.
  // Expected: AU→401, AN→401, AR→403, AW→200, superuser→200
  {
    name: "Set server mode to default (PUT /_admin/server/mode)",
    type: "admin",
    method: "PUT",
    path: "/_admin/server/mode",
    body: { mode: "default" },
  },

  // ── /_admin/server/role ──────────────────────────────────────────────────
  // Auth: AUTHEN – no per-user check.
  // Returns the role of this server node (SINGLE, COORDINATOR, DBSERVER, …).
  // Expected: AU→401, AN→401, AR→200, AW→200, superuser→200
  {
    name: "Get server role (GET /_admin/server/role)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/role",
  },

  // ── /_admin/server/tls GET ───────────────────────────────────────────────
  // Auth: AUTHEN – any authenticated user can read the TLS certificate info.
  // Expected: AU→401, AN→401, AR→200, AW→200, superuser→200
  {
    name: "Get TLS certificate info (GET /_admin/server/tls)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/tls",
  },

  // ── /_admin/server/tls POST ──────────────────────────────────────────────
  // Auth: isSuperuser – only a superuser JWT may trigger a TLS reload.
  // Reloads TLS data from disk (safe: idempotent, reads existing files).
  // Expected: AU→401, AN→401, AR→403, AW→403, superuser→200
  {
    name: "Reload TLS certificate (POST /_admin/server/tls)",
    type: "admin",
    method: "POST",
    path: "/_admin/server/tls",
  },

  // ── /_admin/server/jwt GET ───────────────────────────────────────────────
  // Auth: AUTHEN (Enterprise Edition only; Community always returns 404).
  // Returns the list of active JWT secrets (without the secret values).
  // Expected (EE): AU→401, AN→401, AR→200, AW→200, superuser→200
  {
    name: "Get JWT secrets info (GET /_admin/server/jwt)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/jwt",
  },

  // ── /_admin/server/jwt POST ──────────────────────────────────────────────
  // Auth: isSuperuser (EE only; CE always returns 404).
  // Triggers a hot reload of the JWT secret files.  Safe and idempotent.
  // Expected (EE): AU→401, AN→401, AR→403, AW→403, superuser→200
  {
    name: "Reload JWT secrets (POST /_admin/server/jwt)",
    type: "admin",
    method: "POST",
    path: "/_admin/server/jwt",
  },

  // ── /_admin/server/encryption GET ───────────────────────────────────────
  // Auth: AUTHEN (EE only, not on coordinators; CE always returns 404).
  // Returns the current encryption key status.
  // Expected (EE, single/DBserver): AU→401, AN→401, AR→200, AW→200, SU→200
  // Note: 403 on coordinators and 404 when encryption not active!
  {
    name: "Get encryption key status (GET /_admin/server/encryption)",
    type: "admin",
    method: "GET",
    path: "/_admin/server/encryption",
  },

  // ── /_admin/server/encryption POST ──────────────────────────────────────
  // Auth: isSuperuser (EE only, not on coordinators; CE always returns 404).
  // Rotates the encryption key.
  // Expected (EE, single/DBserver): AU→401, AN→401, AR→403, AW→403, SU→200
  // Note: 403 on coordinators and 404 when encryption not active!
  {
    name: "Rotate encryption key (POST /_admin/server/encryption)",
    type: "admin",
    method: "POST",
    path: "/_admin/server/encryption",
  },

];
