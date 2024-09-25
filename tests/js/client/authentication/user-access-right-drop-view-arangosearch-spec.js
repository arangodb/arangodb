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
const errors = require('@arangodb').errors;
const db = require('@arangodb').db;
const namePrefix = helper.namePrefix;
const dbName = helper.dbName;
const rightLevels = helper.rightLevels;
const testViewName = `${namePrefix}ViewNew`;
const testCol1Name = `${namePrefix}Col1New`;
const testCol2Name = `${namePrefix}Col2New`;
const indexName = `${namePrefix}Inverted`;

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
    for (let testViewType of ["arangosearch", "search-alias"]) {
      describe(`view type ${testViewType}`, () => {
        for (let name of userSet) {
          try {
            helper.switchUser(name, dbName);
          } catch (e) {
            continue;
          }

          describe(`user ${name}`, () => {
            before(() => {
              helper.switchUser(name, dbName);
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
                expect(e.code).to.be.oneOf([403], "Expected to get forbidden or internal REST error code, but got another one");
                expect(e.errorNum).to.oneOf([errors.ERROR_FORBIDDEN.code, errors.ERROR_ARANGO_READ_ONLY.code], "Expected to get forbidden error number, but got another one");
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

                it('view without links', () => {
                  rootCreateView(testViewName, testViewType);
                  expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, the view doesn not exist');
                  if (dbLevel['rw'].has(name)) {
                    db._dropView(testViewName);
                    expect(rootTestView(testViewName)).to.equal(false, 'View deletion reported success, but view was found afterwards');
                  } else {
                    try {
                      db._dropView(testViewName);
                    } catch (e) {
                      checkError(e);
                      return;
                    }
                    expect(rootTestView(testViewName)).to.equal(true, `${name} was able to delete a view with insufficent rights`);
                  }
                });

                if (testViewType === 'arangosearch') {
                  it('view with link to non-existing collection', () => {
                    rootDropView(testViewName);
                    rootCreateCollection("NonExistentCol");
                    rootCreateView(testViewName, testViewType, { links: { "NonExistentCol" : { includeAllFields: true } } });
                    rootDropCollection("NonExistentCol");
                    expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, the view doesn not exist');
                    if (dbLevel['rw'].has(name)) {
                      db._dropView(testViewName);
                      expect(rootTestView(testViewName)).to.equal(false, 'View deletion reported success, but view was found afterwards');
                    } else {
                      try {
                        db._dropView(testViewName);
                      } catch (e) {
                        checkError(e);
                        return;
                      }
                      if (!dbLevel['rw'].has(name)) {
                        expect(rootTestView(testViewName)).to.equal(true, `${name} was able to delete a view with insufficent rights`);
                      }
                    }
                  });

                  it('view with link to existing collection', () => {
                    rootDropView(testViewName);
                    rootDropCollection(testCol1Name);

                    rootCreateCollection(testCol1Name);
                    rootCreateView(testViewName, testViewType, { links: { [testCol1Name]: { includeAllFields: true } } });
                    expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, the view doesn not exist');
                    if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                      db._dropView(testViewName);
                      expect(rootTestView(testViewName)).to.equal(false, 'View deletion reported success, but view was found afterwards');
                    } else {
                      try {
                        db._dropView(testViewName);
                      } catch (e) {
                        checkError(e);
                        return;
                      } finally {
                        expect(rootTestView(testViewName)).to.equal(true, `${name} was able to drop a view with insufficent rights`);
                      }
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
                      if (dbLevel['rw'].has(name) && colLevel['ro'].has(name)) {
                        db._dropView(testViewName);
                        expect(rootTestView(testViewName)).to.equal(false, 'View deletion reported success, but view was found afterwards');
                      } else {
                        try {
                          db._dropView(testViewName);
                        } catch (e) {
                          checkError(e);
                          return;
                        } finally {
                          expect(rootTestView(testViewName)).to.equal(true, `${name} was able to drop a view with insufficent rights`);
                        }
                      }
                    });
                }
              });
            });
          });
        }

      });

    }
  });
});
after(() => {
  arango.connectHandle(connectionHandle);
  db._drop(testViewName);
  db._useDatabase('_system');
});
