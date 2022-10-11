/* jshint esnext: true */
/* global assertTrue, assertEqual, assertNotEqual, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / @author Julia Puget
// / @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const internal = require('internal');
const db = internal.db;
const fs = require('fs');

function StorageForQueryWithCollectionSortSuite() {
  const cn = 'UnitTestCollection';

  let assertResult = (order, res, expectedCount) => {
    assertTrue(res.hasNext());
    let firstVal = res.next().value1;
    let count = 1;
    while (res.hasNext()) {
      ++count;
      let el = res.next();
      assertTrue(order === "asc" ? el.value1 >= firstVal : el.value1 <= firstVal, {order, el, firstVal});
      firstVal = el.value1;
    }
    assertEqual(count, expectedCount);
  };

  return {

    setUpAll: function() {
      db._drop(cn);
      let c = db._create(cn);

      let docs = [];
      for (let i = 0; i < 500000; ++i) {
        docs.push({value1: i});
        if (docs.length === 10000) {
          c.insert(docs);
          docs = [];
        }
      }
    },

    tearDownAll: function() {
      db._drop(cn);
    },

    testSortComparePeakMemoryUsageAscCollection: function() {
      let query = `FOR doc IN ${cn} SORT doc.value1 ASC RETURN doc`;
      let res = db._query(query, null, {spillOverThresholdNumRows: 100000000, stream: true});
      assertResult("asc", res, 500000);
      let queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      const memoryUsage1 = queryStats.peakMemoryUsage;

      res = db._query(query, null, {spillOverThresholdNumRows: 5000, stream: true});
      assertResult("asc", res, 500000);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1, {
        current: queryStats.peakMemoryUsage,
        previous: memoryUsage1
      });

      res = db._query(query, null, {spillOverThresholdMemoryUsage: 5000, stream: true});
      assertResult("asc", res, 500000);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1, {
        current: queryStats.peakMemoryUsage,
        previous: memoryUsage1
      });
    },

    testSortComparePeakMemoryUsageDescCollection: function() {
      let query = `FOR doc IN ${cn} SORT doc.value1 DESC RETURN doc`;
      let res = db._query(query, null, {spillOverThresholdNumRows: 100000000, stream: true});
      assertResult("desc", res, 500000);
      let queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      const memoryUsage1 = queryStats.peakMemoryUsage;

      res = db._query(query, null, {spillOverThresholdNumRows: 5000, stream: true});
      assertResult("desc", res, 500000);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1, {
        current: queryStats.peakMemoryUsage,
        previous: memoryUsage1
      });

      res = db._query(query, null, {spillOverThresholdMemoryUsage: 5000, stream: true});
      assertResult("desc", res, 500000);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1, {
        current: queryStats.peakMemoryUsage,
        previous: memoryUsage1
      });
    },

  };
}

function StorageForQueryWithoutCollectionSortSuite() {

  let assertOrder = (order, values) => {
    let firstVal = values[0];
    values.forEach(el => {
      assertTrue(order === "asc" ? el >= firstVal : el <= firstVal, {order, el, firstVal});
      firstVal = el;
    });
  };

  return {

    testSortComparePeakMemoryUsageAscRange: function() {
      let query = `FOR i IN 1..500000 SORT i ASC RETURN i`;
      let res = db._query(query, null, {spillOverThresholdNumRows: 100000000});
      let allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("asc", allDocs);
      let queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      const memoryUsage1 = queryStats.peakMemoryUsage;

      res = db._query(query, null, {spillOverThresholdNumRows: 5000});
      allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("asc", allDocs);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1);

      res = db._query(query, null, {spillOverThresholdMemoryUsage: 5000});
      allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("asc", allDocs);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1);
    },

    testSortComparePeakMemoryUsageDescRange: function() {
      let query = `FOR i IN 1..500000 SORT i DESC RETURN i`;
      let res = db._query(query, null, {spillOverThresholdNumRows: 100000000});
      let allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("desc", allDocs);
      let queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      const memoryUsage1 = queryStats.peakMemoryUsage;

      res = db._query(query, null, {spillOverThresholdNumRows: 5000});
      allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("desc", allDocs);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1);

      res = db._query(query, null, {spillOverThresholdMemoryUsage: 5000});
      allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("desc", allDocs);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1);
    },

  };
}

function StorageForQueryCleanUpWhenFailureSuite() {
  const query = `FOR i IN 1..500000 SORT i ASC RETURN i`;
  const tempDir = fs.join(internal.options()["temp.intermediate-results-path"], "temp");

  let assertRangeCleanUp = (remainderSstFileName, fileNameData) => {
    const tree = fs.listTree(tempDir);
    if (remainderSstFileName !== "") {
      assertEqual(tree.length, 2);
      assertNotEqual(-1, tree.indexOf(remainderSstFileName));
      let data = fs.readFileSync(fs.join(tempDir, remainderSstFileName));
      assertEqual(fileNameData, data.toString());
    } else {
      assertEqual(tree.length, 1);
    }
  };

  return {

    tearDown: function() {
      const tree = fs.listTree(tempDir);
      tree.forEach(fileName => {
        if (fileName !== "") {
          fs.remove(fs.join(tempDir, fileName));
        }
      });
    },
    
    testQueryFailureBecauseCapacityReached: function() {
      internal.debugSetFailAt("lowTempStorageCapacity");
      try {
        const query = `FOR i IN 1..5000000 SORT i ASC RETURN i`;
        db._query(query, null, {spillOverThresholdNumRows: 5000});
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        assertRangeCleanUp("");
      } finally {
        internal.debugClearFailAt();
      }
    },

    testQueryFailureIngestAll1NoExtraSst: function() {
      internal.debugSetFailAt("failOnIngestAll1");
      try {
        db._query(query, null, {spillOverThresholdNumRows: 5000});
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        assertRangeCleanUp("");
      } finally {
        internal.debugClearFailAt();
      }
    },

    testQueryFailureIngestAll1ExtraSst: function() {
      const tempFileName = "1111111111111111111111111111111111111111111111111111111111111111111222222222222222222222222222222222222222222222233333333333333333333.sst";
      const tempFileNameData = "123456789123456789123456789";
      fs.writeFileSync(fs.join(tempDir, tempFileName), tempFileNameData);
      internal.debugSetFailAt("failOnIngestAll1");
      try {
        db._query(query, null, {spillOverThresholdNumRows: 5000});
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
        assertRangeCleanUp(tempFileName, tempFileNameData);
      } finally {
        internal.debugClearFailAt();
      }
    },

    testQueryFailureIngestAll2NoExtraSst: function() {
      internal.debugSetFailAt("failOnIngestAll2");
      try {
        db._query(query, null, {spillOverThresholdNumRows: 5000});
        fail();
      } catch (err) {
        assertEqual(err.errorNum, 1100); //this failure point doesn't throw exception, so it has rocksdb corruption code
        assertRangeCleanUp("");
      } finally {
        internal.debugClearFailAt();
      }
    },

    testQueryFailureIngestAll2ExtraSst: function() {
      const tempFileName = "1111111111111111111111111111111111111111111111111111111111111111111222222222222222222222222222222222222222222222233333333333333333333.sst";
      const tempFileNameData = "123456789123456789123456789";
      fs.writeFileSync(fs.join(tempDir, tempFileName), tempFileNameData);
      internal.debugSetFailAt("failOnIngestAll2");
      try {
        db._query(query, null, {spillOverThresholdNumRows: 5000});
        fail();
      } catch (err) {
        assertEqual(err.errorNum, 1100); //this failure point doesn't throw exception, so it has rocksdb corruption code
        assertRangeCleanUp(tempFileName, tempFileNameData);
      } finally {
        internal.debugClearFailAt();

      }
    },

  };
}

jsunity.run(StorageForQueryWithCollectionSortSuite);
jsunity.run(StorageForQueryWithoutCollectionSortSuite);
if (internal.debugCanUseFailAt()) {
  jsunity.run(StorageForQueryCleanUpWhenFailureSuite);
}
return jsunity.done();
