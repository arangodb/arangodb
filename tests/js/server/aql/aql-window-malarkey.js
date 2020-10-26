/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, fail, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const errors = internal.errors;

function WindowMalarkeyTestSuite() {

  const cname1 = "WindowTestCollection1";
  const cname2 = "WindowTestCollection2";
  let c1 = null, c2 = null;
  return {
    setUp: function () {
      c1 = db._create(cname1, { numberOfShards: 4 });
      c2 = db._create(cname2, { numberOfShards: 4 });

      let docs = [];
      for (let i = 0; i < 3000; i++) {
        docs.push({ value: i });
      }
      c1.insert(docs);
    },

    tearDown: function () {
      db._drop(cname1);
      db._drop(cname2);
    },

    testResultsInsertAfterRow: function () {
      const q = `
      FOR doc IN @@input 
        SORT doc.value ASC
        WINDOW {preceding: 1, following: 1} AGGREGATE rolling_sum = SUM(doc.value), av = AVG(doc.value), l = LENGTH(doc)
        INSERT {value: doc.value, sum: rolling_sum, avg: av, len: l} into @@output
      `;
      db._query(q, { '@input': cname1, '@output': cname2 });
      assertEqual(c1.count(), c2.count());
      let count = c1.count();
      c2.toArray().forEach(doc => {
        let sum = Math.max(0, doc.value - 1) + doc.value + (doc.value < count - 1 ? doc.value + 1 : 0);
        assertEqual(doc.sum, sum, doc);
        assertEqual(doc.avg, doc.sum / doc.len, doc);
      });
    },

    testResultsInsertAfterRange: function () {
      const q = `
      FOR doc IN @@input 
        WINDOW doc.value WITH {preceding: 1, following: 1} AGGREGATE rolling_sum = SUM(doc.value), av = AVG(doc.value), l = LENGTH(doc)
        INSERT {value: doc.value, sum: rolling_sum, avg: av, len: l} into @@output
      `;
      db._query(q, { '@input': cname1, '@output': cname2 });
      assertEqual(c1.count(), c2.count());
      let count = c1.count();
      c2.toArray().forEach(doc => {
        let sum = Math.max(0, doc.value - 1) + doc.value + (doc.value < count - 1 ? doc.value + 1 : 0);
        assertEqual(doc.sum, sum);
        assertEqual(doc.avg, doc.sum / doc.len, doc);
      });
    },

    testResultsUpdateAfterRow: function () {
      const q = `
      FOR doc IN @@cc 
        SORT doc.value ASC
        WINDOW doc.value WITH {preceding: 1, following: 1} AGGREGATE rolling_sum = SUM(doc.value), av = AVG(doc.value), l = LENGTH(doc)
        UPDATE doc WITH {value: doc.value, sum: rolling_sum, avg: av, len: l} into @@cc
      `;
      db._query(q, { '@cc': cname1 });
      let count = c1.count();
      c1.toArray().forEach(doc => {
        let sum = Math.max(0, doc.value - 1) + doc.value + (doc.value < count - 1 ? doc.value + 1 : 0);
        assertEqual(doc.sum, sum);
        assertEqual(doc.avg, doc.sum / doc.len, doc);
      });
    },

    testResultsUpdateAfterRange: function () {
      const q = `
      FOR doc IN @@cc 
        WINDOW doc.value WITH {preceding: 1, following: 1} AGGREGATE rolling_sum = SUM(doc.value), av = AVG(doc.value), l = LENGTH(doc)
        UPDATE doc WITH {value: doc.value, sum: rolling_sum, avg: av, len: l} into @@cc
      `;
      db._query(q, { '@cc': cname1 });
      let count = c1.count();
      c1.toArray().forEach(doc => {
        let sum = Math.max(0, doc.value - 1) + doc.value + (doc.value < count - 1 ? doc.value + 1 : 0);
        assertEqual(doc.sum, sum);
        assertEqual(doc.avg, doc.sum / doc.len, doc);
      });
    },

    testResultsInsertBefore: function () {
      const q = `
      FOR doc IN @@input 
        INSERT {value: doc.value} into @@output
        WINDOW {preceding: 1, following: 1} AGGREGATE av = AVG(doc.value)
        RETURN av 
      `;
      let didThrow = false;
      try {
        db._query(q, { '@input': cname1, '@output': cname2 });
      } catch (err) {
        didThrow = true;
        assertEqual(errors.ERROR_QUERY_WINDOW_AFTER_MODIFICATION.code, err.errorNum);
      }
      assertTrue(didThrow);
    },

    testResultsUpdateBefore: function () {
      const q = `
      FOR doc IN @@cc 
        UPDATE doc WITH {value2: doc.value + 1} into @@cc
        WINDOW {preceding: 1, following: 1} AGGREGATE av = AVG(doc.value)
        RETURN av 
      `;
      let didThrow = false;
      try {
        db._query(q, { '@cc': cname1 });
      } catch (err) {
        didThrow = true;
        assertEqual(errors.ERROR_QUERY_WINDOW_AFTER_MODIFICATION.code, err.errorNum);
      }
      assertTrue(didThrow);
    },

    testResultsReplaceBefore: function () {
      const q = `
      FOR doc IN @@cc 
        REPLACE doc WITH {value: doc.value + 1} into @@cc
        WINDOW {preceding: 1, following: 1} AGGREGATE av = AVG(doc.value)
        RETURN av 
      `;
      let didThrow = false;
      try {
        db._query(q, { '@cc': cname1 });
      } catch (err) {
        didThrow = true;
        assertEqual(errors.ERROR_QUERY_WINDOW_AFTER_MODIFICATION.code, err.errorNum);
      }
      assertTrue(didThrow);
    },
  };
}

