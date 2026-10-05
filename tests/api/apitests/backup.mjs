// Tests for the /_admin/backup/* endpoints (Enterprise Edition only).
//
// The handler is registered only when both USE_ENTERPRISE is compiled in
// and --backup.api-enabled is not set to "false" (default: "true").
// If the tests are run against a Community Edition build, all requests will
// return 404 (route not registered) instead of the expected codes below.
//
// Auth guard (RestHotBackupHandler::verifyPermitted):
//   default (--backup.api-enabled=true): canUseAdminAction(AdminBackup)
//     → without RBAC: requires RW access on _system
//     → expected columns: AU→403, AN→403, AR→403, AW→<ok>, superuser→<ok>
//   jwt mode (--backup.api-enabled=jwt): isSuperuser only
//     → expected columns: AU→403, AN→403, AR→403, AW→403, superuser→<ok>
//
// All backup operations use POST — any other method returns 405.
//
// The following sub-paths are covered here:
//   /_admin/backup/list    (safe read, no side effects)
//   /_admin/backup/create  (creates a snapshot, cleaned up in teardown)
//   /_admin/backup/delete  (destructive, requires an existing backup in setup)
//
// /_admin/backup/upload, /_admin/backup/download and /_admin/backup/restore
// require additional infrastructure (rclone, transfer config, running backup)
// and are intentionally omitted.

export default [
  {
    // POST /_admin/backup/list
    //
    // Lists all available hot backups on the local server (or across all
    // DB servers when running on a coordinator).  The response body contains
    // a "result.list" object keyed by backup IDs.  This is a pure read
    // operation with no side effects — no setup or teardown needed.
    //
    // Expected:
    //   AU (_sys undef) → 403
    //   AN (_sys none)  → 403
    //   AR (_sys ro)    → 403
    //   AW (_sys rw)    → 200
    //   superuser       → 200
    name: "List hot backups (POST /_admin/backup/list)",
    type: "admin",
    method: "POST",
    path: "/_admin/backup/list",
    body: {},
  },

  {
    // POST /_admin/backup/create
    //
    // Creates a RocksDB checkpoint on the local server and returns the
    // resulting backup ID in result.id.  On success the handler returns
    // HTTP 201 CREATED (uniquely, all other backup sub-paths return 200).
    //
    // We use a fixed label ("apitestcreate") so the teardown can identify
    // and delete exactly the backup(s) our test columns produced, without
    // touching any unrelated pre-existing backups.
    //
    // Expected:
    //   AU (_sys undef) → 403   (no backup written)
    //   AN (_sys none)  → 403   (no backup written)
    //   AR (_sys ro)    → 403   (no backup written)
    //   AW (_sys rw)    → 201   (backup created)
    //   superuser       → 201   (backup created)
    name: "Create a hot backup (POST /_admin/backup/create)",
    type: "admin",
    method: "POST",
    path: "/_admin/backup/create",
    body: { label: "apitestcreate" },

    teardown: async (ctx) => {
      // List all backups and delete any that carry our test label.
      // The unauthorised columns (AU/AN/AR) do not create a backup, so
      // their teardown is a no-op.  AW and superuser both create one.
      const listResp = await ctx.request('POST', '/_admin/backup/list', {});
      const list =
        (listResp.body && listResp.body.result && listResp.body.result.list)
        ? listResp.body.result.list
        : {};
      for (const id of Object.keys(list)) {
        if (id.includes('apitestcreate')) {
          await ctx.request('POST', '/_admin/backup/delete', { id });
        }
      }
    },
  },

  {
    // POST /_admin/backup/delete
    //
    // Deletes the hot backup identified by the "id" field in the request
    // body.  Returns 200 on success, 404 if the backup does not exist.
    //
    // Setup creates a fresh backup (as superuser) before each matrix column
    // so that authorised columns (AW, superuser) have something to delete.
    // The teardown then tries to delete the same backup again; for the
    // authorised columns it will already be gone (getting a 404 that we
    // silently ignore), while for the unauthorised columns (AU/AN/AR) the
    // backup was never deleted by the test itself and teardown removes it.
    //
    // Expected:
    //   AU (_sys undef) → 403   (backup untouched, teardown cleans up)
    //   AN (_sys none)  → 403   (backup untouched, teardown cleans up)
    //   AR (_sys ro)    → 403   (backup untouched, teardown cleans up)
    //   AW (_sys rw)    → 200   (backup deleted)
    //   superuser       → 200   (backup deleted)
    name: "Delete a hot backup (POST /_admin/backup/delete)",
    type: "admin",
    method: "POST",
    path: "/_admin/backup/delete",
    // ctx.data.id is populated by setup; resolveDeep() will interpolate it.
    body: { id: "${ctx.data.id}" },

    setup: async (ctx) => {
      // Create a backup as superuser and return its ID so that ctx.data.id
      // is available when the request body is resolved.
      const r = await ctx.request('POST', '/_admin/backup/create', {
        label: "apitestdelete",
      });
      return { id: r.body.result.id };
    },

    teardown: async (ctx) => {
      // Delete the backup that setup created.  For authorised columns
      // (AW, superuser) the backup was already deleted by the test itself,
      // so the delete call returns 404 — we ignore it silently.
      if (ctx.data && ctx.data.id) {
        await ctx.request('POST', '/_admin/backup/delete', { id: ctx.data.id });
      }
    },
  },
];
