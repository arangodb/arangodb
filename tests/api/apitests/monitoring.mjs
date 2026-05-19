// Tests for /_admin monitoring and information endpoints:
//
//   /_admin/statistics            RestAdminStatisticsHandler
//   /_admin/statistics-description RestAdminStatisticsHandler
//   /_admin/status                RestAdminStatusHandler
//   /_admin/supervisionState      RestSupervisionStateHandler
//   /_admin/support-info          RestSupportInfoHandler
//   /_admin/system-report         RestSystemReportHandler
//   /_admin/telemetrics (GET)     RestTelemetricsHandler
//   /_admin/telemetrics (DELETE)  RestTelemetricsHandler
//   /_admin/time                  RestTimeHandler
//   /_admin/usage-metrics         RestUsageMetricsHandler
//   /_admin/version               RestVersionHandler
//
// Authorization legend
// ────────────────────
// canUseHard(X)   → canUseHardenedAction(AdminX)
//   --server.harden=false (default): AUTHEN – any authenticated user passes
//   --server.harden=true:            RW on _system required
// canUseAdmin(X)  → canUseAdminAction(AdminX) – without RBAC: RW on _system
// isSuperuser     → JWT with no preferred_username (superuser token only)
// ?/S/A/AU        – --server.support-info-api (or similar) policy:
//   "jwt" (default): isSuperuser only
//   "admin":         canUseAdmin(MonitoringInternal)
//   "public":        AUTHEN
//   "disabled":      route not registered → 404
//
// For each test below the expected column values are given under the
// assumption of the DEFAULT server configuration (harden=false, jwt policy
// for support-info/telemetrics, telemetrics enabled).