function WindowDateRangeTestSuite() {

  const cname = "WindowTestCollection1";
  let c = null;
  const keyPref = 'row-';

  const q = `
  FOR doc IN @@cc 
    WINDOW doc.time WITH {preceding: @preceding, following: @following} 
    AGGREGATE minTime = MIN(doc.time), maxTime = MAX(doc.time), 
              minIdx = MIN(doc.idx), maxIdx = MAX(doc.idx),
              l = LENGTH(doc)
    RETURN { minTime: minTime, maxTime: maxTime, 
             minIdx: minIdx, maxIdx: maxIdx,
             length: l, time: doc.time, idx: doc.idx} 
  `;

  function runDurationTest() {
    let intervals = [["P1D", "P2D"],
    ["P2M15DT6H30.33S", "P1M3WT10H"],
    ["P4M3D", "P1Y1M3D"],
    ["P1Y3M2D", "P1M"],
    ["P1Y1M3D", "P1M3DT2H30M20S"],
    ["P2M15D", "P1Y1MT10M"],
    ["P2M15D", "P1Y1MT10M"]];
    
    intervals.forEach(pair => {
      print("testing ", pair);

      let cursor = db._query(q, { '@cc': cname, preceding: pair[0], following: pair[1] });
      assertEqual(cursor.count(), c.count());
      while (cursor.hasNext()) {
        let row = cursor.next();

        // considered rows are all within the specified duration
        let l = db._query("RETURN DATE_DIFF(DATE_SUBTRACT(@row, @duration), @other, 'f')",
          { row: row.time, duration: pair[0], other: row.minTime }).toArray()[0];

        assertTrue(l >= 0);

        l = db._query("RETURN DATE_DIFF(@other, DATE_ADD(@row, @duration), 'f')",
          { row: row.time, duration: pair[1], other: row.maxTime }).toArray()[0];

        assertTrue(l >= 0);

        let diffHours = db._query("RETURN DATE_DIFF(DATE_SUBTRACT(@row, @preceding), DATE_ADD(@row, @following), 'h')",
          { row: row.time, preceding: pair[0], following: pair[1] }).toArray()[0];

        assertTrue(row.length <= diffHours, row);

        assertEqual(c.document(keyPref + row.minIdx).time, row.minTime);
        assertEqual(c.document(keyPref + row.maxIdx).time, row.maxTime);

        let foundBound = false;
        try {

          let rowLess = c.document(keyPref + (row.minIdx - 1));
          assertTrue(rowLess.time < row.minTime, row);
          foundBound = true;
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
        }
        try {
          let rowMore = c.document(keyPref + (row.maxIdx + 1));
          assertTrue(rowMore.time > row.maxTime, row);
          foundBound = true;
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
        }

        assertTrue(foundBound, row);
      }
    });
  }

  return {
    setUpAll: function () {
      c = db._create(cname, { numberOfShards: 4 });

      let i = 0;
      let docs = [];

      let startYear = 1999;
      let endYear = 2021;
      for (let y = startYear; y < endYear; y++) {
        let date = new Date(y, 0);
        for (let m = 0; m < 12; m++) {
          date.setMonth(m);
          for (let d = 1; d <= 31; d += 2) {
            date.setDate(d);
            if (date.getMonth() !== m) {
              break;
            }

            // pick a time of day
            date.setHours(y - startYear + 2, m, d, 100);

            docs.push({ _key: keyPref + i, idx: i, time: date.valueOf() });
            i++;

          }
        }
        c.insert(docs);
      }

      print("Collection count", c.count());
    },

    tearDownAll: function () {
      db._drop(cname);
    },

    testDateRanges: function () {
      let actual = AQL_EXPLAIN(q, { '@cc': cname, preceding: "P1D", following: "P1D" });
      let nodes = actual.plan.nodes;

      let sortNodes = nodes.filter(n => n.type === "SortNode");
      assertEqual(sortNodes.length, 1);
      assertEqual(1, sortNodes[0].elements.length);
      assertFalse(sortNodes[0].stable);

      runDurationTest();
    },

    testDateRangesWithIndex: function () {

      // add sorted index on time
      c.ensureIndex({ type: 'skiplist', fields: ['time'] });

      let actual = AQL_EXPLAIN(q, { '@cc': cname, preceding: "P1D", following: "P1D" });
      let nodes = actual.plan.nodes;

      let sortNodes = nodes.filter(n => n.type === "SortNode");
      assertEqual(sortNodes.length, 0);

      runDurationTest();
    }
  };
}

jsunity.run(WindowMalarkeyTestSuite);
jsunity.run(WindowDateRangeTestSuite);

return jsunity.done();
