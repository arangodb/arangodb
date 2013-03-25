////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, complex queries
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

var internal = require("internal");
var jsunity = require("jsunity");
var QUERY = internal.AQL_QUERY;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlComplexTestSuite () {
  var errors = internal.errors;
  var numbers = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query, bindVars) {
    var cursor = QUERY(query, bindVars, false, 3000);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, bindVars) {
    var result = executeQuery(query, bindVars).getRows();
    return result;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the error code from a result
////////////////////////////////////////////////////////////////////////////////

  function getErrorCode (result) {
    return result.errorNum;
  }


  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop("UnitTestsAhuacatlNumbers");
      numbers = internal.db._create("UnitTestsAhuacatlNumbers");

      for (var i = 1; i <= 100; ++i) {
        numbers.save({ "value" : i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop("UnitTestsAhuacatlNumbers");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList1 : function () {
      var expected = [ 1, 2, 3, 129, -4 ];

      var list = [ ];
      for (var i = -1000; i <= 1000; ++i) {
        list.push(i);
      } 
      var actual = getQueryResults("FOR u IN [ 1, 2, 3, 129, -4 ] FILTER u IN " + JSON.stringify(list) + " RETURN u", { });
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList2 : function () {
      var expected = [ 1, 2, 3, 129, -4 ];

      var list = [ ];
      for (var i = 1000; i >= -1000; --i) {
        list.push(i);
      } 
      var actual = getQueryResults("FOR u IN [ 1, 2, 3, 129, -4 ] FILTER u IN " + JSON.stringify(list) + " RETURN u", { });
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList3 : function () {
      var expected = [ -4, 1, 2, 3, 129 ];

      var list = [ ];
      for (var i = -1000; i <= 1000; ++i) {
        list.push(i);
      } 
      var actual = getQueryResults("FOR u IN " + JSON.stringify(list) + " FILTER u IN [ 1, 2, 3, 129, -4 ] RETURN u", { });
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList4 : function () {
      var expected = [ 129, 3, 2, 1, -4 ];

      var list = [ ];
      for (var i = 1000; i >= -1000; --i) {
        list.push(i);
      } 
      var actual = getQueryResults("FOR u IN " + JSON.stringify(list) + " FILTER u IN [ 1, 2, 3, 129, -4 ] RETURN u", { });
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList5 : function () {
      var list = [ ];
      for (var i = -1000; i <= 1000; ++i) {
        list.push(i);
      }
      var expected = list;
      var actual = getQueryResults("FOR u IN " + JSON.stringify(list) + " RETURN u", { });
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testLongList6 : function () {
      var list = [ ];
      for (var i = -1000; i <= 1000; ++i) {
        list.push(i);
      }
      var expected = list;
      var actual = getQueryResults("FOR u IN " + JSON.stringify(list) + " FILTER u IN " + JSON.stringify(list) + " RETURN u", { });
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test list nesting
////////////////////////////////////////////////////////////////////////////////

    testListNesting1 : function () {
      var list = [ 0 ];
      var last = list;
      for (var i = 1; i < 128; ++i) {
        last.push([ i ]);
        last = last[last.length - 1];
      }

      var expected = [ list ];
      var actual = getQueryResults("RETURN " + JSON.stringify(list), { });
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a long array
////////////////////////////////////////////////////////////////////////////////

    testLongArray1 : function () {
      var vars = { };
      for (var i = 1; i <= 1000; ++i) {
        vars["key" + i] = "value" + i;
      }

      var expected = [ vars ];
      var actual = getQueryResults("RETURN " + JSON.stringify(vars), { });
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test array nesting
////////////////////////////////////////////////////////////////////////////////

    testArrayNesting1 : function () {
      var array = { };
      var last = array;
      for (var i = 1; i < 128; ++i) {
        last["level" + i] = { };
        last = last["level" + i];
      }

      var expected = [ array ];
      var actual = getQueryResults("RETURN " + JSON.stringify(array), { });
      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test a long list of values
////////////////////////////////////////////////////////////////////////////////

    testBind1 : function () {
      var expected = [ "value9", "value666", "value997", "value999" ];

      var vars = { "res" : [ "value9", "value666", "value997", "value999" ] };
      var list = "[ ";
      for (var i = 1; i <= 1000; ++i) {
        vars["bind" + i] = "value" + i;

        if (i > 1) {
          list += ", ";
        }
        list += "@bind" + i;
      }
      list += " ]";

      var actual = getQueryResults("FOR u IN " + list + " FILTER u IN @res RETURN u", vars);
      assertEqual(expected, actual);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlComplexTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
