/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the statement class
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

var arangodb = require("@arangodb");
var db = arangodb.db;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: statements
////////////////////////////////////////////////////////////////////////////////

function StatementSuiteNonCluster () {
  'use strict';
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._useDatabase("_system");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        db._dropDatabase("UnitTestsDatabase0");
      }
      catch (err) {
        // ignore this error
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind method, bind variables
////////////////////////////////////////////////////////////////////////////////

    testExplainBindCollection : function () {
      var st = db._createStatement({ query : "FOR i IN @@collection RETURN i" });
      st.bind("@collection", "_users");
      var result = st.explain();

      assertEqual([ ], result.warnings);
      assertTrue(result.hasOwnProperty("plan"));
      assertFalse(result.hasOwnProperty("plans"));

      var plan = result.plan;
      assertTrue(plan.hasOwnProperty("estimatedCost"));
      assertTrue(plan.hasOwnProperty("rules"));
      assertEqual([ ], plan.rules);
      assertTrue(plan.hasOwnProperty("nodes"));
      assertTrue(plan.hasOwnProperty("collections"));
      assertEqual([ { "name" : "_users", "type" : "read" } ], plan.collections);
      assertTrue(plan.hasOwnProperty("variables"));
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(StatementSuiteNonCluster);
return jsunity.done();

