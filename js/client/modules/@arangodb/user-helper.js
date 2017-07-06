/* jshint globalstrict:true, strict:true, maxlen: 5000 */

// //////////////////////////////////////////////////////////////////////////////
// / @brief Helper module to generate users with specific rights
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
// / @author Michael Hackstein
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const users = require('@arangodb/users');
const namePrefix = `UnitTest`;
const dbName = `${namePrefix}DB`;
const colName = `${namePrefix}Collection`;
const rightLevels = ['rw', 'ro', 'none', 'default'];

const userSet = new Set();
const systemLevel = {};
const dbLevel = {};
const colLevel = {};
const activeUsers = new Set();
const inactiveUsers = new Set();

for (let l of rightLevels) {
  systemLevel[l] = new Set();
  dbLevel[l] = new Set();
  colLevel[l] = new Set();
}


// The Naming Convention will be
// UnitTest_server-level_db-level_col-level
//
// Each of those levels will contain:
// w, r, n or d.
// w == WRITE
// r == READ
// n == NONE
// d == DEFAULT

exports.removeAllUsers = () => {
  for (let u of userSet) {
    try { 
      users.remove(name);
    } catch (e) {
      // If the user does not exist
    }
  }
  try {
    db._dropDatabase(dbName);
  } catch (e) {
    // Nevermind, db does not exist
  }
};

exports.generateAllUsers = () => {
  for (let sys of rightLevels) {
    for (let db of rightLevels) {
      for (let col of rightLevels) {
        for (let active of [true, false]) {
          let name = `${namePrefix}_${sys}_${db}_${col}_${active}`;
          users.save(name, '', active); 
          userSet.add(name);
          if (active) {
            activeUsers.add(name);
          } else {
            inactiveUsers.add(name);
          }

          if (sys !== 'default') {
            users.grantDatabase(name, '_system', sys);
          } else {
            users.revokeDatabase(name, '_system');
          }
          systemLevel[sys].add(name);

          if (db !== 'default') {
            users.grantDatabase(name, dbName, db);
          } else {
            users.revokeDatabase(name, dbName);
          }
          dbLevel[db].add(name);

          if (col !== 'default') {
            users.grantCollection(name, dbName, colName, col);
          } else {
            users.revokeCollection(name, dbName, colName);
          }
          colLevel[col].add(name); 
        }
      }
    }
  }
};

exports.systemLevel = systemLevel;
exports.dbLevel = dbLevel;
exports.colLevel = colLevel;
exports.userSet = userSet;
exports.activeUsers = activeUsers;
exports.inactiveUsers = inactiveUsers;
exports.namePrefix = namePrefix;
exports.dbName = dbName;
exports.colName = colName;
exports.rightLevels = rightLevels;
