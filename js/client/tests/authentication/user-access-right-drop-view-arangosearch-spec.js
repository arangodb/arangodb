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
const helper = require('@arangodb/user-helper');
const errors = require('@arangodb').errors;
const db = require('@arangodb').db;
const namePrefix = helper.namePrefix;
const dbName = helper.dbName;
const rightLevels = helper.rightLevels;
const testViewName = `${namePrefix}ViewNew`;
const testViewType = "arangosearch";
const testCol1Name = `${namePrefix}Col1New`;
const testCol2Name = `${namePrefix}Col1New`;

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

          const rootDropCollection = (colName) => {
            helper.switchUser('root', dbName);
            try {
              db._collection(colName).drop();
            } catch (ignored) { }
            helper.switchUser(name, dbName);
          };

          const rootGrantCollection = (colName, user, explicitRight) => {
              if (rootTestCollection(colName, false)) {
                if (explicitRight !== '' && rightLevels.includes(explicitRight))
                {
                  if (helper.isLdapEnabledExternal()) {
                    users.grantCollection(':role:' + user, dbName, colName, explicitRight);
                  } else {
                    users.grantCollection(user, dbName, colName, explicitRight);
                  }
                }
              helper.switchUser(user, dbName);
              }
            };

          const rootTestView = () => {
            helper.switchUser('root', dbName);
            const view = db._view(testViewName);
            helper.switchUser(name, dbName);
            return view != null;
          };

          const rootCreateView = (viewName = testViewName, links = null) => {
            if (rootTestView(viewName, false)) {
              db._dropView(viewName);
            }
            let view =  db._createView(viewName, testViewType, {});
            if (links != null) {
              view.properties(links, false);
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

          describe('drop a', () => {
            before(() => {
              db._useDatabase(dbName);
              rootCreateView();
            });

            after(() => {
              rootDropView();
              rootDropCollection(testCol1Name);
              rootDropCollection(testCol2Name);
            });

            it('view without links', () => {
              expect(rootTestView()).to.equal(true, 'Precondition failed, the view doesn not exist');
              if (dbLevel['rw'].has(name)) {
                db._dropView(testViewName);
                expect(rootTestView()).to.equal(false, 'View deletion reported success, but view was found afterwards');
              } else {
                try {
                  db._dropView(testViewName);
                } catch (e) {
                  expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code, "Expected to get forbidden error code, but got another one");
                }
                expect(rootTestView()).to.equal(true, `${name} was able to delete a view with insufficent rights`);
              }
            });

            it('view with link to non-existing collection', () => {
              rootDropView();
              rootDropCollection(testCol1Name);

              rootCreateCollection(testCol1Name);
              rootCreateView(testViewName, { links: { "NonExistentCol" : { includeAllFields: true } } });
              expect(rootTestView()).to.equal(true, 'Precondition failed, the view doesn not exist');
              if (dbLevel['rw'].has(name)) {
                db._dropView(testViewName);
                expect(rootTestView()).to.equal(false, 'View deletion reported success, but view was found afterwards');
              } else {
                try {
                  db._dropView(testViewName);
                } catch (e) {
                  expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code, "Expected to get forbidden error code, but got another one");
                }
                if(!dbLevel['rw'].has(name)) {
                  expect(rootTestView()).to.equal(true, `${name} was able to delete a view with insufficent rights`);
                }
              }
            });

            it('view with link to existing collection', () => {
              rootDropView();
              rootDropCollection(testCol1Name);

              rootCreateCollection(testCol1Name);
              rootCreateView(testViewName, { links: { [testCol1Name]: { includeAllFields: true } } });
              expect(rootTestView()).to.equal(true, 'Precondition failed, the view doesn not exist');
              if (dbLevel['rw'].has(name) && colLevel['rw'].has(name)) {
                db._dropView(testViewName);
                expect(rootTestView()).to.equal(false, 'View deletion reported success, but view was found afterwards');
              } else {
                try {
                  db._dropView(testViewName);
                } catch (e) {
                  expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code, "Expected to get forbidden error code, but got another one");
                }
                if(!dbLevel['rw'].has(name)) {
                  expect(rootTestView()).to.equal(true, `${name} was able to delete a view with insufficent rights`);
                }
              }
            });

            it('view with links to multiple collections (switching 1 of them as RW during RO/NONE of a user collection level)', () => {
              rootDropView();
              rootDropCollection(testCol1Name);

              rootCreateCollection(testCol1Name);
              rootCreateCollection(testCol2Name);
              rootCreateView(testViewName, { links: { [testCol1Name]: { includeAllFields: true }, [testCol2Name]: { includeAllFields: true } } });
              expect(rootTestView()).to.equal(true, 'Precondition failed, the view doesn not exist');
              if (dbLevel['rw'].has(name)) {
                if(colLevel['rw'].has(name))
                {
                  db._dropView(testViewName);
                  expect(rootTestView()).to.equal(false, 'View deletion reported success, but view was found afterwards');
                } else {
                  try {
                    rootGrantCollection(testCol2Name, name, 'rw');
                    db._dropView(testViewName);
                    expect(rootTestView()).to.equal(true, `${name} was able to delete a view with insufficent rights`);
                  } catch (e) {
                    expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code, "Expected to get forbidden error code, but got another one");
                  }
                }
              } else {
                try {
                  db._dropView(testViewName);
                } catch (e) {
                  expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code, "Expected to get forbidden error code, but got another one");
                }
                if(!dbLevel['rw'].has(name)) {
                  expect(rootTestView()).to.equal(true, `${name} was able to delete a view with insufficent rights`);
                }
              }
            });
          });
        });
      });
    }
  })
});