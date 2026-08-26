/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, getOptions */
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
// /
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.authentication': 'true',
  };
}

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const users = require("@arangodb/users");
const request = require("@arangodb/request");
let IM = global.instanceManager;

const dbName = "TestDatabase";
const cn = "TestCollection";
const userName = "aardvark-regression-user";
const userPassword = "testi";

function aardvarkAuthBypassRegressionTestSuite () {
  return {
    setUp : function () {
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._create(cn);
      db._useDatabase("_system");

      try { users.remove(userName); } catch (e) {}
      users.save(userName, userPassword);
      // explicitly deny this user any access to the database
      users.grantDatabase(userName, dbName, "none");
    },

    tearDown : function () {
      db._useDatabase("_system");
      try { users.remove(userName); } catch (e) {}
      try { db._dropDatabase(dbName); } catch (e) {}
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that the aardvark UI's query profiling endpoint cannot be used
/// to run AQL with escalated (superuser) permissions for a user who has no
/// access to the database at all
////////////////////////////////////////////////////////////////////////////////

    testAardvarkQueryProfileCannotBypassAuthorization : function () {
      const res = request.post({
        url: IM.url + "/_db/" + dbName + "/_admin/aardvark/query/profile",
        auth: {
          username: userName,
          password: userPassword,
        },
        body: {
          query: "INSERT {Hello:1} INTO " + cn,
        },
        json: true,
      });

      // the request must be declined, not silently executed with superuser
      // rights
      assertEqual(401, res.status);

      // verify that the query was indeed never executed
      db._useDatabase(dbName);
      try {
        assertEqual(0, db._collection(cn).count());
      } finally {
        db._useDatabase("_system");
      }
    },

  };
}

jsunity.run(aardvarkAuthBypassRegressionTestSuite);

return jsunity.done();
