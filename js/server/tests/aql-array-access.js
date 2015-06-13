/*jshint globalstrict:false, strict:false, maxlen: 700 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, array accesses
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

function arrayAccessTestSuite () {
  var values = [
    { name: "sir alfred", age: 60, loves: [ "lettuce", "flowers" ] }, 
    { person: { name: "gadgetto", age: 50, loves: "gadgets" } }, 
    { name: "everybody", loves: "sunshine" }, 
    { name: "judge", loves: [ "order", "policing", "weapons" ] },
    "someone"
  ]; 

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
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray1 : function () {
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[0]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray2 : function () {
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[99]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray3 : function () {
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[-1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray4 : function () {
      var result = AQL_EXECUTE("RETURN NOOPT(PASSTHRU({ foo: 'bar' }))[-1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray5 : function () {
      var result = AQL_EXECUTE("RETURN NOOPT(PASSTHRU(null))[-1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArray6 : function () {
      var result = AQL_EXECUTE("RETURN NOOPT(PASSTHRU('foobar'))[2]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArrayRange1 : function () {
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[0..1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testNonArrayRange2 : function () {
      var result = AQL_EXECUTE("RETURN { foo: 'bar' }[-2..-1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testV8NonArrayRange1 : function () {
      var result = AQL_EXECUTE("RETURN NOOPT(V8({ foo: 'bar' }))[0..1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-array access
////////////////////////////////////////////////////////////////////////////////

    testV8NonArrayRange2 : function () {
      var result = AQL_EXECUTE("RETURN NOOPT(V8({ foo: 'bar' }))[-2..-1]").json;
      assertEqual([ null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery result
////////////////////////////////////////////////////////////////////////////////

    testSubqueryResult : function () {
      for (var i = 0; i < values.length; ++i) {
        var result = AQL_EXECUTE("RETURN (FOR value IN @values RETURN value)[" + i + "]", { values: values }).json;
        assertEqual([ values[i] ], result);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range result
////////////////////////////////////////////////////////////////////////////////

    testSubqueryResultRangeForward1 : function () {
      for (var to = 0; to < 10; ++to) {
        var result = AQL_EXECUTE("RETURN (FOR value IN @values RETURN value)[0.." + to + "]", { values: values }).json;
        var expected = [ ];
        for (var i = 0; i < Math.min(to + 1, values.length); ++i) {
          expected.push(values[i]);
        }
        assertEqual([ expected ], result);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range result
////////////////////////////////////////////////////////////////////////////////

    testSubqueryResultRangeForward2 : function () {
      for (var from = 0; from < 10; ++from) {
        var result = AQL_EXECUTE("RETURN (FOR value IN @values RETURN value)[" + from + "..99]", { values: values }).json;
        var expected = [ ];
        for (var i = Math.min(from, values.length); i < values.length; ++i) {
          expected.push(values[i]);
        }
        assertEqual([ expected ], result);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range result
////////////////////////////////////////////////////////////////////////////////

    testSubqueryResultRangeForwardMisc : function () {
      var test = function (from, to, v) {
        var result = AQL_EXECUTE("RETURN (FOR value IN @values RETURN value)[" + from + ".." + to + "]", { values: values }).json;
        var expected = v.map(function(v) { return values[v]; });
        assertEqual([ expected ], result);
      };

      test(0, 0, [ 0 ]);
      test(0, 1, [ 0, 1 ]);
      test(0, 2, [ 0, 1, 2 ]);
      test(0, 3, [ 0, 1, 2, 3 ]);
      test(0, 4, [ 0, 1, 2, 3, 4 ]);
      test(0, 5, [ 0, 1, 2, 3, 4 ]);
      test(0, 6, [ 0, 1, 2, 3, 4 ]);
      test(0, 99, [ 0, 1, 2, 3, 4 ]);

      test(1, 1, [ 1 ]);
      test(1, 2, [ 1, 2 ]);
      test(1, 3, [ 1, 2, 3 ]);
      test(1, 4, [ 1, 2, 3, 4 ]);
      test(1, 5, [ 1, 2, 3, 4 ]);

      test(2, 2, [ 2 ]);
      test(2, 3, [ 2, 3 ]);

      test(4, 4, [ 4 ]);
      test(4, 5, [ 4 ]);

      test(5, 5, [ ]);
      test(5, 6, [ ]);
      test(1000, 1000, [ ]);
      test(1000000, 1000000, [ ]);
      
      test(0, -1, [ 0, 1, 2, 3, 4 ]);
      test(0, -2, [ 0, 1, 2, 3 ]);
      test(0, -3, [ 0, 1, 2 ]);
      test(0, -4, [ 0, 1 ]);
      test(0, -5, [ 0 ]);
      test(0, -6, [ 0 ]);
      test(0, -7, [ 0 ]);

      test(1, 0, [ 1, 0 ]);
      test(1, -1, [ 1, 2, 3, 4 ]);
      test(1, -2, [ 1, 2, 3 ]);
      test(1, -3, [ 1, 2 ]);
      test(1, -4, [ 1 ]);
      test(1, -5, [ 1, 0 ]);
      test(1, -6, [ 1, 0 ]);
      test(1, -7, [ 1, 0 ]);

      test(2, 0, [ 2, 1, 0 ]);
      test(2, 1, [ 2, 1 ]);
      test(2, -1, [ 2, 3, 4 ]);
      test(2, -2, [ 2, 3 ]);
      test(2, -3, [ 2 ]);
      test(2, -4, [ 2, 1 ]);
      test(2, -5, [ 2, 1, 0 ]);
      test(2, -6, [ 2, 1, 0 ]);
      test(2, -7, [ 2, 1, 0 ]);

      test(3, 0, [ 3, 2, 1, 0 ]);
      test(3, 1, [ 3, 2, 1 ]);
      test(3, 2, [ 3, 2 ]);
      test(3, -1, [ 3, 4 ]);
      test(3, -2, [ 3 ]);
      test(3, -3, [ 3, 2 ]);
      test(3, -4, [ 3, 2, 1 ]);
      test(3, -5, [ 3, 2, 1, 0 ]);
      test(3, -6, [ 3, 2, 1, 0 ]);

      test(4, 0, [ 4, 3, 2, 1, 0 ]);
      test(4, 1, [ 4, 3, 2, 1 ]);
      test(4, 2, [ 4, 3, 2 ]);
      test(4, 3, [ 4, 3 ]);
      test(4, -1, [ 4 ]);
      test(4, -2, [ 4, 3 ]);
      test(4, -3, [ 4, 3, 2 ]);
      test(4, -4, [ 4, 3, 2, 1 ]);
      test(4, -5, [ 4, 3, 2, 1, 0 ]);
      test(4, -6, [ 4, 3, 2, 1, 0 ]);

      test(5, 0, [ 4, 3, 2, 1, 0 ]);
      test(5, 1, [ 4, 3, 2, 1 ]);
      test(5, 2, [ 4, 3, 2 ]);
      test(5, 3, [ 4, 3 ]);
      test(5, -1, [ 4 ]);
      test(5, -2, [ 4, 3 ]);
      test(5, -3, [ 4, 3, 2 ]);
      test(5, -4, [ 4, 3, 2, 1 ]);
      test(5, -5, [ 4, 3, 2, 1, 0 ]);
      test(5, -6, [ 4, 3, 2, 1, 0 ]);
      
      test(6, 6, [ ]);
      test(6, 7, [ ]);
      test(7, 6, [ ]);
      test(10, 10, [ ]);
      test(100, 100, [ ]);
      test(100, 1000, [ ]);
      test(1000, 100, [ ]);

      test(-6, -6, [ ]);
      test(-6, -7, [ ]);
      test(-7, -6, [ ]);
      test(-10, -10, [ ]);
      test(-100, -100, [ ]);
      test(-100, -1000, [ ]);
      test(-1000, -100, [ ]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range result
////////////////////////////////////////////////////////////////////////////////

    testV8SubqueryResultRangeForward1 : function () {
      for (var to = 0; to < 10; ++to) {
        var result = AQL_EXECUTE("RETURN NOOPT(V8(FOR value IN @values RETURN value))[0.." + to + "]", { values: values }).json;
        var expected = [ ];
        for (var i = 0; i < Math.min(to + 1, values.length); ++i) {
          expected.push(values[i]);
        }
        assertEqual([ expected ], result);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range result
////////////////////////////////////////////////////////////////////////////////

    testV8SubqueryResultRangeForward2 : function () {
      for (var from = 0; from < 10; ++from) {
        var result = AQL_EXECUTE("RETURN NOOPT(V8(FOR value IN @values RETURN value))[" + from + "..99]", { values: values }).json;
        var expected = [ ];
        for (var i = Math.min(from, values.length); i < values.length; ++i) {
          expected.push(values[i]);
        }
        assertEqual([ expected ], result);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test range result
////////////////////////////////////////////////////////////////////////////////

    testV8SubqueryResultRangeForwardMisc : function () {
      var test = function (from, to, v) {
        var result = AQL_EXECUTE("RETURN NOOPT(V8(FOR value IN @values RETURN value))[" + from + ".." + to + "]", { values: values }).json;
        var expected = v.map(function(v) { return values[v]; });
        assertEqual([ expected ], result);
      };

      test(0, 0, [ 0 ]);
      test(0, 1, [ 0, 1 ]);
      test(0, 2, [ 0, 1, 2 ]);
      test(0, 3, [ 0, 1, 2, 3 ]);
      test(0, 4, [ 0, 1, 2, 3, 4 ]);
      test(0, 5, [ 0, 1, 2, 3, 4 ]);
      test(0, 6, [ 0, 1, 2, 3, 4 ]);
      test(0, 99, [ 0, 1, 2, 3, 4 ]);

      test(1, 1, [ 1 ]);
      test(1, 2, [ 1, 2 ]);
      test(1, 3, [ 1, 2, 3 ]);
      test(1, 4, [ 1, 2, 3, 4 ]);
      test(1, 5, [ 1, 2, 3, 4 ]);

      test(2, 2, [ 2 ]);
      test(2, 3, [ 2, 3 ]);

      test(4, 4, [ 4 ]);
      test(4, 5, [ 4 ]);

      test(5, 5, [ ]);
      test(5, 6, [ ]);
      test(1000, 1000, [ ]);
      test(1000000, 1000000, [ ]);
      
      test(0, -1, [ 0, 1, 2, 3, 4 ]);
      test(0, -2, [ 0, 1, 2, 3 ]);
      test(0, -3, [ 0, 1, 2 ]);
      test(0, -4, [ 0, 1 ]);
      test(0, -5, [ 0 ]);
      test(0, -6, [ 0 ]);
      test(0, -7, [ 0 ]);

      test(1, 0, [ 1, 0 ]);
      test(1, -1, [ 1, 2, 3, 4 ]);
      test(1, -2, [ 1, 2, 3 ]);
      test(1, -3, [ 1, 2 ]);
      test(1, -4, [ 1 ]);
      test(1, -5, [ 1, 0 ]);
      test(1, -6, [ 1, 0 ]);
      test(1, -7, [ 1, 0 ]);

      test(2, 0, [ 2, 1, 0 ]);
      test(2, 1, [ 2, 1 ]);
      test(2, -1, [ 2, 3, 4 ]);
      test(2, -2, [ 2, 3 ]);
      test(2, -3, [ 2 ]);
      test(2, -4, [ 2, 1 ]);
      test(2, -5, [ 2, 1, 0 ]);
      test(2, -6, [ 2, 1, 0 ]);
      test(2, -7, [ 2, 1, 0 ]);

      test(3, 0, [ 3, 2, 1, 0 ]);
      test(3, 1, [ 3, 2, 1 ]);
      test(3, 2, [ 3, 2 ]);
      test(3, -1, [ 3, 4 ]);
      test(3, -2, [ 3 ]);
      test(3, -3, [ 3, 2 ]);
      test(3, -4, [ 3, 2, 1 ]);
      test(3, -5, [ 3, 2, 1, 0 ]);
      test(3, -6, [ 3, 2, 1, 0 ]);

      test(4, 0, [ 4, 3, 2, 1, 0 ]);
      test(4, 1, [ 4, 3, 2, 1 ]);
      test(4, 2, [ 4, 3, 2 ]);
      test(4, 3, [ 4, 3 ]);
      test(4, -1, [ 4 ]);
      test(4, -2, [ 4, 3 ]);
      test(4, -3, [ 4, 3, 2 ]);
      test(4, -4, [ 4, 3, 2, 1 ]);
      test(4, -5, [ 4, 3, 2, 1, 0 ]);
      test(4, -6, [ 4, 3, 2, 1, 0 ]);

      test(5, 0, [ 4, 3, 2, 1, 0 ]);
      test(5, 1, [ 4, 3, 2, 1 ]);
      test(5, 2, [ 4, 3, 2 ]);
      test(5, 3, [ 4, 3 ]);
      test(5, -1, [ 4 ]);
      test(5, -2, [ 4, 3 ]);
      test(5, -3, [ 4, 3, 2 ]);
      test(5, -4, [ 4, 3, 2, 1 ]);
      test(5, -5, [ 4, 3, 2, 1, 0 ]);
      test(5, -6, [ 4, 3, 2, 1, 0 ]);
      
      test(6, 6, [ ]);
      test(6, 7, [ ]);
      test(7, 6, [ ]);
      test(10, 10, [ ]);
      test(100, 100, [ ]);
      test(100, 1000, [ ]);
      test(1000, 100, [ ]);

      test(-6, -6, [ ]);
      test(-6, -7, [ ]);
      test(-7, -6, [ ]);
      test(-10, -10, [ ]);
      test(-100, -100, [ ]);
      test(-100, -1000, [ ]);
      test(-1000, -100, [ ]);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(arrayAccessTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
