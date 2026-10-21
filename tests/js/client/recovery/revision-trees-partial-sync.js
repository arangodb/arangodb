/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup, assertEqual, assertFalse, assertTrue */

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
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
let IM = global.instanceManager;
const {waitForEstimatorSync } = require('@arangodb/test-helper');

const colName1 = 'UnitTestsRecovery1';
const colName2 = 'UnitTestsRecovery2';
const colName3 = 'UnitTestsRecovery3';

function runSetupRoutine () {
  'use strict';
  db._drop(colName1);
  let c = db._create(colName1);
  c.ensureIndex({ type: "hash", fields: ["value"] });

  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ _key: "test_" + i });
  }
  c.insert(docs);

  db._drop(colName2);
  c = db._create(colName2);

  docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ _key: "test_" + i });
  }
  c.insert(docs);

  db._drop(colName3);
  c = db._create(colName3);

  docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ _key: "test_" + i });
  }
  c.insert(docs);

  waitForEstimatorSync();
  IM.debugSetFailAt("RocksDBMetaCollection::serializeRevisionTree");

  c = db._collection(colName1);
  docs = [];
  for (let i = 1000; i < 2000; ++i) {
    docs.push({ _key: "test_" + i });
  }
  c.insert(docs);

  c = db._collection(colName2);
  for (let i = 0; i < 500; ++i) {
    c.remove({ _key: "test_" + i });
  }

  c = db._collection(colName3);
  c.truncate();

  db._drop('test');
  c = db._create('test');
  c.save({ _key: 'crashme' }, true);

  waitForEstimatorSync();

  IM.debugTerminate('crashing server');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {
      waitForEstimatorSync(); // make sure estimates are consistent
    },
    tearDown: function () {},

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testRevisionTreeCounts: function() {
      const c1 = db._collection(colName1);
      assertEqual(c1._revisionTreeSummary().count, c1.count());
      assertEqual(c1._revisionTreeSummary().count, 2000);

      const c2 = db._collection(colName2);
      assertEqual(c2._revisionTreeSummary().count, c2.count());
      assertEqual(c2._revisionTreeSummary().count, 500);

      const c3 = db._collection(colName3);
      assertEqual(c3._revisionTreeSummary().count, c3.count());
      assertEqual(c3._revisionTreeSummary().count, 0);
    },

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

'use strict';
if (runSetup === true ) {
  runSetupRoutine();
  return 0;
} else {
  jsunity.run(recoverySuite);
  return jsunity.done();
}
