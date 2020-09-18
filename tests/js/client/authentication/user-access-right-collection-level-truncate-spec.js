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
    expect(userSet.size).to.be.greaterThan(0); 
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

            const rootPrepareCollection = () => {
              if (rootTestCollection(false)) {
                const col = db._collection(colName);
                col.truncate({ compact: false });
                col.save({_key: '123'});
                col.save({_key: '456'});
                col.save({_key: '789'});
                col.save({_key: '987'});
                col.save({_key: '654'});
                col.save({_key: '321'});
              }
              helper.switchUser(name, dbName);
            };

            const rootCount = () => {
              let count = -1;
              if (rootTestCollection(false)) {
                count = db._collection(colName).count();
              }
              helper.switchUser(name, dbName);
              return count;
            };

            describe('truncate a collection', () => {
              before(() => {
                db._useDatabase(dbName);
                rootPrepareCollection();
              });

              it('via js', () => {
                expect(rootTestCollection()).to.equal(true, 'Precondition failed, the collection does not exist');
                expect(rootCount()).to.equal(6, 'Precondition failed, too few documents.');
                if ((dbLevel['rw'].has(name) || dbLevel['ro'].has(name)) &&
                   colLevel['rw'].has(name)) {
                  let col = db._collection(colName);
                  col.truncate({ compact: false });
                  expect(rootCount()).to.equal(0, `${name} could not truncate the collection with sufficient rights`);
                } else {
                  var success = false;
                  try {
                    let col = db._collection(colName);
                    col.truncate({ compact: false });
                    success = true;
                  } catch (e) {
                    const err = colLevel['ro'].has(name) ? errors.ERROR_ARANGO_READ_ONLY : errors.ERROR_FORBIDDEN;
                    expect(e.errorNum).to.equal(err.code, `${name} getting an unexpected error code`);
                  }
                  expect(success).to.equal(false, `${name} succeeded with truncate without getting an error (insufficent rights)`);
                  expect(rootCount()).to.equal(6, `${name} could not truncate the collection with sufficient rights`);
                }
              });
            });
          });
        });
      }
    }
  });
});
