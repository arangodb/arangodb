/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, beforeEach, after, afterEach, it, require*/

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
const testViewRename = `${namePrefix}ViewRename`;
const testViewType = "arangosearch";
const testCol1Name = `${namePrefix}Col1New`;
const testCol2Name = `${namePrefix}Col2New`;

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
    //require('internal').sleep(30);
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

        describe('administrate on db level', () => {
          before(() => {
            db._useDatabase(dbName);
          });

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

          const rootGetViewProps = (viewName, switchBack = true) => {
            helper.switchUser('root', dbName);
            let properties = db._view(viewName).properties();
            if (switchBack) {
                helper.switchUser(name, dbName);
            }
            return properties;
          };

          const rootCreateView = (viewName, properties = null) => {
            if (rootTestView(viewName, false)) {
              try {
                db._dropView(viewName);
              } catch (ignored) {}
            }
            let view =  db._createView(viewName, testViewType, {});
            if (properties != null) {
              view.properties(properties, false);
            }
            helper.switchUser(name, dbName);
          };

          const rootGetDefaultViewProps = () => {
            helper.switchUser('root', dbName);
            try {
              var tmpView = db._createView(this.title + "_tmpView", testViewType, {});
              var properties = tmpView.properties();
              db._dropView(this.title + "_tmpView");
            } catch (ignored) { }
            helper.switchUser(name, dbName);
            return properties;
          };

          const rootDropView = (viewName) => {
            if (rootTestView(viewName, false)) {
              try {
                db._dropView(viewName);
              } catch (ignored) {}
            } 
            helper.switchUser(name, dbName);
          };

        const checkError = (e) => {
          expect(e.code).to.equal(403, "Expected to get forbidden REST error code, but got another one");
          expect(e.errorNum).to.oneOf([errors.ERROR_FORBIDDEN.code, errors.ERROR_ARANGO_READ_ONLY.code], "Expected to get forbidden error number, but got another one");
        };

          describe('update a', () => {
            beforeEach(() => {
              rootCreateCollection(testCol1Name);
              rootCreateCollection(testCol2Name);
              rootPrepareCollection(testCol1Name);
              rootPrepareCollection(testCol2Name);

              rootCreateView(testViewName, { links: { [testCol1Name] : {includeAllFields: true } } });
            });

            afterEach(() => {
              rootDropView(testViewRename);
              rootDropView(testViewName);

              rootDropCollection(testCol1Name);
              rootDropCollection(testCol2Name);
            });

            it('view by name', () => {
              require('internal').sleep(2);
              expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, view was not found');
              if (dbLevel['rw'].has(name)) {
                try {
                  db._view(testViewName).rename(testViewRename);
                } catch (e) {
                  //FIXME: remove try/catch block after renaming will work in cluster
                  if (e.code === 501 && e.errorNum === 1470) {
                    return;
                  } else if (e.code === 403) {
                    return; // not authorised is valid if a non-read collection is present in the view
                  } else {
                    throw e;
                  }
                }
                expect(rootTestView(testViewRename)).to.equal(true, 'View renaming reported success, but updated view was not found afterwards');
              } else {
                try {
                  db._view(testViewName).rename(testViewRename);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(rootTestView(testViewRename)).to.equal(false, `${name} was able to rename a view with insufficent rights`);
              }
            });

            it('view by property except links (partial)', () => {
              expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, view was not found');
              if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                db._view(testViewName).properties({ "cleanupIntervalStep": 1 }, true);
                expect(rootGetViewProps(testViewName, true)["cleanupIntervalStep"]).to.equal(1, 'View property update reported success, but property was not updated');
              } else {
                try {
                  db._view(testViewName).properties({ "cleanupIntervalStep": 1 }, true);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(true).to.equal(false, `${name} was able to update a view with insufficent rights`);
              }
            });

            it('view by property except links (full)', () => {
              expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, view was not found');
              if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                db._view(testViewName).properties({ "cleanupIntervalStep": 1 }, false);
                expect(rootGetViewProps(testViewName, true)["cleanupIntervalStep"]).to.equal(1, 'View property update reported success, but property was not updated');
              } else {
                try {
                  db._view(testViewName).properties({ "cleanupIntervalStep": 1 }, false);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(true).to.equal(false, `${name} was able to update a view with insufficent rights`);
              }
            });

            it('view by properties remove (full)', () => {
              expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, view was not found');
              if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                db._view(testViewName).properties({}, false);
                expect(rootGetViewProps(testViewName, true)).to.deep.equal(rootGetDefaultViewProps(), 'View properties update reported success, but properties were not updated');
              } else {
                try {
                  db._view(testViewName).properties({}, false);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(true).to.equal(false, `${name} was able to update a view with insufficent rights`);
              }
            });

            it('view by existing link update (partial)', () => {
              expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, view was not found');
              if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                db._view(testViewName).properties({ links: { [testCol1Name]: { includeAllFields: true, analyzers: ["text_de","text_en"] } } }, true);
                expect(rootGetViewProps(testViewName, true)["links"][testCol1Name]["analyzers"]).to.eql(["text_de","text_en"], 'View link update reported success, but property was not updated');
              } else {
                try {
                  db._view(testViewName).properties({ links: { [testCol1Name]: { includeAllFields: true, analyzers: ["text_de","text_en"] } } }, true);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(true).to.equal(false, `${name} was able to update a view with insufficent rights`);
              }
            });

            it('view by existing link update (full)', () => {
              expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, view was not found');
              if (dbLevel['rw'].has(name) && (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                db._view(testViewName).properties({ links: { [testCol1Name]: { includeAllFields: true, analyzers: ["text_de","text_en"] } } }, false);
                expect(rootGetViewProps(testViewName, true)["links"][testCol1Name]["analyzers"]).to.eql(["text_de","text_en"], 'View link update reported success, but property was not updated');
              } else {
                try {
                  db._view(testViewName).properties({ links: { [testCol1Name]: { includeAllFields: true, analyzers: ["text_de","text_en"] } } }, false);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(true).to.equal(false, `${name} was able to update a view with insufficent rights`);
              }
            });

            var itName = 'view by new link to RW collection (partial)';
            !(colLevel['ro'].has(name) || colLevel['none'].has(name)) ? it.skip(itName) :
            it(itName, () => {
              expect(rootTestView(testViewName)).to.equal(true, 'Precondition failed, view was not found');
              rootGrantCollection(testCol2Name, name, 'rw');
              if (dbLevel['rw'].has(name) && colLevel['ro'].has(name)) {
                db._view(testViewName).properties({ links: { [testCol2Name]: { includeAllFields: true, analyzers: ["text_de"] } } }, true);
                expect(rootGetViewProps(testViewName, true)["links"][testCol2Name]["analyzers"]).to.eql(["text_de"], 'View link update reported success, but property was not updated');
              } else {
                try {
                  db._view(testViewName).properties({ links: { [testCol2Name]: { includeAllFields: true, analyzers: ["text_de"] } } }, true);
                } catch (e) {
                  checkError(e);
                  return;
                }
                expect(true).to.equal(false, `${name} was able to update a view with insufficent rights`);
              }
            });
          });
        });
      });
    }
  });
});
