/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

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

function optimizerRuleTestSuite () {
  const ruleName = "fuse-filters";

  return {

    testResults : function () {
      let queries = [ 
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < 4 FILTER i < 10 SORT i RETURN i",  [ 2, 3 ] ],
        [ "LET a = NOOPT(9) FOR i IN 1..10 FILTER i > 1 FILTER i < 4 FILTER i < a SORT i RETURN i",  [ 2, 3 ] ],
        [ "LET a = NOOPT(9), b = NOOPT(0) FOR i IN 1..10 FILTER i >= b FILTER i > 1 FILTER i < 4 FILTER i < a SORT i RETURN i",  [ 2, 3 ] ],
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query[0]);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(1, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result, query);
      });
    },
    
    testNondeterministicNoFuse : function () {
      let queries = [ 
        [ "FOR i IN 1..10 FILTER i > 1 LIMIT 10 FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i >= RAND() FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < NOOPT(4) SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > NOOPT(1) FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > NOOPT(1) FILTER i < NOOPT(4) SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < (RETURN 4)[0] SORT i RETURN i",  [ 2, 3 ] ],
      ];

      queries.forEach(function(query) {
        let result = AQL_EXPLAIN(query[0]);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(1, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        result = AQL_EXECUTE(query[0]).json;
        assertEqual(query[1], result, query);
      });
    }

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
