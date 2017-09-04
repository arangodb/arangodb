/*jshint globalstrict:false, strict:false, maxlen: 850 */
/*global assertEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language
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
var helper = require("@arangodb/aql-helper");

function ahuacatlDocumentsTestSuite () {
  var c = null;
  const cn = "UnitTestsDocument";
  let execQuery = function (numberOfShards) {
    c = internal.db._create("UnitTestsDocument", { numberOfShards });

    let i;
    for (i = 1; i <= 2000; ++i) {
      c.insert({ _key: "test" + i, value: i });
    } 

    var query = "FOR i IN 1..2000 RETURN DOCUMENT(CONCAT('" + cn + "/test', TO_NUMBER(i)))";
    var nodes = helper.removeClusterNodesFromPlan(AQL_EXPLAIN(query).plan.nodes);
    assertEqual("CalculationNode", nodes[nodes.length - 2].type);
    assertEqual("simple", nodes[nodes.length - 2].expressionType);
    assertEqual("function call", nodes[nodes.length - 2].expression.type);
    assertEqual("DOCUMENT", nodes[nodes.length - 2].expression.name);

    var actual = AQL_EXECUTE(query).json.sort(function(l, r) { return l.value - r.value; });
    assertEqual(2000, actual.length);
    for (i = 1; i <= 2000; ++i) {
      assertEqual(i, actual[i - 1].value);
      assertEqual('test' + i, actual[i - 1]._key);
    }
  };

  return {

    setUp : function () {
      internal.db._drop(cn);
    },

    tearDown : function () {
      internal.db._drop(cn);
    },
      
    testOneShard : function () {
      execQuery(1);
    },

    testTwoShards : function () {
      execQuery(2);
    },

    testFourShards : function () {
      execQuery(4);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlDocumentsTestSuite);

return jsunity.done();

