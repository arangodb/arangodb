/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for user access rights
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

var jsunity = require("jsunity");
const users = require('@arangodb/users');
const db = require('@arangodb').db;

const colName = 'PermissionsTestCollection';
const rightLevels = ['rw', 'ro', 'none'];
const dbs = ['*', '_system'];
const cols = ['*', colName];
const userSet = new Set();
const internal = require('internal');

let conv = function(x) {
  if (x === 'rw') return 2;
  if (x === 'ro') return 1;
  if (x === 'none') return 0;
  return -1;
};

const createUsers = () => {
  db._useDatabase('_system');
  for (const db of dbs) {
    for (const dbLevel of rightLevels) {
      for (const col of cols) {
        for (const colLevel of rightLevels) {
          if (db === '*' && col !== '*') {
            continue;
          }
          const name = `user_${db}_${dbLevel}_${col}_${colLevel}`;
          userSet.add({
            name,
            db: {
              name: db,
              permission: dbLevel
            },
            col: {
              name: col,
              permission: (conv(dbLevel) > conv(colLevel) ? dbLevel : colLevel)
              //permission: colLevel
            }
          });
        }
      }
    }
  }
};

const createUser = (user) => {
  users.save(user.name, '', true);
  users.grantDatabase(user.name, user.db.name, user.db.permission);
  users.grantCollection(user.name, user.db.name, user.col.name, user.col.permission);
};

const removeUser = (user) => {
  users.remove(user.name);
};


function PermissionResolutionSuite () {
  'use strict';
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(colName);
      db._useDatabase('_system');
      db._create(colName);
      internal.wait(1);
      createUsers();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(colName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user
////////////////////////////////////////////////////////////////////////////////

    testAcessLevelFallbacks : function () {
      for (const user of userSet) {
        createUser(user);

        let permission = users.permission(user.name, '_system');
        assertEqual(permission, user.db.permission, 'Expected different permission for _system');

        permission = users.permission(user.name, '_system', colName);
        assertEqual(permission, user.col.permission);


        users.revokeCollection(user.name, '_system', colName);
        users.reload();

        permission = users.permission(user.name, "_system", colName);
        let fullPermission = users.permissionFull(user.name);

        let actual = fullPermission["_system"].collections[colName];
        assertEqual(actual, "undefined");
        
        if (user.db.name !== "*") {
          // This should mirror our current fallbacks
          if (user.col.name === "*") {
            assertEqual(permission, user.col.permission);
          } else {
            assertEqual(permission, user.db.permission);
          }
        } else {
          assertEqual(permission, user.col.permission);
        }

        users.revokeDatabase(user.name, user.db.name);
        users.reload();

        let result = users.permissionFull(user.name);
        const dbPerm = result['*'].permission;
        permission = users.permission(user.name, user.db.name);

        assertEqual(permission, dbPerm, 'Expected different permission for _system');
        removeUser(user);
      }
  },

  testUndefinedAuthLevels: function() {
    
      users.save("haxxman", '', true);
  
      ["rw", "none", "ro"].forEach((lvl) => {
        users.grantCollection("haxxman", "_system", colName, lvl);
        assertEqual(users.permission("haxxman", "_system", colName), lvl);
      });
      assertEqual(users.permission("haxxman", "_system"), "none");
  
      let result = users.permissionFull("haxxman");
      assertEqual(result['_system'].permission, "undefined");
  
      ["rw", "none", "ro"].forEach((lvl) => {
        users.grantDatabase("haxxman", "_system", lvl);
        assertEqual(users.permission("haxxman", "_system"), lvl);
        result = users.permissionFull("haxxman");
        assertEqual(result['_system'].permission, lvl);
      });
  
      users.revokeDatabase("haxxman", "_system");
      result = users.permissionFull("haxxman");
      assertEqual(result['_system'].permission, "undefined");
  
      users.revokeCollection("haxxman", "_system", colName);
      result = users.permissionFull("haxxman");
      assertEqual(result['_system'].collections[colName], "undefined");
    }
  };
};


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(PermissionResolutionSuite);
return jsunity.done();
