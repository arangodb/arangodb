/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNull, fail, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
const internal = require("internal");
var ERRORS = arangodb.errors;

function CollectionTruncateFailuresSuite() {
  'use strict';
  const cn = "UnitTestsTruncate";
  let c;
  const cleanUp = () => {
    internal.debugClearFailAt();
    try {
      db._drop(cn);
    } catch(_) { }
  };

  const docs = [];
  for (let i = 0; i < 10000; ++i) {
    docs.push({value: i % 250, value2: i % 100});
  }

  return {

    tearDown: cleanUp,

    setUp: function () {
      cleanUp();
      c = db._create(cn);
      c.ensureHashIndex("value");
      c.ensureSkiplist("value2");

      // Add two packs of 10.000 Documents.
      // Intermediate commits will commit after 10.000 removals
      c.save(docs);
      c.save(docs);
    },

    testTruncateFailsAfterAllCommits: function () {
      internal.debugSetFailAt("FailAfterAllCommits");
      try {
        c.truncate({ compact: false });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      // All docments should be removed through intermediate commits.
      // We have two packs that fill up those commits.
      // Now validate that we endup with an empty collection.
      assertEqual(c.count(), 0);

      // Test Primary
      {
        let q = `FOR x IN @@c RETURN x._key`;
        let res = db._query(q, {"@c": cn}).toArray();
        assertEqual(res.length, 0);
      }

      // Test Hash
      {
        let q = `FOR x IN @@c FILTER x.value == @i RETURN x`;
        for (let i = 0; i < 250; ++i) {
          // This validates that all documents can be found again
          let res = db._query(q, {"@c": cn, i: i}).toArray();
          assertEqual(res.length, 0);
        }

        // just validate that no other values are inserted.
        let res2 = db._query(q, {"@c": cn, i: 251}).toArray();
        assertEqual(res2.length, 0);
      }

      // Test Skiplist
      {
        let q = `FOR x IN @@c FILTER x.value2 == @i RETURN x`;
        for (let i = 0; i < 100; ++i) {
          // This validates that all documents can be found again
          let res = db._query(q, {"@c": cn, i: i}).toArray();
          assertEqual(res.length, 0);
        }

        // just validate that no other values are inserted.
        let res2 = db._query(q, {"@c": cn, i: 101}).toArray();
        assertEqual(res2.length, 0);
      }

      // Test Selectivity Estimates
      {
        internal.waitForEstimatorSync(); // make sure estimates are consistent
        let indexes = c.getIndexes(true);
        for (let i of indexes) {
          switch (i.type) {
            case 'primary':
              assertEqual(i.selectivityEstimate, 1);
              break;
            case 'hash':
              assertEqual(i.selectivityEstimate, 1);
              break;
            case 'skiplist':
              assertEqual(i.selectivityEstimate, 1);
              break;
            default:
              fail();
          }
        }
      }
    },

    testTruncateFailsBeforeCommit: function () {
      const docsWithEqHash = 20000 / 250;
      const docsWithEqSkip = 20000 / 100;
      internal.debugSetFailAt("FailBeforeIntermediateCommit");
      try {
        c.truncate();
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      // all commits failed, no documents removed
      assertEqual(c.count(), 20000);

      // Test Primary
      {
        let q = `FOR x IN @@c RETURN x._key`;
        let res = db._query(q, {"@c": cn}).toArray();
        assertEqual(res.length, 20000);
      }

      // Test Hash
      {
        let q = `FOR x IN @@c FILTER x.value == @i RETURN x`;
        for (let i = 0; i < 250; ++i) {
          // This validates that all documents can be found again
          let res = db._query(q, {"@c": cn, i: i}).toArray();
          assertEqual(res.length, docsWithEqHash);
        }

        // just validate that no other values are inserted.
        let res2 = db._query(q, {"@c": cn, i: 251}).toArray();
        assertEqual(res2.length, 0);
      }

      // Test Skiplist
      {
        let q = `FOR x IN @@c FILTER x.value2 == @i RETURN x`;
        for (let i = 0; i < 100; ++i) {
          // This validates that all documents can be found again
          let res = db._query(q, {"@c": cn, i: i}).toArray();
          assertEqual(res.length, docsWithEqSkip);
        }

        // just validate that no other values are inserted.
        let res2 = db._query(q, {"@c": cn, i: 101}).toArray();
        assertEqual(res2.length, 0);
      }

      // Test Selectivity Estimates
      {
        internal.waitForEstimatorSync(); // make sure estimates are consistent
        let indexes = c.getIndexes(true);
        for (let i of indexes) {
          switch (i.type) {
            case 'primary':
              assertEqual(i.selectivityEstimate, 1);
              break;
            case 'hash':
              assertEqual(i.selectivityEstimate, 0.0125);
              break;
            case 'skiplist':
              assertEqual(i.selectivityEstimate, 0.005);
              break;
              default:
              fail();
          }
        }
      }

    },

    testTruncateFailsBetweenCommits: function () {
      internal.debugSetFailAt("FailAfterIntermediateCommit");
      const docsWithEqHash = 20000 / 250;
      const docsWithEqSkip = 20000 / 100;

      try {
        c.truncate();
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      // At 10k removals a intermediate commit happens, then a fail
      assertEqual(c.count(), 10000);

      // Test Primary
      {
        let q = `FOR x IN @@c RETURN x._key`;
        let res = db._query(q, {"@c": cn}).toArray();
        assertEqual(res.length, 10000);
      }

      // Test Hash
      {
        let sum = 0;
        let q = `FOR x IN @@c FILTER x.value == @i RETURN x`;
        for (let i = 0; i < 250; ++i) {
          // This validates that all documents can be found again
          let res = db._query(q, {"@c": cn, i: i}).toArray();
          assertTrue(res.length < docsWithEqHash);
          sum += res.length;
        }
        assertEqual(sum, 10000);

        // just validate that no other values are inserted.
        let res2 = db._query(q, {"@c": cn, i: 251}).toArray();
        assertEqual(res2.length, 0);
      }

      // Test Skiplist
      {
        let q = `FOR x IN @@c FILTER x.value2 == @i RETURN x`;
        let sum = 0;
        for (let i = 0; i < 100; ++i) {
          // This validates that all documents can be found again
          let res = db._query(q, {"@c": cn, i: i}).toArray();
          assertTrue(res.length <  docsWithEqSkip);
          sum += res.length;
        }
        assertEqual(sum, 10000);

        // just validate that no other values are inserted.
        let res2 = db._query(q, {"@c": cn, i: 101}).toArray();
        assertEqual(res2.length, 0);
      }

      // Test Selectivity Estimates
      // This may be fuzzy...
      {
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
    },

  };
}

function IntermediateCommitFailureSuite() {
  'use strict';
  const cn = "UnitTestsIntermediate";
  let c;
  const cleanUp = () => {
    internal.debugClearFailAt();
    try {
      db._drop(cn);
    } catch(_) { }
  };

  const docs = [];
  for (let i = 0; i < 10000; ++i) {
    docs.push({value: i % 250, value2: i % 100});
  }

  return {

    tearDown: cleanUp,

    setUp: function () {
      cleanUp();
      c = db._create(cn);
      c.insert(docs);
    },

    testFailOnRemoveAql: function () {
      internal.debugSetFailAt("FailBeforeIntermediateCommit");
      try {
        db._query("FOR doc IN @@cn REMOVE doc IN @@cn", { "@cn" : cn }, { intermediateCommitCount: 10000 });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(c.count(), 10000);
      assertEqual(c.toArray().length, 10000);
    },
    
    testFailOnUpdateAql: function () {
      internal.debugSetFailAt("FailBeforeIntermediateCommit");
      try {
        db._query("FOR doc IN @@cn UPDATE doc WITH { aha: 1 } IN @@cn", { "@cn" : cn }, { intermediateCommitCount: 10000 });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(c.count(), 10000);
      assertEqual(c.toArray().length, 10000);
 
      assertNull(c.firstExample({ aha: 1 }));
    },
    
    testFailOnReplaceAql: function () {
      internal.debugSetFailAt("FailBeforeIntermediateCommit");
      try {
        db._query("FOR doc IN @@cn REPLACE doc WITH { aha: 1 } IN @@cn", { "@cn" : cn }, { intermediateCommitCount: 10000 });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(c.count(), 10000);
      assertEqual(c.toArray().length, 10000);
 
      assertNull(c.firstExample({ aha: 1 }));
    },
    
    testFailOnInsertAql: function () {
      internal.debugSetFailAt("FailBeforeIntermediateCommit");
      try {
        db._query("FOR i IN 1..10000 INSERT {} IN @@cn", { "@cn" : cn }, { intermediateCommitCount: 10000 });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(c.count(), 10000);
      assertEqual(c.toArray().length, 10000);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (internal.debugCanUseFailAt()) {
  jsunity.run(CollectionTruncateFailuresSuite);
  jsunity.run(IntermediateCommitFailureSuite);
}

return jsunity.done();
