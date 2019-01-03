/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */
////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, functions
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
var errors = internal.errors;
var jsunity = require("jsunity");
var helper = require("@arangodb/aql-helper");
var getQueryResults = helper.getQueryResults;
var assertQueryError = helper.assertQueryError;
var assertQueryWarningAndNull = helper.assertQueryWarningAndNull;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlCallApplyTestSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test call function
////////////////////////////////////////////////////////////////////////////////

    testCall : function () {
      var data = [
        [ "foo bar", [ "TRIM", "  foo bar  " ] ],
        [ "foo bar", [ "TRIM", "  foo bar  ", "\r\n \t" ] ],
        [ "..foo bar..", [ "TRIM", "  ..foo bar..  " ] ],
        [ "foo bar", [ "TRIM", "  ..foo bar..  ", ". " ] ],
        [ "foo", [ "LEFT", "foobarbaz", 3 ] ],
        [ "fooba", [ "LEFT", "foobarbaz", 5 ] ],
        [ "foob", [ "SUBSTRING", "foobarbaz", 0, 4 ] ],
        [ "oob", [ "SUBSTRING", "foobarbaz", 1, 3 ] ],
        [ "barbaz", [ "SUBSTRING", "foobarbaz", 3 ] ],
        [ "FOOBARBAZ", [ "UPPER", "foobarbaz" ] ],
        [ "foobarbaz", [ "lower", "FOOBARBAZ" ] ],
        [ "abcfood", [ "concat", "a", "b", "c", "foo", "d" ] ],
        [ 1, [ "flOoR", 1.6 ] ],
        [ 17, [ "MIN", [ 23, 42, 17 ] ] ],
        [ [ 1, 2, 3, 4, 7, 9, 10, 12 ], [ "UNION_DISTINCT", [ 1, 2, 3, 4 ], [ 9, 12 ], [ 2, 9, 7, 10 ] ] ],
        [ { a: true, b: false, c: null }, [ "ZIP", [ "a", "b", "c" ], [ true, false, null ] ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN CALL(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")");
        if (Array.isArray(d[0])) {
          assertEqual(d[0].sort(), actual[0].sort(), d);
        } else {
          assertEqual(d[0], actual[0], d);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test call function
////////////////////////////////////////////////////////////////////////////////

    testCallDynamic1 : function () {
      var actual = getQueryResults("FOR func IN [ 'TRIM', 'LOWER', 'UPPER' ] RETURN CALL(func, '  foObAr  ')");
      assertEqual(actual, [ 'foObAr', '  foobar  ', '  FOOBAR  ' ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test call function
////////////////////////////////////////////////////////////////////////////////

    testCallDynamic2 : function () {
      var actual = getQueryResults("FOR doc IN [ { value: '  foobar', func: 'TRIM' }, { value: 'FOOBAR', func: 'LOWER' }, { value: 'foobar', func: 'UPPER' } ] RETURN CALL(doc.func, doc.value)");
      assertEqual(actual, [ 'foobar', 'foobar', 'FOOBAR' ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test call function
////////////////////////////////////////////////////////////////////////////////

    testCallNonExisting : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN CALL()"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN CALL('nono-existing', [ 'baz' ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN CALL('foobar', 'baz')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN CALL(' trim', 'baz')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code, "RETURN CALL('foo::bar::baz', 'baz')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CALL(123, 'baz')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN CALL([ ], 'baz')"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test call function
////////////////////////////////////////////////////////////////////////////////
    testCallRecursive : function () {
      var actual = getQueryResults("RETURN CALL('CALL', 'TRIM', '  foo bar  ')");
      assertEqual(actual, [ 'foo bar' ]);
      actual = getQueryResults("RETURN CALL('APPLY', 'TRIM', ['  foo bar  '])");
      assertEqual(actual, [ 'foo bar' ]);

      let recursion = [];
      for (let i = 0; i < 100; i++) {
        recursion.push('CALL');
      }
      recursion.push('TRIM');
      recursion.push('  foo bar  ');
      let query = "RETURN CALL('" + recursion.join('\',\'') + "')";
      actual = getQueryResults(query);
      
      assertEqual(actual, [ 'foo bar' ]);

    },
////////////////////////////////////////////////////////////////////////////////
/// @brief test apply function
////////////////////////////////////////////////////////////////////////////////

    testApply : function () {
      var data = [
        [ "foo bar", [ "TRIM", "  foo bar  " ] ],
        [ "foo bar", [ "TRIM", "  foo bar  ", "\r\n \t" ] ],
        [ "..foo bar..", [ "TRIM", "  ..foo bar..  " ] ],
        [ "foo bar", [ "TRIM", "  ..foo bar..  ", ". " ] ],
        [ "foo", [ "LEFT", "foobarbaz", 3 ] ],
        [ "fooba", [ "LEFT", "foobarbaz", 5 ] ],
        [ "foob", [ "SUBSTRING", "foobarbaz", 0, 4 ] ],
        [ "oob", [ "SUBSTRING", "foobarbaz", 1, 3 ] ],
        [ "barbaz", [ "SUBSTRING", "foobarbaz", 3 ] ],
        [ "FOOBARBAZ", [ "UPPER", "foobarbaz" ] ],
        [ "foobarbaz", [ "lower", "FOOBARBAZ" ] ],
        [ "abcfood", [ "concat", "a", "b", "c", "foo", "d" ] ],
        [ 1, [ "flOoR", 1.6 ] ],
        [ 17, [ "MIN", [ 23, 42, 17 ] ] ],
        [ [ 1, 2, 3, 4, 7, 9, 10, 12 ], [ "UNION_DISTINCT", [ 1, 2, 3, 4 ], [ 9, 12 ], [ 2, 9, 7, 10 ] ] ],
        [ { a: true, b: false, c: null }, [ "ZIP", [ "a", "b", "c" ], [ true, false, null ] ] ]
      ];

      data.forEach(function (d) {
        var args = [ ];
        for (var i = 1; i < d[1].length; ++i) {
          args.push(d[1][i]);
        }
        var actual = getQueryResults("RETURN APPLY(" + JSON.stringify(d[1][0]) + ", " + JSON.stringify(args) + ")");
        if (Array.isArray(d[0])) {
          assertEqual(d[0].sort(), actual[0].sort(), d);
        } else {
          assertEqual(d[0], actual[0], d);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test apply function
////////////////////////////////////////////////////////////////////////////////

    testApplyDynamic1 : function () {
      var actual = getQueryResults("FOR func IN [ 'TRIM', 'LOWER', 'UPPER' ] RETURN APPLY(func, [ '  foObAr  ' ])");
      assertEqual(actual, [ 'foObAr', '  foobar  ', '  FOOBAR  ' ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test apply function
////////////////////////////////////////////////////////////////////////////////

    testApplyDynamic2 : function () {
      var actual = getQueryResults("FOR doc IN [ { value: '  foobar', func: 'TRIM' }, { value: 'FOOBAR', func: 'LOWER' }, { value: 'foobar', func: 'UPPER' } ] RETURN APPLY(doc.func, [ doc.value ])");
      assertEqual(actual, [ 'foobar', 'foobar', 'FOOBAR' ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test apply function
////////////////////////////////////////////////////////////////////////////////

    testApplyNonExisting : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN APPLY()"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN APPLY('TRIM', 1, 2)"); 

      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN APPLY('nono-existing', [ 'baz' ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN APPLY('foobar', [ 'baz' ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NAME_UNKNOWN.code, "RETURN APPLY(' trim', [ 'baz' ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code, "RETURN APPLY('foo::bar::baz', [ 'baz' ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN APPLY(123, [ 'baz' ])"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN APPLY([ ], [ 'baz' ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test apply function
////////////////////////////////////////////////////////////////////////////////

    testApplyRecursive : function () {
      var actual = getQueryResults("RETURN APPLY('CALL', ['TRIM', '  foo bar  '])");
      assertEqual(actual, [ 'foo bar' ]);
      actual = getQueryResults("RETURN APPLY('APPLY', ['TRIM', ['  foo bar  ']])");
      assertEqual(actual, [ 'foo bar' ]);

      let recursion = '';
      let close = '';
      let rDepth = 20;
      for (let i = 0; i < rDepth; i++) {
        if (i > 0) {
          recursion += '[';
        }
        recursion += '\'APPLY\'';
        if (i < rDepth) {
          recursion += ',';
        }
        close += ']';
      }
      recursion += '[\'TRIM\', [ \'  foo bar  \'] ' + close;
      let query = "RETURN APPLY(" + recursion + ")";
      actual = getQueryResults(query);
      
      assertEqual(actual, [ 'foo bar' ]);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlCallUserDefinedTestSuite () {
  var aqlfunctions = require("@arangodb/aql/functions");

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      [ "add3", "add2", "call", "throwing" ].forEach(function (f) {
        try {
          aqlfunctions.unregister("UnitTests::func::" + f);
        }
        catch (err) {
        }
      });

      aqlfunctions.register("UnitTests::func::add3", function (a, b, c) {
        return a + b + c;
      });
      aqlfunctions.register("UnitTests::func::add2", function (a, b) {
        return a + b;
      });
      aqlfunctions.register("UnitTests::func::call", function () {
        return undefined;
      });
      aqlfunctions.register("UnitTests::func::throwing", function () {
        throw "doh!";
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      [ "add3", "add2", "call", "throwing" ].forEach(function (f) {
        try {
          aqlfunctions.unregister("UnitTests::func::" + f);
        }
        catch (err) {
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test call function
////////////////////////////////////////////////////////////////////////////////

    testUserDefCall : function () {
      var data = [
        [ null, [ "UnitTests::func::call", 1234 ] ],
        [ null, [ "UnitTests::func::call", "foo", "bar" ] ],
        [ null, [ "UnitTests::func::add2" ] ],
        [ null, [ "UnitTests::func::add2", 23 ] ],
        [ 23, [ "UnitTests::func::add2", 23, null ] ],
        [ 65, [ "UnitTests::func::add2", 23, 42 ] ],
        [ null, [ "UnitTests::func::add3", 23 ] ],
        [ null, [ "UnitTests::func::add3", 23, 42 ] ],
        [ 65, [ "UnitTests::func::add3", 23, 42, null ] ],
        [ 120, [ "UnitTests::func::add3", 23, 42, 55 ] ],
        [ "65foo", [ "UnitTests::func::add3", 23, 42, "foo" ] ],
        [ "bar4213", [ "UnitTests::func::add3", "bar", 42, 13 ] ],
        [ 96, [ "UNITTESTS::FUNC::aDD3", -1, 42, 55 ] ],
        [ 96, [ "unittests::func::ADD3", -1, 42, 55 ] ]
      ];

      data.forEach(function (d) {
        var actual = getQueryResults("RETURN CALL(" + d[1].map(function (v) { return JSON.stringify(v); }).join(", ") + ")");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test apply function
////////////////////////////////////////////////////////////////////////////////

    testUserDefApply : function () {
      var data = [
        [ null, [ "UnitTests::func::call", 1234 ] ],
        [ null, [ "UnitTests::func::call", "foo", "bar" ] ],
        [ null, [ "UnitTests::func::add2" ] ],
        [ null, [ "UnitTests::func::add2", 23 ] ],
        [ 23, [ "UnitTests::func::add2", 23, null ] ],
        [ 65, [ "UnitTests::func::add2", 23, 42 ] ],
        [ null, [ "UnitTests::func::add3", 23 ] ],
        [ null, [ "UnitTests::func::add3", 23, 42 ] ],
        [ 65, [ "UnitTests::func::add3", 23, 42, null ] ],
        [ 120, [ "UnitTests::func::add3", 23, 42, 55 ] ],
        [ "65foo", [ "UnitTests::func::add3", 23, 42, "foo" ] ],
        [ "bar4213", [ "UnitTests::func::add3", "bar", 42, 13 ] ],
        [ 96, [ "UNITTESTS::FUNC::aDD3", -1, 42, 55 ] ],
        [ 96, [ "unittests::func::ADD3", -1, 42, 55 ] ]
      ];

      data.forEach(function (d) {
        var args = [ ];
        for (var i = 1; i < d[1].length; ++i) {
          args.push(d[1][i]);
        }
        var actual = getQueryResults("RETURN APPLY(" + JSON.stringify(d[1][0]) + ", " + JSON.stringify(args) + ")");
        assertEqual(d[0], actual[0], d);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-existing functions
////////////////////////////////////////////////////////////////////////////////

    testUserDefNonExisting : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code, "RETURN CALL('UNITTESTS::FUNC::MEOW', 'baz')"); 
      assertQueryError(errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code, "RETURN APPLY('UNITTESTS::FUNC::MEOW', [ 'baz' ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test throwing function
////////////////////////////////////////////////////////////////////////////////

    testUserDefThrows : function () {
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR.code, "RETURN CALL('UNITTESTS::FUNC::THROWING')"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR.code, "RETURN APPLY('UNITTESTS::FUNC::THROWING', [ ])"); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test function name passed from the outside
////////////////////////////////////////////////////////////////////////////////

    testUserDefFunctionName : function () {
      aqlfunctions.register("UnitTests::func::call", function () { return this.name; });

      var actual = getQueryResults("RETURN UnitTests::func::call()");
      assertEqual("UNITTESTS::FUNC::CALL", actual[0]);
      
      actual = getQueryResults("RETURN CALL('UNITTESTS::FUNC::CALL', [])");
      assertEqual("UNITTESTS::FUNC::CALL", actual[0]);
      
      actual = getQueryResults("RETURN CALL('unittests::func::call', [])");
      assertEqual("UNITTESTS::FUNC::CALL", actual[0]);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlCallApplyTestSuite);
jsunity.run(ahuacatlCallUserDefinedTestSuite);

return jsunity.done();

