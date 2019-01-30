/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertNotEqual, assertUndefined, AQL_EXPLAIN, AQL_EXECUTE */

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
var db = require("@arangodb").db;
var removeAlwaysOnClusterRules = helper.removeAlwaysOnClusterRules;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerRuleTestSuite () {
  var ruleName = "patch-update-statements";
  var paramNone     = { optimizer: { rules: [ "-all" ] } };
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var c;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 2000; ++i) {
        c.save({ _key: "test" + i, value: i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect when explicitly disabled
////////////////////////////////////////////////////////////////////////////////

    testRuleDisabled : function () {
      var queries = [ 
        "FOR doc IN " + c.name() + " REMOVE doc IN " + c.name(),
        "FOR doc IN " + c.name() + " REMOVE doc IN " + c.name() + " RETURN doc",
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramNone);
        assertEqual([ ], removeAlwaysOnClusterRules(result.plan.rules));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has no effect
////////////////////////////////////////////////////////////////////////////////

    testRuleNoEffect : function () {
      var queries = [ 
        "REMOVE 'test0' IN " + c.name(), // nothing returned
        "REMOVE { test: 1 } IN " + c.name(), // nothing returned
        "FOR doc1 IN " + c.name() + " REMOVE doc1 IN " + c.name() + " RETURN doc1.value", // must not kick in here
        "FOR doc1 IN " + c.name() + " REMOVE doc1 IN " + c.name() + " FILTER doc1.value == 2 RETURN doc1", // must not kick in here
        "FOR i IN 1..10 FOR doc1 IN " + c.name() + " REMOVE doc1 IN " + c.name() + " RETURN doc1", // must not kick in here
        "FOR doc1 IN " + c.name() + " FOR i IN 1..10 REMOVE doc1 IN " + c.name() + " RETURN doc1", // must not kick in here
        "FOR doc1 IN " + c.name() + " FOR doc2 IN " + c.name() + " REMOVE doc1 IN " + c.name() + " RETURN doc1", // must not kick in here
        "FOR doc1 IN " + c.name() + " FOR doc2 IN " + c.name() + " REMOVE doc1 IN " + c.name() + " RETURN doc2", // must not kick in here
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that rule has an effect
////////////////////////////////////////////////////////////////////////////////

    testRuleHasEffect : function () {
      c.ensureIndex({ type: "skiplist", fields: ["x"] });

      var queries = [ 
        "FOR doc IN " + c.name() + " REMOVE doc IN " + c.name(), // nothing returned
        "FOR doc IN " + c.name() + " REMOVE doc IN " + c.name() + " RETURN 1", // different values returned
        "FOR doc IN " + c.name() + " FILTER doc.value > 100 REMOVE doc IN " + c.name(), // nothing returned
        "FOR doc IN " + c.name() + " FILTER doc.x > 100 REMOVE doc IN " + c.name(), // using index
      ];

      queries.forEach(function(query) {
        var result = AQL_EXPLAIN(query, { }, paramEnabled);
        assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResultsAfterRemove : function () {
      var query = "FOR doc IN " + c.name() + " REMOVE doc IN " + c.name() + " RETURN doc";
      var result = AQL_EXPLAIN(query, { }); 
      assertEqual(-1, result.plan.rules.indexOf(ruleName), query);

      result = AQL_EXECUTE(query).json;
      assertEqual(2000, result.length);

      for (var i = 0; i < result.length; ++i) {
        assertTrue(result[i].value >= 0);
      }

      assertEqual(0, c.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testResultsAfterRemoveReturnOld : function () {
      var query = "FOR doc IN " + c.name() + " REMOVE doc IN " + c.name() + " RETURN OLD";
      var result = AQL_EXPLAIN(query, {}, { optimizer: { rules: ["-remove-data-modification-out-variables"] } });
      assertNotEqual(-1, result.plan.rules.indexOf(ruleName), query);

      result = AQL_EXECUTE(query).json;
      assertEqual(2000, result.length);

      for (var i = 0; i < result.length; ++i) {
        assertTrue(result[i].value >= 0);
      }
     
      assertEqual(0, c.count()); 
    },
    
  };
}

jsunity.run(optimizerRuleTestSuite);

return jsunity.done();
