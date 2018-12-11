/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertFalse, assertTrue, fail */
// //////////////////////////////////////////////////////////////////////////////
// / @brief tests for truncate on rocksdb with intermediate commits & failures
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
const colName = "UnitTestsRecovery";

const runSetup = function () {
  internal.debugClearFailAt();

  db._drop(colName);
  const c = db._create(colName);
  c.ensureHashIndex("value");
  c.ensureSkiplist("value2");

  const docs = [];
  for (let i = 0; i < 10000; ++i) {
    docs.push({value: i % 250, value2: i % 100});
  }
  // Add two packs of 10.000 Documents.
  // Intermediate commits will commit after 10.000 removals
  c.save(docs);
  c.save(docs);

  internal.debugSetFailAt("SegfaultAfterIntermediateCommit");

  // This will crash the server
  c.truncate();

  fail();
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

const recoverySuite = function () {
  jsunity.jsUnity.attachAssertions();

  const c = db._collection(colName);
  const docsWithEqHash = 20000 / 250;
  const docsWithEqSkip = 20000 / 100;

  return {
    setUp: function () {},
    tearDown: function () {},

    // Test that count of collection remains unmodified.
    // We crashed after one remove commit. But before the other
    testCollectionCount: () => {
      assertEqual(c.count(), 10000);
    },

    // Test that the HashIndex remains intact.
    testHashIndex: () => {
      let sum = 0;
      let q = `FOR x IN @@c FILTER x.value == @i RETURN x`;
      for (let i = 0; i < 250; ++i) {
        // This validates that all documents can be found again
        let res = db._query(q, {"@c": colName, i: i}).toArray();
        let c = res.length;
        assertTrue(c <  docsWithEqHash);
        sum += c;
      }
      assertEqual(sum, 10000);

      // just validate that no other values are inserted.
      let res2 = db._query(q, {"@c": colName, i: 251}).toArray();
      assertEqual(res2.length, 0);
    },

    // Test that the SkiplistIndex remains intact.
    testSkiplistIndex: () => {
      let sum = 0;
      let q = `FOR x IN @@c FILTER x.value2 == @i RETURN x`;
      for (let i = 0; i < 100; ++i) {
        // This validates that all documents can be found again
        let res = db._query(q, {"@c": colName, i: i}).toArray();
        let c = res.length;
        assertTrue(c <  docsWithEqSkip);
        sum += c;
      }
      assertEqual(sum, 10000);

      // just validate that no other values are inserted.
      let res2 = db._query(q, {"@c": colName, i: 101}).toArray();
      assertEqual(res2.length, 0);
    },

    testSelectivityEstimates: () => {
      internal.waitForEstimatorSync(); // make sure estimates are consistent
      let indexes = c.getIndexes(true);
      for (let i of indexes) {
        switch (i.type) {
          case 'primary':
            assertEqual(i.selectivityEstimate, 1);
            break;
          case 'hash':
            assertEqual(i.selectivityEstimate, 0.025);
            break;
          case 'skiplist':
            assertEqual(i.selectivityEstimate, 0.01);
            break;
          default:
            fail();
        }
      }
    }
  };

};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

function main (argv) {
  'use strict';

  if (internal.debugCanUseFailAt()) {
    if (argv[1] === 'setup') {
      runSetup();
      return 0;
    } else {
      jsunity.run(recoverySuite);
      return jsunity.writeDone().status ? 0 : 1;
    }
  } else {
    return jsunity.writeDone().status ? 0 : 1;
  }
}