export default [

  // ── /_admin/statistics ────────────────────────────────────────────────────
  // Auth: canUseHardenedAction(AdminMonitoring)
  // Handler: RestAdminStatisticsHandler (RestBaseHandler)
  // Default (harden=false): AUTHEN – any authenticated user passes.
  // The handler checks auth itself; because it extends RestBaseHandler the
  // general routing does NOT enforce _system DB access.  AU and AN have valid
  // credentials and reach the in-handler check, which passes (harden=false).
  // Expected (harden=false): AU→401, AN→401, AR→200, AW→200, SU→200
  // Expected (harden=true):  AU→401, AN→401, AR→403, AW→200, SU→200
  {
    name: "Get statistics (GET /_admin/statistics)",
    type: "admin",
    method: "GET",
    path: "/_admin/statistics",
  },

  // ── /_admin/statistics-description ───────────────────────────────────────
  // Same handler and same auth guard as /_admin/statistics.
  // Returns human-readable descriptions of all statistic fields.
  // Expected (harden=false): AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Get statistics description (GET /_admin/statistics-description)",
    type: "admin",
    method: "GET",
    path: "/_admin/statistics-description",
  },

  // ── /_admin/status ────────────────────────────────────────────────────────
  // Auth: canUseHardenedAction(AdminMonitoring)
  // Handler: RestAdminStatusHandler (RestBaseHandler)
  // Default (harden=false): AUTHEN.
  // Expected (harden=false): AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Get server status (GET /_admin/status)",
    type: "admin",
    method: "GET",
    path: "/_admin/status",
  },

  // ── /_admin/supervisionState ──────────────────────────────────────────────
  // Auth: canUseAdminAction(AdminSupervisionState) → without RBAC: RW on _system
  // Handler: RestSupervisionStateHandler (RestVocbaseBaseHandler)
  //   Auth check fires first; coordinator check fires second.
  // On a single-server: auth passes for AW/SU, then "not a coordinator" → 403.
  // On a coordinator: auth passes for AW/SU, then agency call produces data.
  // Expected: AU→401, AN→401, AR→403, AW→403 or 200, SU→403 or 200
  {
    name: "Get supervision state (GET /_admin/supervisionState)",
    type: "admin",
    method: "GET",
    path: "/_admin/supervisionState",
  },

  // ── /_admin/support-info ──────────────────────────────────────────────────
  // Auth: controlled by --server.support-info-api policy:
  //   "jwt" (default): isSuperuser only → AU/AN/AR/AW get 403, SU gets 200
  //   "admin":         canUseAdmin(AdminMonitoring) → AW/SU get 200
  //   "public":        no check → all authenticated pass
  // Handler: RestSupportInfoHandler (RestBaseHandler)
  // With default "jwt" policy:
  // Expected: AU→401, AN→401, AR→403, AW→403, SU→200
  {
    name: "Get support info (GET /_admin/support-info)",
    type: "admin",
    method: "GET",
    path: "/_admin/support-info",
  },

  // ── /_admin/system-report ─────────────────────────────────────────────────
  // Auth: canUseHardenedAction(AdminMonitoringInternal)
  // Handler: RestSystemReportHandler (RestBaseHandler)
  // Default (harden=false): AUTHEN.
  // WARNING: This endpoint runs OS commands (date, dmesg, df, uptime, top)
  // and may take up to 60 seconds to respond for authorised callers.
  // Auth failures return immediately with 403, so the test is fast for
  // unauthorized users.
  // Expected (harden=false): AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Get system report (GET /_admin/system-report)",
    type: "admin",
    method: "GET",
    path: "/_admin/system-report",
  },

  // ── /_admin/telemetrics (GET) ─────────────────────────────────────────────
  // Auth: same --server.support-info-api policy as support-info:
  //   "jwt" (default): isSuperuser only
  //   "admin":         canUseAdmin(AdminMonitoringInternal)
  //   "public":        AUTHEN
  // Additional check: if --server.telemetrics-api=false (disabled),
  //   every caller (including SU) receives 403.
  // Handler: RestTelemetricsHandler (RestBaseHandler)
  // With default "jwt" policy and telemetrics enabled:
  // Expected: AU→401, AN→401, AR→403, AW→200, SU→200
  {
    name: "Get telemetrics data (GET /_admin/telemetrics)",
    type: "admin",
    method: "GET",
    path: "/_admin/telemetrics",
  },

  // ── /_admin/telemetrics (DELETE) ──────────────────────────────────────────
  // Same auth as GET /_admin/telemetrics.
  // Effect: resets the internal per-interval request counter (harmless, used
  // for testing rate-limiting behaviour).
  // With default "jwt" policy and telemetrics enabled:
  // Expected: AU→401, AN→401, AR→403, AW→200, SU→200
  {
    name: "Reset telemetrics request counter (DELETE /_admin/telemetrics)",
    type: "admin",
    method: "DELETE",
    path: "/_admin/telemetrics",
  },

  // ── /_admin/time ──────────────────────────────────────────────────────────
  // Auth: none – the handler (RestTimeHandler, RestBaseHandler) has no
  // authentication check whatsoever; it just returns the current server time.
  // Because RestBaseHandler does not mandate a _system DB context, the
  // general routing layer does not enforce _system access either.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Get server time (GET /_admin/time)",
    type: "admin",
    method: "GET",
    path: "/_admin/time",
  },

  // ── /_admin/usage-metrics ─────────────────────────────────────────────────
  // Auth: canUseHardenedAction(AdminMonitoringInternal)
  // Handler: RestUsageMetricsHandler (RestBaseHandler)
  // Default (harden=false): AUTHEN.
  // Expected (harden=false): AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Get usage metrics (GET /_admin/usage-metrics)",
    type: "admin",
    method: "GET",
    path: "/_admin/usage-metrics",
  },

  // ── /_admin/version ───────────────────────────────────────────────────────
  // Auth: none as a gate – the handler (RestVersionHandler, RestBaseHandler)
  // ALWAYS returns HTTP 200.  Internally it uses canUseHardenedAction to
  // decide whether to include the "version" field (allowInfo): with
  // harden=false this is always true, so the full version object is returned
  // to every caller.
  // Expected: AU→401, AN→401, AR→200, AW→200, SU→200
  {
    name: "Get server version (GET /_admin/version)",
    type: "admin",
    method: "GET",
    path: "/_admin/version",
  },

];
