/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for cost estimation
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
const {assertEqual} = jsunity.jsUnity.assertions;
const db = require("@arangodb").db;
const helper = require("@arangodb/aql-helper");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerCostsTestSuite () {
  var c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 150; ++i) {
        c.save({ _key: "test" + i, value: i });
      }
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test cost of LimitNode
////////////////////////////////////////////////////////////////////////////////

    testLimitNodeCost : function () {
      const query = "FOR i IN 1..10000 LIMIT @offset, @limit RETURN i";

      // [ offset, limit, expected number of items ]
      let tests = [ 
        [ 0, 0, 0, 10002 ],
        [ 0, 1, 1, 10003 ],
        [ 0, 10, 10, 10012 ],
        [ 0, 100, 100, 10102 ],
        [ 0, 1000, 1000, 11002 ],
        [ 0, 9999, 9999, 20001 ],
        [ 0, 10000, 10000, 20002 ],
        [ 0, 100000, 10000, 20002 ],
        [ 0, 1000000, 10000, 20002 ],
        [ 0, 10000000, 10000, 20002 ],
        [ 0, 100000000, 10000, 20002 ],
        [ 1, 0, 0, 10002.000001 ],
        [ 1, 1, 1, 10003.000001 ],
        [ 1, 10, 10, 10012.000001 ],
        [ 1, 100, 100, 10102.000001 ],
        [ 1, 1000, 1000, 11002.000001 ],
        [ 1, 9999, 9999, 20001.000001 ],
        [ 1, 10000, 9999, 20001.000001 ],
        [ 1, 100000, 9999, 20001.000001 ],
        [ 1, 1000000000, 9999, 20001.00000 ],
        [ 10, 0, 0, 10002.00001 ],
        [ 10, 1, 1, 10003.00001 ],
        [ 10, 10, 10, 10012.00001 ],
        [ 10, 100, 100, 10102.00001 ],
        [ 10, 9999, 9990, 19992.00001 ],
        [ 10, 10000, 9990, 19992.00001 ],
        [ 10, 1000000000, 9990, 19992.00001 ],
        [ 1000, 0, 0, 10002.00100 ],
        [ 1000, 1, 1, 10003.00100 ],
        [ 1000, 1000, 1000, 11002.00100 ],
        [ 1000, 9999, 9000, 19002.00100 ],
        [ 1000, 10000, 9000, 19002.00100 ],
        [ 1000, 1000000000, 9000, 19002.00100 ],
        [ 5000, 0, 0, 10002.00500 ],
        [ 5000, 1, 1, 10003.00500 ],
        [ 5000, 1000, 1000, 11002.00500 ],
        [ 5000, 9999, 5000, 15002.00500 ],
        [ 5000, 10000, 5000, 15002.00500 ],
        [ 5000, 1000000000, 5000, 15002.00500 ],
        [ 9999, 0, 0, 10002.01000 ],
        [ 9999, 1, 1, 10003.01000 ],
        [ 9999, 2, 1, 10003.01000 ],
        [ 9999, 10, 1, 10003.01000 ],
        [ 9999, 1000, 1, 10003.01000 ],
        [ 9999, 10000, 1, 10003.01000 ],
        [ 9999, 1000000, 1, 10003.01000 ],
        [ 10000, 0, 0, 10002.01000 ],
        [ 10000, 1, 0, 10002.01000 ],
        [ 10000, 2, 0, 10002.01000 ],
        [ 10000, 10, 0, 10002.01000 ],
        [ 10000, 100, 0, 10002.01000 ],
        [ 10000, 1000, 0, 10002.01000 ],
        [ 10000, 10000, 0, 10002.01000 ],
        [ 10000, 100000, 0, 10002.01000 ],
        [ 10000, 1000000, 0, 10002.01000 ],
        [ 10000, 10000000, 0, 10002.01000 ],
        [ 10000, 10000000000000, 0, 10002.01000 ],
        [ 10000000, 0, 0, 10002.01000 ],
        [ 10000000, 1, 0, 10002.01000 ],
        [ 10000000, 2, 0, 10002.01000 ],
        [ 10000000, 10, 0, 10002.01000 ],
        [ 10000000, 100, 0, 10002.01000 ],
        [ 10000000, 1000, 0, 10002.01000 ],
        [ 10000000, 10000, 0, 10002.01000 ],
        [ 10000000, 100000, 0, 10002.01000 ],
        [ 10000000, 1000000, 0, 10002.01000 ],
        [ 10000000, 10000000, 0, 10002.01000 ],
        [ 10000000, 10000000000000, 0, 10002.01000 ],
        [ 100000000000, 0, 0, 10002.01000 ],
        [ 100000000000, 10, 0, 10002.01000 ],
        [ 100000000000, 1000, 0, 10002.01000 ],
        [ 100000000000, 10000000, 0, 10002.01000 ],
        [ 100000000000, 10000000000000, 0, 10002.01000 ],
      ];

      tests.forEach(function(test) {
        let plan = AQL_EXPLAIN(query, { offset: test[0], limit: test[1] });
        let node = helper.findExecutionNodes(plan, "LimitNode")[0];

        // number of nodes cannot be estimated properly due to function call
        assertEqual("LimitNode", node.type);
        assertEqual(test[2], node.estimatedNrItems, test);
        assertEqual(test[3].toFixed(5), node.estimatedCost.toFixed(5), test);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test cost of EnumerateListNode
////////////////////////////////////////////////////////////////////////////////

    testEnumerateListNodeConst : function () {
      var query = "FOR i IN [ 1, 2, 3 ] RETURN i";

      var plan = AQL_EXPLAIN(query);
      var node = helper.findExecutionNodes(plan, "EnumerateListNode")[0];
      
      // number of nodes can be estimated properly 
      assertEqual("EnumerateListNode", node.type);
      assertEqual(3, node.estimatedNrItems);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test cost of EnumerateListNode
////////////////////////////////////////////////////////////////////////////////

    testEnumerateListNodeFunction : function () {
      var query = "FOR i IN NOOPT([ 1, 2, 3 ]) RETURN i";

      var plan = AQL_EXPLAIN(query);
      var node = helper.findExecutionNodes(plan, "EnumerateListNode")[0];
       
      // number of nodes cannot be estimated properly due to function call
      assertEqual("EnumerateListNode", node.type);
      assertEqual(100, node.estimatedNrItems);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test cost of EnumerateListNode
////////////////////////////////////////////////////////////////////////////////

    testEnumerateListNodeSubquery : function () {
      var query = "FOR i IN (FOR j IN [ 1, 2, 3, 4 ] RETURN j) RETURN i";

      var plan = AQL_EXPLAIN(query, { }, { optimizer: { "rules" : [ "-all" ] } });
      var node = helper.findExecutionNodes(plan, "EnumerateListNode")[0];
       
      // number of nodes cannot be estimated properly due to function call
      assertEqual("EnumerateListNode", node.type);
      assertEqual(4, node.estimatedNrItems);
      
      // find the subquery node of the plan
      node = helper.findExecutionNodes(plan, "SubqueryNode")[0];
      // subquery always produces one result
      assertEqual(1, node.estimatedNrItems);

      // find the enumeratelistnode in the subquery
      node = helper.findExecutionNodes(node.subquery, "EnumerateListNode")[0];
      assertEqual(4, node.estimatedNrItems);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that spliced subqueries restore the number of lines at their end
///        node correctly.
////////////////////////////////////////////////////////////////////////////////

    testSplicedSubquery : function () {
      const query = `
        FOR i IN 1..3
          LET sq = (
            FOR k IN 1..5
              RETURN {i, k}
          )
          RETURN sq
      `;

      const explain = AQL_EXPLAIN(query);
      const nodes = explain.plan.nodes;
      const nodeTypes = nodes.map(n => n.type);

      // Check the rough outline first for a better overview in case of a failure
      // of the plan.
      assertEqual(["SingletonNode", "CalculationNode", "EnumerateListNode", "SubqueryStartNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "SubqueryEndNode", "ReturnNode"],
nodeTypes);

      let node, prevNode;
      // SingletonNode
      node = nodes[0];
      assertEqual("SingletonNode", node.type);
      assertEqual(1, node.estimatedNrItems);
      assertEqual(1, node.estimatedCost);
      // CalculationNode
      prevNode = node;
      node = nodes[1];
      assertEqual("CalculationNode", node.type);
      assertEqual(1, node.estimatedNrItems);
      assertEqual(1 + prevNode.estimatedCost, node.estimatedCost);
      // EnumerateListNode
      prevNode = node;
      node = nodes[2];
      assertEqual("EnumerateListNode", node.type);
      assertEqual(3, node.estimatedNrItems);
      assertEqual(3 + prevNode.estimatedCost, node.estimatedCost);
      // SubqueryStartNode
      prevNode = node;
      node = nodes[3];
      assertEqual("SubqueryStartNode", node.type);
      assertEqual(3, node.estimatedNrItems);
      assertEqual(3 + prevNode.estimatedCost, node.estimatedCost);
      // CalculationNode
      prevNode = node;
      node = nodes[4];
      assertEqual("CalculationNode", node.type);
      assertEqual(3, node.estimatedNrItems);
      assertEqual(3 + prevNode.estimatedCost, node.estimatedCost);
      // EnumerateListNode
      prevNode = node;
      node = nodes[5];
      assertEqual("EnumerateListNode", node.type);
      assertEqual(15, node.estimatedNrItems);
      assertEqual(15 + prevNode.estimatedCost, node.estimatedCost);
      // CalculationNode
      prevNode = node;
      node = nodes[6];
      assertEqual("CalculationNode", node.type);
      assertEqual(15, node.estimatedNrItems);
      assertEqual(15 + prevNode.estimatedCost, node.estimatedCost);
      // SubqueryEndNode
      prevNode = node;
      node = nodes[7];
      assertEqual("SubqueryEndNode", node.type);
      assertEqual(3, node.estimatedNrItems);
      assertEqual(3 + prevNode.estimatedCost, node.estimatedCost);
      // ReturnNode
      prevNode = node;
      node = nodes[8];
      assertEqual("ReturnNode", node.type);
      assertEqual(3, node.estimatedNrItems);
      assertEqual(3 + prevNode.estimatedCost, node.estimatedCost);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerCostsTestSuite);

return jsunity.done();

