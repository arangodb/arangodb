/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue */

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
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let db = require('@arangodb').db;
let internal = require('internal');
let jsunity = require('jsunity');

function runSetup () {
  'use strict';

  db._drop('UnitTestsRecovery');
  let c = db._create('UnitTestsRecovery');
  c.ensureIndex({ type: "hash", fields: ["value1"] });
  c.ensureIndex({ type: "hash", fields: ["value2"] });

  let docs = [];
  for (let i = 0; i < 10000; ++i) {
    docs.push({ value1: i, value2: (i % 10) });
  }
  c.insert(docs);
  
  internal.waitForEstimatorSync();
  internal.wal.flush(true, true);

  internal.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testIndexEstimators: function () {
      let c = db._collection('UnitTestsRecovery');

      assertEqual(3, c.getIndexes().length);
      let idx = c.getIndexes()[0];
      assertEqual('primary', idx.type);

      idx = c.getIndexes()[1];
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'value1' ], idx.fields);
      assertEqual('hash', idx.type);
      // values in the index are unique, but the estimates are approximate.
      // so leave some leeway.
      assertTrue(idx.selectivityEstimate >= 0.90, idx); 
      
      idx = c.getIndexes()[2];
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'value2' ], idx.fields);
      assertEqual('hash', idx.type);
      // values in the index are unique, but the estimates are approximate.
      // so leave some leeway.
      assertTrue(idx.selectivityEstimate < 0.005, idx); 
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

function main (argv) {
  'use strict';
  if (argv[1] === 'setup') {
    runSetup();
    return 0;
  } else {
    jsunity.run(recoverySuite);
    return jsunity.writeDone().status ? 0 : 1;
  }
}
