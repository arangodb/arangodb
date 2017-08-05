/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, require, print, beforeEach */

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
const helper = require('@arangodb/user-helper');
const namePrefix = helper.namePrefix;
const rightLevels = helper.rightLevels;
const testDBName = `${namePrefix}DBNew`;

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

const switchUser = (user) => {
  arango.reconnect(arango.getEndpoint(), '_system', user, '');
};
helper.removeAllUsers();

describe('User Rights Management', () => {
  before(helper.generateAllUsers);
  after(helper.removeAllUsers);

  it('should test rights for', () => {
    for (let name of userSet) {
      let canUse = false;
      try {
        switchUser(name);
        canUse = true;
      } catch (e) {
        canUse = false;
      }

      if (canUse) {
        describe(`user ${name}`, () => {
          before(() => {
            switchUser(name);
          });

          describe('administrate on server level', () => {
            const rootTestDB = (switchBack = true) => {
              switchUser('root');
              const allDB = db._databases();
              for (let i of allDB) {
                if (i === testDBName) {
                  if (switchBack) {
                    switchUser(name);
                  }
                  return true;
                }
              }
              if (switchBack) {
                switchUser(name);
              }
              return false;
            };

            const rootDropDB = () => {
              if (rootTestDB(false)) {
                db._dropDatabase(testDBName);
              }
              switchUser(name);
            };

            const rootCreateDB = () => {
              if (!rootTestDB(false)) {
                db._createDatabase(testDBName);
              }
              switchUser(name);
            };

            beforeEach(() => {
              db._useDatabase('_system');
              rootCreateDB();
            });

            after(() => {
              rootDropDB();
            });

            it('drop database', () => {
              if (systemLevel['rw'].has(name)) {
                // User needs rw on _system
                db._dropDatabase(testDBName);
                expect(rootTestDB()).to.equal(false, 'DB drop reported success, but DB was still found afterwards.');
              } else {
                try {
                  db._dropDatabase(testDBName);
                } catch (e) {
                  print(e);
                }
                expect(rootTestDB()).to.equal(true, `${name} was able to drop a database with insufficient rights`);
              }
            });
          });
        });
      }
    }
  });
});
