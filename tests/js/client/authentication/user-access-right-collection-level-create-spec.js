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

// const arango = require('internal').arango;
const db = require('internal').db;
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
          });

          describe('update on collection level', () => {
            const rootTestCollection = (switchBack = true) => {
              helper.switchUser('root', dbName);
              let col = db._collection(colName);
              if (switchBack) {
                helper.switchUser(name, dbName);
              }
              return col !== null;
            };

            const rootTruncateCollection = () => {
              if (rootTestCollection(false)) {
                db._collection(colName).truncate({ compact: false });
              }
              helper.switchUser(name, dbName);
            };

            describe('create a document', () => {
              before(() => {
                db._useDatabase(dbName);
                rootTruncateCollection();
              });

              it('by key', () => {
                expect(rootTestCollection()).to.equal(true, 'Precondition failed, the collection does not exist');
                if ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                   colLevel['rw'].has(name)) {
                  let col = db._collection(colName);
                  try {
                    col.document('123');
                    expect(false).to.equal(true, 'Precondition failed, document already inserted.');
                  } catch (e) {}

                  col.save({
                    _key: '123',
                    foo: 'bar'
                  });
                  expect(col.document('123').foo).to.equal('bar', `${name} the create did not pass through...`);
                } else {
                  let hasReadAccess = ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                    (colLevel['rw'].has(name) || colLevel['ro'].has(name)));
                  if (hasReadAccess) {
                    let col = db._collection(colName);
                    try {
                      col.document('123');
                      expect(false).to.equal(true, 'Precondition failed, document already inserted.');
                    } catch (e) {}
                  }
                  let saveSuccess = false;
                  try {
                    let col = db._collection(colName);
                    col.save({
                      _key: '123',
                      foo: 'bar'
                    });
                    saveSuccess = true;
                  } catch (e) {
                    if (hasReadAccess) {
                      expect(e.errorNum).to.equal(errors.ERROR_ARANGO_READ_ONLY.code);
                    } else {
                      expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                    }
                  }
                  expect(saveSuccess).to.equal(false, `${name} created a document with insufficient rights.`);

                  if (hasReadAccess) {
                    let col = db._collection(colName);
                    try {
                      expect(col.document('123').foo).to.not.equal('bar', `${name} managed to create the document with insufficient rights`);
                    } catch (e) {}
                  }
                }
              });

              it('by aql', () => {
                expect(rootTestCollection()).to.equal(true, `Precondition failed, the collection does not exist`);
                let q = `INSERT {_key: '456', foo: 'bar'} IN ${colName} RETURN NEW`;
                if ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                   colLevel['rw'].has(name)) {
                  let col = db._collection(colName);
                  let res = db._query(q).toArray();
                  expect(res.length).to.equal(1, 'Could not create a document by aql, with sufficient rights');
                  expect(res[0].foo).to.equal('bar', 'Did not create the document properly');
                  expect(col.document('456').foo).to.equal('bar', `${name} managed to create the document with sufficient rights`);
                } else {
                  let hasReadAccess = ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                    (colLevel['rw'].has(name) || colLevel['ro'].has(name)));

                  try {
                    let res = db._query(q).toArray();
                    expect(res.length).to.equal(0, 'Could create a document by aql, with insufficient rights');
                    expect(res[0].foo).to.not.equal('bar', `${name} did create the document with insufficient rights`);
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
                      expect(col.document('456').foo).to.not.equal('bar', `${name} managed to create the document with insufficient rights`);
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
