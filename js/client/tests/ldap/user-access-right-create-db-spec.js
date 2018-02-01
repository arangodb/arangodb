/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, beforeEach, afterEach, before, after, it, require, print */

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
const users = require('@arangodb/users');
const namePrefix = helper.namePrefix;
const rightLevels = helper.rightLevels;
const testDBName = `${namePrefix}DBNew`;

const userSet = helper.ldapUsers;
const systemLevel = helper.systemLevel;

const arango = require('internal').arango;
const db = require('internal').db;
for (let l of rightLevels) {
  systemLevel[l] = new Set();
}

const switchUser = (user) => {
  arango.reconnect(arango.getEndpoint(), '_system', user.name, user.password);
};

const createRoles = () => {
  switchUser({name: 'arangoadmin', password: 'password'});
  users.save(':role:role1', null, true);
};

const removeRoles = () => {
  switchUser({name: 'arangoadmin', password: 'password'});
  users.remove(':role:role1');
};

helper.removeAllUsers();

describe('User Rights Management', () => {
  before(helper.generateAllUsers);
  before(createRoles);
  after(helper.removeAllUsers);
  after(removeRoles);

  it('should test rights for', () => {
    for (let user of userSet) {
      let canUse = false;
      let name = user.name;
      try {
        switchUser(user);
        canUse = true;
      } catch (e) {
        canUse = false;
      }

      if (canUse) {
        describe(`user ${name}`, () => {
          before(() => {
            switchUser(user);
          });

          describe('administrate on server level', () => {
            const rootTestDB = (switchBack = true) => {
              switchUser({name: 'root', password: ''});
              const allDB = db._databases();
              for (let i of allDB) {
                if (i === testDBName) {
                  if (switchBack) {
                    switchUser(user);
                  }
                  return true;
                }
              }
              if (switchBack) {
                switchUser(user);
              }
              return false;
            };

            const rootDropDB = () => {
              if (rootTestDB(false)) {
                db._dropDatabase(testDBName);
              }
              switchUser(user);
            };

            beforeEach(() => {
              db._useDatabase('_system');
              rootDropDB();
            });

            afterEach(() => {
              rootDropDB();
            });

            it('create database', () => {
              print(name);
              print(users.permissionFull(name));
              if (users.permission(name)._system === 'rw') {
                // User needs rw on _system
                db._createDatabase(testDBName);
                expect(rootTestDB()).to.equal(true, 'DB creation reported success, but DB was not found afterwards.');
              } else {
                try {
                  db._createDatabase(testDBName);
                } catch (e) {
                  print(e);
                }
                expect(rootTestDB()).to.equal(false, `${name} was able to create a database with insufficent rights`);
              }
            });
            it('create database with root user directly given', () => {
              if (users.permission(name)._system === 'rw') {
                // options empty, because there are not options at the moment
                var options = {};
                var usersOpt = [{username: 'root'}];
                // User needs rw on _system
                db._createDatabase(testDBName, options, usersOpt);
                expect(rootTestDB()).to.equal(true, 'DB creation reported success, but DB was not found afterwards.');
              } else {
                try {
                  db._createDatabase(testDBName);
                } catch (e) {
                  print(e);
                }
                expect(rootTestDB()).to.equal(false, `${name} was able to create a database with insufficent rights`);
              }
            });
            it('create database with user directly given', () => {
              if (users.permission(name)._system === 'rw') {
                // options empty, because there are not options at the moment
                var options = {};
                var usersOpt = [{username: name}];
                // User needs rw on _system
                db._createDatabase(testDBName, options, usersOpt);
                expect(rootTestDB()).to.equal(true, 'DB creation reported success, but DB was not found afterwards.');
              } else {
                try {
                  db._createDatabase(testDBName);
                } catch (e) {
                  print(e);
                }
                expect(rootTestDB()).to.equal(false, `${name} was able to create a database with insufficent rights`);
              }
            });
            it('create database with multiple users directly given', () => {
              if (users.permission(name)._system === 'rw') {
                // options empty, because there are not options at the moment
                var options = {};
                var usersOpt = [{username: 'root'}, {username: name}];
                // User needs rw on _system
                db._createDatabase(testDBName, options, usersOpt);
                expect(rootTestDB()).to.equal(true, 'DB creation reported success, but DB was not found afterwards.');
              } else {
                try {
                  db._createDatabase(testDBName);
                } catch (e) {
                  print(e);
                }
                expect(rootTestDB()).to.equal(false, `${name} was able to create a database with insufficent rights`);
              }
            });
          });
        });
      }
    }
  });
});
