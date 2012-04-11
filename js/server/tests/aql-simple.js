////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, simple queries
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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlSimpleTestSuite () {
  var collection = null;

  ////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query) {
    var cursor = AQL_STATEMENT(query, undefined);
    if (cursor instanceof AvocadoQueryError) {
      print(query, cursor.message);
    }
    assertFalse(cursor instanceof AvocadoQueryError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query) {
    var cursor = executeQuery(query);
    if (cursor) {
      var results = [ ];
      while (cursor.hasNext()) {
        results.push(cursor.next());
      }
      return results;
    }
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a query string from the parameters given and run the query
////////////////////////////////////////////////////////////////////////////////

  function runQuery (select, wheres, limit, order) {
    var query = assembleQuery(select, wheres, limit, order);
    var result = getQueryResults(query);
    assertFalse(result instanceof AvocadoQueryError);

    return result;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a query string from the parameters given
////////////////////////////////////////////////////////////////////////////////

  function assembleQuery (select, wheres, limit, order) {
    var selectClause = select;
    
    var whereClause = "";
    if (wheres instanceof Array) {
      whereClause = "WHERE ";
      var found = false;
      for (var i in wheres) {
        if (wheres[i] == "" || wheres[i] === undefined || wheres[i] === null || !wheres.hasOwnProperty(i)) {
          continue;
        }
        if (found) {
          whereClause += "&& ";
        }
        found = true;
        whereClause += wheres[i] + " ";
      }
    }

    var orderClause = "";
    if (order != "" && order !== undefined && order !== null) {
      orderClause = "ORDER BY " + order + " ";
    }

    var limitClause = "";
    if (limit !== undefined && limit !== null) {
      limitClause = "LIMIT " + limit;
    }

    var query = "SELECT " + selectClause + " FROM " + collection.name() + " c " + 
                whereClause + orderClause + limitClause;

    return query;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check the length of a query result set
////////////////////////////////////////////////////////////////////////////////

  function checkLength (expected, value1, value2, limit) {
    var result = runQuery("{ }", new Array(value1, value2), limit);
    assertTrue(result instanceof Array);
    var actual = result.length;
    assertEqual(expected, actual);
  }


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      collection = db.UnitTestsNumbersList;

      if (collection.count() == 0) {
        for (var i = 0; i <= 20; i++) {
          for (var j = 0; j <= i; j++) {
            collection.save({ value1: i, value2: j, value3: i * j});
          }
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for a simple eq condition matches
////////////////////////////////////////////////////////////////////////////////

    testResultLengthsEqIndividual1 : function () {
      for (i = 0; i <= 20; i++) {
        checkLength(i + 1, 'c.value1 == ' + i);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for a simple eq condition matches
////////////////////////////////////////////////////////////////////////////////

    testResultLengthsEqIndividual2 : function () {
      for (i = 0; i <= 20; i++) {
        checkLength(21 - i, undefined, 'c.value2 == ' + i);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for combined eq conditions match
////////////////////////////////////////////////////////////////////////////////

    testResultLengthsEqCombined : function () {
      for (i = 0; i <= 20; i++) {
        for (j = 0; j <= i; j++) {
          checkLength(1, 'c.value1 == ' + i, 'c.value2 == ' + j);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for range queries matches
////////////////////////////////////////////////////////////////////////////////
  
    testResultLengthsBetweenIndividual1 : function () {
      for (i = 0; i <= 20; i++) {
        checkLength(i + 1, 'c.value1 >= ' + i, 'c.value1 <= ' + i);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for (senseless) range queries matches
////////////////////////////////////////////////////////////////////////////////
  
    testResultLengthsBetweenIndividual2 : function () {
      for (i = 0; i <= 20; i++) {
        checkLength(0, 'c.value1 > ' + i, 'c.value1 < ' + i);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for single-range queries matches
////////////////////////////////////////////////////////////////////////////////
  
    testResultLengthsLessIndividual1 : function () {
      var expected = 0;
      for (i = 0; i <= 20; i++) {
        for (var j = 0; j <= i; j++, expected++);
        checkLength(expected, 'c.value1 <= ' + i);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for range queries matches
////////////////////////////////////////////////////////////////////////////////
  
    testResultLengthsLessIndividual2 : function () {
      var expected = 0;
      for (i = 0; i <= 20; i++) {
        expected += 21 - i;
        checkLength(expected, 'c.value2 <= ' + i);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for out-of-range queries is 0
////////////////////////////////////////////////////////////////////////////////

    testNonResultsIndividual1 : function () {
      checkLength(0, 'c.value1 < 0');
      checkLength(0, 'c.value1 <= -0.01');
      checkLength(0, 'c.value1 < -0.1');
      checkLength(0, 'c.value1 <= -0.1');
      checkLength(0, 'c.value1 <= -1');
      checkLength(0, 'c.value1 <= -1.0');
      checkLength(0, 'c.value1 < -10');
      checkLength(0, 'c.value1 <= -100');
      
      checkLength(0, 'c.value1 == 21');
      checkLength(0, 'c.value1 == 110');
      checkLength(0, 'c.value1 == 20.001');
      checkLength(0, 'c.value1 > 20');
      checkLength(0, 'c.value1 >= 20.01');
      checkLength(0, 'c.value1 > 100');
      checkLength(0, 'c.value1 >= 100');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for out-of-range queries is 0
////////////////////////////////////////////////////////////////////////////////
  
    testNonResultsIndividual2 : function () {
      checkLength(0, 'c.value2 < 0');
      checkLength(0, 'c.value2 <= -0.01');
      checkLength(0, 'c.value2 < -0.1');
      checkLength(0, 'c.value2 <= -0.1');
      checkLength(0, 'c.value2 <= -1');
      checkLength(0, 'c.value2 <= -1.0');
      checkLength(0, 'c.value2 < -10');
      checkLength(0, 'c.value2 <= -100');
      
      checkLength(0, 'c.value2 == 21');
      checkLength(0, 'c.value2 == 110');
      checkLength(0, 'c.value2 == 20.001');
      checkLength(0, 'c.value2 > 20');
      checkLength(0, 'c.value2 >= 20.01');
      checkLength(0, 'c.value2 > 100');
      checkLength(0, 'c.value2 >= 100');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for out-of-range queries is 0
////////////////////////////////////////////////////////////////////////////////
  
    testNonResultsCombined : function () {
      checkLength(0, 'c.value1 > 0', 'c.value2 > 20');
      checkLength(0, 'c.value1 >= 0', 'c.value2 > 20');
      checkLength(0, 'c.value1 < 0', 'c.value2 > 0');
      checkLength(0, 'c.value1 < 0', 'c.value2 >= 0');

      checkLength(0, 'c.value1 == 21', 'c.value2 == 0');
      checkLength(0, 'c.value1 == 21', 'c.value2 == 1');
      checkLength(0, 'c.value1 == 21', 'c.value2 >= 0');
      checkLength(0, 'c.value1 == 1', 'c.value2 == 21');
      checkLength(0, 'c.value1 == 0', 'c.value2 == 21');
      checkLength(0, 'c.value1 >= 0', 'c.value2 == 21');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for (illogical) ranges is 0
////////////////////////////////////////////////////////////////////////////////
  
    testNonResultsIllogical : function () { 
      checkLength(0, 'c.value1 > 0', 'c.value1 > 20');
      checkLength(0, 'c.value1 >= 0', 'c.value1 > 20');
      checkLength(0, 'c.value1 == 0', 'c.value1 > 20');
      checkLength(0, 'c.value1 != 0', 'c.value1 > 20');
      checkLength(0, 'c.value1 > 20', 'c.value1 > 0');
      checkLength(0, 'c.value1 > 20', 'c.value1 >= 0');
      checkLength(0, 'c.value1 >= 21', 'c.value1 > 0');
      checkLength(0, 'c.value1 >= 21', 'c.value1 >= 0');
      checkLength(0, 'c.value1 >= 21', 'c.value1 == 0');
      checkLength(0, 'c.value1 >= 21', 'c.value1 != 0');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries matches
////////////////////////////////////////////////////////////////////////////////

    testLimitNonOffset1 : function () { 
      checkLength(0, 'c.value1 == 0', undefined, '0');
      checkLength(0, 'c.value1 == 0', undefined, '0,0');

      checkLength(1, 'c.value1 == 0', undefined, '1');
      checkLength(1, 'c.value1 == 0', undefined, '2');
      checkLength(1, 'c.value1 == 0', undefined, '3');

      checkLength(1, 'c.value1 == 0', undefined, '0,1');
      checkLength(1, 'c.value1 == 0', undefined, '0,2');
      checkLength(1, 'c.value1 == 0', undefined, '0,3');

      checkLength(1, 'c.value1 == 0', undefined, '0,-1');
      checkLength(1, 'c.value1 == 0', undefined, '0,-2');
      checkLength(1, 'c.value1 == 0', undefined, '0,-3');

      checkLength(1, 'c.value1 == 0', undefined, '-1');
      checkLength(1, 'c.value1 == 0', undefined, '-2');
      checkLength(1, 'c.value1 == 0', undefined, '-3');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries w/ offset matches
////////////////////////////////////////////////////////////////////////////////

    testLimitOffset1 : function () {
      checkLength(0, 'c.value1 == 0', undefined, '1,1');
      checkLength(0, 'c.value1 == 0', undefined, '1,-1');
      checkLength(0, 'c.value1 == 0', undefined, '1,0');
      
      checkLength(0, 'c.value1 == 0', undefined, '1,2');
      checkLength(0, 'c.value1 == 0', undefined, '1,-2');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries matches
////////////////////////////////////////////////////////////////////////////////
  
    testLimitNonOffset : function () {
      checkLength(0, 'c.value1 == 3', undefined, '0');
      checkLength(0, 'c.value1 == 3', undefined, '0,0');

      checkLength(1, 'c.value1 == 3', undefined, '1');
      checkLength(2, 'c.value1 == 3', undefined, '2');
      checkLength(3, 'c.value1 == 3', undefined, '3');

      checkLength(1, 'c.value1 == 3', undefined, '0,1');
      checkLength(2, 'c.value1 == 3', undefined, '0,2');
      checkLength(3, 'c.value1 == 3', undefined, '0,3');

      checkLength(1, 'c.value1 == 3', undefined, '0,-1');
      checkLength(2, 'c.value1 == 3', undefined, '0,-2');
      checkLength(3, 'c.value1 == 3', undefined, '0,-3');

      checkLength(1, 'c.value1 == 3', undefined, '-1');
      checkLength(2, 'c.value1 == 3', undefined, '-2');
      checkLength(3, 'c.value1 == 3', undefined, '-3');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries w/ offset matches
////////////////////////////////////////////////////////////////////////////////
  
    testLimitOffset2 : function () {
      checkLength(1, 'c.value1 == 3', undefined, '1,1');
      checkLength(1, 'c.value1 == 3', undefined, '1,-1');
      checkLength(0, 'c.value1 == 3', undefined, '1,0');
      
      checkLength(2, 'c.value1 == 3', undefined, '1,2');
      checkLength(3, 'c.value1 == 3', undefined, '1,3');
      checkLength(2, 'c.value1 == 3', undefined, '1,-2');
      checkLength(3, 'c.value1 == 3', undefined, '1,-3');

      checkLength(2, 'c.value1 == 3', undefined, '2,2');
      checkLength(2, 'c.value1 == 3', undefined, '2,3');
      checkLength(2, 'c.value1 == 3', undefined, '2,-2');
      checkLength(2, 'c.value1 == 3', undefined, '2,-3');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries matches
////////////////////////////////////////////////////////////////////////////////
  
    testLimitNonOffset3 : function () {
      checkLength(0, 'c.value1 == 10', undefined, '0');
      checkLength(10, 'c.value1 == 10', undefined, '10');
      checkLength(10, 'c.value1 == 10', undefined, '0,10');
      checkLength(11, 'c.value1 == 10', undefined, '0,11');
      checkLength(11, 'c.value1 == 10', undefined, '0,12');
      checkLength(11, 'c.value1 == 10', undefined, '0,100');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries w/ offset matches
////////////////////////////////////////////////////////////////////////////////

    testLimitOffset3 : function () {
      checkLength(10, 'c.value1 == 10', undefined, '1,10');
      checkLength(10, 'c.value1 == 10', undefined, '1,100');
      checkLength(10, 'c.value1 == 10', undefined, '1,-10');
      checkLength(10, 'c.value1 == 10', undefined, '1,-100');
      checkLength(9, 'c.value1 == 10', undefined, '2,10');
      checkLength(9, 'c.value1 == 10', undefined, '2,11');

      checkLength(1, 'c.value1 == 10', undefined, '10,10');
      checkLength(1, 'c.value1 == 10', undefined, '10,-10');
      checkLength(0, 'c.value1 == 10', undefined, '100,1');
      checkLength(0, 'c.value1 == 10', undefined, '100,10');
      checkLength(0, 'c.value1 == 10', undefined, '100,100');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for some prefab queries matches 
////////////////////////////////////////////////////////////////////////////////

    testMixedQueries : function () {
      checkLength(175,'c.value1 != 0', 'c.value2 < 11');
      checkLength(0,'c.value1 < 0', 'c.value2 > 14');
      checkLength(0,'c.value1 < 0', 'c.value2 >= 19');
      checkLength(1,'c.value1 == 0', 'c.value2 != 2');
      checkLength(0,'c.value1 < 0', 'c.value2 == 3');
      checkLength(0,'c.value1 <= 0', 'c.value2 > 4');
      checkLength(111,'c.value1 >= 0', 'c.value2 < 6');
      checkLength(7,'c.value1 > 10', 'c.value2 == 14');
      checkLength(11,'c.value1 == 10', 'c.value2 != 19');
      checkLength(3,'c.value1 == 10', 'c.value2 <= 2');
      checkLength(12,'c.value1 != 10', 'c.value2 == 8');
      checkLength(50,'c.value1 >= 11', 'c.value2 < 5');
      checkLength(132,'c.value1 > 12', 'c.value2 != 12');
      checkLength(0,'c.value1 <= 12', 'c.value2 > 18');
      checkLength(204,'c.value1 != 12', 'c.value2 != 6');
      checkLength(91,'c.value1 > 13', 'c.value2 <= 12');
      checkLength(0,'c.value1 == 13', 'c.value2 >= 17');
      checkLength(39,'c.value1 <= 13', 'c.value2 < 3');
      checkLength(21,'c.value1 < 13', 'c.value2 >= 7');
      checkLength(15,'c.value1 == 14', 'c.value2 != 17');
      checkLength(3,'c.value1 > 14', 'c.value2 == 18');
      checkLength(1,'c.value1 != 14', 'c.value2 >= 20');
      checkLength(119,'c.value1 >= 14', 'c.value2 != 6');
      checkLength(15,'c.value1 == 15', 'c.value2 != 3');
      checkLength(45,'c.value1 <= 15', 'c.value2 > 6');
      checkLength(7,'c.value1 == 15', 'c.value2 < 7');
      checkLength(99,'c.value1 < 15', 'c.value2 <= 8');
      checkLength(54,'c.value1 >= 15', 'c.value2 <= 8');
      checkLength(152,'c.value1 < 17', 'c.value2 != 16');
      checkLength(6,'c.value1 >= 17', 'c.value2 > 17');
      checkLength(33,'c.value1 < 17', 'c.value2 < 2');
      checkLength(205,'c.value1 != 18', 'c.value2 != 13');
      checkLength(171,'c.value1 < 18', 'c.value2 != 20');
      checkLength(161,'c.value1 < 18', 'c.value2 != 8');
      checkLength(2,'c.value1 >= 19', 'c.value2 == 18');
      checkLength(178,'c.value1 < 19', 'c.value2 != 7');
      checkLength(7,'c.value1 > 1', 'c.value2 == 14');
      checkLength(213,'c.value1 > 1', 'c.value2 <= 15');
      checkLength(1,'c.value1 > 1', 'c.value2 == 20');
      checkLength(55,'c.value1 <= 20', 'c.value2 > 10');
      checkLength(5,'c.value1 >= 20', 'c.value2 >= 16');
      checkLength(9,'c.value1 == 20', 'c.value2 <= 8');
      checkLength(10,'c.value1 >= 20', 'c.value2 <= 9');
      checkLength(3,'c.value1 > 2', 'c.value2 == 18');
      checkLength(10,'c.value1 <= 3', 'c.value2 <= 15');
      checkLength(6,'c.value1 < 3', 'c.value2 != 4');
      checkLength(17,'c.value1 != 3', 'c.value2 == 4');
      checkLength(208,'c.value1 > 3', 'c.value2 != 8');
      checkLength(188,'c.value1 > 4', 'c.value2 <= 13');
      checkLength(101,'c.value1 >= 4', 'c.value2 <= 5');
      checkLength(0,'c.value1 == 4', 'c.value2 > 7');
      checkLength(205,'c.value1 != 5', 'c.value2 >= 1');
      checkLength(150,'c.value1 >= 5', 'c.value2 < 10');
      checkLength(0,'c.value1 <= 5', 'c.value2 == 18');
      checkLength(215,'c.value1 >= 5', 'c.value2 != 20');
      checkLength(15,'c.value1 > 5', 'c.value2 == 3');
      checkLength(202,'c.value1 >= 6', 'c.value2 != 13');
      checkLength(3,'c.value1 >= 6', 'c.value2 >= 19');
      checkLength(150,'c.value1 >= 6', 'c.value2 > 3');
      checkLength(7,'c.value1 == 7', 'c.value2 > 0');
      checkLength(55,'c.value1 >= 7', 'c.value2 >= 11');
      checkLength(55,'c.value1 != 7', 'c.value2 >= 11');
      checkLength(0,'c.value1 <= 7', 'c.value2 == 16');
      checkLength(0,'c.value1 == 7', 'c.value2 == 20');
      checkLength(203,'c.value1 >= 7', 'c.value2 <= 20');
      checkLength(36,'c.value1 <= 7', 'c.value2 <= 7');
      checkLength(174,'c.value1 > 8', 'c.value2 != 1');
      checkLength(167,'c.value1 != 8', 'c.value2 < 11');
      checkLength(0,'c.value1 <= 8', 'c.value2 == 20');
      checkLength(0,'c.value1 == 8', 'c.value2 >= 9');
      checkLength(55,'c.value1 <= 9', 'c.value2 <= 18');
      checkLength(48,'c.value1 >= 9', 'c.value2 <= 3');
      checkLength(12,'c.value1 >= 9', 'c.value2 == 5');
      checkLength(2,'c.value1 <= 9', 'c.value2 == 8');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check query result values
////////////////////////////////////////////////////////////////////////////////

    testIdemValues1 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value: c.value1 }", new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(i, results[j]["value"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check query result values
////////////////////////////////////////////////////////////////////////////////

    testIdemValues2 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value: c.value2 }", new Array("c.value2 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(i, results[j]["value"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check query result values
////////////////////////////////////////////////////////////////////////////////

    testIdemValues3 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value1: c.value1, value2: c.value2 }", new Array("c.value1 == " + i, "c.value2 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(i, results[j]["value1"]);
          assertEqual(i, results[j]["value2"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check computed query result values
////////////////////////////////////////////////////////////////////////////////

    testValueTypes1 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value1: c.value1, value2: c.value1 - 1, value3: c.value1 + 1, value4: null, value5: concat('der fux' , 'xx') }", new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(i, results[j]["value1"]);
          assertEqual(i - 1, results[j]["value2"]);
          assertEqual(i + 1, results[j]["value3"]);
          assertEqual(null, results[j]["value4"]);
          assertEqual("der fuxxx", results[j]["value5"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check computed const result values
////////////////////////////////////////////////////////////////////////////////

    testValueTypes2 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value1: null, value2: null + null, value3: undefined, value4: [], value5: { }, value6: 0, value7: 0.0 }", new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(null, results[j]["value1"]);
          assertEqual(undefined, results[j]["value2"]);
          assertEqual(undefined, results[j]["value3"]);
          assertEqual([], results[j]["value4"]);
          assertEqual({}, results[j]["value5"]);
          assertEqual(0, results[j]["value6"]);
          assertEqual(0.0, results[j]["value7"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check numeric result values
////////////////////////////////////////////////////////////////////////////////

    testNumericValues1 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value1: 0, value2: -0, value3: +0, value4: 0 + 0, value5: 0 - 0, value6: 0 * 0 }", new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(0, results[j]["value1"]);
          assertEqual(0, results[j]["value2"]);
          assertEqual(0, results[j]["value3"]);
          assertEqual(0, results[j]["value4"]);
          assertEqual(0, results[j]["value5"]);
          assertEqual(0, results[j]["value6"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check numeric result values
////////////////////////////////////////////////////////////////////////////////

    testNumericValues2 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value1: 1, value2: -1, value3: +1, value4: -1 + 2, value5: 2 - 1, value6: 1 * 1 }", new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(1, results[j]["value1"]);
          assertEqual(-1, results[j]["value2"]);
          assertEqual(1, results[j]["value3"]);
          assertEqual(1, results[j]["value4"]);
          assertEqual(1, results[j]["value5"]);
          assertEqual(1, results[j]["value6"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check numeric result values
////////////////////////////////////////////////////////////////////////////////

    testNumericValues3 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value1: 1.0, value2: -1.0, value3: +1.0, value4: -1.0 + 2.0, value5: 2.0 - 1.0, value6: 1.0 * 1.0 }", new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(1, results[j]["value1"]);
          assertEqual(-1, results[j]["value2"]);
          assertEqual(1, results[j]["value3"]);
          assertEqual(1, results[j]["value4"]);
          assertEqual(1, results[j]["value5"]);
          assertEqual(1, results[j]["value6"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check numeric result values
////////////////////////////////////////////////////////////////////////////////

    testNumericValues4 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value1: 42.0 - 10, value2: -15 + 37.5, value3: +11.4 - 18.3, value4: -15.3 - 16.5, value5: 14.0 - 18.5, value6: 14.2 * -13.5, value7: -14.05 * -14.15 }", new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(32, results[j]["value1"]);
          assertEqual(22.5, results[j]["value2"]);
          assertEqual(-6.9, results[j]["value3"]);
          assertEqual(-31.8, results[j]["value4"]);
          assertEqual(-4.5, results[j]["value5"]);
          assertEqual(-191.7, results[j]["value6"]);
          assertEqual(198.8075, results[j]["value7"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check string result values
////////////////////////////////////////////////////////////////////////////////

    testStringValues1 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery("{ value1: '', value2: ' ', value3: '   ', value4: '\\'', value5: '\n', value6: '\\\\ \n\\'\\n\\\\\\'' }", new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual("", results[j]["value1"]);
          assertEqual(" ", results[j]["value2"]);
          assertEqual("   ", results[j]["value3"]);
          assertEqual("\'", results[j]["value4"]);
          assertEqual("\n", results[j]["value5"]);
          assertEqual("\\ \n\'\n\\\'", results[j]["value6"]);
        }
      }
    },

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief check string result values
  ////////////////////////////////////////////////////////////////////////////////

    testStringValues2 : function () {
      for (var i = 0; i <= 20; i++) {
        var results = runQuery('{ value1: "", value2: " ", value3: "   ", value4: "\\"", value5: "\n", value6: "\\\\ \n\\"\\n\\\\\\"" }', new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual("", results[j]["value1"]);
          assertEqual(" ", results[j]["value2"]);
          assertEqual("   ", results[j]["value3"]);
          assertEqual("\"", results[j]["value4"]);
          assertEqual("\n", results[j]["value5"]);
          assertEqual("\\ \n\"\n\\\"", results[j]["value6"]);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check document result values
////////////////////////////////////////////////////////////////////////////////

    testDocumentValues : function () {
      var expected = { "value1" : { "value11" : { "value111" : "", "value112" : 4 }, "value12" : "bang" }, "value2" : -5, "value3": { } };
      for (var i = 0; i <= 20; i++) {
        var results = runQuery(JSON.stringify(expected), new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(expected, results[j]); 
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check array result values
////////////////////////////////////////////////////////////////////////////////

    testArrayValues : function () {
      var expected = { "v" : [ 1, 2, 5, 99, { }, [ "value1", "value99", [ { "value5" : 5, "null" : null }, [ 1, -99, 0 ], "peng" ], { "x": 9 } ], { "aha" : 55 }, [ 0 ], [ ] ] };
      for (var i = 0; i <= 20; i++) {
        var results = runQuery(JSON.stringify(expected), new Array("c.value1 == " + i));
        for (var j = 0; j < results.length; j++) {
          assertEqual(expected, results[j]); 
        }
      }
    },

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlSimpleTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
