/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual */

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
    'query.optimizer-rules': "-use-indexes"
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
      let c = db._create(cn);
      c.ensureIndex({ type: "skiplist", fields: ["value"] });
    },
    
    tearDown: function() {
      db._drop(cn);
    },
    
    testDisabledRule : function() {
      let stmt = new ArangoStatement(db, { query: "FOR doc IN " + cn + " FILTER doc.value > 9 RETURN doc" });
      let plan = stmt.explain().plan;
      assertEqual(-1, plan.rules.indexOf("use-indexes"));
    },

    testEnabledRule : function() {
      let stmt = new ArangoStatement(db, { query: "FOR doc IN " + cn + " FILTER doc.value > 9 RETURN doc", options: { optimizer: { rules: ["+use-indexes"] } } });
      let plan = stmt.explain().plan;
      assertNotEqual(-1, plan.rules.indexOf("use-indexes"));
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
