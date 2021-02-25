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

const isCoordinator = require('@arangodb/cluster').isCoordinator();
const isEnterprise = require("internal").isEnterprise();

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

function WindowHappyTestSuite() {

  const gm = require('@arangodb/general-graph');
  const vn = 'UnitTestVertexCollection';
  const en = 'UnitTestEdgeCollection';
  const gn = 'UnitTestGraph';

  return {
    tearDown: function () {
      try {
        gm._drop(gn);
      } catch (e) { }
      db._drop(vn);
      db._drop(en);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test window with traversal
    ////////////////////////////////////////////////////////////////////////////////

    testWindowAndTraversal: function () {

      let vc = db._create(vn, { numberOfShards: 4 });
      let ec = db._createEdgeCollection(en, { numberOfShards: 4 });
      try {
        gm._drop(gn);
      } catch (e) {
        // It is expected that this graph does not exist.
      }

      gm._create(gn, [gm._relation(en, vn, vn)]);

      var start = vc.save({ _key: 's', value: 0 })._id;
      var a = vc.save({ _key: 'a', value: 2 })._id;
      var b = vc.save({ _key: 'b', value: 3 })._id;
      var c = vc.save({ _key: 'c', value: 4 })._id;
      var d = vc.save({ _key: 'd', value: 5 })._id;
      ec.save(start, a, {});
      ec.save(a, b, {});
      ec.save(b, c, {});
      ec.save(c, a, {});
      ec.save(a, d, {});
      var cursor = db._query(
        `WITH ${vn}
        FOR v IN 1..10 OUTBOUND '${start}' ${en} OPTIONS {uniqueEdges: 'none'}
        WINDOW v.value WITH {preceding: 5, following: 0} AGGREGATE l = LENGTH(v)
        RETURN MERGE(v, {length: l})`).toArray();
      // We expect to get s->a->d
      // We expect to get s->a->b->c->a->d
      // We expect to get s->a->b->c->a->b->c->a->d
      // We expect to get s->a->b->c->a->b->c->a->b->c->a
      assertEqual(cursor.length, 13);
      assertEqual(cursor[0]._id, a); // We start with a
      assertEqual(cursor[0].length, 4, cursor[0]);
      assertEqual(cursor[1]._id, a); // We somehow return to a
      assertEqual(cursor[1].length, 4);
      assertEqual(cursor[2]._id, a); // We somehow return to a again
      assertEqual(cursor[2].length, 4);
      assertEqual(cursor[3]._id, a); // We somehow return to a again
      assertEqual(cursor[3].length, 4);
      assertEqual(cursor[4]._id, b); // We once find b
      assertEqual(cursor[4].length, 7);
      assertEqual(cursor[5]._id, b); // We find b again
      assertEqual(cursor[5].length, 7);
      assertEqual(cursor[6]._id, b); // And b again
      assertEqual(cursor[6].length, 7);
      assertEqual(cursor[7]._id, c); // And once c
      assertEqual(cursor[7].length, 10);
      assertEqual(cursor[8]._id, c); // We find c again
      assertEqual(cursor[8].length, 10);
      assertEqual(cursor[9]._id, c); // And c again
      assertEqual(cursor[9].length, 10);
      assertEqual(cursor[10]._id, d); // Short Path d
      assertEqual(cursor[10].length, 13);
      assertEqual(cursor[11]._id, d); // One Loop d
      assertEqual(cursor[11].length, 13);
      assertEqual(cursor[12]._id, d); // Second Loop d
      assertEqual(cursor[12].length, 13);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test copying of subquery results
    ////////////////////////////////////////////////////////////////////////////////

    testWindowInSubquery: function () {
      var expected = [[[], [], []]];
      for (var i = 1; i <= 100; ++i) {
        if (i < 50) {
          expected[0][0].push(i);
        }
        else {
          expected[0][1].push(i);
          let sum = ((i > 51) ? i - 2 : 0) + ((i > 50) ? i - 1 : 0) + i + (i < 100 ? i + 1 : 0) + + (i < 99 ? i + 2 : 0);
          expected[0][2].push(sum);
        }
      }

      const q = `LET a = (FOR i IN 1..100 FILTER i < 50 RETURN i) 
      LET b = (FOR i IN 1..100 
                FILTER i NOT IN a 
                WINDOW {preceding: 2, following: 2} AGGREGATE c = SUM(i)
                RETURN [i, c]) 
      RETURN [a, b[*][0], b[*][1]]`;

      assertEqual(expected, db._query(q).toArray());
    },

    ////////////////////////////////////////////////////////////////////////////////
    // testOneShardDBAndSpliceSubquery
    ////////////////////////////////////////////////////////////////////////////////

    testWindowAndSubqueryInOneShardDB: function () {
      const dbName = "SingleShardDB";
      const docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ foo: i });
      }
      const q = `
        FOR x IN a
          SORT x.foo ASC
          LET subquery = (
            FOR y IN b
              FILTER x.foo == y.foo
              WINDOW y.foo WITH {preceding: 1, following: 1} AGGREGATE sum = SUM(y.foo)
              RETURN MERGE(y, {sum: sum})
          )
          RETURN {x, subquery}
      `;
      try {
        db._createDatabase(dbName, { sharding: "single" });
        db._useDatabase(dbName);
        const a = db._create("a");
        const b = db._create("b");
        a.save(docs);
        b.save(docs);
        const statement = db._createStatement(q);
        const rules = statement.explain().plan.rules;
        if (isEnterprise && isCoordinator) {
          // Has one shard rule
          assertNotEqual(-1, rules.indexOf("cluster-one-shard"));
        }
        // Has one splice subquery rule
        assertNotEqual(-1, rules.indexOf("splice-subqueries"));
        // Result is as expected
        const result = statement.execute().toArray();
        for (let i = 0; i < 100; ++i) {
          assertEqual(result[i].x.foo, i);
          assertEqual(result[i].subquery.length, 1);
          assertEqual(result[i].subquery[0].foo, i);
          assertEqual(result[i].subquery[0].sum, i, result[i].subquery[0]);
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
  };
};

jsunity.run(WindowMalarkeyTestSuite);
jsunity.run(WindowDateRangeTestSuite);
jsunity.run(WindowHappyTestSuite);

return jsunity.done();
