/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Lars Maier
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const db = require("@arangodb").db;
const jsunity = require("jsunity");
const console = require('console');
const _ = require("lodash");
const isEnterprise = require("internal").isEnterprise();

function optimizerRuleTestSuite() {
  const ruleName = "decay-unnecessary-sorted-gather";

  const sharedCollection = "c";
  const singleShardCollection = "d";

  const satelliteCollection = "sat";


  return {

    setUpAll: function () {
      db._create(sharedCollection, {numberOfShards: 3});
      db._create(singleShardCollection, {numberOfShards: 1});

      if (isEnterprise) {
        db._create(satelliteCollection, {replicationFactor: "satellite"});
      }
    },

    tearDownAll: function () {
      db._drop(sharedCollection);
      db._drop(singleShardCollection);
      db._drop(satelliteCollection);
    },

    testNonEnterprise: function () {
      // this query has to be that complicated, because
      // 1. if we only use a single collection with repl fact 1, the one shard rule will apply
      // 2. the FILTER forces the sharded collection to enumerated first
      let query = `FOR x IN ${sharedCollection} FILTER x.attr FOR y IN ${singleShardCollection} SORT y.attr RETURN {x, y}`;

      let result = AQL_EXPLAIN(query);

      let nodes = result.plan.nodes.filter(function (n) {
        return ["SortNode", "GatherNode"].includes(n.type);
      });

      assertEqual(3, nodes.length, "Expected number of Sort and Gather nodes");
      assertEqual("GatherNode", nodes[0].type);
      assertEqual("SortNode", nodes[1].type);
      assertEqual("GatherNode", nodes[2].type);

      assertEqual(0, nodes[2].elements.length, "Gather node should be unsorted");
      assertTrue(result.plan.rules.includes(ruleName));
    },


    testNonEnterpriseDisabled: function () {
      // this query has to be that complicated, because
      // 1. if we only use a single collection with repl fact 1, the one shard rule will apply
      // 2. the FILTER forces the sharded collection to enumerated first
      let query = `FOR x IN ${sharedCollection} FILTER x.attr FOR y IN ${singleShardCollection} SORT y.attr RETURN {x, y}`;
      let result = AQL_EXPLAIN(query, null, {optimizer: {rules: ["-" + ruleName]}});
      let nodes = result.plan.nodes.filter(function (n) {
        return ["SortNode", "GatherNode"].includes(n.type);
      });

      assertEqual(3, nodes.length, "Expected number of Sort and Gather nodes");
      assertEqual("GatherNode", nodes[0].type);
      assertEqual("SortNode", nodes[1].type);
      assertEqual("GatherNode", nodes[2].type);

      assertEqual(1, nodes[2].elements.length, "Gather node should still be sorted");

      assertFalse(result.plan.rules.includes(ruleName));
    },

    testNonEnterpriseNoMatch: function () {
      // Here we first scan the single shared collection, then the shared collection. The gather must therefore be sorted.
      let query = `FOR x IN ${singleShardCollection} FILTER x.attr FOR y IN ${sharedCollection} SORT y.attr RETURN {x, y}`;
      let result = AQL_EXPLAIN(query);
      let nodes = result.plan.nodes.filter(function (n) {
        return ["SortNode", "GatherNode"].includes(n.type);
      });

      assertEqual(3, nodes.length, "Expected number of Sort and Gather nodes");
      assertEqual("GatherNode", nodes[0].type);
      assertEqual("SortNode", nodes[1].type);
      assertEqual("GatherNode", nodes[2].type);

      assertEqual(1, nodes[2].elements.length, "Gather node should be sorted");
      assertFalse(result.plan.rules.includes(ruleName));
    },

    testWithSatelliteMultiShard: function () {
      if (!isEnterprise) {
        return ;
      }

      // Do a satellite join with a shared collection, i.e. gather should be sorted
      let query = `FOR x IN ${sharedCollection} FOR y IN ${satelliteCollection} FILTER x.ref == y.ref SORT x.attr RETURN {x, y}`;
      let result = AQL_EXPLAIN(query);
      let nodes = result.plan.nodes.filter(function (n) {
        return ["SortNode", "GatherNode"].includes(n.type);
      });

      assertEqual(2, nodes.length, "Expected number of Sort and Gather nodes");
      assertEqual("SortNode", nodes[0].type);
      assertEqual("GatherNode", nodes[1].type);

      assertTrue(1 === nodes[1].elements.length, "Gather node should be sorted");
      assertFalse(result.plan.rules.includes(ruleName));
    },


    testWithSatelliteSingelShard: function () {
      if (!isEnterprise) {
        return ;
      }

      // Do a satellite join with a shared collection, i.e. gather should be sorted
      let query = `FOR x IN ${sharedCollection} FOR y IN ${singleShardCollection} FILTER y.attr FOR z IN ${satelliteCollection} FILTER y.ref == z.ref SORT x.attr RETURN {x, y, z}`;
      let result = AQL_EXPLAIN(query);
      let nodes = result.plan.nodes.filter(function (n) {
        return ["SortNode", "GatherNode"].includes(n.type);
      });

      assertEqual(3, nodes.length, "Expected number of Sort and Gather nodes");
      assertEqual("GatherNode", nodes[0].type);
      assertEqual("SortNode", nodes[1].type);
      assertEqual("GatherNode", nodes[2].type);

      assertTrue(0 === nodes[2].elements.length, "Gather node should not be sorted");
      assertTrue(result.plan.rules.includes(ruleName));
    }

    /*
        testDisabled: function () {
          let queries = [
            `FOR v, e, p IN 1..3 OUTBOUND "${vertexCollectionName}/0" GRAPH "${graphName}" RETURN v`
          ];

          queries.forEach(function (query) {
            let result = AQL_EXPLAIN(query);
            assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
          });

          queries.forEach(function (query) {
            let result = AQL_EXPLAIN(query, null, {optimizer: {rules: ["-" + ruleName]}});
            assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
          });
        },

        testRuleNoEffect: function () {
          let queries = [
            `FOR v, e, p IN 1..3 OUTBOUND "${vertexCollectionName}/0" GRAPH "${graphName}" RETURN v`,
            `FOR doc IN ${collectionName} SORT doc.x FOR v, e, p IN 1..3 OUTBOUND doc.ref GRAPH "${graphName}" RETURN v`
          ];

          queries.forEach(function (query) {
            let result = AQL_EXPLAIN(query);
            assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
          });
        },

        testRuleHasEffect: function () {
          let queries = [
            `FOR doc IN ${collectionName} FOR v, e, p IN 1..3 OUTBOUND doc.ref GRAPH "${graphName}" RETURN v`,
            `FOR doc IN ${collectionName} FOR v, e, p IN 1..3 OUTBOUND doc.ref GRAPH "${graphName}" RETURN v`,
            `FOR v, e, p IN 1..3 OUTBOUND "v/1" GRAPH "${graphName}" FOR doc IN ${collectionName} FILTER v.ref == doc.ref RETURN v`
          ];

          queries.forEach(function (query) {
            let result = AQL_EXPLAIN(query);
            assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
          });
        },*/
  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
