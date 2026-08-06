/*jshint globalstrict:false, strict:false */
/* global getOptions, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// Authorization questions asked by the /_api/user endpoint family.
//
// Observation-based counterpart of tests/api/apitests/users.mjs.
//
// Handler: arangod/RestHandler/RestUsersHandler.cpp
//
// IMPORTANT - base UseDatabase check:
// RestUsersHandler overrides checkUserCanAccess(): for an *authenticated*
// request whose (db-stripped) request path starts with "/_api/user/" it
// returns OK *without* running the base RestHandler::checkUserCanAccess()
// check. Consequently:
//   - GET  /_api/user           (path "/_api/user",  no trailing slash) and
//   - POST /_api/user           (path "/_api/user")
//     DO go through the base check -> `UseDatabase name=_system level=read`.
//   - every /_api/user/<user>[/...] request SKIPS the base check, so the only
//     questions observed are the ones the handler body asks itself.
//
// Handler body questions (ExecContext helpers):
//   canReadUser(u)            -> `ReadUser name=<u>`
//   canCreateUser(u)          -> `CreateUser name=<u>`
//   canDropUser(u)            -> `DropUser name=<u>`
//   canModifyUserProfile(u)   -> `ModifyUserProfile name=<u>`
//   canGrantUserPermissions(u)-> `GrantUserPermissions name=<u>`
// canReadUser/canModifyUserProfile have a "target == connected user" exception
// that returns OK without asking; our target is `testuser` (!= root), so the
// question is always asked. NEVER target `root`.
//
// All of them except canReadUser also consult the server-wide read-only gate,
// which is traced as the pseudo-question `IsReadOnly`. In canModifyUserProfile
// the gate is consulted even when the self-exception skipped the permission
// question, so modifying your own profile still traces `IsReadOnly` - just no
// `ModifyUserProfile`.
//
// All requests go to the _system database (paths /_db/_system/_api/user/...).
// Per-endpoint preconditions (testuser, database d2, collection c2 in d2,
// grants) are recreated inline BEFORE beginObserve() with arango.*_RAW (they
// run as root, before observation, so they do not pollute the observed
// questions) and cleaned up afterwards / in tearDown.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false'
  };
}

const jsunity = require('jsunity');
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');
const { singleOnly } = require('@arangodb/testutils/apitest-fixtures');
// a single server reads the _users collection while storing a user or a
// permission; a coordinator writes it via the agency and asks nothing

function userApiAuthzSuite () {
  const testuser = 'testuser';

  function dropTestuser () {
    arango.DELETE_RAW(`/_db/_system/_api/user/${testuser}`);
  }
  function createTestuser () {
    dropTestuser();
    arango.POST_RAW(`/_db/_system/_api/user`,
                    { user: testuser, passwd: 'testpasswd' });
  }
  function dropD2 () {
    arango.DELETE_RAW(`/_db/_system/_api/database/d2`);
  }
  function createD2 () {
    dropD2();
    arango.POST_RAW(`/_db/_system/_api/database`, { name: 'd2' });
  }
  function createC2inD2 () {
    arango.POST_RAW(`/_db/d2/_api/collection`, { name: 'c2' });
  }

  return {
    tearDown: function () {
      disableObserve();
      dropTestuser();
      dropD2();
    },

    // GET /_api/user - list users. Path "/_api/user" (no trailing slash) does
    // NOT skip the base check, so `UseDatabase name=_system level=read` is
    // asked. Then canUseAdminAction(AdminReadUsers) -> `AdminReadUsers`, then
    // canReadUsers() -> `ReadUser name=<u>` for EVERY user (note: this bulk
    // variant does NOT apply the self-exception, so root is included).
    // AUDIT: the user list is enumerated dynamically; AdminReadUsers is an
    // admin action asked in addition to the per-user ReadUser questions.
    testListUsers: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/user`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        "AdminReadUsers",
        "ReadUser name=root",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read"
        ])
      ], endObserve());
    },

    // POST /_api/user - create testuser. Path "/_api/user" -> base check runs.
    // canCreateUser(testuser) -> `CreateUser name=testuser`.
    testCreateUser: function () {
      dropTestuser();
      beginObserve();
      arango.POST_RAW(`/_db/_system/_api/user`,
                      { user: testuser, passwd: 'testpasswd' });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        "IsReadOnly",
        "CreateUser name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
      dropTestuser();
    },

    // POST /_api/user/testuser - check credentials. Path skips the base check
    // and the handler only calls um->checkCredentials() (no can()), so NO
    // authorization question is asked at all.
    // AUDIT: empty expectation - this endpoint is AUTHEN-only.
    testCheckCredentials: function () {
      createTestuser();
      beginObserve();
      arango.POST_RAW(`/_db/_system/_api/user/${testuser}`,
                      { passwd: 'testpasswd' });
      assertPermissions(["UseApiVersion version=0"], endObserve());
    },

    // GET /_api/user/testuser - canReadUser(testuser)
    testGetUser: function () {
      createTestuser();
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/user/${testuser}`);
      assertPermissions([
        "UseApiVersion version=0",
        "ReadUser name=testuser"
      ], endObserve());
    },

    // GET /_api/user/testuser/config - canReadUser(testuser)
    testGetUserConfig: function () {
      createTestuser();
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/user/${testuser}/config`);
      assertPermissions([
        "UseApiVersion version=0",
        "ReadUser name=testuser"
      ], endObserve());
    },

    // GET /_api/user/testuser/database - canReadUser(testuser); the database
    // enumeration only reads stored auth levels (no can()).
    testGetUserDatabases: function () {
      createTestuser();
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/user/${testuser}/database`);
      assertPermissions([
        "UseApiVersion version=0",
        "ReadUser name=testuser"
      ], endObserve());
    },

    // GET /_api/user/testuser/database/d2 - canReadUser(testuser)
    testGetUserDatabase: function () {
      createTestuser();
      createD2();
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/user/${testuser}/database/d2`);
      assertPermissions([
        "UseApiVersion version=0",
        "ReadUser name=testuser"
      ], endObserve());
    },

    // GET /_api/user/testuser/database/d2/c2 - canReadUser(testuser)
    testGetUserCollection: function () {
      createTestuser();
      createD2();
      createC2inD2();
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/user/${testuser}/database/d2/c2`);
      assertPermissions([
        "UseApiVersion version=0",
        "ReadUser name=testuser"
      ], endObserve());
    },

    // PUT /_api/user/testuser - canModifyUserProfile(testuser)
    testReplaceUser: function () {
      createTestuser();
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_api/user/${testuser}`,
                     { passwd: 'newpasswd' });
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        "ModifyUserProfile name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },

    // PATCH /_api/user/testuser - canModifyUserProfile(testuser)
    testModifyUser: function () {
      createTestuser();
      beginObserve();
      arango.PATCH_RAW(`/_db/_system/_api/user/${testuser}`, { active: true });
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        "ModifyUserProfile name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },

    // PUT /_api/user/testuser/config/testkey - canModifyUserProfile(testuser)
    testSetUserConfig: function () {
      createTestuser();
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_api/user/${testuser}/config/testkey`,
                     { value: 42 });
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        "ModifyUserProfile name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },

    // PUT /_api/user/testuser/database/d2 - canGrantUserPermissions(testuser)
    testGrantDatabase: function () {
      createTestuser();
      createD2();
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_api/user/${testuser}/database/d2`,
                     { grant: 'ro' });
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        "GrantUserPermissions name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },

    // PUT /_api/user/testuser/database/d2/c2 - canGrantUserPermissions(testuser)
    // (existsCollection() only resolves the collection, no can())
    testGrantCollection: function () {
      createTestuser();
      createD2();
      createC2inD2();
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_api/user/${testuser}/database/d2/c2`,
                     { grant: 'ro' });
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        "GrantUserPermissions name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },

    // DELETE /_api/user/testuser - canDropUser(testuser)
    testDeleteUser: function () {
      createTestuser();
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/user/${testuser}`);
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        "DropUser name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },

    // DELETE /_api/user/testuser/config/testkey - canModifyUserProfile(testuser)
    testDeleteUserConfig: function () {
      createTestuser();
      arango.PUT_RAW(`/_db/_system/_api/user/${testuser}/config/testkey`,
                     { value: 1 });
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/user/${testuser}/config/testkey`);
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        "ModifyUserProfile name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },

    // DELETE /_api/user/testuser/database/d2 - canGrantUserPermissions(testuser)
    testRevokeDatabase: function () {
      createTestuser();
      createD2();
      arango.PUT_RAW(`/_db/_system/_api/user/${testuser}/database/d2`,
                     { grant: 'ro' });
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/user/${testuser}/database/d2`);
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        "GrantUserPermissions name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },

    // DELETE /_api/user/testuser/database/d2/c2 -
    // canGrantUserPermissions(testuser)
    testRevokeCollection: function () {
      createTestuser();
      createD2();
      createC2inD2();
      arango.PUT_RAW(`/_db/_system/_api/user/${testuser}/database/d2/c2`,
                     { grant: 'ro' });
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/user/${testuser}/database/d2/c2`);
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        "GrantUserPermissions name=testuser",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },
  };
}

jsunity.run(userApiAuthzSuite);
return jsunity.done();
