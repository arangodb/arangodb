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
const testColName = `${namePrefix}ColNew`;
const testColNameAnother = `${namePrefix}ColAnotherNew`;
const indexName = `${namePrefix}Inverted`;
const testViewType = "search-alias";

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
              return view !== null;
            };

            const rootDropView = (viewName = testViewName) => {
              helper.switchUser('root', dbName);
              try {
                db._dropView(viewName);
              } catch (ignored) { }
              helper.switchUser(name, dbName);
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

            });
          });
        });
      }
    });

  });
});
after(() => {
  arango.connectHandle(connectionHandle);
  db._drop(testColName);
  db._useDatabase('_system');
});
