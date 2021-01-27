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
const errors = require('@arangodb').errors;
const db = require('@arangodb').db;
const namePrefix = helper.namePrefix;
const dbName = helper.dbName;
const rightLevels = helper.rightLevels;
const testViewName = `${namePrefix}ViewNew`;
const testViewType = "arangosearch";
const testColName = `${namePrefix}ColNew`;
const testColNameAnother = `${namePrefix}ColAnotherNew`;

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
            helper.switchUser('root', dbName);
            try {
              db._collection(colName).drop();
            } catch (ignored) { }
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

          const rootDropView = (viewName = testViewName) => {
            helper.switchUser('root', dbName);
            try {
              db._dropView(viewName);
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
            expect(e.errorNum).to.oneOf([errors.ERROR_FORBIDDEN.code, errors.ERROR_ARANGO_READ_ONLY.code], "Expected to get forbidden error number, but got another one");
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
              expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view exists');

              if (dbLevel['rw'].has(name)) {
                db._createView(testViewName, testViewType, {});
                expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
              } else {
                try {
                  db._createView(testViewName, testViewType, {});
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
              }
            });

            it('view with non-empty parameters (except links)', () => {
              rootDropView(testViewName);
              rootDropCollection(testColName);

              expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view still exists');

              if (dbLevel['rw'].has(name)) {
                db._createView(testViewName, testViewType, { cleanupIntervalStep: 20 });
                expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                expect(rootGetViewProps(testViewName, true)["cleanupIntervalStep"]).to.equal(20, 'View creation reported success, but view property was not set as expected during creation');
              } else {
                try {
                  db._createView(testViewName, testViewType, { cleanupIntervalStep: 20 });
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
              }
            });

            // FIXME: uncomment after PR 6199 is done with respectful changes
            /*
            it('view with links to existing collection', () => {
              rootDropView(testViewName);
              rootDropCollection(testColName);

              rootCreateCollection(testColName);
              expect(rootTestView(testViewName)).to.equal(false, 'Precondition failed, the view still exists');
              expect(rootTestCollection(testColName)).to.equal(true, 'Precondition failed, the collection still not exists');

              if (dbLevel['rw'].has(name) && colLevel['rw'].has(name) || colLevel['ro'].has(name)) {
                db._createView(testViewName, testViewType, { links: { [testColName]: { includeAllFields: true } } });
                expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                expect(rootTestViewHasLinks(testViewName, [`${testColName}`])).to.equal(true, 'View links expected to be visible, but were not found afterwards');
              } else {
                try {
                  db._createView(testViewName, testViewType, { links: { [testColName]: { includeAllFields: true } } });
                } catch (e) {
                  checkError(e);
                  return;
                }
                if(!dbLevel['rw'].has(name)) {
                  expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                }
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

              if (dbLevel['rw'].has(name) && colLevel['rw'].has(name) || colLevel['ro'].has(name)) {
                db._createView(testViewName, testViewType, { links: { 
                  [testColName]: { includeAllFields: true }, [testColNameAnother]: { includeAllFields: true } 
                  }
                });
                expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                expect(rootTestViewHasLinks(testViewName, [`${testColName}`, `${testColNameAnother}`])).to.equal(true, 'View links expected to be visible, but were not found afterwards');
              } else {
                try {
                  db._createView(testViewName, testViewType, { links: { 
                    [testColName]: { includeAllFields: true }, [testColNameAnother]: { includeAllFields: true } 
                    }
                  });
                } catch (e) {
                  checkError(e);
                  return;
                }
                if(!dbLevel['rw'].has(name)) {
                  expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                }
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

              if (dbLevel['rw'].has(name) && colLevel['rw'].has(name)) {
                db._createView(testViewName, testViewType, { links: { 
                  [testColName]: { includeAllFields: true }, [testColNameAnother]: { includeAllFields: true }
                  } 
                });
                expect(rootTestView(testViewName)).to.equal(true, 'View creation reported success, but view was not found afterwards');
                expect(rootTestViewHasLinks(testViewName, [`${testColName}`, `${testColNameAnother}`])).to.equal(true, 'View links expected to be visible, but were not found afterwards');
              } else {
                try {
                  db._createView(testViewName, testViewType, { links: {
                    [testColName]: { includeAllFields: true }, [testColNameAnother]: { includeAllFields: true }
                    } 
                  });
                } catch (e) {
                  checkError(e);
                  return;
                }
                if(!dbLevel['rw'].has(name)) {
                  expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
                }
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

              if (dbLevel['rw'].has(name)) {
                try {
                  db._createView(testViewName, testViewType, { links: {
                    [testColName]: { includeAllFields: true }, [testColNameAnother]: { includeAllFields: true }
                    }
                  });
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(rootTestView(testViewName)).to.equal(false, `${name} was able to create a view with insufficent rights`);
            });
            */
          });
        });
      });
    }
  });
});
