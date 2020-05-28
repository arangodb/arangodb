/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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
var internal = require("internal");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesSortTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      let docs = [];
      for (var i = 0; i < 2000; ++i) {
        docs.push({ _key: "test" + i, value: i % 10 });
      }
      c.insert(docs);

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

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      if (!require("@arangodb/cluster").isCluster()) {
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      }

      var results = AQL_EXECUTE(query);
      var expected = [ ];
      for (var j = 0; j < 200; ++j) {
        expected.push(1);
        expected.push(9);
      }
      assertEqual(expected.sort(), results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      if (!require("@arangodb/cluster").isCluster()) {
        assertEqual(expected.length, results.stats.scannedIndex);
      }
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
      internal.waitForEstimatorSync(); // make sure estimates are consistent

      var query = "FOR i IN " + c.name() + " FILTER i.value == 1 SORT i.value, i.value2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      if (!require("@arangodb/cluster").isCluster()) {
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      }

      var results = AQL_EXECUTE(query);
      var expected = [ ];
      for (var j = 0; j < 200; ++j) {
        expected.push(1);
      }
      assertEqual(expected, results.json, query);
      assertEqual(0, results.stats.scannedFull);
      if (!require("@arangodb/cluster").isCluster()) {
        assertEqual(expected.length, results.stats.scannedIndex);
      }
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
      internal.waitForEstimatorSync(); // make sure estimates are consistent

      var query = "FOR i IN " + c.name() + " FILTER i.value == 9 || i.value == 1 SORT i.value, i.value2 RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      if (!require("@arangodb/cluster").isCluster()) {
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      }

      var results = AQL_EXECUTE(query);
      var expected = [ ];
      for (var j = 0; j < 200; ++j) {
        expected.push(1);
        expected.push(9);
      }
      assertEqual(expected.sort(), results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      if (!require("@arangodb/cluster").isCluster()) {
        assertEqual(expected.length, results.stats.scannedIndex);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSingleAttributeSortNotOptimizedAwayRocksDB : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2");
      c.ensureHashIndex("value3");

      var queries = [
                     "FOR j IN " + c.name() + " FILTER j.value2 == 2 FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 RETURN i.value2",
                     "FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value2 == 3 SORT i.value3 RETURN i.value2",
                     "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2, i.value3 RETURN i.value2",
                     "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 RETURN i.value2",
                     "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2, NOOPT(1) RETURN i.value2",
                     "FOR i IN " + c.name() + " FILTER i.value3 == 2 SORT i.value2 RETURN i.value2",
                     "FOR i IN " + c.name() + " FILTER i.value3 == 2 SORT i.value3, i.value2 RETURN i.value2",
                     "FOR i IN " + c.name() + " FILTER i.value3 == 2 SORT NOOPT(1) RETURN i.value2"
                     ];

      queries.forEach(function(query) {
                      var plan = AQL_EXPLAIN(query).plan;
                      var nodeTypes = plan.nodes.map(function(node) {
                                                     return node.type;
                                                     });

                      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
                      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
                      });

      queries = ["FOR i IN " + c.name() + " FILTER i.value2 == 2 || i.value2 == 3 SORT i.value2 RETURN i.value2"];

      queries.forEach(function(query) {
                      var plan = AQL_EXPLAIN(query).plan;
                      var nodeTypes = plan.nodes.map(function(node) {
                                                     return node.type;
                                                     });

                      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
                      assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
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
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && NOOPT(1) SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER NOOPT(1) && i.value2 == 2 SORT i.value2 RETURN i.value2",
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        // The HashIndex does not yet guarantee sorting, but we're filtering on a constant condition
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testDontOptimizeAway : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureIndex({ type: "skiplist", fields: ["value2"] });

      var queries = [
        "FOR j IN 1..1 FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 RETURN i.value2",
        "FOR i IN " + c.name() + " FOR j IN 1..1 FILTER i.value2 == 2 SORT i.value2 RETURN i.value2",
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testMultiAttributeSortOptimizedAway : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      c.ensureHashIndex("value2", "value3");

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2, i.value3 RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC, i.value3 DESC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC, i.value3 DESC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && NOOPT(1) SORT i.value2 RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2, i.value4 RETURN i.value2", true ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        // The HashIndex does not yet guarantee sorting, but we can optimize it away anyway
        if (query[1]) {
          assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
        }
        else {
          assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testCannotUsePrimaryIndexForSortIfConstRanges : function () {
      var queries = [
        "FOR i IN " + c.name() + " FILTER i._key == 'test1' SORT i._key RETURN i._key",
        "FOR i IN " + c.name() + " FILTER i._key == 'test1' SORT i._key DESC RETURN i._key"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertTrue(
          (
            ( nodeTypes.indexOf("IndexNode") !== -1) ||
              ( nodeTypes.indexOf("SingleRemoteOperationNode") !== -1)
          ), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual(['test1'], results.json, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testCannotUseHashIndexForSortIfConstRanges : function () {
      c.ensureIndex({ type: "hash", fields: [ "value2", "value3" ] });

      var queries = [
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC, i.value3 ASC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC, i.value3 DESC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 ASC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 DESC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC RETURN i.value2", false ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2, i.value4 DESC RETURN i.value2", true ],
        [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value4, i.value2 DESC RETURN i.value2", true ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        // stil can optimize away the sort node as we're filtering on constant values
        if (query[1]) {
          assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
        }
        else {
          assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
        }
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test index usage
    ////////////////////////////////////////////////////////////////////////////////

    testCannotUseHashIndexForSortIfConstRangesMoreRocksDB : function () {
      c.ensureIndex({ type: "hash", fields: [ "value2", "value3", "value4" ] });

      var queries = [
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 ASC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 DESC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 ASC, i.value4 ASC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 DESC, i.value4 DESC RETURN i.value2", true ],

                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value4 ASC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value4 DESC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 ASC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 DESC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 ASC, i.value4 ASC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 DESC, i.value4 DESC RETURN i.value2" ,true ],

                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value3 ASC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value3 DESC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value3 ASC, i.value4 ASC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value3 DESC, i.value4 DESC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value4 ASC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value4 DESC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 ASC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 DESC, i.value3 DESC, i.value4 DESC RETURN i.value2", true ],
                     [ "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 DESC RETURN i.value2", true ]
                     ];

      queries.forEach(function(query) {
                      var plan = AQL_EXPLAIN(query[0]).plan;
                      var nodeTypes = plan.nodes.map(function(node) {
                                                     return node.type;
                                                     });

                      assertEqual(-1, nodeTypes.indexOf("SortNode"), query[0]);

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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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
        "LET docs = NOOPT([ { value2: 2, value3: 2 } ]) FOR i IN docs FILTER i.value2 == 2 && i.value3 == 2 FOR j IN " + c.name() + " FILTER j.value2 == 3 && j.value3 == 3 SORT i.value2 RETURN i.value2",
        "LET docs = NOOPT([ { value2: 2, value3: 2 } ]) FOR i IN docs FILTER i.value2 == 2 && i.value3 == 2 FOR j IN " + c.name() + " FILTER j.value2 == 3 && j.value3 == 3 SORT i.value2, i.value3 RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC, i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC, i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value2 DESC, i.value3 DESC, i.value4 DESC RETURN i.value2",

        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 ASC, i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 DESC, i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 && i.value4 == 2 SORT i.value2 DESC, i.value3 DESC, i.value4 DESC RETURN i.value2"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testCanUseSkiplistIndexForSortIfConstRanges : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value, value4: i.value } IN " + c.name());

      c.ensureSkiplist("value2", "value3", "value4");

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC, i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC, i.value3 DESC RETURN i.value2",

        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 DESC, i.value4 DESC RETURN i.value2",

        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value4 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 && i.value3 == 2 SORT i.value3 DESC, i.value4 DESC RETURN i.value2",

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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testCannotUseSkiplistIndexForSortIfConstRanges : function () {
      c.ensureSkiplist("value2", "value3", "value4");

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value3 ASC, i.value4 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC, i.value4 ASC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 DESC RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 2 SORT i.value2 ASC, i.value3 ASC, i.value4 DESC RETURN i.value2"
     ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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
          if (node.type === "IndexNode") {
            assertEqual(node.indexes.length, 1);
            assertEqual(idx.id.replace(/^.+\//g, ''), node.indexes[0].id);
            assertEqual("skiplist", node.indexes[0].type);
            assertTrue(node.indexes[0].sparse);
          }
          return node.type;
        });

        // index is used for sorting we made sure it is not null
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        // right now sparse indexes are not able to deliver sorting
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
        let opt = { optimizer: { rules: ["-reduce-extraction-to-projection"] } };
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // no index is used for sorting
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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
          if (node.type === "IndexNode") {
            assertNotEqual(idx1.id.replace(/^.+\//g, ''), node.indexes[0].id);
            assertEqual(idx2.id.replace(/^.+\//g, ''), node.indexes[0].id);
            assertEqual("skiplist", node.indexes[0].type);
            assertFalse(node.indexes[0].sparse);
          }
          return node.type;
        });

        // index is used for sorting
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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
        "FOR i IN " + c.name() + " FILTER i.value2 == 10 && i.value3 >= 4 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 == 10 && i.value3 == 4 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 > null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 != null && i.value3 > null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 != null && i.value3 != null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 != null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > 10 && i.value3 > 4 SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= 10 && i.value3 >= 4 SORT i.value2, i.value3 RETURN i.value2",
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          if (node.type === "IndexNode") {
            assertEqual(node.indexes.length, 1);
            assertEqual(idx.id.replace(/^.+\//g, ''), node.indexes[0].id);
            assertEqual("skiplist", node.indexes[0].type);
            assertTrue(node.indexes[0].sparse);
          }
          return node.type;
        });

        // index is used for sorting and filtering. We made sure it is never null
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testSparseIndexSortMultiNotDetected : function () {
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      // If the first value is a range, the optimizer doesn't detect the index properly
      var idx = c.ensureSkiplist("value2", "value3", { sparse: true });

      var queries = [
        "FOR i IN " + c.name() + " FILTER i.value2 >= null && i.value3 > null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 > null && i.value3 >= null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= null && i.value3 >= null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 != null && i.value3 >= null SORT i.value2, i.value3 RETURN i.value2",
        "FOR i IN " + c.name() + " FILTER i.value2 >= null && i.value3 != null SORT i.value2, i.value3 RETURN i.value2",
      ];

      queries.forEach(function(query) {
        let opt = { optimizer: { rules: ["-reduce-extraction-to-projection"] } };
        var plan = AQL_EXPLAIN(query, null, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          if (node.type === "IndexNode") {
            assertEqual(node.indexes.length, 1);
            assertEqual(idx.id.replace(/^.+\//g, ''), node.indexes[0].id);
            assertEqual("skiplist", node.indexes[0].type);
            assertTrue(node.indexes[0].sparse);
          }
          return node.type;
        });

        // index is used for sorting and filtering. We made sure it is never null
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
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
        let opt = { optimizer: { rules: ["-reduce-extraction-to-projection"] } };
        var plan = AQL_EXPLAIN(query, {}, opt).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // no index is used for sorting
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query);
      });
    }

  };
}

jsunity.run(optimizerIndexesSortTestSuite);

return jsunity.done();
