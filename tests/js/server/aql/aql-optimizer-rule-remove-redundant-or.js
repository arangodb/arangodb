/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

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
// TODO add some test which don't use number values!

var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function NewAqlRemoveRedundantORTestSuite () {
  var ruleName = "remove-redundant-or";
  
  var isRuleUsed = function (query, params) {
   var result = AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
   assertTrue(result.plan.rules.indexOf(ruleName) !== -1, query);
   result = AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all" ] } });
   assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
  };
  
  var ruleIsNotUsed = function (query, params) {
   var result = AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+" + ruleName ] } });
   assertTrue(result.plan.rules.indexOf(ruleName) === -1, query);
  };

  var executeWithRule = function (query, params) {
    return AQL_EXECUTE(query, params, { optimizer: { rules: [ "-all", "+" + ruleName ] } }).json;
  };

  var executeWithoutRule = function (query, params) {
    return AQL_EXECUTE(query, params, { optimizer: { rules: [ "-all" ] } }).json;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test the rule fires for actual values
////////////////////////////////////////////////////////////////////////////////
    
    testFiresGtGt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || i > 2 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresGtLt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || 2 < i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLtLt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || 2 < i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLtGt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || i > 2 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
////////////////////////////////////////////////////////////////////////////////

    testFiresGtGe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || i >= 2 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresGtLe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || 2 <= i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLtLe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 < i || 2 <= i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLtGe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 < i || i >= 2 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },

////////////////////////////////////////////////////////////////////////////////

    testFiresGeGt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || i > 2 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresGeLt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || 2 < i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLeLt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || 2 < i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLeGt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || i > 2 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },

////////////////////////////////////////////////////////////////////////////////

    testFiresGeGe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || i >= 2 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresGeLe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || 2 <= i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLeLe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || 2 <= i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLeGe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || i >= 2 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

    testFiresGtGt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || i > 1 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresGtLt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || 1 < i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLtLt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || 1 < i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLtGt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || i > 1 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
////////////////////////////////////////////////////////////////////////////////

    testFiresGtGe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || i >= 1 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresGtLe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || 1 <= i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLtLe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || 1 <= i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLtGe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 < i || i >= 1 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },

////////////////////////////////////////////////////////////////////////////////

    testFiresGeGt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || i > 1 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresGeLt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || 1 < i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLeLt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || 1 < i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLeGt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || i > 1 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },

////////////////////////////////////////////////////////////////////////////////

    testFiresGeGe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || i >= 1 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresGeLe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || 1 <= i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLeLe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || 1 <= i RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresLeGe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || i >= 1 RETURN i";
      
      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
////////////////////////////////////////////////////////////////////////////////

    testDudGtGt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || 2 > i RETURN i";
      
      ruleIsNotUsed(query, {});
    },
    
    testDudGtLt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || i < 2 RETURN i";
      
      ruleIsNotUsed(query, {});
    },
    
    testDudLtLt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || i < 2 RETURN i";
      
      ruleIsNotUsed(query, {});
    },
    
    testDudLtGt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || 2 > i RETURN i";
      
      ruleIsNotUsed(query, {});
    },
    
////////////////////////////////////////////////////////////////////////////////

    testDudGtGe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || 2 >= i RETURN i";
      
      ruleIsNotUsed(query, {});
    },
    
    testDudGtLe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || i <= 2 RETURN i";
      
      ruleIsNotUsed(query, {});
    },
    
    testDudLtLe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 < i || i <= 2 RETURN i";
      
      ruleIsNotUsed(query, {});
    },
    
    testDudLtGe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 < i || 2 >= i RETURN i";
      
      ruleIsNotUsed(query, {});
    },

////////////////////////////////////////////////////////////////////////////////

    testDudGeGt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || 2 > i RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudGeLt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || i < 2 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLeLt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || i < 2 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLeGt1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || 2 > i RETURN i";
      
      ruleIsNotUsed(query, {});
    },

////////////////////////////////////////////////////////////////////////////////

    testDudGeGe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || 2 >= i RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudGeLe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || i <= 2 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLeLe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || i <= 2 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLeGe1 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || 2 >= i RETURN i";
      
      ruleIsNotUsed(query, {});
    },

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

    testDudGtGt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || i < 1 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudGtLt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER i > 1 || 1 > i RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLtLt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || 1 > i RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLtGt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || i < 1 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
////////////////////////////////////////////////////////////////////////////////

    testDudGtGe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || 1 >= i RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudGtLe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i > 1 || i <= 1 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLtLe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] FILTER 1 < i || i <= 1 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLtGe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 < i || 1 >= i RETURN i";
      ruleIsNotUsed(query, {});
    },

////////////////////////////////////////////////////////////////////////////////

    testDudGeGt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || 1 > i RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudGeLt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || i < 1 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLeLt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || i < 1 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLeGt2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || 1 > i RETURN i";
      ruleIsNotUsed(query, {});
    },

////////////////////////////////////////////////////////////////////////////////

    testDudGeGe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || 1 >= i RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudGeLe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER i >= 1 || i <= 1 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLeLe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || i <= 1 RETURN i";
      ruleIsNotUsed(query, {});
    },
    
    testDudLeGe2 : function () {
      var query = "FOR i IN  [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ] "
        + " FILTER 1 <= i || 1 >= i RETURN i";
      ruleIsNotUsed(query, {});
    },
  };
}

jsunity.run(NewAqlRemoveRedundantORTestSuite);

return jsunity.done();

