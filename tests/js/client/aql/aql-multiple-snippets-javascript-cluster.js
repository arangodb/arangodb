/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual, assertMatch, arango */

////////////////////////////////////////////////////////////////////////////////
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
const arangodb = require("@arangodb");
const db = arangodb.db;
const errors = require("internal").errors;

function MultipleSnippetsInCoordinatorSuite () {
  'use strict';
  const cn = "UnitTestsSnippets";
  
  return {
    setUpAll: function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 3 });
    },

    tearDownAll: function () {
      db._drop(cn);
    },

    // tests an issue reported via OASIS-24962:
    // when a query is started from within some JavaScript environment
    // (e.g. console mode or Foxx) and has multiple coordinator snippets,
    // only the outermost coordinator snippet can make use of the
    // JavaScript V8 context. The other generated coordinator snippets
    // will believe that they also have access to the JavaScript V8
    // context of the other snippet, but in fact they don't. The problem
    // exists because the Query object is shared among coordinator
    // snippets. Once this is fixed properly, this test should stop
    // failing.
    testMultipleCoordinatorSnippetsJavaScriptProblem: function () {
      const query = `
LET x = NOOPT(V8(['a', 'b', 'c']))
LET q1 = (LET values = V8(x) FOR doc IN @@cn RETURN doc._key IN values)
LET q2 = (LET values = V8(x) FOR doc IN @@cn RETURN doc._key IN values)
RETURN MERGE({}, q1, q2)
`;
      
      // verify that we actually have multiple coordinator snippets
      let nodes = db._createStatement({query: query, bindVars: { "@cn": cn }, options: {}}).explain().plan.nodes.map((n) => n.type);
      const expected = [ "SingletonNode", "CalculationNode", "CalculationNode", "SubqueryStartNode", "ScatterNode", "RemoteNode", "IndexNode", "CalculationNode", "RemoteNode", "GatherNode", "SubqueryEndNode", "SubqueryStartNode", "ScatterNode", "RemoteNode", "IndexNode", "CalculationNode", "RemoteNode", "GatherNode", "SubqueryEndNode", "CalculationNode", "ReturnNode" ];
      assertEqual(expected, nodes);

      let remotes = 0;
      nodes.forEach((n) => { if (n === 'RemoteNode') { remotes++; } });
      assertEqual(4, remotes);

      // query execution must fail because we have multiple coordinator
      // snippets that will use JavaScript, but only one of them actually has
      // a JavaScript context attached
      let command = `
        let query = ${JSON.stringify(query)};
        let vars = { "@cn": ${JSON.stringify(cn)}};
        return AQL_EXECUTE(query, vars);
      `;
      let actual = arango.POST("/_admin/execute", command);
      assertMatch(/no v8 executor available to enter for current transaction context/, actual);
    },
    
    testMultipleCoordinatorSnippetsNoJavaScriptNeededExceptForOutermostSnippet: function () {
      const query = `
LET x = NOOPT(['a', 'b', 'c'])
LET q1 = (LET values = NOOPT(x) FOR doc IN @@cn RETURN doc._key IN values)
LET q2 = (LET values = NOOPT(x) FOR doc IN @@cn RETURN doc._key IN values)
RETURN V8(APPEND(q1, q2))
`;
      
      // verify that we actually have multiple coordinator snippets
      let nodes = db._createStatement({query: query, bindVars: { "@cn": cn }}).explain().plan.nodes.map((n) => n.type);
      const expected = [ "SingletonNode", "CalculationNode", "SubqueryStartNode", "CalculationNode", "ScatterNode", "RemoteNode", "IndexNode", "CalculationNode", "RemoteNode", "GatherNode", "SubqueryEndNode", "SubqueryStartNode", "CalculationNode", "ScatterNode", "RemoteNode", "IndexNode", "CalculationNode", "RemoteNode", "GatherNode", "SubqueryEndNode", "CalculationNode", "ReturnNode" ];

      let remotes = 0;
      nodes.forEach((n) => { if (n === 'RemoteNode') { remotes++; } });
      assertEqual(4, remotes);

      // query execution for this query should work because only the outermost
      // snippet requires JavaScript
      let result = db._query(query, { "@cn": cn }).toArray();
      // results don't matter. all that counts is that we don't run into an error
      // during query execution
      assertEqual([[]], result);
    },
    
    testMultipleCoordinatorSnippetsNoJavaScriptNeeded: function () {
      const query = `
LET x = NOOPT(['a', 'b', 'c'])
LET q1 = (LET values = NOOPT(x) FOR doc IN @@cn RETURN doc._key IN values)
LET q2 = (LET values = NOOPT(x) FOR doc IN @@cn RETURN doc._key IN values)
RETURN APPEND(q1, q2)
`;
      
      // verify that we actually have multiple coordinator snippets
      let nodes = db._createStatement({query: query, bindVars: { "@cn": cn }}).explain().plan.nodes.map((n) => n.type);
      const expected = [ "SingletonNode", "CalculationNode", "SubqueryStartNode", "CalculationNode", "ScatterNode", "RemoteNode", "IndexNode", "CalculationNode", "RemoteNode", "GatherNode", "SubqueryEndNode", "SubqueryStartNode", "CalculationNode", "ScatterNode", "RemoteNode", "IndexNode", "CalculationNode", "RemoteNode", "GatherNode", "SubqueryEndNode", "CalculationNode", "ReturnNode" ];

      let remotes = 0;
      nodes.forEach((n) => { if (n === 'RemoteNode') { remotes++; } });
      assertEqual(4, remotes);

      // query execution for this query should work because only the outermost
      // snippet requires JavaScript
      let result = db._query(query, { "@cn": cn }).toArray();
      // results don't matter. all that counts is that we don't run into an error
      // during query execution
      assertEqual([[]], result);
    },
  
  };
}
 
jsunity.run(MultipleSnippetsInCoordinatorSuite);

return jsunity.done();
