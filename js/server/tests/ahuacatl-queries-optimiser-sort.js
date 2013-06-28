////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, sort optimisations
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
var ArangoError = require("org/arangodb").ArangoError;
var EXPLAIN = internal.AQL_EXPLAIN; 
var QUERY = internal.AQL_QUERY; 

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimiserSortTestSuite () {
  var collection = null;
  var cn = "UnitTestsAhuacatlOptimiserSort";

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
        collection.save({ "value" : i, "value2" : i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation without index
////////////////////////////////////////////////////////////////////////////////

    testNoIndex1 : function () {
      var query = "FOR c IN " + cn + " SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("sort", explain[1].type); // no optimisation as there's no index
      assertEqual("return", explain[2].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation without index
////////////////////////////////////////////////////////////////////////////////

    testNoIndex2 : function () {
      var query = "FOR c IN " + cn + " SORT c.value DESC RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(99, actual[0].value);
      assertEqual(98, actual[1].value);
      assertEqual(97, actual[2].value);
      assertEqual(96, actual[3].value);
      assertEqual(49, actual[50].value);
      assertEqual(1, actual[98].value);
      assertEqual(0, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("sort", explain[1].type); // no optimisation as there's no index
      assertEqual("return", explain[2].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testSkiplist1 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("sort", explain[1].type); // no optimisation as there's no filter and thus no index involved
      assertEqual("return", explain[2].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testSkiplist2 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 15 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(85, actual.length);
      assertEqual(15, actual[0].value);
      assertEqual(16, actual[1].value);
      assertEqual(17, actual[2].value);
      assertEqual(18, actual[3].value);
      assertEqual(98, actual[83].value);
      assertEqual(99, actual[84].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); // sort optimised away
      assertEqual("return", explain[2].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testSkiplist3 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 15 SORT c.value DESC RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(85, actual.length);
      assertEqual(99, actual[0].value);
      assertEqual(98, actual[1].value);
      assertEqual(97, actual[2].value);
      assertEqual(96, actual[3].value);
      assertEqual(16, actual[83].value);
      assertEqual(15, actual[84].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // sort still there because sort order is DESC
      assertEqual("return", explain[3].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testMultipleSorts1 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); // sort optimised away
      assertEqual("return", explain[2].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testMultipleSorts2 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value SORT c.value DESC RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(99, actual[0].value);
      assertEqual(98, actual[1].value);
      assertEqual(97, actual[2].value);
      assertEqual(96, actual[3].value);
      assertEqual(49, actual[50].value);
      assertEqual(1, actual[98].value);
      assertEqual(0, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // sort still there
      assertEqual("return", explain[3].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testMultipleSorts3 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value DESC SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      var explain = explainQuery(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // sort still there
      assertEqual("sort", explain[3].type); // sort still there
      assertEqual("return", explain[4].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields1 : function () {
      collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // sort still there
      assertEqual("return", explain[3].type);
      
      query = "FOR c IN " + cn + " FILTER c.value == 0 && c.value2 >= 0 SORT c.value RETURN c";

      actual = getQueryResults(query, false);
      assertEqual(1, actual.length);
      assertEqual(0, actual[0].value);
      
      explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); // sort optimised away
      assertEqual("return", explain[2].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields2 : function () {
      collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value, c.value2 RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // sort still there
      assertEqual("return", explain[3].type);
      
      query = "FOR c IN " + cn + " FILTER c.value == 0 && c.value2 <= 1 SORT c.value, c.value2 RETURN c";

      actual = getQueryResults(query, false);
      assertEqual(1, actual.length);
      assertEqual(0, actual[0].value);

      explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); // sort optimised away
      assertEqual("return", explain[2].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields3 : function () {
      collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value2 >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // cannot use index for sorting
      assertEqual("return", explain[3].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields4 : function () {
      collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 && c.value2 >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); // cannot use index for sorting
      assertEqual("sort", explain[2].type); // cannot use index for sorting
      assertEqual("return", explain[3].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimisation with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields5 : function () {
      collection.ensureSkiplist("value2", "value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // cannot use index for sorting
      assertEqual("return", explain[3].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testNonFieldSort1 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value + 1 RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // sort still there
      assertEqual("return", explain[3].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testNonFieldSort2 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT 1 + c.value RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // sort still there
      assertEqual("return", explain[3].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testNonFieldSort3 : function () {
      collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value * 2 RETURN c";

      var actual = getQueryResults(query, false);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      var explain = explainQuery(query);
      assertEqual("for", explain[0].type);
      assertEqual("filter", explain[1].type); 
      assertEqual("sort", explain[2].type); // sort still there
      assertEqual("return", explain[3].type);
    }

  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimiserSortTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
