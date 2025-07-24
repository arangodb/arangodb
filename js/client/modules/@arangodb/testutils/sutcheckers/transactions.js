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
// / @brief checks no transactions were left on the SUT by tests
// //////////////////////////////////////////////////////////////////////////////

exports.checker = class {
  constructor(runner) {
    this.runner = runner;
    this.name = 'transactions';
  }
  setUp (te) {
    return true;
  }
  runCheck (te) {
    try {
      let tl = db._transactions();
      if (tl.length !== 0) {
        this.runner.setResult(te, false, {
          status: false,
          message: 'Cleanup of transactions missing - found transactions left over: ' +
            JSON.stringify(tl)
        }, this.name);
        return false;
      }
    } catch (x) {
      this.runner.setResult(te, true, {
        status: false,
        message: 'failed to fetch the transactions on the system after the test: ' + x.message
      }, this.name);
      return false;
    }
    return true;
  }
};

