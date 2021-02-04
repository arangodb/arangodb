/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, require*/

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for user access rights
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
const testViewType = "arangosearch";
const testColName = `${namePrefix}ColNew`;
const testColNameAnother = `${namePrefix}ColAnotherNew`;
const keySpaceId = 'task_create_view_keyspace';

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;

const arango = require('internal').arango;
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
  return download(arango.getEndpoint().replace('tcp', 'http') + `/_db/${dbName}/_admin/execute?returnAsJSON=true`,
    code,
    httpOptions);
};

helper.switchUser('root', '_system');
helper.removeAllUsers();
helper.generateAllUsers();

function hasIResearch (db) {
  return !(db._views() === 0); // arangosearch views are not supported
}

!hasIResearch(db) ? describe.skip : describe('User Rights Management', () => {
  it('should check if all users are created', () => {
    helper.switchUser('root', '_system');
    if (db._views() === 0) {
      return; // arangosearch views are not supported
    }
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
                db._create(colName);
                if (colLevel['none'].has(name)) {
                    if (helper.isLdapEnabledExternal()) {
                      users.grantCollection(':role:' + name, dbName, colName, 'none');
                    } else {
                      users.grantCollection(name, dbName, colName, 'none');
                    }
                } else if (colLevel['ro'].has(name)) {
                  if (helper.isLdapEnabledExternal()) {
                    users.grantCollection(':role:' + name, dbName, colName, 'ro');
                  } else {
                    users.grantCollection(name, dbName, colName, 'ro');
                  }
                } else if (colLevel['rw'].has(name)) {
                  if (helper.isLdapEnabledExternal()) {
                    users.grantCollection(':role:' + name, dbName, colName, 'rw');
                  } else {
                    users.grantCollection(name, dbName, colName, 'rw');
                  }
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
              return view != null;
            };

            const rootTestViewHasLinks = (viewName = testViewName, links) => {
              helper.switchUser('root', dbName);
              var view = db._view(viewName);
              if (view != null) {
                links.every(function(link) {
                  const links = view.properties().links;
                  if (links != null && links.hasOwnProperty([link])){
                    return true;
                  } else {
                    view = null;
                    return false;
                  }
                });
              }
              helper.switchUser(name, dbName);
              return view != null;
            };

            const rootTestViewLinksEmpty = (viewName = testViewName) => {
              helper.switchUser('root', dbName);
              var view = db._view(viewName);
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
                if (explicitRight !== '' && rightLevels.includes(explicitRight))
                {
                  if (helper.isLdapEnabledExternal()) {
                    users.grantCollection(':role:' + user, dbName, colName, explicitRight);
                  } else {
                    users.grantCollection(user, dbName, colName, explicitRight);
                  }
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

              it('view with empty (default) parameters', () => {
                expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view still exists');

                setKey(keySpaceId, name);
                const taskId = 'task_create_view_default_params_' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._createView('${testViewName}', '${testViewType}', {});
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
                };

                if (dbLevel['rw'].has(name)) {
                  tasks.register(task);
                  wait(keySpaceId, name);
                  expect(getKey(keySpaceId, `${name}_status`)).to.equal(true, `${name} could not create the view with sufficient rights`);
                  expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                } else {
                  try {
                    tasks.register(task);
                    wait(keySpaceId, name);
                  } catch (e) {
                    checkError(e);
                    return;
                  } finally {
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                  expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                }
              });

              it('view with non-empty parameters (except links)', () => {
                rootDropView(testViewName);
                rootDropCollection(testColName);

                expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view still exists');

                setKey(keySpaceId, name);
                const taskId = 'task_create_view_non_default_params_except_links_' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      db._createView('${testViewName}', '${testViewType}', { cleanupIntervalStep: 20 });
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
                };

                if (dbLevel['rw'].has(name)) {
                  tasks.register(task);
                  wait(keySpaceId, name);
                  expect(getKey(keySpaceId, `${name}_status`)).to.equal(true, `${name} could not create the view with sufficient rights`);
                  expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                  expect(rootGetViewProps(testViewName, true)["cleanupIntervalStep"]).to.equal(20, 'View creation reported success, but view property was not set as expected during creation');
                } else {
                  try {
                    tasks.register(task);
                    wait(keySpaceId, name);
                  } catch (e) {
                    checkError(e);
                    return;
                  } finally {
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                  expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                }
              });

              // FIXME: uncomment after PR 6199 is done with respectful changes
              /*
              it('view with links to existing collection', () => {
                rootDropView(testViewName);
                rootDropCollection(testColName);

                rootCreateCollection(testColName);
                expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view still exists');
                expect(rootTestCollection(testColName)).to.equal(true, 'Precondition failed, the collection still not exists')

                setKey(keySpaceId, name);
                const taskId = 'task_create_view_with_links_to_existing_collection_' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      var view = db._createView('${testViewName}', '${testViewType}', { links: { '${testColName}': { includeAllFields: true } } });
                    global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
                };

                if (dbLevel['rw'].has(name)) {
                  if (colLevel['rw'].has(name) || colLevel['ro'].has(name)) {
                    tasks.register(task);
                    wait(keySpaceId, name);
                    expect(getKey(keySpaceId, `${name}_status`)).to.equal(true, `${name} could not create the view with sufficient rights`);
                    expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                    expect(rootTestViewHasLinks(testViewName, [`${testColName}`])).to.equal(true, 'View links expected to be visible, but were not found afterwards');
                  } else {
                    tasks.register(task);
                    wait(keySpaceId, name);
                    expect(getKey(keySpaceId, `${name}_status`)).to.equal(false, `${name} could create the view with insufficient rights`);
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                } else {
                  try {
                    tasks.register(task);
                    wait(keySpaceId, name);
                  } catch (e) {
                    checkError(e);
                    return;
                  } finally {
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                  expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                }
              });

              it('view with links to multiple collections with same access level', () => {
                rootDropView(testViewName);
                rootDropCollection(testColName);

                rootCreateCollection(testColName);
                rootCreateCollection(testColNameAnother);

                expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view still exists');
                expect(rootTestCollection(testColName)).to.equal(true, 'Precondition failed, the collection still not exists');
                expect(rootTestCollection(testColNameAnother)).to.equal(true, 'Precondition failed, the collection still not exists');

                setKey(keySpaceId, name);
                const taskId = 'task_create_view_with_links_to_existing_collections_with_same_access_level_' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      var view = db._createView('${testViewName}', '${testViewType}', { links: { 
                        '${testColName}': { includeAllFields: true }, '${testColNameAnother}': { includeAllFields: true } 
                        }
                      });
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
                };

                if (dbLevel['rw'].has(name)) {
                  if (colLevel['rw'].has(name) || colLevel['ro'].has(name)) {
                    tasks.register(task);
                    wait(keySpaceId, name);
                    expect(getKey(keySpaceId, `${name}_status`)).to.equal(true, `${name} could not create the view with sufficient rights`);
                    expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                    expect(rootTestViewHasLinks(testViewName, [`${testColName}`, `${testColNameAnother}`])).to.equal(true, 'View links expected to be visible, but were not found afterwards');
                  } else {
                    tasks.register(task);
                    wait(keySpaceId, name);
                    expect(getKey(keySpaceId, `${name}_status`)).to.equal(false, `${name} could create the view with insufficient rights`);
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                } else {
                  try {
                    tasks.register(task);
                    wait(keySpaceId, name);
                  } catch (e) {
                    checkError(e);
                    return;
                  } finally {
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                  expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                }
              });

              var itName = 'view with links to multiple collections with RO access level to one of them';
              !(colLevel['rw'].has(name) || colLevel['none'].has(name)) ? it.skip(itName) :
              it(itName, () => {
                rootDropView(testViewName);
                rootDropCollection(testColName);
                rootDropCollection(testColNameAnother);

                rootCreateCollection(testColName);
                rootCreateCollection(testColNameAnother);
                rootGrantCollection(testColNameAnother, name, "ro");

                expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view still exists');
                expect(rootTestCollection(testColName)).to.equal(true, 'Precondition failed, the collection still not exists');
                expect(rootTestCollection(testColNameAnother)).to.equal(true, 'Precondition failed, the collection still not exists');

                setKey(keySpaceId, name);
                const taskId = 'task_create_view_with_links_to_existing_collections_with_RO_access_level_to_one_of_them_' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      var view = db._createView('${testViewName}', '${testViewType}', { links: { 
                        '${testColName}': { includeAllFields: true }, '${testColNameAnother}': { includeAllFields: true } 
                        }
                      });
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
                };

                if (dbLevel['rw'].has(name)) {
                  if (colLevel['rw'].has(name)) {
                    tasks.register(task);
                    wait(keySpaceId, name);
                    expect(getKey(keySpaceId, `${name}_status`)).to.equal(true, `${name} could not create the view with sufficient rights`);
                    expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                    expect(rootTestViewHasLinks(testViewName, [`${testColName}`, `${testColNameAnother}`])).to.equal(true, 'View links expected to be visible, but were not found afterwards');
                  } else {
                    tasks.register(task);
                    wait(keySpaceId, name);
                    expect(getKey(keySpaceId, `${name}_status`)).to.equal(false, `${name} could create the view with insufficient rights`);
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                } else {
                  try {
                    tasks.register(task);
                    wait(keySpaceId, name);
                  } catch (e) {
                    checkError(e);
                    return;
                  } finally {
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                  expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                }
              });

              var itName = 'view with links to multiple collections with NONE access level to one of them';
              !(colLevel['rw'].has(name) || colLevel['ro'].has(name)) ? it.skip(itName) :
              it(itName, () => {
                rootDropView(testViewName);
                rootDropCollection(testColName);
                rootDropCollection(testColNameAnother);

                rootCreateCollection(testColName);
                rootCreateCollection(testColNameAnother);
                rootGrantCollection(testColNameAnother, name, "none");
                
                expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view still exists');
                expect(rootTestCollection(testColName)).to.equal(true, 'Precondition failed, the collection still not exists');
                expect(rootTestCollection(testColNameAnother)).to.equal(true, 'Precondition failed, the collection still not exists');

                setKey(keySpaceId, name);
                const taskId = 'task_create_view_with_links_to_existing_collections_with_NONE_access_level_to_one_of_them_' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      var view = db._createView('${testViewName}', '${testViewType}', { links: { 
                        '${testColName}': { includeAllFields: true }, '${testColNameAnother}': { includeAllFields: true } 
                        }
                      });
                      global.KEY_SET('${keySpaceId}', '${name}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${name}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
                };

                if (dbLevel['rw'].has(name)) {
                  tasks.register(task);
                  wait(keySpaceId, name);
                  expect(getKey(keySpaceId, `${name}_status`)).to.equal(false, `${name} could create the view with insufficient rights`);
                  expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                } else {
                  try {
                    tasks.register(task);
                    wait(keySpaceId, name);
                  } catch (e) {
                    checkError(e);
                    return;
                  } finally {
                    expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                  }
                  expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                }
              });
              */
            });
          });
        });
      }
    }
  });
});
