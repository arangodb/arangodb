/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE, AQL_EXECUTEJSON */

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
var isEqual = helper.isEqual;
var db = require("@arangodb").db;
var _ = require("lodash");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function singleAttributeTestSuite () {
  var ruleName = "use-indexes";
  var cn = "UnitTestsAhuacatlRange";
  var c;

  // various choices to control the optimizer: 
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      db._drop(cn);
      c = db._create(cn);

      var i;

      for (i = 0; i < 100; ++i) {
        c.save({ value1: "test" + i, value2: i });
      }

      c.ensureSkiplist("value1");
      c.ensureSkiplist("value2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testRangesSingleAttribute : function () {
      var queries = [ 
        [ "FOR i IN [ 2 ] FOR j IN " + cn + " FILTER j.value2 == i SORT j.value2 RETURN j.value2", [ 2 ], true ],
        [ "FOR i IN [ 2 ] FOR j IN " + cn + " FILTER i == j.value2 SORT j.value2 RETURN j.value2", [ 2 ], true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER i == j.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ], true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 == i SORT j.value2 RETURN j.value2", [ 2, 3 ], true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 >= i FILTER j.value2 <= i SORT j.value2 RETURN j.value2", [ 2, 3 ] , true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER i <= j.value2 FILTER i >= j.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ] , true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 >= i FILTER j.value2 < i + 1 SORT j.value2 RETURN j.value2", [ 2, 3 ], true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 >= i FILTER j.value2 <= i + 1 SORT j.value2 RETURN j.value2", [ 2, 3, 3, 4 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 == 2 FOR j IN " + cn + " FILTER i.value2 == j.value2 RETURN j.value2", [ 2 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 == 2 FOR j IN " + cn + " FILTER j.value2 == i.value2 RETURN j.value2", [ 2 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 == 2 || i.value2 == 3 FOR j IN " + cn + " FILTER j.value2 == i.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 == i.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER i.value2 == j.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 >= i.value2 FILTER j.value2 <= i.value2 + 1 SORT j.value2 RETURN j.value2", [ 2, 3, 3, 4 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER i.value2 <= j.value2 FILTER i.value2 + 1 >= j.value2 SORT j.value2 RETURN j.value2", [ 2, 3, 3, 4 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 IN [ i.value2 ] SORT j.value2 RETURN j.value2", [ 2, 3 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 >= 97 FOR j IN " + cn + " FILTER j.value2 == i.value2 SORT j.value2 RETURN j.value2", [ 97, 98, 99 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 >= 97 FOR j IN " + cn + " FILTER j.value2 IN [ i.value2 ] SORT j.value2 RETURN j.value2", [ 97, 98, 99 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 >= 97 FOR j IN " + cn + " FILTER j.value2 >= i.value2 SORT j.value2 RETURN j.value2", [ 97, 98, 98, 99, 99, 99 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value2 >= 97 FOR j IN " + cn + " FILTER i.value2 <= j.value2 SORT j.value2 RETURN j.value2", [ 97, 98, 98, 99, 99, 99 ], true ]
      ];

      var opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        var resultDisabled = AQL_EXECUTE(query[0], { }, paramDisabled).json;
        var resultEnabled  = AQL_EXECUTE(query[0], { }, paramEnabled).json;

        if (query[2]) {
          assertNotEqual(-1, AQL_EXPLAIN(query[0], { }, paramEnabled).plan.rules.indexOf(ruleName), query[0]);
        }
        else {
          assertEqual(-1, AQL_EXPLAIN(query[0], { }, paramEnabled).plan.rules.indexOf(ruleName), query[0]);
        }

        assertTrue(isEqual(query[1], resultDisabled), query[0]);
        assertTrue(isEqual(query[1], resultEnabled), query[0]);

        var plans = AQL_EXPLAIN(query[0], { }, opts).plans;
        plans.forEach(function(plan) {
          var result = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
          assertEqual(query[1], result, query[0]);
        });
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function nonIndexedAttributeTestSuite () {
  var ruleName = "use-indexes";
  var cn = "UnitTestsAhuacatlRange";
  var c;

  // various choices to control the optimizer: 
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);

      var i;

      for (i = 0; i < 100; ++i) {
        c.save({ value1: i * 10, value2: i });
      }

      c.ensureSkiplist("value1", "value2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testRangesNonIndexed : function () {
      var queries = [ 
        [ "FOR i IN [ 2 ] FOR j IN " + cn + " FILTER j.value2 == i SORT j.value2 RETURN j.value2", [ 2 ] ],
        [ "FOR i IN [ 2 ] FOR j IN " + cn + " FILTER i == j.value2 SORT j.value2 RETURN j.value2", [ 2 ] ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER i == j.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ] ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 == i SORT j.value2 RETURN j.value2", [ 2, 3 ] ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 >= i FILTER j.value2 <= i SORT j.value2 RETURN j.value2", [ 2, 3 ] ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER i <= j.value2 FILTER i >= j.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ] ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 >= i FILTER j.value2 < i + 1 SORT j.value2 RETURN j.value2", [ 2, 3 ] ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 >= i FILTER j.value2 <= i + 1 SORT j.value2 RETURN j.value2", [ 2, 3, 3, 4 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 == 2 FOR j IN " + cn + " FILTER i.value2 == j.value2 RETURN j.value2", [ 2 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 == 2 FOR j IN " + cn + " FILTER j.value2 == i.value2 RETURN j.value2", [ 2 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 == 2 || i.value2 == 3 FOR j IN " + cn + " FILTER j.value2 == i.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 == i.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER i.value2 == j.value2 SORT j.value2 RETURN j.value2", [ 2, 3 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 >= i.value2 FILTER j.value2 <= i.value2 + 1 SORT j.value2 RETURN j.value2", [ 2, 3, 3, 4 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER i.value2 <= j.value2 FILTER i.value2 + 1 >= j.value2 SORT j.value2 RETURN j.value2", [ 2, 3, 3, 4 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value2 IN [ i.value2 ] SORT j.value2 RETURN j.value2", [ 2, 3 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 >= 97 FOR j IN " + cn + " FILTER j.value2 IN [ i.value2 ] SORT j.value2 RETURN j.value2", [ 97, 98, 99 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 >= 97 FOR j IN " + cn + " FILTER j.value2 >= i.value2 SORT j.value2 RETURN j.value2", [ 97, 98, 98, 99, 99, 99 ] ],
        [ "FOR i IN " + cn + " FILTER i.value2 >= 97 FOR j IN " + cn + " FILTER i.value2 <= j.value2 SORT j.value2 RETURN j.value2", [ 97, 98, 98, 99, 99, 99 ] ]
      ];

      var opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        var resultDisabled = AQL_EXECUTE(query[0], { }, paramDisabled).json;
        var resultEnabled  = AQL_EXECUTE(query[0], { }, paramEnabled).json;

        assertEqual(-1, AQL_EXPLAIN(query[0], { }, paramEnabled).plan.rules.indexOf(ruleName), query[0]);

        assertTrue(isEqual(query[1], resultDisabled), query[0]);
        assertTrue(isEqual(query[1], resultEnabled), query[0]);

        var plans = AQL_EXPLAIN(query[0], { }, opts).plans;
        plans.forEach(function(plan) {
          var result = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
          assertEqual(query[1], result, query[0]);
        });
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function nestedAttributeTestSuite () {
  var ruleName = "use-indexes";
  var cn = "UnitTestsAhuacatlRange";
  var c;

  // various choices to control the optimizer: 
  var paramEnabled  = { optimizer: { rules: [ "-all", "+" + ruleName ] } };
  var paramDisabled = { optimizer: { rules: [ "+all", "-" + ruleName ] } };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      c = db._create(cn);

      var i;

      for (i = 0; i < 100; ++i) {
        c.save({ value1: { value2: i } });
      }

      c.ensureSkiplist("value1.value2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test results
////////////////////////////////////////////////////////////////////////////////

    testRangesNested : function () {
      var queries = [ 
        [ "FOR i IN [ 2 ] FOR j IN " + cn + " FILTER j.value1.value2 == i SORT j.value1.value2 RETURN j.value1.value2", [ 2 ], true ],
        [ "FOR i IN [ 2 ] FOR j IN " + cn + " FILTER i == j.value1.value2 SORT j.value1.value2 RETURN j.value1.value2", [ 2 ], true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER i == j.value1.value2 SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3 ], true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value1.value2 == i SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3 ], true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value1.value2 >= i FILTER j.value1.value2 <= i SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3 ] , true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER i <= j.value1.value2 FILTER i >= j.value1.value2 SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3 ] , true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value1.value2 >= i FILTER j.value1.value2 < i + 1 SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3 ], true ],
        [ "FOR i IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value1.value2 >= i FILTER j.value1.value2 <= i + 1 SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3, 3, 4 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 == 2 FOR j IN " + cn + " FILTER i.value1.value2 == j.value1.value2 RETURN j.value1.value2", [ 2 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 == 2 FOR j IN " + cn + " FILTER j.value1.value2 == i.value1.value2 RETURN j.value1.value2", [ 2 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 == 2 || i.value1.value2 == 3 FOR j IN " + cn + " FILTER j.value1.value2 == i.value1.value2 SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value1.value2 == i.value1.value2 SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER i.value1.value2 == j.value1.value2 SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value1.value2 >= i.value1.value2 FILTER j.value1.value2 <= i.value1.value2 + 1 SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3, 3, 4 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER i.value1.value2 <= j.value1.value2 FILTER i.value1.value2 + 1 >= j.value1.value2 SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3, 3, 4 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 IN [ 2, 3 ] FOR j IN " + cn + " FILTER j.value1.value2 IN [ i.value1.value2 ] SORT j.value1.value2 RETURN j.value1.value2", [ 2, 3 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 >= 97 FOR j IN " + cn + " FILTER j.value1.value2 IN [ i.value1.value2 ] SORT j.value1.value2 RETURN j.value1.value2", [ 97, 98, 99 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 >= 97 FOR j IN " + cn + " FILTER j.value1.value2 >= i.value1.value2 SORT j.value1.value2 RETURN j.value1.value2", [ 97, 98, 98, 99, 99, 99 ], true ],
        [ "FOR i IN " + cn + " FILTER i.value1.value2 >= 97 FOR j IN " + cn + " FILTER i.value1.value2 <= j.value1.value2 SORT j.value1.value2 RETURN j.value1.value2", [ 97, 98, 98, 99, 99, 99 ], true ]
      ];

      var opts = _.clone(paramEnabled);
      opts.allPlans = true;
      opts.verbosePlans = true;

      queries.forEach(function(query) {
        var resultDisabled = AQL_EXECUTE(query[0], { }, paramDisabled).json;
        var resultEnabled  = AQL_EXECUTE(query[0], { }, paramEnabled).json;

        if (query[2]) {
          assertNotEqual(-1, AQL_EXPLAIN(query[0], { }, paramEnabled).plan.rules.indexOf(ruleName), query[0]);
        }
        else {
          assertEqual(-1, AQL_EXPLAIN(query[0], { }, paramEnabled).plan.rules.indexOf(ruleName), query[0]);
        }

        assertTrue(isEqual(query[1], resultDisabled), query[0]);
        assertTrue(isEqual(query[1], resultEnabled), query[0]);

        var plans = AQL_EXPLAIN(query[0], { }, opts).plans;
        plans.forEach(function(plan) {
          var result = AQL_EXECUTEJSON(plan, { optimizer: { rules: [ "-all" ] } }).json;
          assertEqual(query[1], result, query[0]);
        });
      });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(singleAttributeTestSuite);
jsunity.run(nonIndexedAttributeTestSuite);
jsunity.run(nestedAttributeTestSuite);

return jsunity.done();

