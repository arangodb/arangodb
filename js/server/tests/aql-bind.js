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

var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlBindParametersTestSuite () {
  var collection = null;

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
    var cursor = executeQuery(query, bindParameters);
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
    var result = getQueryResults("SELECT { value: @7 } FROM " + collection.name() + " c " +
                                      "WHERE c.value4 == @7", params);

    assertEqual(expectedLength, result.length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check length of result set for string bind parameter
////////////////////////////////////////////////////////////////////////////////

  function executeRsLengthCheckString (expectedLength, value) {
    var params = { "0" : value };
    var result = getQueryResults("SELECT { value: @0 } FROM " + collection.name() + " c " +
                                      "WHERE c.value3 == @0", params);

    assertEqual(expectedLength, result.length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check length of result set for 1 bind parameter
////////////////////////////////////////////////////////////////////////////////

  function executeRsLengthCheck (expectedLength, value) {
    var params = { "value1" : value };
    var result = getQueryResults("SELECT { value: @value1@ } FROM " + collection.name() + " c " +
                                      "WHERE c.value1 == @value1@", params);

    assertEqual(expectedLength, result.length);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check length of result set for 2 bind parameters 
////////////////////////////////////////////////////////////////////////////////

  function executeRsLengthCheck2 (expectedLength, value1, value2) {
    var params = { "value1" : value1, "value2" : value2 };
    var result = getQueryResults("SELECT { value: @value1@ } FROM " + collection.name() + " c " +
                                      "WHERE c.value1 == @value1@ && c.value2 == @value2@", params);

    assertEqual(expectedLength, result.length);
  }


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      collection = db.UnitTestsBind;

      if (collection.count() == 0) {
        for (var i = 0; i <= 10; i++) {
          for (var j = 0; j <= 10; j++) {
            collection.save({ "value1" : i, "value2" : j, "value3" : "test", "value4" : null, "value5" : -13.5});
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
/// @brief check result sets 
////////////////////////////////////////////////////////////////////////////////

    testRsLengths1 : function () {
      executeRsLengthCheck(11, 0);
      executeRsLengthCheck(11, 1);
      executeRsLengthCheck(11, 2);
      executeRsLengthCheck(11, 9);
      executeRsLengthCheck(11, 10);

      executeRsLengthCheck(0, -1);
      executeRsLengthCheck(0, -5);
      executeRsLengthCheck(0, -10);
      executeRsLengthCheck(0, -11);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check result sets 
////////////////////////////////////////////////////////////////////////////////
  
    testRsLengths2 : function () {
      executeRsLengthCheck2(1, 0, 0);
      executeRsLengthCheck2(1, 0, 1);
      executeRsLengthCheck2(1, 1, 0);
      executeRsLengthCheck2(1, 2, 0);
      executeRsLengthCheck2(1, 2, 1);
      executeRsLengthCheck2(1, 0, 2);
      executeRsLengthCheck2(1, 1, 2);
      executeRsLengthCheck2(1, 10, 0);
      executeRsLengthCheck2(1, 10, 1);
      executeRsLengthCheck2(1, 0, 10);
      executeRsLengthCheck2(1, 1, 10);
      executeRsLengthCheck2(1, 10, 10);

      executeRsLengthCheck2(0, 11, 0);
      executeRsLengthCheck2(0, 11, 1);
      executeRsLengthCheck2(0, 0, 11);
      executeRsLengthCheck2(0, 1, 11);
      executeRsLengthCheck2(0, -1, 0);
      executeRsLengthCheck2(0, 1, -1);
      executeRsLengthCheck2(0, 10, -1);
      executeRsLengthCheck2(0, 11, -1);
      executeRsLengthCheck2(0, -10, 1);
      executeRsLengthCheck2(0, -10, 10);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check result sets 
////////////////////////////////////////////////////////////////////////////////
          
    testRsLengthsString : function () {
      executeRsLengthCheckString(121, "test");

      executeRsLengthCheckString(0, " test");
      executeRsLengthCheckString(0, " test ");
      executeRsLengthCheckString(0, "test ");
      executeRsLengthCheckString(0, "test ");
      executeRsLengthCheckString(0, "rest");
      executeRsLengthCheckString(0, 15);
      executeRsLengthCheckString(0, 1);
      executeRsLengthCheckString(0, -15);
      executeRsLengthCheckString(0, null);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check result sets 
////////////////////////////////////////////////////////////////////////////////
          
    testRsLengthsNull : function () {
      executeRsLengthCheckNull(0, "test");
      executeRsLengthCheckNull(0, "1");
      executeRsLengthCheckNull(0, 1);
      executeRsLengthCheckNull(0, 2);
    }

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlBindParametersTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
