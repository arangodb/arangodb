/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for AQL, persistent index
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

const internal = require("internal");
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;

const cn = "UnitTestsCollection";
const idxName = "testIdx";
const query1 = `FOR d IN ${cn} FILTER d.value == 1 SORT d._key RETURN d`;
const query2 = `FOR d IN ${cn} FILTER d.value == 2 SORT d._key RETURN d`;
const tolerance = 0.03;


let explain = function (query, params) {
  return helper.removeClusterNodes(helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-indexes" ] } })).map(function(node) { return node.type; }));
};

let insertMoreDocs = function (numDocs, sameValue) {
  let docs = [];
  for (let i = 0; i < numDocs; ++i) {
    docs.push({value: (sameValue ? 2 : i)});
    if (docs.length >= numDocs / 10) {
      internal.db[cn].insert(docs);
      docs = [];
    }
  }
};

let testAndValidate = function (query, isUnique, moreDocs, numDocs, sameValue, isPrimary, selectivity) {
  if (moreDocs) {
    insertMoreDocs(numDocs, sameValue);
  }
  internal.db[cn].ensureIndex({type: "persistent", name: idxName, unique: isUnique, fields: ["value"]});

  const nodes = AQL_EXPLAIN(query).plan.nodes;
  for (const key in nodes) {
    if (nodes[key].type === "IndexNode") {
      const indexNode = nodes[key];
      assertEqual(indexNode.indexes.length, 1);
      const usedIndex = indexNode.indexes[0];
      assertEqual(usedIndex.type, (isPrimary ? "primary" : "persistent"));
      assertEqual(usedIndex.name, (isPrimary ? "primary" : idxName));
      assertEqual(usedIndex.fields.length, 1);
      assertEqual(usedIndex.fields[0], (isPrimary ? "_key" : "value"));
      assertTrue(Math.abs(usedIndex.selectivityEstimate - selectivity) < tolerance);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function PersistentIndexTestSuite () {
  let collection;

  return {

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      let docs = [];
      for (let i = 1; i <= 5; ++i) {
        for (let j = 1; j <= 5; ++j) {
          docs.push({ "a" : i, "b": j });
        }
      }
      collection.insert(docs);

      collection.ensureIndex({ type: "persistent", fields: ["a", "b"] });
    },

    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field without results
////////////////////////////////////////////////////////////////////////////////

    testEqSingleVoid1 : function () {
      var query = "FOR v IN " + collection.name() + " FILTER v.a == 99 RETURN v";
      var expected = [ ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field without results
////////////////////////////////////////////////////////////////////////////////

    testEqSingleVoid2 : function () {
      var query = "FOR v IN " + collection.name() + " FILTER 99 == v.a RETURN v";
      var expected = [ ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle1 : function () {
      var query = "FOR v IN " + collection.name() + " FILTER v.a == 1 SORT v.b RETURN [ v.a, v.b ]";
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle2 : function () {
      var query = "FOR v IN " + collection.name() + " FILTER 1 == v.a SORT v.b RETURN [ v.a, v.b ]";
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode","CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle3 : function () {
      var query = "FOR v IN " + collection.name() + " FILTER v.a == 5 SORT v.b RETURN [ v.a, v.b ]";
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with equality
////////////////////////////////////////////////////////////////////////////////

    testEqSingle4 : function () {
      var query = "FOR v IN " + collection.name() + " FILTER 5 == v.a SORT v.b RETURN [ v.a, v.b ]";
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index - optimizer should remove sort.
////////////////////////////////////////////////////////////////////////////////

    testEqSingle5 : function () {
      var query = "FOR v IN " + collection.name() + " FILTER v.a == 1 SORT v.a, v.b RETURN [ v.a, v.b ]";
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index - optimizer should remove sort.
////////////////////////////////////////////////////////////////////////////////

    testEqSingle6 : function () {
      var query = "FOR v IN " + collection.name() + " FILTER v.a >= 4 SORT v.a RETURN v.a";
      var expected = [ 4, 4, 4, 4, 4, 5, 5, 5, 5, 5 ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index - optimizer should remove sort.
////////////////////////////////////////////////////////////////////////////////

    testEqSingle7: function () {
      var query = "FOR v IN " + collection.name() + " FILTER v.a >= 4 && v.a < 5 SORT v.a RETURN v.a";
      var expected = [ 4, 4, 4, 4, 4 ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with greater than 
////////////////////////////////////////////////////////////////////////////////

    testGtSingle1 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a > 4 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with greater than 
////////////////////////////////////////////////////////////////////////////////

    testGtSingle2 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 4 < v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testGeSingle1 : function () {
      var query = "FOR v IN " + collection.name() + " FILTER v.a >= 5 SORT v.b RETURN [ v.a, v.b ]";
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testLtSingle1 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 5 <= v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with less than 
////////////////////////////////////////////////////////////////////////////////

    testLtSingle2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a < 2 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with less than 
////////////////////////////////////////////////////////////////////////////////

    testGtSingle3 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 2 > v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testLtSingle4 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a <= 1 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with greater equal 
////////////////////////////////////////////////////////////////////////////////

    testGeSingle2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 >= v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a >= 1 && v.a <= 2 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 <= v.a && 2 >= v.a SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle3 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a > 1 && v.a < 2 RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle4 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 < v.a && 2 > v.a RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle5 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 <= v.a && 2 > v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle6 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 <= v.a && 2 > v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle7 : function () {
      var expected = [ [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a > 1 && v.a <= 2 SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first collection index field with a range access
////////////////////////////////////////////////////////////////////////////////

    testRangeSingle8 : function () {
      var expected = [ [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 < v.a && 2 >= v.a SORT v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiVoid1 : function () {
      var expected = [ ];
      var query = "FOR v IN " + collection.name() + " FILTER v.a == 99 && v.b == 1 RETURN v";
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiVoid2 : function () {
      var expected = [ ];
      var query = "FOR v IN " + collection.name() + " FILTER 99 == v.a && 1 == v.b RETURN v";
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiVoid3 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a == 1 && v.b == 99 RETURN v");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiVoid4 : function () {
      var expected = [ ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 == v.a && 99 == v.b RETURN v");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiAll1 : function () {
      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <=5; ++j) {
          var expected = [ [ i, j ] ];
          var query = "FOR v IN " + collection.name() + " FILTER v.a == @a && v.b == @b RETURN [ v.a, v.b ]";
          var actual = getQueryResults(query, { "a": i, "b": j });

          assertEqual(expected, actual);

          assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query, { a: i, b: j }));
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqMultiAll2 : function () {
      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <=5; ++j) {
          var expected = [ [ i, j ] ];
          var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER @a == v.a && @b == v.b RETURN [ v.a, v.b ]", { "a" : i, "b" : j });

          assertEqual(expected, actual);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt1 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a == 1 && v.b > 2 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt2 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 == v.a && 2 < v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt3 : function () {
      var expected = [ [ 4, 4 ], [ 4, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a == 4 && v.b > 3 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqGt4 : function () {
      var expected = [ [ 4, 4 ], [ 4, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 4 == v.a && 3 < v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a == 1 && v.b < 4 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 == v.a && 4 > v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt3 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a == 5 && v.b < 5 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLt4 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 5 == v.a && 5 > v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a == 1 && v.b <= 3 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 1 == v.a && 3 >= v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe3 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ]  ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a == 5 && v.b <= 4 SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testEqLe4 : function () {
      var expected = [ [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ]  ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 5 == v.a && 4 >= v.b SORT v.b RETURN [ v.a, v.b ]");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtlt1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 2, 1 ], [ 2, 2 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a < 3 && v.b < 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtlt2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 2, 1 ], [ 2, 2 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 > v.a && 3 > v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeLe1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 3, 1 ], [ 3, 2 ], [ 3, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a <= 3 && v.b <= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeLe2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 3, 1 ], [ 3, 2 ], [ 3, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 >= v.a && 3 >= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtLe1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a < 3 && v.b <= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtLe2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 > v.a && 3 >= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeLt1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 2, 1 ], [ 2, 2 ], [ 3, 1 ], [ 3, 2 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a <= 3 && v.b < 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeLt2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 2, 1 ], [ 2, 2 ], [ 3, 1 ], [ 3, 2 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 >= v.a && 3 > v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtEq1 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a < 3 && v.b == 4 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtEq2 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 > v.a && 4 == v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeEq1 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a <= 3 && v.b == 4 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeEq2 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 >= v.a && 4 == v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtGt1 : function () {
      var expected = [ [ 1, 4 ], [ 1, 5 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a < 3 && v.b > 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtGt2 : function () {
      var expected = [ [ 1, 4 ], [ 1, 5 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 > v.a && 3 < v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeGe1 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ], [ 3, 3 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a <= 3 && v.b >= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeGe2 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ], [ 3, 3 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 >= v.a && 3 <= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtGe1 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a < 3 && v.b >= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLtGe2 : function () {
      var expected = [ [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 > v.a && 3 <= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeGt1 : function () {
      var expected = [ [ 1, 4 ], [ 1, 5 ], [ 2, 4 ], [ 2, 5 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a <= 3 && v.b > 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testLeGt2 : function () {
      var expected = [ [ 1, 4 ], [ 1, 5 ], [ 2, 4 ], [ 2, 5 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 >= v.a && 3 < v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtlt1 : function () {
      var expected = [ [ 4, 1 ], [ 4, 2 ], [ 5, 1 ], [ 5, 2 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a > 3 && v.b < 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtlt2 : function () {
      var expected = [ [ 4, 1 ], [ 4, 2 ], [ 5, 1 ], [ 5, 2 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 < v.a && 3 > v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeLe1 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 3, 3 ], [ 4, 1 ], [ 4, 2 ], [ 4, 3 ], [ 5, 1 ], [ 5, 2 ], [ 5, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a >= 3 && v.b <= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeLe2 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 3, 3 ], [ 4, 1 ], [ 4, 2 ], [ 4, 3 ], [ 5, 1 ], [ 5, 2 ], [ 5, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 <= v.a && 3 >= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtLe1 : function () {
      var expected = [ [ 4, 1 ], [ 4, 2 ], [ 4, 3 ], [ 5, 1 ], [ 5, 2 ], [ 5, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a > 3 && v.b <= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtLe2 : function () {
      var expected = [ [ 4, 1 ], [ 4, 2 ], [ 4, 3 ], [ 5, 1 ], [ 5, 2 ], [ 5, 3 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 < v.a && 3 >= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeLt1 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 4, 1 ], [ 4, 2 ], [ 5, 1 ], [ 5, 2 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a >= 3 && v.b < 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeLt2 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 4, 1 ], [ 4, 2 ], [ 5, 1 ], [ 5, 2 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 <= v.a && 3 > v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtEq1 : function () {
      var expected = [ [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a > 3 && v.b == 4 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtEq2 : function () {
      var expected = [ [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 < v.a && 4 == v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeEq1 : function () {
      var expected = [ [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a >= 3 && v.b == 4 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeEq2 : function () {
      var expected = [ [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 <= v.a && 4 == v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtGt1 : function () {
      var expected = [ [ 4, 4 ], [ 4, 5 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a > 3 && v.b > 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtGt2 : function () {
      var expected = [ [ 4, 4 ], [ 4, 5 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 < v.a && 3 < v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeGe1 : function () {
      var expected = [ [ 3, 3 ], [ 3, 4 ], [ 3, 5 ], [ 4, 3 ], [ 4, 4 ], [ 4, 5 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a >= 3 && v.b >= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeGe2 : function () {
      var expected = [ [ 3, 3 ], [ 3, 4 ], [ 3, 5 ], [ 4, 3 ], [ 4, 4 ], [ 4, 5 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 <= v.a && 3 <= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtGe1 : function () {
      var expected = [ [ 4, 3 ], [ 4, 4 ], [ 4, 5 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a > 3 && v.b >= 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGtGe2 : function () {
      var expected = [ [ 4, 3 ], [ 4, 4 ], [ 4, 5 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 < v.a && 3 <= v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeGt1 : function () {
      var expected = [ [ 3, 4 ], [ 3, 5 ], [ 4, 4 ], [ 4, 5 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER v.a >= 3 && v.b > 3 SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collection fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testGeGt2 : function () {
      var expected = [ [ 3, 4 ], [ 3, 5 ], [ 4, 4 ], [ 4, 5 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults("FOR v IN " + collection.name() + " FILTER 3 <= v.a && 3 < v.b SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with a constant
////////////////////////////////////////////////////////////////////////////////

    testRefConst1 : function () {
      var expected = [ [ 3, 1 ], [ 3, 2 ], [ 3, 3 ], [ 3, 4 ], [ 3, 5 ] ];
      var actual = getQueryResults("LET x = 3 FOR v IN " + collection.name() + " FILTER v.a == x SORT v.a, v.b RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with a constant
////////////////////////////////////////////////////////////////////////////////

    testRefConst2 : function () {
      var expected = [ [ 3, 5 ] ];
      var actual = getQueryResults("LET x = 3 LET y = 5 FOR v IN " + collection.name() + " FILTER v.a == x && v.b == y RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefSingle1 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v1 IN " + collection.name() + " FOR v2 IN " + collection.name() + " FILTER v1.a == 1 && v2.a == v1.a && v1.b == 1 SORT v1.a, v2.b RETURN [ v1.a, v2.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testRefSingle2 : function () {
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ] ];
      var actual = getQueryResults("FOR v1 IN " + collection.name() + " FOR v2 IN " + collection.name() + " FILTER 1 == v1.a && v1.a == v2.a && 1 == v1.b SORT v1.a, v2.b RETURN [ v1.a, v2.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testRefFilterSame : function () {
      collection.ensureIndex({ type: "persistent", fields: ["c"] });
      collection.ensureIndex({ type: "persistent", fields: ["d"] });

      collection.truncate({ compact: false });

      let docs = [];
      for (var i = 1; i <= 5; ++i) {
        for (var j = 1; j <= 5; ++j) {
          docs.push({ "c" : i, "d": j });
        }
      }
      collection.save(docs);

      var query = "FOR a IN " + collection.name() + " FILTER a.c == a.d SORT a.c RETURN [ a.c, a.d ]";
      var expected = [ [ 1, 1 ], [ 2, 2 ], [ 3, 3 ], [ 4, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testRefFilterNonExisting : function () {
      var query = "FOR a IN " + collection.name() + " FILTER a.e == a.f SORT a.a, a.b RETURN [ a.a, a.b ]";
      var expected = [ [ 1, 1 ], [ 1, 2 ], [ 1, 3 ], [ 1, 4 ], [ 1, 5 ], [ 2, 1 ], [ 2, 2 ], [ 2, 3 ], [ 2, 4 ], [ 2, 5 ], [ 3, 1 ], [ 3, 2 ], [ 3, 3 ], [ 3, 4 ], [ 3, 5 ], [ 4, 1 ], [ 4, 2 ], [ 4, 3 ], [ 4, 4 ], [ 4, 5 ], [ 5, 1 ], [ 5, 2 ], [ 5, 3 ], [ 5, 4 ], [ 5, 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test partial coverage
////////////////////////////////////////////////////////////////////////////////

    testPartialCoverage : function () {
      collection.save({ "a": 20, "c": 1 });
      collection.save({ "a": 20, "c": 2 });
      collection.save({ "a": 21, "c": 1 });
      collection.save({ "a": 21, "c": 2 });

      // c is not indexed, but we still need to find the correct results
      var query = "FOR a IN " + collection.name() + " FILTER a.a == 20 && a.c == 1 RETURN [ a.a, a.c ]";
      var actual = getQueryResults(query);
      var expected = [ [ 20, 1 ] ];

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query));

      query = "FOR a IN " + collection.name() + " FILTER a.a == 20 SORT a.a, a.c RETURN [ a.a, a.c ]";
      actual = getQueryResults(query);
      expected = [ [ 20, 1 ], [ 20, 2 ] ];

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));

      query = "FOR a IN " + collection.name() + " FILTER a.a >= 20 SORT a.a, a.c RETURN [ a.a, a.c ]";
      actual = getQueryResults(query);
      expected = [ [ 20, 1 ], [ 20, 2 ], [ 21, 1 ], [ 21, 2 ] ];

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));

      query = "FOR a IN " + collection.name() + " FILTER a.a >= 21 && a.a <= 21 SORT a.a, a.c RETURN [ a.a, a.c ]";
      actual = getQueryResults(query);
      expected = [ [ 21, 1 ], [ 21, 2 ] ];

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));

      query = "FOR a IN " + collection.name() + " FILTER a.a >= 20 && a.a <= 21 && a.c <= 2 SORT a.a, a.c RETURN [ a.a, a.c ]";
      actual = getQueryResults(query);
      expected = [ [ 20, 1 ], [ 20, 2 ], [ 21, 1 ], [ 21, 2 ] ];

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));

      query = "FOR a IN " + collection.name() + " FILTER a.a == 20 && a.c >= 1 SORT a.a, a.c RETURN [ a.a, a.c ]";
      actual = getQueryResults(query);
      expected = [ [ 20, 1 ], [ 20, 2 ] ];

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reverse iteration
////////////////////////////////////////////////////////////////////////////////

    testReverseIteration : function () {
      var expectedOne = [ ], expectedTwo = [ ];
      for (var i = 5; i >= 1; --i) {
        for (var j = 5; j >= 1; --j) {
          expectedOne.push(i);
          expectedTwo.push([ i, j ]);
        }
      }

      var query = "FOR a IN " + collection.name() + " SORT a.a DESC RETURN a.a";
      var actual = getQueryResults(query);
      assertEqual(expectedOne, actual);

      // produces an empty range
      query = "FOR a IN " + collection.name() + " FILTER a.a >= 100 SORT a.a DESC RETURN a.a";
      actual = getQueryResults(query);
      assertEqual([ ], actual);

      query = "FOR a IN " + collection.name() + " SORT a.a DESC, a.b DESC RETURN [ a.a, a.b ]";
      actual = getQueryResults(query);
      assertEqual(expectedTwo, actual);

      // produces an empty range
      query = "FOR a IN " + collection.name() + " FILTER a.a >= 100 SORT a.a DESC, a.b DESC RETURN [ a.a, a.b ]";
      actual = getQueryResults(query);
      assertEqual([ ], actual);
    },

    testInvalidValuesinList : function () {
      var query = "FOR x IN @list FOR i IN " + collection.name() + " FILTER i.a == x SORT i.a RETURN i.a";
      var bindParams = {
        list: [
          null,
          1, // Find this
          "blub/bla",
          "noKey",
          2, // And this
          123456,
          { "the": "foxx", "is": "wrapped", "in":"objects"},
          [15, "man", "on", "the", "dead", "mans", "chest"],
          3 // And this
        ]
      };
      assertEqual([ 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3], AQL_EXECUTE(query, bindParams).json);
    },

  };
}

function PersistentIndexMultipleIndexesTestSuite () {
  let collection;

  return {

    setUpAll : function () {
      internal.db._drop(cn);

      collection = internal.db._create(cn);

      let docs = [];
      for (let i = 1; i <= 5; ++i) {
        for (let j = 1; j <= 5; ++j) {
          docs.push({ "a" : i, "b": j, "c" : i });
        }
      }
      collection.insert(docs);

      collection.ensureIndex({ type: "persistent", fields: ["a", "b"] });
      collection.ensureIndex({ type: "persistent", fields: ["c"] });
    },

    tearDownAll : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the single field index with equality
////////////////////////////////////////////////////////////////////////////////

    testMultiEqSingle1 : function () {
      var query = "FOR v IN " + cn + " FILTER v.c == 1 SORT v.b RETURN [ v.b ]";
      var expected = [ [ 1 ], [ 2 ], [ 3 ], [ 4 ], [ 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the single field index with equality
////////////////////////////////////////////////////////////////////////////////

    testMultiEqSingle2 : function () {
      var query = "FOR v IN " + cn + " FILTER 1 == v.c SORT v.b RETURN [ v.b ]";
      var expected = [ [ 1 ], [ 2 ], [ 3 ], [ 4 ], [ 5 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first index field with equality
////////////////////////////////////////////////////////////////////////////////

    testMultiEqSingle3 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + cn + " FILTER v.c == 4 SORT v.b RETURN [ v.b, v.c ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the first index field with equality
////////////////////////////////////////////////////////////////////////////////

    testMultiEqSingle4 : function () {
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults("FOR v IN " + cn + " FILTER 4 == v.c SORT v.b RETURN [ v.b, v.c ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testMultiEqMultiAll1 : function () {
      for (let i = 1; i <= 5; ++i) {
        for (let j = 1; j <= 5; ++j) {
          var query = "FOR v IN " + cn + " FILTER v.a == @a && v.b == @b RETURN [ v.a, v.b ]";
          var expected = [ [ i, j ] ];
          var actual = getQueryResults(query, { "a": i, "b": j });

          assertEqual(expected, actual);

          assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query, { a: i, b: j }));
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple fields with multiple operators
////////////////////////////////////////////////////////////////////////////////

    testMultiEqMultiAll2 : function () {
      for (let i = 1; i <= 5; ++i) {
        for (let j = 1; j <= 5; ++j) {
          var query = "FOR v IN " + cn + " FILTER @a == v.a && @b == v.b RETURN [ v.a, v.b ]";
          var expected = [ [ i, j ] ];
          var actual = getQueryResults(query, { "a": i, "b": j });

          assertEqual(expected, actual);

          assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query, { a: i, b: j }));
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test references with constants
////////////////////////////////////////////////////////////////////////////////

    testMultiRefConst1 : function () {
      var query = "LET x = 4 FOR v IN " + cn + " FILTER v.c == x SORT v.b RETURN [ v.b, v.c ]";
      var expected = [ [ 1, 4 ], [ 2, 4 ], [ 3, 4 ], [ 4, 4 ], [ 5, 4 ] ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "CalculationNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test references with constants
////////////////////////////////////////////////////////////////////////////////

    testMultiRefConst2 : function () {
      var expected = [ [ 3, 5 ] ];
      var actual = getQueryResults("LET x = 3 LET y = 5 FOR v IN " + cn + " FILTER v.a == x && v.b == y RETURN [ v.a, v.b ]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testMultiRefSingle1 : function () {
      for (let i = 1; i <= 5; ++i) {
        var expected = [ [ i, i ] ];
        var actual = getQueryResults("FOR v1 IN " + cn + " FOR v2 IN " + cn + " FILTER v1.c == @c && v1.b == 1 && v2.c == v1.c && v2.b == 1 RETURN [ v1.c, v2.c ]", { "c" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testMultiRefSingle2 : function () {
      for (let i = 1; i <= 5; ++i) {
        var expected = [ [ i, i ] ];
        var actual = getQueryResults("FOR v1 IN " + cn + " FOR v2 IN " + cn + " FILTER v1.c == @c && v1.b == 1 && v2.c == v1.c && v2.b == v1.b RETURN [ v1.c, v2.c ]", { "c" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testMultiRefSingle3 : function () {
      for (let i = 1; i <= 5; ++i) {
        var expected = [ [ i, i ] ];
        var actual = getQueryResults("FOR v1 IN " + cn + " FOR v2 IN " + cn + " FILTER @c == v1.c && 1 == v1.b && v1.c == v2.c && v1.b == v2.b RETURN [ v1.c, v2.c ]", { "c" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testMultiRefMulti1 : function () {
      for (let i = 1; i <= 5; ++i) {
        var expected = [ [ i, 1 ] ];
        var actual = getQueryResults("FOR v1 IN " + cn + " FOR v2 IN " + cn + " FILTER v1.c == @a && v1.b == 1 && v2.c == v1.c && v2.b == v1.b RETURN [ v1.a, v1.b ]", { "a" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testMultiRefMulti2 : function () {
      for (let i = 1; i <= 5; ++i) {
        var expected = [ [ i, 1 ] ];
        var actual = getQueryResults("FOR v1 IN " + cn + " FOR v2 IN " + cn + " FILTER @a == v1.c && 1 == v1.b && v1.c == v2.c && v1.b == v2.b RETURN [ v1.a, v1.b ]", { "a" : i });

        assertEqual(expected, actual);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access
////////////////////////////////////////////////////////////////////////////////

    testMultiRefMulti3 : function () {
      var query = "FOR v1 IN " + cn + " FILTER @a == v1.a && @b == v1.b RETURN [ v1.a, v1.b ]";
      var expected = [ [ 2, 3 ] ];
      var actual = getQueryResults(query, { "a": 2, "b": 3 });

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query, { a: 2, b: 3 }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testMultiRefFilterSame1 : function () {
      var query = "FOR a IN " + cn + " FILTER a.a == a.a SORT a.a RETURN a.a";
      var expected = [ 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5 ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testMultiRefFilterSame2 : function () {
      var query = "FOR a IN " + cn + " FILTER a.a == a.c SORT a.a RETURN a.a";
      var expected = [ 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5 ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testMultiRefNon1 : function () {
      var query = "FOR a IN " + cn + " FILTER a.a == 1 RETURN a.a";
      var expected = [ 1, 1, 1, 1, 1 ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testMultiRefNon2 : function () {
      var query = "FOR a IN " + cn + " FILTER a.d == a.a SORT a.a RETURN a.a";
      var expected = [ ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);

      // RocksDB uses Index For sort
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test ref access with filters on the same attribute
////////////////////////////////////////////////////////////////////////////////

    testMultiRefNon3 : function () {
      var query = "FOR a IN " + cn + " FILTER a.d == 1 SORT a.a RETURN a.a";
      var expected = [ ];
      var actual = getQueryResults(query);

      assertEqual(expected, actual);
      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "FilterNode", "CalculationNode", "SortNode", "CalculationNode", "ReturnNode" ], explain(query));
    },

    testMultiInvalidValuesinList : function () {
      var query = "FOR x IN @list FOR i IN " + cn + " FILTER i.c == x SORT i.c RETURN i.c";
      var bindParams = {
        list: [
          null,
          1, // Find this
          "blub/bla",
          "noKey",
          2, // And this
          123456,
          { "the": "foxx", "is": "wrapped", "in":"objects"},
          [15, "man", "on", "the", "dead", "mans", "chest"],
          3 // And this
        ]
      };
      assertEqual([ 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3], AQL_EXECUTE(query, bindParams).json);
    }

  };
}

function PersistentIndexOverlappingSuite () {
  const rulesDisabled = {optimizer: {rules: ["-all"]}};
  const rulesEnabled = {optimizer: {rules: ["+all"]}};

  return {

    setUp : function () {
      internal.db._drop(cn);
      let c = internal.db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["a"] });

      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({a: i});
      }
      c.insert(docs);
    },

    tearDown : function () {
      internal.db._drop(cn);
    },

    testLargeOverlappingRanges : function () {
      let query = `FOR x IN @@cn FILTER (1000 <= x.a && x.a < 2500) 
                                     || (1500 <= x.a && x.a < 3000)
                                     RETURN x`;

      let bindVars = {"@cn": cn};
      assertEqual(2000, AQL_EXECUTE(query, bindVars, rulesDisabled).json.length);
      assertEqual(2000, AQL_EXECUTE(query, bindVars, rulesEnabled).json.length);
    }

  };
}

function PersistentIndexSelectivityTestSuite () {
  return {
    setUp: function () {
      internal.db._drop(cn);
      let c = internal.db._create(cn);
      c.insert({value: 100000});
    },

    tearDown: function () {
      internal.db._drop(cn);
    },

    testUniqueSimple: function () {
      testAndValidate(query1, true, false, 0, false, false, 1);
    },

    testUniqueSimple2: function () {
      testAndValidate(query2, true, false, 0, false, false, 1);
    },

    testNonUniqueSimple: function () {
      testAndValidate(query1, false, false, 0, false, false, 1);
    },

    testNonUniqueSimple2: function () {
      testAndValidate(query2, false, false, 0, false, false, 1);
    },

    testUniqueSimpleManyDocs: function () {
      testAndValidate(query1, true, true, 10000, false, false, 1);
    },

    testUniqueSimpleManyDocs2: function () {
      testAndValidate(query2, true, true, 10000, false, false, 1);
    },

    testUniqueSimpleManyDocs3: function () {
      testAndValidate(query1, true, true, 50, false, false, 1);
    },

    testUniqueSimpleManyDocs4: function () {
      testAndValidate(query2, true, true, 50, false, false, 1);
    },

    testUniqueSimpleManyDocs5: function () {
      testAndValidate(query1, true, true, 50000, false, false, 1);
    },

    testUniqueSimpleManyDocs6: function () {
      testAndValidate(query2, true, true, 50000, false, false, 1);
    },

    testNonUniqueSimpleManyDocs: function () {
      testAndValidate(query1, false, true, 10000, false, false, 1);
    },

    testNonUniqueSimpleManyDocs2: function () {
      testAndValidate(query2, false, true, 10000, false, false, 1);
    },

    testNonUniqueSimpleManyDocs3: function () {
      testAndValidate(query1, false, true, 50, false, false, 1);
    },

    testNonUniqueSimpleManyDocs4: function () {
      testAndValidate(query2, false, true, 50, false, false, 1);
    },

    testNonUniqueSimpleManyDocs5: function () {
      testAndValidate(query1, false, true, 50000, false, false, 1);
    },

    testNonUniqueSimpleManyDocs6: function () {
      testAndValidate(query2, false, true, 50000, false, false, 1);
    },

    // in the tests below, even though the selectivity for the persistent index is very low, it would still be chosen, because
    // the cost would still be smaller than the cost of the primary index because, as it doesn't cover the FILTER,
    // it gets at least the default cost of the FILTER which will outnumber the cost for the persistent index
    testNonUniqueManyDocsIndexLowerSelectivityChoosePersistent: function () {
      insertMoreDocs(9, false);
      const numDocs = 1000;
      testAndValidate(query1, false, true, numDocs, true, false, (10/(numDocs + 10)));
    },

    testNonUniqueManyDocsIndexLowerSelectivityChoosePersistent2: function () {
      insertMoreDocs(9, false);
      const numDocs = 1000;
      testAndValidate(query2, false, true, numDocs, true, false, (10/(numDocs + 10)));
    },

    testNonUniqueManyDocsIndexLowerSelectivityChoosePersistent3: function () {
      insertMoreDocs(9, false);
      const numDocs = 50;
      testAndValidate(query1, false, true, numDocs, true, false, (10/(numDocs + 10)));
    },

    testNonUniqueManyDocsIndexLowerSelectivityChoosePersistent4: function () {
      insertMoreDocs(9, false);
      const numDocs = 50;
      testAndValidate(query2, false, true, numDocs, true, false, (10/(numDocs + 10)));
    },

    testNonUniqueManyDocsIndexLowerSelectivityChoosePersistent5: function () {
      insertMoreDocs(10000, false);
      const numDocs = 50000;
      testAndValidate(query1, false, true, numDocs, true, false, (10001/(numDocs + 10001)));
    },

    testNonUniqueManyDocsIndexLowerSelectivityChoosePersistent6: function () {
      insertMoreDocs(10000, false);
      const numDocs = 50000;
      testAndValidate(query2, false, true, numDocs, true, false, (10001/(numDocs + 10001)));
    },


    testNonUniqueManyDocsIndexLowSelectivityChooseOther: function () {
      const numDocs = 10000;
      testAndValidate(query1, false, true, numDocs, true, true, 1);
    },

    testNonUniqueManyDocsIndexLowSelectivityChooseOther2: function () {
      const numDocs = 10000;
      testAndValidate(query2, false, true, numDocs, true, true, 1);
    },

    testNonUniqueManyDocsIndexLowSelectivityChooseOther3: function () {
      const numDocs = 50;
      testAndValidate(query1, false, true, numDocs, true, true, 1);
    },

    testNonUniqueManyDocsIndexLowSelectivityChooseOther4: function () {
      const numDocs = 50;
      testAndValidate(query2, false, true, numDocs, true, true, 1);
    },

    testNonUniqueManyDocsIndexLowSelectivityChooseOther5: function () {
      const numDocs = 50000;
      testAndValidate(query1, false, true, numDocs, true, true, 1);
    },

    testNonUniqueManyDocsIndexLowSelectivityChooseOther6: function () {
      const numDocs = 50000;
      testAndValidate(query2, false, true, numDocs, true, true, 1);
    },

  };
}

jsunity.run(PersistentIndexTestSuite);
jsunity.run(PersistentIndexMultipleIndexesTestSuite);
jsunity.run(PersistentIndexOverlappingSuite);
jsunity.run(PersistentIndexSelectivityTestSuite);

return jsunity.done();
