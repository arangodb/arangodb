/* jshint strict: false, sub: true */
/* global print db arango */
'use strict';

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////
const tasks = require("@arangodb/tasks");

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks no tasks were left on the SUT by tests
// //////////////////////////////////////////////////////////////////////////////

exports.checker = class {
  constructor(runner) {
    this.runner = runner;
    this.name = 'tasks';
    this.taskCount = 0;
  }
  filterTasksList(taskList) {
    return taskList.filter(task => !task.hasOwnProperty('id') || task.id !== 'foxx-queue-manager');
  }
  setUp (te) {
    try {
      this.taskCount = this.filterTasksList(tasks.get()).length;
    } catch (x) {
      this.runner.setResult(te, true, {
        status: false,
        message: 'failed to fetch the tasks on the system before the test: ' + x.message
      });
      return false;
    }
    return true;
  }
  runCheck (te) {
    try {
      if (this.filterTasksList(tasks.get()).length !== this.taskCount) {
        this.runner.setResult(te, false, {
          status: false,
          message: 'Cleanup of tasks missing - found tasks left over: ' +
            JSON.stringify(tasks.get())
        });
        return false;
      }
    } catch (x) {
      this.runner.setResult(te, true, {
        status: false,
        message: 'failed to fetch the tasks on the system after the test: ' + x.message
      });
      return false;
    }
    return true;
  }
};

