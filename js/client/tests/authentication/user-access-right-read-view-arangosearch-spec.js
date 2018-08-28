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
const testView1Name = `${namePrefix}View1New`;
const testView2Name = `${namePrefix}View2New`;
const testViewType = "arangosearch";
const testCol1Name = `${namePrefix}Col1New`;
const testCol2Name = `${namePrefix}Col2New`;
const testNumDocs = 3;

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;

const arango = require('internal').arango;
const tmp = require('internal');
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
          before(() => {
            db._useDatabase(dbName);
            rootCreateCollection(testCol1Name);
            rootCreateCollection(testCol2Name);
            var properties = { properties : {} };
            properties['links'] = {};
            properties['links'][testCol1Name] = { includeAllFields: true };
            rootCreateView(testView1Name, properties);
            properties['links'][testCol2Name] = { includeAllFields: true };
            rootCreateView(testView2Name, properties);
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
              db._create(colName);
              if (explicitRight !== '' && rightLevels.has(explicitRight))
              {
                if (helper.isLdapEnabledExternal()) {
                  users.grantCollection(':role:' + name, dbName, colName, explicitRight);
                } else {
                  users.grantCollection(name, dbName, colName, explicitRight);
                }
              } else {
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
            }
            helper.switchUser(name, dbName);
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

          const rootPrepareCollection = (colName, numDocs = 1, defKey = true) => {
              if (rootTestCollection(colName, false)) {
                db._collection(colName).truncate({ compact: false });
                for(var i = 0; i < numDocs; ++i)
                {
                  var doc = {prop1: colName + "_1", propI: i, plink: "lnk" + i};
                  if (!defKey) {
                    doc._key = colName + i;
                  }
                  db._collection(colName).save(doc, {waitForSync: true});
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

          const rootTestView = (viewName, switchBack = true) => {
            helper.switchUser('root', dbName);
            let view = db._view(viewName);
            if (switchBack) {
                helper.switchUser(name, dbName);
            }
            return view != null;
          };

          const rootCreateView = (viewName, properties = null) => {
            if (rootTestView(viewName, false)) {
              db._dropView(viewName);
            }
            let view =  db._createView(viewName, testViewType, {});
            if (properties != null) {
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
            expect(e.code).to.be.oneOf([403, 404], "Expected to get forbidden OR not found REST error code, but got another one");
            expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code, "Expected to get forbidden error number, but got another one");
          };

          describe('read a view', () => {
            before(() => {
              db._useDatabase(dbName);
            });

            it('by AQL with link to single collection', () => {
              expect(rootTestView(testView1Name)).to.equal(true, 'Precondition failed, the view doesn\'t exist');
              let query = `FOR d IN  ${testView1Name} RETURN d`;
              if ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                db._query(query);
                //FIXME: issue #429 (https://github.com/arangodb/backlog/issues/429)
                //let result = db._query(query).toArray();
                //expect(result.length).to.equal(testNumDocs, 'View read failed');
              } else {
                try {
                  db._query(query);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(false).to.equal(true, `${name} managed to perform a query on view with insufficient rights`);
              }
            });

            it('by AQL with links to multiple collections with same access level', () => {
              expect(rootTestView(testView2Name)).to.equal(true, 'Precondition failed, the view doesn\'t exist');
              let query = `FOR d IN  ${testView2Name} RETURN d`;
              if ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                db._query(query, null, { waitForSync: true });
                //FIXME: issue #429 (https://github.com/arangodb/backlog/issues/429)
                //let result = db._query(query, null, { waitForSync: true }).toArray();
                //expect(result.length).to.equal(testNumDocs*2, 'View read failed');
              } else {
                try {
                  db._query(query);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(false).to.equal(true, `${name} managed to perform a query on view with insufficient rights`);
              }
            });

            var descName = 'view with links to multiple collections with NONE access level to one of them';
            !(colLevel['rw'].has(name) || colLevel['ro'].has(name)) ? describe(descName, () => {}) :
            describe(descName, () => {
              before(() => {
                  rootGrantCollection(testCol2Name, name, 'none');
                  expect(rootTestView(testView2Name)).to.equal(true, 'Precondition failed, the view doesn\'t exist');
              });

              it('by AQL query (data)', () => {
                let query = `FOR d IN  ${testView2Name} RETURN d`;
                try {
                  db._query(query);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(false).to.equal(true, `${name} managed to perform a query on view with insufficient rights`);
              });
              it('by its properties', () => {
                try {
                  db._view(testView2Name).properties();
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(false).to.equal(true, `${name} managed to get the view properties with insufficient rights`);
              });
            });
          });
        });
      });
    }
  });
});
