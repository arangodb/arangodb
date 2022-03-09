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
const users = require('@arangodb/users');
const helper = require('@arangodb/testutils/user-helper');
const tasks = require('@arangodb/tasks');
const pu = require('@arangodb/testutils/process-utils');
const download = require('internal').download;
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

const setKeySpace = (keySpaceId, name) => {
  return executeJS(`global.KEY_SET('${keySpaceId}', '${name}', false);`);
};

const dropKeySpace = (keySpaceId) => {
  executeJS(`global.KEYSPACE_DROP('${keySpaceId}');`);
};

const getKey = (keySpaceId, key) => {
  return executeJS(`return global.KEY_GET('${keySpaceId}', '${key}');`).body === 'true';
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

describe('User Rights Management', () => {
  if (!helper.isLdapEnabledExternal()) {
    it('should check if all users are created', () => {
      helper.switchUser('root', '_system');
      expect(userSet.size).to.be.greaterThan(0); 
      expect(userSet.size).to.equal(helper.userCount);
      for (let name of userSet) {
        expect(users.document(name), `Could not find user: ${name}`).to.not.be.undefined;
      }
    });
  }

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

            const rootCreateCollection = (colName, edge = false) => {
              if (!rootTestCollection(colName, false)) {
                if (edge) {
                  db._createEdgeCollection(colName);
                } else {
                  db._create(colName);
                }
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
                expect(!rootTestGraph()).to.equal(false, 'Precondition failed, the graph still not exists');
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
                    expect(!rootTestGraph()).to.equal(true, 'Graph drop reported success, but graph was found afterwards.');
                    expect(!rootTestCollection(testEdgeColName)).to.equal(true, 'Graph drop reported success, but edge collection was found afterwards.');
                    expect(!rootTestCollection(testVertexColName)).to.equal(true, 'Graph drop reported success, but vertex collection was found afterwards.');
                  } else {
                    expect(!rootTestGraph()).to.equal(false, `${name} was able to drop a graph with insufficent rights`);
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
                expect(rootTestGraph()).to.equal(true, 'Precondition failed, the graph still not exists');
                expect(rootTestCollection(testEdgeColName)).to.equal(true, 'Precondition failed, the edge collection still not exists');
                expect(rootTestCollection(testVertexColName)).to.equal(true, 'Precondition failed, the vertex collection still not exists');
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
                    expect(!rootTestGraph()).to.equal(true, 'Graph drop reported success, but graph was found afterwards.');
                    expect(!rootTestCollection(testEdgeColName)).to.equal(true, 'Graph drop reported success, but edge collection was found afterwards.');
                    expect(!rootTestCollection(testVertexColName)).to.equal(true, 'Graph drop reported success, but vertex collection was found afterwards.');
                  } else {
                    expect(!rootTestGraph()).to.equal(false, `${name} was able to drop a graph with insufficent rights`);
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
        });
      }
    }
  });
});
