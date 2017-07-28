/* jshint globalstrict:true, strict:true, maxlen: 5000 */
/* global describe, before, after, it, require, beforeEach */

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
const tasks = require('@arangodb/tasks');
const pu = require('@arangodb/process-utils');
const download = require('internal').download;
const rightLevels = helper.rightLevels;
const keySpaceId = 'task_update_user_keyspace';

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

const wait = (keySpaceId, key, count) => {
  for (let i = 0; i < 200; i++) {
    if (getKey(keySpaceId, key) >= count) break;
    require('internal').wait(0.1);
  }
};

const createKeySpace = (keySpaceId) => {
  return executeJS(`return global.KEYSPACE_CREATE('${keySpaceId}', 128, true);`).body === 'true';
};

const dropKeySpace = (keySpaceId) => {
  executeJS(`global.KEYSPACE_DROP('${keySpaceId}');`);
};

const setKey = (keySpaceId, key) => {
  return executeJS(`global.KEY_SET('${keySpaceId}', '${key}', 0);`);
};

const getKey = (keySpaceId, key) => {
  return Number(executeJS(`return global.KEY_GET('${keySpaceId}', '${key}');`).body);
};

const executeJS = (code) => {
  let httpOptions = pu.makeAuthorizationHeaders({
    username: 'root',
    password: ''
  });
  httpOptions.method = 'POST';
  httpOptions.timeout = 1800;
  httpOptions.returnBodyOnError = true;
  return download(arango.getEndpoint().replace('tcp', 'http') + '/_admin/execute?returnAsJSON=true',
    code,
    httpOptions);
};

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
          const taskId = 'task_drop_user_' + name;
          before(() => {
            switchUser(name);
            expect(createKeySpace(keySpaceId)).to.equal(true, 'keySpace creation failed!');
            setKey(keySpaceId, name);
          });

          after(() => {
            switchUser('root');
            dropKeySpace(keySpaceId);
            try {
              tasks.unregister(taskId);
            } catch (e) {}
          });

          describe('administrate on server level', () => {
            beforeEach(() => {
              db._useDatabase('_system');
            });

            it('update a user', () => {
              if (systemLevel['rw'].has(name)) {
                tasks.register({
                  id: taskId,
                  name: taskId,
                  period: 0.3,
                  command: `(function (params) {
                    let count = global.KEY_GET('${keySpaceId}', '${name}');
                    global.KEY_SET('${keySpaceId}', '${name}', ++count);
                  })(params);`
                });
                wait(keySpaceId, name, 3);
                require('@arangodb/users').grantDatabase(name, '_system', 'none');
                require('internal').wait(1);
                expect(getKey(keySpaceId, name)).to.be.below(6, `The repeatable task of ${name} was able run with insufficient rights`);
              }
            });
          });
        });
      }
    }
  });
});

