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
// / Licensed under the Apache License, Version 2.0 (the 'License');
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an 'AS IS' BASIS,
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
const tasks = require('@arangodb/tasks');
const pu = require('@arangodb/process-utils');
const users = require('@arangodb/users');
const internal = require('internal');
const helper = require('@arangodb/testutils/user-helper');
const download = internal.download;
const keySpaceId = 'task_update_user_keyspace';
const taskId = 'task_update_user_periodic';
const arango = internal.arango;
const taskPeriod = 0.3;

let { dropKeySpaceSystem, getKeySystem, setKeySystem } = require('@arangodb/testutils/client-utils.js');

const waitForTaskStart = () => {
  let i = 50;
  while (--i > 0) {
    internal.wait(0.1);
    if (getKeySystem(keySpaceId, 'bob') !== 0) {
      break;
    }
  }
  expect(i).to.be.above(0, 'Task must have run at least once');
};

const waitForTaskStop = () => {
  let i = 100;
  let last = 0;
  while (--i > 0) {
    internal.wait(taskPeriod);
    let current = getKeySystem(keySpaceId, 'bob');
    expect(current).to.not.be.equal(0, 'Received invalid key');
    if (current === last) {
      break;
    }
    last = current;
    internal.wait(taskPeriod);
  }
  expect(i).to.be.above(0, 'The repeatable task was able continue running with insufficient rights');
};

describe('User Rights Management', () => {
  before(() => {
    if(!users.exists('bob'))
      users.save('bob');
    users.grantDatabase('bob', '_system', 'rw');
    users.grantCollection('bob', '_system', '*', 'rw');

    helper.switchUser('bob');
  });
  after(() => {
    helper.switchUser('root', '_system');
    users.remove('bob');
    dropKeySpaceSystem(keySpaceId);
    try {
      internal.print('Unregistering task');
      tasks.unregister(taskId);
    } catch (e) {}
  });

  it('test cancelling periodic tasks', () => {
    helper.switchUser('bob');
    setKeySystem(keySpaceId, 'bob');

    tasks.register({
      id: taskId,
      name: taskId,
      period: taskPeriod,
      command: `(function (params) {
      global.GLOBAL_CACHE_SET('${keySpaceId}-bob', require('internal').time());
      })(params);`
    });

    internal.print('Started task, now waiting...');
    waitForTaskStart();

    helper.switchUser('root', '_system');
    internal.print('Downgrading rights');

    // should cause the task to stop eventually
    users.grantDatabase('bob', '_system', 'ro');
    internal.print('Waiting for task to stop');
    waitForTaskStop();
  });
});
