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
// / @brief checks no tasks were left on the SUT by tests
// //////////////////////////////////////////////////////////////////////////////

const tu = require('@arangodb/testutils/test-utils');

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks that no new collections were left on the SUT. 
// //////////////////////////////////////////////////////////////////////////////
exports.checker = class {
  constructor(runner) {
    this.runner = runner;
    this.name = 'collections';
    this.collectionsBefore = [];
  }
  setUp(te) {
    try {
      db._collections().forEach(collection => {
        this.collectionsBefore.push(collection._name);
      });
    } catch (x) {
      this.runner.setResult(te, true, {
        status: false,
        message: 'failed to fetch the previously available collections: ' + x.message
      });
      return false;
    }
    return true;
  }
  runCheck(te) {
    let collectionsAfter = [];
    try {
      db._collections().forEach(collection => {
        collectionsAfter.push(collection._name);
      });
    } catch (x) {
      this.runner.setResult(te, false, {
        status: false,
        message: 'failed to fetch the currently available collections: ' + x.message
      });
      return false;
    }
    let delta = tu.diffArray(this.collectionsBefore, collectionsAfter).filter(function(name) {
      return ! ((name[0] === '_') || (name === "compact") || (name === "election")
                || (name === "log")); // exclude system/agency collections from the comparison
      return (name[0] !== '_'); // exclude system collections from the comparison
    });
    if (delta.length !== 0) {
       this.runner.setResult(te, true, {
        status: false,
        message: 'Cleanup missing - test left over collection: ' + delta
       });
      return false;
    }
    return true;
  }
};
