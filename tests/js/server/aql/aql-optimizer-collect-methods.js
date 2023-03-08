/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, AQL_EXECUTE, AQL_EXPLAIN */

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

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const isCluster = require("@arangodb/cluster").isCluster();

function optimizerCollectMethodsTestSuite () {
  let c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 3 });

      let docs = [];
      for (let i = 0; i < 1500; ++i) {
        docs.push({ group: "test" + (i % 10), value: i, haxe: "test" + i });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

    testLargeResult : function () {
      ["hash", "sorted"].forEach((method) => {
        const query = "FOR i IN 0..99999 COLLECT group = i % 10000 AGGREGATE min = MIN(i), max = MAX(i) OPTIONS { method: '" + method + "'} SORT group RETURN { group, min, max }";
        let results = AQL_EXECUTE(query);
        assertEqual(10000, results.json.length);

        for (let i = 0; i < 10000; ++i) {
          assertEqual(i, results.json[i].group);
          assertEqual(i, results.json[i].min);
          assertEqual(i + 90000, results.json[i].max);
        }

        let nodes = AQL_EXPLAIN(query).plan.nodes.filter((n) => n.type === 'CollectNode');
        assertEqual(1, nodes.length);
        assertEqual(method, nodes[0].collectOptions.method);
      });
    },
    
    testLargeResultWithInto : function () {
      ["hash", "sorted"].forEach((method) => {
        const query = "FOR i IN 0..99999 COLLECT group = i % 10000 AGGREGATE min = MIN(i), max = MAX(i) INTO values OPTIONS { method: '" + method + "'} SORT group RETURN { group, min, max, values }";
        let results = AQL_EXECUTE(query);
        assertEqual(10000, results.json.length);

        for (let i = 0; i < 10000; ++i) {
          assertEqual(i, results.json[i].group);
          assertEqual(i, results.json[i].min);
          assertEqual(i + 90000, results.json[i].max);
          assertEqual(10, results.json[i].values.length);
          results.json[i].values.sort((lhs, rhs) => {
            return lhs.i - rhs.i;
          });
          for (let j = 0; j < 10; ++j) {
            assertEqual(i + j * 10000, results.json[i].values[j].i);
          }
        }

        let nodes = AQL_EXPLAIN(query).plan.nodes.filter((n) => n.type === 'CollectNode');
        assertEqual(1, nodes.length);
        assertEqual(method, nodes[0].collectOptions.method);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief number of plans
////////////////////////////////////////////////////////////////////////////////

    testNumberOfPlans : function () {
      c.ensureIndex({ type: "persistent", fields: [ "value" ] }); 
      const queries = [
        "FOR j IN " + c.name() + " COLLECT value = j RETURN value",
        "FOR j IN " + c.name() + " COLLECT value = j WITH COUNT INTO l RETURN [ value, l ]",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g RETURN g",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g = j.haxe RETURN g",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g RETURN [ value, g ]",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g KEEP j RETURN g"
      ];
      
      queries.forEach(function(query) {
        let plans = AQL_EXPLAIN(query, null, { allPlans: true, optimizer: { rules: [ "-all" ] } }).plans;
        assertEqual(2, plans.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief number of plans
////////////////////////////////////////////////////////////////////////////////

    testNumberOfPlansWithIntoSorted : function () {
      const queries = [
        "FOR j IN " + c.name() + " COLLECT value = j INTO g OPTIONS { method: 'sorted' } RETURN g",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g = j.haxe OPTIONS { method: 'sorted' } RETURN g",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g OPTIONS { method: 'sorted' } RETURN [ value, g ]",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g KEEP j OPTIONS { method: 'sorted' } RETURN g"
      ];
      
      queries.forEach(function(query) {
        let plans = AQL_EXPLAIN(query, null, { allPlans: true, optimizer: { rules: [ "-all" ] } }).plans;
        assertEqual(1, plans.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief number of plans
////////////////////////////////////////////////////////////////////////////////

    testNumberOfPlansWithIntoHash : function () {
      const queries = [
        "FOR j IN " + c.name() + " COLLECT value = j INTO g OPTIONS { method: 'hash' } RETURN g",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g = j.haxe OPTIONS { method: 'hash' } RETURN g",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g OPTIONS { method: 'hash' } RETURN [ value, g ]",
        "FOR j IN " + c.name() + " COLLECT value = j INTO g KEEP j OPTIONS { method: 'hash' } RETURN g"
      ];
      
      queries.forEach(function(query) {
        let plans = AQL_EXPLAIN(query, null, { allPlans: true, optimizer: { rules: [ "-all" ] } }).plans;
        assertEqual(1, plans.length);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect hash COLLECT
////////////////////////////////////////////////////////////////////////////////

    testDefaultsToHashed : function () {
      const queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.haxe RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500 ],
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let aggregateNodes = 0;
        let sortNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("hash", node.collectOptions.method);
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
       
        assertEqual(isCluster ? 2 : 1, aggregateNodes, query);
        assertEqual(1, sortNodes);

        let results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief expect hash COLLECT
    ////////////////////////////////////////////////////////////////////////////////
    
    testHashedWithNonSortedIndex : function () {
      c.ensureIndex({ type: "hash", fields: [ "group" ] });
      c.ensureIndex({ type: "hash", fields: [ "group", "value" ] });
      
      const queries = [
                     [ "FOR j IN " + c.name() + " COLLECT value = j RETURN value", 1500, false],
                     [ "FOR j IN " + c.name() + " COLLECT value = j.haxe RETURN value", 1500, false],
                     [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", 10, true],
                     [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500, true ],
                     [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10, true ],
                     [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500, true ]
                     ];
      
      queries.forEach(function(query) {
          let plan = AQL_EXPLAIN(query[0]).plan;
          let aggregateNodes = 0;
          let sortNodes = 0;
          plan.nodes.map(function(node) {
            if (node.type === "CollectNode") {
              ++aggregateNodes;
              assertFalse(query[2] && node.collectOptions.method !== "sorted");
              assertEqual(query[2] ? "sorted" : "hash",
                         node.collectOptions.method, query[0]);
            }
            if (node.type === "SortNode") {
              ++sortNodes;
            }
          });

          assertEqual(isCluster ? 2 : 1, aggregateNodes);
          assertEqual(query[2] ? 0 : 1, sortNodes);
          
          let results = AQL_EXECUTE(query[0]);
          assertEqual(query[1], results.json.length);
       });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect sorted COLLECT
////////////////////////////////////////////////////////////////////////////////

    testSortedIndex : function () {
      c.ensureIndex({ type: "persistent", fields: [ "group" ] });
      c.ensureIndex({ type: "persistent", fields: [ "group", "value" ] });

      const queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let aggregateNodes = 0;
        let sortNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("sorted", node.collectOptions.method);
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });

        assertEqual(isCluster ? 2 : 1, aggregateNodes);
        assertEqual(0, sortNodes);

        let results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    },

    testSortedIndex2 : function () {
      c.ensureIndex({ type: "persistent", fields: [ "group", "value" ] });

      const queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = 1 RETURN value", 1 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j RETURN 1", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.value RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.value WITH COUNT INTO l RETURN [ value, l ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let aggregateNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
          }
        });

        assertEqual(isCluster ? 2 : 1, aggregateNodes);

        let results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief expect hash COLLECT w/ sort node removed
////////////////////////////////////////////////////////////////////////////////

    testSortRemoval : function () {
      const queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j SORT null RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.haxe SORT null RETURN value", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group SORT null RETURN value", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value SORT null RETURN [ value1, value2 ]", 1500 ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l SORT null RETURN [ value, l ]", 10 ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l SORT null RETURN [ value1, value2, l ]", 1500 ]
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let aggregateNodes = 0;
        let sortNodes = 0;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("hash", node.collectOptions.method);
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
        
        assertEqual(isCluster ? 2 : 1, aggregateNodes);
        assertEqual(0, sortNodes);

        let results = AQL_EXECUTE(query[0]);
        assertEqual(query[1], results.json.length);
      });
    },

    testSkip : function () {
      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ value: "test" + i });
      }
      c.insert(docs);

      const query = "FOR doc IN " + c.name() + " COLLECT v = doc.value INTO group OPTIONS { method: 'sorted' } LIMIT 1, 2 RETURN group"; 

      let plan = AQL_EXPLAIN(query).plan;
      let aggregateNodes = 0;
      let sortNodes = 0;
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

      let result = AQL_EXECUTE(query).json;
      assertEqual(2, result.length);
      assertTrue(Array.isArray(result[0]));
      assertTrue(Array.isArray(result[1]));
      assertEqual(1, result[0].length);
      assertEqual(1, result[1].length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test override of collect method
////////////////////////////////////////////////////////////////////////////////

    testOverrideMethodWithHashButHavingIndex : function () {
      c.ensureIndex({ type: "persistent", fields: [ "group" ] }); 
      c.ensureIndex({ type: "persistent", fields: [ "group", "value" ] }); 

      // the expectation is that the optimizer will still use the 'sorted' method here as there are
      // sorted indexes supporting it
      const queries = [
        [ "FOR j IN " + c.name() + " COLLECT value = j.group INTO g OPTIONS { method: 'hash' } RETURN [ value, g ]", "hash" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group INTO g OPTIONS { method: 'sorted' } RETURN [ value, g ]", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group INTO g RETURN [ value, g ]", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group OPTIONS { method: 'hash' } RETURN value", "hash" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group OPTIONS { method: 'sorted' } RETURN value", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group OPTIONS { method: 'hash' } RETURN value", "hash" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group OPTIONS { method: 'sorted' } RETURN value", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group RETURN value", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value OPTIONS { method: 'hash' } RETURN [ value1, value2 ]", "hash" ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value OPTIONS { method: 'sorted' } RETURN [ value1, value2 ]", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value RETURN [ value1, value2 ]", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l OPTIONS { method: 'hash' } RETURN [ value, l ]", "hash" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l OPTIONS { method: 'sorted' } RETURN [ value, l ]", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l RETURN [ value, l ]", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l OPTIONS { method: 'hash' } RETURN [ value1, value2, l ]", "hash" ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l OPTIONS { method: 'sorted' } RETURN [ value1, value2, l ]", "sorted" ],
        [ "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l RETURN [ value1, value2, l ]", "sorted" ]
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let aggregateNodes = 0;
        let sortNodes = 0;
        let hasInto = false;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual(query[1], node.collectOptions.method, query);
            if (node.outVariable) {
              hasInto = true;
            }
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
       
        assertEqual((isCluster && !hasInto) ? 2 : 1, aggregateNodes);
        assertEqual(query[1] === 'hash' ? 1 : 0, sortNodes);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test override of collect method
////////////////////////////////////////////////////////////////////////////////

    testOverrideMethodSortedUsed : function () {
      // the expectation is that the optimizer will use the 'sorted' method here because we
      // explicitly ask for it
      const queries = [
        "FOR j IN " + c.name() + " COLLECT value = j.group INTO g OPTIONS { method: 'sorted' } RETURN [ value, g ]",
        "FOR j IN " + c.name() + " COLLECT value = j.group OPTIONS { method: 'sorted' } RETURN value",
        "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value OPTIONS { method: 'sorted' } RETURN [ value1, value2 ]",
        "FOR j IN " + c.name() + " COLLECT value = j.group WITH COUNT INTO l OPTIONS { method: 'sorted' } RETURN [ value, l ]",
        "FOR j IN " + c.name() + " COLLECT value1 = j.group, value2 = j.value WITH COUNT INTO l OPTIONS { method: 'sorted' } RETURN [ value1, value2, l ]"
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query).plan;
        let aggregateNodes = 0;
        let sortNodes = 0;
        let hasInto = false;
        plan.nodes.map(function(node) {
          if (node.type === "CollectNode") {
            ++aggregateNodes;
            assertEqual("sorted", node.collectOptions.method);
            if (node.outVariable) {
              hasInto = true;
            }
          }
          if (node.type === "SortNode") {
            ++sortNodes;
          }
        });
        
        assertEqual((isCluster && !hasInto) ? 2 : 1, aggregateNodes);
        assertEqual(1, sortNodes);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple collects in single query
////////////////////////////////////////////////////////////////////////////////

    testMultipleCollectsInSingleQuery : function () {
      // this will tell the optimizer to optimize the cloned plan with this specific rule again
      let result = AQL_EXECUTE("LET values1 = (FOR i IN 1..4 COLLECT x = i RETURN x)  LET values2 = (FOR i IN 2..6 COLLECT x = i RETURN x) RETURN [ values1, values2 ]").json[0];

      assertEqual([ 1, 2, 3, 4 ], result[0]);
      assertEqual([ 2, 3, 4, 5, 6 ], result[1]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collect result bigger than block size
////////////////////////////////////////////////////////////////////////////////

    testCollectResultBiggerThanBlocksize : function () {
      let result = AQL_EXECUTE("FOR doc IN " + c.name() + " COLLECT id = doc.value INTO g RETURN { id, g }").json;
      assertEqual(1500, result.length);
      
      result = AQL_EXECUTE("FOR doc IN " + c.name() + " COLLECT id = doc.group INTO g RETURN { id, g }").json;
      assertEqual(10, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test many collects
////////////////////////////////////////////////////////////////////////////////
   
    testManyCollects : function () {
      c.truncate({ compact: false });
      c.insert({ value: 3 });
      let q = "", g = [];
      for (let i = 0; i < 10; ++i) {
        q += "LET q" + i + " = (FOR doc IN " + c.name() + " COLLECT id = doc.value RETURN id) ";
        g.push("q" + i);
      }
      q += "RETURN INTERSECTION(" + g.join(", ") + ")";
      assertTrue(AQL_EXPLAIN(q, null).stats.plansCreated >= 128);
      let result = AQL_EXECUTE(q).json;
      assertEqual([3], result[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collect with offset
////////////////////////////////////////////////////////////////////////////////
 
    testCollectWithOffset : function () {
      // create a persistent index so the optimizer can use the sorted collect
      c.ensureIndex({ type: "persistent", fields: [ "value" ] });
      const query = "FOR doc IN " + c.name() + " COLLECT v = doc.value OPTIONS { method: 'sorted' } LIMIT 1001, 2 RETURN v";
        
      let plan = AQL_EXPLAIN(query).plan;
      // we want a sorted collect!
      let aggregateNodes = 0;
      let hasInto = false;
      plan.nodes.map(function(node) {
        assertNotEqual("SortNode", node.type);
        if (node.type === "CollectNode") {
          ++aggregateNodes;
          assertEqual("sorted", node.collectOptions.method);
          if (node.outVariable) {
            hasInto = true;
          }
        }
      });
    
      assertEqual((isCluster && !hasInto) ? 2 : 1, aggregateNodes);
      
      let result = AQL_EXECUTE(query).json;
      assertEqual([ 1001, 1002 ], result);
    },
  
////////////////////////////////////////////////////////////////////////////////
/// @brief regression test external memory management
////////////////////////////////////////////////////////////////////////////////
 
    testRegressionMemoryManagementInHashedCollect : function () {
      // This query triggers the following case:
      // We have large input documents in `doc` which require
      // non-inlined AQL values.
      // Now we go above batch sizes with other values
      // This way we have external management that needs
      // to be manged accross different input and output blocks of AQL
      const query = `
        FOR doc IN ${c.name()}
          LIMIT 2
          FOR other IN 1..1001
            COLLECT d = doc, o = other
            RETURN 1`;
      const res = db._query(query).toArray();
      // We do not care for the result,
      // ASAN would figure out invalid memory access here.
      assertEqual(2002, res.length);
    },

    testRegressionMemoryManagementInSortedCollect : function () {
      // This query triggers the following case:
      // We have large input documents in `doc` which require
      // non-inlined AQL values.
      // Now we go above batch sizes with other values
      // This way we have external management that needs
      // to be manged accross different input and output blocks of AQL
      const query = `
        FOR doc IN ${c.name()}
          LIMIT 2
          FOR other IN 1..1001
            SORT doc, other
            COLLECT d = doc, o = other
            RETURN 1`;
      const res = db._query(query).toArray();
      // We do not care for the result,
      // ASAN would figure out invalid memory access here.
      assertEqual(2002, res.length);
    },

    // Regression test. There was a bug where the sorted collect returned
    // [ { "n" : undefined }, { "n" : "testi" } ]
    // instead of
    // [ { "n" : null }, { "n" : "testi" } ].
    testNullVsUndefinedInSortedCollect : function () {
      const query = `
        FOR doc IN [ {_key:"foo"}, {_key:"bar", name:"testi"} ]
        COLLECT n = doc.name OPTIONS { method: "sorted" }
        RETURN { n }
      `;
      const res = db._query(query).toArray();
      assertEqual([ { "n" : null }, { "n" : "testi" } ], res);
    },

    // Just for the sake of completeness, a version of the regression test above
    // for the hashed collect variant.
    testNullVsUndefinedInHashedCollect : function () {
      const query = `
        FOR doc IN [ {_key:"foo"}, {_key:"bar", name:"testi"} ]
        COLLECT n = doc.name OPTIONS { method: "hash" }
        RETURN { n }
      `;
      const res = db._query(query).toArray();
      assertEqual([ { "n" : null }, { "n" : "testi" } ], res);
    },
  
  };
}

jsunity.run(optimizerCollectMethodsTestSuite);

return jsunity.done();
