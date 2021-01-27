/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, beforeEach, afterEach, before, after, it, require */

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
const helper = require('@arangodb/testutils/user-helper');
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
            const rootTestDB = (switchBack = true) => {
              helper.switchUser('root');
              const allDB = db._databases();
              for (let i of allDB) {
                if (i === testDBName) {
                  if (switchBack) {
                    helper.switchUser(name);
                  }
                  return true;
                }
              }
              if (switchBack) {
                helper.switchUser(name);
              }
              return false;
            };

            const rootDropDB = () => {
              if (rootTestDB(false)) {
                db._dropDatabase(testDBName);
              }
              helper.switchUser(name);
            };

            beforeEach(() => {
              db._useDatabase('_system');
              rootDropDB();
            });

            afterEach(() => {
              rootDropDB();
            });

            it('create database', () => {
              if (systemLevel['rw'].has(name)) {
                // User needs rw on _system
                db._createDatabase(testDBName);
                expect(rootTestDB()).to.equal(true, 'DB creation reported success, but DB was not found afterwards.');
              } else {
                try {
                  db._createDatabase(testDBName);
                } catch (e) {
                }
                expect(rootTestDB()).to.equal(false, `${name} was able to create a database with insufficent rights`);
              }
            });
            it('create database with root user directly given', () => {
              if (systemLevel['rw'].has(name)) {
                // options empty, because there are not options at the moment
                var options = {};
                var users = [{username: 'root'}];
                // User needs rw on _system
                db._createDatabase(testDBName, options, users);
                expect(rootTestDB()).to.equal(true, 'DB creation reported success, but DB was not found afterwards.');
              } else {
                try {
                  db._createDatabase(testDBName);
                } catch (e) {
                  //
                }
                expect(rootTestDB()).to.equal(false, `${name} was able to create a database with insufficent rights`);
              }
            });
            it('create database with user directly given', () => {
              if (systemLevel['rw'].has(name)) {
                let ldap = helper.isLdapEnabledExternal();
                // options empty, because there are not options at the moment
                var options = {};
                var users = [{username: ldap ? ":role:" + name : name}];
                // User needs rw on _system
                db._createDatabase(testDBName, options, users);
                expect(rootTestDB()).to.equal(true, 'DB creation reported success, but DB was not found afterwards.');
              } else {
                try {
                  db._createDatabase(testDBName);
                } catch (e) {
                  //print(e);
                }
                expect(rootTestDB()).to.equal(false, `${name} was able to create a database with insufficent rights`);
              }
            });
            it('create database with multiple users directly given', () => {
              if (systemLevel['rw'].has(name)) {
                let ldap = helper.isLdapEnabledExternal();
                // options empty, because there are not options at the moment
                var options = {};
                var users = [{username: 'root'}, {username: ldap ? ":role:" + name : name}];
                // User needs rw on _system
                db._createDatabase(testDBName, options, users);
                expect(rootTestDB()).to.equal(true, 'DB creation reported success, but DB was not found afterwards.');
              } else {
                try {
                  db._createDatabase(testDBName);
                } catch (e) {
                  //print(e);
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

