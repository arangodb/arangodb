/*jshint strict: false, maxlen: 500 */
/*global require, assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index usage
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
var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 2000; ++i) {
        c.save({ _key: "test" + i, value: i });
      }

      c.ensureSkiplist("value");
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testValuePropagation : function () {
      var queries = [
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == 10 && i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == 10 FiLTER i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == 10 FiLTER j.value == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == j.value && i.value == 10 RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == j.value FILTER i.value == 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 10 FOR j IN " + c.name() + " FILTER i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 10 FOR j IN " + c.name() + " FILTER j.value == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER 10 == i.value && i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER 10 == i.value FiLTER i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER 10 == i.value FiLTER j.value == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == j.value && 10 == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FOR j IN " + c.name() + " FILTER i.value == j.value FILTER 10 == i.value RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 10 == i.value FOR j IN " + c.name() + " FILTER i.value == j.value RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 10 == i.value FOR j IN " + c.name() + " FILTER j.value == i.value RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var indexNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "IndexRangeNode") {
            ++indexNodes;
          }
        });

        assertNotEqual(-1, plan.rules.indexOf("propagate-constant-attributes"));
        assertEqual(2, indexNodes);

        var results = AQL_EXECUTE(query);
        assertEqual([ 10 ], results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testValuePropagationSubquery : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 10 " +
                  "LET sub1 = (FOR j IN " + c.name() + " FILTER j.value == i.value RETURN j.value) " +
                  "LET sub2 = (FOR j IN " + c.name() + " FILTER j.value == i.value RETURN j.value) " +
                  "LET sub3 = (FOR j IN " + c.name() + " FILTER j.value == i.value RETURN j.value) " +
                  "RETURN [ i.value, sub1, sub2, sub3 ]";

      var plan = AQL_EXPLAIN(query).plan;

      assertNotEqual(-1, plan.rules.indexOf("propagate-constant-attributes"));

      var results = AQL_EXECUTE(query);
      assertEqual([ [ 10, [ 10 ], [ 10 ], [ 10 ] ] ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testNoValuePropagationSubquery : function () {
      var query = "LET sub1 = (FOR j IN " + c.name() + " FILTER j.value == 10 RETURN j.value) " +
                  "LET sub2 = (FOR j IN " + c.name() + " FILTER j.value == 11 RETURN j.value) " +
                  "LET sub3 = (FOR j IN " + c.name() + " FILTER j.value == 12 RETURN j.value) " +
                  "RETURN [ sub1, sub2, sub3 ]";

      var plan = AQL_EXPLAIN(query).plan;

      assertEqual(-1, plan.rules.indexOf("propagate-constant-attributes"));

      var results = AQL_EXECUTE(query);
      assertEqual([ [ [ 10 ], [ 11 ], [ 12 ] ] ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexSimple : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value >= 10 SORT i.value LIMIT 10 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexJoin : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 8 FOR j IN " + c.name() + " FILTER i.value == j.value RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var indexes = 0;
      plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          ++indexes;
        }
        return node.type;
      });
      assertEqual(2, indexes);

      var results = AQL_EXECUTE(query);
      assertEqual([ 8 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexJoinJoin : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 8 FOR j IN " + c.name() + " FILTER i._key == j._key RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var indexes = 0;
      plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          ++indexes;
        }
        return node.type;
      });
      assertEqual(2, indexes);

      var results = AQL_EXECUTE(query);
      assertEqual([ 8 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexJoinJoinJoin : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 8 FOR j IN " + c.name() + " FILTER i.value == j.value FOR k IN " + c.name() + " FILTER j.value == k.value RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var indexes = 0;
      plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          ++indexes;
        }
        return node.type;
      });
      assertEqual(3, indexes);

      var results = AQL_EXECUTE(query);
      assertEqual([ 8 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexSubquery : function () {
      var query = "LET results = (FOR i IN " + c.name() + " FILTER i.value >= 10 SORT i.value LIMIT 10 RETURN i.value) RETURN results";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual("SubqueryNode", nodeTypes[1], query);

      var subNodeTypes = plan.nodes[1].subquery.nodes.map(function(node) {
        return node.type;
      });
      assertNotEqual(-1, subNodeTypes.indexOf("IndexRangeNode"), query);
      assertEqual(-1, subNodeTypes.indexOf("SortNode"), query);
      assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 ], results.json[0], query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexSubSubquery : function () {
      var query = "FOR i IN " + c.name() + " LIMIT 1 RETURN (FOR j IN " + c.name() + " FILTER j._key == i._key RETURN j.value)";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      var idx = nodeTypes.indexOf("SubqueryNode");
      assertNotEqual(-1, idx, query);

      var subNodeTypes = plan.nodes[idx].subquery.nodes.map(function(node) {
        return node.type;
      });
      assertNotEqual(-1, subNodeTypes.indexOf("IndexRangeNode"), query);
      assertEqual(-1, subNodeTypes.indexOf("SortNode"), query);
      assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

      var results = AQL_EXECUTE(query);
      assertTrue(results.stats.scannedFull > 0); // for the outer query
      assertTrue(results.stats.scannedIndex > 0); // for the inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultipleSubqueries : function () {
      var query = "LET a = (FOR x IN " + c.name() + " FILTER x._key == 'test1' RETURN x._key) " + 
                  "LET b = (FOR x IN " + c.name() + " FILTER x._key == 'test2' RETURN x._key) " + 
                  "LET c = (FOR x IN " + c.name() + " FILTER x._key == 'test3' RETURN x._key) " + 
                  "LET d = (FOR x IN " + c.name() + " FILTER x._key == 'test4' RETURN x._key) " + 
                  "LET e = (FOR x IN " + c.name() + " FILTER x._key == 'test5' RETURN x._key) " + 
                  "LET f = (FOR x IN " + c.name() + " FILTER x._key == 'test6' RETURN x._key) " + 
                  "LET g = (FOR x IN " + c.name() + " FILTER x._key == 'test7' RETURN x._key) " + 
                  "LET h = (FOR x IN " + c.name() + " FILTER x._key == 'test8' RETURN x._key) " + 
                  "LET i = (FOR x IN " + c.name() + " FILTER x._key == 'test9' RETURN x._key) " + 
                  "LET j = (FOR x IN " + c.name() + " FILTER x._key == 'test10' RETURN x._key) " +
                  "RETURN [ a, b, c, d, e, f, g, h, i, j ]";


      var explain = AQL_EXPLAIN(query);
      var plan = explain.plan;

      var walker = function (nodes, func) {
        nodes.forEach(function(node) {
          if (node.type === "SubqueryNode") {
            walker(node.subquery.nodes, func);
          }
          func(node);
        });
      };

      var indexNodes = 0, collectionNodes = 0;
      walker(plan.nodes, function (node) {
        if (node.type === "IndexRangeNode") {
          ++indexNodes;
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(10, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex); 
      assertEqual([ [ [ 'test1' ], [ 'test2' ], [ 'test3' ], [ 'test4' ], [ 'test5' ], [ 'test6' ], [ 'test7' ], [ 'test8' ], [ 'test9' ], [ 'test10' ] ] ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultipleSubqueriesMultipleIndexes : function () {
      c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "LET a = (FOR x IN " + c.name() + " FILTER x.value == 1 RETURN x._key) " + 
                  "LET b = (FOR x IN " + c.name() + " FILTER x.value == 2 RETURN x._key) " + 
                  "LET c = (FOR x IN " + c.name() + " FILTER x.value == 3 RETURN x._key) " + 
                  "LET d = (FOR x IN " + c.name() + " FILTER x.value == 4 RETURN x._key) " + 
                  "LET e = (FOR x IN " + c.name() + " FILTER x.value == 5 RETURN x._key) " + 
                  "LET f = (FOR x IN " + c.name() + " FILTER x.value == 6 RETURN x._key) " + 
                  "LET g = (FOR x IN " + c.name() + " FILTER x.value == 7 RETURN x._key) " + 
                  "LET h = (FOR x IN " + c.name() + " FILTER x.value == 8 RETURN x._key) " + 
                  "LET i = (FOR x IN " + c.name() + " FILTER x.value == 9 RETURN x._key) " + 
                  "LET j = (FOR x IN " + c.name() + " FILTER x.value == 10 RETURN x._key) " +
                  "RETURN [ a, b, c, d, e, f, g, h, i, j ]";


      var explain = AQL_EXPLAIN(query);
      var plan = explain.plan;

      var walker = function (nodes, func) {
        nodes.forEach(function(node) {
          if (node.type === "SubqueryNode") {
            walker(node.subquery.nodes, func);
          }
          func(node);
        });
      };

      var indexNodes = 0, collectionNodes = 0;
      walker(plan.nodes, function (node) {
        if (node.type === "IndexRangeNode") {
          ++indexNodes;
          if (indexNodes <= 5) {
            assertEqual("hash", node.index.type);
          }
          else {
            assertEqual("skiplist", node.index.type);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(10, indexNodes);
      assertEqual(32, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex); 
      assertEqual([ [ [ 'test1' ], [ 'test2' ], [ 'test3' ], [ 'test4' ], [ 'test5' ], [ 'test6' ], [ 'test7' ], [ 'test8' ], [ 'test9' ], [ 'test10' ] ] ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultipleSubqueriesHashIndexes : function () {
      c.dropIndex(c.getIndexes()[1]); // drop skiplist index
      c.ensureHashIndex("value");
      var query = "LET a = (FOR x IN " + c.name() + " FILTER x.value == 1 RETURN x._key) " + 
                  "LET b = (FOR x IN " + c.name() + " FILTER x.value == 2 RETURN x._key) " + 
                  "LET c = (FOR x IN " + c.name() + " FILTER x.value == 3 RETURN x._key) " + 
                  "LET d = (FOR x IN " + c.name() + " FILTER x.value == 4 RETURN x._key) " + 
                  "LET e = (FOR x IN " + c.name() + " FILTER x.value == 5 RETURN x._key) " + 
                  "LET f = (FOR x IN " + c.name() + " FILTER x.value == 6 RETURN x._key) " + 
                  "LET g = (FOR x IN " + c.name() + " FILTER x.value == 7 RETURN x._key) " + 
                  "LET h = (FOR x IN " + c.name() + " FILTER x.value == 8 RETURN x._key) " + 
                  "LET i = (FOR x IN " + c.name() + " FILTER x.value == 9 RETURN x._key) " + 
                  "LET j = (FOR x IN " + c.name() + " FILTER x.value == 10 RETURN x._key) " +
                  "RETURN [ a, b, c, d, e, f, g, h, i, j ]";

      var explain = AQL_EXPLAIN(query);
      var plan = explain.plan;

      var walker = function (nodes, func) {
        nodes.forEach(function(node) {
          if (node.type === "SubqueryNode") {
            walker(node.subquery.nodes, func);
          }
          func(node);
        });
      };

      var indexNodes = 0, collectionNodes = 0;
      walker(plan.nodes, function (node) {
        if (node.type === "IndexRangeNode") {
          ++indexNodes;
          assertEqual("hash", node.index.type);
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(10, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex); 
      assertEqual([ [ [ 'test1' ], [ 'test2' ], [ 'test3' ], [ 'test4' ], [ 'test5' ], [ 'test6' ], [ 'test7' ], [ 'test8' ], [ 'test9' ], [ 'test10' ] ] ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testJoinMultipleIndexes : function () {
      c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "FOR i IN " + c.name() + " FILTER i.value < 10 FOR j IN " + c.name() + " FILTER j.value == i.value RETURN j._key";

      var explain = AQL_EXPLAIN(query);
      var plan = explain.plan;

      var collectionNodes = 0, indexNodes = 0;
      plan.nodes.forEach(function(node) {
        if (node.type === "IndexRangeNode") {
          ++indexNodes;
          if (indexNodes === 1) {
            // skiplist must be used for the first FOR
            assertEqual("skiplist", node.index.type);
            assertEqual("i", node.outVariable.name);
          }
          else {
            // second FOR should use a hash index
            assertEqual("hash", node.index.type);
            assertEqual("j", node.outVariable.name);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(2, indexNodes);
      assertEqual(4, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex); 
      assertEqual([ 'test0', 'test1', 'test2', 'test3', 'test4', 'test5', 'test6', 'test7', 'test8', 'test9' ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testJoinRangesMultipleIndexes : function () {
      c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "FOR i IN " + c.name() + " FILTER i.value < 5 FOR j IN " + c.name() + " FILTER j.value < i.value RETURN j._key";

      var explain = AQL_EXPLAIN(query);
      var plan = explain.plan;

      var collectionNodes = 0, indexNodes = 0;
      plan.nodes.forEach(function(node) {
        if (node.type === "IndexRangeNode") {
          ++indexNodes;
          // skiplist must be used for both FORs
          assertEqual("skiplist", node.index.type);
          if (indexNodes === 1) {
            assertEqual("i", node.outVariable.name);
          }
          else {
            assertEqual("j", node.outVariable.name);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(2, indexNodes);
      assertEqual(2, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex); 
      assertEqual([ 'test0', 'test0', 'test1', 'test0', 'test1', 'test2', 'test0', 'test1', 'test2', 'test3' ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testTripleJoin : function () {
      c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "FOR i IN " + c.name() + " FILTER i.value == 4 FOR j IN " + c.name() + " FILTER j.value == i.value FOR k IN " + c.name() + " FILTER k.value < j.value RETURN k._key";

      var explain = AQL_EXPLAIN(query);
      var plan = explain.plan;

      var collectionNodes = 0, indexNodes = 0;
      plan.nodes.forEach(function(node) {
        if (node.type === "IndexRangeNode") {
          ++indexNodes;
          if (indexNodes === 1) {
            assertEqual("hash", node.index.type);
            assertEqual("i", node.outVariable.name);
          }
          else if (indexNodes === 2) {
            assertEqual("hash", node.index.type);
            assertEqual("j", node.outVariable.name);
          }
          else {
            assertEqual("skiplist", node.index.type);
            assertEqual("k", node.outVariable.name);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(3, indexNodes);
      assertEqual(18, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex); 
      assertEqual([ 'test0', 'test1', 'test2', 'test3' ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSubqueryMadness : function () {
      c.ensureHashIndex("value"); // now we have a hash and a skiplist index
      var query = "LET a = (FOR x IN " + c.name() + " FILTER x.value == 1 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "LET b = (FOR x IN " + c.name() + " FILTER x.value == 2 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "LET c = (FOR x IN " + c.name() + " FILTER x.value == 3 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "LET d = (FOR x IN " + c.name() + " FILTER x.value == 4 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "LET e = (FOR x IN " + c.name() + " FILTER x.value == 5 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "LET f = (FOR x IN " + c.name() + " FILTER x.value == 6 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "LET g = (FOR x IN " + c.name() + " FILTER x.value == 7 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "LET h = (FOR x IN " + c.name() + " FILTER x.value == 8 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "LET i = (FOR x IN " + c.name() + " FILTER x.value == 9 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "LET j = (FOR x IN " + c.name() + " FILTER x.value == 10 FOR y IN " + c.name() + " FILTER y.value == x.value RETURN x._key) " + 
                  "RETURN [ a, b, c, d, e, f, g, h, i, j ]";

      var explain = AQL_EXPLAIN(query);
      var plan = explain.plan;
      
      var walker = function (nodes, func) {
        nodes.forEach(function(node) {
          if (node.type === "SubqueryNode") {
            walker(node.subquery.nodes, func);
          }
          func(node);
        });
      };

      var indexNodes = 0, collectionNodes = 0;
      walker(plan.nodes, function (node) {
        if (node.type === "IndexRangeNode") {
          ++indexNodes;
          if (indexNodes < 5) {
            assertEqual("hash", node.index.type);
          }
          else {
            assertEqual("skiplist", node.index.type);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(20, indexNodes);
      assertEqual(64, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex); 
      assertEqual([ [ [ 'test1' ], [ 'test2' ], [ 'test3' ], [ 'test4' ], [ 'test5' ], [ 'test6' ], [ 'test7' ], [ 'test8' ], [ 'test9' ], [ 'test10' ] ] ], results.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrPrimary : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._key == 'test1' || i._key == 'test9' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("primary", node.index.type);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testPrimaryIndexDynamic : function () {
      var queries = [
        [ "LET a = PASSTHRU('test35') FOR i IN " + c.name() + " FILTER i._key IN [ a ] RETURN i.value", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN [ PASSTHRU('test35') ] RETURN i.value", [ 35 ] ],
        [ "LET a = PASSTHRU('test35') FOR i IN " + c.name() + " FILTER i._key IN [ a, a, a ] RETURN i.value", [ 35 ] ],
        [ "LET a = PASSTHRU('test35') FOR i IN " + c.name() + " FILTER i._key IN [ 'test35', 'test36' ] RETURN i.value", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU('test35'), b = PASSTHRU('test36'), c = PASSTHRU('test-9') FOR i IN " + c.name() + " FILTER i._key IN [ b, b, a, b, c ] RETURN i.value", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU('test35'), b = PASSTHRU('test36'), c = PASSTHRU('test37') FOR i IN " + c.name() + " FILTER i._key IN [ a, b, c ] RETURN i.value", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU('test35'), b = PASSTHRU('test36'), c = PASSTHRU('test37') FOR i IN " + c.name() + " FILTER i._key IN [ a ] || i._key IN [ b, c ] RETURN i.value", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU('test35'), b = PASSTHRU('test36'), c = PASSTHRU('test37'), d = PASSTHRU('test35') FOR i IN " + c.name() + " FILTER i._key IN [ a, b, c, d ] || i._key IN [ a, b, c, d ] RETURN i.value", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU('test35') FOR i IN " + c.name() + " FILTER i._key == a RETURN i.value", [ 35 ] ],
        [ "LET a = PASSTHRU('test35') FOR i IN " + c.name() + " FILTER i._key == a || i._key == a RETURN i.value", [ 35 ] ],
        [ "LET a = PASSTHRU('test35'), b = PASSTHRU('test36') FOR i IN " + c.name() + " FILTER i._key == a || i._key == b RETURN i.value", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU('test35'), b = PASSTHRU('test36'), c = PASSTHRU('test37'), d = PASSTHRU('test35') FOR i IN " + c.name() + " FILTER i._key == a || i._key == b || i._key == c || i._key == d RETURN i.value", [ 35, 36, 37 ] ] 
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure the index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value), query);
        });
    
        assertTrue(results.stats.scannedIndex < 10);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrHash : function () {
      c.ensureHashIndex("value");
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("hash", node.index.type);
          assertFalse(node.index.unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrUniqueHash : function () {
      c.ensureUniqueConstraint("value");
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("hash", node.index.type);
          assertTrue(node.index.unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrSkiplist : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertFalse(node.index.unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrUniqueSkiplist : function () {
      c.ensureUniqueSkiplist("value");
      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertTrue(node.index.unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndex : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value2 == 1 RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1 ], results.json, query);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrHashMultipleRanges : function () {
      c.ensureHashIndex("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 2 && i.value3 == 2) || (i.value2 == 3 && i.value3 == 3) RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("hash", node.index.type);
          assertFalse(node.index.unique);
          assertEqual([ "value2", "value3" ], node.index.fields);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 2, 3 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrSkiplistMultipleRanges : function () {
      c.ensureSkiplist("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 2 && i.value3 == 2) || (i.value2 == 3 && i.value3 == 3) RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertFalse(node.index.unique);
          assertEqual([ "value2", "value3" ], node.index.fields);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 2, 3 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrHashMultipleRangesPartialNoIndex1 : function () {
      c.ensureHashIndex("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value3 == 1) || (i.value2 == 2) RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrHashMultipleRangesPartialNoIndex2 : function () {
      c.ensureHashIndex("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value3 == 1) || (i.value3 == 2) RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

/* TODO
    testIndexOrSkiplistMultipleRangesPartialIndex : function () {
      c.ensureSkiplist("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value3 == 1) || (i.value2 == 2) RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertFalse(node.index.unique);
          assertEqual([ "value2", "value3" ], node.index.fields);
        }
        return node.type;
      });
      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },
  */

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrSkiplistMultipleRangesPartialNoIndex : function () {
      c.ensureSkiplist("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value3 == 1) || (i.value3 == 2) RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });
      assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndUseIndex : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value2 == 1 RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
    },
 
////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////
    
    testIndexOrNoIndexBecauseOfDifferentAttributes : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: 1 } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value == 1 || i.value == 2) || i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 || (i.value == 1 || i.value == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || (i.value == 2 || i.value2 == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 2 || i.value == 1) || i.value == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value == 1 && i.value2 == 1) || (i.value == 2 || i.value3 != 1) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value3 != 1 || i.value == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value == 1 && i.value2 == 1) || (i.value == 2 || i.value3 == 0) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value3 == 0 || i.value == 2) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(2, results.json.length); 
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfOperator : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: 1 } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value2 != i.value RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 != i.value || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 1 && PASSTHRU(i.value2) == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 1 FILTER PASSTHRU(i.value2) == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER PASSTHRU(i.value2) == 2 && i.value != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER PASSTHRU(i.value2) == 2 FILTER i.value != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 != 2 && PASSTHRU(i.value) == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 != 2 FILTER PASSTHRU(i.value) == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER PASSTHRU(i.value) == 1 && i.value2 != 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER PASSTHRU(i.value) == 1 FILTER i.value2 != 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value3 != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 != 1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 != 1 || i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value3 != 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length, query); 
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteFunctions : function () {
      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() != -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() <= 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && RAND() < 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && PASSTHRU(true) == true RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && 1 + PASSTHRU(2) == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && 1 + PASSTHRU(2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() != -1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() <= 10 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() < 10 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + PASSTHRU(2) == 3 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + PASSTHRU(2) && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value IN [ 1 ] && RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 FILTER i.value IN [ 1 ] RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() != -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() <= 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER RAND() < 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER PASSTHRU(true) == true RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER 1 + PASSTHRU(2) == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER 1 + PASSTHRU(2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() != -1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() <= 10 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() < 10 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + PASSTHRU(2) == 3 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + PASSTHRU(2) FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value IN [ 1 ] FILTER RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 FILTER i.value IN [ 1 ] RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length); 
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteReferences : function () {
      var queries = [
        "LET a = 1 FOR i IN " + c.name() + " FILTER i.value == 1 && a == 1 RETURN i.value",
        "LET a = 1 FOR i IN " + c.name() + " FILTER a == 1 && i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER i.value == 1 && a == 1 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER a == 1 && i.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER i.value == 1 && a.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER a.value == 1 && i.value == 1 RETURN i.value",
        "LET a = PASSTHRU({ value: 1 }) FOR i IN " + c.name() + " FILTER i.value == 1 && a.value == 1 RETURN i.value",
        "LET a = PASSTHRU({ value: 1 }) FOR i IN " + c.name() + " FILTER a.value == 1 && i.value == 1 RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER i.value == 1 && (1 IN sub) RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER (1 IN sub) && i.value == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER i.value == 1 && j == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER j == 1 && i.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER i.value == 1 && j.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER j.value == 1 && i.value == 1 RETURN i.value",
        "LET a = 1 FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a == 1 RETURN i.value",
        "LET a = 1 FOR i IN " + c.name() + " FILTER a == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a == 1 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER a == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER a.value == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = PASSTHRU({ value: 1 }) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a.value == 1 RETURN i.value",
        "LET a = PASSTHRU({ value: 1 }) FOR i IN " + c.name() + " FILTER a.value == 1 FILTER i.value == 1 RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER (1 IN sub) RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER (1 IN sub) FILTER i.value == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER i.value == 1 FILTER j == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER j == 1 FILTER i.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER i.value == 1 FILTER j.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER j.value == 1 FILTER i.value == 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length); 
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteOtherCollection : function () {
      var queries = [
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER i.value == 1 && j.value == 1 LIMIT 2000 RETURN i.value",
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER j.value == 1 && i.value == 1 LIMIT 2000 RETURN i.value",
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER i.value == 1 FILTER j.value == 1 LIMIT 2000 RETURN i.value",
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER j.value == 1 FILTER i.value == 1 LIMIT 2000 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length); 
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteOperators : function () {
      var queries = [
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER i.value == 1 && a != 2 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER a != 2 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value != 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 2 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value - 1 == 0 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 == 0 && i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 && a + 1 == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a + 1 == 1 && i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 && a IN [ 0, 1 ] RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a IN [ 0, 1 ] && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && NOT (i.value != 1) RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || NOT NOT (i.value == 1) RETURN i",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a != 2 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER a != 2 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER i.value != 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 2 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER i.value - 1 == 0 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 == 0 FILTER i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a + 1 == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a + 1 == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a IN [ 0, 1 ] RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a IN [ 0, 1 ] FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 3 && (i.value != 1 || i.value != 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 3 FILTER (i.value != 1 || i.value != 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER NOT (i.value != 1) RETURN i",
        "FOR i IN " + c.name() + " FILTER (i.value != 1 || i.value != 2) && i.value == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value != 1 || i.value != 2) FILTER i.value == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 != 1 || i.value2 != 2) && i.value == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 != 1 || i.value2 != 2) FILTER i.value == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 3 && (i.value2 != 1 || i.value2 != 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 3 FILTER (i.value2 != 1 || i.value2 != 2) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length); 
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },
 
////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////
 
    testIndexAndDespiteOr : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: 1 } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER (i.value == 1 && i.value2 == 1) || (i.value == 2 && i.value2 == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value2 == 2 && i.value == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value3 != 2 && i.value == 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value == 1 && i.value2 == 1) || (i.value == 2 && i.value3 != 2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value == 1) || (i.value3 == 1 && i.value == 2) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(2, results.json.length, query); 
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndDespiteOperatorsEmpty : function () {
      var queries = [
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER i.value == 1 && a != 1 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER a != 1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 1 && i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 && i.value - 1 == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 == 1 && i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 && a + 1 == 0 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a + 1 == 0 && i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 && a IN [ 1 ] RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a IN [ 1 ] && i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a != 1 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER a != 1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER i.value != 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != 1 FILTER i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER i.value - 1 == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 == 1 FILTER i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a + 1 == 0 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a + 1 == 0 FILTER i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 FILTER a IN [ 1 ] RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a IN [ 1 ] FILTER i.value == 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(0, results.json.length); 
        assertTrue(results.stats.scannedIndex >= 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfFunctions : function () {
      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() != -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() <= 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || RAND() < 10 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || PASSTHRU(true) == true RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || 1 + PASSTHRU(2) == 3 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || 1 + PASSTHRU(2) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() != -1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() <= 10 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() < 10 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + PASSTHRU(2) == 3 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER 1 + PASSTHRU(2) || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value IN [ 1, 2 ] || RAND() >= -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER RAND() >= -1 || i.value IN [ 1, 2 ] RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(2000, results.json.length); 
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfReferences : function () {
      var queries = [
        "LET a = 1 FOR i IN " + c.name() + " FILTER i.value == 1 || a == 1 RETURN i.value",
        "LET a = 1 FOR i IN " + c.name() + " FILTER a == 1 || i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER i.value == 1 || a == 1 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER a == 1 || i.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER i.value == 1 || a.value == 1 RETURN i.value",
        "LET a = { value: 1 } FOR i IN " + c.name() + " FILTER a.value == 1 || i.value == 1 RETURN i.value",
        "LET a = PASSTHRU({ value: 1 }) FOR i IN " + c.name() + " FILTER i.value == 1 || a.value == 1 RETURN i.value",
        "LET a = PASSTHRU({ value: 1 }) FOR i IN " + c.name() + " FILTER a.value == 1 || i.value == 1 RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER i.value == 1 || 1 IN sub RETURN i.value",
        "LET sub = (FOR x IN [ 1, 2, 3 ] RETURN x) FOR i IN " + c.name() + " FILTER 1 IN sub || i.value == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER i.value == 1 || j == 1 RETURN i.value",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER j == 1 || i.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER i.value == 1 || j.value == 1 RETURN i.value",
        "FOR j IN [ { value: 1 } ] FOR i IN " + c.name() + " FILTER j.value == 1 || i.value == 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(2000, results.json.length); 
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfOtherCollection : function () {
      var queries = [
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER i.value == 1 || j.value == 1 LIMIT 2000 RETURN i.value",
        "FOR j IN " + c.name() + " FOR i IN " + c.name() + " FILTER j.value == 1 || i.value == 1 LIMIT 2000 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(2000, results.json.length); 
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexBecauseOfOperators : function () {
      var queries = [
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER i.value == 1 || a != 2 RETURN i.value",
        "LET a = PASSTHRU(1) FOR i IN " + c.name() + " FILTER a != 2 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value != -1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value != -1 || i.value == 1 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value == 1 || i.value - 1 != 0 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value - 1 != 0 || i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 || a + 1 == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a + 1 == 1 || i.value == 1 RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER i.value == 1 || a IN [ 0, 1 ] RETURN i.value",
        "LET a = PASSTHRU(0) FOR i IN " + c.name() + " FILTER a IN [ 0, 1 ] || i.value == 1 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(2000, results.json.length); 
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndSkiplistIndexedNonIndexed : function () {
      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length); 
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndHashIndexedNonIndexed : function () {
      // drop the skiplist index
      c.dropIndex(c.getIndexes()[1]);
      c.ensureHashIndex("value");
      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length); 
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndAlternativeHashAndSkiplistIndexedNonIndexed : function () {
      // create a competitor index
      c.ensureHashIndex("value");

      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length); 
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndSkiplistPartialIndexedNonIndexed : function () {
      // drop the skiplist index
      c.dropIndex(c.getIndexes()[1]);

      // create an alternative index
      c.ensureSkiplist("value", "value3");

      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length); 
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexAndAlternativeSkiplistPartialIndexedNonIndexed : function () {
      // create a competitor index
      c.ensureSkiplist("value", "value3");

      // value2 is not indexed
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 FILTER i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value == 2 && i.value2 != 1 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 FILTER i.value == 2 RETURN i",
        "FOR i IN " + c.name() + " FILTER i.value2 != 1 && i.value == 2 RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length); 
        assertEqual(2, results.json[0].value);
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexNotNoIndex : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value == 1 || i.value == 2) RETURN i", 1998 ],
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value == 1 && i.value == 2) RETURN i", 2000 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 || NOT (i.value == -1) RETURN i", 2000 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length); 
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexNotUseIndex : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value == 2) && i.value == 1 RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 && NOT (i.value == 2) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 && NOT (i.value != 1) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value != 1) && i.value == 1 RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 || NOT (i.value != -1) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value == 2) FILTER i.value == 1 RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER NOT (i.value == 2) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value == 1 FILTER NOT (i.value != 1) RETURN i", 1 ],
        [ "FOR i IN " + c.name() + " FILTER NOT (i.value != 1) FILTER i.value == 1 RETURN i", 1 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length); 
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexUsageIn : function () {
      c.ensureHashIndex("value2");
      c.ensureSkiplist("value3");
      
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN PASSTHRU([]) RETURN i.value2", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN PASSTHRU([23]) RETURN i.value2", [ 23 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN PASSTHRU([23, 42]) RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = PASSTHRU(42) FOR i IN " + c.name() + " FILTER i.value2 IN APPEND([ 23 ], a) RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = PASSTHRU(23) FOR i IN " + c.name() + " FILTER i.value2 IN APPEND([ 23 ], a) RETURN i.value2", [ 23 ] ],
        [ "LET a = PASSTHRU(23) FOR i IN " + c.name() + " FILTER i.value2 IN APPEND([ a ], a) RETURN i.value2", [ 23 ] ],
        [ "LET a = PASSTHRU(23), b = PASSTHRU(42) FOR i IN " + c.name() + " FILTER i.value2 IN [ a, b ] RETURN i.value2", [ 23, 42 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN [23] RETURN i.value2", [ 23 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN [23, 42] RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = PASSTHRU([]) FOR i IN " + c.name() + " FILTER i.value2 IN a RETURN i.value2", [ ] ],
        [ "LET a = PASSTHRU([23]) FOR i IN " + c.name() + " FILTER i.value2 IN a RETURN i.value2", [ 23 ] ],
        [ "LET a = PASSTHRU([23, 42]) FOR i IN " + c.name() + " FILTER i.value2 IN a RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = PASSTHRU(42) FOR i IN " + c.name() + " FILTER i.value3 IN APPEND([ 23 ], a) RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = PASSTHRU(23) FOR i IN " + c.name() + " FILTER i.value3 IN APPEND([ 23 ], a) RETURN i.value2", [ 23 ] ],
        [ "LET a = PASSTHRU(23) FOR i IN " + c.name() + " FILTER i.value3 IN APPEND([ a ], a) RETURN i.value2", [ 23 ] ],
        [ "LET a = PASSTHRU(23), b = PASSTHRU(42) FOR i IN " + c.name() + " FILTER i.value3 IN [ a, b ] RETURN i.value2", [ 23, 42 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN PASSTHRU([]) RETURN i.value2", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN PASSTHRU([23]) RETURN i.value2", [ 23 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN PASSTHRU([23, 42]) RETURN i.value2", [ 23, 42 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN [23] RETURN i.value2", [ 23 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN [23, 42] RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = PASSTHRU([]) FOR i IN " + c.name() + " FILTER i.value3 IN a RETURN i.value2", [ ] ],
        [ "LET a = PASSTHRU([23]) FOR i IN " + c.name() + " FILTER i.value3 IN a RETURN i.value2", [ 23 ] ],
        [ "LET a = PASSTHRU([23, 42]) FOR i IN " + c.name() + " FILTER i.value3 IN a RETURN i.value2", [ 23, 42 ] ],
        [ "LET a = PASSTHRU('test42') FOR i IN " + c.name() + " FILTER i._key IN APPEND([ 'test23' ], a) RETURN i._key", [ 'test23', 'test42' ] ],
        [ "LET a = PASSTHRU('test23') FOR i IN " + c.name() + " FILTER i._key IN APPEND([ 'test23' ], a) RETURN i._key", [ 'test23' ] ],
        [ "LET a = PASSTHRU('test23') FOR i IN " + c.name() + " FILTER i._key IN APPEND([ a ], a) RETURN i._key", [ 'test23' ] ],
        [ "LET a = PASSTHRU('test23'), b = PASSTHRU('test42') FOR i IN " + c.name() + " FILTER i._key IN [ a, b ] RETURN i._key", [ 'test23', 'test42' ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN PASSTHRU([]) RETURN i._key", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN PASSTHRU(['test23']) RETURN i._key", [ 'test23' ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN PASSTHRU(['test23', 'test42']) RETURN i._key", [ 'test23', 'test42' ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN ['test23'] RETURN i._key", [ 'test23' ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN ['test23', 'test42'] RETURN i._key", [ 'test23', 'test42' ] ],
        [ "LET a = PASSTHRU([]) FOR i IN " + c.name() + " FILTER i._key IN a RETURN i._key", [ ] ],
        [ "LET a = PASSTHRU(['test23']) FOR i IN " + c.name() + " FILTER i._key IN a RETURN i._key", [ 'test23' ] ],
        [ "LET a = PASSTHRU(['test23', 'test42']) FOR i IN " + c.name() + " FILTER i._key IN a RETURN i._key", [ 'test23', 'test42' ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1].length, results.json.length, query); 
        assertEqual(query[1].sort(), results.json.sort(), query);
        assertTrue(results.stats.scannedIndex >= 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexUsageInNoIndex : function () {
      c.ensureHashIndex("value2");
      c.ensureSkiplist("value3");
      
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN [] RETURN i.value2", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value3 IN [] RETURN i.value2", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i._key IN [] RETURN i.value2", [ ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("NoResultsNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(0, results.json.length); 
        assertEqual(0, results.stats.scannedIndex);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHash : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("hash", node.index.type);
          assertFalse(node.index.unique);
          assertTrue(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 2 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseUniqueHash : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureUniqueConstraint("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("hash", node.index.type);
          assertTrue(node.index.unique);
          assertTrue(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 2 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashCollisions : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("hash", node.index.type);
          assertFalse(node.index.unique);
          assertTrue(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(20, results.json.length);
      assertEqual(20, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashCollisionsOr : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value2 == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("hash", node.index.type);
          assertFalse(node.index.unique);
          assertTrue(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(40, results.json.length);
      assertEqual(40, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashDynamic : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var queries = [
        "LET a = PASSTHRU(null) FOR i IN " + c.name() + " FILTER i.value2 == a RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == PASSTHRU(null) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == PASSTHRU(null) || i.value2 == PASSTHRU(null) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // must not use an index
        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      // must not use an index
      assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashesNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureHashIndex("value2", { sparse: true });
      c.ensureHashIndex("value2", { sparse: false });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("hash", node.index.type);
          assertFalse(node.index.unique);
          assertFalse(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashMulti : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 && i.value2 == 2 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          if (node.type === "IndexRangeNode") {
            assertEqual("hash", node.index.type);
            assertFalse(node.index.unique);
            assertTrue(node.index.sparse);
          }
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query);
        assertEqual([ 2 ], results.json, query);
        assertEqual(1, results.stats.scannedIndex);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashMultiMissing : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query);
        assertEqual([ 2 ], results.json, query);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashMultiNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == null RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query);
        assertEqual([ 0 ], results.json, query);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseHashesMultiNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3", { sparse: true });
      c.ensureHashIndex("value2", "value3", { sparse: false });

      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("hash", node.index.type);
          assertFalse(node.index.unique);
          assertFalse(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 0 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplist : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertFalse(node.index.unique);
          assertTrue(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 2 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseUniqueSkiplist : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureUniqueSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertTrue(node.index.unique);
          assertTrue(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 2 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistCollisions : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertFalse(node.index.unique);
          assertTrue(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(20, results.json.length);
      assertEqual(20, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistCollisionsOr : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value2 == 9 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertFalse(node.index.unique);
          assertTrue(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(40, results.json.length);
      assertEqual(40, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistDynamic : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var queries = [
        "LET a = PASSTHRU(null) FOR i IN " + c.name() + " FILTER i.value2 == a RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == PASSTHRU(null) RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == PASSTHRU(null) || i.value2 == PASSTHRU(null) RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // must not use an index
        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query);
        assertEqual(1, results.json.length);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      // must not use an index
      assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistsNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value % 100 } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });
      c.ensureSkiplist("value2", { sparse: false });
      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertFalse(node.index.unique);
          assertFalse(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistMulti : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 && i.value2 == 2 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          if (node.type === "IndexRangeNode") {
            assertEqual("skiplist", node.index.type);
            assertFalse(node.index.unique);
            assertTrue(node.index.sparse);
          }
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query);
        assertEqual([ 2 ], results.json, query);
        assertEqual(1, results.stats.scannedIndex);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistMultiMissing : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query);
        assertEqual([ 2 ], results.json, query);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistMultiNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value2 == null RETURN i.value",
        "FOR i IN " + c.name() + " FILTER i.value3 == null RETURN i.value"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query);
        assertEqual([ 0 ], results.json, query);
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sparse indexes
////////////////////////////////////////////////////////////////////////////////

    testSparseSkiplistsMultiNull : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " FILTER i.value >= 1 UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", { sparse: true });
      c.ensureSkiplist("value2", "value3", { sparse: false });

      var query = "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexRangeNode") {
          assertEqual("skiplist", node.index.type);
          assertFalse(node.index.unique);
          assertFalse(node.index.sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 0 ], results.json, query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerEdgeIndexTestSuite () {
  var c;
  var e;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      db._drop("UnitTestsEdgeCollection");
      c = db._create("UnitTestsCollection");
      e = db._createEdgeCollection("UnitTestsEdgeCollection");

      for (var i = 0; i < 2000; i += 100) {
        var j;

        for (j = 0; j < i; ++j) {
          e.save("UnitTestsCollection/from" + i, "UnitTestsCollection/nono", { value: i + "-" + j });
        }
        for (j = 0; j < i; ++j) {
          e.save("UnitTestsCollection/nono", "UnitTestsCollection/to" + i, { value: i + "-" + j });
        }
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testFindNone : function () {
      var queries = [
        "FOR i IN " + e.name() + " FILTER i._from == '' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from0' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from1' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from2' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._from == '/' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._from == '--foobar--' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._to == '' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection/from0' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection/from1' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection/from2' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection/' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._to == '/' RETURN i._key",
        "FOR i IN " + e.name() + " FILTER i._to == '--foobar--' RETURN i._key"
      ];

      queries.forEach(function(query) {
        var results = AQL_EXECUTE(query);
        assertEqual([ ], results.json, query);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testFindFrom : function () {
      var queries = [
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from100' RETURN i._key", 100 ],
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from200' RETURN i._key", 200 ],
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from1000' RETURN i._key", 1000 ],
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from1100' RETURN i._key", 1100 ],
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from1900' RETURN i._key", 1900 ]
      ];

      queries.forEach(function(query) {
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query[0]);
        assertEqual(0, results.stats.scannedFull);
        assertEqual(query[1], results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testFindTo : function () {
      var queries = [
        [ "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection/to100' RETURN i._key", 100 ],
        [ "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection/to200' RETURN i._key", 200 ],
        [ "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection/to1000' RETURN i._key", 1000 ],
        [ "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection/to1100' RETURN i._key", 1100 ],
        [ "FOR i IN " + e.name() + " FILTER i._to == 'UnitTestsCollection/to1900' RETURN i._key", 1900 ]
      ];

      queries.forEach(function(query) {
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query[0]);
        assertEqual(0, results.stats.scannedFull);
        assertEqual(query[1], results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testFindFromTo : function () {
      var queries = [
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from100' && i._to == 'UnitTestsCollection/nono' RETURN i._key", 100 ],
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from200' && i._to == 'UnitTestsCollection/nono' RETURN i._key", 200 ],
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from1000' && i._to == 'UnitTestsCollection/nono' RETURN i._key", 1000 ],
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from1100' && i._to == 'UnitTestsCollection/nono' RETURN i._key", 1100 ],
        [ "FOR i IN " + e.name() + " FILTER i._from == 'UnitTestsCollection/from1900' && i._to == 'UnitTestsCollection/nono' RETURN i._key", 1900 ]
      ];

      queries.forEach(function(query) {
        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query[0]);
        assertEqual(0, results.stats.scannedFull);
        assertEqual(query[1], results.stats.scannedIndex);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesInOrTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 2000; ++i) {
        c.save({ _key: "test" + i, value1: (i % 100), value2: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testNoIndex : function () {
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ ] RETURN i.value1", [ ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35 ] RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 35, 35 ] RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36 ] RETURN i.value1", [ 35, 36 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 36, 36, 35, 36, -9 ] RETURN i.value1", [ 35, 36 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36, 37 ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 35 RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 36 RETURN i.value1", [ 35, 36 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 36 || i.value1 == 37 RETURN i.value1", [ 35, 36, 37 ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure no index is used
        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
        });
     
        if (query[1].length > 0) { 
          assertTrue(results.stats.scannedFull > 0);
        }
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testNoHashIndex : function () {
      c.ensureHashIndex("value1");
      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value > 95 || i.value1 IN [ 95 ] RETURN i.value1",
        "FOR i IN " + c.name() + " FILTER i.value > 95 || i.value1 IN [ 95, 95, 95 ] RETURN i.value1",
        "FOR i IN " + c.name() + " FILTER i.value > 95 || i.value1 IN [ 95, 96 ] RETURN i.value1",  
        "FOR i IN " + c.name() + " FILTER i.value > 95 || i.value1 IN [ 96, 96, 95, 96, -9 ] RETURN i.value1", 
        "FOR i IN " + c.name() + " FILTER i.value > 95 || i.value1 IN [ 95, 96, 97 ] RETURN i.value1",
        "FOR i IN " + c.name() + " FILTER i.value > 95 || i.value1 == 95 RETURN i.value1",
        "FOR i IN " + c.name() + " FILTER i.value > 95 || i.value1 == 95 || i.value1 == 95 RETURN i.value1",
        "FOR i IN " + c.name() + " FILTER i.value > 95 || i.value1 == 95 || i.value1 == 96 RETURN i.value1", 
        "FOR i IN " + c.name() + " FILTER i.value > 95 || i.value1 == 95 || i.value1 == 96 || i.value1 == 97 RETURN i.value1" 
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure no index is used
        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query);
        results.json.forEach(function(value) {
          assertTrue(value >= 95 && value < 100);
        });
     
        assertEqual(0, results.stats.scannedIndex);
        assertTrue(results.stats.scannedFull > 0);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testHashIndex : function () {
      c.ensureHashIndex("value1");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35 ] RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 35, 35 ] RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36 ] RETURN i.value1", [ 35, 36 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 36, 36, 35, 36, -9 ] RETURN i.value1", [ 35, 36 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36, 37 ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35 ] || i.value1 IN [ 36, 37 ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36, 37 ] || i.value1 IN [ 35, 36, 37 ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 35 RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 36 RETURN i.value1", [ 35, 36 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 36 || i.value1 == 37 RETURN i.value1", [ 35, 36, 37 ] ] 
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure the index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
        });
     
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testHashIndexString : function () {
      c.ensureHashIndex("value1");
      for (var i = 0; i < 1000; ++i) {
        c.save({ value1: "test" + (i % 100) });
      }

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 'test1', 'test3', 'test4' ] RETURN i.value1", [ 'test1', 'test3', 'test4' ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 'test1', 'test1', 'test1' ] RETURN i.value1", [ 'test1' ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 'test9', 'test0' ] RETURN i.value1", [ 'test0', 'test9' ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 'test6', 'test6', 'test5', 'test6', 'test9999', 'test-1', 'foobar' ] RETURN i.value1", [ 'test5', 'test6' ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 'test5' ] || i.value1 IN [ 'test6', 'test7' ] RETURN i.value1", [ 'test5', 'test6', 'test7' ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 'test5', 'test6', 'test7' ] || i.value1 IN [ 'test5', 'test6', 'test7' ] RETURN i.value1", [ 'test5', 'test6', 'test7' ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 'test5' RETURN i.value1", [ 'test5' ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 'test5' || i.value1 == 'test5' RETURN i.value1", [ 'test5' ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 'test5' || i.value1 == 'test6' RETURN i.value1", [ 'test5', 'test6' ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 'test5' || i.value1 == 'test6' || i.value1 == 'test7' RETURN i.value1", [ 'test5', 'test6', 'test7' ] ] 
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure the index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
        });
     
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testHashIndexDynamic : function () {
      c.ensureHashIndex("value1");
      var queries = [
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 IN [ a ] RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, a, a ] RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36 ] RETURN i.value1", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(-9) FOR i IN " + c.name() + " FILTER i.value1 IN [ b, b, a, b, c ] RETURN i.value1", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, b, c ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37) FOR i IN " + c.name() + " FILTER i.value1 IN [ a ] || i.value1 IN [ b, c ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37), d = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, b, c, d ] || i.value1 IN [ a, b, c, d ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == a RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == b RETURN i.value1", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37), d = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == b || i.value1 == c || i.value1 == d RETURN i.value1", [ 35, 36, 37 ] ] 
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure the index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
        });
     
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testHashIndexDifferentAttributes : function () {
      c.ensureHashIndex("value1");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value2 == 5 || i.value1 == 7 RETURN i.value1", 41 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value2 == 5 || i.value1 == 3 RETURN i.value1", 21 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value2 == 5 || i.value1 == 3 || i.value2 == 5 RETURN i.value1", 21 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value1 == 5 || i.value2 == 7 RETURN i.value1", 41 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value1 == 5 || i.value2 == 7 || i.value2 == 19 RETURN i.value1", 42 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 9, 12 ] || i.value2 == 18 RETURN i.value1", 61 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 9, 12 ] || i.value2 IN [ 18, 24, 42 ] RETURN i.value1", 63 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure no index is used
        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query); 
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testHashIndexMulti : function () {
      c.ensureHashIndex("value1", "value2");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 30 && i.value2 == 30 RETURN i.value1", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 30, 31, 32 ] && i.value2 IN [ 30, 31, 32, 130 ] RETURN i.value1", 4 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 30, 31, 32 ] FILTER i.value2 IN [ 30, 31, 32, 130 ] RETURN i.value1", 4 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30) RETURN i.value1", 3 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30 || i.value2 == 35) RETURN i.value1", 3 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && i.value2 IN [ 32, 30, 45, 99, 12, 7 ] RETURN i.value1", 2 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) FILTER (i.value2 == 32 || i.value2 == 29 || i.value2 == 30) RETURN i.value1", 3 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) FILTER (i.value2 == 32 || i.value2 == 29 || i.value2 == 30 || i.value2 == 35) RETURN i.value1", 3 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) FILTER i.value2 IN [ 32, 30, 45, 99, 12, 7 ] RETURN i.value1", 2 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure the index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query);
     
        assertTrue(results.stats.scannedIndex < 10);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testHashIndexMultiDynamic : function () {
      c.ensureHashIndex("value1", "value2");
      var queries = [
        [ "LET a = PASSTHRU(30) FOR i IN " + c.name() + " FILTER i.value1 == a && i.value2 == a RETURN i.value1", 1 ],
        [ "LET a = PASSTHRU(30), b = PASSTHRU(31), c = PASSTHRU(32), d = PASSTHRU(130) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, b, c ] && i.value2 IN [ a, b, c, d ] RETURN i.value1", 4 ],
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) && (i.value2 == c || i.value2 == a || i.value2 == b) RETURN i.value1", 3 ],
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32), d = PASSTHRU(35) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) && (i.value2 == c || i.value2 == a || i.value2 == b || i.value2 == d) RETURN i.value1", 3 ],
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) && i.value2 IN [ c, b, 45, 99, 12, 7 ] RETURN i.value1", 2 ],
        [ "LET a = PASSTHRU(30) FOR i IN " + c.name() + " FILTER i.value1 == a FILTER i.value2 == a RETURN i.value1", 1 ],
        [ "LET a = PASSTHRU(30), b = PASSTHRU(31), c = PASSTHRU(32), d = PASSTHRU(130) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, b, c ] FILTER i.value2 IN [ a, b, c, d ] RETURN i.value1", 4 ],
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) FILTER (i.value2 == c || i.value2 == a || i.value2 == b) RETURN i.value1", 3 ],
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32), d = PASSTHRU(35) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) FILTER (i.value2 == c || i.value2 == a || i.value2 == b || i.value2 == d) RETURN i.value1", 3 ],
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) FILTER i.value2 IN [ c, b, 45, 99, 12, 7 ] RETURN i.value1", 2 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure the index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query);
     
        assertTrue(results.stats.scannedIndex < 10);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSkiplistIndex : function () {
      c.ensureSkiplist("value1");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35 ] RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 35, 35 ] RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36 ] RETURN i.value1", [ 35, 36 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 36, 36, 35, 36, -9 ] RETURN i.value1", [ 35, 36 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36, 37 ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35 ] || i.value1 IN [ 36, 37 ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36, 37 ] || i.value1 IN [ 35, 36, 37 ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 35 RETURN i.value1", [ 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 36 RETURN i.value1", [ 35, 36 ] ], 
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 36 || i.value1 == 37 RETURN i.value1", [ 35, 36, 37 ] ] 
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure the index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
        });
     
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSkiplistIndexDynamic : function () {
      c.ensureSkiplist("value1");
      var queries = [
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 IN [ a ] RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, a, a ] RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 36 ] RETURN i.value1", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(-9) FOR i IN " + c.name() + " FILTER i.value1 IN [ b, b, a, b, c ] RETURN i.value1", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, b, c ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37) FOR i IN " + c.name() + " FILTER i.value1 IN [ a ] || i.value1 IN [ b, c ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, b, c ] || i.value1 IN [ a, b, c ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37), d = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, b, c, d ] || i.value1 IN [ a, b, c, d ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == a RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == b RETURN i.value1", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == b || i.value1 == c RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37), d = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == b || i.value1 == c || i.value1 == d RETURN i.value1", [ 35, 36, 37 ] ] 
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure the index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
        });
    
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSkiplistIndexDifferentAttributes : function () {
      c.ensureSkiplist("value1");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value2 == 5 || i.value1 == 7 RETURN i.value1", 41 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value2 == 5 || i.value1 == 3 RETURN i.value1", 21 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value2 == 5 || i.value1 == 3 || i.value2 == 5 RETURN i.value1", 21 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value1 == 5 || i.value2 == 7 RETURN i.value1", 41 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value1 == 5 || i.value2 == 7 || i.value2 == 19 RETURN i.value1", 42 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 9, 12 ] || i.value2 == 18 RETURN i.value1", 61 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 9, 12 ] || i.value2 IN [ 18, 24, 42 ] RETURN i.value1", 63 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure no index is used
        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length); 
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testNoSkiplistIndexMulti : function () {
      c.ensureSkiplist("value1", "value2");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value2 == 5 || i.value1 == 7 RETURN i.value1", 41 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value2 == 5 || i.value1 == 3 RETURN i.value1", 21 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value2 == 5 || i.value1 == 3 || i.value2 == 5 RETURN i.value1", 21 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value1 == 5 || i.value2 == 7 RETURN i.value1", 41 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 3 || i.value1 == 5 || i.value2 == 7 || i.value2 == 19 RETURN i.value1", 42 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 9, 12 ] || i.value2 == 18 RETURN i.value1", 61 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 9, 12 ] || i.value2 IN [ 18, 24, 42 ] RETURN i.value1", 63 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure no index is used
        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query); 
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSkiplistIndexMulti : function () {
      c.ensureSkiplist("value1", "value2");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 30 && i.value2 == 30 RETURN i.value1", 1 ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 30, 31, 32 ] && i.value2 IN [ 30, 31, 32, 130 ] RETURN i.value1", 4 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30) RETURN i.value1", 3 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30 || i.value2 == 35) RETURN i.value1", 3 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && i.value2 IN [ 32, 30, 45, 99, 12, 7 ] RETURN i.value1", 2 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query); 
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSkiplistIndexMultiDynamic : function () {
      c.ensureSkiplist("value1", "value2");
      var queries = [
        [ "LET a = PASSTHRU(30) FOR i IN " + c.name() + " FILTER i.value1 == a && i.value2 == a RETURN i.value1", 1 ],
        [ "LET a = PASSTHRU(30), b = PASSTHRU(31), c = PASSTHRU(32), d = PASSTHRU(130) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, b, c ] && i.value2 IN [ a, b, c, d ] RETURN i.value1", 4 ],
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) && (i.value2 == c || i.value2 == a || i.value2 == b) RETURN i.value1", 3 ],
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32), d = PASSTHRU(35) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) && (i.value2 == c || i.value2 == a || i.value2 == b || i.value2 == d) RETURN i.value1", 3 ],
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) && i.value2 IN [ c, a, 45, 99, 12, 7 ] RETURN i.value1", 2 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query); 
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSkiplistIndexSort : function () {
      c.ensureSkiplist("value1");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 30 && i.value2 == 30 SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 30, 31, 32 ] && i.value2 IN [ 30, 31, 32, 130 ] SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ], [ 30, 130 ], [ 31, 31 ], [ 32, 32 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 32, 32, 32, 32, 31, 31, 31, 31, 30, 30, 30, 32, 32, 31 ] && i.value2 IN [ 30, 31, 32, 130, 30, 31, 32, 30, 'foobar' ] SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ], [ 30, 130 ], [ 31, 31 ], [ 32, 32 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30) SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 29, 29 ], [ 30, 30 ], [ 32, 32 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30 || i.value2 == 35) SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 29, 29 ], [ 30, 30 ], [ 32, 32 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && i.value2 IN [ 32, 30, 45, 99, 12, 7 ] SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ], [ 32, 32 ] ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        // we will need a sort...
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1].length, results.json.length, query); 
        for (var i = 0; i < results.json.length; ++i) {
          assertEqual(query[1][i], results.json[i], query);
        }
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSkiplistIndexSortMulti : function () {
      c.ensureSkiplist("value1", "value2");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 30 && i.value2 == 30 SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 30, 31, 32 ] && i.value2 IN [ 30, 31, 32, 130 ] SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ], [ 30, 130 ], [ 31, 31 ], [ 32, 32 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 32, 32, 32, 32, 31, 31, 31, 31, 30, 30, 30, 32, 32, 31 ] && i.value2 IN [ 30, 31, 32, 130, 30, 31, 32, 30, 'foobar' ] SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ], [ 30, 130 ], [ 31, 31 ], [ 32, 32 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30) SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 29, 29 ], [ 30, 30 ], [ 32, 32 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30 || i.value2 == 35) SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 29, 29 ], [ 30, 30 ], [ 32, 32 ] ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && i.value2 IN [ 32, 30, 45, 99, 12, 7 ] SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ], [ 32, 32 ] ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        // we don't need a sort...
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1].length, results.json.length, query); 
        for (var i = 0; i < results.json.length; ++i) {
          assertEqual(query[1][i], results.json[i], query);
        }
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSkiplistIndexSortManyCandidates : function () {
      c.ensureSkiplist("value2");
      c.ensureSkiplist("value2", "value1");
      c.ensureSkiplist("value2", "valuex");

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 30 SORT i.value2 RETURN i.value2", [ 30 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN [ 30, 31, 32 ] SORT i.value2 RETURN i.value2", [ 30, 31, 32 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN [ 32, 32, 32, 32, 31, 31, 31, 31, 30, 30, 30, 32, 32, 31, 'foobar' ] SORT i.value2 RETURN i.value2", [ 30, 31, 32 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 32 || i.value2 == 29 || i.value2 == 30 SORT i.value2 RETURN i.value2", [ 29, 30, 32 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 32 || i.value2 == 29 || i.value2 == 30 || i.value2 == 35 SORT i.value2 RETURN i.value2", [ 29, 30, 32, 35 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 IN [ 32, 30, 45, 99, 12, 7 ] SORT i.value2 RETURN i.value2", [ 7, 12, 30, 32, 45, 99 ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1].length, results.json.length, query); 
        for (var i = 0; i < results.json.length; ++i) {
          assertEqual(query[1][i], results.json[i], query);
        }
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesRangesTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 2000; ++i) {
        c.save({ _key: "test" + i, value1: (i % 100), value2: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testRangesSingle : function () {
      c.ensureSkiplist("value1");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 < 3 || i.value1 == 5 || i.value1 == 9 || i.value1 > 98 RETURN i.value1", [ 0, 1, 2, 5, 9, 99 ] ],
        [ "FOR i IN " + c.name() + " FILTER ((i.value1 < 2 || i.value1 == 3) || (i.value1 == 96 || i.value1 > 97)) RETURN i.value1", [ 0, 1, 3, 96, 98, 99 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 34, 33 ] && i.value1 < 35 RETURN i.value1", [ 33, 34 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 35, 34, 33, 99 ] && i.value1 >= 34 RETURN i.value1", [ 34, 35, 99 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 9 || i.value1 > 10) && (i.value1 <= 12) RETURN i.value1", [ 9, 11, 12 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 9 || i.value1 >= 10) && (i.value1 <= 12) RETURN i.value1", [ 9, 10, 11, 12 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 >=9 && (i.value1 == 10 || i.value1 <= 12) RETURN i.value1", [ 9, 10, 11, 12 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 >=9 && (i.value1 == 10 || i.value1 <= 12 || i.value1 IN [ 5, 3 ]) RETURN i.value1", [ 9, 10, 11, 12 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 >= 10) && (i.value1 == 10 || i.value1 <= 12) RETURN i.value1", [ 10, 11, 12 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 9 || i.value1 >= 10) && (i.value1 == 10 || i.value1 <= 12) RETURN i.value1", [ 9, 10, 11, 12 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 > 5 && i.value1 < 10) || (i.value1 > 37 && i.value1 <= 41) RETURN i.value1", [ 6, 7, 8, 9, 38, 39, 40, 41 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 > 37 && i.value1 <= 41) || (i.value1 > 5 && i.value1 < 10) RETURN i.value1", [ 6, 7, 8, 9, 38, 39, 40, 41 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 <= 41 && i.value1 > 37) || (i.value1 < 10 && i.value1 > 5) RETURN i.value1", [ 6, 7, 8, 9, 38, 39, 40, 41 ] ],
        [ "FOR i IN " + c.name() + " FILTER ((i.value1 <= 41 && i.value1 > 37) || i.value1 IN [ 3, 12 ]) || (i.value1 IN [ 74, 77 ] || (i.value1 < 10 && i.value1 > 5)) RETURN i.value1", [ 3, 6, 7, 8, 9, 12, 38, 39, 40, 41, 74, 77 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 > 12 || i.value1 < 7) && (i.value1 > 97 || i.value1 < 5) RETURN i.value1", [ 0, 1, 2, 3, 4, 98, 99 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 >= 11 && i.value1 <= 15) && (i.value1 > 7 && i.value < 28) && i.value1 IN [ 23, 17, 11, 12, 13, 29, 28, 27, 14, 15, 15, 15 ] RETURN i.value1", [ 11, 12, 13, 14, 15 ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1].length * 20, results.json.length, query);
        var last = query[1][0];
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
          assertTrue(value >= last);
          last = value;
        });
     
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testRangesWithRational : function () {
      c.ensureSkiplist("value1");
      for (var i = 0; i < 20; ++i) {
        c.save({ value1: 3.5 });
        c.save({ value1: 12.5 });
        c.save({ value1: 10.1 });
        c.save({ value1: 10.9 });
        c.save({ value1: 42.23 });
      }
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 4, 5 ] RETURN i.value1", [ 3, 4, 5 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 >= 10 && i.value1 < 11) RETURN i.value1", [ 10, 10.1, 10.9 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 >= 10.1 && i.value1 <= 10.9) RETURN i.value1", [ 10.1, 10.9 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 >= 10 && i.value1 <= 11) RETURN i.value1", [ 10, 10.1, 10.9, 11 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 4, 5 ] || i.value1 < 5 RETURN i.value1", [ 0, 1, 2, 3, 3.5, 4, 5 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 4, 5 ] && i.value1 < 5 RETURN i.value1", [ 3, 4 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 4, 5 ] && i.value1 <= 5 RETURN i.value1", [ 3, 4, 5 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 > 40 && i.value1 > 42 && i.value1 < 43 && i.value1 < 44 RETURN i.value1", [ 42.23 ] ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 < 44 && i.value1 > 42) || (i.value1 >= 41 && i.value1 <= 44) RETURN i.value1", [ 41, 42, 42.23, 43, 44 ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1].length * 20, results.json.length, query);
        var last = query[1][0];
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
          assertTrue(value >= last);
          last = value;
        });
     
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultipleFilters : function () {
      c.ensureSkiplist("value1");
      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value1 < 3 FILTER i.value1 < 10 RETURN i.value1", [ 0, 1, 2 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 < 10 FILTER i.value1 IN [ 7, 3, 1 ] RETURN i.value1", [ 1, 3, 7 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 < 10 FILTER i.value1 < 9 FILTER i.value1 < 8 FILTER i.value1 < 7 RETURN i.value1", [ 0, 1, 2, 3, 4, 5, 6 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 < 10 FILTER i.value1 < 9 FILTER i.value1 < 8 FILTER i.value1 == 3 RETURN i.value1", [ 3 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 < 10 FILTER i.value1 < 9 FILTER i.value1 < 8 FILTER i.value1 > 3 RETURN i.value1", [ 4, 5, 6, 7 ] ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 12, 13, 14, 15, 17 ] FILTER i.value1 < 20 FILTER i.value1 > 10 FILTER i.value1 IN [ 11, 12, 13, 14, 15, 16, 17, 18 ] RETURN i.value1", [ 12, 13, 14, 15, 17 ] ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1].length * 20, results.json.length, query);
        var last = query[1][0];
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
          assertTrue(value >= last);
          last = value;
        });
     
        assertTrue(results.stats.scannedIndex > 0);
        assertEqual(0, results.stats.scannedFull);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesSortTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 2000; ++i) {
        c.save({ _key: "test" + i, value: i % 10 });
      }

      c.ensureSkiplist("value");
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSortWithMultipleIndexes : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      // create multiple indexes
      c.ensureHashIndex("value");
      c.ensureHashIndex("value", "value2");
      c.ensureSkiplist("value", "value2");

      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 SORT i.value RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

      var results = AQL_EXECUTE(query);
      var expected = [ ];
      for (var j = 0; j < 200; ++j) {
        expected.push(1);
      }
      assertEqual(expected.sort(), results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(expected.length, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSortWithMultipleIndexesAndRanges : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      // create multiple indexes
      c.ensureHashIndex("value");
      c.ensureHashIndex("value", "value2");
      c.ensureSkiplist("value", "value2");

      var query = "FOR i IN " + c.name() + " FILTER i.value == 9 || i.value == 1 SORT i.value RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

      var results = AQL_EXECUTE(query);
      var expected = [ ];
      for (var j = 0; j < 200; ++j) {
        expected.push(1);
        expected.push(9);
      }
      assertEqual(expected.sort(), results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(expected.length, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultiSortWithMultipleIndexes : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      // create multiple indexes
      c.ensureHashIndex("value");
      c.ensureHashIndex("value", "value2");
      c.ensureSkiplist("value", "value2");

      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 SORT i.value, i.value2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

      var results = AQL_EXECUTE(query);
      var expected = [ ];
      for (var j = 0; j < 200; ++j) {
        expected.push(1);
      }
      assertEqual(expected, results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(expected.length, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultiSortWithMultipleIndexesAndRanges : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      // create multiple indexes
      c.ensureHashIndex("value");
      c.ensureHashIndex("value", "value2");
      c.ensureSkiplist("value", "value2");

      var query = "FOR i IN " + c.name() + " FILTER i.value == 9 || i.value == 1 SORT i.value, i.value2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

      var results = AQL_EXECUTE(query);
      var expected = [ ];
      for (var j = 0; j < 200; ++j) {
        expected.push(1);
        expected.push(9);
      }
      assertEqual(expected.sort(), results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(expected.length, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSingleAttributeSortNotOptimizedAway : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2");
      c.ensureHashIndex("value3");

      var queries = [
        "FOR j IN " + c.name() + " FILTER j.value2 == 2 FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 RETURN i.value2",
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value2 == 3 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value2 == 3 SORT i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2, PASSTHRU(1) RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 SORT i.value3, i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value3 == 2 SORT PASSTHRU(1) RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSingleAttributeSortOptimizedAway : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2");

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 3 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && PASSTHRU(1) SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER PASSTHRU(1) && i.value2 == 2 SORT i.value2 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultiAttributeSortOptimizedAway : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3");

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC, i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC, i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && PASSTHRU(1) SORT i.value2 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryIndexForSortIfConstRanges : function () {
      var queries = [
        "FOR i IN " + c.name() + " FILTER i._key == 'test1' SORT i._key RETURN i._key",
        "FOR i IN " + c.name() + " FILTER i._key == 'test1' SORT i._key DESC RETURN i._key"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseHashIndexForSortIfConstRanges : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3");

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC, i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC, i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testNoHashIndexForSort : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value, value4: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3");

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value4 ASC RETURN i.value2",
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testNoHashIndexForSortDifferentVariable : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3");

      var queries = [
        "LET docs = PASSTHRU([ { value2: 2, value3: 2 } ]) FOR i IN docs FILTER i.value2 == 2 && i.value3 == 2 FOR j IN " + c.name() + " FILTER j.value2 == 3 && j.value3 == 3 SORT i.value2 RETURN i.value2",
        "LET docs = PASSTHRU([ { value2: 2, value3: 2 } ]) FOR i IN docs FILTER i.value2 == 2 && i.value3 == 2 FOR j IN " + c.name() + " FILTER j.value2 == 3 && j.value3 == 3 SORT i.value2, i.value3 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForSortIfConstRanges : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value, value4: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", "value4");

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC, i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 DESC, i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 DESC, i.value3 DESC, i.value4 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 DESC, i.value4 DESC RETURN i.value2",

        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC, i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC, i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC, i.value3 DESC, i.value4 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 DESC, i.value4 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value4 DESC RETURN i.value2",

        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 ASC, i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 DESC, i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 DESC, i.value3 DESC, i.value4 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value3 DESC, i.value4 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value4 DESC RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testNoSkiplistIndexForSort : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value, value4: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3");

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2, i.value3, i.value4 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3, i.value4 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value4 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value4, i.value2, i.value3 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testNoSkiplistIndexForSortDifferentVariable : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3");

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FOR j IN " + c.name() + " FILTER j.value2 == 3 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 FOR j IN " + c.name() + " FILTER j.value2 == 3 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 FOR j IN " + c.name() + " FILTER j.value2 == 3 && j.value3 == 3 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 FOR j IN " + c.name() + " FILTER j.value2 == 3 && j.value3 == 3 SORT i.value2, i.value3 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSparseIndexSort : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var idx = c.ensureSkiplist("value2", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 > 10 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= 10 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 10 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= true SORT i.value2 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          if (node.type === "IndexRangeNode") {
            assertEqual(idx.id.replace(/^.+\//g, ''), node.index.id);
            assertEqual("skiplist", node.index.type);
            assertTrue(node.index.sparse);
          }
          return node.type;
        });

        // index is used for sorting
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSparseIndexNoSort : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      c.ensureSkiplist("value2", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 < 10 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 <= 10 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 <= null SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= null SORT i.value2 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // no index is used for sorting
        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSparseIndexesSort : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value } IN " + c.name());

      var idx1 = c.ensureSkiplist("value2", { sparse: true }); // cannot use for sort
      var idx2 = c.ensureSkiplist("value2", { sparse: false }); // can use for sort

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 < 10 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 <= 10 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 <= null SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= null SORT i.value2 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          if (node.type === "IndexRangeNode") {
            assertNotEqual(idx1.id.replace(/^.+\//g, ''), node.index.id);
            assertEqual(idx2.id.replace(/^.+\//g, ''), node.index.id);
            assertEqual("skiplist", node.index.type);
            assertFalse(node.index.sparse);
          }
          return node.type;
        });

        // index is used for sorting
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSparseIndexSortMulti : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var idx = c.ensureSkiplist("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 > null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > 10 && i.value3 > 4 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= 10 && i.value3 >= 4 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 10 && i.value3 >= 4 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 10 && i.value3 == 4 SORT i.value2, i.value3 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          if (node.type === "IndexRangeNode") {
            assertEqual(idx.id.replace(/^.+\//g, ''), node.index.id);
            assertEqual("skiplist", node.index.type);
            assertTrue(node.index.sparse);
          }
          return node.type;
        });

        // index is used for sorting
        assertNotEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSparseIndexNoSortMulti : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 1 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= 1 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 <= 1 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 < 1 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 > 0 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 < 0 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 < 0 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 <= 0 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 == null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 1 && i.value3 < 1 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 1 && i.value3 <= 1 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 1 && i.value3 == null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 1 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= 1 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 <= 1 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 < 1 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 == null SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 > 0 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == null && i.value3 < 0 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 < 0 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 <= 0 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 == null SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 1 && i.value3 < 1 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 1 && i.value3 <= 1 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 1 && i.value3 == null SORT i.value2 RETURN i.value2",
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // no index is used for sorting
        assertEqual(-1, nodeTypes.indexOf("IndexRangeNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerIndexesTestSuite);
jsunity.run(optimizerEdgeIndexTestSuite);
jsunity.run(optimizerIndexesInOrTestSuite);
jsunity.run(optimizerIndexesRangesTestSuite);
jsunity.run(optimizerIndexesSortTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
