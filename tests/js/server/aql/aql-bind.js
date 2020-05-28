/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertException */

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
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlBindTestSuite () {
  var errors = internal.errors;
  var numbers = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      internal.db._drop("UnitTestsAhuacatlNumbers");
      numbers = internal.db._create("UnitTestsAhuacatlNumbers");

      for (var i = 1; i <= 100; ++i) {
        numbers.save({ "value" : i });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop("UnitTestsAhuacatlNumbers");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind with underscored variables
////////////////////////////////////////////////////////////////////////////////

    testBindNamesWithUnderscore1 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9 ];
      var actual = getQueryResults("FOR u IN @_values FILTER u != @_value RETURN u", { "_values" : [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ], "_value" : 8 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind with underscored variables
////////////////////////////////////////////////////////////////////////////////

    testBindNamesWithUnderscore2 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9 ];
      var actual = getQueryResults("FOR u IN @values_to_look_in FILTER u != @value_to_look_for RETURN u", { "values_to_look_in" : [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ], "value_to_look_for" : 8 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind with underscored variables
////////////////////////////////////////////////////////////////////////////////

    testBindNamesWithUnderscore3 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9 ];
      var actual = getQueryResults("FOR u IN @values_ FILTER u != @value_ RETURN u", { "values_" : [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ], "value_" : 8 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind with underscored variables
////////////////////////////////////////////////////////////////////////////////

    testBindNamesWithUnderscore4 : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 9 ];
      var actual = getQueryResults("FOR u IN @___values___ FILTER u != @___value___ RETURN u", { "___values___" : [ 1, 2, 3, 4, 5, 6, 7, 8, 9 ], "___value___" : 8 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind with underscored variables
////////////////////////////////////////////////////////////////////////////////

    testBindNamesWithUnderscoreInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR u IN [ 1, 2 ] RETURN @@_", { "_" : 1 });
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR u IN [ 1, 2 ] RETURN @@__", { "__" : 1 });
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR u IN [ 1, 2 ] RETURN @_", { "_" : 1 });
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR u IN [ 1, 2 ] RETURN @__", { "__" : 1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test asc / desc variable
////////////////////////////////////////////////////////////////////////////////

    testBindAscDesc1 : function () {
      var expected = [ 1, 2, 3, 4, 6, 7, 9 ];
      var actual = getQueryResults("FOR u IN @list SORT u @order RETURN u", { "list" : [ 9, 4, 3, 7, 2, 1, 6 ], "order" : "asc" });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test asc / desc variable
////////////////////////////////////////////////////////////////////////////////

    testBindAscDesc2 : function () {
      var expected = [ 1, 2, 3, 4, 6, 7, 9 ].reverse();
      var actual = getQueryResults("FOR u IN @list SORT u @order RETURN u", { "list" : [ 9, 4, 3, 7, 2, 1, 6 ], "order" : "DESC" });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test asc / desc variable
////////////////////////////////////////////////////////////////////////////////

    testBindAscDesc3 : function () {
      var expected = [ 1, 2, 3, 4, 6, 7, 9 ];
      var actual = getQueryResults("FOR u IN @list SORT u @order RETURN u", { "list" : [ 9, 4, 3, 7, 2, 1, 6 ], "order" : true });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test asc / desc variable
////////////////////////////////////////////////////////////////////////////////

    testBindAscDesc4 : function () {
      var expected = [ 1, 2, 3, 4, 6, 7, 9 ].reverse();
      var actual = getQueryResults("FOR u IN @list SORT u @order RETURN u", { "list" : [ 9, 4, 3, 7, 2, 1, 6 ], "order" : false });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a single bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindSingle : function () {
      var expected = [ 2 ];
      var actual = getQueryResults("FOR u IN @list FILTER u == 2 RETURN u", { "list" : [ 1, 2, 3, 4, 5, 22 ] });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a null bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindNull : function () {
      var expected = [ null ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : null });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a bool bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindBool1 : function () {
      var expected = [ true ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : true });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a bool bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindBool2 : function () {
      var expected = [ false ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : false });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a numeric bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindNumeric1 : function () {
      var expected = [ -5 ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : -5 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a numeric bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindNumeric2 : function () {
      var expected = [ 0 ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : 0 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a numeric bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindNumeric3 : function () {
      var expected = [ 1 ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : 1 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test different value types
////////////////////////////////////////////////////////////////////////////////

    testBindValueTypes : function () {
      var values = { 
        nullValue: null, 
        falseValue: false, 
        trueValue: true, 
        intValue: 2, 
        doubleValue: -4.2, 
        emptyString : "", 
        nonemptyString : "foo", 
        arrayValue: [ 1, 2, 3, null, "", "one", "two", "foobarbaz" ], 
        objectValues: { "" : 1, "foo-bar-baz" : "test", "a b c" : -42 } 
      };

      var actual = getQueryResults("RETURN @values", { values: values });
      assertEqual([ values ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a string bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindString1 : function () {
      var expected = [ "the quick fox" ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : "the quick fox" });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a string bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindString2 : function () {
      var expected = [ "" ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : "" });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a string bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindString3 : function () {
      var expected = [ "\"the\"", "'fox'" ];
      var actual = getQueryResults("FOR u IN @list FILTER u IN @values RETURN u", { "list" : [ "\"the\"", "'fox'"  ], "values" : [ "\"the\"", "'fox'" ] });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test an array bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindArray1 : function () {
      var expected = [ "" ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : [ ] });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test an array bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindArray2 : function () {
      var expected = [ true, false, 1, null, [ ] ];
      var actual = getQueryResults("FOR u IN @list FILTER u IN @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : [ true, false, 1, null, [ ] ] });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test an object bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindObject1 : function () {
      var expected = [ { } ];
      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : [ "the quick fox", true, false, -5, 0, 1, null, "", [ ], { } ], "value" : { } });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test an object bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindObject2 : function () {
      var expected = [ { "brown" : true, "fox" : true, "quick" : true } ];
      var list = [ { "fox" : false, "brown" : false, "quick" : false },
                   { "fox" : true,  "brown" : false, "quick" : false },
                   { "fox" : true,  "brown" : true,  "quick" : false },
                   { "fox" : true,  "brown" : true,  "quick" : true },
                   { "fox" : false, "brown" : true,  "quick" : false },
                   { "fox" : false, "brown" : true,  "quick" : true },
                   { "fox" : false, "brown" : false, "quick" : true } ];

      var actual = getQueryResults("FOR u IN @list FILTER u == @value RETURN u", { "list" : list, "value" : { "fox" : true, "brown" : true, "quick" : true } });

      assertEqual(expected, actual);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple occurrences of bind variables
////////////////////////////////////////////////////////////////////////////////

    testBindMultiple1 : function () {
      var expected = [ 1.2, 1.2, 1.2 ];
      var actual = getQueryResults("FOR u IN [ @value, @value, @value ] FILTER u == @value RETURN u", { "value" : 1.2 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple occurrences of bind variables
////////////////////////////////////////////////////////////////////////////////

    testBindMultiple2 : function () {
      var expected = [ 5, 15 ];
      var actual = getQueryResults("FOR u IN [ @value1, @value2, @value3 ] FILTER u IN [ @value1, @value2, @value3 ] && u >= @value1 && u <= @value2 RETURN u", { "value1" : 5, "value2" : 15, "value3" : 77 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bind parameter names
////////////////////////////////////////////////////////////////////////////////

    testBindNames : function () {
      var expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ];
      var actual = getQueryResults("FOR u IN [ @1, @2, @0, @11, @10, @fox, @FOX, @fox_, @fox__, @fox_1 ] RETURN u", { "1" : 1, "2" : 2, "0" : 3, "11" : 4, "10" : 5, "fox" : 6, "FOX" : 7, "fox_" : 8, "fox__" : 9, "fox_1" : 10 });
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

      var actual = getQueryResults("FOR u IN @list FILTER u.name != @name && u.id != @id && u.likes IN @likes && u.active == @active RETURN u", { "list" : list, "name" : "Amy", "id" : 2, "likes" : [ "fishing", "hiking", "swimming" ], "active" : true });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test missing parameter values
////////////////////////////////////////////////////////////////////////////////

    testBindMissing : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR u IN [ 1, 2 ] RETURN @", { });
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR u IN [ 1, 2 ] RETURN @@", { });
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "FOR u IN [ 1, 2 ] RETURN @value", { }); 
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "FOR u IN [ 1, 2 ] RETURN @value", { "values" : [ ] }); 
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "FOR u IN [ 1, 2 ] RETURN @value1", { "value" : 1, "value11": 2 }); 
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "FOR u IN [ 1, 2 ] RETURN @value1 + @value2", { "value1" : 1 }); 
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "FOR u IN [ 1, 2 ] RETURN @value1 + @value2 + @value3", { "value1" : 1, "value2" : 2 }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test superfluous parameter values
////////////////////////////////////////////////////////////////////////////////

    testBindSuperfluous : function () {
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code, "FOR u IN [ 1, 2 ] RETURN 1", { "value" : 1 });
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code, "FOR u IN [ 1, 2 ] RETURN 1", { "value" : null }); 
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code, "FOR u IN [ 1, 2 ] RETURN @value", { "value" : 3, "value2" : 3 });
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED.code, "FOR u IN [ 1, 2 ] RETURN @value1 + @value2 + @value3", { "value1" : 1, "value2" : 2, "value3" : 3, "value4" : 4 }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection bind variable with id
////////////////////////////////////////////////////////////////////////////////

    testBindCollectionWithId : function () {
      var expected = [ 5, 63 ];
      var actual = getQueryResults("FOR u IN @@collection FILTER u.value IN [ @value1, @value2 ] SORT u.value RETURN u.value", { "@collection" : numbers._id, "value1" : 5, "value2" : 63 });

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindCollection1 : function () {
      var expected = [ 5, 63 ];
      var actual = getQueryResults("FOR u IN @@collection FILTER u.value IN [ @value1, @value2 ] SORT u.value RETURN u.value", { "@collection" : numbers.name(), "value1" : 5, "value2" : 63 });

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
      var actual = getQueryResults("FOR " + collection + " IN @@" + collection + " FILTER " + collection + ".value == 99 RETURN " + collection + ".value", params);

      assertEqual(expected, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection bind variable
////////////////////////////////////////////////////////////////////////////////

    testBindCollectionInvalid : function () {
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : null }); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : false }); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : true }); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : 0 }); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : 2 }); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : "" }); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : [ ] }); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : [ { } ] }); });
      assertException(function() { getQueryResults("FOR u IN @@collection RETURN 1", { "@collection" : { "name" : "collection" } }); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test limit with bind parameters
////////////////////////////////////////////////////////////////////////////////

    testBindLimit : function () {
      var offset, limit;
      var data = [ 1, 2, 3, 4 ];

      for (offset = 0; offset < 6; ++offset) {
        for (limit = 0; limit < 6; ++limit) {
          var actual = getQueryResults("FOR u IN [ 1, 2, 3, 4 ] LIMIT @offset, @count RETURN u", { "offset" : offset, "count": limit });
       
          assertEqual(data.slice(offset, limit === 0 ? limit : offset + limit), actual);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test limit with invalid bind parameters
////////////////////////////////////////////////////////////////////////////////

    testBindLimitInvalid : function () {
      assertException(function() { getQueryResults("FOR u IN [ 1, 2, 3 ] LIMIT @offset, @count RETURN u", { "offset" : "foo", "count" : 1 }); });
      assertException(function() { getQueryResults("FOR u IN [ 1, 2, 3 ] LIMIT @offset, @count RETURN u", { "offset" : 1, "count" : "foo" }); });
      assertException(function() { getQueryResults("FOR u IN [ 1, 2, 3 ] LIMIT @offset, @count RETURN u", { "offset" : "foo", "count" : "foo" }); });
      assertException(function() { getQueryResults("FOR u IN [ 1, 2, 3 ] LIMIT @offset, @count RETURN u", { "offset" : -1, "count" : -1 }); });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bound attribute names
////////////////////////////////////////////////////////////////////////////////

    testBindAttributeNames1 : function () {
      var actual = getQueryResults("FOR u IN [ { age: 1 }, { age: 2 }, { age: 3 } ] RETURN u.@what", { "what": "age" });

      assertEqual([ 1, 2, 3 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bound attribute names
////////////////////////////////////////////////////////////////////////////////

    testBindAttributeNames2 : function () {
      var actual = getQueryResults("FOR u IN [ { age: 1 }, { age: 2 }, { age: 3 } ] FILTER u.@what == @age RETURN u.@what", { "what": "age", "age": 2 });

      assertEqual([ 2 ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bound attribute names
////////////////////////////////////////////////////////////////////////////////

    testBindAttributeNames3 : function () {
      var actual = getQueryResults("FOR u IN [ { age1: 1 }, { age2: 2 }, { age3: 3 } ] RETURN u.@what", { "what": "age2" });

      assertEqual([ null, 2, null ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bound attribute names
////////////////////////////////////////////////////////////////////////////////

    testBindAttributeNames4 : function () {
      var actual = getQueryResults("FOR u IN [ { `the fox` : 1 }, { `the-foxx` : 2 }, { `3`: 3 } ] FILTER u.@what == @value RETURN @value", { "what" : "the fox", "value" : 1 });
      assertEqual([ 1 ], actual);
      
      actual = getQueryResults("FOR u IN [ { `the fox` : 1 }, { `the-foxx` : 2 }, { `3`: 3 } ] FILTER u.@what == @value RETURN @value", { "what" : "the-foxx", "value" : 2 });
      assertEqual([ 2 ], actual);
      
      actual = getQueryResults("FOR u IN [ { `the fox` : 1 }, { `the-foxx` : 2 }, { `3`: 3 } ] FILTER u.@what == @value RETURN @value", { "what" : "3", "value" : 3 });
      assertEqual([ 3 ], actual);
      
      actual = getQueryResults("FOR u IN [ { `the fox` : 1 }, { `the-foxx` : 2 }, { `3`: 3 } ] FILTER HAS(u, @what) RETURN u", { "what" : "the fox" });
      assertEqual([ { "the fox" : 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bound attribute names
////////////////////////////////////////////////////////////////////////////////

    testBindAttributeNames5 : function () {
      var actual = getQueryResults("FOR u IN [ { name: { first: 'foo', last: 'bar' } } ] FILTER u.name.@part1 == 'foo' && u.name.@part2 == 'bar' RETURN u", { "part1" : "first", "part2" : "last" });
      assertEqual([ { name: { first: "foo", last: "bar" } } ], actual);
      
      actual = getQueryResults("FOR u IN [ { name: { first: 'foo', last: 'bar' } } ] FILTER u.@part1 == 'foo' && u.@part2 == 'bar' RETURN u", { "part1" : [ "name", "first" ], "part2" : [ "name", "last" ] });
      assertEqual([ { name: { first: "foo", last: "bar" } } ], actual);
      
      actual = getQueryResults("FOR u IN [ { name: { first: 'foo', last: 'bar' } } ] FILTER u.@part1.@first == 'foo' && u.@part1.@last == 'bar' RETURN u", { "part1" : "name", "first" : "first", "last" : "last" });
      assertEqual([ { name: { first: "foo", last: "bar" } } ], actual);
      
      actual = getQueryResults("FOR u IN [ { name: { first: 'foo', last: 'bar' } } ] FILTER u.@first == 'foo' && u.@last == 'bar' RETURN u", { "first" : [ "name", "first" ], "last" : [ "name", "last" ] });
      assertEqual([ { name: { first: "foo", last: "bar" } } ], actual);
      
      actual = getQueryResults("FOR u IN [ { values: [ { age: 28 }, { age: 30 }, { age: 40 } ] } ] RETURN u.values[*].@what", { "what" : "age" });
      assertEqual([ [ 28, 30, 40 ] ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bound attribute names
////////////////////////////////////////////////////////////////////////////////

    testBindAttributeNamesInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_TYPE.code, "FOR u IN [ { age: 1 }, { age: 2 } ] RETURN u.@what", { "what" : 1 });
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_TYPE.code, "FOR u IN [ { age: 1 }, { age: 2 } ] RETURN u.@what", { "what" : true });
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_TYPE.code, "FOR u IN [ { age: 1 }, { age: 2 } ] RETURN u.@what", { "what" : null });
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_TYPE.code, "FOR u IN [ { age: 1 }, { age: 2 } ] RETURN u.@what", { "what" : "" });
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_TYPE.code, "FOR u IN [ { age: 1 }, { age: 2 } ] RETURN u.@what", { "what" : [ ] });
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_TYPE.code, "FOR u IN [ { age: 1 }, { age: 2 } ] RETURN u.@what", { "what" : { } });
      assertQueryError(errors.ERROR_QUERY_BIND_PARAMETER_MISSING.code, "FOR u IN [ { age: 1 }, { age: 2 } ] RETURN u.@what", { "age" : "age" });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlBindTestSuite);

return jsunity.done();

