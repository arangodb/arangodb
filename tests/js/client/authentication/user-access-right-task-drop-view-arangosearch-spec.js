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
const testCol1Name = `${namePrefix}Col1New`;
const testCol2Name = `${namePrefix}Col2New`;
const indexName = `${namePrefix}Inverted`;
const keySpaceId = 'task_drop_view_keyspace';

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
    for (let testViewType of ["arangosearch", "search-alias"]) {
      describe(`view type ${testViewType}`, () => {
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

                const rootCreateCollection = (colName) => {
                  if (!rootTestCollection(colName, false)) {
                    let c = db._create(colName);
                    if (colName === testCol1Name) {
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

                const rootDropCollection = (colName) => {
                  helper.switchUser('root', dbName);
                  try {
                    db._collection(colName).drop();
                  } catch (ignored) { }
                  helper.switchUser(name, dbName);
                };

                const rootGrantCollection = (colName, user, explicitRight = '') => {
                  if (rootTestCollection(colName, false)) {
                    if (explicitRight !== '' && rightLevels.includes(explicitRight)) {
                      users.grantCollection(user, dbName, colName, explicitRight);
                    }
                  }
                  helper.switchUser(user, dbName);
                };

                const rootTestView = (viewName = testViewName, switchBack = true) => {
                  helper.switchUser('root', dbName);
                  delete db[viewName];
                  let view = db._view(viewName);
                  if (switchBack) {
                    helper.switchUser(name, dbName);
                  }
                  return view !== null;
                };

                const rootCreateView = (viewName, viewType, properties = null) => {
                  if (rootTestView(viewName, false)) {
                    db._dropView(viewName);
                  }
                  let view =  db._createView(viewName, viewType, {});
                  if (properties !== null) {
                    view.properties(properties, false);
                  }
                  helper.switchUser(name, dbName);
                };

                const rootDropView = (viewName = testViewName) => {
                  helper.switchUser('root', dbName);
                  try {
                    db._dropView(viewName);
                  } catch (ignored) { }
                  helper.switchUser(name, dbName);
                };

                const checkError = (e) => {
                  expect(e.code).to.equal(403, "Expected to get forbidden REST error code, but got another one");
                  expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code, "Expected to get forbidden error number, but got another one");
                };

                describe('drop a', () => {
                  before(() => {
                    db._useDatabase(dbName);
                  });

                  after(() => {
                    rootDropView(testViewName);
                    rootDropCollection(testCol1Name);
                    rootDropCollection(testCol2Name);
                  });

                  const key = `${testViewType}_${name}`;

                  it('view without links', () => {
                    rootCreateView(testViewName, testViewType);
                    expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, the view doesn not exist');
                    setKey(keySpaceId, key);
                    const taskId = 'task_drop_view_without_links_' + key;
                    const task = {
                      id: taskId,
                      name: taskId,
                      command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._dropView('${testViewName}');
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
                      expect(getKey(keySpaceId, `${key}_status`)).to.equal(true, `${name} could not drop the view with sufficient rights`);
                      expect(rootTestView(testViewName)).to.equal(false, 'View deletion reported success, but view was found afterwards');
                    } else {
                      try {
                        tasks.register(task);
                        wait(keySpaceId, key);
                      } catch (e) {
                        checkError(e);
                        return;
                      } finally {
                        expect(rootTestView(testViewName)).to.equal(true, `${name} was able to drop a view with insufficent rights`);
                      }
                      expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                    }
                  });

                  if (testViewType === 'arangosearch') {
                    it('view with link to non-existing collection', () => {
                      rootDropView(testViewName);
                      rootCreateCollection("NonExistentCol");
                      rootCreateView(testViewName, testViewType, { links: { "NonExistentCol" : { includeAllFields: true } } });
                      rootDropCollection("NonExistentCol");
                      expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, the view doesn not exist');
                      setKey(keySpaceId, key);
                      const taskId = 'task_drop_view_with_link_to_nonexist_col' + key;
                      const task = {
                        id: taskId,
                        name: taskId,
                        command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._dropView('${testViewName}');
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
                        expect(rootTestView(testViewName)).to.equal(false, 'View deletion reported success, but view was found afterwards');
                      } else {
                        try {
                          tasks.register(task);
                          wait(keySpaceId, key);
                        } catch (e) {
                          checkError(e);
                          return;
                        } finally {
                          expect(rootTestView(testViewName)).to.equal(true, `${name} was able to drop a view with insufficent rights`);
                        }
                        expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                      }
                    });

                    it('view with link to existing collection', () => {
                      rootDropView(testViewName);
                      rootDropCollection(testCol1Name);

                      rootCreateCollection(testCol1Name);
                      rootCreateView(testViewName, testViewType, { links: { [testCol1Name]: { includeAllFields: true } } });
                      expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, the view doesn not exist');
                      setKey(keySpaceId, key);
                      const taskId = 'task_drop_view_with_link_to_existing_col' + key;
                      const task = {
                        id: taskId,
                        name: taskId,
                        command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._dropView('${testViewName}');
                      global.KEY_SET('${keySpaceId}', '${key}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${key}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${key}', true);
                    }
                  })(params);`
                      };
                      if (dbLevel['rw'].has(name)) {
                        if(colLevel['rw'].has(name) || colLevel['ro'].has(name)){
                          tasks.register(task);
                          wait(keySpaceId, key);
                          expect(rootTestView(testViewName)).to.equal(false, 'View deletion reported success, but view was found afterwards');
                        } else {
                          tasks.register(task);
                          wait(keySpaceId, key);
                          expect(getKey(keySpaceId, `${key}_status`)).to.equal(false, `${name} managed to drop the view with insufficient rights`);
                          expect(rootTestView(testViewName)).to.equal(true, 'View deletion reported success, but view was found afterwards');
                        }
                      } else {
                        try {
                          tasks.register(task);
                          wait(keySpaceId, key);
                        } catch (e) {
                          checkError(e);
                          return;
                        } finally {
                          expect(rootTestView(testViewName)).to.equal(true, `${name} was able to drop a view with insufficent rights`);
                        }
                        expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                      }
                    });

                    let itName = 'view with links to multiple collections (switching 1 of them as RW during RO/NONE of a user collection level)';
                    !(colLevel['ro'].has(name) || colLevel['none'].has(name)) ? it.skip(itName) :
                      it(itName, () => {
                        rootDropView(testViewName);
                        rootDropCollection(testCol1Name);

                        rootCreateCollection(testCol1Name);
                        rootCreateCollection(testCol2Name);
                        rootCreateView(testViewName, testViewType, { links: { [testCol1Name]: { includeAllFields: true }, [testCol2Name]: { includeAllFields: true } } });
                        expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, the view doesn not exist');
                        rootGrantCollection(testCol2Name, name, 'rw');

                        setKey(keySpaceId, key);
                        const taskId = 'task_drop_view_with_link_to_existing_diff_col' + key;
                        const task = {
                          id: taskId,
                          name: taskId,
                          command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._dropView('${testViewName}');
                      global.KEY_SET('${keySpaceId}', '${key}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${key}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${key}', true);
                    }
                  })(params);`
                        };
                        if (dbLevel['rw'].has(name)) {
                          if (colLevel['ro'].has(name)) {
                            tasks.register(task);
                            wait(keySpaceId, key);
                            expect(rootTestView(testViewName)).to.equal(false, 'View deletion reported success, but view was found afterwards');
                          } else {
                            tasks.register(task);
                            wait(keySpaceId, key);
                            expect(getKey(keySpaceId, `${key}_status`)).to.equal(false, `${name} could drop the view with insufficient rights`);
                            expect(rootTestView(testViewName)).to.equal(true, `${name} was able to drop a view with insufficent rights`);
                          }
                        } else {
                          try {
                            tasks.register(task);
                            wait(keySpaceId, key);
                          } catch (e) {
                            checkError(e);
                            return;
                          } finally {
                            expect(getKey(keySpaceId, `${key}_status`)).to.equal(false, `${name} could drop the view with insufficient rights`);
                          }
                          expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                        }
                      });

                  }
                });
              });
            });
          }
        }

      });
    }
  });
});
after(() => {
  arango.connectHandle(connectionHandle);
  db._drop('UnitTestCollection');
  db._useDatabase('_system');
});
