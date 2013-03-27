////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, limit optimisations
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
var internal = require("internal");
var ArangoError = require("org/arangodb/arango-error").ArangoError; 
var EXPLAIN = internal.AQL_EXPLAIN; 
var QUERY = internal.AQL_QUERY; 

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimiserLimitTestSuite () {
  var collection = null;
  var cn = "UnitTestsAhuacatlOptimiserLimit";

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = QUERY(query, undefined);
    if (cursor instanceof ArangoError) {
      print(query, cursor.errorMessage);
    }
    assertFalse(cursor instanceof ArangoError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief explain a given query
////////////////////////////////////////////////////////////////////////////////

  function explainQuery (query) {
    return EXPLAIN(query);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, isFlat) {
    var result = executeQuery(query).getRows();
    var results = [ ];

    for (var i in result) {
      if (!result.hasOwnProperty(i)) {
        continue;
      }

      var row = result[i];
      if (isFlat) {
        results.push(row);
      } 
      else {
        var keys = [ ];
        for (var k in row) {
          if (row.hasOwnProperty(k) && k != '_id' && k != '_rev' && k != '_key') {
            keys.push(k);
          }
        }
       
        keys.sort();
        var resultRow = { };
        for (var k in keys) {
          if (keys.hasOwnProperty(k)) {
            resultRow[keys[k]] = row[keys[k]];
          }
        }
        results.push(resultRow);
      }
    }

    return results;
  }


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      for (var i = 0; i < 100; ++i) {
        collection.save({ "value" : i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation for non-collection access, limit > 0
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionNoRestriction : function () {
      var list = [ ];
      for (var i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 500, expectedLength: 100 },
        { offset: 0, limit: 50, expectedLength: 50 },
        { offset: 0, limit: 5, expectedLength: 5 },
        { offset: 0, limit: 1, expectedLength: 1 },
        { offset: 1, limit: 50, expectedLength: 50 },
        { offset: 1, limit: 1, expectedLength: 1 },
        { offset: 10, limit: 50, expectedLength: 50 },
        { offset: 95, limit: 5, expectedLength: 5 },
        { offset: 95, limit: 50, expectedLength: 5 },
        { offset: 98, limit: 50, expectedLength: 2 },
        { offset: 98, limit: 2, expectedLength: 2 },
        { offset: 99, limit: 1, expectedLength: 1 },
        { offset: 99, limit: 2, expectedLength: 1 },
        { offset: 100, limit: 2, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query, false);
        assertEqual(test.expectedLength, actual.length);

        var explain = explainQuery(query);
        assertEqual("for", explain[0].type);
        assertEqual(true, explain[0].limit);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation for full collection access, limit > 0
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionNoRestriction : function () {
      var tests = [
        { offset: 0, limit: 500, expectedLength: 100 },
        { offset: 0, limit: 50, expectedLength: 50 },
        { offset: 0, limit: 5, expectedLength: 5 },
        { offset: 0, limit: 1, expectedLength: 1 },
        { offset: 1, limit: 50, expectedLength: 50 },
        { offset: 1, limit: 1, expectedLength: 1 },
        { offset: 10, limit: 50, expectedLength: 50 },
        { offset: 95, limit: 5, expectedLength: 5 },
        { offset: 95, limit: 50, expectedLength: 5 },
        { offset: 98, limit: 50, expectedLength: 2 },
        { offset: 98, limit: 2, expectedLength: 2 },
        { offset: 99, limit: 1, expectedLength: 1 },
        { offset: 99, limit: 2, expectedLength: 1 },
        { offset: 100, limit: 2, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query, false);
        assertEqual(test.expectedLength, actual.length);

        var explain = explainQuery(query);
        assertEqual("for", explain[0].type);
        assertEqual(test.offset + test.limit, explain[0]["expression"]["extra"]["limit"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation for non collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionNoRestrictionEmpty : function () {
      var list = [ ];
      for (var i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 0 },
        { offset: 1, limit: 0 },
        { offset: 10, limit: 0 },
        { offset: 95, limit: 0 },
        { offset: 98, limit: 0 },
        { offset: 99, limit: 0 },
        { offset: 100, limit: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query, false);
        assertEqual(0, actual.length);

        var explain = explainQuery(query);
        assertEqual("return (empty)", explain[0].type);
        assertEqual("const list", explain[0].expression.type);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation for full collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionNoRestrictionEmpty : function () {
      var tests = [
        { offset: 0, limit: 0 },
        { offset: 1, limit: 0 },
        { offset: 10, limit: 0 },
        { offset: 95, limit: 0 },
        { offset: 98, limit: 0 },
        { offset: 99, limit: 0 },
        { offset: 100, limit: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query, false);
        assertEqual(0, actual.length);

        var explain = explainQuery(query);
        assertEqual("return (empty)", explain[0].type);
        assertEqual("const list", explain[0].expression.type);
      }
    },
      
////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation for non-collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionDoubleLimitEmpty : function () {
      var list = [ ];
      for (var i = 0; i < 100; ++i) {
        list.push(i);
      }

      var query = "FOR c IN " + JSON.stringify(list) + " LIMIT 10 LIMIT 0 RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(0, actual.length);

      var explain = explainQuery(query);
      assertEqual("return (empty)", explain[0].type);
      assertEqual("const list", explain[0].expression.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation for full collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionDoubleLimitEmpty : function () {
      var query = "FOR c IN " + cn + " LIMIT 10 LIMIT 0 RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(0, actual.length);

      var explain = explainQuery(query);
      assertEqual("return (empty)", explain[0].type);
      assertEqual("const list", explain[0].expression.type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with 2 limits
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionLimitLimit : function () {
      var list = [ ];
      for (var i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 500, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 20, expectedLength: 5 },
        { offset: 10, limit: 50, offset2: 1, limit2: 20, expectedLength: 20 },
        { offset: 10, limit: 90, offset2: 10, limit2: 20, expectedLength: 20 },
        { offset: 90, limit: 10, offset2: 9, limit2: 20, expectedLength: 1 },
        { offset: 50, limit: 50, offset2: 0, limit2: 50, expectedLength: 50 },
        { offset: 50, limit: 50, offset2: 10, limit2: 50, expectedLength: 40 },
        { offset: 50, limit: 50, offset2: 50, limit2: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " LIMIT " + test.offset + ", " + test.limit + " LIMIT " + test.offset2 + ", " + test.limit2 + " RETURN c";

        var actual = getQueryResults(query, false);
        assertEqual(test.expectedLength, actual.length);

        var explain = explainQuery(query);
        assertEqual("for", explain[0].type);
        assertEqual(true, explain[0].limit);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with 2 limits
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionLimitLimit : function () {
      var tests = [
        { offset: 0, limit: 500, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 20, expectedLength: 5 },
        { offset: 10, limit: 50, offset2: 1, limit2: 20, expectedLength: 20 },
        { offset: 10, limit: 90, offset2: 10, limit2: 20, expectedLength: 20 },
        { offset: 90, limit: 10, offset2: 9, limit2: 20, expectedLength: 1 },
        { offset: 50, limit: 50, offset2: 0, limit2: 50, expectedLength: 50 },
        { offset: 50, limit: 50, offset2: 10, limit2: 50, expectedLength: 40 },
        { offset: 50, limit: 50, offset2: 50, limit2: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " LIMIT " + test.offset + ", " + test.limit + " LIMIT " + test.offset2 + ", " + test.limit2 + " RETURN c";

        var actual = getQueryResults(query, false);
        assertEqual(test.expectedLength, actual.length);

        var explain = explainQuery(query);
        assertEqual("for", explain[0].type);
        assertEqual(test.offset + test.limit, explain[0]["expression"]["extra"]["limit"]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation for non-collection access, limit > 0 and
/// filter conditions
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionFilter : function () {
      var list = [ ];
      for (var i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " FILTER c >= " + test.value + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query, false);
        assertEqual(test.expectedLength, actual.length);
      
        var explain = explainQuery(query);
        assertEqual("for", explain[0].type);
        assertEqual(true, explain[0].limit);
      }
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation for full collection access, limit > 0 and
/// filter conditions
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionFilter : function () {
      var tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " FILTER c.value >= " + test.value + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query, false);
        assertEqual(test.expectedLength, actual.length);
      
        var explain = explainQuery(query);
        assertEqual("for", explain[0].type);
        assertEqual(true, explain[0].limit);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation for full collection access, limit > 0 and
/// filter conditions
////////////////////////////////////////////////////////////////////////////////

    testLimitFilterFilterCollectionFilter : function () {
      var tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " FILTER c.value >= " + test.value + " FILTER c.value <= 9999 LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query, false);
        assertEqual(test.expectedLength, actual.length);
      
        var explain = explainQuery(query);
        assertEqual("for", explain[0].type);
        assertEqual(undefined, explain[0].limit);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionHashIndex1 : function () {
      collection.ensureHashIndex("value");

      var query = "FOR c IN " + cn + " FILTER c.value == 23 || c.value == 24 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(2, actual.length);
      assertEqual(23, actual[0].value);
      assertEqual(24, actual[1].value);

      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(true, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionHashIndex2 : function () {
      collection.ensureHashIndex("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(true, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFilterFilterCollectionHashIndex : function () {
      collection.ensureHashIndex("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 FILTER c.value <= 9999 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(undefined, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSkiplistIndex1 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value == 23 || c.value == 24 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(2, actual.length);
      assertEqual(23, actual[0].value);
      assertEqual(24, actual[1].value);

      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(true, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSkiplistIndex2 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(true, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFilterFilterSkiplistIndex : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 FILTER c.value <= 9999 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(undefined, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort1 : function () {
      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(true, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort2 : function () {
      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(10, actual.length);

      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(22, actual[2].value);
      assertEqual(29, actual[9].value);

      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(true, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort3 : function () {
      var query = "FOR c IN " + cn + " SORT c.value LIMIT 0, 10 FILTER c.value >= 20 && c.value < 30 RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(0, actual.length);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(undefined, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimisation with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort4 : function () {
      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 SORT c.value LIMIT 0, 10 RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(10, actual.length);

      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(22, actual[2].value);
      assertEqual(29, actual[9].value);
        
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual(undefined, explain[0].limit);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested1 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] FOR i IN [ 5, 6, 7 ] LIMIT 2 RETURN { o: o, i: i }";

      var actual = getQueryResults(query, false);
      assertEqual([ { i: 5, o: 1 }, { i: 6, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested2 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] FOR i IN [ 5, 6, 7 ] LIMIT 0, 1 RETURN { o: o, i: i }";

      var actual = getQueryResults(query, false);
      assertEqual([ { i: 5, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested3 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] FOR i IN [ 5, 6, 7 ] LIMIT 1, 1 RETURN { o: o, i: i }";

      var actual = getQueryResults(query, false);
      assertEqual([ { i: 6, o: 2 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested4 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] LIMIT 1 FOR i IN [ 5, 6, 7 ] RETURN { o: o, i: i }";

      var actual = getQueryResults(query, false);
      assertEqual([ { i: 5, o: 1 }, { i: 6, o: 1 }, { i: 7, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested5 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] LIMIT 1, 1 FOR i IN [ 5, 6, 7 ] RETURN { o: o, i: i }";

      var actual = getQueryResults(query, false);
      assertEqual([ { i: 5, o: 2 }, { i: 6, o: 2 }, { i: 7, o: 2 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested6 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] LIMIT 1 FOR i IN [ 5, 6, 7 ] LIMIT 2 RETURN { o: o, i: i }";

      var actual = getQueryResults(query, false);
      assertEqual([ { i: 5, o: 1 }, { i: 6, o: 1 } ], actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimiserLimitTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
