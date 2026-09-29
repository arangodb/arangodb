// Tests for the /_admin/activities and /_admin/async-registry endpoints.
//
// Both endpoints expose internal observability data and are guarded by the
// AdminMonitoringInternal permission (canUseAdminAction(MonInternal)).
// Without RBAC this maps to requiring RW access on the _system database,
// so only AW (rw on _system) and the superuser column receive 200.
// AU, AN, AR all lack sufficient _system access and receive 403.
//
// /_admin/activities additionally requires the /_arango/experimental URL
// prefix.  Without that prefix the handler's API-version switch falls to the
// default branch and returns 405 METHOD_NOT_ALLOWED, even for authorised
// callers.  Using the experimental path therefore produces the cleanest
// output (403 vs 200) for authorization comparison across versions.
//
// /_admin/activities also supports a compile-time option
// (--activities.only-superuser) that restricts access to the superuser only
// (isSuperuser check instead of canUseAdminAction).  When that option is off
// (the default) AW also receives 200.

export default [
  {
    // GET /_arango/experimental/_admin/activities
    //
    // The /_arango/experimental prefix is mandatory: the activities handler
    // checks _request->requestedApiVersion() and returns 405 for any version
    // other than experimentalApiVersion.  The auth check runs before the
    // version check, so unauthorized callers still see 403.
    //
    // Expected (default config, --activities.only-superuser=false):
    //   AU (_sys undef) → 403
    //   AN (_sys none)  → 403
    //   AR (_sys ro)    → 403
    //   AW (_sys rw)    → 200
    //   superuser       → 200
    name: "List activities (GET /_arango/experimental/_admin/activities)",
    type: "admin",
    method: "GET",
    path: "/_arango/experimental/_admin/activities",
  },

  {
    // GET /_admin/async-registry
    //
    // Returns the current async-operation registry as JSON.  No special URL
    // prefix is required.  Guarded solely by canUseAdminAction(MonInternal).
    //
    // Expected:
    //   AU (_sys undef) → 403
    //   AN (_sys none)  → 403
    //   AR (_sys ro)    → 403
    //   AW (_sys rw)    → 200
    //   superuser       → 200
    name: "List async-registry (GET /_admin/async-registry)",
    type: "admin",
    method: "GET",
    path: "/_admin/async-registry",
  },
];
