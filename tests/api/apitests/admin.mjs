// Tests for the /_admin/auth/reload endpoint.
//
// Mounted at /_admin/auth/reload (exact match, POST).
// RestAuthReloadHandler::execute() calls canUseAdminAction(AdminAuthReload),
// which without RBAC maps to requiring RW access on the _system database.
//
// The operation is idempotent (triggers an auth-cache revalidation) and has
// no harmful side effects, so no setup or teardown is required.
//
// Expected columns (default configuration):
//   AU (_sys undef) → 403
//   AN (_sys none)  → 403
//   AR (_sys ro)    → 403
//   AW (_sys rw)    → 200
//   superuser       → 200

export default [
  {
    name: "Reload auth cache (POST /_admin/auth/reload)",
    type: "admin",
    method: "POST",
    path: "/_admin/auth/reload",
  },
];
