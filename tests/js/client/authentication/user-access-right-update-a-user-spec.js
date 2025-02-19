/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, print, afterEach, beforeEach */

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
// / @author Michael Hackstein
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const users = require('@arangodb/users');
const helper = require('@arangodb/testutils/user-helper');
const namePrefix = helper.namePrefix;
const rightLevels = helper.rightLevels;
const errors = require('@arangodb').errors;

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;
const testUser = `${namePrefix}TestUser`;

const arango = require('internal').arango;
let connectionHandle = arango.getConnectionHandle();
const db = require('internal').db;
for (let l of rightLevels) {
  systemLevel[l] = new Set();
  dbLevel[l] = new Set();
  colLevel[l] = new Set();
}

helper.removeAllUsers();
helper.generateAllUsers();

describe('User Rights Management', () => {
  it('should test rights for', () => {
    expect(userSet.size).to.be.greaterThan(0); 
    for (let name of userSet) {
      let canUse = false;
      try {
        helper.switchUser(name);
        canUse = true;
      } catch (e) {
        canUse = false;
      }

      if (canUse) {
        describe(`user ${name}`, () => {
          before(() => {
            helper.switchUser(name);
          });

          describe('administrate on server level', () => {
            const rootTestUser = (switchBack = true) => {
              helper.switchUser('root');
              try {
                const u = users.document(testUser);
                if (switchBack) {
                  helper.switchUser(name);
                }
                return u !== undefined;
              } catch (e) {
                if (switchBack) {
                  helper.switchUser(name);
                }
                return false;
              }
            };

            const rootDropUser = () => {
              if (rootTestUser(false)) {
                users.remove(testUser);
              }
              helper.switchUser(name);
            };

            const rootCreateUser = () => {
              if (!rootTestUser(false)) {
                users.save(testUser, '', true);
              }
              helper.switchUser(name);
            };

            beforeEach(() => {
              db._useDatabase('_system');
              rootDropUser();
              rootCreateUser();
            });

            afterEach(() => {
              rootDropUser();
            });

            it('update a user', () => {
              if (systemLevel['rw'].has(name)) {
                // User needs rw on _system
                users.grantDatabase(testUser, '_system', 'rw');
              } else {
                let didGrant = false;
                try {
                  users.grantDatabase(testUser, '_system', 'rw');
                  didGrant = true;
                } catch (e) {
                  expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
                }
                expect(didGrant).to.equal(false, `${name} was able to update a user with insufficient rights.`);
              }
            });
          });
        });
      }
    }
  });
});

after(() => {
  arango.connectHandle(connectionHandle);
  db._useDatabase('_system');
});
