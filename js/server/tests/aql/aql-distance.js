/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global AQL_EXECUTE, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, geo queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simran Brucherseifer
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");
var assertQueryError = helper.assertQueryError;
var assertQueryWarningAndNull = helper.assertQueryWarningAndNull;

function assertAlmostEqual(a, b, text) {
  if (typeof(a) === 'number') {
    a = a.toPrecision(7);
  }
  if (typeof(b) === 'number') {
    b = b.toPrecision(7);
  }
  if (((a === 0) && (b === 0.0))||
      ((b === 0) && (a === 0.0))) {
    return;
  }

  assertEqual(a, b, text);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function distanceSuite () {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test DISTANCE function
////////////////////////////////////////////////////////////////////////////////

    testDistance1 : function () {
      var co1 = { lat: 52.5163, lon: 13.3777, _key: "BrandenburgGate" };
      var co2 = { lat: 50.9322, lon: 6.94, _key: "ArangoHQ" };
      var query = "DISTANCE(" + co1.lat + "," + co1.lon + "," + co2.lat + "," + co2.lon + ")";
      var expected    = [ 476918.89688380965  ]; // Vincenty's formula: 477.47 km

      var actual = AQL_EXECUTE("RETURN " + query).json;
      assertAlmostEqual(expected[0], actual[0]);
      
      actual = AQL_EXECUTE("RETURN NOOPT(" + query + ")").json;
      assertAlmostEqual(expected[0], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test DISTANCE function
////////////////////////////////////////////////////////////////////////////////

    testDistance2 : function () {
      var co1 = { lat: 40.78, lon: -73.97, _key: "CentralPark" };
      var co2 = { lat: 34.05, lon: -118.25, _key: "LosAngeles" };
      var query = "DISTANCE(" + co1.lat + "," + co1.lon + "," + co2.lat + "," + co2.lon + ")";
      var expected = [ 3938926.7382122413 ]; // Vincenty's formula: 3943.29 km
      
      var actual = AQL_EXECUTE("RETURN " + query).json;
      assertAlmostEqual(expected[0], actual[0]);
      
      actual = AQL_EXECUTE("RETURN NOOPT(" + query + ")").json;
      assertAlmostEqual(expected[0], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test DISTANCE function
////////////////////////////////////////////////////////////////////////////////

    testDistance3 : function () {
      var co1 = { lat: 0, lon: 0, _key: "zeroPoint" };
      var co2 = { lat: 0, lon: 180, _key: "otherSideOfGLobe" };
      var query = "DISTANCE(" + co1.lat + "," + co1.lon + "," + co2.lat + "," + co2.lon + ")";
      var expected = [ 20015086.79602057 ]; // Half of equatorial circumference (WGS 84): 20037.5085 km
      
      var actual = AQL_EXECUTE("RETURN " + query).json;
      assertAlmostEqual(expected[0], actual[0]);
      
      actual = AQL_EXECUTE("RETURN NOOPT(" + query + ")").json;
      assertAlmostEqual(expected[0], actual[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid DISTANCE arguments
////////////////////////////////////////////////////////////////////////////////

    testDistanceInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DISTANCE()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DISTANCE( 0 )");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DISTANCE( 0, 0 )");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DISTANCE( 0, 0, 0 )");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DISTANCE( 0, 0, 0, 0, 0 )");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DISTANCE( 0, 0, 0, \"foo\" )");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DISTANCE( 0, 0, 0, [ 1, 2, 3 ] )");
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(distanceSuite);

return jsunity.done();

