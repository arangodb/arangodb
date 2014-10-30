/*jshint strict: false, maxlen: 500 */
/*global require, assertEqual, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Ahuacatl, subqueries
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
var getQueryResults = helper.getQueryResults2;
var findExecutionNodes = helper.findExecutionNodes;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSubqueryTestSuite () {

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
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryDependent1 : function () {
      var expected = [ 2, 4, 6 ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] LET s = (FOR j IN [ 1, 2, 3 ] RETURN i * 2) RETURN s[i - 1]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryDependent2 : function () {
      var expected = [ 2, 4, 6 ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] LET t = (FOR k IN [ 1, 2, 3 ] RETURN k) LET s = (FOR j IN [ 1, 2, 3 ] RETURN t[i - 1] * 2) RETURN s[i - 1]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery evaluation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryDependent3 : function () {
      var expected = [ 4, 5, 6, 8, 10, 12, 12, 15, 18 ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] FOR j IN [ 4, 5, 6 ] LET s = (FOR k IN [ 1 ] RETURN i * j) RETURN s[0]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery optimisation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryIndependent1 : function () {
      var expected = [ 9, 8, 7 ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] LET s = (FOR j IN [ 9, 8, 7 ] RETURN j) RETURN s[i - 1]");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery optimisation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryIndependent2 : function () {
      var expected = [ [ 9, 8, 7 ] ];
      var actual = getQueryResults("LET s = (FOR j IN [ 9, 8, 7 ] RETURN j) RETURN s");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery optimisation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryIndependent3 : function () {
      var expected = [ [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ], [ 25 ] ];
      var actual = getQueryResults("FOR i IN [ 1, 2, 3 ] FOR j IN [ 1, 2, 3 ] LET s = (FOR k IN [ 25 ] RETURN k) RETURN s");

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery optimisation
////////////////////////////////////////////////////////////////////////////////

    testSubqueryIndependent4 : function () {
      var expected = [ [ 2, 4, 6 ] ];
      var actual = getQueryResults("LET a = (FOR i IN [ 1, 2, 3 ] LET s = (FOR j IN [ 1, 2 ] RETURN j) RETURN i * s[1]) RETURN a");

      assertEqual(expected, actual);
    },

    testSubqueryOutVariableName : function () {
      var XPResult = AQL_EXPLAIN('FOR u IN _users LET theLetVariable = (FOR j IN _users RETURN j) RETURN theLetVariable');

      var SubqueryNode = findExecutionNodes(XPResult, "SubqueryNode")[0];

      assertEqual(SubqueryNode.outVariable.name, "theLetVariable");
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSubqueryTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
