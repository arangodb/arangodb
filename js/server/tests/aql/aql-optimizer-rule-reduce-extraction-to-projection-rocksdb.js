/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

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

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
var db = require("@arangodb").db;
var ruleName = "reduce-extraction-to-projection";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var c = null;
  var cn = "UnitTestsOptimizer";

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);

      for (var i = 0; i < 1000; ++i) {
        c.insert({ value1: i, value2: "test" + i });
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
      c = null;
    },

    testNotActive : function () {
      var queries = [
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc",
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value2",
        "FOR doc IN @@cn SORT doc.value1, doc.value2 RETURN doc",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value2",
        "FOR doc IN @@cn COLLECT v = doc.value1 INTO g RETURN g",
        "FOR doc IN @@cn FILTER doc.value1 == 1 SORT doc.value2 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc",
        "FOR doc IN @@cn FILTER doc.value1 == 1 FILTER doc.value2 == 1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == 1 && doc.value2 == 1 RETURN doc.value2",
        "FOR doc IN @@cn FILTER doc.value1 >= 132 && doc.value <= 134 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc == { value1: 1 } RETURN doc",
        "FOR doc IN @@cn FILTER doc && doc.value1 == 1 RETURN doc.value1",
        "FOR doc IN @@cn INSERT MERGE(doc, { foo: doc.value }) INTO @@cn"
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { "@cn" : cn });
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testActive : function () {
      var queries = [
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN 1",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn SORT doc.value1 RETURN 1",
        "FOR doc IN @@cn COLLECT v = doc.value1 INTO g RETURN v", // g will be optimized away
        "FOR doc IN @@cn FILTER doc.value1 == 1 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 > 1 SORT doc.value1 RETURN doc.value1",
        "FOR doc IN @@cn FILTER doc.value1 == { value1: 1 } RETURN doc.value1",
        "FOR doc IN @@cn SORT doc.value1 RETURN doc.value1"
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

    testResults : function () {
      var queries = [
        [ "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN 42", [ 42 ] ],
        [ "FOR doc IN @@cn FILTER doc.value1 == 1 RETURN doc.value1", [ 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.value1 <= 1 SORT doc.value1 RETURN doc.value1", [ 0, 1 ] ],
        [ "FOR doc IN @@cn FILTER doc.value1 >= 132 && doc.value1 <= 134 SORT doc.value1 RETURN doc.value1", [ 132, 133, 134 ] ]
      ];
      
      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query[0], { "@cn" : cn });
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query[0]);
        
        result = AQL_EXECUTE(query[0], { "@cn" : cn });
        assertEqual(query[1], result.json);
      });
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
