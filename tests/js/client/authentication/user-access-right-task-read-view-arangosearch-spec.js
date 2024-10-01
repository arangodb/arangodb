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
const testView1Name = `${namePrefix}View1New`;
const testView2Name = `${namePrefix}View2New`;
const testViewType = "arangosearch";
const testCol1Name = `${namePrefix}Col1New`;
const testCol2Name = `${namePrefix}Col2New`;
const indexName = `${namePrefix}Inverted`;
const testNumDocs = 3;
const keySpaceId = 'task_read_view_keyspace';

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;

const arango = require('internal').arango;
let connectionHandle = arango.getConnectionHandle();
const tmp = require('internal');
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

const getNumericKey = (keySpaceId, key, defaultValue = 0) => {
  try {
    return parseInt(executeJS(`return global.KEY_GET('${keySpaceId}', '${key}');`).body);
  } catch(e) {
    return defaultValue;
  }
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
                before(() => {
                  db._useDatabase(dbName);
                  rootCreateCollection(testCol1Name);
                  rootCreateCollection(testCol2Name);
                  let properties = { properties : {} };
                  if (testViewType === "arangosearch") {
                    properties['links'] = {};
                    properties['links'][testCol1Name] = { includeAllFields: true };
                    rootCreateView(testView1Name, testViewType, properties);
                    properties['links'][testCol2Name] = { includeAllFields: true };
                    rootCreateView(testView2Name, testViewType, properties);
                  } else {
                    properties['indexes'] = [ { collection: testCol1Name, index: indexName } ];
                    rootCreateView(testView1Name, testViewType, properties);
                  }
                  rootPrepareCollection(testCol1Name, testNumDocs);
                  rootPrepareCollection(testCol2Name, testNumDocs, false);
                });

                after(() => {
                  rootDropView(testView1Name);
                  rootDropView(testView2Name);
                  rootDropCollection(testCol1Name);
                  rootDropCollection(testCol2Name);
                });

                const rootTestCollection = (colName, switchBack = true) => {
                  helper.switchUser('root', dbName);
                  let col = db._collection(colName);
                  if (switchBack) {
                    helper.switchUser(name, dbName);
                  }
                  return col !== null;
                };

                const rootCreateCollection = (colName, explicitRight = '') => {
                  if (!rootTestCollection(colName, false)) {
                    let c = db._create(colName);
                    if (colName === testCol1Name) {
                      c.ensureIndex({ type: "inverted", name: indexName, fields: [ { name: "value" } ] });
                    }
                    if (explicitRight !== '' && rightLevels.has(explicitRight)) {
                      users.grantCollection(name, dbName, colName, explicitRight);
                    } else {
                      if (colLevel['none'].has(name)) {
                        users.grantCollection(name, dbName, colName, 'none');
                      } else if (colLevel['ro'].has(name)) {
                        users.grantCollection(name, dbName, colName, 'ro');
                      } else if (colLevel['rw'].has(name)) {
                        users.grantCollection(name, dbName, colName, 'rw');
                      }
                    }
                  }
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

                const rootPrepareCollection = (colName, numDocs = 1, defKey = true) => {
                  if (rootTestCollection(colName, false)) {
                    db._collection(colName).truncate({ compact: false });
                    let docs = [];
                    for (let i = 0; i < numDocs; ++i) {
                      let doc = {prop1: colName + "_1", propI: i, plink: "lnk" + i};
                      if (!defKey) {
                        doc._key = colName + i;
                      }
                      docs.push(doc);
                    }
                    db._collection(colName).insert(docs);
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

                const rootTestView = (viewName, switchBack = true) => {
                  helper.switchUser('root', dbName);
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

                const rootDropView = (viewName) => {
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

                describe('read a view', () => {
                  before(() => {
                    db._useDatabase(dbName);
                  });
                    
                  const key = `${testViewType}_${name}`;

                  it('by AQL with link to single collection', () => {
                    expect(rootTestView(testView1Name)).to.equal(true, 'Precondition failed, the view doesn\'t exist');
                    setKey(keySpaceId, key);
                    const taskId = 'task_read_view_single_collection_' + key;
                    const task = {
                      id: taskId,
                      name: taskId,
                      command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      let query = \"FOR d IN  ${testView1Name} RETURN d\";
                      let result = db._query(query);
                      global.KEY_SET('${keySpaceId}', '${key}_length', result.toArray().length);
                      global.KEY_SET('${keySpaceId}', '${key}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${key}_status', false);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${key}', true);
                    }
                  })(params);`
                    };
                    if (dbLevel['rw'].has(name)) {
                      if (colLevel['rw'].has(name) || colLevel['ro'].has(name)) {
                        tasks.register(task);
                        wait(keySpaceId, key);
                        expect(getKey(keySpaceId, `${key}_status`)).to.equal(true, `${name} could not read the view with sufficient rights`);
                        //FIXME: issue #429 (https://github.com/arangodb/backlog/issues/429)
                        //expect(getNumericKey(keySpaceId, `${name}_length`)).to.equal(testNumDocs, 'View read failed');
                      } else {
                        tasks.register(task);
                        wait(keySpaceId, key);
                        expect(getKey(keySpaceId, `${key}_status`)).to.not.equal(true, `${name} managed to read the view with insufficient rights`);
                      }
                    } else {
                      try {
                        tasks.register(task);
                        wait(keySpaceId, key);
                      } catch (e) {
                        checkError(e);
                        return;
                      } finally {
                        expect(getKey(keySpaceId, `${key}_status`)).to.equal(false, `${name} could read the view with insufficient rights`);
                      }
                      expect(false).to.equal(true, `${key} managed to register a task with insufficient rights`);
                    }
                  });

                  if (testViewType === 'arangosearch') {
                    it('by AQL with links to multiple collections with same access level', () => {
                      expect(rootTestView(testView2Name)).to.equal(true, 'Precondition failed, the view doesn\'t exist');
                      setKey(keySpaceId, key);
                      const taskId = 'task_read_view_multi_collections_same_access_' + key;
                      const task = {
                        id: taskId,
                        name: taskId,
                        command: `(function (params) {
                    try {
                      const db = require('@arangodb').db;
                      let query = \"FOR d IN  ${testView2Name} RETURN d\";
                      let result = db._query(query);
                      global.KEY_SET('${keySpaceId}', '${key}_length', result.toArray().length);
                      global.KEY_SET('${keySpaceId}', '${key}_status', true);
                    } catch (e) {
                      global.KEY_SET('${keySpaceId}', '${key}_status', false);
                    }finally {
                      global.KEY_SET('${keySpaceId}', '${key}', true);
                    }
                  })(params);`
                      };
                      if (dbLevel['rw'].has(name)) {
                        if (colLevel['rw'].has(name) || colLevel['ro'].has(name)) {
                          tasks.register(task);
                          wait(keySpaceId, key);
                          expect(getKey(keySpaceId, `${key}_status`)).to.equal(true, `${name} could not read the view with sufficient rights`);
                          //FIXME: issue #429 (https://github.com/arangodb/backlog/issues/429)
                          //expect(getNumericKey(keySpaceId, `${name}_length`)).to.equal(testNumDocs*2, 'View read failed');
                        } else {
                          tasks.register(task);
                          wait(keySpaceId, key);
                          expect(getKey(keySpaceId, `${key}_status`)).to.equal(false, `${name} managed to read the view with insufficient rights`);
                        }
                      } else {
                        try {
                          tasks.register(task);
                          wait(keySpaceId, key);
                        } catch (e) {
                          checkError(e);
                          return;
                        } finally {
                          expect(getKey(keySpaceId, `${key}_status`)).to.equal(false, `${name} managed to read the view with insufficient rights`);
                        }
                        expect(false).to.equal(true, `${key} managed to register a task with insufficient rights`);
                      }
                    });

                    let descName = 'view with links to multiple collections with NONE access level to one of them';
                    !(colLevel['rw'].has(name) || colLevel['ro'].has(name)) ? describe(descName, () => {}) :
                      describe(descName, () => {
                        before(() => {
                          rootGrantCollection(testCol2Name, name, 'none');
                          expect(rootTestView(testView2Name)).to.equal(true, 'Precondition failed, the view doesn\'t exist');
                        });

                        it('by AQL query (data)', () => {
                          setKey(keySpaceId, key);
                          const taskId = 'task_read_view_multi_collections_with_1_of_none_access_data_' + key;
                          const task = {
                            id: taskId,
                            name: taskId,
                            command: `(function (params) {
                      try {
                        const db = require('@arangodb').db;
                        let query = \"FOR d IN  ${testView2Name} RETURN d\";
                        let result = db._query(query);
                        global.KEY_SET('${keySpaceId}', '${key}_status', true);
                      } catch (e) {
                        global.KEY_SET('${keySpaceId}', '${key}_status', false);
                      }finally {
                        global.KEY_SET('${keySpaceId}', '${key}', true);
                      }
                    })(params);`
                          };
                          if (dbLevel['rw'].has(name)) {
                            tasks.register(task);
                            wait(keySpaceId, key);
                            expect(getKey(keySpaceId, `${key}_status`)).to.equal(false, `${name} managed to perform a query on view with insufficient rights`);
                          } else {
                            try {
                              tasks.register(task);
                              wait(keySpaceId, key);
                            } catch (e) {
                              checkError(e);
                              return;
                            } finally {
                              expect(getKey(keySpaceId, `${key}_status`)).to.equal(false, `${name} managed to read the view with insufficient rights`);
                            }
                            expect(false).to.equal(true, `${name} managed to register a task with insufficient rights`);
                          }
                        });
                        it('by its properties', () => {
                          setKey(keySpaceId, key);
                          const taskId = 'task_read_view_multi_collections_with_1of_none_access_props_' + key;
                          const task = {
                            id: taskId,
                            name: taskId,
                            command: `(function (params) {
                      try {
                        const db = require('@arangodb').db;
                        db._view('${testView2Name}').properties();
                        global.KEY_SET('${keySpaceId}', '${key}_status', true);
                      } catch (e) {
                        global.KEY_SET('${keySpaceId}', '${key}_status', false);
                      }finally {
                        global.KEY_SET('${keySpaceId}', '${key}', true);
                      }
                    })(params);`
                          };
                          if (dbLevel['rw'].has(name)) {
                            tasks.register(task);
                            wait(keySpaceId, key);
                            expect(getKey(keySpaceId, `${key}_status`)).to.not.equal(true, `${name} managed to get a view properties with insufficient rights`);
                          } else {
                            try {
                              tasks.register(task);
                              wait(keySpaceId, key);
                            } catch (e) {
                              checkError(e);
                              return;
                            } finally {
                              expect(getKey(keySpaceId, `${key}_status`)).to.equal(false, `${name} managed to get the view properties with insufficient rights`);
                            }
                            expect(false).to.equal(true, `${key} managed to register a task with insufficient rights`);
                          }
                        });
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
