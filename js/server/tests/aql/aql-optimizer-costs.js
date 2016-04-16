/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXPLAIN */

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

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerCostsTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 150; ++i) {
        c.save({ _key: "test" + i, value: i });
      }
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
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
      var query = "FOR i IN PASSTHRU([ 1, 2, 3 ]) RETURN i";

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
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerCostsTestSuite);

return jsunity.done();

