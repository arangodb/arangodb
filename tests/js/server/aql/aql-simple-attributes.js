/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, fail, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for simple attributes
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
var internal = require("internal");
var errors = internal.errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSimpleAttributesTestSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple attribute names
////////////////////////////////////////////////////////////////////////////////

    testSimple : function () {
      var expected = { "foo" : "bar", "bar" : 25 };
      var actual = AQL_EXECUTE("LET foo = PASSTHRU('bar') LET bar = PASSTHRU(25) RETURN { foo, bar }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple attribute names
////////////////////////////////////////////////////////////////////////////////

    testSimpleBackticked : function () {
      var expected = { "foo" : "bar", "bar" : 25 };
      var actual = AQL_EXECUTE("LET foo = PASSTHRU('bar') LET bar = PASSTHRU(25) RETURN { `foo`, `bar` }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple attribute names
////////////////////////////////////////////////////////////////////////////////

    testMixed1 : function () {
      var expected = { "foo" : "bar", "bar" : 25, "baz" : "test" };
      var actual = AQL_EXECUTE("LET foo = PASSTHRU('bar') LET bar = PASSTHRU(25) RETURN { foo, bar, baz: 'test' }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple attribute names
////////////////////////////////////////////////////////////////////////////////

    testMixed2 : function () {
      var expected = { "foo" : "bar", "bar" : 25, "baz" : "test", "haxe" : 42 };
      var actual = AQL_EXECUTE("LET foo = PASSTHRU('bar') LET bar = PASSTHRU(25) RETURN { foo, bar, baz: 'test', [ PASSTHRU(CONCAT('h', 'axe')) ] : 42 }");
      assertEqual(expected, actual.json[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple with undefined variables
////////////////////////////////////////////////////////////////////////////////

    testUndefinedAttributeName1 : function () {
      try {
        AQL_EXECUTE("RETURN { foo }");
        fail();
      }
      catch (e) {
        assertEqual(errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, e.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple with undefined variables
////////////////////////////////////////////////////////////////////////////////

    testUndefinedAttributeName2 : function () {
      try {
        AQL_EXECUTE("LET baz = PASSTHRU(42) RETURN { foo : 'bar', baz, bar }");
        fail();
      }
      catch (e) {
        assertEqual(errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, e.errorNum);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSimpleAttributesTestSuite);

return jsunity.done();

