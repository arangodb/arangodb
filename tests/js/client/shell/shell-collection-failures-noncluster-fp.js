/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNull, fail, assertFalse, arango */

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
/// @author Michael Hackstein
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require("internal");
const ERRORS = arangodb.errors;
const waitForEstimatorSync = require('@arangodb/test-helper').waitForEstimatorSync;
let IM = global.instanceManager;

function DocumentOperationsFailuresSuite() {
  'use strict';
  const cn = "UnitTestsDocuments";

  return {

    setUp: function () {
      IM.debugClearFailAt();
    },
    
    tearDown: function () {
      IM.debugClearFailAt();
      db._drop(cn);
    },
    
    testInsertSizeLimit: function () {
      let c = db._create(cn);

      IM.debugSetFailAt("addOperationSizeError");

      try {
        c.insert({ _key: "testi" });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_RESOURCE_LIMIT.code);
      }

      assertEqual(0, c.count());
    
      IM.debugClearFailAt();

      c.insert({ _key: "testi" });
      assertEqual(1, c.count());
    },

    testInsertFailure1: function () {
      let c = db._create(cn);

      IM.debugSetFailAt("RocksDBCollection::insertFail1Always");

      try {
        c.insert({ _key: "testi" });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(0, c.count());
    
      IM.debugClearFailAt();

      c.insert({ _key: "testi" });
      assertEqual(1, c.count());
    },
    
    testInsertFailure2: function () {
      let c = db._create(cn);

      IM.debugSetFailAt("RocksDBCollection::insertFail2Always");

      try {
        c.insert({ _key: "testi" });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(0, c.count());
    
      IM.debugClearFailAt();

      c.insert({ _key: "testi" });
      assertEqual(1, c.count());
    },
    
    testRemoveSizeLimit: function () {
      let c = db._create(cn);
      c.insert({ _key: "testi" });

      IM.debugSetFailAt("addOperationSizeError");

      try {
        c.remove("testi");
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_RESOURCE_LIMIT.code);
      }

      assertEqual(1, c.count());
    
      IM.debugClearFailAt();

      c.remove("testi");
      assertEqual(0, c.count());
    },
    
    testRemoveFailure1: function () {
      let c = db._create(cn);
      c.insert({ _key: "testi" });

      IM.debugSetFailAt("RocksDBCollection::removeFail1Always");

      try {
        c.remove("testi");
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(1, c.count());
    
      IM.debugClearFailAt();

      c.remove("testi");
      assertEqual(0, c.count());
    },
    
    testRemoveFailure2: function () {
      let c = db._create(cn);
      c.insert({ _key: "testi" });

      IM.debugSetFailAt("RocksDBCollection::removeFail2Always");

      try {
        c.remove("testi");
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(1, c.count());
    
      IM.debugClearFailAt();

      c.remove("testi");
      assertEqual(0, c.count());
    },
    
    testModifySizeLimit: function () {
      let c = db._create(cn);
      c.insert({ _key: "testi", value: 1 });

      IM.debugSetFailAt("addOperationSizeError");

      try {
        c.update("testi", { value: 2 });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_RESOURCE_LIMIT.code);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("testi").value);
    
      IM.debugClearFailAt();

      c.update("testi", { value: 3 });
      assertEqual(1, c.count());
      assertEqual(3, c.document("testi").value);
    },
    
    testModifyFailure1: function () {
      let c = db._create(cn);
      c.insert({ _key: "testi", value: 1 });

      IM.debugSetFailAt("RocksDBCollection::modifyFail1Always");

      try {
        c.update("testi", { value: 2 });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("testi").value);
    
      IM.debugClearFailAt();

      c.update("testi", { value: 3 });
      assertEqual(1, c.count());
      assertEqual(3, c.document("testi").value);
    },
    
    testModifyFailure2: function () {
      let c = db._create(cn);
      c.insert({ _key: "testi", value: 1 });

      IM.debugSetFailAt("RocksDBCollection::modifyFail3Always");

      try {
        c.update("testi", { value: 2 });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("testi").value);
    
      IM.debugClearFailAt();

      c.update("testi", { value: 3 });
      assertEqual(1, c.count());
      assertEqual(3, c.document("testi").value);
    },
    
    testModifyFailure3: function () {
      let c = db._create(cn);
      c.insert({ _key: "testi", value: 1 });

      IM.debugSetFailAt("RocksDBCollection::modifyFail2Always");

      try {
        c.update("testi", { value: 2 });
        fail();
      } catch (e) {
        // Validate that we died with debug
        assertEqual(e.errorNum, ERRORS.ERROR_DEBUG.code);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("testi").value);
    
      IM.debugClearFailAt();

      c.update("testi", { value: 3 });
      assertEqual(1, c.count());
      assertEqual(3, c.document("testi").value);
    },
  };
}


function CollectionTruncateFailuresSuite() {
  'use strict';
  const cn = "UnitTestsTruncate";
  let c;
  const cleanUp = () => {
    IM.debugClearFailAt();
    try {
      db._drop(cn);
    } catch(_) { }
  };

  const docs = [];
  for (let i = 0; i < 10000; ++i) {
    docs.push({value: i % 250, value2: i % 100});
  }

  return {

    setUp: function () {
      cleanUp();
      c = db._create(cn);
      c.ensureIndex({ type: "hash", fields: ["value"] });
      c.ensureIndex({ type: "skiplist", fields: ["value2"] });

      // Add two packs of 10.000 Documents.
      // Intermediate commits will commit after 10.000 removals
      c.insert(docs);
      c.insert(docs);
    },
    
    tearDown: cleanUp,

    testTruncateFailsAfterAllCommits: function () {
      IM.debugSetFailAt("FailAfterAllCommits");
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
        waitForEstimatorSync();  // make sure estimates are consistent
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
      IM.debugSetFailAt("FailBeforeIntermediateCommit");
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
        waitForEstimatorSync();  // make sure estimates are consistent
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
      IM.debugSetFailAt("FailAfterIntermediateCommit");
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
        waitForEstimatorSync();  // make sure estimates are consistent
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
    IM.debugClearFailAt();
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
      IM.debugSetFailAt("FailBeforeIntermediateCommit");
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
      IM.debugSetFailAt("FailBeforeIntermediateCommit");
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
      IM.debugSetFailAt("FailBeforeIntermediateCommit");
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
      IM.debugSetFailAt("FailBeforeIntermediateCommit");
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
    
jsunity.run(DocumentOperationsFailuresSuite);
jsunity.run(CollectionTruncateFailuresSuite);
jsunity.run(IntermediateCommitFailureSuite);
return jsunity.done();
