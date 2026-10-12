/*jshint globalstrict:false, strict:false */
/* global getOptions, runSetup, assertTrue, assertFalse, assertEqual, fail, arango */

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

// Regression test for the fix in RestDatabaseHandler::checkDatabaseAccess():
// GET /_db/_system/_api/database/user must be reachable for an authenticated
// user, even if that user has no access at all to the _system database. This
// is the route the web UI calls right after login to figure out which
// databases to offer for selection. See arangod/RestHandler/RestDatabaseHandler.cpp.

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse} = jsunity.jsUnity.assertions;
const db = require('@arangodb').db;
const arango = require('@arangodb').arango;
let IM = global.instanceManager;

const dbName = "UnitTestsDbUserListAccess";
const userName = "test_no_system_access";
const userPasswd = "testi";

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'runSetup': true
  };
}

if (runSetup === true) {
  let users = require("@arangodb/users");

  db._createDatabase(dbName);

  users.save(userName, userPasswd);
  users.grantDatabase(userName, dbName, "rw");
  // make sure the user has no access whatsoever to _system, regardless of
  // any server-configured default database access for new users
  users.revokeDatabase(userName, "_system");

  return true;
}

function testSuite() {
  return {
    tearDown: function () {
      arango.reconnect(IM.endpoint, "_system", "root", "");
    },

    testUserWithoutSystemAccessCanListOwnDatabases: function () {
      // reconnect using the database the user actually has access to;
      // the user has no access to _system at all
      arango.reconnect(IM.endpoint, dbName, userName, userPasswd);

      let result = arango.GET_RAW("/_db/_system/_api/database/user");
      assertEqual(200, result.code, JSON.stringify(result));
      assertFalse(result.parsedBody.error, JSON.stringify(result));
      assertTrue(Array.isArray(result.parsedBody.result));
      assertTrue(result.parsedBody.result.includes(dbName));
      // the user has no access to _system, so it must not show up
      assertFalse(result.parsedBody.result.includes("_system"));
    },

    testUserWithoutSystemAccessCannotListAllDatabases: function () {
      arango.reconnect(IM.endpoint, dbName, userName, userPasswd);

      // in contrast to /_api/database/user, plain /_api/database is not
      // exempted and still requires access to _system; the user is rejected
      // before even reaching the handler, with "no read access to database"
      let result = arango.GET_RAW("/_db/_system/_api/database");
      assertEqual(401, result.code, JSON.stringify(result));
      assertTrue(result.parsedBody.error, JSON.stringify(result));
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
