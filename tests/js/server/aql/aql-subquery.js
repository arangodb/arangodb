/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Ahuacatl, subqueries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
const { assertEqual, assertTrue } = jsunity.jsUnity.assertions;
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var findExecutionNodes = helper.findExecutionNodes;
const { db } = require("@arangodb");
const isCoordinator = require('@arangodb/cluster').isCoordinator();
const isEnterprise = require("internal").isEnterprise();

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSubqueryTestSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryReference1 : function () {
      var expected = [ [ [ 1 ], [ 1 ] ] ];
      var actual = getQueryResults("LET a = (RETURN 1) LET b = a RETURN [ a, b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryReference2 : function () {
      var expected = [ [ [ 1 ], [ 1 ], [ 1 ] ] ];
      var actual = getQueryResults("LET a = (RETURN 1) LET b = a LET c = b RETURN [ a, b, c ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryReference3 : function () {
      var expected = [ [ [ 1 ], [ 1 ], [ 1 ] ] ];
      var actual = getQueryResults("LET a = (RETURN 1) LET b = a LET c = a RETURN [ a, b, c ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryReferences1 : function () {
      var expected = [ [ 1 ] ];
      var actual = getQueryResults("LET a = true ? (RETURN 1) : (RETURN 0) RETURN a");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryReferences2 : function () {
      var expected = [ [ 1 ] ];
      var actual = getQueryResults("LET a = true ? (RETURN 1) : (RETURN 0) LET b = a RETURN b");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryDependent1 : function () {
      var expected = [ 2, 4, 6 ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] LET s = (FOR j IN [ 1, 2, 3 ] RETURN i * 2) RETURN s[i - 1]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryDependent2 : function () {
      var expected = [ 2, 4, 6 ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] LET t = (FOR k IN [ 1, 2, 3 ] RETURN k) LET s = (FOR j IN [ 1, 2, 3 ] RETURN t[i - 1] * 2) RETURN s[i - 1]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryDependent3 : function () {
      var expected = [ 4, 5, 6, 8, 10, 12, 12, 15, 18 ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] FOR j IN [ 4, 5, 6 ] LET s = (FOR k IN [ 1 ] RETURN i * j) RETURN s[0]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery optimization
////////////////////////////////////////////////////////////////////////////////

    testSubqueryIndependent1 : function () {
      var expected = [ 9, 8, 7 ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] LET s = (FOR j IN [ 9, 8, 7 ] RETURN j) RETURN s[i - 1]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery optimization
////////////////////////////////////////////////////////////////////////////////

    testSubqueryIndependent2 : function () {
      var expected = [ [ 9, 8, 7 ] ];
      var actual = getQueryResults("LET s = (FOR j IN [ 9, 8, 7 ] RETURN j) RETURN s");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery optimization
////////////////////////////////////////////////////////////////////////////////

    testSubqueryIndependent3 : function () {
      var expected = [ [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ] ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] FOR j IN [ 1, 2, 3 ] LET s = (FOR k IN [ 25 ] RETURN k) RETURN s");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery optimization
////////////////////////////////////////////////////////////////////////////////

    testSubqueryIndependent4 : function () {
      var expected = [ [ 2, 4, 6 ] ];
      var actual = getQueryResults("LET a = (FOR i IN [ 1, 2, 3 ] LET s = (FOR j IN [ 1, 2 ] RETURN j) RETURN i * s[1]) RETURN a");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery optimization
////////////////////////////////////////////////////////////////////////////////

    testSubqueryOutVariableName : function () {
      const explainResult = AQL_EXPLAIN("FOR u IN _users LET theLetVariable = (FOR j IN _users RETURN j) RETURN theLetVariable",
        {}, {optimizer: {rules: ['-splice-subqueries']}});

      const subqueryNode = findExecutionNodes(explainResult, "SubqueryNode")[0];

      assertEqual(subqueryNode.outVariable.name, "theLetVariable");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test splice-subqueries optimization
////////////////////////////////////////////////////////////////////////////////

    testSpliceSubqueryOutVariableName : function () {
      const explainResult = AQL_EXPLAIN("FOR u IN _users LET theLetVariable = (FOR j IN _users RETURN j) RETURN theLetVariable");

      const subqueryEndNode = findExecutionNodes(explainResult, "SubqueryEndNode")[0];

      assertEqual(subqueryEndNode.outVariable.name, "theLetVariable");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery as function parameter
////////////////////////////////////////////////////////////////////////////////

    testSubqueryAsFunctionParameter1: function () {
      var expected = [ [ 1, 2, 3 ] ];
      var actual = getQueryResults("RETURN PASSTHRU(FOR i IN [ 1, 2, 3 ] RETURN i)");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN PASSTHRU((FOR i IN [ 1, 2, 3 ] RETURN i))");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery as function parameter
////////////////////////////////////////////////////////////////////////////////

    testSubqueryAsFunctionParameter2: function () {
      var expected = [ 6 ];
      var actual = getQueryResults("RETURN MAX(APPEND((FOR i IN [ 1, 2, 3 ] RETURN i), (FOR i IN [ 4, 5, 6 ] RETURN i)))");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN MAX(APPEND(FOR i IN [ 1, 2, 3 ] RETURN i, FOR i IN [ 4, 5, 6 ] RETURN i))");
      assertEqual(expected, actual);

      expected = [ 9 ];
      actual = getQueryResults("RETURN MAX(APPEND(FOR i IN [ 9, 7, 8 ] RETURN i, [ 1 ]))");
      assertEqual(expected, actual);

      expected = [ [ 1, 2, 3, 4, 5, 6 ] ];
      actual = getQueryResults("RETURN UNION((FOR i IN [ 1, 2, 3 ] RETURN i), (FOR i IN [ 4, 5, 6 ] RETURN i))");
      assertEqual(expected, actual);

      actual = getQueryResults("RETURN UNION(FOR i IN [ 1, 2, 3 ] RETURN i, FOR i IN [ 4, 5, 6 ] RETURN i)");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test copying of subquery results
////////////////////////////////////////////////////////////////////////////////

    testIndependentSubqueries1: function () {
      var expected = [ [ [ ], [ ] ] ];
      // use a single block
      for (var i = 1; i <= 100; ++i) {
        expected[0][0].push(i);
        expected[0][1].push(i);
      }

      var actual = getQueryResults("LET a = (FOR i IN 1..100 RETURN i) LET b = (FOR i IN 1..100 RETURN i) RETURN [a, b]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test copying of subquery results
////////////////////////////////////////////////////////////////////////////////

    testIndependentSubqueries2: function () {
      var expected = [ [ [ ], [ ] ] ];
      // use multiple blocks
      for (var i = 1; i <= 10000; ++i) {
        expected[0][0].push(i);
        expected[0][1].push(i);
      }

      var actual = getQueryResults("LET a = (FOR i IN 1..10000 RETURN i) LET b = (FOR i IN 1..10000 RETURN i) RETURN [a, b]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test copying of subquery results
////////////////////////////////////////////////////////////////////////////////

    testDependentSubqueries1: function () {
      var expected = [ [ [ ], [ ] ] ];
      for (var i = 1; i <= 100; ++i) {
        expected[0][0].push(i);
        expected[0][1].push(i);
      }

      var actual = getQueryResults("LET a = (FOR i IN 1..100 RETURN i) LET b = (FOR i IN 1..100 FILTER i IN a RETURN i) RETURN [a, b]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test copying of subquery results
////////////////////////////////////////////////////////////////////////////////

    testDependentSubqueries2: function () {
      var expected = [ [ [ ], [ ] ] ];
      for (var i = 1; i <= 100; ++i) {
        if (i < 50) {
          expected[0][0].push(i);
        }
        else {
          expected[0][1].push(i);
        }
      }

      var actual = getQueryResults("LET a = (FOR i IN 1..100 FILTER i < 50 RETURN i) LET b = (FOR i IN 1..100 FILTER i NOT IN a RETURN i) RETURN [a, b]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test copying of subquery results
////////////////////////////////////////////////////////////////////////////////

    testDependentSubqueries3: function () {
      var expected = [ ];
      for (var i = 1; i <= 2000; ++i) {
        expected.push(i);
      }

      var actual = getQueryResults("LET a = (FOR i IN 1..2000 RETURN i) FOR i IN 1..10000 FILTER i IN a RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test limit with offset in a subquery
/// This test was introduced due to a bug (that never made it into devel) where
/// the offset in the LimitBlock was mutated during a subquery run and not reset
/// in between.
////////////////////////////////////////////////////////////////////////////////

    testSubqueriesWithLimitAndOffset: function () {
      const query = `
        FOR i IN 2..4
        LET a = (FOR j IN [0, i, i+10] LIMIT 1, 1 RETURN j)
        RETURN FIRST(a)`;
      const expected = [ 2, 3, 4 ];

      var actual = getQueryResults(query);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief this tests a rather complex interna of AQL execution combinations
/// A subquery should only be executed if it has an input row
/// A count collect block will produce an output even if it does not get an input
/// specifically it will rightfully count 0.
/// The insert block will write into the collection if it gets an input.
/// Even if the outer subquery is skipped. Henve we require to have documents
/// inserted here.
////////////////////////////////////////////////////////////////////////////////
    testCollectWithinEmptyNestedSubquery: function () {
      const colName = "UnitTestSubqueryCollection";
      try {
        db._create(colName);
        const query = `
          FOR k IN 1..2
            LET sub1 = (
              FOR x IN []
                LET sub2 = (
                  COLLECT WITH COUNT INTO q
                  INSERT {counted: q} INTO ${colName}
                  RETURN NEW
                )
                RETURN sub2
            )
            RETURN [k, sub1]
        `;
        const expected = [ [1, []], [2, []]];

        var actual = getQueryResults(query);
        assertEqual(expected, actual);
        assertEqual(db[colName].count(), 1);
      } finally {
        db._drop(colName);
      }
      
    },

    testCollectionAccessSubquery: function () {
      const colName = "UnitTestSubqueryCollection";
      try {
        const col = db._create(colName, {numberOfShards: 9});
        let dbServers = 0;
        if (isCoordinator) {
          dbServers = Object.values(col.shards(true)).filter((value, index, self) => {
            return self.indexOf(value) === index;
          }).length;
        }
        const docs = [];
        const expected = new Map();
        for (let i = 0; i < 2000; ++i) {
          docs.push({value: i, mod100: i % 100});
          const oldValue = expected.get(i % 100) || [];
          oldValue.push(i);
          expected.set(i % 100, oldValue);
        }
        col.save(docs);

        // Now we do a left outer join on the same collection
        const query = `
          FOR left IN ${colName}
            LET rightJoin = (
              FOR right IN ${colName}
                FILTER left.mod100 == right.mod100
                RETURN right.value
            )
            RETURN {key: left.mod100, value: rightJoin}
        `;
        // First NoIndex variant
        {
          const cursor = db._query(query);
          const actual = cursor.toArray();
          const {scannedFull, scannedIndex, filtered, httpRequests} = cursor.getExtra().stats;
          assertEqual(scannedFull, 4002000);
          assertEqual(scannedIndex, 0);
          assertEqual(filtered, 3960000);
          if (isCoordinator) {
            assertTrue(httpRequests <= 4003 * dbServers + 1, httpRequests);
          } else {
            assertEqual(httpRequests, 0);
          }
          const foundKeys = new Map();
          for (const {key, value} of actual) {
            assertTrue(expected.has(key));
            // Use sort here as no ordering is guaranteed by query.
            assertEqual(expected.get(key).sort(), value.sort());
            foundKeys.set(key, (foundKeys.get(key) || 0) + 1);
          }
          assertEqual(foundKeys.size, expected.size);
          for (const value of foundKeys.values()) {
            assertEqual(value, 20);
          }
        }
        // Second with Index
        {
          col.ensureHashIndex("mod100");

          const cursor = db._query(query);
          const actual = cursor.toArray();
          const {scannedFull, scannedIndex, filtered, httpRequests} = cursor.getExtra().stats;
          assertEqual(scannedFull, 2000);
          assertEqual(scannedIndex, 40000);
          assertEqual(filtered, 0);
          if (isCoordinator) {
            assertTrue(httpRequests <= 4003 * dbServers + 1, httpRequests);
          } else {
            assertEqual(httpRequests, 0);
          }
          const foundKeys = new Map();
          for (const {key, value} of actual) {
            assertTrue(expected.has(key));
            // Use sort here as no ordering is guaranteed by query.
            assertEqual(expected.get(key).sort(), value.sort());
            foundKeys.set(key, (foundKeys.get(key) || 0) + 1);
          }
          assertEqual(foundKeys.size, expected.size);
          for (const value of foundKeys.values()) {
            assertEqual(value, 20);
          }
        }        
      } finally {
        db._drop(colName);
      }
    },

    testOneShardDBAndSpliceSubquery: function () {
      const dbName = "SingleShardDB";
      const docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({foo: i});
      }
      const q = `
        FOR x IN a
          SORT x.foo ASC
          LET subquery = (
            FOR y IN b
              FILTER x.foo == y.foo
              RETURN y
          )
          RETURN {x, subquery}
      `;
      try {
        db._createDatabase(dbName, {sharding: "single"});
        db._useDatabase(dbName);
        const a = db._create("a");
        const b = db._create("b");
        a.save(docs);
        b.save(docs);
        const statement = db._createStatement(q);
        const rules = statement.explain().plan.rules;
        if (isEnterprise && isCoordinator) {
          // Has one shard rule
          assertTrue(rules.indexOf("cluster-one-shard") !== -1);
        }
        // Has one splice subquery rule
        assertTrue(rules.indexOf("splice-subqueries") !== -1);
        // Result is as expected
        const result = statement.execute().toArray();
        for (let i = 0; i < 100; ++i) {
          assertEqual(result[i].x.foo, i);
          assertEqual(result[i].subquery.length, 1);
          assertEqual(result[i].subquery[0].foo, i);
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    }
  }; 
}

jsunity.run(ahuacatlSubqueryTestSuite);

return jsunity.done();

