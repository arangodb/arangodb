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
const keySpaceId = 'task_create_graph_keyspace';

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

            const rootDropCollection = (colName) => {
              if (rootTestCollection(colName, false)) {
                db._collection(colName).drop();
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

            describe('create a', () => {
              before(() => {
                db._useDatabase(dbName);
                rootDropGraph();
              });

              after(() => {
                rootDropGraph();
                rootDropCollection(testEdgeColName);
                rootDropCollection(testVertexColName);
              });

              it('graph', () => {
                assertFalse(rootTestGraph(), 'Precondition failed, the graph still exists');
                assertFalse(rootTestCollection(testEdgeColName), 'Precondition failed, the edge collection still exists');
                assertFalse(rootTestCollection(testVertexColName), 'Precondition failed, the vertex collection still exists');
                setKey(keySpaceId, name);
                const taskId = 'task_create_graph_' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      require('@arangodb/general-graph')._create('${testGraphName}', [{
                        collection: '${testEdgeColName}',
                        'from': [ '${testVertexColName}' ],
                        'to': [ '${testVertexColName}' ]
                      }]);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}', true);
                    }
                  })(params);`
                };
                if (dbLevel['rw'].has(name)) {
                  if (dbLevel['rw'].has(name)) {
                    tasks.register(task);
                    wait(keySpaceId, name);
                    assertTrue(rootTestGraph(), 'Graph creation reported success, but graph was not found afterwards.');
                    assertTrue(rootTestCollection(testEdgeColName), 'Graph creation reported success, but edge colleciton was not found afterwards.');
                    assertTrue(rootTestCollection(testVertexColName), 'Graph creation reported success, but vertex colleciton was not found afterwards.');
                  } else {
                    tasks.register(task);
                    wait(keySpaceId, name);
                    assertFalse(rootTestGraph(), `${name} was able to create a graph with insufficent rights`);
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

              it('graph with existing collections', () => {
                rootDropGraph();

                rootCreateCollection(testEdgeColName, true);
                rootCreateCollection(testVertexColName, false);
                assertFalse(rootTestGraph(), 'Precondition failed, the graph still exists');
                assertTrue(rootTestCollection(testEdgeColName), 'Precondition failed, the edge collection still not exists');
                assertTrue(rootTestCollection(testVertexColName), 'Precondition failed, the vertex collection still not exists');
                setKey(keySpaceId, name + '_existing_collections');
                const taskId = 'task_create_graph_existing_collections' + name;
                const task = {
                  id: taskId,
                  name: taskId,
                  command: `(function (params) {
                    try {
                      require('@arangodb/general-graph')._create('${testGraphName}', [{
                        collection: '${testEdgeColName}',
                        'from': [ '${testVertexColName}' ],
                        'to': [ '${testVertexColName}' ]
                      }]);
                    } finally {
                      global.KEY_SET('${keySpaceId}', '${name}_existing_collections', true);
                    }
                  })(params);`
                };
                if (dbLevel['rw'].has(name)) {
                  if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                    tasks.register(task);
                    wait(keySpaceId, `${name}_existing_collections`);
                    assertTrue(rootTestGraph(), 'Graph creation reported success, but graph was not found afterwards.');
                    assertTrue(rootTestCollection(testEdgeColName), 'Graph creation reported success, but edge colleciton was not found afterwards.');
                    assertTrue(rootTestCollection(testVertexColName), 'Graph creation reported success, but vertex colleciton was not found afterwards.');
                  } else {
                    tasks.register(task);
                    wait(keySpaceId, `${name}_existing_collections`);
                    assertFalse(rootTestGraph(), `${name} was able to create a graph with insufficent rights`);
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
