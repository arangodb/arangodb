// Tests for the /_api/view endpoint family.
//
// Handler: RestViewHandler (RestVocbaseBaseHandler)
// Mounted at: /_db/<name>/_api/view (prefix)
//
// All tests use database 'd' (created by global setup) and, for views that
// link to a collection, collection 'c' inside 'd'.  Mutating operations
// (create, drop, rename, modify properties) use a temporary view
// 'v_apitest' (or 'v_apitest_rename' for the rename tests) so that no
// permanent state is left behind after each matrix cell.
//
// All tests are type "all" (shorthand for ["collection", "database", "admin"]).
//
// Auth model (without RBAC)
// ──────────────────────────
// RestVocbaseBaseHandler checks at least some DB access before the handler
// is reached:
//   DB=undef (U) → 401   DB=none  (N) → 401
//   DB=ro    (R) → read operations succeed; write operations → 403
//   DB=rw    (W) → all operations succeed
//   (admin users AU/AN have no d access → 401; AR/AW/SU use their d-access)
//
// Per-operation mapping (without RBAC):
//   canSeeView       → DB RO (no-op beyond the DB access check; all visible)
//   canCreateView    → DB RW
//   canDropView      → DB RW
//   canUseView(RO)   → DB RO
//   canUseView(modify)→ DB RW
//   canRenameView    → DB RW
//
// Note: view operations additionally verify that the user has RO access to
// all linked collections (via canUseCollection(RO)).  Collection 'c' is used
// for links; without RBAC the collection access level simply tracks the DB
// access level, so this does not add a separate dimension in the matrix.

// ── constants ────────────────────────────────────────────────────────────────

const TEST_VIEW       = 'v_apitest';
const TEST_VIEW_NEW   = 'v_apitest_new';

// Minimal arangosearch view body (linked to collection c in database d).
const TEST_VIEW_BODY  = {
  name: TEST_VIEW,
  type: 'arangosearch',
  links: { c: { includeAllFields: true } },
};

// ── helpers ──────────────────────────────────────────────────────────────────

async function createView(ctx) {
  // Remove any stale view before creating a fresh one.
  await ctx.request('DELETE', `/_db/d/_api/view/${TEST_VIEW}`);
  const r = await ctx.request('POST', '/_db/d/_api/view', TEST_VIEW_BODY);
  if (r.status !== 200 && r.status !== 201) {
    throw new Error(`setup: failed to create view ${TEST_VIEW}: ${r.status} ${JSON.stringify(r.body)}`);
  }
}

async function deleteView(ctx) {
  await ctx.request('DELETE', `/_db/d/_api/view/${TEST_VIEW}`);
}

async function createRenameView(ctx) {
  // A dedicated view used for rename tests.  The test will try to rename it
  // to TEST_VIEW_NEW; teardown cleans up either name.
  await ctx.request('DELETE', `/_db/d/_api/view/${TEST_VIEW}`);
  await ctx.request('DELETE', `/_db/d/_api/view/${TEST_VIEW_NEW}`);
  const r = await ctx.request('POST', '/_db/d/_api/view',
    { name: TEST_VIEW, type: 'arangosearch', links: { c: { includeAllFields: true } } });
  if (r.status !== 200 && r.status !== 201) {
    throw new Error(`setup: failed to create rename view ${TEST_VIEW}: ${r.status} ${JSON.stringify(r.body)}`);
  }
}

async function deleteRenameView(ctx) {
  // The test may have renamed the view; clean up both possible names.
  await ctx.request('DELETE', `/_db/d/_api/view/${TEST_VIEW}`);
  await ctx.request('DELETE', `/_db/d/_api/view/${TEST_VIEW_NEW}`);
}

// ── test entries ─────────────────────────────────────────────────────────────

