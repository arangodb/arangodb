/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, require */

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
const helper = require('@arangodb/user-helper');
const tasks = require('@arangodb/tasks');
const pu = require('@arangodb/process-utils');
const download = require('internal').download;
const namePrefix = helper.namePrefix;
const rightLevels = helper.rightLevels;
const testDBName = `${namePrefix}DBNew`;
const errors = require('@arangodb').errors;
const keySpaceId = 'task_create_db_keyspace';

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

const wait = (keySpaceId, key) => {
  for (let i = 0; i < 200; i++) {
    if (getKey(keySpaceId, key)) break;
    require('internal').wait(0.1);
  }
};

const createKeySpace = (keySpaceId) => {
  return executeJS(`return global.KEYSPACE_CREATE('${keySpaceId}', 128, true);`).body === 'true';
};

const dropKeySpace = (keySpaceId) => {
  executeJS(`global.KEYSPACE_DROP('${keySpaceId}');`);
};

const getKey = (keySpaceId, key) => {
  return executeJS(`return global.KEY_GET('${keySpaceId}', '${key}');`).body === 'true';
};

const setKey = (keySpaceId, name) => {
  return executeJS(`global.KEY_SET('${keySpaceId}', '${name}', false);`);
};

const executeJS = (code) => {
  let httpOptions = pu.makeAuthorizationHeaders({
    username: 'root',
    password: ''
  });
  httpOptions.method = 'POST';
  httpOptions.timeout = 1800;
  httpOptions.returnBodyOnError = true;
  return download(arango.getEndpoint().replace('tcp', 'http') + '/_admin/execute?returnAsJSON=true',
    code,
    httpOptions);
};

const switchUser = (user) => {
  arango.reconnect(arango.getEndpoint(), '_system', user, '');
};

helper.removeAllUsers();

describe('User Rights Management', () => {
  before(helper.generateAllUsers);
  after(helper.removeAllUsers);

  it('should test rights for', () => {
    for (let name of userSet) {
      let canUse = false;
      try {
        switchUser(name);
        canUse = true;
      } catch (e) {
        canUse = false;
      }

      if (canUse) {
        describe(`user ${name}`, () => {
          before(() => {
            switchUser(name);
            expect(createKeySpace(keySpaceId)).to.equal(true, 'keySpace creation failed!');
          });

          after(() => {
            dropKeySpace(keySpaceId);
          });

          describe('administrate on server level', () => {
            const rootTestDB = (switchBack = true) => {
              switchUser('root');
              const allDB = db._databases();
              for (let i of allDB) {
                if (i === testDBName) {
                  if (switchBack) {
                    switchUser(name);
                  }
                  return true;
                }
              }
              if (switchBack) {
                switchUser(name);
              }
              return false;
            };

            const rootDropDB = () => {
              if (rootTestDB(false)) {
                db._dropDatabase(testDBName);
              }
              switchUser(name);
            };

            before(() => {
              db._useDatabase('_system');
              rootDropDB();
            });

            after(() => {
              rootDropDB();
            });

            it('create database', () => {
              setKey(keySpaceId, name);
              const taskId = 'task_db_create' + name;
              const task = {
                id: taskId,
                name: taskId,
                command: `(function (params) {
                  try {
                    const db = require('@arangodb').db;
                    db._createDatabase('${testDBName}');
                  } finally {
                    global.KEY_SET('${keySpaceId}', '${name}', true);
                  }
                })(params);`
              };
              if (systemLevel['rw'].has(name)) {
                tasks.register(task);
                wait(keySpaceId, name);
                if (systemLevel['rw'].has(name)) {
                  expect(rootTestDB()).to.equal(true, 'DB creation reported success, but DB was not found afterwards.');
                } else {
                  expect(rootTestDB()).to.equal(false, `${name} was able to create a database with insufficent rights`);
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

