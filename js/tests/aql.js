////////////////////////////////////////////////////////////////////////////////
/// @brief tests for "aql.js"
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

function aqlTestSuite () {
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
    var aQuery = AQL_PREPARE(db, query);
    if (aQuery) {
      return aQuery.execute();
    }
    return aQuery;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query) {
    var aCursor = this.executeQuery(query);
    if (aCursor) {
      var results = [ ];
      while (aCursor.hasNext()) {
        results.push(aCursor.next());
      }
      return results;
    }
    return aCursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief assemble a query string from the parameters given
////////////////////////////////////////////////////////////////////////////////

  function assembleQuery (value1, value2, limit) {
    var query = 'SELECT { } FROM ' + this.collection._name + ' c WHERE ';
    var cond = '';

    if (value1 !== undefined) {
      cond += value1;
    }
    if (value2 !== undefined) {
      if (cond != '') {
        cond += ' && ';
      }
      cond += value2;
    }
    query += cond;

    if (limit !== undefined) {
      query += ' LIMIT ' + limit;
    }
    print("Q: " + query);

    return query;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check the length of a query result set
////////////////////////////////////////////////////////////////////////////////

  function checkLength (expected, value1, value2, limit) {
    var query = this.assembleQuery(value1, value2, limit);
    var result = this.getQueryResults(query);
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

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(aqlTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
