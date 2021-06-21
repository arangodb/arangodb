/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for revision trees - corrupt and repair
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
// / @author Max Neunhoeffer
// / @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
const wait = internal.wait;

function waitForTreeReady(c) {
  while (true) {
    let pending = c._revisionTreePendingUpdates();
    // For whatever reason, sometimes we get an empty object back, let's
    // skip that.
    if (pending.hasOwnProperty("inserts")) {
      if (pending.inserts === 0 &&
          pending.removes === 0 &&
          pending.truncates === 0) {
        return;
      }
    }
    console.log("Still waiting for pending updates in tree: ", pending);
    wait(0.5);
  }
}

function corruptRepairSuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  const colName1 = 'UnitTestsTreeCorrupt';

  return {
    setUp: function () {
      db._drop(colName1);
      let c = db._create(colName1);

      for (let i = 0; i < 1000; ++i) {
        c.save({ _key: "test_" + i });
      }

      waitForTreeReady(c);
    },

    tearDown: function () {
      db._drop(colName1);
    },

    testRevisionTreeCorrupt: function() {
      const c1 = db._collection(colName1);
      assertEqual(c1._revisionTreeSummary().count, c1.count());
      assertEqual(c1._revisionTreeSummary().count, 1000);
      let trees = c1._revisionTreeVerification();
      assertTrue(trees.equal);

      // Not let's corrupt the tree:
      c1._revisionTreeCorrupt(17, 17);
      let summary = c1._revisionTreeSummary();
      assertEqual(summary.count, 17);
      assertEqual(summary.hash, 17);
      internal.debugSetFailAt("MerkleTree::skipConsistencyCheck");
      trees = c1._revisionTreeVerification();
      assertFalse(trees.equal);
      internal.debugClearFailAt();

      // And repair it again:
      c1._revisionTreeRebuild();
      summary = c1._revisionTreeSummary();
      assertNotEqual(summary.count, 17);
      assertNotEqual(summary.hash, 17);
      trees = c1._revisionTreeVerification();
      assertTrue(trees.equal);
    },

  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(corruptRepairSuite);
}

return jsunity.done();
