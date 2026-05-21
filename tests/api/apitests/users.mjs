// Tests for the /_api/user endpoint family.
//
// Handler: RestUsersHandler
// Mounted at: /_api/user (prefix); accessed here via /_db/_system/_api/user
//
// All tests are type "admin" (5 columns: AU, AN, AR, AW, superuser).
//
// Auth model (without RBAC)
// ──────────────────────────
//   canReadUser(u)   → requires RO access to the _system database
//   canWriteUser(u)  → requires RW access to the _system database
//   AUTHEN           → any authenticated user (no further check)
//
// Expected patterns for the admin user matrix:
//   canReadUser  → AU:403, AN:403, AR:403, AW:200, superuser:200
//   canWriteUser → AU:403, AN:403, AR:403, AW:200, superuser:200
//   AUTHEN       → AU:403, AN:403, AR:403, AW:200, superuser:200
//
// Resources created per-test in setup / cleaned up in teardown:
//   testuser  – a temporary user (user="testuser", passwd="testpasswd")
//   d2        – a temporary database
//   c2        – a collection inside d2

// ── helpers ──────────────────────────────────────────────────────────────────

async function createTestuser(ctx) {
  await ctx.request('DELETE', '/_db/_system/_api/user/testuser');
  const r = await ctx.request('POST', '/_db/_system/_api/user',
    { user: 'testuser', passwd: 'testpasswd' });
  if (r.status !== 201 && r.status !== 200) {
    throw new Error(`setup: failed to create testuser: ${r.status} ${JSON.stringify(r.body)}`);
  }
}

async function deleteTestuser(ctx) {
  await ctx.request('DELETE', '/_db/_system/_api/user/testuser');
}

async function createD2(ctx) {
  await ctx.request('DELETE', '/_db/_system/_api/database/d2');
  const r = await ctx.request('POST', '/_db/_system/_api/database', { name: 'd2' });
  if (r.status !== 201 && r.status !== 200) {
    throw new Error(`setup: failed to create database d2: ${r.status} ${JSON.stringify(r.body)}`);
  }
}

async function deleteD2(ctx) {
  await ctx.request('DELETE', '/_db/_system/_api/database/d2');
}

async function createC2inD2(ctx) {
  const r = await ctx.request('POST', '/_db/d2/_api/collection', { name: 'c2' });
  if (r.status !== 200 && r.status !== 201) {
    throw new Error(`setup: failed to create collection c2 in d2: ${r.status} ${JSON.stringify(r.body)}`);
  }
}

// ── test entries ─────────────────────────────────────────────────────────────

