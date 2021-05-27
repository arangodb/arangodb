/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue, assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication (dual ldap)
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

const arango = require("@arangodb").arango;
const db = require("internal").db;
const users = require("@arangodb/users");

// access forbidden to server/database
const FORBIDDEN = exports.FORBIDDEN = 'FORBIDDEN';

// access ok
const OK = exports.OK = 'OK';

// generate a single test
const buildTestFunction = function(value) {
  const user = value[0];
  const pw = value[1];
  const dbname = value[2];
  const result = value[3];

  if (result === FORBIDDEN) {
    return function() {
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user, pw);
        fail();
      } catch (e) {
        assertEqual(10, e.errorNum);
      }
    };
  } else if (result === OK) {
    return function() {
      try {
        arango.reconnect(arango.getEndpoint(), dbname, user, pw);
      } catch (e) {
        fail();
      }
    };
  } else {
    return function() {
      fail();
    };
  }
};

// generate a test suite
exports.createSuite = function(definition) {
  'use strict';

  // hardcoded in testsuite
  const jwtSecret = 'haxxmann';
  const dbname = 'test';
  const role1 = "role1";
  const role2 = "adminrole";
  const role3 = "bundeskanzlerin";
  const roles = [role1, role2, role3];

  // standard functions first
  const suite = {
    setUpAll: function() {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");

      for (const role of roles) {
        try {
          users.remove(":role:" + role);
        } catch (err) {}
      }

      try {
        db._dropDatabase(dbname);
      } catch (err) {}
    },

    tearDownAll: function() {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");

      for (const role of roles) {
        try {
          users.remove(":role:" + role);
        } catch (err) {}
      }

      try {
        db._dropDatabase(dbname);
      } catch (err) {}
    },

    testCreateRoles: function() {
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

    testDatabases: function() {
      let r = db._createDatabase(dbname);
      assertTrue(r);

      users.grantDatabase(":role:" + role1, dbname, "rw");
      users.grantDatabase(":role:" + role2, dbname, "rw");
      users.grantDatabase(":role:" + role3, dbname, "rw");
    }
  };


  Object.entries(definition).forEach(([key, value]) => {
    suite["test" + key] = buildTestFunction(value);
  });

  return function GenericLdap() {
    return suite;
  };
};
