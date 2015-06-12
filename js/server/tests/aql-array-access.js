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
/// @brief test subquery result
////////////////////////////////////////////////////////////////////////////////

    testSubqueryResult : function () {
      for (var i = 0; i < values.length; ++i) {
        var result = AQL_EXECUTE("RETURN (FOR value IN @values RETURN value)[" + i + "]", { values: values }).json;
        assertEqual([ values[i] ], result);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery result
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
/// @brief test subquery result
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
