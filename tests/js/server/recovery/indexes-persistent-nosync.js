/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for dump/reload
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  db._drop('UnitTestsRecovery');
  let c = db._create('UnitTestsRecovery');
  c.ensureIndex({ type: 'persistent', fields: ['value'] });

  internal.debugSetFailAt('TransactionWriteCommitMarkerNoRocksSync');

  db._executeTransaction({
    collections: { write: 'UnitTestsRecovery' },
    action: `function () {
      const db = require('internal').db;
      const c = db._collection('UnitTestsRecovery');
      for (let i = 0; i < 1000; ++i) {
        c.save({ value: i }, { waitForSync: true });
      }
    }`
  });

  internal.debugTerminate('crashing server');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {},
    tearDown: function () {},

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testIndexesRocksDBNoSync: function () {
      const c = db._collection('UnitTestsRecovery');
      const idxs = c.getIndexes();
      assertEqual(idxs.length, 2);
      let idx = idxs[1];
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ 'value' ], idx.fields);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(1, c.byExample({ value: i }).toArray().length, i);
      }
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
