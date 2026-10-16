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
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const {assertEqual, assertTrue, assertFalse, assertNotEqual, assertNotUndefined} = jsunity.jsUnity.assertions;
const users = require('@arangodb/users');
const helper = require('@arangodb/testutils/user-helper');
const tasks = require('@arangodb/tasks');
const graphModule = require('@arangodb/general-graph');
const namePrefix = helper.namePrefix;
const dbName = helper.dbName;
const rightLevels = helper.rightLevels;
const testGraphName = `${namePrefix}GraphNew`;
const testEdgeColName = `${namePrefix}EdgeColNew`;
const testVertexColName = `${namePrefix}VertexColNew`;
const errors = require('@arangodb').errors;
const keySpaceId = 'task_drop_graph_keyspace';

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;

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

const setKeySpace = (keySpaceId, name) => {
  return executeJS(`global.KEY_SET('${keySpaceId}', '${name}', false);`);
};

const dropKeySpace = (keySpaceId) => {
  executeJS(`global.KEYSPACE_DROP('${keySpaceId}');`);
};

const getKey = (keySpaceId, key) => {
  return executeJS(`return global.KEY_GET('${keySpaceId}', '${key}');`).parsedBody === true;
};

const executeJS = (code) => {
  return arango.POST_RAW('/_admin/execute', code);
};

helper.switchUser('root', '_system');
helper.removeAllUsers();
helper.generateAllUsers();

describe('User Rights Management', () => {
  it('should check if all users are created', () => {
    helper.switchUser('root', '_system');
    assertTrue(userSet.size > 0); 
    assertEqual(userSet.size, helper.userCount);
    for (let name of userSet) {
      assertNotUndefined(users.document(name), `Could not find user: ${name}`);
    }
  });

  it('should test rights for', () => {
    assertTrue(userSet.size > 0);
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
            assertTrue(createKeySpace(keySpaceId), 'keySpace creation failed!');
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

            const rootCreateCollection = (colName, edge = false) => {
              if (!rootTestCollection(colName, false)) {
                if (edge) {
                  db._createEdgeCollection(colName);
                } else {
                  db._create(colName);
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

            const rootTestGraph = (switchBack = true) => {
              helper.switchUser('root', dbName);
              const graph = graphModule._exists(testGraphName);
              if (switchBack) {
                helper.switchUser(name, dbName);
              }
              return graph !== false;
            };

            const rootDropGraph = () => {
              if (rootTestGraph(false)) {
                graphModule._drop(testGraphName, true);
              }
              helper.switchUser(name, dbName);
            };

            const rootCreateGraph = () => {
              if (!rootTestGraph(false)) {
                graphModule._create(testGraphName, [{
                  collection: testEdgeColName,
                  'from': [ testVertexColName ],
                  'to': [ testVertexColName ]
                }]);
              }
              helper.switchUser(name, dbName);
            };

            describe('drop a', () => {
              before(() => {
                db._useDatabase(dbName);
                rootDropGraph();
                rootCreateCollection(testEdgeColName, true);
                rootCreateCollection(testVertexColName, false);
                rootCreateGraph();
              });

              after(() => {
                rootDropGraph();
              });

              it('graph', () => {
                assertTrue(rootTestGraph(), 'Precondition failed, the graph still not exists');
                setKeySpace(keySpaceId, name);
                const taskId = 'task_create_graph_' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      require('@arangodb/general-graph')._drop('${testGraphName}', true);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
                };
                if (dbLevel['rw'].has(name)) {
                  tasks.register(task);
                  wait(keySpaceId, name);
                  if (colLevel['rw'].has(name)) {
                    assertFalse(rootTestGraph(), 'Graph drop reported success, but graph was found afterwards.');
                    assertFalse(rootTestCollection(testEdgeColName), 'Graph drop reported success, but edge collection was found afterwards.');
                    assertFalse(rootTestCollection(testVertexColName), 'Graph drop reported success, but vertex collection was found afterwards.');
                  } else {
                    assertTrue(rootTestGraph(), `${name} was able to drop a graph with insufficent rights`);
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

            describe('drop a', () => {
              before(() => {
                db._useDatabase(dbName);
                rootDropGraph();
                rootCreateCollection(testEdgeColName, true);
                rootCreateCollection(testVertexColName, false);
                rootCreateGraph();
              });

              after(() => {
                rootDropGraph();
              });

              it('graph with specified collection access', () => {
                assertTrue(rootTestGraph(), 'Precondition failed, the graph still not exists');
                assertTrue(rootTestCollection(testEdgeColName), 'Precondition failed, the edge collection still not exists');
                assertTrue(rootTestCollection(testVertexColName), 'Precondition failed, the vertex collection still not exists');
                setKeySpace(keySpaceId, name + '_specified_collection_access');
                const taskId = 'task_create_graph_specified_collection_access' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      require('@arangodb/general-graph')._drop('${testGraphName}', true);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}_specified_collection_access', true);
                    }
                  })(params);`
                };
                if (dbLevel['rw'].has(name)) {
                  tasks.register(task);
                  wait(keySpaceId, `${name}_specified_collection_access`);
                  if (colLevel['rw'].has(name)) {
                    assertFalse(rootTestGraph(), 'Graph drop reported success, but graph was found afterwards.');
                    assertFalse(rootTestCollection(testEdgeColName), 'Graph drop reported success, but edge collection was found afterwards.');
                    assertFalse(rootTestCollection(testVertexColName), 'Graph drop reported success, but vertex collection was found afterwards.');
                  } else {
                    assertTrue(rootTestGraph(), `${name} was able to drop a graph with insufficent rights`);
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
