////////////////////////////////////////////////////////////////////////////////
/// @brief tests for bind parameters
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

function aqlBindParametersTestSuite () {
  var collection = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  function setUp () {
    this.collection = db.UnitTestsBind;

    if (this.collection.count() == 0) {
      for (var i = 0; i <= 10; i++) {
        for (var j = 0; j <= 10; j++) {
          this.collection.save({ "value1" : i, "value2" : j, "value3" : "test", "value4" : null, "value5" : -13.5});
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

  function executeQuery (query, bindParameters) {
    var cursor = AQL_STATEMENT(query, bindParameters);
    assertFalse(cursor instanceof AvocadoQueryError);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, bindParameters) {
    var cursor = this.executeQuery(query, bindParameters);
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
/// @brief check length of result set for null bind parameter
////////////////////////////////////////////////////////////////////////////////

  function executeRsLengthCheckNull (expectedLength, value) {
    var params = { "7" : value };
    var result = this.getQueryResults("SELECT { value: @7 } FROM " + this.collection._name + " c " +
                                      "WHERE c.value4 == @7", params);

    assertEqual(expectedLength, result.length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check length of result set for string bind parameter
////////////////////////////////////////////////////////////////////////////////

  function executeRsLengthCheckString (expectedLength, value) {
    var params = { "0" : value };
    var result = this.getQueryResults("SELECT { value: @0 } FROM " + this.collection._name + " c " +
                                      "WHERE c.value3 == @0", params);

    assertEqual(expectedLength, result.length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check length of result set for 1 bind parameter
////////////////////////////////////////////////////////////////////////////////

  function executeRsLengthCheck (expectedLength, value) {
    var params = { "value1" : value };
    var result = this.getQueryResults("SELECT { value: @value1@ } FROM " + this.collection._name + " c " +
                                      "WHERE c.value1 == @value1@", params);

    assertEqual(expectedLength, result.length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check length of result set for 2 bind parameters 
////////////////////////////////////////////////////////////////////////////////

  function executeRsLengthCheck2 (expectedLength, value1, value2) {
    var params = { "value1" : value1, "value2" : value2 };
    var result = this.getQueryResults("SELECT { value: @value1@ } FROM " + this.collection._name + " c " +
                                      "WHERE c.value1 == @value1@ && c.value2 == @value2@", params);

    assertEqual(expectedLength, result.length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check result sets 
////////////////////////////////////////////////////////////////////////////////

  function testRsLengths1 () {
    this.executeRsLengthCheck(11, 0);
    this.executeRsLengthCheck(11, 1);
    this.executeRsLengthCheck(11, 2);
    this.executeRsLengthCheck(11, 9);
    this.executeRsLengthCheck(11, 10);

    this.executeRsLengthCheck(0, -1);
    this.executeRsLengthCheck(0, -5);
    this.executeRsLengthCheck(0, -10);
    this.executeRsLengthCheck(0, -11);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check result sets 
////////////////////////////////////////////////////////////////////////////////
  
  function testRsLengths2 () {
    this.executeRsLengthCheck2(1, 0, 0);
    this.executeRsLengthCheck2(1, 0, 1);
    this.executeRsLengthCheck2(1, 1, 0);
    this.executeRsLengthCheck2(1, 2, 0);
    this.executeRsLengthCheck2(1, 2, 1);
    this.executeRsLengthCheck2(1, 0, 2);
    this.executeRsLengthCheck2(1, 1, 2);
    this.executeRsLengthCheck2(1, 10, 0);
    this.executeRsLengthCheck2(1, 10, 1);
    this.executeRsLengthCheck2(1, 0, 10);
    this.executeRsLengthCheck2(1, 1, 10);
    this.executeRsLengthCheck2(1, 10, 10);

    this.executeRsLengthCheck2(0, 11, 0);
    this.executeRsLengthCheck2(0, 11, 1);
    this.executeRsLengthCheck2(0, 0, 11);
    this.executeRsLengthCheck2(0, 1, 11);
    this.executeRsLengthCheck2(0, -1, 0);
    this.executeRsLengthCheck2(0, 1, -1);
    this.executeRsLengthCheck2(0, 10, -1);
    this.executeRsLengthCheck2(0, 11, -1);
    this.executeRsLengthCheck2(0, -10, 1);
    this.executeRsLengthCheck2(0, -10, 10);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check result sets 
////////////////////////////////////////////////////////////////////////////////
          
  function testRsLengthsString () {
    this.executeRsLengthCheckString(121, "test");

    this.executeRsLengthCheckString(0, " test");
    this.executeRsLengthCheckString(0, " test ");
    this.executeRsLengthCheckString(0, "test ");
    this.executeRsLengthCheckString(0, "test ");
    this.executeRsLengthCheckString(0, "rest");
    this.executeRsLengthCheckString(0, 15);
    this.executeRsLengthCheckString(0, 1);
    this.executeRsLengthCheckString(0, -15);
    this.executeRsLengthCheckString(0, null);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check result sets 
////////////////////////////////////////////////////////////////////////////////
          
  function testRsLengthsNull () {
    this.executeRsLengthCheckNull(0, "test");
    this.executeRsLengthCheckNull(0, "1");
    this.executeRsLengthCheckNull(0, 1);
    this.executeRsLengthCheckNull(0, 2);
  }

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsUnity.run(aqlBindParametersTestSuite);

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