export default [

  // ── GET /_api/user ────────────────────────────────────────────────────────
  {
    // GET /_api/user
    // Auth: canReadUsers(list) → RO on _system.
    // Returns the list of users visible to the caller (filtered by canReadUser).
    // Expected: AU→401, AN→401, AR→403, AW→200, superuser→200
    name: 'List all users (GET /_api/user)',
    type: 'admin',
    method: 'GET',
    path: '/_db/_system/_api/user',
  },

  // ── POST /_api/user ───────────────────────────────────────────────────────
  {
    // POST /_api/user
    // Auth: canWriteUser(u) → RW on _system.
    // Creates user "testuser".
    // setup:    delete testuser if it already exists.
    // teardown: delete testuser regardless of whether the test succeeded.
    // Expected: AU→401, AN→401, AR→403, AW→201, superuser→201
    name: 'Create user testuser (POST /_api/user)',
    type: 'admin',
    method: 'POST',
    path: '/_db/_system/_api/user',
    body: { user: 'testuser', passwd: 'testpasswd' },
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/_system/_api/user/testuser');
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── POST /_api/user/{user} ────────────────────────────────────────────────
  {
    // POST /_api/user/testuser
    // Auth: AUTHEN — just checks the provided credentials; no further
    // authorization needed beyond being authenticated.
    // setup:    create testuser so there is a target to authenticate against.
    // teardown: delete testuser.
    // Expected: AU→200, AN→200, AR→200, AW→200, superuser→200
    name: 'Check user credentials (POST /_api/user/testuser)',
    type: 'admin',
    method: 'POST',
    path: '/_db/_system/_api/user/testuser',
    body: { passwd: 'testpasswd' },
    setup: async (ctx) => {
      await createTestuser(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── GET /_api/user/{user} ─────────────────────────────────────────────────
  {
    // GET /_api/user/testuser
    // Auth: canReadUser(u) → RO on _system.
    // Returns the user record for "testuser".
    // setup:    create testuser.
    // teardown: delete testuser.
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Get user info (GET /_api/user/testuser)',
    type: 'admin',
    method: 'GET',
    path: '/_db/_system/_api/user/testuser',
    setup: async (ctx) => {
      await createTestuser(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── GET /_api/user/{user}/config ──────────────────────────────────────────
  {
    // GET /_api/user/testuser/config
    // Auth: canReadUser(u) → RO on _system.
    // Returns all configuration entries stored for "testuser".
    // setup:    create testuser.
    // teardown: delete testuser.
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Get user config (GET /_api/user/testuser/config)',
    type: 'admin',
    method: 'GET',
    path: '/_db/_system/_api/user/testuser/config',
    setup: async (ctx) => {
      await createTestuser(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── GET /_api/user/{user}/database ────────────────────────────────────────
  {
    // GET /_api/user/testuser/database
    // Auth: canReadUser(u) → RO on _system.
    // Lists all database access levels granted to "testuser".
    // setup:    create testuser.
    // teardown: delete testuser.
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'List database permissions for testuser (GET /_api/user/testuser/database)',
    type: 'admin',
    method: 'GET',
    path: '/_db/_system/_api/user/testuser/database',
    setup: async (ctx) => {
      await createTestuser(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── GET /_api/user/{user}/database/{db} ───────────────────────────────────
  {
    // GET /_api/user/testuser/database/d2
    // Auth: canReadUser(u) → RO on _system.
    // Returns the access level "testuser" has on database "d2".
    // setup:    create testuser and database d2.
    // teardown: delete testuser and drop d2.
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Get user database permission for d2 (GET /_api/user/testuser/database/d2)',
    type: 'admin',
    method: 'GET',
    path: '/_db/_system/_api/user/testuser/database/d2',
    setup: async (ctx) => {
      await createTestuser(ctx);
      await createD2(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
      await deleteD2(ctx);
    },
  },

  // ── GET /_api/user/{user}/database/{db}/{coll} ────────────────────────────
  {
    // GET /_api/user/testuser/database/d2/c2
    // Auth: canReadUser(u) → RO on _system.
    // Returns the access level "testuser" has on collection "c2" in "d2".
    // setup:    create testuser, database d2, and collection c2 inside d2.
    // teardown: delete testuser and drop d2 (which also removes c2).
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Get user collection permission for d2/c2 (GET /_api/user/testuser/database/d2/c2)',
    type: 'admin',
    method: 'GET',
    path: '/_db/_system/_api/user/testuser/database/d2/c2',
    setup: async (ctx) => {
      await createTestuser(ctx);
      await createD2(ctx);
      await createC2inD2(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
      await deleteD2(ctx);
    },
  },

  // ── PUT /_api/user/{user} (full replace) ──────────────────────────────────
  {
    // PUT /_api/user/testuser
    // Auth: canWriteUser(u) → RW on _system.
    // Replaces the "testuser" record (password change is a common use-case).
    // setup:    create testuser.
    // teardown: delete testuser.
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Replace user record (PUT /_api/user/testuser)',
    type: 'admin',
    method: 'PUT',
    path: '/_db/_system/_api/user/testuser',
    body: { passwd: 'newpasswd' },
    setup: async (ctx) => {
      await createTestuser(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── PATCH /_api/user/{user} (partial update) ──────────────────────────────
  {
    // PATCH /_api/user/testuser
    // Auth: canWriteUser(u) → RW on _system.
    // Partially updates the "testuser" record (e.g. active flag).
    // setup:    create testuser.
    // teardown: delete testuser.
    // Expected: AU→401, AN→401, AR→403, AW→200, superuser→200
    name: 'Modify user record (PATCH /_api/user/testuser)',
    type: 'admin',
    method: 'PATCH',
    path: '/_db/_system/_api/user/testuser',
    body: { active: true },
    setup: async (ctx) => {
      await createTestuser(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── PUT /_api/user/{user}/config/{key} ────────────────────────────────────
  {
    // PUT /_api/user/testuser/config/testkey
    // Auth: canWriteUser(u) → RW on _system.
    // Stores a JSON value under the configuration key "testkey" for "testuser".
    // setup:    create testuser.
    // teardown: delete testuser (which removes all associated config entries).
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Set user config key (PUT /_api/user/testuser/config/testkey)',
    type: 'admin',
    method: 'PUT',
    path: '/_db/_system/_api/user/testuser/config/testkey',
    body: { value: 42 },
    setup: async (ctx) => {
      await createTestuser(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── PUT /_api/user/{user}/database/{db} ───────────────────────────────────
  {
    // PUT /_api/user/testuser/database/d2
    // Auth: canWriteUser(u) → RW on _system.
    // Grants "testuser" the specified access level on database "d2".
    // setup:    create testuser and database d2.
    // teardown: delete testuser and drop d2.
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Grant database access (PUT /_api/user/testuser/database/d2)',
    type: 'admin',
    method: 'PUT',
    path: '/_db/_system/_api/user/testuser/database/d2',
    body: { grant: 'ro' },
    setup: async (ctx) => {
      await createTestuser(ctx);
      await createD2(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
      await deleteD2(ctx);
    },
  },

  // ── PUT /_api/user/{user}/database/{db}/{coll} ────────────────────────────
  {
    // PUT /_api/user/testuser/database/d2/c2
    // Auth: canWriteUser(u) → RW on _system.
    // Grants "testuser" the specified access level on collection "c2" in "d2".
    // setup:    create testuser, database d2, and collection c2 inside d2.
    // teardown: delete testuser and drop d2 (which also removes c2).
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Grant collection access (PUT /_api/user/testuser/database/d2/c2)',
    type: 'admin',
    method: 'PUT',
    path: '/_db/_system/_api/user/testuser/database/d2/c2',
    body: { grant: 'ro' },
    setup: async (ctx) => {
      await createTestuser(ctx);
      await createD2(ctx);
      await createC2inD2(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
      await deleteD2(ctx);
    },
  },

  // ── DELETE /_api/user/{user} ──────────────────────────────────────────────
  {
    // DELETE /_api/user/testuser
    // Auth: canWriteUser(u) → RW on _system.
    // Deletes the "testuser" account entirely.
    // setup:    create testuser so there is something to delete.
    // teardown: attempt deletion again (idempotent — 404 is fine) to ensure
    //           the user is gone even if the test user lacked permission.
    // Expected: AU→403, AN→403, AR→403, AW→202, superuser→202
    name: 'Delete user testuser (DELETE /_api/user/testuser)',
    type: 'admin',
    method: 'DELETE',
    path: '/_db/_system/_api/user/testuser',
    setup: async (ctx) => {
      await createTestuser(ctx);
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── DELETE /_api/user/{user}/config/{key} ─────────────────────────────────
  {
    // DELETE /_api/user/testuser/config/testkey
    // Auth: canWriteUser(u) → RW on _system.
    // Removes the configuration entry "testkey" from "testuser".
    // setup:    create testuser and store the config key so there is something
    //           to delete.
    // teardown: delete testuser.
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Delete user config key (DELETE /_api/user/testuser/config/testkey)',
    type: 'admin',
    method: 'DELETE',
    path: '/_db/_system/_api/user/testuser/config/testkey',
    setup: async (ctx) => {
      await createTestuser(ctx);
      // Store a value so the key exists before the test tries to delete it.
      await ctx.request('PUT', '/_db/_system/_api/user/testuser/config/testkey',
        { value: 1 });
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
    },
  },

  // ── DELETE /_api/user/{user}/database/{db} ────────────────────────────────
  {
    // DELETE /_api/user/testuser/database/d2
    // Auth: canWriteUser(u) → RW on _system.
    // Revokes all database-level permissions "testuser" has on "d2".
    // setup:    create testuser, database d2, and grant testuser ro access on d2
    //           so there is a permission to revoke.
    // teardown: delete testuser and drop d2.
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Revoke database permission (DELETE /_api/user/testuser/database/d2)',
    type: 'admin',
    method: 'DELETE',
    path: '/_db/_system/_api/user/testuser/database/d2',
    setup: async (ctx) => {
      await createTestuser(ctx);
      await createD2(ctx);
      // Grant ro so the permission exists; the test will revoke it.
      await ctx.request('PUT', '/_db/_system/_api/user/testuser/database/d2',
        { grant: 'ro' });
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
      await deleteD2(ctx);
    },
  },

  // ── DELETE /_api/user/{user}/database/{db}/{coll} ─────────────────────────
  {
    // DELETE /_api/user/testuser/database/d2/c2
    // Auth: canWriteUser(u) → RW on _system.
    // Revokes all collection-level permissions "testuser" has on "c2" in "d2".
    // setup:    create testuser, database d2, collection c2 in d2, and grant
    //           testuser ro access on c2 so there is a permission to revoke.
    // teardown: delete testuser and drop d2 (which also removes c2).
    // Expected: AU→403, AN→403, AR→403, AW→200, superuser→200
    name: 'Revoke collection permission (DELETE /_api/user/testuser/database/d2/c2)',
    type: 'admin',
    method: 'DELETE',
    path: '/_db/_system/_api/user/testuser/database/d2/c2',
    setup: async (ctx) => {
      await createTestuser(ctx);
      await createD2(ctx);
      await createC2inD2(ctx);
      // Grant ro so the permission exists; the test will revoke it.
      await ctx.request('PUT', '/_db/_system/_api/user/testuser/database/d2/c2',
        { grant: 'ro' });
    },
    teardown: async (ctx) => {
      await deleteTestuser(ctx);
      await deleteD2(ctx);
    },
  },

];
