/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, assertTrue, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Ahuacatl, skiplist index queries
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
/// @author 
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var jsunity = require("jsunity");
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function NewAqlReplaceORWithINTestSuite () {
  var replace;
  var ruleName = "replace-OR-with-IN";
  
  var isRuleUsed = function (query, params) {
   var result = AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
   assertTrue(result.plan.rules.indexOf(ruleName) !== -1, query);
   var result = AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all" ] } });
   assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
  };
  
  var ruleIsNotUsed = function (query, params) {
   var result = AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
   assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop("UnitTestsNewAqlReplaceORWithINTestSuite");
      replace = internal.db._create("UnitTestsNewAqlReplaceORWithINTestSuite");

      for (var i = 1; i <= 10; ++i) {
        replace.save({ "value" : i});
        replace.save({"a" : {"b" : i}});
      }

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop("UnitTestsNewAqlReplaceORWithINTestSuite");
      replace = null;
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief test the rule fires for actual values
////////////////////////////////////////////////////////////////////////////////

    testFires2Values1 : function () {
      var query = "FOR x IN " + replace.name() + 
        " FILTER x.value == 1 || x.value == 2 SORT x.value RETURN x.value";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
    },
    
    testFires2Values2 : function () {
      var query = "FOR x IN " + replace.name() + 
        " FILTER x.a.b == 1 || x.a.b == 2 SORT x.a.b RETURN x.a.b";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
    },
    
    testFires4Values1 : function () {
      var query = "FOR x IN " + replace.name() + " FILTER x.value == 1 " +
        "|| x.value == 2 || x.value == 3 || x.value == 4 SORT x.value RETURN x.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
    },
    
    testFiresNoAttributeAccess : function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x == 1 || x == 2 RETURN x";

      isRuleUsed(query, {});

      var expected = [ ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
    },

    testFiresNoCollection : function () {
      var query = 
        "FOR x in 1..10 FILTER x == 1 || x == 2 SORT x RETURN x";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);

    },
    
    testFiresBind : function () {
      var query = 
        "FOR v IN " + replace.name() 
        + " FILTER v.value == @a || v.value == @b SORT v.value RETURN v.value";
      var params = {"a": 1, "b": 2};

      isRuleUsed(query, params);

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, params); 
      assertEqual(expected, actual);

    },
  
    testFiresVariables : function () {
      var query = 
        "LET x = 1 LET y = 2 FOR v IN " + replace.name() 
        + " FILTER v.value == x || v.value == y SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
    },
    
    testFires2AttributeAccesses1 : function () {
      var query = 
        "LET x = {a:1,b:2} FOR v IN " + replace.name() 
        + " FILTER v.value == x.a || v.value == x.b SORT v.value RETURN v.value"

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
    },
    
    testFires2AttributeAccesses2 : function () {
      var query = 
        "LET x = {a:1,b:2} FOR v IN " + replace.name() 
        + " FILTER x.a == v.value || v.value == x.b SORT v.value RETURN v.value"

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
    },
    
    testFires2AttributeAccesses3 : function () {
      var query = 
        "LET x = {a:1,b:2} FOR v IN " + replace.name() 
        + " FILTER x.a == v.value || x.b == v.value SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
    },

    
    testFiresMixed1 : function () {
      var query = 
        "LET x = [1,2] FOR v IN " + replace.name() 
        + " FILTER x[0] == v.value || x[1] == v.value SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
    },
    
    testFiresMixed2 : function () {
      var query = 
        "LET x = [1,2] LET y = {b:2} FOR v IN " + replace.name() 
        + " FILTER x[0] == v.value || y.b == v.value SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
    },
    
    testDudDifferentAttributes1 : function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 || x.val2 == 2 RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudDifferentVariables : function () {
      var query = 
        "FOR y IN " + replace.name() + " FOR x IN " + replace.name() 
        + " FILTER x.val1 == 1 || y.val1 == 2 RETURN x";

      ruleIsNotUsed(query, {});
    },
    
    testDudDifferentAttributes2: function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 || x == 2 RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudNoOR1: function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 RETURN x";

      ruleIsNotUsed(query, {});
    },
    
    testDudNoOR2: function () {
      var query = 
        "FOR x IN " + replace.name() 
        + " FILTER x.val1 == 1 && x.val2 == 2 RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudNonEquality1: function () {
      var query = 
        "FOR x IN " + replace.name() 
        + " FILTER x.val1 > 1 || x.val1 == 2 RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudNonEquality2: function () {
      var query = 
        "FOR x IN " + replace.name() 
        + " FILTER x.val1 == 1 ||  2 < x.val1 RETURN x";

      ruleIsNotUsed(query, {});
    },
  };
}

jsunity.run(NewAqlReplaceORWithINTestSuite);

return jsunity.done();

