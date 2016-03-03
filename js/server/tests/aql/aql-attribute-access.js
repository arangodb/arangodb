/*jshint globalstrict:false, strict:false, maxlen: 700 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, attribute accesses
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

function attributeAccessTestSuite () {
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
/// @brief test direct access
////////////////////////////////////////////////////////////////////////////////

    testDirectAccess : function () {
      var result = AQL_EXECUTE("RETURN " + JSON.stringify(values[0]) + ".name").json;
      assertEqual([ "sir alfred" ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test direct access
////////////////////////////////////////////////////////////////////////////////

    testDirectAccessBind : function () {
      var result = AQL_EXECUTE("RETURN @value.name", { value: values[0] }).json;
      assertEqual([ "sir alfred" ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-existing attribute
////////////////////////////////////////////////////////////////////////////////

    testNonExistingAttribute : function () {
      var result = AQL_EXECUTE("FOR value IN @values RETURN value.broken", { values: values }).json;
      assertEqual([ null, null, null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test partially existing attribute
////////////////////////////////////////////////////////////////////////////////

    testPartiallyExistingAttribute : function () {
      var result = AQL_EXECUTE("FOR value IN @values RETURN value.name", { values: values }).json;
      assertEqual([ "sir alfred", null, "everybody", "judge", null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test partially existing sub-attribute
////////////////////////////////////////////////////////////////////////////////

    testPartiallyExistingSubAttribute : function () {
      var result = AQL_EXECUTE("FOR value IN @values RETURN value.person.name", { values: values }).json;
      assertEqual([ null, "gadgetto", null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test function call result attribute
////////////////////////////////////////////////////////////////////////////////

    testFunctionCallResultAttribute : function () {
      var result = AQL_EXECUTE("FOR value IN @values RETURN NOOPT(PASSTHRU(value.age))", { values: values }).json;
      assertEqual([ 60, null, null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test function call result sub-attribute
////////////////////////////////////////////////////////////////////////////////

    testFunctionCallResultAttribute1 : function () {
      var result = AQL_EXECUTE("FOR value IN @values RETURN NOOPT(PASSTHRU(value.person.name))", { values: values }).json;
      assertEqual([ null, "gadgetto", null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test function call result sub-attribute
////////////////////////////////////////////////////////////////////////////////

    testFunctionCallResultAttribute2 : function () {
      var result = AQL_EXECUTE("FOR value IN @values RETURN NOOPT(PASSTHRU(value).person).name", { values: values }).json;
      assertEqual([ null, "gadgetto", null, null, null ], result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test subquery result attribute
////////////////////////////////////////////////////////////////////////////////

    testSubqueryResultAttribute : function () {
      // won't work (accessing an attribute of an array)
      var result = AQL_EXECUTE("RETURN (FOR value IN @values RETURN value).name", { values: values }).json;
      assertEqual([ null ], result);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(attributeAccessTestSuite);

return jsunity.done();

