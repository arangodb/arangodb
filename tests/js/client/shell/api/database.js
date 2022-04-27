/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined */

// //////////////////////////////////////////////////////////////////////////////
// / @brief 
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author 
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");

let api = "/_api/database";

function dealing_with_database_information_methodsSuite () {
  return {

    ////////////////////////////////////////////////////////////////////////////////;
    // retrieving the list of databases;
    ////////////////////////////////////////////////////////////////////////////////;

    test_retrieves_the_list_of_databases: function() {
      let doc = arango.GET_RAW(api);

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // retrieving the list of databases for the current user;
    ////////////////////////////////////////////////////////////////////////////////;

    test_retrieves_the_list_of_user_specific_databases: function() {
      let doc = arango.GET_RAW(api + "/user");

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
    },

    ////////////////////////////////////////////////////////////////////////////////;
    // checking information about current database;
    ////////////////////////////////////////////////////////////////////////////////;

    test_retrieves_information_about_the_current_database: function() {
      let doc = arango.GET_RAW(api + "/current");

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody["result"]["name"], "_system", doc);
      assertEqual(typeof doc.parsedBody["result"]["path"], 'string');
      assertTrue(doc.parsedBody["result"]["isSystem"]);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// checking information about current database;
////////////////////////////////////////////////////////////////////////////////;
function dealing_with_database_manipulation_methodsSuite () {
  let name = "UnitTestsDatabase";
  return {

    setUp: function() {
      arango.DELETE(api + `/${name}`);
    },

    tearDown: function() {
      arango.DELETE(api + `/${name}`);
      db._flushCache();
      db._users.toArray().forEach(user => {
        if (user.user !== "root") {
          arango.DELETE_RAW("/_api/user/" + encodeURIComponent(user.user));
        }
      });
    },

    test_creates_a_new_database: function() {
      let body = {"name" : name };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_database_without_a_name: function() {
      let body = "{ }";
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["error"]);
      assertEqual(doc.parsedBody["errorNum"], 1229);
    },

    test_creates_a_database_with_an_empty_name: function() {
      let body = { "name": " " };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["error"]);
      assertEqual(doc.parsedBody["errorNum"], 1229);
    },

    test_creates_a_database_with_an_invalid_name: function() {
      let body = {"name" : `_${name}` };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["error"]);
      assertEqual(doc.parsedBody["errorNum"], 1229);
    },

    test_re_creates_an_existing_database: function() {
      let body = {"name" : name };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      doc = arango.POST_RAW(api, body);
      assertEqual(doc.code, 409);
      assertEqual(doc.headers['content-type'], contentType);

      assertTrue(doc.parsedBody["error"]);
      assertEqual(doc.parsedBody["errorNum"], 1207);
    },

    test_creates_a_database_with_users_EQ_null: function() {
      let body = {"name" : name, "users" : null };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_database_with_users_EQ_empty_array: function() {
      let body = {"name" : name, "users" : [ ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_drops_an_existing_database: function() {
      let cmd = api + `/${name}`;
      let body = {"name" : name };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      doc = arango.DELETE_RAW(cmd);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_drops_a_non_existing_database: function() {
      let cmd = api + `/${name}`;
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 404);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody["errorNum"], 1228);
      assertTrue(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_and_retrieves_the_properties: function() {
      let body = {"name" : name };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);

      //  list of databases should include the new database;
      doc = arango.GET_RAW(api);
      assertEqual(doc.code, 200);

      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === name) >= 0);

      //  retrieve information about _system database;
      doc = arango.GET_RAW(api + "/current");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['result']["name"], "_system");
      assertEqual(typeof doc.parsedBody['result']["path"], 'string');
      assertTrue(doc.parsedBody['result']["isSystem"]);

      //  retrieve information about new database;
      doc = arango.GET_RAW(`/_db/${name}${api}/current`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['result']["name"], name);
      assertEqual(typeof doc.parsedBody['result']["path"], 'string');
      assertFalse(doc.parsedBody['result']["isSystem"]);

      doc = arango.DELETE_RAW(api + `/${name}`);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_with_two_users: function() {
      let body = {"name" : name, "users": [ { "username": "admin", "password": "secret", "extra": { "gender": "m" } }, { "username": "foxx", "active": false } ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);

      //  list of databases should include the new database;
      doc = arango.GET_RAW(api);
      assertEqual(doc.code, 200);

      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === name) >= 0);

      //  retrieve information about new database;
      doc = arango.GET_RAW(`/_db/${name}${api}/current`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody['result']["name"], name);
      assertEqual(typeof doc.parsedBody['result']["path"], 'string');
      assertFalse(doc.parsedBody['result']["isSystem"]);

      //  retrieve information about user "admin";
      doc = arango.GET_RAW("/_db/_system/_api/user/admin");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["user"], "admin");
      assertTrue(doc.parsedBody["active"]);
      assertEqual(doc.parsedBody["extra"]["gender"], "m");

      //  retrieve information about user "foxx";
      doc = arango.GET_RAW("/_db/_system/_api/user/foxx");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["user"], "foxx");
      assertFalse(doc.parsedBody["active"]);

      doc = arango.DELETE_RAW(api + `/${name}`);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_with_two_users__using_user_attribute: function() {
      let body = {"name" : name, "users": [ { "user": "admin", "password": "secret", "extra": { "gender": "m" } }, { "user": "foxx", "active": false } ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);

      //  list of databases should include the new database;
      doc = arango.GET_RAW(api);
      assertEqual(doc.code, 200);

      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === name) >= 0);

      //  retrieve information about new database;
      doc = arango.GET_RAW(`/_db/${name}${api}/current`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["result"]["name"], name);
      assertEqual(typeof doc.parsedBody["result"]["path"], 'string');
      assertFalse(doc.parsedBody["result"]["isSystem"]);

      //  retrieve information about user "admin";
      doc = arango.GET_RAW("/_db/_system/_api/user/admin");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["user"], "admin", doc);
      assertTrue(doc.parsedBody["active"]);
      assertEqual(doc.parsedBody["extra"]["gender"], "m");

      //  retrieve information about user "foxx";
      doc = arango.GET_RAW("/_db/_system/_api/user/foxx");
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["user"], "foxx");
      assertFalse(doc.parsedBody["active"]);

      doc = arango.DELETE_RAW(api + `/${name}`);
      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_with_an_invalid_user_object: function() {
      let body = {"name" : "${name}", "users": [ { } ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["error"]);
    },

    test_creates_a_new_database_with_an_invalid_user: function() {
      let body = {"name" : name, "users": [ { "username": "" } ] };
      let doc = arango.POST_RAW(api, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);

      //  list of databases should include the new database;
      doc = arango.GET_RAW(api);
      assertEqual(doc.code, 200);

      assertTrue(doc.parsedBody["result"].findIndex( x => x === "_system") >= 0);
      assertTrue(doc.parsedBody["result"].findIndex( x => x === name) >= 0);

      //  retrieve information about new database;
      doc = arango.GET_RAW(`/_db/${name}${api}/current`);
      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody["result"]["name"], name);
      assertEqual(typeof doc.parsedBody["result"]["path"], 'string');
      assertFalse(doc.parsedBody["result"]["isSystem"]);

      doc = arango.DELETE_RAW(api + `/${name}`);
      assertEqual(doc.code, 200, doc);
      assertTrue(doc.parsedBody["result"]);
      assertFalse(doc.parsedBody["error"]);
    }
  };
}
jsunity.run(dealing_with_database_information_methodsSuite);
jsunity.run(dealing_with_Unicode_database_namesSuite);
jsunity.run(dealing_with_database_manipulation_methodsSuite);
return jsunity.done();
