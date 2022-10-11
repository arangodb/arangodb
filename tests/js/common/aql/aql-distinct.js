/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, fail, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for COLLECT w/ COUNT
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
var db = require("@arangodb").db;
var internal = require("internal");
var helper = require("@arangodb/aql-helper");
var errors = internal.errors;
var _ = require("lodash");
  
var containsDistinct = function(query) {
  var plan = AQL_EXPLAIN(query).plan;

  var nodes = helper.findExecutionNodes(plan, "CollectNode");
  assertTrue(nodes.length >= 1);
  assertTrue(nodes.reduce(function(node, prev) {
    return prev || (node.collectOptions.method === 'distinct');
  }), false);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlDistinct () {
  var c;
        
  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 100; ++i) {
        c.save({ value1: i, value2: i % 10, value3: "test" + i, value4: "test" + (i % 10) });
      }
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage in different places
////////////////////////////////////////////////////////////////////////////////

    testDistinctUsageFail : function () {
      var queries = [
        "FOR i IN DISTINCT 1..10 RETURN i",
        "FOR i IN 1..10 LET a = DISTINCT i RETURN a",
        "RETURN DISTINCT 1",
        "LET x = DISTINCT 1 RETURN x",
        "LET a = 1 LET x = DISTINCT a RETURN x",
        "LET a = 1 COLLECT x = a RETURN DISTINCT x",
        "LET x = (FOR i IN 1..100 RETURN i) RETURN DISTINCT x",
        "LET values = (RETURN DISTINCT 1) RETURN values"
      ];

      queries.forEach(function(query) {
        try {
          AQL_EXECUTE(query);
          fail();
        } catch (e) {
          assertEqual(errors.ERROR_QUERY_PARSE.code, e.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctInTopLevelForWithInteger : function () {
      var query = "FOR i IN " + c.name() + " RETURN DISTINCT i.value2";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;
      result = result.sort();

      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctInTopLevelForWithString : function () {
      var query = "FOR i IN " + c.name() + " RETURN DISTINCT i.value4";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;
      result = result.sort();

      assertEqual([ "test0", "test1", "test2", "test3", "test4", "test5", "test6", "test7", "test8", "test9" ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctOnDistinctValuesWithInteger : function () {
      var query = "FOR i IN " + c.name() + " RETURN DISTINCT i.value1";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;
      assertEqual(100, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctOnDistinctValuesWithString : function () {
      var query = "FOR i IN " + c.name() + " RETURN DISTINCT i.value3";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;
      assertEqual(100, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctInTopLevelForWithExpression : function () {
      var query = "FOR i IN " + c.name() + " RETURN DISTINCT i.value1 % 10";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;
      result = result.sort();

      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctReferringToLet : function () {
      var query = "LET a = PASSTHRU(1) FOR i IN " + c.name() + " RETURN DISTINCT a";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;
      assertEqual([ 1 ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctInSubquery : function () {
      var query = "LET values = (FOR i IN " + c.name() + " RETURN DISTINCT i.value2) RETURN values";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json[0];
      result = result.sort();

      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctInSubqueries : function () {
      var query = "LET values1 = (FOR i IN " + c.name() + " RETURN DISTINCT i.value2) LET values2 = (FOR i IN " + c.name() + " RETURN DISTINCT i.value4) RETURN [ values1, values2 ]";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json[0];
      var integers = result[0].sort();
      var strings  = result[1].sort();

      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 ], integers);
      assertEqual([ "test0", "test1", "test2", "test3", "test4", "test5", "test6", "test7", "test8", "test9" ], strings);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctOnDistinctValuesInSubqueries : function () {
      var query = "LET values1 = (FOR i IN " + c.name() + " RETURN DISTINCT i.value1) LET values2 = (FOR i IN " + c.name() + " RETURN DISTINCT i.value3) RETURN [ values1, values2 ]";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json[0];
      var integers = result[0];
      var strings  = result[1];

      assertEqual(100, integers.length);
      assertEqual(100, strings.length);
    },
            
    testDistinctInOptimizedAwaySubquery : function () {
      let query = "LET values = (FOR i IN " + c.name() + " RETURN DISTINCT i.value1) FOR doc IN values RETURN 42";
      containsDistinct(query);
      let result = AQL_EXECUTE(query).json;

      assertEqual(100, result.length);
  
      let plan = AQL_EXPLAIN(query).plan;
      assertNotEqual(-1, plan.rules.indexOf("inline-subqueries"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctWithCollect1 : function () {
      var query = "FOR i IN " + c.name() + " COLLECT x = i.value1 % 11 RETURN DISTINCT x";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;
      result = result.sort(function(l, r) {
        return l - r;
      });

      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctWithCollect2 : function () {
      var query = "FOR i IN " + c.name() + " COLLECT x = i.value1 RETURN DISTINCT x % 11";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;
      result = result.sort(function(l, r) {
        return l - r;
      });

      assertEqual([ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctWithArrays1 : function () {
      var query = "FOR i IN 1..2 RETURN DISTINCT (FOR j IN [ 1, 2, 3, 4, 1, 3 ] RETURN j)";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;

      assertEqual(1, result.length);
      assertEqual([ 1, 2, 3, 4, 1, 3 ], result[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testDistinctWithArrays2 : function () {
      var query = "FOR i IN 1..2 RETURN (FOR j IN [ 1, 2, 3, 4, 1, 3 ] RETURN DISTINCT j)";
      containsDistinct(query);
      var result = AQL_EXECUTE(query).json;

      assertEqual(2, result.length);
      assertEqual([ 1, 2, 3, 4 ], result[0].sort(function (l, r) { return l - r; }));
      assertEqual([ 1, 2, 3, 4 ], result[1].sort(function (l, r)  { return l - r; }));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct regression test:
/// crash when a subsequent call to DistinctCollectBlock::getSome() added no new
/// non-distinct results. The crash may occur ONLY IN MAINTAINER MODE, while
/// in non-maintainer mode problems may occur only because either an empty
/// result is returned (which would cause some parent blocks to crash), or
/// an erroneous result is returned, but I didn't find a way to produce this
/// reliably with an AQL query.
////////////////////////////////////////////////////////////////////////////////

    testDistinctRegressionGetSomeWithEmptyResult : function () {
      const query = 'FOR i IN 1..2 FOR k IN 1..1000 RETURN DISTINCT k';
      containsDistinct(query);
      const options = {optimizer: {rules: [ "-interchange-adjacent-enumerations" ]}};

      // check plan
      const plan = helper.getCompactPlan(AQL_EXPLAIN(query, {}, options));
      assertEqual(
        [
          "SingletonNode",
          "CalculationNode",
          "CalculationNode",
          "EnumerateListNode",
          "EnumerateListNode",
          "CollectNode",
          "ReturnNode",
        ],
        plan.map(node => node.type)
      );

      // execute query
      const result = AQL_EXECUTE(query, {}, options).json;
      assertEqual(1000, result.length);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for COLLECT
////////////////////////////////////////////////////////////////////////////////

function ahuacatlCollect () {
  var c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      for (var i = 0; i < 10; ++i) {
        for (var j = 0; j < 10; ++j) {
          c.save({a:{key: i, name: j}});
          c.save({a:{key: i, name: j}});
        }
      }
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief distinct usage 
////////////////////////////////////////////////////////////////////////////////

    testCollect1 : function () {
      var query = "FOR result IN UnitTestsCollection COLLECT key=result.a.key, name = result.a.name INTO group LIMIT 2,5 RETURN [key,name]";
      var result = AQL_EXECUTE(query).json;
      assertEqual([[0,2],[0,3],[0,4],[0,5],[0,6]], result);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlDistinct);
jsunity.run(ahuacatlCollect);

return jsunity.done();

