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
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const users = require('@arangodb/users');
const helper = require('@arangodb/user-helper');
const errors = require('@arangodb').errors;
const dbName = helper.dbName;
const colName = helper.colName;
const rightLevels = helper.rightLevels;

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
            const rootTestCollection = (switchBack = true) => {
              switchUser('root', dbName);
              let col = db._collection(colName);
              if (switchBack) {
                switchUser(name, dbName);
              }
              return col !== null;
            };

            describe('manipulate', () => {
              before(() => {
                db._useDatabase(dbName);
              });

              it('indexes', () => {
                expect(rootTestCollection()).to.equal(true, 'Precondition failed, the collection does not exist');
                if (dbLevel['rw'].has(name) && colLevel['rw'].has(name)) {
                  let col = db._collection(colName);
                  let origIdxCount = col.getIndexes().length;
                  expect(origIdxCount).to.equal(1); // Only primary index
                  let idx = col.ensureHashIndex('foo');
                  expect(col.getIndexes().length).to.equal(origIdxCount + 1, 'Ensure Index reported success, but collection does not show it.');
                  col.dropIndex(idx);
                  expect(col.getIndexes().length).to.equal(origIdxCount, 'Drop Index reported success, but collection does still show it.');
                } else {
                  let hasReadAccess = ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                    (colLevel['rw'].has(name) || colLevel['ro'].has(name)));
                  try {
                    let col = db._collection(colName);
                    col.ensureHashIndex('foo');
                  } catch (e) {
                    expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                  }
                  if (hasReadAccess) {
                    let col = db._collection(colName);
                    let origIdxCount = col.getIndexes().length;
                    expect(col.getIndexes().length).to.equal(origIdxCount, `${name} was able to create a new index on the collection, with insufficent rights.`);
                  }
                }
              });

              it('properties', () => {
                expect(rootTestCollection()).to.equal(true, 'Precondition failed, the collection does not exist');
                if (dbLevel['rw'].has(name) && colLevel['rw'].has(name)) {
                  let col = db._collection(colName);
                  let wfs = col.properties().waitForSync;
                  col.properties({waitForSync: !wfs});
                  expect(col.properties().waitForSync).to.equal(!wfs, 'Change properties reported success, but collection did not change it.');
                } else {
                  let hasReadAccess = ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                    (colLevel['rw'].has(name) || colLevel['ro'].has(name)));
                  try {
                    let col = db._collection(colName);
                    let wfs = col.properties().waitForSync;
                    col.properties({waitForSync: !wfs});
                  } catch (e) {
                    expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                  }
                  if (hasReadAccess) {
                    let col = db._collection(colName);
                    let wfs = col.properties().waitForSync;
                    expect(col.properties().waitForSync).to.equal(wfs, `${name} was able to change properties of the collection, with insufficent rights.`);
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
