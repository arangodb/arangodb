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
var db = require("@arangodb").db;

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
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryIdEq : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._id == 'UnitTestsCollection/test22' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 22 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryIdEqNoMatches : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._id == 'UnitTestsCollection/qux' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryId : function () {
      var values = [ "UnitTestsCollection/test1", "UnitTestsCollection/test2", "UnitTestsCollection/test21", "UnitTestsCollection/test30" ];
      var query = "FOR i IN " + c.name() + " FILTER i._id IN " + JSON.stringify(values) + " RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 2, 21, 30 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(4, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _id
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryIdNoMatches : function () {
      var values = [ "UnitTestsCollection/foo", "UnitTestsCollection/bar", "UnitTestsCollection/baz", "UnitTestsCollection/qux" ];
      var query = "FOR i IN " + c.name() + " FILTER i._id IN " + JSON.stringify(values) + " RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _key
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryKeyEq : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._key == 'test6' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 6 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _key
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryKeyEqNoMatches : function () {
      var query = "FOR i IN " + c.name() + " FILTER i._key == 'qux' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _key
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryKey : function () {
      var values = [ "test1", "test2", "test21", "test30" ];
      var query = "FOR i IN " + c.name() + " FILTER i._key IN " + JSON.stringify(values) + " RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 2, 21, 30 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(4, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test _key
////////////////////////////////////////////////////////////////////////////////

    testUsePrimaryKeyNoMatches : function () {
      var values = [ "foo", "bar", "baz", "qux" ];
      var query = "FOR i IN " + c.name() + " FILTER i._key IN " + JSON.stringify(values) + " RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _key
////////////////////////////////////////////////////////////////////////////////

    testFakeKey : function () {
      var query = "LET t = { _key: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._key RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _key
////////////////////////////////////////////////////////////////////////////////

    testFakeKeyNonConst : function () {
      var query = "LET t = NOOPT({ _key: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._key RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _id
////////////////////////////////////////////////////////////////////////////////

    testFakeId : function () {
      var query = "LET t = { _id: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._id RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _id
////////////////////////////////////////////////////////////////////////////////

    testFakeIdNonConst : function () {
      var query = "LET t = NOOPT({ _id: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._id RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _rev
////////////////////////////////////////////////////////////////////////////////

    testFakeRev : function () {
      var query = "LET t = { _rev: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._rev RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _rev
////////////////////////////////////////////////////////////////////////////////

    testFakeRevNonConst : function () {
      var query = "LET t = NOOPT({ _rev: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._rev RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _from
////////////////////////////////////////////////////////////////////////////////

    testFakeFrom : function () {
      var query = "LET t = { _from: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._from RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _from
////////////////////////////////////////////////////////////////////////////////

    testFakeFromNonConst : function () {
      var query = "LET t = NOOPT({ _from: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._from RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _to
////////////////////////////////////////////////////////////////////////////////

    testFakeTo : function () {
      var query = "LET t = { _to: 'test12' } FOR i IN " + c.name() + " FILTER i._key == t._to RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test fake _to
////////////////////////////////////////////////////////////////////////////////

    testFakeToNonConst : function () {
      var query = "LET t = NOOPT({ _to: 'test12' }) FOR i IN " + c.name() + " FILTER i._key == t._to RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      
      var results = AQL_EXECUTE(query);
      assertEqual([ 12 ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(1, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFiltersWithLimit : function () {
      c.insert({ value: "one" });
      c.insert({ value: "two" });
      c.insert({ value: "one" });
      c.insert({ value: "two" });
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] SORT i.value DESC LIMIT 0, 2 FILTER i.value == 'one' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);

      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      
      results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertTrue(results.stats.scannedFull > 0);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFilters1 : function () {
      c.insert({ value: "one" });
      c.insert({ value: "one" });
      c.insert({ value: "two" });
      c.insert({ value: "two" });
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] FILTER i.value == 'one' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 'one', 'one' ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);

      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      
      results = AQL_EXECUTE(query);
      assertEqual([ 'one', 'one' ], results.json, query);
      assertTrue(results.stats.scannedFull > 0);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFilters2 : function () {
      c.insert({ value: "one" });
      c.insert({ value: "one" });
      c.insert({ value: "two" });
      c.insert({ value: "two" });
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] LIMIT 0, 4 FILTER i.value == 'one' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertNotEqual(-1, nodeTypes.indexOf("LimitNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 'one', 'one' ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
      
      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      
      results = AQL_EXECUTE(query);
      assertEqual([ 'one', 'one' ], results.json, query);
      assertTrue(results.stats.scannedFull > 0);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFiltersInvalid1 : function () {
      c.insert({ value: "one" });
      c.insert({ value: "one" });
      c.insert({ value: "two" });
      c.insert({ value: "two" });
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] FILTER i.value == 'three' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertNotEqual(-1, nodeTypes.indexOf("NoResultsNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
      
      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      
      results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertEqual(0, results.stats.scannedIndex);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexMultipleFiltersInvalid2 : function () {
      c.insert({ value: "one" });
      c.insert({ value: "one" });
      c.insert({ value: "two" });
      c.insert({ value: "two" });
      var query = "FOR i IN " + c.name() + " FILTER i.value IN ['one', 'two'] LIMIT 0, 4 FILTER i.value == 'three' RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertEqual(0, results.stats.scannedFull);
      assertTrue(results.stats.scannedIndex > 0);
      
      // retry without index
      var idx = c.lookupIndex({ type: "skiplist", fields: [ "value" ] });
      c.dropIndex(idx);
      
      results = AQL_EXECUTE(query);
      assertEqual([ ], results.json, query);
      assertTrue(results.stats.scannedFull > 0);
      assertEqual(0, results.stats.scannedIndex);
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
          if (node.type === "IndexNode") {
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
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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
        if (node.type === "IndexNode") {
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
        if (node.type === "IndexNode") {
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
        if (node.type === "IndexNode") {
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
      assertNotEqual(-1, subNodeTypes.indexOf("IndexNode"), query);
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
      assertNotEqual(-1, subNodeTypes.indexOf("IndexNode"), query);
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
        if (node.type === "IndexNode") {
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
        if (node.type === "IndexNode") {
          ++indexNodes;
          assertEqual("hash", node.indexes[0].type);
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
        if (node.type === "IndexNode") {
          ++indexNodes;
          assertEqual("hash", node.indexes[0].type);
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
        if (node.type === "IndexNode") {
          ++indexNodes;
          if (indexNodes === 1) {
            // skiplist must be used for the first FOR
            assertEqual("skiplist", node.indexes[0].type);
            assertEqual("i", node.outVariable.name);
          }
          else {
            // second FOR should use a hash index
            assertEqual("hash", node.indexes[0].type);
            assertEqual("j", node.outVariable.name);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(2, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

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
        if (node.type === "IndexNode") {
          ++indexNodes;
          // skiplist must be used for both FORs
          assertEqual("skiplist", node.indexes[0].type);
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
      assertEqual(1, explain.stats.plansCreated);

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
        if (node.type === "IndexNode") {
          ++indexNodes;
          if (indexNodes === 1) {
            assertEqual("hash", node.indexes[0].type);
            assertEqual("i", node.outVariable.name);
          }
          else if (indexNodes === 2) {
            assertEqual("hash", node.indexes[0].type);
            assertEqual("j", node.outVariable.name);
          }
          else {
            assertEqual("skiplist", node.indexes[0].type);
            assertEqual("k", node.outVariable.name);
          }
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(3, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

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
        if (node.type === "IndexNode") {
          ++indexNodes;
          assertEqual("hash", node.indexes[0].type);
        }
        else if (node.type === "EnumerateCollectionNode") {
          ++collectionNodes;
        }
      });

      assertEqual(0, collectionNodes);
      assertEqual(20, indexNodes);
      assertEqual(1, explain.stats.plansCreated);

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
        if (node.type === "IndexNode") {
          assertEqual("primary", node.indexes[0].type);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual("hash", node.indexes[0].type);
          assertTrue(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 9 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrMultiplySkiplist : function () {
      var query = "FOR i IN " + c.name() + " FILTER (i.value > 1 && i.value < 9) && (i.value2 == null || i.value3 == null) RETURN i.value";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 2, 3, 4, 5, 6, 7, 8 ], results.json.sort(), query);
      assertTrue(results.stats.scannedIndex > 0);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexSkiplistMultiple : function () {
      c.truncate();
      for (var i = 0; i < 10; ++i) {
        c.insert({ value1: i, value2: i });
      }
      c.ensureIndex({ type: "skiplist", fields: [ "value1", "value2" ] });

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 > 1 && i.value2 < 9) && (i.value1 == 3) RETURN i.value1";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
      assertEqual(-1, nodeTypes.indexOf("FilterNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 3 ], results.json.sort(), query);
      assertEqual(1, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexSkiplistMultiple2 : function () {
      c.truncate();
      for (var i = 0; i < 10; ++i) {
        c.insert({ value1: i, value2: i });
      }
      c.ensureIndex({ type: "skiplist", fields: [ "value1", "value2" ] });

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 > 1 && i.value2 < 9) && (i.value1 == 2 || i.value1 == 3) RETURN i.value1";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 2, 3 ], results.json.sort(), query);
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
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertTrue(node.indexes[0].unique);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1 ], results.json, query);
      assertTrue(results.stats.scannedIndex > 0);
      assertEqual(0, results.stats.scannedFull);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionEq : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: [ i.value ] } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || [ i.value ] == i.value2 RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(2000, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionIn : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: [ i.value ] } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || i.value IN i.value2 RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(2000, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionLt1 : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value - 1 } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || i.value2 < i.value RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(2000, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionLt2 : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value - 1 } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || i.value2 < PASSTHRU(i.value) RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(2000, results.json.length);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrNoIndexComplexConditionGt : function () {
      c.ensureSkiplist("value2");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value + 1 } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER i.value == -1 || i.value2 > i.value RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual(2000, results.json.length);
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
        if (node.type === "IndexNode") {
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertEqual([ "value2", "value3" ], node.indexes[0].fields);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertEqual([ "value2", "value3" ], node.indexes[0].fields);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(0, results.stats.scannedIndex);
      assertTrue(results.stats.scannedFull > 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testIndexOrSkiplistMultipleRangesPartialIndex : function () {
      c.ensureSkiplist("value2", "value3");
      AQL_EXECUTE("FOR i IN " + c.name() + " UPDATE i WITH { value2: i.value, value3: i.value } IN " + c.name());

      var query = "FOR i IN " + c.name() + " FILTER (i.value2 == 1 && i.value3 == 1) || (i.value2 == 2) RETURN i.value2";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertEqual([ "value2", "value3" ], node.indexes[0].fields);
        }
        return node.type;
      });
      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

      var results = AQL_EXECUTE(query);
      assertEqual([ 1, 2 ], results.json.sort(), query);
      assertEqual(2, results.stats.scannedIndex);
      assertEqual(0, results.stats.scannedFull);
    },

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
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        if ( nodeTypes.indexOf("NoResultsNode") !== -1) {
          // The condition is not impossible
          // We have to use an index
          assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        }
        // The condition is impossible. We do not care for indexes.
        // assertNotEqual(-1, nodeTypes.indexOf("NoResultsNode"), query);
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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertTrue(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertFalse(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
          if (node.type === "IndexNode") {
            assertEqual(1, node.indexes.length);
            assertEqual("hash", node.indexes[0].type);
            assertFalse(node.indexes[0].unique);
            assertTrue(node.indexes[0].sparse);
          }
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("hash", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertFalse(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertTrue(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertTrue(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertFalse(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
          if (node.type === "IndexNode") {
            assertEqual(1, node.indexes.length);
            assertEqual("skiplist", node.indexes[0].type);
            assertFalse(node.indexes[0].unique);
            assertTrue(node.indexes[0].sparse);
          }
          return node.type;
        });

        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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

        // Cannot use Index
        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);
        var results = AQL_EXECUTE(query);
        assertEqual([ 2 ], results.json, query);
        assertTrue(results.stats.scannedFull > 0);
        assertEqual(0, results.stats.scannedIndex);
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

        assertEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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
        if (node.type === "IndexNode") {
          assertEqual(1, node.indexes.length);
          assertEqual("skiplist", node.indexes[0].type);
          assertFalse(node.indexes[0].unique);
          assertFalse(node.indexes[0].sparse);
        }
        return node.type;
      });

      assertNotEqual(-1, nodeTypes.indexOf("IndexNode"), query);

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

function optimizerIndexesMultiCollectionTestSuite () {
  var c1;
  var c2;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection1");
      db._drop("UnitTestsCollection2");
      c1 = db._create("UnitTestsCollection1");
      c2 = db._create("UnitTestsCollection2");
 
      var i;
      for (i = 0; i < 200; ++i) {
        c1.save({ _key: "test" + i, value: i });
      }
      for (i = 0; i < 200; ++i) {
        c2.save({ _key: "test" + i, value: i, ref: "UnitTestsCollection1/test" + i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection1");
      db._drop("UnitTestsCollection2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexAndSortInInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.ref LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryNode");
      assertNotEqual(-1, sub);

      var subNodeTypes = plan.nodes[sub].subquery.nodes.map(function(node) {
        return node.type;
      });
      
      assertEqual("SingletonNode", subNodeTypes[0], query);
      var idx = subNodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("hash", plan.nodes[sub].subquery.nodes[idx].indexes[0].type);
      assertNotEqual(-1, subNodeTypes.indexOf("SortNode"), query); // must have sort node for inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseIndexAndNoSortInInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.value LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryNode");
      assertNotEqual(-1, sub);

      var subNodeTypes = plan.nodes[sub].subquery.nodes.map(function(node) {
        return node.type;
      });
      
      assertEqual("SingletonNode", subNodeTypes[0], query);
      var idx = subNodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("hash", plan.nodes[sub].subquery.nodes[idx].indexes[0].type);
      assertEqual(-1, subNodeTypes.indexOf("SortNode"), query); // must not have sort node for inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseRightIndexAndSortInInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.ref LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryNode");
      assertNotEqual(-1, sub);

      var subNodeTypes = plan.nodes[sub].subquery.nodes.map(function(node) {
        return node.type;
      });
      
      assertEqual("SingletonNode", subNodeTypes[0], query);
      var idx = subNodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("hash", plan.nodes[sub].subquery.nodes[idx].indexes[0].type);
      assertNotEqual(-1, subNodeTypes.indexOf("SortNode"), query); // must have sort node for inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseRightIndexAndSortInInner2 : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.value LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryNode");
      assertNotEqual(-1, sub);

      var subNodeTypes = plan.nodes[sub].subquery.nodes.map(function(node) {
        return node.type;
      });
      
      assertEqual("SingletonNode", subNodeTypes[0], query);
      var idx = subNodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("hash", plan.nodes[sub].subquery.nodes[idx].indexes[0].type);
      assertEqual(-1, subNodeTypes.indexOf("SortNode"), query); // we're filtering on a constant, must not have sort node for inner query
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseRightIndexAndSortInInnerInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR z IN 1..2 FOR j IN " + c2.name() + " FILTER j.value == i.value SORT j.value LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryNode");
      assertNotEqual(-1, sub);

      var subNodeTypes = plan.nodes[sub].subquery.nodes.map(function(node) {
        return node.type;
      });
      
      assertEqual("SingletonNode", subNodeTypes[0], query);
      var idx = subNodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("hash", plan.nodes[sub].subquery.nodes[idx].indexes[0].type);
      assertNotEqual(-1, subNodeTypes.indexOf("SortNode"), query); // we're filtering on a constant, but we're in an inner loop
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
////////////////////////////////////////////////////////////////////////////////

    testUseRightIndexNoSortInInner : function () {
      c2.ensureIndex({ type: "hash", fields: [ "value" ] });
      c2.ensureIndex({ type: "skiplist", fields: [ "ref" ] });
      var query = "FOR i IN " + c1.name() + " LET res = (FOR j IN " + c2.name() + " FILTER j.ref == i.ref SORT j.ref LIMIT 1 RETURN j) SORT res[0] RETURN i";

      var plan = AQL_EXPLAIN(query).plan;
      var nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual("SingletonNode", nodeTypes[0], query);
      assertEqual(-1, nodeTypes.indexOf("IndexNode"), query); // no index for outer query
      assertNotEqual(-1, nodeTypes.indexOf("SortNode"), query); // sort node for outer query

      var sub = nodeTypes.indexOf("SubqueryNode");
      assertNotEqual(-1, sub);

      var subNodeTypes = plan.nodes[sub].subquery.nodes.map(function(node) {
        return node.type;
      });
      
      assertEqual("SingletonNode", subNodeTypes[0], query);
      var idx = subNodeTypes.indexOf("IndexNode");
      assertNotEqual(-1, idx, query); // index used for inner query
      assertEqual("skiplist", plan.nodes[sub].subquery.nodes[idx].indexes[0].type);
      assertEqual(-1, subNodeTypes.indexOf("SortNode"), query); // must not have sort node for inner query
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerIndexesTestSuite);
jsunity.run(optimizerIndexesMultiCollectionTestSuite);

return jsunity.done();