export default [

  // ── GET /_db/d/_api/view ──────────────────────────────────────────────────
  {
    // GET /_api/view
    // Auth: canSeeView → DB RO (no-op; list is filtered to visible views only).
    // Sets up a view so the list is non-trivially populated; teardown removes it.
    // Expected: DB-undef→401, DB-none→401, DB-ro→200, DB-rw→200
    name: 'List views in database d (GET /_db/d/_api/view)',
    type: 'all',
    method: 'GET',
    path: '/_db/d/_api/view',
    setup: async (ctx) => {
      await createView(ctx);
    },
    teardown: async (ctx) => {
      await deleteView(ctx);
    },
  },

  // ── POST /_db/d/_api/view ─────────────────────────────────────────────────
  {
    // POST /_api/view
    // Auth: canCreateView → DB RW.
    // Creates view v_apitest linked to collection c.
    // setup:    remove v_apitest if it already exists.
    // teardown: remove v_apitest regardless of whether the test succeeded.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→201
    name: 'Create arangosearch view (POST /_db/d/_api/view)',
    type: 'all',
    method: 'POST',
    path: '/_db/d/_api/view',
    body: TEST_VIEW_BODY,
    setup: async (ctx) => {
      await ctx.request('DELETE', `/_db/d/_api/view/${TEST_VIEW}`);
    },
    teardown: async (ctx) => {
      await deleteView(ctx);
    },
  },

  // ── GET /_db/d/_api/view/{name} ───────────────────────────────────────────
  {
    // GET /_api/view/{name}
    // Auth: canUseView(RO) → DB RO.
    // Returns the identity and type of the view.
    // setup:    create v_apitest.
    // teardown: delete v_apitest.
    // Expected: DB-undef→401, DB-none→401, DB-ro→200, DB-rw→200
    name: 'Get view identity (GET /_db/d/_api/view/v_apitest)',
    type: 'all',
    method: 'GET',
    path: `/_db/d/_api/view/${TEST_VIEW}`,
    setup: async (ctx) => {
      await createView(ctx);
    },
    teardown: async (ctx) => {
      await deleteView(ctx);
    },
  },

  // ── GET /_db/d/_api/view/{name}/properties ────────────────────────────────
  {
    // GET /_api/view/{name}/properties
    // Auth: canUseView(RO) → DB RO.
    // Returns the full properties (links, consolidation policy, etc.)
    // of the view.
    // setup:    create v_apitest.
    // teardown: delete v_apitest.
    // Expected: DB-undef→401, DB-none→401, DB-ro→200, DB-rw→200
    name: 'Get view properties (GET /_db/d/_api/view/v_apitest/properties)',
    type: 'all',
    method: 'GET',
    path: `/_db/d/_api/view/${TEST_VIEW}/properties`,
    setup: async (ctx) => {
      await createView(ctx);
    },
    teardown: async (ctx) => {
      await deleteView(ctx);
    },
  },

  // ── PATCH /_db/d/_api/view/{name}/properties ──────────────────────────────
  {
    // PATCH /_api/view/{name}/properties
    // Auth: canUseView(modify) → DB RW.
    // Partially updates view properties; sending an empty object {} is a
    // valid no-op (leaves all properties unchanged).
    // setup:    create v_apitest.
    // teardown: delete v_apitest.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200
    name: 'Partially update view properties (PATCH /_db/d/_api/view/v_apitest/properties)',
    type: 'all',
    method: 'PATCH',
    path: `/_db/d/_api/view/${TEST_VIEW}/properties`,
    body: { cleanupIntervalStep: 2 },
    setup: async (ctx) => {
      await createView(ctx);
    },
    teardown: async (ctx) => {
      await deleteView(ctx);
    },
  },

  // ── PUT /_db/d/_api/view/{name}/properties ────────────────────────────────
  {
    // PUT /_api/view/{name}/properties
    // Auth: canUseView(modify) → DB RW.
    // Replaces all view properties with the supplied object; {} resets to
    // defaults (empty links map, default consolidation policy, etc.).
    // setup:    create v_apitest.
    // teardown: delete v_apitest.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200
    name: 'Replace view properties (PUT /_db/d/_api/view/v_apitest/properties)',
    type: 'all',
    method: 'PUT',
    path: `/_db/d/_api/view/${TEST_VIEW}/properties`,
    body: {},
    setup: async (ctx) => {
      await createView(ctx);
    },
    teardown: async (ctx) => {
      await deleteView(ctx);
    },
  },

  // ── PATCH /_db/d/_api/view/{name}/rename ──────────────────────────────────
  {
    // PATCH /_api/view/{name}/rename
    // Auth: canRenameView → DB RW.
    // Renames the view in-place.  The teardown removes both the original name
    // (v_apitest) and the new name (v_apitest_new) so the state is clean
    // regardless of whether the rename succeeded.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200
    name: 'Rename view via PATCH (PATCH /_db/d/_api/view/v_apitest/rename)',
    type: 'all',
    method: 'PATCH',
    path: `/_db/d/_api/view/${TEST_VIEW}/rename`,
    body: { name: TEST_VIEW_NEW },
    setup: async (ctx) => {
      await createRenameView(ctx);
    },
    teardown: async (ctx) => {
      await deleteRenameView(ctx);
    },
  },

  // ── PUT /_db/d/_api/view/{name}/rename ────────────────────────────────────
  {
    // PUT /_api/view/{name}/rename
    // Auth: canRenameView → DB RW.
    // PUT and PATCH on /rename are handled by the same code path;
    // the result is identical.  Uses separate setup/teardown so that both
    // entries run independently.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200
    name: 'Rename view via PUT (PUT /_db/d/_api/view/v_apitest/rename)',
    type: 'all',
    method: 'PUT',
    path: `/_db/d/_api/view/${TEST_VIEW}/rename`,
    body: { name: TEST_VIEW_NEW },
    setup: async (ctx) => {
      await createRenameView(ctx);
    },
    teardown: async (ctx) => {
      await deleteRenameView(ctx);
    },
  },

  // ── DELETE /_db/d/_api/view/{name} ────────────────────────────────────────
  {
    // DELETE /_api/view/{name}
    // Auth: canDropView → DB RW.
    // Drops the view entirely.
    // setup:    create v_apitest so the test user has something to drop.
    // teardown: delete v_apitest in case the test user lacked permission.
    // Expected: DB-undef→401, DB-none→401, DB-ro→403, DB-rw→200
    name: 'Drop view (DELETE /_db/d/_api/view/v_apitest)',
    type: 'all',
    method: 'DELETE',
    path: `/_db/d/_api/view/${TEST_VIEW}`,
    setup: async (ctx) => {
      await createView(ctx);
    },
    teardown: async (ctx) => {
      await deleteView(ctx);
    },
  },

];
