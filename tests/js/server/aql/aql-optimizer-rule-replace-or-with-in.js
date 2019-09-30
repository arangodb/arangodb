/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Ahuacatl, replace-or-with-in rule
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
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function NewAqlReplaceORWithINTestSuite () {
  var replace;
  var ruleName = "replace-or-with-in";
  
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
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      internal.debugClearFailAt();
      internal.db._drop("UnitTestsNewAqlReplaceORWithINTestSuite");
      replace = internal.db._create("UnitTestsNewAqlReplaceORWithINTestSuite");

      let docs = [];
      for (var i = 1; i <= 10; ++i) {
        docs.push({ "value" : i, x: [i]});
        docs.push({"a" : {"b" : i}});
        docs.push({"value": i + 10, "bb": i, "cc": 10 - i });
      }
      replace.insert(docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.debugClearFailAt();
      internal.db._drop("UnitTestsNewAqlReplaceORWithINTestSuite");
      replace = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test OOM
////////////////////////////////////////////////////////////////////////////////

    testOom : function () {
      if (!internal.debugCanUseFailAt()) {
        return;
      }
      internal.debugSetFailAt("OptimizerRules::replaceOrWithInRuleOom");
      try {
        AQL_EXECUTE("FOR i IN 1..10 FILTER i == 1 || i == 2 RETURN i");
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_DEBUG.code, err.errorNum);
      }
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
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFires2Values2 : function () {
      var query = "FOR x IN " + replace.name() + 
        " FILTER x.a.b == 1 || x.a.b == 2 SORT x.a.b RETURN x.a.b";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFires4Values1 : function () {
      var query = "FOR x IN " + replace.name() + " FILTER x.value == 1 " +
        "|| x.value == 2 || x.value == 3 || x.value == 4 SORT x.value RETURN x.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2, 3, 4];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresNoAttributeAccess : function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x == 1 || x == 2 RETURN x";

      isRuleUsed(query, {});

      var expected = [ ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },

    testFiresNoCollection : function () {
      var query = 
        "FOR x in 1..10 FILTER x == 1 || x == 2 SORT x RETURN x";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
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
      assertEqual(executeWithRule(query, params), executeWithoutRule(query, params));
    },
  
    testFiresVariables : function () {
      var query = 
        "LET x = 1 LET y = 2 FOR v IN " + replace.name() 
        + " FILTER v.value == x || v.value == y SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFires2AttributeAccesses1 : function () {
      var query = 
        "LET x = {a:1,b:2} FOR v IN " + replace.name() 
        + " FILTER v.value == x.a || v.value == x.b SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFires2AttributeAccesses2 : function () {
      var query = 
        "LET x = {a:1,b:2} FOR v IN " + replace.name() 
        + " FILTER x.a == v.value || v.value == x.b SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFires2AttributeAccesses3 : function () {
      var query = 
        "LET x = {a:1,b:2} FOR v IN " + replace.name() 
        + " FILTER x.a == v.value || x.b == v.value SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },

    
    testFiresMixed1 : function () {
      var query = 
        "LET x = [1,2] FOR v IN " + replace.name() 
        + " FILTER x[0] == v.value || x[1] == v.value SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresMixed2 : function () {
      var query = 
        "LET x = [1,2] LET y = {b:2} FOR v IN " + replace.name() 
        + " FILTER x[0] == v.value || y.b == v.value SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 2 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresSelfReference1 : function () {
      var query = 
        "FOR v IN " + replace.name() 
        + " FILTER v.value == v.a.b  || v.value == 10 || v.value == 7 SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 7, 10 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresSelfReference2 : function () {
      var query = 
        "FOR v IN " + replace.name() 
        + " FILTER v.a.b == v.value || v.value == 10 || 7 == v.value SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 7, 10 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },

    testFiresAttributeIsList1 : function () {
      var query = 
       "LET x = {a:1,b:2} FOR v IN " + replace.name() 
       + " FILTER v.x[0] == x.a || v.x[0] == 3 SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 3 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresAttributeIsList2 : function () {
      var query = 
       "LET x = {a:1,b:2} FOR v IN " + replace.name() 
       + " FILTER x.a == v.x[0] || v.x[0] == 3 SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 3 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },

    testFiresAttributeIsList3 : function () {
      var query = 
       "LET x = {a:1,b:2} FOR v IN " + replace.name() 
       + " FILTER x.a == v.x[0] || 3 == v.x[0] SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 3 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFiresAttributeIsList4 : function () {
      var query = 
       "LET x = {a:1,b:2} FOR v IN " + replace.name() 
       + " FILTER v.x[0] == x.a || 3 == v.x[0] SORT v.value RETURN v.value";

      isRuleUsed(query, {});

      var expected = [ 1, 3 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
    },
    
    testFires2Loops: function () {
      var query = 
       "FOR x IN " + replace.name() 
       + " FOR y IN "  + replace.name() 
       + " FILTER x.value == y.bb || x.value == y.cc" 
       + " FILTER x.value != null SORT x.value RETURN x.value";
     
      ruleIsNotUsed(query, {});

      var expected = [ 1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10  ]; 
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
      assertEqual(expected, executeWithoutRule(query, {}));
    },
    
    testFiresNonsense1: function () {
      var query =
        "FOR v in " + replace.name() 
        + " FILTER 1 == 2 || v.value == 2 || v.value == 3 SORT v.value RETURN v.value" ;
     
      isRuleUsed(query, {});

      var expected = [ 2, 3 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
      assertEqual(expected, executeWithoutRule(query, {}));
    },
    
    testFiresNonsense2: function () {
      var query = "FOR v in " + replace.name() + 
        " FILTER 1 == 2 || 2 == v.value || v.value == 3 SORT v.value RETURN v.value";
     
      isRuleUsed(query, {});

      var expected = [ 2, 3 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
      assertEqual(expected, executeWithoutRule(query, {}));
    },
    
    testFiresNonsense3: function () {
      var query = 
      "FOR v in " + replace.name() 
      + " FILTER v.value == 2 || 3 == v.value || 1 == 2 SORT v.value RETURN v.value";
     
      isRuleUsed(query, {});

      var expected = [ 2, 3 ];
      var actual = getQueryResults(query, {}); 
      assertEqual(expected, actual);
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));
      assertEqual(expected, executeWithoutRule(query, {}));
    },
   
    testFiresRand: function () {
      var query = "LET a = RAND(), b = RAND() FOR i IN [1,2,3,4,5] "  
        + "FILTER i.a.b == a || i.a.b == b RETURN i";

      isRuleUsed(query, {});
    },

    testFiresLongAttributes : function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.a.b.c.d == 1 || x.a.b.c.d == 2 || 3 == x.a.b.c.d RETURN x";

      isRuleUsed(query, {});
    },

    testDudCommonConstant1: function () {
      var query = "LET x = {a:@a} FOR v IN " + replace.name() 
        + " FILTER x.a == v.value || x.a == v._key RETURN v._key";

      var key = replace.any()._key;
      ruleIsNotUsed(query, {a: key});
    },
    
    testDudCommonConstant2: function () {
      var query = "LET x = {a:1} FOR v IN " + replace.name() 
        + " FILTER x.a == v.value || x.a == v._key RETURN v._key";

      ruleIsNotUsed(query, {});
    },
    
    testDudCommonConstant3: function () {
      var query = "LET x = NOOPT({a:1}) FOR v IN " + replace.name() 
        + " FILTER x.a == v.value || x.a == v._key RETURN v._key";

      ruleIsNotUsed(query, {});
    },

    testDudAlwaysTrue: function () {
      var query = 
      "FOR x IN " + replace.name() 
      + " FILTER x.value == x.value || x.value == 2 || x.value == 3 SORT x.value RETURN x.value";
     
      isRuleUsed(query, {});
      assertEqual(executeWithRule(query, {}), executeWithoutRule(query, {}));

    },

    testDudDifferentAttributes1 : function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 || x.val2 == 2 RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudDifferentAttributes2: function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 || x == 2 RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudDifferentAttributes3: function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 || 2 == x.val2 RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudDifferentAttributes4: function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 || 2 == x.val2 || 3 == x.val1 RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudDifferentAttributes5: function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 || 2 == x.val1 || 3 == x.val1 || 4 == x.val2 RETURN x";

      isRuleUsed(query, {});
    },

    testDudDifferentAttributesWithBool1: function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 || x == true RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudDifferentAttributesWithBool2: function () {
      var query = 
        "FOR x IN " + replace.name() + " FILTER x.val1 == 1 || x.val2 == true RETURN x";

      ruleIsNotUsed(query, {});
    },

    testDudDifferentVariables : function () {
      var query = 
        "FOR y IN " + replace.name() + " FOR x IN " + replace.name() 
        + " FILTER x.val1 == 1 || y.val1 == 2 RETURN x";

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

    testDudAttributeIsList: function () {
      var query = 
        "LET x = {a:1, b:2} FOR v IN " + replace.name() 
        + " FILTER v.x[0] == x.a || v.x[1] == 3 SORT v.value RETURN v.value";
      ruleIsNotUsed(query, {});
    },

    testDudNested: function () {
      var query = 
        "FOR i IN [ { att1: 'foo', att2: true }, { att1: 'bar', att2: false } ] FILTER i.att1 == 'bar' || i.att2 == true RETURN i";
      ruleIsNotUsed(query, {});
    }
  };
}

jsunity.run(NewAqlReplaceORWithINTestSuite);

return jsunity.done();

