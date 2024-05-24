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
// //////////////////////////////////////////////////////////////////////////////
// / @brief checks that no new databases were left on the SUT. 
// //////////////////////////////////////////////////////////////////////////////
exports.checker = class {
  constructor(runner) {
    this.runner = runner;
    this.name = 'databases';
  }
  setUp(te) { return true;}
  runCheck(te) {
    // TODO: we are currently filtering out the UnitTestDB here because it is 
    // created and not cleaned up by a lot of the `authentication` tests. This
    // should be fixed eventually
    try {
      db._useDatabase('_system');
      let databasesAfter = db._databases().filter((name) => name !== 'UnitTestDB');
      if (databasesAfter.length !== 1 || databasesAfter[0] !== '_system') {
        this.runner.setResult(te, false, {
          status: false,
          message: 'Cleanup missing - test left over databases: [ ' + JSON.stringify(databasesAfter)
        });
        return false;
      }
      return true;
    } catch (x) {
      this.runner.setResult(te, false, {
        status: false,
        message: 'failed to fetch the databases list: ' + x.message
      });
    }
    return false;
  }
};

