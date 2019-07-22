/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

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
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");
var assertQueryError = helper.assertQueryError;
const isCluster = require("@arangodb/cluster").isCluster();

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerCollectInClusterSuite () {
  let c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 3 });

      for (let i = 0; i < 1000; ++i) {
        c.save({ group: "test" + (i % 10), value: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },
    
    testCount : function () {
      let query = "FOR doc IN " + c.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);
       
      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },
    
    testCountMulti : function () {
      let query = "FOR doc1 IN " + c.name() + " FILTER doc1.value < 10 FOR doc2 IN " + c.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(10000, results.json[0]);
       
      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },

    testDistinct : function () {
      let query = "FOR doc IN " + c.name() + " SORT doc.value RETURN DISTINCT doc.value";

      let results = AQL_EXECUTE(query);
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }
       
      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },
    
    testDistinctMulti : function () {
      let query = "FOR doc1 IN " + c.name() + " FILTER doc1.value < 10 FOR doc2 IN " + c.name() + " SORT doc2.value RETURN DISTINCT doc2.value";

      let results = AQL_EXECUTE(query, null, { optimizer: { rules: ["-interchange-adjacent-enumerations"] } });
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }
       
      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },

  };
}

function optimizerCollectInClusterSingleShardSuite () {
  let c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 1 });

      for (let i = 0; i < 1000; ++i) {
        c.save({ group: "test" + (i % 10), value: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },
    
    testSingleCount : function () {
      let query = "FOR doc IN " + c.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(1000, results.json[0]);
       
      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },
    
    testSingleCountMulti : function () {
      let query = "FOR doc1 IN " + c.name() + " FILTER doc1.value < 10 FOR doc2 IN " + c.name() + " COLLECT WITH COUNT INTO length RETURN length";

      let results = AQL_EXECUTE(query);
      assertEqual(1, results.json.length);
      assertEqual(10000, results.json[0]);
       
      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type === 'IndexNode' ? 'EnumerateCollectionNode' : node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },


    testSingleDistinct : function () {
      let query = "FOR doc IN " + c.name() + " SORT doc.value RETURN DISTINCT doc.value";

      let results = AQL_EXECUTE(query);
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }
       
      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },
    
    testSingleDistinctMulti : function () {
      let query = "FOR doc1 IN " + c.name() + " FILTER doc1.value < 10 FOR doc2 IN " + c.name() + " SORT doc2.value RETURN DISTINCT doc2.value";

      let results = AQL_EXECUTE(query, null, { optimizer: { rules: ["-interchange-adjacent-enumerations"] } });
      assertEqual(1000, results.json.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual(i, results.json[i]);
      }
       
      let plan = AQL_EXPLAIN(query).plan;
      let nodeTypes = plan.nodes.map(function(node) {
        return node.type;
      });

      assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      assertEqual(["SingletonNode", "EnumerateCollectionNode", "CalculationNode", "FilterNode", "RemoteNode", "GatherNode", "ScatterNode", "RemoteNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "CollectNode", "RemoteNode", "GatherNode", "CollectNode", "ReturnNode"], nodeTypes);
    },

  };
}

jsunity.run(optimizerCollectInClusterSuite);
jsunity.run(optimizerCollectInClusterSingleShardSuite);

return jsunity.done();
