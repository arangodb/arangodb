////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, bind parameters
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

function ahuacatlBindTestSuite () {
  var errors = internal.errors;
  var numbers = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query
////////////////////////////////////////////////////////////////////////////////

  function executeQuery (query, bindVars) {
    var cursor = QUERY(query, bindVars);
    return cursor;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a given query and return the results as an array
////////////////////////////////////////////////////////////////////////////////

  function getQueryResults (query, bindVars, isFlat) {
    var result = executeQuery(query, bindVars).getRows();
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

////////////////////////////////////////////////////////////////////////////////
/// @brief return the error code from a result
////////////////////////////////////////////////////////////////////////////////

  function getErrorCode (fn) {
    try {
      fn();
    }
    catch (e) {
      return e.errorNum;
    }
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
/// @brief test a single bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindSingle : function () {
      var expected = [ 2 ];
      var actual = getQueryResults("FOR u IN @list FILTER u == 2 RETURN u", { "list" : [ 1, 2, 3, 4, 5, 22 ] }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a null bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindNull : function () {
      var expected = [ null ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : null }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a bool bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindBool1 : function () {
      var expected = [ true ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : true }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a bool bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindBool2 : function () {
      var expected = [ false ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : false }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a numeric bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindNumeric1 : function () {
      var expected = [ -5 ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : -5 }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a numeric bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindNumeric2 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : 0 }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a numeric bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindNumeric3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : 1 }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a string bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindString1 : function () {
      var expected = [ "the quick fox" ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : "the quick fox" }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a string bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindString2 : function () {
      var expected = [ "" ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : "" }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a string bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindString3 : function () {
      var expected = [ "\"the\"", "'fox'" ];
      var actual = getQueryResults("FOR u IN @list FILTER u IN @values RETURN u", { "list" : [ "\"the\"", "'fox'"  ], "values" : [ "\"the\"", "'fox'" ] }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a list bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindList1 : function () {
      var expected = [ "" ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : [ ] }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a list bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindList2 : function () {
      var expected = [ true, false, 1, null, [ ] ];
      var actual = getQueryResults("FOR u IN @list FILTER u IN @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : [ true, false, 1, null, [ ] ] }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test an array bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindArray1 : function () {
      var expected = [ { } ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : { } }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test an array bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindArray2 : function () {
      var expected = [ { "brown" : true, "fox" : true, "quick" : true } ];
      var list = [ { "fox" : false, "brown" : false, "quick" : false },
                   { "fox" : true,  "brown" : false, "quick" : false },
                   { "fox" : true,  "brown" : true,  "quick" : false },
                   { "fox" : true,  "brown" : true,  "quick" : true },
                   { "fox" : false, "brown" : true,  "quick" : false },
                   { "fox" : false, "brown" : true,  "quick" : true },
                   { "fox" : false, "brown" : false, "quick" : true } ];

      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : list, "value" : { "fox" : true, "brown" : true, "quick" : true } }, false);

      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple occurrences of bind variables
////////////////////////////////////////////////////////////////////////////////

    testBindMultiple1 : function () {
      var expected = [ 1.2, 1.2, 1.2 ];
      var actual = getQueryResults("FOR u IN [ @value, @value, @value ] FILTER u == @value RETURN u", { "value" : 1.2 }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple occurrences of bind variables
////////////////////////////////////////////////////////////////////////////////

    testBindMultiple2 : function () {
      var expected = [ 5, 15 ];
      var actual = getQueryResults("FOR u IN [ @value1, @value2, @value3 ] FILTER u IN [ @value1, @value2, @value3 ] && u >= @value1 && u <= @value2 RETURN u", { "value1" : 5, "value2" : 15, "value3" : 77 }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind parameter names
////////////////////////////////////////////////////////////////////////////////

    testBindNames : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults("FOR u IN [ @1, @2, @0, @11, @10, @fox, @FOX, @fox_, @fox__, @fox_1 ] RETURN u", { "1" : 1, "2" : 2, "0" : 3, "11" : 4, "10" : 5, "fox" : 6, "FOX" : 7, "fox_" : 8, "fox__" : 9, "fox_1" : 10 }, true);
      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind variable data types
////////////////////////////////////////////////////////////////////////////////

    testBindCombined : function () {
      var expected = [ { "active" : true, "id" : 1, "likes" : "swimming", "name" : "John" } ]; 
      var list = [ { "id" : 1, "active" : true, "likes" : "swimming", "name" : "John" }, 
                   { "id" : 2, "active" : true, "likes" : "swimming", "name" : "Vanessa" }, 
                   { "id" : 3, "active" : false, "likes" : "hiking", "name" : "Fred" }, 
                   { "id" : 4, "active" : true, "likes" : "chess", "name" : "Mohammed" }, 
                   { "id" : 5, "active" : false, "likes" : "lying", "name" : "Chris" }, 
                   { "id" : 6, "active" : true, "likes" : "running", "name" : "Amy" }, 
                   { "id" : 7, "active" : false, "likes" : "chess", "name" : "Stuart" } ];

      var actual = getQueryResults("FOR u IN @list FILTER u.name != @name && u.id != @id && u.likes IN @likes && u.active == @active RETURN u", { "list" : list, "name" : "Amy", "id" : 2, "likes" : [ "fishing", "hiking", "swimming" ], "active" : true }, false);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test missing parameter values
////////////////////////////////////////////////////////////////////////////////

    testBindMissing : function () {
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN @", { }); } ));
      assertEqual(errors.ERROR_QUERY_PARSE.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN @@", { }); } ));
      assertEqual(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN @value", { }); } ));
      assertEqual(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN @value", { "values" : [ ] }); } ));
      assertEqual(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN @value1", { "value" : 1, "value11": 2 }); } ));
      assertEqual(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN @value1 + @value2", { "value1" : 1 }); } ));
      assertEqual(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN @value1 + @value2 + @value3", { "value1" : 1, "value2" : 2 }); } ));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test superfluous parameter values
////////////////////////////////////////////////////////////////////////////////

    testBindSuperfluous : function () {
      assertEqual(errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN 1", { "value" : 1 }); } ));
      assertEqual(errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN 1", { "value" : null }); } ));
      assertEqual(errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN @value", { "value" : 3, "value2" : 3 }); } ));
      assertEqual(errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code, getErrorCode(function() { QUERY("FOR u IN [ 1, 2 ] RETURN @value1 + @value2 + @value3", { "value1" : 1, "value2" : 2, "value3" : 3, "value4" : 4 }); } ));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindCollection1 : function () {
      var expected = [ 5, 63 ];
      var actual = getQueryResults("FOR u IN @@collection FILTER u.value IN [ @value1, @value2 ] SORT u.value RETURN u.value", { "@collection" : numbers.name(), "value1" : 5, "value2" : 63 }, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindCollection2 : function () {
      var expected = [ 99 ];
      var collection = numbers.name();
      var params = { };
      params["@" + collection] = collection;
      var actual = getQueryResults("FOR " + collection + " IN @@" + collection + " FILTER " + collection + ".value == 99 RETURN " + collection + ".value", params, true);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindCollectionInvalid : function () {
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : null }, true); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : false }, true); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : true }, true); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : 0 }, true); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : 2 }, true); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : "" }, true); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : [ ] }, true); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : [ { } ] }, true); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : { "name" : "collection" } }, true); });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlBindTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
