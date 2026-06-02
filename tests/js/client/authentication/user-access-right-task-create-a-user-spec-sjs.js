/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
// / @author Michael Hackstein
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse, assertNotEqual, assertNotUndefined} = jsunity.jsUnity.assertions;
const users = require('@arangodb/users');
const helper = require('@arangodb/testutils/user-helper');
const tasks = require('@arangodb/tasks');
const namePrefix = helper.namePrefix;
const rightLevels = helper.rightLevels;
const errors = require('@arangodb').errors;
const keySpaceId = 'task_create_user_keyspace';

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;
const testUser = `${namePrefix}TestUser`;

const arango = require('internal').arango;
let connectionHandle = arango.getConnectionHandle();
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
  return executeJS(`return global.KEYSPACE_CREATE('${keySpaceId}', 128, true);`).parsedBody === true;
};

const dropKeySpace = (keySpaceId) => {
  executeJS(`global.KEYSPACE_DROP('${keySpaceId}');`);
};

const getKey = (keySpaceId, key) => {
  return executeJS(`return global.KEY_GET('${keySpaceId}', '${key}');`).parsedBody === true;
};

const setKey = (keySpaceId, name) => {
  return executeJS(`global.KEY_SET('${keySpaceId}', '${name}', false);`);
};

const executeJS = (code) => {
  return arango.POST_RAW('/_admin/execute', code);
};

helper.removeAllUsers();
helper.generateAllUsers();

describe('User Rights Management', () => {
  it('should test rights for', () => {
    assertTrue(userSet.size > 0); 
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
            // What are defaults if unchanged?
            helper.switchUser(name);
            assertTrue(createKeySpace(keySpaceId), 'keySpace creation failed!');
          });

          after(() => {
            dropKeySpace(keySpaceId);
          });

          describe('administrate on server level', () => {
            const rootTestUser = (switchBack = true) => {
              helper.switchUser('root');
              try {
                const u = users.document(testUser);
                if (switchBack) {
                  helper.switchUser(name);
                }
                return u !== undefined;
              } catch (e) {
                if (switchBack) {
                  helper.switchUser(name);
                }
                return false;
              }
            };

            const rootDropUser = () => {
              if (rootTestUser(false)) {
                users.remove(testUser);
              }
              helper.switchUser(name);
            };

            before(() => {
              db._useDatabase('_system');
              rootDropUser();
            });

            after(() => {
              rootDropUser();
            });

            it('create a user', () => {
              setKey(keySpaceId, name);
              const taskId = 'task_create_user_' + name;
              const task = {
                id: taskId,
                name: taskId,
                command: `(function (params) {
                  try {
                    require('@arangodb/users').save('${testUser}', '', true);
                  } finally {
                    global.KEY_SET('${keySpaceId}', '${name}', true);
                  }
                })(params);`
              };
              if (systemLevel['rw'].has(name)) {
                tasks.register(task);
                wait(keySpaceId, name);
                if (systemLevel['rw'].has(name)) {
                  assertTrue(rootTestUser(), 'User creation reported success, but User was not found afterwards.');
                } else {
                  assertFalse(rootTestUser(), `${name} was able to create a user with insufficent rights`);
                }
              } else {
                try {
                  tasks.register(task);
                  assertFalse(true, `${name} managed to register a task with insufficient rights`);
                } catch (e) {
                  assertEqual(e.errorNum, errors.ERROR_FORBIDDEN.code);
                }
              }
            });
          });
        });
      }
    }
  });
});

after(() => {
  arango.connectHandle(connectionHandle);
  db._useDatabase('_system');
});
