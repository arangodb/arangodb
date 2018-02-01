/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, require */

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
// / @author Mark Vollmary
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const expect = require('chai').expect;
const users = require('@arangodb/users');
const db = require('@arangodb').db;
const helper = require('@arangodb/user-helper');

const colName = 'PermissionsTestCollection';
const rightLevels = ['rw', 'ro', 'none'];
const dbs = ['*', '_system'];
const cols = ['*', colName];
const defUsers = helper.ldapUsers;
const userSet = new Set();
const internal = require('internal');

const createUsers = () => {
  db._useDatabase('_system');
  for (const user of defUsers) {
    for (const db of dbs) {
      for (const dbLevel of rightLevels) {
        for (const col of cols) {
          for (const colLevel of rightLevels) {
            const name = user.name;
            userSet.add({
              name,
              db: {
                name: db,
                permission: dbLevel
              },
              col: {
                name: col,
                permission: colLevel
              },
              config: {
                username: user.name,
                userrole: user.role
              }
            });
          }
        }
      }
    }
  }
};

const createRole = (user) => {
  users.save(':role:' + user.config.username, null, true);
  users.grantDatabase(':role:' + user.config.username, user.db.name, user.db.permission);
  users.grantCollection(':role:' + user.config.username, user.db.name, user.col.name, user.col.permission);
};

const removeRole = (user) => {
  users.remove(':role:' + user.config.username);
};

describe('User Rights Management', () => {
  before(() => {
    // only in-memory set of users
    db._useDatabase('_system');
    db._create(colName);
    internal.wait(1);
    createUsers();
  });

  after(() => {
    db._drop(colName);
  });

  it('should test permission for', () => {
    for (const user of userSet) {
      if (user.name === 'arangoadmin') {
        describe(`user ${user.name} with their role ${user.config.username}`, () => {
          before(() => {
            createRole(user);
          });
          after(() => {
            removeRole(user);
          });
          it('on database', () => {
            const permission = users.permission(':role:' + user.config.username, '_system');
            expect(permission).to.equal(user.db.permission,
                                        'Expected different permission for _system');
          });
          it('on collection', () => {
            const permission = users.permission(':role:' + user.config.username, '_system', colName);
            expect(permission).to.equal(user.col.permission);
          });
          it('on collection after revoking database level permission', () => {
            users.revokeCollection(':role:' + user.config.username, '_system', colName);
            users.reload();
            const permission = users.permission(':role:' + user.config.username, user.db.name, colName);

            let result = users.permissionFull(':role:' + user.config.username);
            let collPerm;
            if (user.db.name !== '*') {
              collPerm = result[user.db.name].collections['*'];
            } else {
              // this is checking collections permission level on database '*' which is not possible
              collPerm = permission;
            }

            expect(permission).to.equal(collPerm);
          });
          it('on database after revoking database level permission', () => {
            users.revokeDatabase(':role:' + user.config.username, user.db.name);
            users.reload();

            let result = users.permissionFull(':role:' + user.config.username);
            const dbPerm = result['*'].permission;
            const permission = users.permission(':role:' + user.config.username, user.db.name);

            expect(permission).to.equal(dbPerm,
                                        'Expected different permission for _system');
          });
        });
      }
    }
  });
});
