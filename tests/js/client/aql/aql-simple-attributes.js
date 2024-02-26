/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
const db = require('internal').db;

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
      var actual = db._query("LET foo = PASSTHRU('bar') LET bar = PASSTHRU(25) RETURN { foo, bar }");
      assertEqual(expected, actual.toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple attribute names
////////////////////////////////////////////////////////////////////////////////

    testSimpleBackticked : function () {
      var expected = { "foo" : "bar", "bar" : 25 };
      var actual = db._query("LET foo = PASSTHRU('bar') LET bar = PASSTHRU(25) RETURN { `foo`, `bar` }");
      assertEqual(expected, actual.toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple attribute names
////////////////////////////////////////////////////////////////////////////////

    testMixed1 : function () {
      var expected = { "foo" : "bar", "bar" : 25, "baz" : "test" };
      var actual = db._query("LET foo = PASSTHRU('bar') LET bar = PASSTHRU(25) RETURN { foo, bar, baz: 'test' }");
      assertEqual(expected, actual.toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple attribute names
////////////////////////////////////////////////////////////////////////////////

    testMixed2 : function () {
      var expected = { "foo" : "bar", "bar" : 25, "baz" : "test", "haxe" : 42 };
      var actual = db._query("LET foo = PASSTHRU('bar') LET bar = PASSTHRU(25) RETURN { foo, bar, baz: 'test', [ PASSTHRU(CONCAT('h', 'axe')) ] : 42 }");
      assertEqual(expected, actual.toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test simple with undefined variables
////////////////////////////////////////////////////////////////////////////////

    testUndefinedAttributeName1 : function () {
      try {
        db._query("RETURN { foo }");
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
        db._query("LET baz = PASSTHRU(42) RETURN { foo : 'bar', baz, bar }");
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

