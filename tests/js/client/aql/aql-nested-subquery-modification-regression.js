/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = require("@arangodb").db;

const colName = "UnitTestCollection";

const tearDown = () => {
  db._drop(colName);
};
const createCollection = () => {
  db._createDocumentCollection(colName, { numberOfShards: 3 });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function subqueryModificationRegressionSuite() {
  return {
    setUp: function() {
      tearDown();
      createCollection();
    },
    tearDown,
    testNestedSubqueryInsert: function () {
      // We only got this query to crash on cluster setups, as
      // the distribute/gather combination deletes defaultCalls
      // in the callstack.
      const query = `
        LET sq0 = (
          LET sq1 = (
            LET sq2 = (INSERT {} INTO ${colName} RETURN NEW )
            RETURN {}) 
          RETURN {})
        RETURN {}`;

      const r = db._query(query);
    },
    testNestedSubqueryInsertWithLimitAndFilter: function() {
      // This query even crashes on single server as the
      // LIMIT/FILTER combination induces the same problem as with the
      // query above
      const query = `
        LET sq0 = (
          LET sq1 = (
            LET sq2 = (INSERT {} INTO ${colName} RETURN NEW )
            RETURN {}) 
          FILTER NOOPT(true)
          LIMIT 1,10
          RETURN {})
        RETURN {}`;

      const r = db._query(query);
    },
    testNestedSubqueryRemoveWithLimitAndFilter: function() {
      const prepQuery = `INSERT {_key: "testDocument"} IN ${colName} RETURN {}`;
      db._query(prepQuery);
      const query = `
        LET sq0 = (
          LET sq1 = (
            LET sq2 = (REMOVE {_key: "testDocument"} IN ${colName} RETURN {} )
            RETURN {})
          FILTER NOOPT(true)
          LIMIT 1,10
          RETURN {})
        RETURN {}`;
      const r = db._query(query);

      const check = db._query(`FOR x IN ${colName} RETURN x`).toArray();
      assertEqual(check.length, 0);
    },
    testNestedSubqueryUpdateWithLimitAndFilter: function() {
      // This query even crashes on single server as the
      // LIMIT/FILTER combination induces the same problem as with the
      // query above
      const prepQuery = `INSERT {_key: "testDocument", testAttr: 1} IN ${colName} RETURN {}`;
      db._query(prepQuery);
      const query = `
        LET sq0 = (
          LET sq1 = (
            LET sq2 = (UPDATE {_key: "testDocument", testAttr: 0} IN ${colName} RETURN {} )
            RETURN {})
          FILTER NOOPT(true)
          LIMIT 1,10
          RETURN {})
        RETURN {}`;

      const r = db._query(query);

      const check = db._query(`FOR x IN ${colName} RETURN x`).toArray();
      assertEqual(check.length, 1);

      assertEqual(check[0].testAttr, 0);
    },
    testNestedSubqueryUpdateWithLimitAndFilter: function() {
     const prepQuery = `INSERT {_key: "testDocument", testAttr: 1} IN ${colName} RETURN {}`;
      db._query(prepQuery);
      const query = `
        LET sq0 = (
          LET sq1 = (
            LET sq2 = (UPDATE {_key: "testDocument", testAttr: 0} IN ${colName} RETURN {} )
            RETURN {})
          FILTER NOOPT(true)
          LIMIT 1,10
          RETURN {})
        RETURN {}`;

      const r = db._query(query);

      const check = db._query(`FOR x IN ${colName} RETURN x`).toArray();
      assertEqual(check.length, 1);

      assertEqual(check[0].testAttr, 0);
    },
    testNestedSubqueryReplaceWithLimitAndFilter: function() {
      const prepQuery = `INSERT {_key: "testDocument", testAttr: 1} IN ${colName} RETURN {}`;
      db._query(prepQuery);
      const query = `
        LET sq0 = (
          LET sq1 = (
            LET sq2 = (UPDATE {_key: "testDocument", testAttr: 0} IN ${colName} RETURN {} )
            RETURN {})
          FILTER NOOPT(true)
          LIMIT 1,10
          RETURN {})
        RETURN {}`;

      const r = db._query(query);

      const check = db._query(`FOR x IN ${colName} RETURN x`).toArray();
      assertEqual(check.length, 1);

      assertEqual(check[0].testAttr, 0);
    },
    testNestedSubqueryUpsertWithLimitAndFilter: function() {
     const query = `
        LET sq0 = (
          FOR x IN 1..1001
            LET sq1 = (
              LET sq2 = (UPSERT { _key: 'test' }
                         INSERT { _key: 'test', value: [x] }
                         UPDATE { value: APPEND(OLD.value, [x]) } IN ${colName} RETURN NEW)
              RETURN sq2)
            FILTER NOOPT(true)
            LIMIT 1,10
            RETURN sq1)
        RETURN sq0`;

      const r = db._query(query);

      const r2 = db._query( `FOR x IN ${colName} FILTER x._key == 'test' RETURN x`);
      const arr = r2.toArray();
      assertEqual(arr.length, 1);
      assertEqual(arr[0].value.length, 1001);
    },
    testNestedSubqueryInsertWithLimitAndFilter2: function() {
      const query = `
        LET sq0 = (
          FOR y IN 1..20
          LET sq1 = (
            FOR x IN 1..1001
              LET sq2 = (INSERT {v: TO_STRING(x) } INTO ${colName} RETURN NEW)
              LIMIT 1000,1
              FILTER NOOPT(true)
              RETURN sq2)
          FILTER NOOPT(true)
          LIMIT 1,10
          RETURN sq1)
        LIMIT 5,100
        RETURN sq0`;

      const r = db._query(query);
    },
    testDeeplyNestedSubquery: function() {
     const query = `
        LET sq0 = (
          LET sq1 = (
            LET sq2 = (
              LET sq3 = (
                LET sq4 = (
                  FOR x IN 1..1001
                    LET sq5 = (INSERT {v: TO_STRING(x) } INTO ${colName} RETURN NEW)
                    LIMIT 900,120
                    RETURN sq5)
                FILTER NOOPT(true)
                LIMIT 1,10
                RETURN sq4)
              RETURN sq3)
            RETURN sq2)
          RETURN sq1)
        RETURN sq0`;

      const r = db._query(query);
    },
  };
}

jsunity.run(subqueryModificationRegressionSuite);

return jsunity.done();
