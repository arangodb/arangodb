/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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
var db = require("@arangodb").db;

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
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        let opt = { optimizer: { rules: ["-reduce-extraction-to-projection"] } };
        var plan = AQL_EXPLAIN(query[0], {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure no index is used
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

        var results = AQL_EXECUTE(query[0], {}, opt);
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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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
        [ "FOR i IN " + c.name() + " FILTER i.value1 == 30 && i.value2 == 30 SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ] ], false ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 30, 31, 32 ] && i.value2 IN [ 30, 31, 32, 130 ] SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ], [ 30, 130 ], [ 31, 31 ], [ 32, 32 ] ], false ],
        [ "FOR i IN " + c.name() + " FILTER i.value1 IN [ 32, 32, 32, 32, 31, 31, 31, 31, 30, 30, 30, 32, 32, 31 ] && i.value2 IN [ 30, 31, 32, 130, 30, 31, 32, 30, 'foobar' ] SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ], [ 30, 130 ], [ 31, 31 ], [ 32, 32 ] ], false ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30) SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 29, 29 ], [ 30, 30 ], [ 32, 32 ] ], false ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && (i.value2 == 32 || i.value2 == 29 || i.value2 == 30 || i.value2 == 35) SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 29, 29 ], [ 30, 30 ], [ 32, 32 ] ], false ],
        [ "FOR i IN " + c.name() + " FILTER (i.value1 == 30 || i.value1 == 29 || i.value1 == 32) && i.value2 IN [ 32, 30, 45, 99, 12, 7 ] SORT i.value1, i.value2 RETURN [ i.value1, i.value2 ]", [ [ 30, 30 ], [ 32, 32 ] ], false ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

    testInOrReplacements : function() {
      c.ensureSkiplist("foo");

      c.insert({ value1: "testi", value2: "foobar", foo: 0 });
      c.insert({ value1: "testi", value2: "foobar", foo: 1 });
      c.insert({ value1: "testi", value2: "foobar", foo: 2 });
      c.insert({ value1: "testi", value2: "foobar", foo: 5 });
      c.insert({ value1: "testi", value2: "foobar", foo: 9 });
      c.insert({ value1: "testi", value2: "foobar", foo: 10 });
      c.insert({ value1: "testi", value2: "foobar", foo: 11 });

      let query = "FOR doc IN " + c.name() + " FILTER (LIKE(doc.value1, '%testi%') OR LIKE(doc.value2, '%foobar%')) AND doc.foo IN [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] RETURN doc";

      let results = AQL_EXECUTE(query);
      assertEqual(5, results.json.length);
    }
        
  };
}

jsunity.run(optimizerIndexesInOrTestSuite);

return jsunity.done();

