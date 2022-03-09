/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue, assertNotEqual, arango */

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
const primaryEndpoint = arango.getEndpoint();
const helper = require("@arangodb/test-helper");

function waitForTreeReady(c) {
  while (true) {
    let name = c.name();
    let pending = db._connection.POST("/_admin/execute?returnAsJSON=true",
      `return require("internal").db._collection("${name}")._revisionTreePendingUpdates();`);
    // For whatever reason, sometimes we get an empty object back, let's
    // skip that.
    if (pending.hasOwnProperty("inserts")) {
      if (pending.inserts === 0 &&
          pending.removes === 0 &&
          pending.truncates === 0) {
        return;
      }
    }
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
      let trees = c1._revisionTreeVerification();
      assertTrue(trees.equal);

      // Not let's corrupt the tree:
      db._connection.POST("/_admin/execute?returnAsJSON=true",
        `require("internal").db._collection("${colName1}")._revisionTreeCorrupt(17, 17); return true;`);
      helper.debugSetFailAt(primaryEndpoint, "MerkleTree::skipConsistencyCheck");
      trees = c1._revisionTreeVerification();
      assertFalse(trees.equal);
      helper.debugClearFailAt(primaryEndpoint);

      // And repair it again:
      db._connection.POST("/_admin/execute?returnAsJSON=true",
        `require("internal").db._collection("${colName1}")._revisionTreeRebuild(); return true;`);
      trees = c1._revisionTreeVerification();
      assertTrue(trees.equal);
    },

  };
}

if (helper.debugCanUseFailAt(primaryEndpoint)) {
  jsunity.run(corruptRepairSuite);
}

return jsunity.done();
