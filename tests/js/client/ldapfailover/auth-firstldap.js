/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication (dual ldap)
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2021 triagens GmbH, Cologne, Germany
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
/// @author Frank Celler
/// @author Copyright 2021, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const users = require("@arangodb/users");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AuthSuite() {
  'use strict';
  var baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  // hardcoded in testsuite
  const jwtSecret = 'haxxmann';
  const dbname = 'test';
  const role1 = "role1";
  const role2 = "adminrole";
  const role3 = "bundeskanzlerin";
  const roles = [role1, role2, role3];
  
  return {
    setUpAll: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");

      for (const role of roles) {
        try {
          users.remove(":role:" + role);
        }
        catch (err) {
        }
      }

      try {
        db._dropDatabase(dbname);
      } catch (err) {
      }
    },

    tearDownAll: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");

      for (const role of roles) {
        try {
          users.remove(":role:" + role);
        }
        catch (err) {
        }
      }

      try {
        db._dropDatabase(dbname);
      } catch (err) {
      }
    },
    
    testCreateRoles: function () {
      for (const role of roles) {
         const fr = ":role:" + role;
         let r = users.save(fr, "password", true);
         assertEqual(fr, r.user);
      }

      users.reload();

      for (const role of roles) {
         const fr = ":role:" + role;
         assertTrue(users.exists(fr));
      }
    },

    testDatabases: function () {
      let r = db._createDatabase(dbname);
      assertTrue(r);

      users.grantDatabase(":role:" + role1, dbname, "rw");
      users.grantDatabase(":role:" + role2, dbname, "rw");
      users.grantDatabase(":role:" + role3, dbname, "rw");
    },

    testRole1: function () {
      // both LDAP will answer for user1 resp. user2
      const user1 = "user1";
      const user2 = user1 + ",dc=arangodb";

      // this will fail, as the PW is wrong
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user1, "");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }

      // this will succeed as the PW is correct and LDAP1 is active and responsible
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user1, "abc");
	assertTrue(true);
      } catch (e) {
	assertTrue(false);
      }

      // this will fail, as the PW is wrong
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user2, "");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }

      // this will fail as the PW is correct and LDAP2 is not active
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user2, "abc");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }
    },

    testRole2: function () {
      // only first LDAP will answer for user1
      const user1 = "The Donald";
      const user2 = user1 + ",dc=arangodb";

      // this will fail, as the PW is wrong
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user1, "");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }

      // this will succeed as the PW is correct and LDAP1 is active and responsible
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user1, "abc");
	assertTrue(true);
      } catch (e) {
	assertTrue(false);
      }

      // this will fail, as the PW is wrong
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user2, "");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }

      // this will fail as the PW is correct and LDAP2 is not active
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user2, "abc");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }
    },

    testRole3: function () {
      // only second LDAP will answer for user2
      const user1 = "Angela Merkel";
      const user2 = user1 + ",dc=arangodb";

      // this will fail, as the PW is wrong
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user1, "");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }

      // this will fail as the PW is correct and LDAP1 is active and not responsible
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user1, "abc");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }

      // this will fail, as the PW is wrong
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user2, "");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }

      // this will fail as the PW is correct and LDAP2 is not active
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user2, "abc");
	assertTrue(false);
      } catch (e) {
        assertEqual(10, e.errorNum);
      }
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AuthSuite);

return jsunity.done();
