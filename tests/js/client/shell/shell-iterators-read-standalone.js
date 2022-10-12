/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual, assertNotUndefined, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const fs = require("fs");
const jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const base = require(fs.join(internal.pathForTesting('client'),
                             'shell', 'shell-iterators.inc'));

function StandaloneAqlIteratorReadSuite() {
  'use strict';

  const ctx = {
    query: (...args) => internal.db._query(...args),
    collection: (name) => internal.db[name],
    abort: () => {},
  };
  const permute = function(run) {
    [ {}, {intermediateCommitCount: 111} ].forEach((opts) => {
      run(ctx, opts);
    });
  };
  
  let suite = {};
  deriveTestSuite(base.IteratorReadSuite(permute, true), suite, '_StandaloneAqlRead');
  return suite;
}

jsunity.run(StandaloneAqlIteratorReadSuite);

return jsunity.done();
