/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'query.parallelize-gather-writes': 'false'
  };
}
let jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
let db = require('internal').db;
let ArangoStatement = require("@arangodb/arango-statement").ArangoStatement;

function testSuite() {
  return {
    setUp: function() {
      db._drop(cn);
      db._create(cn, { numberOfShards: 3 });
    },
    
    tearDown: function() {
      db._drop(cn);
    },
    
    testRule : function() {
      let stmt = new ArangoStatement(db, { query: "FOR doc IN " + cn + " UPDATE doc WITH { updated: true } IN " + cn });
      let plan = stmt.explain().plan;
      let nodes = plan.nodes;
      let gather = plan.nodes.filter(function(node) { return node.type === 'GatherNode'; });
      assertEqual(1, gather.length);
      assertEqual("serial", gather[0].parallelism);

      assertEqual(-1, plan.rules.indexOf("parallelize-gather"));
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
