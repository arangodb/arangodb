/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertFalse, AQL_EXPLAIN */
////////////////////////////////////////////////////////////////////////////////
/// @brief tests for optimizer rules
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

let jsunity = require("jsunity");
let db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function explainSuite () {
  const cn = "UnitTestsAhuacatlExplain";

  return {

    setUpAll : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDownAll : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test nodes in plan
////////////////////////////////////////////////////////////////////////////////

    testNodes : function () {
      const query = "FOR i IN " + cn + " FILTER i.value > 1 LET a = i.value / 2 SORT a DESC COLLECT x = a INTO g RETURN x";
      
      let actual = AQL_EXPLAIN(query, null, { optimizer: { rules: [ "-all" ] } });
      let nodes = actual.plan.nodes, node;

      let n = 0;
      node = nodes[n++];
      assertEqual("SingletonNode", node.type);
      assertEqual([ ], node.dependencies);
      assertEqual(1, node.id);
      assertEqual(1, node.estimatedCost);
      let prev = node.id;
      
      node = nodes[n++];
      assertEqual("ScatterNode", node.type);
      assertEqual([ prev ], node.dependencies);
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("RemoteNode", node.type);
      assertEqual([ prev ], node.dependencies);
      assertTrue(node.estimatedCost >= 1.0);
      prev = node.id;
     
      node = nodes[n++];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual([ prev ], node.dependencies);
      assertEqual("_system", node.database);
      assertEqual(cn, node.collection);
      assertEqual("i", node.outVariable.name);
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("RemoteNode", node.type);
      assertEqual([ prev ], node.dependencies);
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("GatherNode", node.type);
      assertEqual([ prev ], node.dependencies);
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("CalculationNode", node.type);
      assertEqual([ prev ], node.dependencies);
      assertTrue(node.hasOwnProperty("expression"));
      assertFalse(node.canThrow);
      let out = node.outVariable.name;
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("FilterNode", node.type);
      assertEqual([ prev ], node.dependencies);
      assertEqual(out, node.inVariable.name);
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("CalculationNode", node.type);
      assertEqual([ prev ], node.dependencies);
      assertTrue(node.hasOwnProperty("expression"));
      assertEqual("a", node.outVariable.name);
      assertFalse(node.canThrow);
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("SortNode", node.type);
      assertEqual([ prev ], node.dependencies);
      assertEqual(1, node.elements.length);
      assertEqual("a", node.elements[0].inVariable.name);
      assertFalse(node.stable);
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("SortNode", node.type);
      assertEqual([ prev ], node.dependencies);
      assertEqual(1, node.elements.length);
      assertEqual("a", node.elements[0].inVariable.name);
      assertTrue(node.stable);
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("CollectNode", node.type);
      assertEqual([ prev ], node.dependencies);
      assertEqual(1, node.groups.length);
      assertEqual("a", node.groups[0].inVariable.name);
      assertEqual("x", node.groups[0].outVariable.name);
      assertEqual("g", node.outVariable.name);
      prev = node.id;
      
      node = nodes[n++];
      assertEqual("ReturnNode", node.type);
      assertEqual([ prev ], node.dependencies);
      assertEqual("x", node.inVariable.name);
    }

  };
}

jsunity.run(explainSuite);

return jsunity.done();
