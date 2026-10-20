/*jshint globalstrict:false, strict:false */
/* global getOptions, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const {assertEqual, assertNotUndefined} = jsunity.jsUnity.assertions;
const arango = require("@arangodb").arango;
const db = require("@arangodb").db;
const users = require('@arangodb/users');
const helper = require('@arangodb/testutils/user-helper');

// configured (not effective) grants of `user`, keyed by database name
function configuredGrants(user) {
  const res = arango.GET(`/_api/user/${user}/database?full=true`);
  assertEqual(false, res.error);
  return res.result;
}

// effective (fallback-resolved) grant of `user` on `dbName`
function effectiveGrant(user, dbName) {
  const res = arango.GET(`/_api/user/${user}/database/${dbName}`);
  assertEqual(false, res.error);
  return res.result;
}

function testSuite() {
  const system = '_system';
  const dbName = 'AuthCreateDatabaseGrantsDb';
  // `switchUser` only knows the empty password for 'root' and 'bob'
  const admin = 'bob';

  function dropTestDatabase() {
    try {
      db._dropDatabase(dbName);
    } catch (err) {
      // did not exist
    }
  }

  return {
    setUp: function () {
      helper.switchUser('root', system);
      dropTestDatabase();
    },

    tearDown: function () {
      helper.switchUser('root', system);
      dropTestDatabase();
      try {
        users.remove(admin);
      } catch (err) {
        // was not created
      }
      users.reload();
    },

    // root's access comes from its `*` wildcard grant, so no explicit grant
    // may be written for the new database.
    testRootGetsNoExplicitGrantOnCreatedDatabase: function () {
      db._createDatabase(dbName);

      const grants = configuredGrants('root');
      assertNotUndefined(grants[dbName],
                         `${dbName} missing from the full listing`);
      assertEqual('undefined', grants[dbName].permission,
                  `expected no configured grant for ${dbName}`);
      assertEqual('undefined', grants[dbName].collections['*'],
                  `expected no configured collection grant for ${dbName}`);
      // this is what actually grants the access
      assertEqual('rw', grants['*'].permission);
    },

    // Skipping the grant must not cost root any access.
    testRootRetainsEffectiveAccessOnCreatedDatabase: function () {
      db._createDatabase(dbName);

      const res = arango.GET(`/_api/user/root/database/${dbName}`);
      assertEqual(false, res.error);
      assertEqual('rw', res.result);

      db._useDatabase(dbName);
      try {
        db._create('someCollection');
        assertNotUndefined(db._collection('someCollection'));
      } finally {
        db._useDatabase(system);
      }
    },

    // The point of not writing the grant: the new database must keep following
    // the grant it inherits. A redundant explicit `rw` grant would shadow the
    // `_system` fallback and survive narrowing it.
    testCreatedDatabaseKeepsFollowingTheInheritedGrant: function () {
      users.save(admin, '');
      users.grantDatabase(admin, system, 'rw');
      users.reload();

      helper.switchUser(admin, system);
      try {
        db._createDatabase(dbName);
      } finally {
        helper.switchUser('root', system);
      }
      assertEqual('rw', effectiveGrant(admin, dbName));

      users.grantDatabase(admin, system, 'ro');
      users.reload();
      assertEqual('ro', effectiveGrant(admin, dbName),
                  `${dbName} no longer follows the inherited _system grant`);
    },

    // An admin whose access to the new database comes from the `_system`
    // fallback rather than a `*` wildcard grant must equally not get one.
    testAdminWithoutWildcardGrantGetsNoExplicitGrant: function () {
      users.save(admin, '');
      users.grantDatabase(admin, system, 'rw');
      users.reload();

      helper.switchUser(admin, system);
      try {
        db._createDatabase(dbName);
      } finally {
        helper.switchUser('root', system);
      }

      const grants = configuredGrants(admin);
      assertNotUndefined(grants[dbName],
                         `${dbName} missing from the full listing`);
      assertEqual('undefined', grants[dbName].permission,
                  `expected no configured grant for ${dbName}`);
      assertEqual('undefined', grants[dbName].collections['*'],
                  `expected no configured collection grant for ${dbName}`);
    },
  }; // return

} // end of suite

jsunity.run(testSuite);
return jsunity.done();
