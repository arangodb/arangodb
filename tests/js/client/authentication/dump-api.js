/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, assertNotUndefined */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Lars Maier
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const users = require("@arangodb/users");

function AuthSuite() {
  'use strict';

  const user1 = 'hackers@arangodb.com';
  const user2 = 'noone@arangodb.com';

  const cn = 'UnitTestsCollection';
      
  return {
    setUpAll: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");

      try {
        users.remove(user1);
      } catch (err) {
      }
      try {
        users.remove(user2);
      } catch (err) {
      }
    
      try {
        db._dropDatabase('UnitTestsDatabase');
      } catch (err) {
      }
      
      db._createDatabase('UnitTestsDatabase');
      db._useDatabase('UnitTestsDatabase');
      db._create(cn);
      db._useDatabase('_system');

      users.save(user1, "foobar");
      users.save(user2, "foobar");
      users.grantDatabase(user1, 'UnitTestsDatabase');
      users.grantCollection(user1, 'UnitTestsDatabase', "*");
      users.grantDatabase(user2, 'UnitTestsDatabase', 'ro');
      users.grantCollection(user2, 'UnitTestsDatabase', cn, 'none');
      users.reload();

    },

    tearDownAll: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");
      try {
        users.remove(user1);
      } catch (err) {
      }
      try {
        users.remove(user2);
      } catch (err) {
      }

      try {
        db._dropDatabase('UnitTestsDatabase');
      } catch (err) {
      }
    },

    testCreateContextNoPermission: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', "root", "");

      const shards = db[cn].shards(true);
      const shard = Object.keys(shards)[0];
      const server = shards[shard][0];

      // user2 should not be able to create a dump context
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user2, 'foobar');
      let result = arango.POST_RAW(`/_api/dump/start?dbserver=${server}`, {shards: [shard]});
      assertEqual(result.code, 403);

      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user1, 'foobar');
      result = arango.POST_RAW(`/_api/dump/start?dbserver=${server}`, {shards: [shard]});
      assertEqual(result.code, 201);
      let token = result.headers["x-arango-dump-id"];
      assertNotUndefined(token);

      // user2 should not be able to read from the collection using the handle
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user2, 'foobar');
      result = arango.POST_RAW(`/_api/dump/next/${token}?dbserver=${server}&batchId=1`, {});
      assertEqual(result.code, 403);

      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user1, 'foobar');
      result = arango.POST_RAW(`/_api/dump/next/${token}?dbserver=${server}&batchId=1`, {});
      assertEqual(result.code, 204); // collection is empty


      // user 2 should not be able to delete the dump context
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user2, 'foobar');
      result = arango.DELETE_RAW(`/_api/dump/${token}?dbserver=${server}`, {});
      assertEqual(result.code, 403);

      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user1, 'foobar');
      result = arango.DELETE_RAW(`/_api/dump/${token}?dbserver=${server}`, {});
      assertEqual(result.code, 200);
    },
  };
}


jsunity.run(AuthSuite);

return jsunity.done();
