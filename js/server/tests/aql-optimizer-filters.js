/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for filters
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerFiltersTestSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief filters that should be optimized away
////////////////////////////////////////////////////////////////////////////////

    testOptimizeAwayFalseFilter : function () {
      var queries = [ 
        "FOR i IN 1..10 FILTER i == 1 && false RETURN i",
        "FOR i IN 1..10 FILTER (i == 1 || i == 2) && false RETURN i",
        "FOR i IN 1..10 FILTER false && i == 1 RETURN i",
        "FOR i IN 1..10 FILTER false && (i == 1 || i == 2) RETURN i",
        "FOR i IN 1..10 FILTER IS_STRING(i) && false RETURN i",
        "FOR i IN 1..10 FILTER false && IS_STRING(i) RETURN i",
        "FOR i IN 1..10 FILTER false && RAND() RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertNotEqual(-1, nodeTypes.indexOf("NoResultsNode"), query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query).json;
        assertEqual([ ], results, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief filters that should be optimized away
////////////////////////////////////////////////////////////////////////////////

    testOptimizeAwayTrueFilter : function () {
      var queries = [ 
        "FOR i IN 1..10 FILTER i == 1 || true RETURN i",
        "FOR i IN 1..10 FILTER i == 1 || i == 2 || true RETURN i",
        "FOR i IN 1..10 FILTER true || i == 1 RETURN i",
        "FOR i IN 1..10 FILTER true || i == 1 || i == 2 RETURN i",
        "FOR i IN 1..10 FILTER IS_STRING(i) || true RETURN i",
        "FOR i IN 1..10 FILTER true || IS_STRING(i) RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("FilterNode"), query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query).json;
        assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], results, query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief filters that should not be optimized away
////////////////////////////////////////////////////////////////////////////////

    testKeepFilter : function () {
      var queries = [ 
        "FOR i IN 1..10 FILTER false || i == 1 RETURN i",
        "FOR i IN 1..10 FILTER i == 1 || false RETURN i",
        "FOR i IN 1..10 FILTER IS_STRING(i) || false RETURN i",
        "FOR i IN 1..10 FILTER false || IS_STRING(i) RETURN i",
        "FOR i IN 1..10 FILTER i == 1 || i == 2 RETURN i",
        "FOR i IN 1..10 FILTER i == 1 || (i == 2 && true) RETURN i",
        "FOR i IN 1..10 FILTER true && i == 1 RETURN i",
        "FOR i IN 1..10 FILTER true && (i == 1 || i == 2) RETURN i",
        "FOR i IN 1..10 FILTER IS_STRING(i) && true RETURN i",
        "FOR i IN 1..10 FILTER true && IS_STRING(i) RETURN i"
      ];

      queries.forEach(function(query) {
        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertNotEqual(-1, nodeTypes.indexOf("FilterNode"), query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerFiltersTestSuite);

return jsunity.done();

