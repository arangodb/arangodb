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
// / @author Vadim Kondratyev
// / @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const users = require('@arangodb/users');
const helper = require('@arangodb/testutils/user-helper');
const tasks = require('@arangodb/tasks');
const pu = require('@arangodb/testutils/process-utils');
const download = require('internal').download;
const errors = require('@arangodb').errors;
const db = require('@arangodb').db;
const namePrefix = helper.namePrefix;
const dbName = helper.dbName;
const rightLevels = helper.rightLevels;
const testViewName = `${namePrefix}ViewNew`;
const testColName = `${namePrefix}ColNew`;
const testColNameAnother = `${namePrefix}ColAnotherNew`;
const indexName = `${namePrefix}Inverted`;
const keySpaceId = 'task_create_view_keyspace';

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;

const arango = require('internal').arango;
let connectionHandle = arango.getConnectionHandle();
for (let l of rightLevels) {
  systemLevel[l] = new Set();
  dbLevel[l] = new Set();
  colLevel[l] = new Set();
}

const wait = (keySpaceId, key) => {
  for (let i = 0; i < 200; i++) {
    if (getKey(keySpaceId, key)) {
      break;
    }
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
  }, {});
  httpOptions.method = 'POST';
  httpOptions.timeout = 1800;
  httpOptions.returnBodyOnError = true;
  return download(arango.getEndpoint().replace('tcp', 'http') + `/_db/${dbName}/_admin/execute?returnAsJSON=true`,
    code,
    httpOptions);
};

helper.switchUser('root', '_system');
helper.removeAllUsers();
helper.generateAllUsers();

const testViewType = "search-alias";

describe('User Rights Management', () => {
  it('should check if all users are created', () => {
    helper.switchUser('root', '_system');
    expect(userSet.size).to.be.greaterThan(0); 
    expect(userSet.size).to.equal(helper.userCount);
    for (let name of userSet) {
      expect(users.document(name), `Could not find user: ${name}`).to.not.be.undefined;
    }
  });

  it('should test rights for', () => {
    expect(userSet.size).to.be.greaterThan(0);
    for (let name of userSet) {
      let canUse = false;
      try {
        helper.switchUser(name, dbName);
        canUse = true;
      } catch (e) {
        canUse = false;
      }

      if (canUse) {
        describe(`user ${name}`, () => {
          before(() => {
            helper.switchUser(name, dbName);
            expect(createKeySpace(keySpaceId)).to.equal(true, 'keySpace creation failed!');
          });

          after(() => {
            dropKeySpace(keySpaceId);
          });

          describe('administrate on db level', () => {
            const rootTestCollection = (colName, switchBack = true) => {
              helper.switchUser('root', dbName);
              let col = db._collection(colName);
              if (switchBack) {
                helper.switchUser(name, dbName);
              }
              return col !== null;
            };

            const rootCreateCollection = (colName = testColName) => {
              if (!rootTestCollection(colName, false)) {
                let c = db._create(colName);
                if (colName === testColName) {
                  c.ensureIndex({ type: "inverted", name: indexName, fields: [ { name: "value" } ] });
                }
                if (colLevel['none'].has(name)) {
                  users.grantCollection(name, dbName, colName, 'none');
                } else if (colLevel['ro'].has(name)) {
                  users.grantCollection(name, dbName, colName, 'ro');
                } else if (colLevel['rw'].has(name)) {
                  users.grantCollection(name, dbName, colName, 'rw');
                }
              }
              helper.switchUser(name, dbName);
            };

            const rootDropCollection = (colName = testColName) => {
              if (rootTestCollection(colName, false)) {
                try {
                  db._collection(colName).drop();
                } catch (ignored) { }
              }
              helper.switchUser(name, dbName);
            };

            const rootTestView = (viewName = testViewName) => {
              helper.switchUser('root', dbName);
              const view = db._view(viewName);
              helper.switchUser(name, dbName);
              return view !== null;
            };

            const rootTestViewHasLinks = (viewName = testViewName, links) => {
              helper.switchUser('root', dbName);
              let view = db._view(viewName);
              if (view !== null) {
                links.every(function(link) {
                  const links = view.properties().links;
                  if (links !== null && links.hasOwnProperty([link])){
                    return true;
                  } else {
                    view = null;
                    return false;
                  }
                });
              }
              helper.switchUser(name, dbName);
              return view !== null;
            };

            const rootTestViewLinksEmpty = (viewName = testViewName) => {
              helper.switchUser('root', dbName);
              let view = db._view(viewName);
              return Object.keys(view.properties().links).length === 0;
            };

            const rootDropView = () => {
              helper.switchUser('root', dbName);
              try {
                db._dropView(testViewName);
              } catch (ignored) { }
              helper.switchUser(name, dbName);
            };

            const rootGetViewProps = (viewName, switchBack = true) => {
              helper.switchUser('root', dbName);
              let properties = db._view(viewName).properties();
              if (switchBack) {
                helper.switchUser(name, dbName);
              }
              return properties;
            };

            const rootGrantCollection = (colName, user, explicitRight = '') => {
              if (rootTestCollection(colName, false)) {
                if (explicitRight !== '' && rightLevels.includes(explicitRight)) {
                  users.grantCollection(user, dbName, colName, explicitRight);
                }
              }
              helper.switchUser(user, dbName);
            };

            const checkError = (e) => {
              expect(e.code).to.equal(403, "Expected to get forbidden REST error code, but got another one");
              expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code, "Expected to get forbidden error number, but got another one");
            };

            describe('create a', () => {
              before(() => {
                db._useDatabase(dbName);
              });

              after(() => {
                rootDropView(testViewName);
                rootDropCollection(testColName);
              });

              const key = `${name}`;

              it('view with empty (default) parameters', () => {
                expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view still exists');

                setKey(keySpaceId, name);
                const taskId = 'task_create_view_default_params_' + key;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._createView('${testViewName}', '${testViewType}', {});
                      global.KEY_SET('${keySpaceId}', '${key}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${key}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${key}', true);
                    }
                  })(params);`
                };

                if (dbLevel['rw'].has(name)) {
                  tasks.register(task);
                  wait(keySpaceId, key);
                  expect(getKey(keySpaceId, `${key}_status`)).to.equal(true, `${name} could not create the view with sufficient rights`);
                  expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                } else {
                  try {
                    tasks.register(task);
                    wait(keySpaceId, key);
                  } catch (e) {
                    checkError(e);
                    return;
                  } finally {
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                  expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                }
              });

            });
          });
        });
      }
    }
  });
});
after(() => {
  arango.connectHandle(connectionHandle);
  db._drop('UnitTestCollection');
  db._useDatabase('_system');
});
