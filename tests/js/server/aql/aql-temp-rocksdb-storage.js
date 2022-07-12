/* jshint esnext: true */
/* global AQL_EXECUTE, assertTrue, assertEqual */

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
const errors = require('@arangodb').errors;


function StorageForQueryWithCollectionSortSuite() {
  const cn = 'UnitTestCollection';

  let assertResult = (order, res, expectedCount) => {
    assertTrue(res.hasNext());
    let firstVal = res.next().value1;
    let count = 1;
    while (res.hasNext()) {
      ++count;
      let el = res.next();
      assertTrue(order === "asc" ? el.value1 >= firstVal : el.value1 <= firstVal, { order, el, firstVal });
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
      let res = db._query(query, null, {thresholdNumRows: 100000000, stream: true });
      assertResult("asc", res, 500000);
      let queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      const memoryUsage1 = queryStats.peakMemoryUsage;
      
      res = db._query(query, null, {thresholdNumRows: 10000, stream: true });
      assertResult("asc", res, 500000);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1, { current: queryStats.peakMemoryUsage, previous: memoryUsage1 });
    },

    testSortComparePeakMemoryUsageDescCollection: function() {
      let query = `FOR doc IN ${cn} SORT doc.value1 DESC RETURN doc`;
      let res = db._query(query, null, {thresholdNumRows: 100000000, stream: true });
      assertResult("desc", res, 500000);
      let queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      const memoryUsage1 = queryStats.peakMemoryUsage;
      
      res = db._query(query, null, {thresholdNumRows: 5000, stream: true });
      assertResult("desc", res, 500000);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1, { current: queryStats.peakMemoryUsage, previous: memoryUsage1 });
    },
    
  };
}

function StorageForQueryWithoutCollectionSortSuite() {
  const cn = 'UnitTestCollection';

  let assertOrder = (order, values) => {
    let firstVal = values[0];
    values.forEach(el => {
      assertTrue(order === "asc" ? el >= firstVal : el <= firstVal, { order, el, firstVal });
      firstVal = el;
    });
  };

  return {

    testSortComparePeakMemoryUsageAscRange: function() {
      let query = `FOR i IN 1..500000 SORT i ASC RETURN i`;
      let res = db._query(query, null, {thresholdNumRows: 100000000});
      let allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("asc", allDocs);
      let queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      const memoryUsage1 = queryStats.peakMemoryUsage;
      
      res = db._query(query, null, {thresholdNumRows: 10000});
      allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("asc", allDocs);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1);

    },

    testSortComparePeakMemoryUsageDescRange: function() {
      let query = `FOR i IN 1..500000 SORT i DESC RETURN i`;
      let res = db._query(query, null, {thresholdNumRows: 100000000});
      let allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("desc", allDocs);
      let queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      const memoryUsage1 = queryStats.peakMemoryUsage;
      
      res = db._query(query, null, {thresholdNumRows: 5000});
      allDocs = res.toArray();
      assertEqual(allDocs.length, 500000);
      assertOrder("desc", allDocs);
      queryStats = res.getExtra().stats;
      assertTrue(queryStats.hasOwnProperty("peakMemoryUsage"));
      assertTrue(queryStats.peakMemoryUsage * 2 < memoryUsage1);
    },

  };
}

jsunity.run(StorageForQueryWithCollectionSortSuite);
jsunity.run(StorageForQueryWithoutCollectionSortSuite);
return jsunity.done();
