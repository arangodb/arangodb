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
const helper = require('@arangodb/testutils/user-helper');
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

          describe('read on collection level', () => {
            const rootTestCollection = (switchBack = true) => {
              helper.switchUser('root', dbName);
              let col = db._collection(colName);
              if (switchBack) {
                helper.switchUser(name, dbName);
              }
              return col !== null;
            };

            const rootPrepareCollection = () => {
              if (rootTestCollection(false)) {
                db._collection(colName).truncate({ compact: false });
                db._collection(colName).save({_key: '123'});
              }
              helper.switchUser(name, dbName);
            };

            describe('read a document', () => {
              before(() => {
                db._useDatabase(dbName);
                rootPrepareCollection();
              });

              it('by key', () => {
                expect(rootTestCollection()).to.equal(true, 'Precondition failed, the collection does not exist');
                if ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                   (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                  let col = db._collection(colName);
                  expect(col.count()).to.equal(1, `${name} is not allowed to COUNT with sufficient rights`);
                  expect(col.document('123')._key).to.equal('123', 'Could not read a document by key, with sufficient rights');
                } else {
                  try {
                    let col = db._collection(colName);
                    let c = col.count();
                    expect(c).to.equal(-123, `${name} is allowed to COUNT with insufficient rights`);
                  } catch (e) {
                    expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                  }
                  try {
                    let col = db._collection(colName);
                    let doc = col.document('123');
                    expect(doc).to.be.undefined;
                  } catch (e) {
                    expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                  }
                }
              });

              it('by aql', () => {
                expect(rootTestCollection()).to.equal(true, 'Precondition failed, the collection does not exist');
                let q = `FOR x IN ${colName} RETURN x`;
                if ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                   (colLevel['rw'].has(name) || colLevel['ro'].has(name))) {
                  let res = db._query(q).toArray();
                  expect(res.length).to.equal(1, 'Could not read a document by aql, with sufficient rights');
                  expect(res[0]._key).to.equal('123', 'Did read an unexpected document');
                } else {
                  try {
                    let res = db._query(q).toArray();
                    expect(res.length).to.equal(0, 'Could read a document by aql, with insufficient rights');
                  } catch (e) {
                    expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
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
