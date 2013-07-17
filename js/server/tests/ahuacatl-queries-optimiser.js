////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, optimiser tests
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
var helper = require("org/arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlOptimiserTestSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "empty for loop"
////////////////////////////////////////////////////////////////////////////////

    testEmptyForLoop1 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR i IN [ ] RETURN 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "empty for loop"
////////////////////////////////////////////////////////////////////////////////

    testEmptyForLoop2 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR i IN [ 1 ] FOR j IN [ ] RETURN 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "empty for loop"
////////////////////////////////////////////////////////////////////////////////

    testEmptyForLoop3 : function () {
      var expected = [ [ ] ];

      var actual = getQueryResults("FOR i IN [ 1 ] LET f = (FOR j IN [ ] RETURN 1) RETURN f");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "empty for loop"
////////////////////////////////////////////////////////////////////////////////

    testEmptyForLoop4 : function () {
      var expected = [ [ ] ];

      var actual = getQueryResults("FOR i IN [ 1 ] LET f = (FOR j IN [1] FOR k IN [ ] RETURN 1) RETURN f");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "empty for loop"
////////////////////////////////////////////////////////////////////////////////

    testEmptyForLoop5 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR i IN (FOR j IN [ ] RETURN j) RETURN 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "empty for loop"
////////////////////////////////////////////////////////////////////////////////

    testEmptyForLoop6 : function () {
      var expected = [ [ ] ];

      var actual = getQueryResults("LET i = (FOR j IN [ ] RETURN j) RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "empty for loop"
////////////////////////////////////////////////////////////////////////////////

    testEmptyForLoop7 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR i IN [ ] FILTER 1 == 1 RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "empty for loop"
////////////////////////////////////////////////////////////////////////////////

    testEmptyForLoop8 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR i IN [ ] FILTER 1 > 2 RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "optimised away scope"
////////////////////////////////////////////////////////////////////////////////

    testOptimiseAwayScope1 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR i IN [ 1, 2 ] FILTER i == 1 && i < 1 RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "optimised away scope"
////////////////////////////////////////////////////////////////////////////////

    testOptimiseAwayScope2 : function () {
      var expected = [ ];

      var actual = getQueryResults("FOR i IN [ 1, 2 ] FOR j IN [ 1, 2 ] FILTER j == 1 && j < 1 RETURN j");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "optimised away scope"
////////////////////////////////////////////////////////////////////////////////

    testOptimiseAwayScope3 : function () {
      var expected = [ ];

      var actual = getQueryResults("LET a = (FOR i IN [ 1, 2 ] FILTER i > 5 RETURN i) FOR i IN a RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "optimised away scope"
////////////////////////////////////////////////////////////////////////////////

    testOptimiseAwayScope4 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("LET a = (FILTER 1 > 2 RETURN 1) RETURN 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "optimised away scope"
////////////////////////////////////////////////////////////////////////////////

    testOptimiseAwayScope5 : function () {
      var expected = [ [ ] ];

      var actual = getQueryResults("LET a = (FILTER 1 > 2 RETURN 1) RETURN a");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "optimised away scope"
////////////////////////////////////////////////////////////////////////////////

    testOptimiseAwayScope6 : function () {
      var expected = [ ];

      var actual = getQueryResults("FILTER 1 > 2 RETURN 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "optimised away scope"
////////////////////////////////////////////////////////////////////////////////

    testOptimiseAwayScope7 : function () {
      var expected = [ ];

      var actual = getQueryResults("LET a = 1 FILTER 1 > 2 RETURN 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "optimised away scope"
////////////////////////////////////////////////////////////////////////////////

    testOptimiseAwayScope8 : function () {
      var expected = [ ];

      var actual = getQueryResults("LET a = 1 FILTER 1 == 1 FILTER 1 > 2 RETURN 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "removed constant filter"
////////////////////////////////////////////////////////////////////////////////

    testRemovedConstantFilter1 : function () {
      var expected = [ 1 ];

      var actual = getQueryResults("FILTER 1 == 1 RETURN 1");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "removed constant filter"
////////////////////////////////////////////////////////////////////////////////

    testRemovedConstantFilter2 : function () {
      var expected = [ 1, 2 ];

      var actual = getQueryResults("FOR i IN [ 1, 2 ] FILTER 1 == 1 RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "removed constant filter"
////////////////////////////////////////////////////////////////////////////////

    testRemovedConstantFilter3 : function () {
      var expected = [ 1, 2 ];

      var actual = getQueryResults("FOR i IN [ 1, 2 ] FOR j IN [ 1 ] FILTER 1 == 1 RETURN i");
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test special case "removed constant filter"
////////////////////////////////////////////////////////////////////////////////

    testRemovedConstantFilter4 : function () {
      var expected = [ [ 1 ], [ 1 ] ];

      var actual = getQueryResults("FOR i IN [ 1, 2 ] LET a = (FOR j IN [ 1 ] FILTER 1 == 1 RETURN 1) RETURN a");
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlOptimiserTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
