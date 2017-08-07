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

const internal = require('internal');

const users = require('@arangodb/users');
const namePrefix = 'UnitTest';
const dbName = `${namePrefix}DB`;
const colName = `${namePrefix}Collection`;
const rightLevels = ['rw', 'ro', 'none'];

const userSet = new Set();
const systemLevel = {};
const dbLevel = {};
const colLevel = {};

const db = internal.db;

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
  for (let sys of rightLevels) {
    for (let db of rightLevels) {
      for (let col of rightLevels) {
        let name = `${namePrefix}_${sys}_${db}_${col}`;
        try {
          users.remove(name);
        } catch (e) {
          // If the user does not exist
        }
      }
    }
  }
  try {
    db._dropDatabase(dbName);
  } catch (e) {
    // Nevermind, db does not exist
  }
};

exports.generateAllUsers = () => {
  let dbs = db._databases();
  let create = true;
  for (let d of dbs) {
    if (d === dbName) {
      // We got it, do not create
      create = false;
      break;
    }
  }
  if (create) {
    db._createDatabase(dbName);
    db._useDatabase(dbName);
    if (db._collection(colName) === null) {
      db._create(colName);
    }
    db._useDatabase('_system');
  }
  for (let sys of rightLevels) {
    for (let db of rightLevels) {
      for (let col of rightLevels) {
        let name = `${namePrefix}_${sys}_${db}_${col}`;
        users.save(name, '', true);
        userSet.add(name);

        users.grantDatabase(name, '_system', sys);
        let sysPerm = users.permission(name, '_system');
        if (sys !== sysPerm) {
          internal.print('Wrong sys permissions for user ' + name);
          internal.print(sys + ' !== ' + sysPerm);
        }
        systemLevel[sys].add(name);

        users.grantDatabase(name, dbName, db);
        let dbPerm = users.permission(name, dbName);
        if (db !== dbPerm) {
          internal.print('Wrong db permissions for user ' + name);
          internal.print(db + ' !== ' + dbPerm);
        }
        dbLevel[db].add(name);

        users.grantCollection(name, dbName, colName, col);
        let colPerm = users.permission(name, dbName, colName);
        if (col !== colPerm) {
          internal.print('Wrong collection permissions for user ' + name);
          internal.print(col + ' !== ' + colPerm);
        }
        colLevel[col].add(name);
      }
    }
  }
};

exports.systemLevel = systemLevel;
exports.dbLevel = dbLevel;
exports.colLevel = colLevel;
exports.userSet = userSet;
exports.namePrefix = namePrefix;
exports.dbName = dbName;
exports.colName = colName;
exports.rightLevels = rightLevels;
exports.userCount = 3 * 3 * 3;
