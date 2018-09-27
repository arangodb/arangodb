/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index usage
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
var db = require("@arangodb").db;
var aqlfunctions = require("@arangodb/aql/functions");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerUserFunctionsTestSuite () {
  'use strict';

  return {
    setUp : function () {
      db._useDatabase("_system");
      aqlfunctions.unregisterGroup("UnitTests::");
    },

    tearDown : function () {
      aqlfunctions.unregisterGroup("UnitTests::");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test data modification
////////////////////////////////////////////////////////////////////////////////

    testUserFunctionDataModificationObject : function () {
      aqlfunctions.register("UnitTests::modify", function (what) { 
        what.a = 1; 
        what.b = 3; 
        what.c = [ 1, 2 ]; 
        what.d.push([ 1, 2 ]); 
        what.e.f = { a: 1, b: 2 };
        delete what.f; 
        what.g = "foo";
        return what; 
      }, true);

      var data = { a: 2, b: 42, c: [ 1 ], d: [ 1 ], e: { a: 1 }, f: [ 0, 1 ] };
      var expected = { a: 1, b: 3, c: [ 1, 2 ], d: [ 1, [ 1, 2 ] ], e: { a: 1, f: { a: 1, b: 2 } }, g: "foo" };
      var result = AQL_EXECUTE("FOR i IN 1..1 RETURN UnitTests::modify(" + JSON.stringify(data) + ")", { }, { literalSizeThreshold: 0 });

      assertEqual(1, result.json.length);
      for (var i = 0; i < 1; ++i) {
        assertEqual(expected, result.json[i]);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test data modification
////////////////////////////////////////////////////////////////////////////////

    testUserFunctionDataModificationArray : function () {
      aqlfunctions.register("UnitTests::modify", function (what) { 
        what[0] = 1; 
        what[1] = 42; 
        what[2] = [ 1, 2 ]; 
        what[3].push([ 1, 2 ]); 
        what[4] = { a: 9, b: 2 };
        what.push("foo");
        what.push("bar");
        what.pop();
        return what;
      }, true);

      var data = [ 2, 3, [ 1 ], [ 1 ], { a: 1 }, [ 0, 1 ] ];
      var expected = [ 1, 42, [ 1, 2 ], [ 1, [ 1, 2 ] ], { a: 9, b: 2 }, [ 0, 1 ], "foo" ];
      var result = AQL_EXECUTE("FOR i IN 1..10 RETURN UnitTests::modify(" + JSON.stringify(data) + ")", { }, { literalSizeThreshold: 0 });

      assertEqual(10, result.json.length);
      for (var i = 0; i < 10; ++i) {
        assertEqual(expected, result.json[i]);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerUserFunctionsTestSuite);

return jsunity.done();

