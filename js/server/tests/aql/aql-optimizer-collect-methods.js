/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for COLLECT w/ COUNT
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

function optimizerCollectMethodsTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 1500; ++i) {
        c.save({ group: "test" + (i % 10), value: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief number of plans
////////////////////////////////////////////////////////////////////////////////

    testHashedNumberOfPlans : function () {
      var queries = [
        "FOR j IN " + c.name() + " COLLECT value = j RETURN value",
        "FOR j IN " + c.name() + " COLLECT value = j WITH COUNT INTO l RETURN [ value, l ]"
      ];

      queries.forEach(function(query) {
        var plans = AQL_EXPLAIN(query, null, { allPlans: true, optimizer: { rules: [ "-all" ] } }).plans;

        assertEqual(2, plans.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief number of plans
////////////////////////////////////////////////////////////////////////////////

    testSortedNumberOfPlans : function () {
      c.ensureIndex({ type: "skiplist", fields: [ "value" ] }); 
      var queries = [
        "FOR j IN " + c.name() + " COLLECT value = j RETURN value",
        "FOR j IN " + c.name() + " COLLECT value = j WITH COUNT INTO l RETURN [ value, l ]"
      ];
      
      queries.forEach(function(query) {
        var plans = AQL_EXPLAIN(query, null, { allPlans: true, optimizer: { rules: [ "-all" ] } }).plans;

        assertEqual(2, plans.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief number of plans
////////////////////////////////////////////////////////////////////////////////

    testNumberOfPlansWithInto : function () {
      var queries = [
        "FOR j IN " + c.name() + " COLLECT value = j INTO g RETURN g",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g = j._key RETURN g",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g RETURN [ value, g ]",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g KEEP j RETURN g"
      ];
      
      queries.forEach(function(query) {
        var plans = AQL_EXPLAIN(query, null, { allPlans: true, optimizer: { rules: [ "-all" ] } }).plans;

        assertEqual(1, plans.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect hash COLLECT
////////////////////////////////////////////////////////////////////////////////

    testHashed : function () {
      var queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j._key RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;

        var aggregateNodes = 0;
        var sortNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("hash", node.collectOptions.method);
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
        
        assertEqual(1, aggregateNodes);
        assertEqual(1, sortNodes);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect hash COLLECT
////////////////////////////////////////////////////////////////////////////////

    testHashedWithNonSortedIndex : function () {
      c.ensureIndex({ type: "hash", fields: [ "group" ] }); 
      c.ensureIndex({ type: "hash", fields: [ "group", "value" ] }); 

      var queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j._key RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;

        var aggregateNodes = 0;
        var sortNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("hash", node.collectOptions.method);
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
        
        assertEqual(1, aggregateNodes);
        assertEqual(1, sortNodes);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect sorted COLLECT
////////////////////////////////////////////////////////////////////////////////

    testSortedIndex : function () {
      c.ensureIndex({ type: "skiplist", fields: [ "group" ] }); 
      c.ensureIndex({ type: "skiplist", fields: [ "group", "value" ] }); 

      var queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;

        var aggregateNodes = 0;
        var sortNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("sorted", node.collectOptions.method);
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
        
        assertEqual(1, aggregateNodes);
        assertEqual(0, sortNodes);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect hash COLLECT w/ sort node removed
////////////////////////////////////////////////////////////////////////////////

    testSortRemoval : function () {
      var queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j SORT null RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j._key SORT null RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group SORT null RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value SORT null RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l SORT null RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l SORT null RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query[0]).plan;

        var aggregateNodes = 0;
        var sortNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("hash", node.collectOptions.method);
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
        
        assertEqual(1, aggregateNodes);
        assertEqual(0, sortNodes);

        var results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test override of collect method
////////////////////////////////////////////////////////////////////////////////

    testOverrideMethodWithHashIgnored : function () {
      c.ensureIndex({ type: "skiplist", fields: [ "group" ] }); 
      c.ensureIndex({ type: "skiplist", fields: [ "group", "value" ] }); 

      // the expectation is that the optimizer will still use the 'sorted' method here as there are
      // sorted indexes supporting it
      var queries = [
        "FOR j IN " + c.name() + " COLLECT value = j.group INTO g OPTIONS { method: 'hash' } RETURN [ value, g ]",
        "FOR j IN " + c.name() + " COLLECT value = j.group OPTIONS { method: 'hash' } RETURN value",
        "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value OPTIONS { method: 'hash' } RETURN [ value1, value2 ]",
        "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l OPTIONS { method: 'hash' } RETURN [ value, l ]",
        "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l OPTIONS { method: 'hash' } RETURN [ value1, value2, l ]"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;

        var aggregateNodes = 0;
        var sortNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("sorted", node.collectOptions.method);
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
        
        assertEqual(1, aggregateNodes);
        assertEqual(0, sortNodes);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test override of collect method
////////////////////////////////////////////////////////////////////////////////

    testOverrideMethodSortedUsed : function () {
      // the expectation is that the optimizer will use the 'sorted' method here because we
      // explicitly ask for it
      var queries = [
        "FOR j IN " + c.name() + " COLLECT value = j.group INTO g OPTIONS { method: 'sorted' } RETURN [ value, g ]",
        "FOR j IN " + c.name() + " COLLECT value = j.group OPTIONS { method: 'sorted' } RETURN value",
        "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value OPTIONS { method: 'sorted' } RETURN [ value1, value2 ]",
        "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l OPTIONS { method: 'sorted' } RETURN [ value, l ]",
        "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l OPTIONS { method: 'sorted' } RETURN [ value1, value2, l ]"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;

        var aggregateNodes = 0;
        var sortNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("sorted", node.collectOptions.method);
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
        
        assertEqual(1, aggregateNodes);
        assertEqual(1, sortNodes);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collects in single query
////////////////////////////////////////////////////////////////////////////////

    testMultipleCollectsInSingleQuery : function () {
      // this will tell the optimizer to optimize the cloned plan with this specific rule again
      var result = AQL_EXECUTE("LET values1 = (FOR i IN 1..4 COLLECT x = i RETURN x)  LET values2 = (FOR i IN 2..6 COLLECT x = i RETURN x) RETURN [ values1, values2 ]").json[0];

      assertEqual([ 1, 2, 3, 4 ], result[0]);
      assertEqual([ 2, 3, 4, 5, 6 ], result[1]);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerCollectMethodsTestSuite);

return jsunity.done();

