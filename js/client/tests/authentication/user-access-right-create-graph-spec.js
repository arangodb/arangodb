/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, require*/

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
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const users = require('@arangodb/users');
const helper = require('@arangodb/user-helper');
const errors = require('@arangodb').errors;
const graphModule = require('@arangodb/general-graph');
const namePrefix = helper.namePrefix;
const dbName = helper.dbName;
const rightLevels = helper.rightLevels;
const testGraphName = `${namePrefix}GraphNew`;
const testEdgeColName = `${namePrefix}EdgeColNew`;
const testVertexColName = `${namePrefix}VertexColNew`;

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

const switchUser = (user, dbname) => {
  arango.reconnect(arango.getEndpoint(), dbname, user, '');
};

switchUser('root', '_system');
helper.removeAllUsers();

describe('User Rights Management', () => {
  before(helper.generateAllUsers);
  after(helper.removeAllUsers);

  it('should check if all users are created', () => {
    switchUser('root', '_system');
    expect(userSet.size).to.equal(helper.userCount);
    for (let name of userSet) {
      expect(users.document(name), `Could not find user: ${name}`).to.not.be.undefined;
    }
  });

  it('should test rights for', () => {
    for (let name of userSet) {
      let canUse = false;
      try {
        switchUser(name, dbName);
        canUse = true;
      } catch (e) {
        canUse = false;
      }

      if (canUse) {
        describe(`user ${name}`, () => {
          before(() => {
            switchUser(name, dbName);
          });

          describe('administrate on db level', () => {
            const rootTestCollection = (colName, switchBack = true) => {
              switchUser('root', dbName);
              let col = db._collection(colName);
              if (switchBack) {
                switchUser(name, dbName);
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
              switchUser(name, dbName);
            };

            const rootDropCollection = (colName) => {
              if (rootTestCollection(colName, false)) {
                db._collection(colName).drop();
              }
              switchUser(name, dbName);
            };

            const rootTestGraph = (switchBack = true) => {
              switchUser('root', dbName);
              const graph = graphModule._exists(testGraphName);
              if (switchBack) {
                switchUser(name, dbName);
              }
              return graph !== false;
            };

            const rootDropGraph = () => {
              if (rootTestGraph(false)) {
                graphModule._drop(testGraphName, true);
              }
              switchUser(name, dbName);
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
                expect(rootTestGraph()).to.equal(false, 'Precondition failed, the graph still exists');
                expect(rootTestCollection(testEdgeColName)).to.equal(false, 'Precondition failed, the edge collection still exists');
                expect(rootTestCollection(testVertexColName)).to.equal(false, 'Precondition failed, the vertex collection still exists');
                if (dbLevel['rw'].has(name)) {
                  graphModule._create(testGraphName, [{
                    collection: testEdgeColName,
                    'from': [ testVertexColName ],
                    'to': [ testVertexColName ]
                  }]);
                  expect(rootTestGraph()).to.equal(true, 'Graph creation reported success, but graph was not found afterwards.');
                  expect(rootTestCollection(testEdgeColName)).to.equal(true, 'Graph creation reported success, but edge colleciton was not found afterwards.');
                  expect(rootTestCollection(testVertexColName)).to.equal(true, 'Graph creation reported success, but vertex colleciton was not found afterwards.');
                } else {
                  try {
                    graphModule._create(testGraphName, [{
                      collection: testEdgeColName,
                      'from': [ testVertexColName ],
                      'to': [ testVertexColName ]
                    }]);
                  } catch (e) {
                    expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                  }
                  expect(rootTestGraph()).to.equal(false, `${name} was able to create a graph with insufficent rights`);
                }
              });

              it('graph with existing collections', () => {
                rootCreateCollection(testEdgeColName, true);
                rootCreateCollection(testVertexColName, false);
                expect(rootTestGraph()).to.equal(false, 'Precondition failed, the graph still exists');
                expect(rootTestCollection(testEdgeColName)).to.equal(true, 'Precondition failed, the edge collection still not exists');
                expect(rootTestCollection(testVertexColName)).to.equal(true, 'Precondition failed, the vertex collection still not exists');
                if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                  graphModule._create(testGraphName, [{
                    collection: testEdgeColName,
                    'from': [ testVertexColName ],
                    'to': [ testVertexColName ]
                  }]);
                  expect(rootTestGraph()).to.equal(true, 'Graph creation reported success, but graph was not found afterwards.');
                  expect(rootTestCollection(testEdgeColName)).to.equal(true, 'Graph creation reported success, but edge colleciton was not found afterwards.');
                  expect(rootTestCollection(testVertexColName)).to.equal(true, 'Graph creation reported success, but vertex colleciton was not found afterwards.');
                } else {
                  try {
                    graphModule._create(testGraphName, [{
                      collection: testEdgeColName,
                      'from': [ testVertexColName ],
                      'to': [ testVertexColName ]
                    }]);
                  } catch (e) {
                    expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                  }
                  expect(rootTestGraph()).to.equal(false, `${name} was able to create a graph with insufficent rights`);
                }
              });
            });
          });
        });
      }
    }
  });
});
