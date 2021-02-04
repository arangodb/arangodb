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
const foxxManager = require('@arangodb/foxx/manager');
const dbName = helper.dbName;
const rightLevels = helper.rightLevels;
const errors = require('@arangodb').errors;
const fs = require('fs');
const basePath = fs.makeAbsolute(fs.join(require('internal').pathForTesting('common'), 'test-data', 'apps'));

const userSet = helper.userSet;
const systemLevel = helper.systemLevel;
const dbLevel = helper.dbLevel;
const colLevel = helper.colLevel;

const arangodb = require('@arangodb');
const arango = require('@arangodb').arango;
const aql = arangodb.aql;
const db = require('internal').db;
for (let l of rightLevels) {
  systemLevel[l] = new Set();
  dbLevel[l] = new Set();
  colLevel[l] = new Set();
}

require("@arangodb/test-helper").waitForFoxxInitialized();

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
          const mount = `/${name}_mount`;
          before(() => {
            helper.switchUser(name, dbName);
            db._useDatabase(dbName);
          });

          after(() => {
            helper.switchUser('root', dbName);
            try {
              foxxManager.uninstall(mount, {force: true});
            } catch (e) {}
          });

          it('register a Foxx service', () => {
            if (dbLevel['rw'].has(name)) {
              try {
                foxxManager.install(fs.join(basePath, 'minimal-working-service'), mount);
                const size = db._query(aql`
                  FOR service IN _apps
                  FILTER service.mount == ${mount}
                  RETURN service.checksum
                `).toArray().length;
                expect(size).to.equal(1, `${name} could not register foxx service with sufficient rights`);
                // The service should return the user we acces it as:
                let res = arango.PUT(mount, '');
                expect(res.hello._documents[0]).to.be.equal(name);
              } catch (e) {
                if (e.errorNum === errors.ERROR_ARANGO_READ_ONLY.code ||
                    e.errorNum === errors.ERROR_FORBIDDEN.code) {
                  expect(false).to.be.equal(true, `${name} could not register Foxx service with sufficient rights`);
                } // FIXME: workaround ignore all other errors for now
              }
            } else {
              try {
                foxxManager.install(fs.join(basePath, 'minimal-working-service'), mount);
              } catch (e) {
                // expect(e.errorNum).to.equal(errors.ERROR_ARANGO_READ_ONLY.code);
                // TODO should be forbidden rather than read only
                // expect(e.errorNum).to.equal(errors.ERROR_FORBIDDEN.code);
              }
              const size = db._query(aql`
                FOR service IN _apps
                FILTER service.mount == ${mount}
                RETURN service.checksum
              `).toArray().length;
              expect(size).to.equal(0, `${name} could register Foxx service with insufficient rights`);
            }
          });
        });
      }
    }
  });
});
