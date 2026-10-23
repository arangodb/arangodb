/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual */

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

// ModificationExecutor must not discard unconsumed rows from the
// current input range when a 1000-operation batch fills during FASTFORWARD /
// fullCount skip after LIMIT.

const db = require("@arangodb").db;
const jsunity = require("jsunity");

function modificationLimitBatchSuite() {
  const cn = "UnitTestsModifyLimitBatch";

  const runInsertLimit = (n) => {
    db._drop(cn);
    const c = db._create(cn);
    const result = db._query(
      `FOR i IN 1..${n} INSERT { i } INTO ${cn} LIMIT 1 RETURN i`
    ).toArray();
    assertEqual([1], result, `n=${n}`);
    assertEqual(n, c.count(), `n=${n}`);
    return c;
  };

  return {
    tearDown: function () {
      db._drop(cn);
    },

    // Test A: original regression (just past one batch)
    testInsertLimit1002: function () {
      runInsertLimit(1002);
    },

    // Test B: exactly one full batch
    testInsertLimit1000: function () {
      runInsertLimit(1000);
    },

    // Test C: more than one full batch
    testInsertLimit2002: function () {
      runInsertLimit(2002);
    },

    // Test D: boundary values around the batch size
    testInsertLimitBoundaries: function () {
      for (const n of [999, 1000, 1001, 1002]) {
        runInsertLimit(n);
      }
    },

    // Test G: non-regression — no LIMIT, and small input
    testInsertNoLimitAboveBatch: function () {
      db._drop(cn);
      const c = db._create(cn);
      const result = db._query(
        `FOR i IN 1..1002 INSERT { i } INTO ${cn} RETURN i`
      ).toArray();
      assertEqual(1002, result.length);
      assertEqual(1002, c.count());
    },

    testInsertLimitSmall: function () {
      runInsertLimit(10);
    },

    // Test E: UPDATE / REPLACE / REMOVE with LIMIT past a batch boundary
    testUpdateLimit1002: function () {
      db._drop(cn);
      const c = db._create(cn);
      let docs = [];
      for (let i = 1; i <= 1002; ++i) {
        docs.push({ _key: "k" + i, v: 0 });
      }
      c.insert(docs);

      const result = db._query(
        `FOR d IN ${cn}
         UPDATE d WITH { v: 1 } IN ${cn}
         LIMIT 1
         RETURN OLD._key`
      ).toArray();
      assertEqual(1, result.length);
      assertEqual(1002, c.byExample({ v: 1 }).toArray().length);
    },

    testReplaceLimit1002: function () {
      db._drop(cn);
      const c = db._create(cn);
      let docs = [];
      for (let i = 1; i <= 1002; ++i) {
        docs.push({ _key: "k" + i, v: 0 });
      }
      c.insert(docs);

      const result = db._query(
        `FOR d IN ${cn}
         REPLACE d WITH { v: 1 } IN ${cn}
         LIMIT 1
         RETURN OLD._key`
      ).toArray();
      assertEqual(1, result.length);
      assertEqual(1002, c.byExample({ v: 1 }).toArray().length);
    },

    testRemoveLimit1002: function () {
      db._drop(cn);
      const c = db._create(cn);
      let docs = [];
      for (let i = 1; i <= 1002; ++i) {
        docs.push({ _key: "k" + i });
      }
      c.insert(docs);

      const result = db._query(
        `FOR d IN ${cn}
         REMOVE d IN ${cn}
         LIMIT 1
         RETURN OLD._key`
      ).toArray();
      assertEqual(1, result.length);
      assertEqual(0, c.count());
    },

    // Test F: fullCount / FASTFORWARD path (exercises skipRowsRange with
    // hardLimit 0 + fullCount after LIMIT is satisfied).
    testInsertLimitFullCount1002: function () {
      db._drop(cn);
      const c = db._create(cn);
      const cursor = db._query(
        `FOR i IN 1..1002 INSERT { i } INTO ${cn} LIMIT 1 RETURN i`,
        {},
        { fullCount: true }
      );
      assertEqual([1], cursor.toArray());
      assertEqual(1002, c.count());
      assertEqual(1002, cursor.getExtra().stats.fullCount);
    },

    testInsertLimitFullCount2002: function () {
      db._drop(cn);
      const c = db._create(cn);
      const cursor = db._query(
        `FOR i IN 1..2002 INSERT { i } INTO ${cn} LIMIT 1 RETURN i`,
        {},
        { fullCount: true }
      );
      assertEqual([1], cursor.toArray());
      assertEqual(2002, c.count());
      assertEqual(2002, cursor.getExtra().stats.fullCount);
    }
  };
}

jsunity.run(modificationLimitBatchSuite);
return jsunity.done();
