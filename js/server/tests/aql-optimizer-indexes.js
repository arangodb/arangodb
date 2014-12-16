/*jshint strict: false, maxlen: 500 */
/*global require, assertTrue, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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
      assertEqual(12, explain.stats.plansCreated);

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
      assertEqual(36, explain.stats.plansCreated);

      var results = AQL_EXECUTE(query);
      assertEqual(0, results.stats.scannedFull);
      assertNotEqual(0, results.stats.scannedIndex); 
      assertEqual([ [ [ 'test1' ], [ 'test2' ], [ 'test3' ], [ 'test4' ], [ 'test5' ], [ 'test6' ], [ 'test7' ], [ 'test8' ], [ 'test9' ], [ 'test10' ] ] ], results.json);
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
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 36 || i.value1 == 37 RETURN i.value1", [ 35, 36, 37 ] ], 
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
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37) FOR i IN " + c.name() + " FILTER i.value1 IN [ a, b, c ] || i.value1 IN [ a, b, c ] RETURN i.value1", [ 35, 36, 37 ] ], 
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == a RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == b RETURN i.value1", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == b || i.value1 == c RETURN i.value1", [ 35, 36, 37 ] ], 
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
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30) RETURN i.value1", 3 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30 || i.value2 == 35) RETURN i.value1", 3 ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && i.value2 IN [ 32, 30, 45, 99, 12, 7 ] RETURN i.value1", 2 ]
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
     
        assertTrue(results.stats.scannedIndex > 0);
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
        [ "LET a = PASSTHRU(29), b = PASSTHRU(30), c = PASSTHRU(32) FOR i IN " + c.name() + " FILTER (i.value1 == b || i.value1 == a || i.value1 == c) && i.value2 IN [ c, b, 45, 99, 12, 7 ] RETURN i.value1", 2 ]
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
     
        assertTrue(results.stats.scannedIndex > 0);
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
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 35 || i.value1 == 36 || i.value1 == 37 RETURN i.value1", [ 35, 36, 37 ] ], 
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
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == a RETURN i.value1", [ 35 ] ],
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == b RETURN i.value1", [ 35, 36 ] ], 
        [ "LET a = PASSTHRU(35), b = PASSTHRU(36), c = PASSTHRU(37) FOR i IN " + c.name() + " FILTER i.value1 == a || i.value1 == b || i.value1 == c RETURN i.value1", [ 35, 36, 37 ] ], 
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
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 3, 9, 12 ] || i.value2 IN [ 18, 24, 42 ] RETURN i.value1", 63 ],

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
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerIndexesTestSuite);
jsunity.run(optimizerIndexesInOrTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
