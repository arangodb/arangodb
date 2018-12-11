/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNull, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for condition collapsing
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

function optimizerConditionsTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test negations
////////////////////////////////////////////////////////////////////////////////

    testNegation : function () {
      var query = "FOR doc IN " + c.name() + " FILTER doc.a != null && LENGTH(doc.a) > 0 && !(doc.a == 'abc' || doc.a == 'def' || doc.a == 'xyz') RETURN doc";

      var nodes = AQL_EXPLAIN(query).plan.nodes;
 
      var calcNode = nodes[2];
      assertEqual("CalculationNode", calcNode.type);
      
      var expression = calcNode.expression;
      
      assertEqual("logical and", expression.type);
      assertEqual(2, expression.subNodes.length);
      assertEqual("logical and", expression.subNodes[0].type);
      assertEqual("compare !=", expression.subNodes[0].subNodes[0].type);
      assertEqual("attribute access", expression.subNodes[0].subNodes[0].subNodes[0].type);
      assertEqual("a", expression.subNodes[0].subNodes[0].subNodes[0].name);
      assertEqual("value", expression.subNodes[0].subNodes[0].subNodes[1].type);
      assertNull(expression.subNodes[0].subNodes[0].subNodes[1].value);
      
      assertEqual("compare >", expression.subNodes[0].subNodes[1].type);
      assertEqual("function call", expression.subNodes[0].subNodes[1].subNodes[0].type);
      assertEqual("LENGTH", expression.subNodes[0].subNodes[1].subNodes[0].name);
      assertEqual("value", expression.subNodes[0].subNodes[1].subNodes[1].type);
      assertEqual(0, expression.subNodes[0].subNodes[1].subNodes[1].value);
      
      assertEqual("unary not", expression.subNodes[1].type);
      assertEqual("logical or", expression.subNodes[1].subNodes[0].type);
      assertEqual("logical or", expression.subNodes[1].subNodes[0].subNodes[0].type);
      assertEqual("compare ==", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[0].type);
      assertEqual("attribute access", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[0].subNodes[0].type);
      assertEqual("a", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[0].subNodes[0].name);
      assertEqual("value", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[0].subNodes[1].type);
      assertEqual("abc", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[0].subNodes[1].value);
      assertEqual("compare ==", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[1].type);
      assertEqual("attribute access", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[1].subNodes[0].type);
      assertEqual("a", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[1].subNodes[0].name);
      assertEqual("value", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[1].subNodes[1].type);
      assertEqual("def", expression.subNodes[1].subNodes[0].subNodes[0].subNodes[1].subNodes[1].value);
      assertEqual("compare ==", expression.subNodes[1].subNodes[0].subNodes[1].type);
      assertEqual("attribute access", expression.subNodes[1].subNodes[0].subNodes[1].subNodes[0].type);
      assertEqual("a", expression.subNodes[1].subNodes[0].subNodes[1].subNodes[0].name);
      assertEqual("value", expression.subNodes[1].subNodes[0].subNodes[1].subNodes[1].type);
      assertEqual("xyz", expression.subNodes[1].subNodes[0].subNodes[1].subNodes[1].value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test condition collapsing
////////////////////////////////////////////////////////////////////////////////

    testOrMergeSimple : function () {
      var query = "FOR doc IN " + c.name() + " FILTER doc.b == 1 || (doc.a == 1 || doc.b == 2) RETURN doc";
      var nodes = AQL_EXPLAIN(query).plan.nodes;

      var calcNode = nodes[2];
      assertEqual("CalculationNode", calcNode.type);
      
      var expression = calcNode.expression;
      
      assertEqual("logical or", expression.type);
      assertEqual(2, expression.subNodes.length);
      assertEqual("compare ==", expression.subNodes[0].type);
      assertEqual("attribute access", expression.subNodes[0].subNodes[0].type);
      assertEqual("b", expression.subNodes[0].subNodes[0].name);
      assertEqual("value", expression.subNodes[0].subNodes[1].type);
      assertEqual(1, expression.subNodes[0].subNodes[1].value);
      
      assertEqual("logical or", expression.subNodes[1].type);
      assertEqual(2, expression.subNodes[1].subNodes.length);
      
      assertEqual("compare ==", expression.subNodes[1].subNodes[0].type);
      assertEqual("attribute access", expression.subNodes[1].subNodes[0].subNodes[0].type);
      assertEqual("a", expression.subNodes[1].subNodes[0].subNodes[0].name);
      assertEqual("value", expression.subNodes[1].subNodes[0].subNodes[1].type);
      assertEqual(1, expression.subNodes[1].subNodes[0].subNodes[1].value);
      
      assertEqual("compare ==", expression.subNodes[1].subNodes[1].type);
      assertEqual("attribute access", expression.subNodes[1].subNodes[1].subNodes[0].type);
      assertEqual("b", expression.subNodes[1].subNodes[1].subNodes[0].name);
      assertEqual("value", expression.subNodes[1].subNodes[1].subNodes[1].type);
      assertEqual(2, expression.subNodes[1].subNodes[1].subNodes[1].value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test condition collapsing
////////////////////////////////////////////////////////////////////////////////

    testAndMergeSimple : function () {
      var query = "FOR doc IN " + c.name() + " FILTER doc.b == 1 && (doc.a == 1 && doc.c == 2) RETURN doc";
      var nodes = AQL_EXPLAIN(query).plan.nodes;

      var calcNode = nodes[2];
      assertEqual("CalculationNode", calcNode.type);
      
      var expression = calcNode.expression;
      
      assertEqual("logical and", expression.type);
      assertEqual(2, expression.subNodes.length);
      assertEqual("compare ==", expression.subNodes[0].type);
      assertEqual("attribute access", expression.subNodes[0].subNodes[0].type);
      assertEqual("b", expression.subNodes[0].subNodes[0].name);
      assertEqual("value", expression.subNodes[0].subNodes[1].type);
      assertEqual(1, expression.subNodes[0].subNodes[1].value);
      
      assertEqual("logical and", expression.subNodes[1].type);
      assertEqual(2, expression.subNodes[1].subNodes.length);
      
      assertEqual("compare ==", expression.subNodes[1].subNodes[0].type);
      assertEqual("attribute access", expression.subNodes[1].subNodes[0].subNodes[0].type);
      assertEqual("a", expression.subNodes[1].subNodes[0].subNodes[0].name);
      assertEqual("value", expression.subNodes[1].subNodes[0].subNodes[1].type);
      assertEqual(1, expression.subNodes[1].subNodes[0].subNodes[1].value);
      
      assertEqual("compare ==", expression.subNodes[1].subNodes[1].type);
      assertEqual("attribute access", expression.subNodes[1].subNodes[1].subNodes[0].type);
      assertEqual("c", expression.subNodes[1].subNodes[1].subNodes[0].name);
      assertEqual("value", expression.subNodes[1].subNodes[1].subNodes[1].type);
      assertEqual(2, expression.subNodes[1].subNodes[1].subNodes[1].value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test condition collapsing
////////////////////////////////////////////////////////////////////////////////

    testAndOrMergeSimple : function () {
      var query = "FOR doc IN " + c.name() + " FILTER doc.b == 1 || (doc.a == 1 && doc.b == 2) RETURN doc";
      var nodes = AQL_EXPLAIN(query).plan.nodes;

      var calcNode = nodes[2];
      assertEqual("CalculationNode", calcNode.type);
      
      var expression = calcNode.expression;
      
      assertEqual("logical or", expression.type);
      assertEqual(2, expression.subNodes.length);
      assertEqual("compare ==", expression.subNodes[0].type);
      assertEqual("attribute access", expression.subNodes[0].subNodes[0].type);
      assertEqual("b", expression.subNodes[0].subNodes[0].name);
      assertEqual("value", expression.subNodes[0].subNodes[1].type);
      assertEqual(1, expression.subNodes[0].subNodes[1].value);
      
      assertEqual("logical and", expression.subNodes[1].type);
      assertEqual(2, expression.subNodes[1].subNodes.length);
      
      assertEqual("compare ==", expression.subNodes[1].subNodes[0].type);
      assertEqual("attribute access", expression.subNodes[1].subNodes[0].subNodes[0].type);
      assertEqual("a", expression.subNodes[1].subNodes[0].subNodes[0].name);
      assertEqual("value", expression.subNodes[1].subNodes[0].subNodes[1].type);
      assertEqual(1, expression.subNodes[1].subNodes[0].subNodes[1].value);
      
      assertEqual("compare ==", expression.subNodes[1].subNodes[1].type);
      assertEqual("attribute access", expression.subNodes[1].subNodes[1].subNodes[0].type);
      assertEqual("b", expression.subNodes[1].subNodes[1].subNodes[0].name);
      assertEqual("value", expression.subNodes[1].subNodes[1].subNodes[1].type);
      assertEqual(2, expression.subNodes[1].subNodes[1].subNodes[1].value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test condition collapsing
////////////////////////////////////////////////////////////////////////////////

    testAndMerge : function () {
      var query = "FOR doc IN " + c.name() + " FILTER doc.b == 1 && (doc.c == 'a' || doc.c == 'b') && doc.a IN [1,2] RETURN doc";
      var nodes = AQL_EXPLAIN(query).plan.nodes;

      var calcNode = nodes[2];
      assertEqual("CalculationNode", calcNode.type);
      
      var expression = calcNode.expression;
      assertEqual("logical and", expression.type);
      assertEqual(2, expression.subNodes.length);
     
      var left = expression.subNodes[0];

      assertEqual("logical and", left.type);
      assertEqual(2, left.subNodes.length);
      assertEqual("compare ==", left.subNodes[0].type);
      assertEqual("attribute access", left.subNodes[0].subNodes[0].type);
      assertEqual("b", left.subNodes[0].subNodes[0].name);
      assertEqual("value", left.subNodes[0].subNodes[1].type);
      assertEqual(1, left.subNodes[0].subNodes[1].value);

      assertEqual("compare in", left.subNodes[1].type);
      assertEqual(2, left.subNodes[1].subNodes.length);
      assertEqual("attribute access", left.subNodes[1].subNodes[0].type);
      assertEqual("c", left.subNodes[1].subNodes[0].name);
      assertEqual("array", left.subNodes[1].subNodes[1].type);
      assertEqual("value", left.subNodes[1].subNodes[1].subNodes[0].type);
      assertEqual("a", left.subNodes[1].subNodes[1].subNodes[0].value);
      assertEqual("value", left.subNodes[1].subNodes[1].subNodes[1].type);
      assertEqual("b", left.subNodes[1].subNodes[1].subNodes[1].value);

      var right = expression.subNodes[1];
     
      assertEqual("compare in", right.type);
      assertEqual("attribute access", right.subNodes[0].type);
      assertEqual("a", right.subNodes[0].name);

      assertEqual("array", right.subNodes[1].type);
      assertEqual(2, right.subNodes[1].subNodes.length); // [1,2]
    },

  };
}
jsunity.run(optimizerConditionsTestSuite);

return jsunity.done();

