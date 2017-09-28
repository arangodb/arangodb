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

          describe('update on collection level', () => {

            const rootTestCollection = (switchBack = true) => {
              switchUser('root', dbName);
              let col = db._collection(colName);
              if (switchBack) {
                switchUser(name, dbName);
              }
              return col !== null;
            };

            const rootPrepareCollection = () => {
              if (rootTestCollection(false)) {
                db._collection(colName).truncate();
                db._collection(colName).save({_key: '123'});
                db._collection(colName).save({_key: '456'});
              }
              switchUser(name, dbName);
            };

            describe('remove a document', () => {
              before(() => {
                db._useDatabase(dbName);
                rootPrepareCollection();
              });

              it('by key', () => {
                expect(rootTestCollection()).to.equal(true, 'Precondition failed, the collection does not exist.');
                if ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                   colLevel['rw'].has(name)) {
                  let col = db._collection(colName);
                  expect(col.document('123')._key).to.equal('123', 'Precondition failed, document does not exist.');
                  col.remove('123');
                  try {
                    col.document('123');
                    expect(true).to.be(false, `${name} could not drop the document with sufficient rights`);
                  } catch (e) {}
                } else {
                  let hasReadAccess = ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                    (colLevel['rw'].has(name) || colLevel['ro'].has(name)));
                  try {
                    let col = db._collection(colName);
                    if (hasReadAccess) {
                      expect(col.document('123')._key).to.equal('123', 'Precondition failed, document does not exist.');
                    }
                    col.remove('123');
                    expect(true).to.be(false);
                  } catch (e) {
                    if (hasReadAccess) {
                      expect(e.errorNum).to.equal(errors.ERROR_ARANGO_READ_ONLY.code);
                    } else {
                      expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                    }
                  }
                  if (hasReadAccess) {
                    let col = db._collection(colName);
                    try {
                      expect(col.document('123')._key).to.equal('123', `${name} managed to remove the document with insufficient rights`);
                    } catch (e) {}
                  }
                }
              });

              it('by aql', () => {
                expect(rootTestCollection()).to.equal(true, 'Precondition failed, the collection does not exist');
                let q = `REMOVE '456' IN ${colName} RETURN OLD`;
                if ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                   colLevel['rw'].has(name)) {
                  let col = db._collection(colName);
                  let res = db._query(q).toArray();
                  expect(res.length).to.equal(1, 'Could not remove a document by aql, with sufficient rights');
                  expect(res[0]._key).to.equal('456', 'Did not remove the document properly');
                  try {
                    col.document('456');
                    expect(true).to.be(false, 'Document still in collection after remove');
                  } catch (e) {}
                } else {
                  let hasReadAccess = ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                    (colLevel['rw'].has(name) || colLevel['ro'].has(name)));

                  try {
                    let res = db._query(q).toArray();
                    expect(res.length).to.equal(1, 'Could remove a document by aql, with sufficient rights');
                    expect(res[0]._key).to.equal('456', `${name} did remove the document with insufficient rights`);
                  } catch (e) {
                    if (hasReadAccess) {
                      expect(e.errorNum).to.equal(errors.ERROR_ARANGO_READ_ONLY.code);
                    } else {
                      expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                    }
                  }

                  if (hasReadAccess) {
                    let col = db._collection(colName);
                    try {
                      expect(col.document('456')._key).to.equal('456', `${name} managed to remove the document with insufficient rights`);
                    } catch (e) {}
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
