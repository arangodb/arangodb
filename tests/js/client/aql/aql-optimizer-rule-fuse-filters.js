/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
const db = require('internal').db;

function optimizerRuleTestSuite () {
  const ruleName = "fuse-filters";

  return {

    testResultsNoMove : function () {
      let queries = [ 
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < 4 FILTER i < 10 SORT i RETURN i",  [ 2, 3 ] ],
        [ "LET a = NOOPT(9) FOR i IN 1..10 FILTER i > 1 FILTER i < 4 FILTER i < a SORT i RETURN i",  [ 2, 3 ] ],
        [ "LET a = NOOPT(9), b = NOOPT(0) FOR i IN 1..10 FILTER i >= b FILTER i > 1 FILTER i < 4 FILTER i < a SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < (RETURN 4)[0] SORT i RETURN i",  [ 2, 3 ] ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], options: {optimizer: {rules: ["-move-filters-into-enumerate"] } } }).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(1, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);

        result = db._query(query[0]).toArray();
        assertEqual(query[1], result, query);
      });
    },
    
    testResultsWithMove : function () {
      let queries = [ 
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < 4 FILTER i < 10 SORT i RETURN i",  [ 2, 3 ] ],
        [ "LET a = NOOPT(9) FOR i IN 1..10 FILTER i > 1 FILTER i < 4 FILTER i < a SORT i RETURN i",  [ 2, 3 ] ],
        [ "LET a = NOOPT(9), b = NOOPT(0) FOR i IN 1..10 FILTER i >= b FILTER i > 1 FILTER i < 4 FILTER i < a SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < (RETURN 4)[0] SORT i RETURN i",  [ 2, 3 ] ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query[0]).explain();
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertEqual(0, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);
        let listNodes = result.plan.nodes.filter(function(n) { return n.type === 'EnumerateListNode'; });
        assertEqual(1, listNodes.length);
        assertTrue(listNodes[0].hasOwnProperty('filter'));

        result = db._query(query[0]).toArray();
        assertEqual(query[1], result, query);
      });
    },
    
    testNondeterministicNoFuseNoMove : function () {
      let queries = [ 
        [ "FOR i IN 1..10 FILTER i > 1 LIMIT 10 FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i >= RAND() FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < NOOPT(4) SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > NOOPT(1) FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > NOOPT(1) FILTER i < NOOPT(4) SORT i RETURN i",  [ 2, 3 ] ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement({query: query[0], options: {optimizer: {rules: ["-move-filters-into-enumerate"] } } }).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        assertNotEqual(1, result.plan.nodes.filter(function(n) { return n.type === 'FilterNode'; }).length);

        result = db._query(query[0]).toArray();
        assertEqual(query[1], result, query);
      });
    },

    testNondeterministicNoFuseWithMove : function () {
      let queries = [ 
        [ "FOR i IN 1..10 FILTER i > 1 LIMIT 10 FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i >= RAND() FILTER i < 4 SORT i RETURN i",  [ 2, 3 ] ],
        [ "FOR i IN 1..10 FILTER i > 1 FILTER i < NOOPT(4) SORT i RETURN i",  [ 2, 3 ] ],
      ];

      queries.forEach(function(query) {
        let result = db._createStatement(query[0]).explain();
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
        let listNodes = result.plan.nodes.filter(function(n) { return n.type === 'EnumerateListNode'; });
        assertEqual(1, listNodes.length);
        assertTrue(listNodes[0].hasOwnProperty('filter'));
        
        result = db._query(query[0]).toArray();
        assertEqual(query[1], result, query);
      });
    }

  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
