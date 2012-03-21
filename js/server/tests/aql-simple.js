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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlSimpleTestSuite () {
  var collection = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  function setUp () {
    this.collection = db.UnitTestsNumbersList;

    if (this.collection.count() == 0) {
      for (var i = 0; i <= 20; i++) {
        for (var j = 0; j <= i; j++) {
          this.collection.save({ value1: i, value2: j, value3: i * j});
        }
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  function tearDown () {
  }

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
    var cursor = this.executeQuery(query);
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
    var query = this.assembleQuery(select, wheres, limit, order);
    var result = this.getQueryResults(query);
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

    var query = "SELECT " + selectClause + " FROM " + this.collection._name + " c " + 
                whereClause + orderClause + limitClause;

    return query;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check the length of a query result set
////////////////////////////////////////////////////////////////////////////////

  function checkLength (expected, value1, value2, limit) {
    var result = this.runQuery("{ }", new Array(value1, value2), limit);
    assertTrue(result instanceof Array);
    var actual = result.length;
    assertEqual(expected, actual);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for a simple eq condition matches
////////////////////////////////////////////////////////////////////////////////

  function testResultLengthsEqIndividual1 () {
    for (i = 0; i <= 20; i++) {
      this.checkLength(i + 1, 'c.value1 == ' + i);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for a simple eq condition matches
////////////////////////////////////////////////////////////////////////////////

  function testResultLengthsEqIndividual2 () {
    for (i = 0; i <= 20; i++) {
      this.checkLength(21 - i, undefined, 'c.value2 == ' + i);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for combined eq conditions match
////////////////////////////////////////////////////////////////////////////////

  function testResultLengthsEqCombined () {
    for (i = 0; i <= 20; i++) {
      for (j = 0; j <= i; j++) {
        this.checkLength(1, 'c.value1 == ' + i, 'c.value2 == ' + j);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for range queries matches
////////////////////////////////////////////////////////////////////////////////
  
  function testResultLengthsBetweenIndividual1 () {
    for (i = 0; i <= 20; i++) {
      this.checkLength(i + 1, 'c.value1 >= ' + i, 'c.value1 <= ' + i);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for (senseless) range queries matches
////////////////////////////////////////////////////////////////////////////////
  
  function testResultLengthsBetweenIndividual2 () {
    for (i = 0; i <= 20; i++) {
      this.checkLength(0, 'c.value1 > ' + i, 'c.value1 < ' + i);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for single-range queries matches
////////////////////////////////////////////////////////////////////////////////
  
  function testResultLengthsLessIndividual1 () {
    var expected = 0;
    for (i = 0; i <= 20; i++) {
      for (var j = 0; j <= i; j++, expected++);
      this.checkLength(expected, 'c.value1 <= ' + i);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for range queries matches
////////////////////////////////////////////////////////////////////////////////
  
  function testResultLengthsLessIndividual2 () {
    var expected = 0;
    for (i = 0; i <= 20; i++) {
      expected += 21 - i;
      this.checkLength(expected, 'c.value2 <= ' + i);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for out-of-range queries is 0
////////////////////////////////////////////////////////////////////////////////

  function testNonResultsIndividual1 () {
    this.checkLength(0, 'c.value1 < 0');
    this.checkLength(0, 'c.value1 <= -0.01');
    this.checkLength(0, 'c.value1 < -0.1');
    this.checkLength(0, 'c.value1 <= -0.1');
    this.checkLength(0, 'c.value1 <= -1');
    this.checkLength(0, 'c.value1 <= -1.0');
    this.checkLength(0, 'c.value1 < -10');
    this.checkLength(0, 'c.value1 <= -100');
    
    this.checkLength(0, 'c.value1 == 21');
    this.checkLength(0, 'c.value1 == 110');
    this.checkLength(0, 'c.value1 == 20.001');
    this.checkLength(0, 'c.value1 > 20');
    this.checkLength(0, 'c.value1 >= 20.01');
    this.checkLength(0, 'c.value1 > 100');
    this.checkLength(0, 'c.value1 >= 100');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for out-of-range queries is 0
////////////////////////////////////////////////////////////////////////////////
  
  function testNonResultsIndividual2 () {
    this.checkLength(0, 'c.value2 < 0');
    this.checkLength(0, 'c.value2 <= -0.01');
    this.checkLength(0, 'c.value2 < -0.1');
    this.checkLength(0, 'c.value2 <= -0.1');
    this.checkLength(0, 'c.value2 <= -1');
    this.checkLength(0, 'c.value2 <= -1.0');
    this.checkLength(0, 'c.value2 < -10');
    this.checkLength(0, 'c.value2 <= -100');
    
    this.checkLength(0, 'c.value2 == 21');
    this.checkLength(0, 'c.value2 === 21');
    this.checkLength(0, 'c.value2 == 110');
    this.checkLength(0, 'c.value2 == 20.001');
    this.checkLength(0, 'c.value2 > 20');
    this.checkLength(0, 'c.value2 >= 20.01');
    this.checkLength(0, 'c.value2 > 100');
    this.checkLength(0, 'c.value2 >= 100');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for out-of-range queries is 0
////////////////////////////////////////////////////////////////////////////////
  
  function testNonResultsCombined () {
    this.checkLength(0, 'c.value1 > 0', 'c.value2 > 20');
    this.checkLength(0, 'c.value1 >= 0', 'c.value2 > 20');
    this.checkLength(0, 'c.value1 < 0', 'c.value2 > 0');
    this.checkLength(0, 'c.value1 < 0', 'c.value2 >= 0');

    this.checkLength(0, 'c.value1 == 21', 'c.value2 == 0');
    this.checkLength(0, 'c.value1 == 21', 'c.value2 == 1');
    this.checkLength(0, 'c.value1 == 21', 'c.value2 >= 0');
    this.checkLength(0, 'c.value1 == 1', 'c.value2 == 21');
    this.checkLength(0, 'c.value1 == 0', 'c.value2 == 21');
    this.checkLength(0, 'c.value1 >= 0', 'c.value2 == 21');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for (illogical) ranges is 0
////////////////////////////////////////////////////////////////////////////////
  
  function testNonResultsIllogical () { 
    this.checkLength(0, 'c.value1 > 0', 'c.value1 > 20');
    this.checkLength(0, 'c.value1 >= 0', 'c.value1 > 20');
    this.checkLength(0, 'c.value1 == 0', 'c.value1 > 20');
    this.checkLength(0, 'c.value1 === 0', 'c.value1 > 20');
    this.checkLength(0, 'c.value1 !== 0', 'c.value1 > 20');
    this.checkLength(0, 'c.value1 != 0', 'c.value1 > 20');
    this.checkLength(0, 'c.value1 > 20', 'c.value1 > 0');
    this.checkLength(0, 'c.value1 > 20', 'c.value1 >= 0');
    this.checkLength(0, 'c.value1 >= 21', 'c.value1 > 0');
    this.checkLength(0, 'c.value1 >= 21', 'c.value1 >= 0');
    this.checkLength(0, 'c.value1 >= 21', 'c.value1 == 0');
    this.checkLength(0, 'c.value1 >= 21', 'c.value1 === 0');
    this.checkLength(0, 'c.value1 >= 21', 'c.value1 !== 0');
    this.checkLength(0, 'c.value1 >= 21', 'c.value1 != 0');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries matches
////////////////////////////////////////////////////////////////////////////////

  function testLimitNonOffset1 () { 
    this.checkLength(0, 'c.value1 == 0', undefined, '0');
    this.checkLength(0, 'c.value1 == 0', undefined, '0,0');

    this.checkLength(1, 'c.value1 == 0', undefined, '1');
    this.checkLength(1, 'c.value1 == 0', undefined, '2');
    this.checkLength(1, 'c.value1 == 0', undefined, '3');

    this.checkLength(1, 'c.value1 == 0', undefined, '0,1');
    this.checkLength(1, 'c.value1 == 0', undefined, '0,2');
    this.checkLength(1, 'c.value1 == 0', undefined, '0,3');

    this.checkLength(1, 'c.value1 == 0', undefined, '0,-1');
    this.checkLength(1, 'c.value1 == 0', undefined, '0,-2');
    this.checkLength(1, 'c.value1 == 0', undefined, '0,-3');

    this.checkLength(1, 'c.value1 == 0', undefined, '-1');
    this.checkLength(1, 'c.value1 == 0', undefined, '-2');
    this.checkLength(1, 'c.value1 == 0', undefined, '-3');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries w/ offset matches
////////////////////////////////////////////////////////////////////////////////

  function testLimitOffset1 () {
    this.checkLength(0, 'c.value1 == 0', undefined, '1,1');
    this.checkLength(0, 'c.value1 == 0', undefined, '1,-1');
    this.checkLength(0, 'c.value1 == 0', undefined, '1,0');
    
    this.checkLength(0, 'c.value1 == 0', undefined, '1,2');
    this.checkLength(0, 'c.value1 == 0', undefined, '1,-2');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries matches
////////////////////////////////////////////////////////////////////////////////
  
  function testLimitNonOffset2 () {
    this.checkLength(0, 'c.value1 == 3', undefined, '0');
    this.checkLength(0, 'c.value1 == 3', undefined, '0,0');

    this.checkLength(1, 'c.value1 == 3', undefined, '1');
    this.checkLength(2, 'c.value1 == 3', undefined, '2');
    this.checkLength(3, 'c.value1 == 3', undefined, '3');

    this.checkLength(1, 'c.value1 == 3', undefined, '0,1');
    this.checkLength(2, 'c.value1 == 3', undefined, '0,2');
    this.checkLength(3, 'c.value1 == 3', undefined, '0,3');

    this.checkLength(1, 'c.value1 == 3', undefined, '0,-1');
    this.checkLength(2, 'c.value1 == 3', undefined, '0,-2');
    this.checkLength(3, 'c.value1 == 3', undefined, '0,-3');

    this.checkLength(1, 'c.value1 == 3', undefined, '-1');
    this.checkLength(2, 'c.value1 == 3', undefined, '-2');
    this.checkLength(3, 'c.value1 == 3', undefined, '-3');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries w/ offset matches
////////////////////////////////////////////////////////////////////////////////
  
  function testLimitOffset2 () {
    this.checkLength(1, 'c.value1 == 3', undefined, '1,1');
    this.checkLength(1, 'c.value1 == 3', undefined, '1,-1');
    this.checkLength(0, 'c.value1 == 3', undefined, '1,0');
    
    this.checkLength(2, 'c.value1 == 3', undefined, '1,2');
    this.checkLength(3, 'c.value1 == 3', undefined, '1,3');
    this.checkLength(2, 'c.value1 == 3', undefined, '1,-2');
    this.checkLength(3, 'c.value1 == 3', undefined, '1,-3');

    this.checkLength(2, 'c.value1 == 3', undefined, '2,2');
    this.checkLength(2, 'c.value1 == 3', undefined, '2,3');
    this.checkLength(2, 'c.value1 == 3', undefined, '2,-2');
    this.checkLength(2, 'c.value1 == 3', undefined, '2,-3');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries matches
////////////////////////////////////////////////////////////////////////////////
  
  function testLimitNonOffset3 () {
    this.checkLength(0, 'c.value1 == 10', undefined, '0');
    this.checkLength(10, 'c.value1 == 10', undefined, '10');
    this.checkLength(10, 'c.value1 == 10', undefined, '0,10');
    this.checkLength(11, 'c.value1 == 10', undefined, '0,11');
    this.checkLength(11, 'c.value1 == 10', undefined, '0,12');
    this.checkLength(11, 'c.value1 == 10', undefined, '0,100');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for limit queries w/ offset matches
////////////////////////////////////////////////////////////////////////////////

  function testLimitOffset3 () {
    this.checkLength(10, 'c.value1 == 10', undefined, '1,10');
    this.checkLength(10, 'c.value1 == 10', undefined, '1,100');
    this.checkLength(10, 'c.value1 == 10', undefined, '1,-10');
    this.checkLength(10, 'c.value1 == 10', undefined, '1,-100');
    this.checkLength(9, 'c.value1 == 10', undefined, '2,10');
    this.checkLength(9, 'c.value1 == 10', undefined, '2,11');

    this.checkLength(1, 'c.value1 == 10', undefined, '10,10');
    this.checkLength(1, 'c.value1 == 10', undefined, '10,-10');
    this.checkLength(0, 'c.value1 == 10', undefined, '100,1');
    this.checkLength(0, 'c.value1 == 10', undefined, '100,10');
    this.checkLength(0, 'c.value1 == 10', undefined, '100,100');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test if the number of results for some prefab queries matches 
////////////////////////////////////////////////////////////////////////////////

  function testMixedQueries () {
    this.checkLength(175,'c.value1 != 0', 'c.value2 < 11');
    this.checkLength(0,'c.value1 < 0', 'c.value2 > 14');
    this.checkLength(0,'c.value1 === 0', 'c.value2 === 18');
    this.checkLength(0,'c.value1 < 0', 'c.value2 >= 19');
    this.checkLength(1,'c.value1 == 0', 'c.value2 != 2');
    this.checkLength(0,'c.value1 < 0', 'c.value2 == 3');
    this.checkLength(0,'c.value1 <= 0', 'c.value2 > 4');
    this.checkLength(111,'c.value1 >= 0', 'c.value2 < 6');
    this.checkLength(7,'c.value1 > 10', 'c.value2 == 14');
    this.checkLength(7,'c.value1 != 10', 'c.value2 === 14');
    this.checkLength(11,'c.value1 == 10', 'c.value2 != 19');
    this.checkLength(3,'c.value1 == 10', 'c.value2 <= 2');
    this.checkLength(3,'c.value1 < 10', 'c.value2 === 7');
    this.checkLength(12,'c.value1 != 10', 'c.value2 == 8');
    this.checkLength(50,'c.value1 >= 11', 'c.value2 < 5');
    this.checkLength(6,'c.value1 === 11', 'c.value2 < 6');
    this.checkLength(10,'c.value1 != 12', 'c.value2 === 10');
    this.checkLength(132,'c.value1 > 12', 'c.value2 != 12');
    this.checkLength(13,'c.value1 === 12', 'c.value2 != 14');
    this.checkLength(0,'c.value1 === 12', 'c.value2 == 15');
    this.checkLength(0,'c.value1 <= 12', 'c.value2 > 18');
    this.checkLength(204,'c.value1 != 12', 'c.value2 != 6');
    this.checkLength(91,'c.value1 > 13', 'c.value2 <= 12');
    this.checkLength(0,'c.value1 == 13', 'c.value2 >= 17');
    this.checkLength(39,'c.value1 <= 13', 'c.value2 < 3');
    this.checkLength(21,'c.value1 < 13', 'c.value2 >= 7');
    this.checkLength(15,'c.value1 == 14', 'c.value2 != 17');
    this.checkLength(3,'c.value1 > 14', 'c.value2 == 18');
    this.checkLength(1,'c.value1 != 14', 'c.value2 >= 20');
    this.checkLength(119,'c.value1 >= 14', 'c.value2 != 6');
    this.checkLength(15,'c.value1 == 15', 'c.value2 != 3');
    this.checkLength(45,'c.value1 <= 15', 'c.value2 > 6');
    this.checkLength(7,'c.value1 == 15', 'c.value2 < 7');
    this.checkLength(99,'c.value1 < 15', 'c.value2 <= 8');
    this.checkLength(54,'c.value1 >= 15', 'c.value2 <= 8');
    this.checkLength(1,'c.value1 === 16', 'c.value2 == 3');
    this.checkLength(16,'c.value1 === 16', 'c.value2 != 6');
    this.checkLength(152,'c.value1 < 17', 'c.value2 != 16');
    this.checkLength(6,'c.value1 >= 17', 'c.value2 > 17');
    this.checkLength(16,'c.value1 <= 17', 'c.value2 === 2');
    this.checkLength(33,'c.value1 < 17', 'c.value2 < 2');
    this.checkLength(9,'c.value1 <= 18', 'c.value2 === 10');
    this.checkLength(205,'c.value1 != 18', 'c.value2 != 13');
    this.checkLength(171,'c.value1 < 18', 'c.value2 != 20');
    this.checkLength(0,'c.value1 === 18', 'c.value2 > 20');
    this.checkLength(161,'c.value1 < 18', 'c.value2 != 8');
    this.checkLength(2,'c.value1 > 18', 'c.value2 === 8');
    this.checkLength(1,'c.value1 == 19', 'c.value2 === 15');
    this.checkLength(2,'c.value1 >= 19', 'c.value2 == 18');
    this.checkLength(178,'c.value1 < 19', 'c.value2 != 7');
    this.checkLength(7,'c.value1 > 1', 'c.value2 == 14');
    this.checkLength(213,'c.value1 > 1', 'c.value2 <= 15');
    this.checkLength(1,'c.value1 > 1', 'c.value2 == 20');
    this.checkLength(0,'c.value1 == 1', 'c.value2 === 5');
    this.checkLength(55,'c.value1 <= 20', 'c.value2 > 10');
    this.checkLength(5,'c.value1 >= 20', 'c.value2 >= 16');
    this.checkLength(0,'c.value1 != 20', 'c.value2 === 20');
    this.checkLength(9,'c.value1 == 20', 'c.value2 <= 8');
    this.checkLength(10,'c.value1 >= 20', 'c.value2 <= 9');
    this.checkLength(3,'c.value1 > 2', 'c.value2 == 18');
    this.checkLength(10,'c.value1 <= 3', 'c.value2 <= 15');
    this.checkLength(6,'c.value1 < 3', 'c.value2 != 4');
    this.checkLength(17,'c.value1 != 3', 'c.value2 == 4');
    this.checkLength(208,'c.value1 > 3', 'c.value2 != 8');
    this.checkLength(188,'c.value1 > 4', 'c.value2 <= 13');
    this.checkLength(0,'c.value1 < 4', 'c.value2 === 4');
    this.checkLength(101,'c.value1 >= 4', 'c.value2 <= 5');
    this.checkLength(0,'c.value1 == 4', 'c.value2 > 7');
    this.checkLength(205,'c.value1 != 5', 'c.value2 >= 1');
    this.checkLength(150,'c.value1 >= 5', 'c.value2 < 10');
    this.checkLength(6,'c.value1 === 5', 'c.value2 < 14');
    this.checkLength(4,'c.value1 >= 5', 'c.value2 === 17');
    this.checkLength(4,'c.value1 > 5', 'c.value2 === 17');
    this.checkLength(0,'c.value1 <= 5', 'c.value2 == 18');
    this.checkLength(1,'c.value1 === 5', 'c.value2 == 2');
    this.checkLength(215,'c.value1 >= 5', 'c.value2 != 20');
    this.checkLength(15,'c.value1 > 5', 'c.value2 == 3');
    this.checkLength(202,'c.value1 >= 6', 'c.value2 != 13');
    this.checkLength(3,'c.value1 >= 6', 'c.value2 >= 19');
    this.checkLength(150,'c.value1 >= 6', 'c.value2 > 3');
    this.checkLength(7,'c.value1 == 7', 'c.value2 > 0');
    this.checkLength(55,'c.value1 >= 7', 'c.value2 >= 11');
    this.checkLength(55,'c.value1 != 7', 'c.value2 >= 11');
    this.checkLength(0,'c.value1 <= 7', 'c.value2 == 16');
    this.checkLength(0,'c.value1 == 7', 'c.value2 == 20');
    this.checkLength(203,'c.value1 >= 7', 'c.value2 <= 20');
    this.checkLength(5,'c.value1 <= 7', 'c.value2 === 3');
    this.checkLength(14,'c.value1 >= 7', 'c.value2 === 5');
    this.checkLength(36,'c.value1 <= 7', 'c.value2 <= 7');
    this.checkLength(174,'c.value1 > 8', 'c.value2 != 1');
    this.checkLength(9,'c.value1 === 8', 'c.value2 != 10');
    this.checkLength(167,'c.value1 != 8', 'c.value2 < 11');
    this.checkLength(9,'c.value1 === 8', 'c.value2 < 14');
    this.checkLength(0,'c.value1 <= 8', 'c.value2 == 20');
    this.checkLength(0,'c.value1 == 8', 'c.value2 >= 9');
    this.checkLength(4,'c.value1 >= 9', 'c.value2 === 17');
    this.checkLength(55,'c.value1 <= 9', 'c.value2 <= 18');
    this.checkLength(48,'c.value1 >= 9', 'c.value2 <= 3');
    this.checkLength(12,'c.value1 >= 9', 'c.value2 == 5');
    this.checkLength(2,'c.value1 <= 9', 'c.value2 == 8');
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check query result values
////////////////////////////////////////////////////////////////////////////////

  function testIdemValues1 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value: c.value1 }", new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(i, results[j]["value"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check query result values
////////////////////////////////////////////////////////////////////////////////

  function testIdemValues2 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value: c.value2 }", new Array("c.value2 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(i, results[j]["value"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check query result values
////////////////////////////////////////////////////////////////////////////////

  function testIdemValues3 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value1: c.value1, value2: c.value2 }", new Array("c.value1 == " + i, "c.value2 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(i, results[j]["value1"]);
        assertEqual(i, results[j]["value2"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check query result values
////////////////////////////////////////////////////////////////////////////////

  function testIdemValues3 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value1: c.value1, value2: c.value2 }", new Array("c.value1 == " + i, "c.value2 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(i, results[j]["value1"]);
        assertEqual(i, results[j]["value2"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check computed query result values
////////////////////////////////////////////////////////////////////////////////

  function testValueTypes1 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value1: c.value1, value2: c.value1 - 1, value3: c.value1 + 1, value4: null, value5: 'der fux' + 'xx' }", new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(i, results[j]["value1"]);
        assertEqual(i - 1, results[j]["value2"]);
        assertEqual(i + 1, results[j]["value3"]);
        assertEqual(null, results[j]["value4"]);
        assertEqual("der fuxxx", results[j]["value5"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check computed const result values
////////////////////////////////////////////////////////////////////////////////

  function testValueTypes2 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value1: null, value2: null + null, value3: undefined, value4: [], value5: { }, value6: 0, value7: 0.0 }", new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(null, results[j]["value1"]);
        assertEqual(0, results[j]["value2"]);
        assertEqual(undefined, results[j]["value3"]);
        assertEqual([], results[j]["value4"]);
        assertEqual({}, results[j]["value5"]);
        assertEqual(0, results[j]["value6"]);
        assertEqual(0.0, results[j]["value7"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check numeric result values
////////////////////////////////////////////////////////////////////////////////

  function testNumericValues1 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value1: 0, value2: -0, value3: +0, value4: 0 + 0, value5: 0 - 0, value6: 0 * 0 }", new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(0, results[j]["value1"]);
        assertEqual(0, results[j]["value2"]);
        assertEqual(0, results[j]["value3"]);
        assertEqual(0, results[j]["value4"]);
        assertEqual(0, results[j]["value5"]);
        assertEqual(0, results[j]["value6"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check numeric result values
////////////////////////////////////////////////////////////////////////////////

  function testNumericValues2 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value1: 1, value2: -1, value3: +1, value4: -1 + 2, value5: 2 - 1, value6: 1 * 1 }", new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(1, results[j]["value1"]);
        assertEqual(-1, results[j]["value2"]);
        assertEqual(1, results[j]["value3"]);
        assertEqual(1, results[j]["value4"]);
        assertEqual(1, results[j]["value5"]);
        assertEqual(1, results[j]["value6"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check numeric result values
////////////////////////////////////////////////////////////////////////////////

  function testNumericValues3 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value1: 1.0, value2: -1.0, value3: +1.0, value4: -1.0 + 2.0, value5: 2.0 - 1.0, value6: 1.0 * 1.0 }", new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(1, results[j]["value1"]);
        assertEqual(-1, results[j]["value2"]);
        assertEqual(1, results[j]["value3"]);
        assertEqual(1, results[j]["value4"]);
        assertEqual(1, results[j]["value5"]);
        assertEqual(1, results[j]["value6"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check numeric result values
////////////////////////////////////////////////////////////////////////////////

  function testNumericValues4 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value1: 42.0 - 10, value2: -15 + 37.5, value3: +11.4 - 18.3, value4: -15.3 - 16.5, value5: 14.0 - 18.5, value6: 14.2 * -13.5, value7: -14.05 * -14.15 }", new Array("c.value1 == " + i));
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
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check string result values
////////////////////////////////////////////////////////////////////////////////

  function testStringValues1 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery("{ value1: '', value2: ' ', value3: '   ', value4: '\\'', value5: '\n', value6: '\\\\ \n\\'\\n\\\\\\'' }", new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual("", results[j]["value1"]);
        assertEqual(" ", results[j]["value2"]);
        assertEqual("   ", results[j]["value3"]);
        assertEqual("\'", results[j]["value4"]);
        assertEqual("\n", results[j]["value5"]);
        assertEqual("\\ \n\'\n\\\'", results[j]["value6"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check string result values
////////////////////////////////////////////////////////////////////////////////

  function testStringValues2 () {
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery('{ value1: "", value2: " ", value3: "   ", value4: "\\"", value5: "\n", value6: "\\\\ \n\\"\\n\\\\\\"" }', new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual("", results[j]["value1"]);
        assertEqual(" ", results[j]["value2"]);
        assertEqual("   ", results[j]["value3"]);
        assertEqual("\"", results[j]["value4"]);
        assertEqual("\n", results[j]["value5"]);
        assertEqual("\\ \n\"\n\\\"", results[j]["value6"]);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check document result values
////////////////////////////////////////////////////////////////////////////////

  function testDocumentValues () {
    var expected = { "value1" : { "value11" : { "value111" : "", "value112" : 4 }, "value12" : "bang" }, "value2" : -5, "value3": { } };
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery(JSON.stringify(expected), new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(expected, results[j]); 
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check array result values
////////////////////////////////////////////////////////////////////////////////

  function testArrayValues () {
    var expected = { "v" : [ 1, 2, 5, 99, { }, [ "value1", "value99", [ { "value5" : 5, "null" : null }, [ 1, -99, 0 ], "peng" ], { "x": 9 } ], { "aha" : 55 }, [ 0 ], [ ] ] };
    for (var i = 0; i <= 20; i++) {
      var results = this.runQuery(JSON.stringify(expected), new Array("c.value1 == " + i));
      for (var j = 0; j < results.length; j++) {
        assertEqual(expected, results[j]); 
      }
    }
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(aqlSimpleTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
