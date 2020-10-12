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

      node = nodes[0];
      assertEqual("SingletonNode", node.type);
      assertEqual([ ], node.dependencies);
      assertEqual(1, node.id);
      
      node = nodes[1];
      assertEqual("EnumerateCollectionNode", node.type);
      assertEqual([ 1 ], node.dependencies);
      assertEqual(2, node.id);
      assertEqual("_system", node.database);
      assertEqual(cn, node.collection);
      assertEqual("i", node.outVariable.name);
      
      node = nodes[2];
      assertEqual("CalculationNode", node.type);
      assertEqual([ 2 ], node.dependencies);
      assertEqual(3, node.id);
      assertTrue(node.hasOwnProperty("expression"));
      assertFalse(node.canThrow);
      let out = node.outVariable.name;
      
      node = nodes[3];
      assertEqual("FilterNode", node.type);
      assertEqual([ 3 ], node.dependencies);
      assertEqual(4, node.id);
      assertEqual(out, node.inVariable.name);
      
      node = nodes[4];
      assertEqual("CalculationNode", node.type);
      assertEqual([ 4 ], node.dependencies);
      assertEqual(5, node.id);
      assertTrue(node.hasOwnProperty("expression"));
      assertEqual("a", node.outVariable.name);
      assertFalse(node.canThrow);
      
      node = nodes[5];
      assertEqual("SortNode", node.type);
      assertEqual([ 5 ], node.dependencies);
      assertEqual(6, node.id);
      assertEqual(1, node.elements.length);
      assertEqual("a", node.elements[0].inVariable.name);
      assertFalse(node.stable);

      node = nodes[6];
      assertEqual("SortNode", node.type);
      assertEqual([ 6 ], node.dependencies);
      assertEqual(9, node.id);
      assertEqual(1, node.elements.length);
      assertEqual("a", node.elements[0].inVariable.name);
      assertTrue(node.stable);
      
      node = nodes[7];
      assertEqual("CollectNode", node.type);
      assertEqual([ 9 ], node.dependencies);
      assertEqual(7, node.id);
      assertEqual(1, node.groups.length);
      assertEqual("a", node.groups[0].inVariable.name);
      assertEqual("x", node.groups[0].outVariable.name);
      assertEqual("g", node.outVariable.name);
      
      node = nodes[8];
      assertEqual("ReturnNode", node.type);
      assertEqual([ 7 ], node.dependencies);
      assertEqual(8, node.id);
      assertEqual("x", node.inVariable.name);
    }

  };
}

jsunity.run(explainSuite);

return jsunity.done();
