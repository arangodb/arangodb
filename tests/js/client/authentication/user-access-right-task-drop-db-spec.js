/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, require, beforeEach */

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
// / @author Michael Hackstein
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const helper = require('@arangodb/testutils/user-helper');
const tasks = require('@arangodb/tasks');
const pu = require('@arangodb/process-utils');
const download = require('internal').download;
const namePrefix = helper.namePrefix;
const rightLevels = helper.rightLevels;
const testDBName = `${namePrefix}DBNew`;
const errors = require('@arangodb').errors;
const keySpaceId = 'task_drop_db_keyspace';

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;

const arango = require('internal').arango;
const db = require('internal').db;
for (let l of rightLevels) {
  systemLevel[l] = new Set();
  dbLevel[l] = new Set();
  colLevel[l] = new Set();
}

let { waitSystem, dropKeySpaceSystem, setKeySystem } = require('@arangodb/testutils/client-utils.js');

helper.removeAllUsers();
helper.generateAllUsers();

describe('User Rights Management', () => {
  it('should test rights for', () => {
    expect(userSet.size).to.be.greaterThan(0); 
    for (let name of userSet) {
      let canUse = false;
      try {
        helper.switchUser(name);
        canUse = true;
      } catch (e) {
        canUse = false;
      }

      if (canUse) {
        describe(`user ${name}`, () => {
          before(() => {
            helper.switchUser(name);
          });

          after(() => {
            dropKeySpaceSystem(keySpaceId);
          });

          describe('administrate on server level', () => {
            const rootTestDB = (switchBack = true) => {
              helper.switchUser('root');
              const allDB = db._databases();
              for (let i of allDB) {
                if (i === testDBName) {
                  if (switchBack) {
                    helper.switchUser(name);
                  }
                  return true;
                }
              }
              if (switchBack) {
                helper.switchUser(name);
              }
              return false;
            };

            const rootDropDB = () => {
              if (rootTestDB(false)) {
                db._dropDatabase(testDBName);
              }
              helper.switchUser(name);
            };

            const rootCreateDB = () => {
              if (!rootTestDB(false)) {
                db._createDatabase(testDBName);
              }
              helper.switchUser(name);
            };

            beforeEach(() => {
              db._useDatabase('_system');
              rootCreateDB();
            });

            after(() => {
              rootDropDB();
            });

            it('drop database', () => {
              setKeySystem(keySpaceId, name);
              const taskId = 'task_db_create' + name;
              const task = {
                id: taskId,
                name: taskId,
                command: `(function (params) {
                  try {
                    const db = require('@arangodb').db;
                    db._dropDatabase('${testDBName}');
                  } finally {
                    global.GLOBAL_CACHE_SET('${keySpaceId}-${name}', true);
                  }
                })(params);`
              };
              if (systemLevel['rw'].has(name)) {
                tasks.register(task);
                waitSystem(keySpaceId, name);
                if (systemLevel['rw'].has(name)) {
                  expect(rootTestDB()).to.equal(false, 'DB drop reported success, but DB was still found afterwards.');
                } else {
                  expect(rootTestDB()).to.equal(true, `${name} was able to drop a database with insufficient rights`);
                }
              } else {
                try {
                  tasks.register(task);
                  expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                } catch (e) {
                  expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                }
              }
            });
          });
        });
      }
    }
  });
});
