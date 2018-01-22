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
const request = require('@arangodb/request');
const arango = require('@arangodb').arango;

const colName = 'PermissionsTestCollection';
const rightLevels = ['rw', 'ro', 'none'];
const dbs = ['*', '_system'];
const cols = ['*', colName];
const userSet = new Set();
const internal = require('internal');

const createUsers = () => {
  db._useDatabase('_system');
  for (const db of dbs) {
    for (const dbLevel of rightLevels) {
      for (const col of cols) {
        for (const colLevel of rightLevels) {
          const name = `user_${db}_${dbLevel}_${col}_${colLevel}`;
          userSet.add({
            name,
            db: {
              name: db,
              permission: dbLevel
            },
            col: {
              name: col,
              permission: colLevel
            }
          });
        }
      }
    }
  }
};

const baseUrl = () => {
  return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
};

const createUser = (user) => {
  users.save(user.name, '', true);
  users.grantDatabase(user.name, user.db.name, user.db.permission);
  users.grantCollection(user.name, user.db.name, user.col.name, user.col.permission);
};

const removeUser = (user) => {
  users.remove(user.name);
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
      describe(`user ${user.name}`, () => {
        before(() => {
          createUser(user);
        });
        after(() => {
          removeUser(user);
        });
        it('on database', () => {
          const permission = users.permission(user.name, '_system');
          expect(permission).to.equal(user.db.permission,
            'Expected different permission for _system');
        });
        it('on collection', () => {
          const permission = users.permission(user.name, '_system', colName);
          expect(permission).to.equal(user.col.permission);
        });
        it('on collection after revoking database level permission', () => {
          users.revokeCollection(user.name, '_system', colName);
          const permission = users.permission(user.name, user.db.name, colName);

          const res = request(baseUrl() + '/_api/user/' + user.name + '/database?full=true');
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', 200);
          expect(res.body).to.be.an('string');
          const obj = JSON.parse(res.body);
          expect(obj).to.have.property('result');

          let collPerm;
          if (user.db.name !== '*') {
            collPerm = obj.result[user.db.name].collections['*'];
          } else {
            // this is checking collections permission level on database '*' which is not possible
            collPerm = permission;
          }

          expect(permission).to.equal(collPerm);
        });
        it('on database after revoking database level permission', () => {
          users.revokeDatabase(user.name, user.db.name);
          const permission = users.permission(user.name, user.db.name);

          const res = request(baseUrl() + '/_api/user/' + user.name + '/database?full=true');
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', 200);
          expect(res.body).to.be.an('string');
          const obj = JSON.parse(res.body);
          expect(obj).to.have.property('result');
          const dbPerm = obj.result['*'].permission;

          expect(permission).to.equal(dbPerm,
            'Expected different permission for _system');
        });
      });
    }
  });
});
