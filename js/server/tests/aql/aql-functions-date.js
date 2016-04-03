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

function ahuacatlDateFunctionsTestSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test is_datestring function
////////////////////////////////////////////////////////////////////////////////

    testIsDateString : function () {
      var values = [
        [ "2000-04-29", true ],
        [ "2000-04-29Z", true ],
        [ "2012-02-12 13:24:12", true ],
        [ "2012-02-12 13:24:12Z", true ],
        [ "2012-02-12 23:59:59.991", true ],
        [ "2012-02-12 23:59:59.991Z", true ],
        [ "2012-02-12", true ],
        [ "2012-02-12Z", true ],
        [ "2012-02-12T13:24:12Z", true ],
        [ "2012-02-12Z", true ],
        [ "2012-2-12Z", true ],
        [ "1910-01-02T03:04:05Z", true ],
        [ "1910-01-02 03:04:05Z", true ],
        [ "1910-01-02", true ],
        [ "1910-01-02Z", true ],
        [ "1970-01-01T01:05:27", true ],
        [ "1970-01-01T01:05:27Z", true ],
        [ "1970-01-01 01:05:27Z", true ],
        [ "1970-1-1Z", true ],
        [ "1970-1-1", true ],
        [ "1221-02-28T23:59:59Z", true ],
        [ "1221-02-28 23:59:59Z", true ],
        [ "1221-02-28Z", true ],
        [ "1221-2-28Z", true ],
        [ "1000-12-24T04:12:00Z", true ],
        [ "1000-12-24Z", true ],
        [ "1000-12-24 04:12:00Z", true ],
        [ "6789-12-31T23:59:58.99Z", true ],
        [ "6789-12-31Z", true ],
        [ "9999-12-31T23:59:59.999Z", true ],
        [ "9999-12-31Z", true ],
        [ "9999-12-31z", true ],
        [ "9999-12-31", true ],
        [ "2012Z", true ],
        [ "2012z", true ],
        [ "2012", true ],
        [ "2012-1Z", true ],
        [ "2012-1z", true ],
        [ "2012-1-1z", true ],
        [ "2012-01-01Z", true ],
        [ "2012-01-01Z", true ],
        [ "  2012-01-01Z", true ],
        [ "  2012-01-01z", true ],
        [ "foo2012-01-01z", false ],
        [ "2012-01-01foo", false ],
        [ "foo", false ],
        [ "bar", false ],
        [ "2015-foobar", false ],
        [ "", false ],
        [ -95674000, false ],
        [ 1399395674000, false ],
        [ 60123, false ],
        [ 1, false ],
        [ 0, false ],
        [ -4, false ],
        [ true, false ],
        [ false, false ],
        [ null, false ],
        [ [ ], false ],
        [ [ 1 ], false ],
        [ [ "foo" ], false ],
        [ [ "foo", "bar" ], false ],
        [ [ "2015-01-23" ], false ],
        [ { }, false ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN IS_DATESTRING(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual, value);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test is_datestring function
////////////////////////////////////////////////////////////////////////////////

    testIsDateStringInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_DATESTRING()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_DATESTRING('foo', 'bar')");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_now function
////////////////////////////////////////////////////////////////////////////////

    testDateNowInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_NOW(1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_NOW(1, 1)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_now function
////////////////////////////////////////////////////////////////////////////////

    testDateNow : function () {
      var actual = getQueryResults("RETURN IS_NUMBER(DATE_NOW())")[0];

      assertEqual(true, actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_dayofweek function
////////////////////////////////////////////////////////////////////////////////

    testDateDayOfWeekInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFWEEK()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFWEEK(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAYOFWEEK(null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAYOFWEEK(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAYOFWEEK([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAYOFWEEK({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_dayofweek function
////////////////////////////////////////////////////////////////////////////////

    testDateDayOfWeek : function () {
      var values = [
        [ "2000-04-29", 6 ],
        [ "2000-04-29Z", 6 ],
        [ "2012-02-12 13:24:12", 0 ],
        [ "2012-02-12 13:24:12Z", 0 ],
        [ "2012-02-12 23:59:59.991", 0 ],
        [ "2012-02-12 23:59:59.991Z", 0 ],
        [ "2012-02-12", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12Z", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05Z", 0 ],
        [ "1910-01-02 03:04:05Z", 0 ],
        [ "1910-01-02", 0 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27", 4 ],
        [ "1970-01-01T01:05:27Z", 4 ],
        [ "1970-01-01 01:05:27Z", 4 ],
        [ "1970-1-1Z", 4 ],
        [ "1221-02-28T23:59:59Z", 0 ],
        [ "1221-02-28 23:59:59Z", 0 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 3 ],
        [ "1000-12-24Z", 3 ],
        [ "1000-12-24 04:12:00Z", 3 ],
        [ "6789-12-31T23:59:58.99Z", 0 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 5 ],
        [ "9999-12-31Z", 5 ],
        [ "9999-12-31z", 5 ],
        [ "9999-12-31", 5 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 2 ],
        [ 60123, 4 ],
        [ 1, 4 ],
        [ 0, 4 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_DAYOFWEEK(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_year function
////////////////////////////////////////////////////////////////////////////////

    testDateYearInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_YEAR()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_YEAR(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_YEAR(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_YEAR(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_YEAR([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_YEAR({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_year function
////////////////////////////////////////////////////////////////////////////////

    testDateYear : function () {
      var values = [
        [ "2000-04-29Z", 2000 ],
        [ "2012-02-12 13:24:12Z", 2012 ],
        [ "2012-02-12 23:59:59.991Z", 2012 ],
        [ "2012-02-12Z", 2012 ],
        [ "2012-02-12T13:24:12Z", 2012 ],
        [ "2012-02-12Z", 2012 ],
        [ "2012-2-12Z", 2012 ],
        [ "1910-01-02T03:04:05Z", 1910 ],
        [ "1910-01-02 03:04:05Z", 1910 ],
        [ "1910-01-02Z", 1910 ],
        [ "1970-01-01T01:05:27Z", 1970 ],
        [ "1970-01-01 01:05:27Z", 1970 ],
        [ "1970-1-1Z", 1970 ],
        [ "1221-02-28T23:59:59Z", 1221 ],
        [ "1221-02-28 23:59:59Z", 1221 ],
        [ "1221-02-28Z", 1221 ],
        [ "1221-2-28Z", 1221 ],
        [ "1000-12-24T04:12:00Z", 1000 ],
        [ "1000-12-24Z", 1000 ],
        [ "1000-12-24 04:12:00Z", 1000 ],
        [ "6789-12-31T23:59:58.99Z", 6789 ],
        [ "6789-12-31Z", 6789 ],
        [ "9999-12-31T23:59:59.999Z", 9999 ],
        [ "9999-12-31Z", 9999 ],
        [ "9999-12-31z", 9999 ],
        [ "9999-12-31", 9999 ],
        [ "2012Z", 2012 ],
        [ "2012z", 2012 ],
        [ "2012", 2012 ],
        [ "2012-1Z", 2012 ],
        [ "2012-1z", 2012 ],
        [ "2012-1-1z", 2012 ],
        [ "2012-01-01Z", 2012 ],
        [ "2012-01-01Z", 2012 ],
        [ "  2012-01-01Z", 2012 ],
        [ "  2012-01-01z", 2012 ],
        [ 1399395674000, 2014 ],
        [ 60123, 1970 ],
        [ 1, 1970 ],
        [ 0, 1970 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_YEAR(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_month function
////////////////////////////////////////////////////////////////////////////////

    testDateMonthInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MONTH()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MONTH(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MONTH(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MONTH(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MONTH([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MONTH({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_month function
////////////////////////////////////////////////////////////////////////////////

    testDateMonth : function () {
      var values = [
        [ "2000-04-29Z", 4 ],
        [ "2012-02-12 13:24:12Z", 2 ],
        [ "2012-02-12 23:59:59.991Z", 2 ],
        [ "2012-02-12Z", 2 ],
        [ "2012-02-12T13:24:12Z", 2 ],
        [ "2012-02-12Z", 2 ],
        [ "2012-2-12Z", 2 ],
        [ "1910-01-02T03:04:05Z", 1 ],
        [ "1910-01-02 03:04:05Z", 1 ],
        [ "1910-01-02Z", 1 ],
        [ "1970-01-01T01:05:27Z", 1 ],
        [ "1970-01-01 01:05:27Z", 1 ],
        [ "1970-1-1Z", 1 ],
        [ "1221-02-28T23:59:59Z", 2 ],
        [ "1221-02-28 23:59:59Z", 2 ],
        [ "1221-02-28Z", 2 ],
        [ "1221-2-28Z", 2 ],
        [ "1000-12-24T04:12:00Z", 12 ],
        [ "1000-12-24Z", 12 ],
        [ "1000-12-24 04:12:00Z", 12 ],
        [ "6789-12-31T23:59:58.99Z", 12 ],
        [ "6789-12-31Z", 12 ],
        [ "9999-12-31T23:59:59.999Z", 12 ],
        [ "9999-12-31Z", 12 ],
        [ "9999-12-31z", 12 ],
        [ "9999-12-31", 12 ],
        [ "2012Z", 1 ],
        [ "2012z", 1 ],
        [ "2012", 1 ],
        [ "2012-2Z", 2 ],
        [ "2012-1Z", 1 ],
        [ "2012-1z", 1 ],
        [ "2012-1-1z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "  2012-01-01Z", 1 ],
        [ "  2012-01-01z", 1 ],
        [ 1399395674000, 5 ],
        [ 60123, 1 ],
        [ 1, 1 ],
        [ 0, 1 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_MONTH(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_day function
////////////////////////////////////////////////////////////////////////////////

    testDateDayInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAY()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAY(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAY(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAY(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAY([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAY({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_day function
////////////////////////////////////////////////////////////////////////////////

    testDateDay : function () {
      var values = [
        [ "2000-04-29Z", 29 ],
        [ "2012-02-12 13:24:12Z", 12 ],
        [ "2012-02-12 23:59:59.991Z", 12 ],
        [ "2012-02-12Z", 12 ],
        [ "2012-02-12T13:24:12Z", 12 ],
        [ "2012-02-12Z", 12 ],
        [ "2012-2-12Z", 12 ],
        [ "1910-01-02T03:04:05Z", 2 ],
        [ "1910-01-02 03:04:05Z", 2 ],
        [ "1910-01-02Z", 2 ],
        [ "1970-01-01T01:05:27Z", 1 ],
        [ "1970-01-01 01:05:27Z", 1 ],
        [ "1970-1-1Z", 1 ],
        [ "1221-02-28T23:59:59Z", 28 ],
        [ "1221-02-28 23:59:59Z", 28 ],
        [ "1221-02-28Z", 28 ],
        [ "1221-2-28Z", 28 ],
        [ "1000-12-24T04:12:00Z", 24 ],
        [ "1000-12-24Z", 24 ],
        [ "1000-12-24 04:12:00Z", 24 ],
        [ "6789-12-31T23:59:58.99Z", 31 ],
        [ "6789-12-31Z", 31 ],
        [ "9999-12-31T23:59:59.999Z", 31 ],
        [ "9999-12-31Z", 31 ],
        [ "9999-12-31z", 31 ],
        [ "9999-12-31", 31 ],
        [ "2012Z", 1 ],
        [ "2012z", 1 ],
        [ "2012", 1 ],
        [ "2012-2Z", 1 ],
        [ "2012-1Z", 1 ],
        [ "2012-1z", 1 ],
        [ "2012-1-1z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "2012-01-02Z", 2 ],
        [ "2012-01-2Z", 2 ],
        [ "  2012-01-01Z", 1 ],
        [ "  2012-01-01z", 1 ],
        [ 1399395674000, 6 ],
        [ 60123, 1 ],
        [ 1, 1 ],
        [ 0, 1 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_DAY(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_hour function
////////////////////////////////////////////////////////////////////////////////

    testDateHourInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_HOUR()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_HOUR(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_HOUR(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_HOUR(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_HOUR([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_HOUR({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_hour function
////////////////////////////////////////////////////////////////////////////////

    testDateHour : function () {
      var values = [
        [ "2000-04-29", 0 ],
        [ "2000-04-29Z", 0 ],
        [ "2012-02-12 13:24:12", 13 ],
        [ "2012-02-12 13:24:12Z", 13 ],
        [ "2012-02-12 23:59:59.991Z", 23 ],
        [ "2012-02-12", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12", 13 ],
        [ "2012-02-12T13:24:12Z", 13 ],
        [ "2012-02-12", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05", 3 ],
        [ "1910-01-02T03:04:05Z", 3 ],
        [ "1910-01-02 03:04:05Z", 3 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27Z", 1 ],
        [ "1970-01-01 01:05:27Z", 1 ],
        [ "1970-01-01T12:05:27Z", 12 ],
        [ "1970-01-01 12:05:27Z", 12 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", 23 ],
        [ "1221-02-28 23:59:59Z", 23 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 4 ],
        [ "1000-12-24Z", 0 ],
        [ "1000-12-24 04:12:00Z", 4 ],
        [ "6789-12-31T23:59:58.99Z", 23 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 23 ],
        [ "9999-12-31Z", 0 ],
        [ "9999-12-31z", 0 ],
        [ "9999-12-31", 0 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-2Z", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-02Z", 0 ],
        [ "2012-01-2Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 17 ],
        [ 3600000, 1 ],
        [ 7200000, 2 ],
        [ 8200000, 2 ],
        [ 60123, 0 ],
        [ 1, 0 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_HOUR(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_minute function
////////////////////////////////////////////////////////////////////////////////

    testDateMinuteInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MINUTE()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MINUTE(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MINUTE(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MINUTE(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MINUTE([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MINUTE({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_minute function
////////////////////////////////////////////////////////////////////////////////

    testDateMinute : function () {
      var values = [
        [ "2000-04-29Z", 0 ],
        [ "2012-02-12 13:24:12Z", 24 ],
        [ "2012-02-12 23:59:59.991Z", 59 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12Z", 24 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05Z", 4 ],
        [ "1910-01-02 03:04:05Z", 4 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27Z", 5 ],
        [ "1970-01-01 01:05:27Z", 5 ],
        [ "1970-01-01T12:05:27Z", 5 ],
        [ "1970-01-01 12:05:27Z", 5 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", 59 ],
        [ "1221-02-28 23:59:59Z", 59 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 12 ],
        [ "1000-12-24Z", 0 ],
        [ "1000-12-24 04:12:00Z", 12 ],
        [ "6789-12-31T23:59:58.99Z", 59 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 59 ],
        [ "9999-12-31Z", 0 ],
        [ "9999-12-31z", 0 ],
        [ "9999-12-31", 0 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-2Z", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-02Z", 0 ],
        [ "2012-01-2Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 1 ],
        [ 3600000, 0 ],
        [ 7200000, 0 ],
        [ 8200000, 16 ],
        [ 60123, 1 ],
        [ 1, 0 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_MINUTE(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_second function
////////////////////////////////////////////////////////////////////////////////

    testDateSecondInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_SECOND()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_SECOND(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_SECOND(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_SECOND(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_SECOND([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_SECOND({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_second function
////////////////////////////////////////////////////////////////////////////////

    testDateSecond : function () {
      var values = [
        [ "2000-04-29Z", 0 ],
        [ "2012-02-12 13:24:12Z", 12 ],
        [ "2012-02-12 23:59:59.991Z", 59 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12Z", 12 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05Z", 5 ],
        [ "1910-01-02 03:04:05Z", 5 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27Z", 27 ],
        [ "1970-01-01 01:05:27Z", 27 ],
        [ "1970-01-01T12:05:27Z", 27 ],
        [ "1970-01-01 12:05:27Z", 27 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", 59 ],
        [ "1221-02-28 23:59:59Z", 59 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 0 ],
        [ "1000-12-24Z", 0 ],
        [ "1000-12-24 04:12:00Z", 0 ],
        [ "6789-12-31T23:59:58.99Z", 58 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 59 ],
        [ "9999-12-31Z", 0 ],
        [ "9999-12-31z", 0 ],
        [ "9999-12-31", 0 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-2Z", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-02Z", 0 ],
        [ "2012-01-2Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 14 ],
        [ 3600000, 0 ],
        [ 7200000, 0 ],
        [ 8200000, 40 ],
        [ 61123, 1 ],
        [ 60123, 0 ],
        [ 2001, 2 ],
        [ 2000, 2 ],
        [ 1000, 1 ],
        [ 1, 0 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_SECOND(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_millisecond function
////////////////////////////////////////////////////////////////////////////////

    testDateMillisecondInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MILLISECOND()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MILLISECOND(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MILLISECOND(null)"); 
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MILLISECOND(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MILLISECOND([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_MILLISECOND({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_millisecond function
////////////////////////////////////////////////////////////////////////////////

    testDateMillisecond : function () {
      var values = [
        [ "2000-04-29Z", 0 ],
        [ "2012-02-12 13:24:12Z", 0 ],
        [ "2012-02-12 23:59:59.991Z", 991 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-02-12T13:24:12Z", 0 ],
        [ "2012-02-12Z", 0 ],
        [ "2012-2-12Z", 0 ],
        [ "1910-01-02T03:04:05Z", 0 ],
        [ "1910-01-02 03:04:05Z", 0 ],
        [ "1910-01-02Z", 0 ],
        [ "1970-01-01T01:05:27Z", 0 ],
        [ "1970-01-01 01:05:27Z", 0 ],
        [ "1970-01-01T12:05:27Z", 0 ],
        [ "1970-01-01 12:05:27Z", 0 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", 0 ],
        [ "1221-02-28 23:59:59Z", 0 ],
        [ "1221-02-28Z", 0 ],
        [ "1221-2-28Z", 0 ],
        [ "1000-12-24T04:12:00Z", 0 ],
        [ "1000-12-24Z", 0 ],
        [ "1000-12-24 04:12:00Z", 0 ],
        [ "6789-12-31T23:59:58.99Z", 990 ],
        [ "6789-12-31Z", 0 ],
        [ "9999-12-31T23:59:59.999Z", 999 ],
        [ "9999-12-31Z", 0 ],
        [ "9999-12-31z", 0 ],
        [ "9999-12-31", 0 ],
        [ "2012Z", 0 ],
        [ "2012z", 0 ],
        [ "2012", 0 ],
        [ "2012-2Z", 0 ],
        [ "2012-1Z", 0 ],
        [ "2012-1z", 0 ],
        [ "2012-1-1z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-01Z", 0 ],
        [ "2012-01-02Z", 0 ],
        [ "2012-01-2Z", 0 ],
        [ "  2012-01-01Z", 0 ],
        [ "  2012-01-01z", 0 ],
        [ 1399395674000, 0 ],
        [ 1399395674123, 123 ],
        [ 3600000, 0 ],
        [ 7200000, 0 ],
        [ 8200000, 0 ],
        [ 8200040, 40 ],
        [ 61123, 123 ],
        [ 60123, 123 ],
        [ 2001, 1 ],
        [ 2000, 0 ],
        [ 1000, 0 ],
        [ 753, 753 ],
        [ 53, 53 ],
        [ 1, 1 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_MILLISECOND(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

// TODO: verify all assertions of the following functions below are actually correct:
// DATE_DAYOFYEAR(), DATE_ISOWEEK(), DATE_LEAPYEAR(), DATE_QUARTER(), DATE_ADD(),
// DATE_SUBTRACT(), DATE_DIFF(). Unaffected should be: DATE_COMPARE(), DATE_FORMAT().
////////////////////////////////////////////////////////////////////////////////
/// @brief test date_dayofyear function
////////////////////////////////////////////////////////////////////////////////

    testDateDayOfYearInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFYEAR()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFYEAR(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAYOFYEAR(null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAYOFYEAR(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAYOFYEAR([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DAYOFYEAR({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_dayofyear function
////////////////////////////////////////////////////////////////////////////////

    testDateDayOfYear : function () {
      var values = [
        [ "2000-04-29", 120 ],
        [ "2000-04-29Z", 120 ],
        [ "2000-12-31", 366 ],
        [ "2000-12-31Z", 366 ],
        [ "2100-12-31", 365 ],
        [ "2100-12-31Z", 365 ],
        [ "2400-12-31", 366 ],
        [ "2400-12-31Z", 366 ],
        [ "2012-02-12 13:24:12", 43 ],
        [ "2012-02-12 13:24:12Z", 43 ],
        [ "2012-02-12 23:59:59.991", 43 ],
        [ "2012-02-12 23:59:59.991Z", 43 ],
        [ "2012-02-12", 43 ],
        [ "2012-02-12Z", 43 ],
        [ "2012-02-12T13:24:12Z", 43 ],
        [ "2012-02-12Z", 43 ],
        [ "2012-2-12Z", 43 ],
        [ "1910-01-02T03:04:05Z", 2 ],
        [ "1910-01-02 03:04:05Z", 2 ],
        [ "1910-01-02", 2 ],
        [ "1910-01-02Z", 2 ],
        [ "1970-01-01T01:05:27", 1 ],
        [ "1970-01-01T01:05:27Z", 1 ],
        [ "1970-01-01 01:05:27Z", 1 ],
        [ "1970-1-1Z", 1 ],
        [ "1221-02-28T23:59:59Z", 59 ],
        [ "1221-02-28 23:59:59Z", 59 ],
        [ "1221-02-28Z", 59 ],
        [ "1221-2-28Z", 59 ],
        /* 1000 was historically a leap year, but we do Gregorian calendar here */
        [ "1000-12-24T04:12:00Z", 358 ],
        [ "1000-12-24Z", 358 ],
        [ "1000-12-24 04:12:00Z", 358 ],
        [ "3456-12-31T23:59:58.99Z", 366 ],
        [ "3456-12-31Z", 366 ],
        [ "9999-12-31T23:59:59.999Z", 365 ],
        [ "9999-12-31Z", 365 ],
        [ "9999-12-31z", 365 ],
        [ "9999-12-31", 365 ],
        [ "2012Z", 1 ],
        [ "2012z", 1 ],
        [ "2012", 1 ],
        [ "2012-1Z", 1 ],
        [ "2012-1z", 1 ],
        [ "2012-1-1z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "2012-01-01Z", 1 ],
        [ "  2012-01-01Z", 1 ],
        [ "  2012-01-01z", 1 ],
        [ 1399395674000, 126 ],
        [ 60123, 1 ],
        [ 1, 1 ],
        [ 0, 1 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_DAYOFYEAR(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_isoweek function
////////////////////////////////////////////////////////////////////////////////

    testDateISOWeekInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISOWEEK()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISOWEEK(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISOWEEK(null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISOWEEK(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISOWEEK([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISOWEEK({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_isoweek function
////////////////////////////////////////////////////////////////////////////////

    testDateISOWeek : function () {
      var values = [
        [ "2000-04-29", 17 ],
        [ "2000-04-29Z", 17 ],
        [ "2000-12-31", 52 ],
        [ "2000-12-31Z", 52 ],
        [ "2100-12-31", 52 ],
        [ "2100-12-31Z", 52 ],
        [ "2400-12-31", 52 ],
        [ "2400-12-31Z", 52 ],
        [ "2012-02-12 13:24:12", 6 ],
        [ "2012-02-12 13:24:12Z", 6 ],
        [ "2012-02-12 23:59:59.991", 6 ],
        [ "2012-02-12 23:59:59.991Z", 6 ],
        [ "2012-02-12", 6 ],
        [ "2012-02-12Z", 6 ],
        [ "2012-02-12T13:24:12Z", 6 ],
        [ "2012-02-12Z", 6 ],
        [ "2012-2-12Z", 6 ],
        [ "1910-01-02T03:04:05Z", 52 ],
        [ "1910-01-02 03:04:05Z", 52 ],
        [ "1910-01-02", 52 ],
        [ "1910-01-02Z", 52 ],
        [ "1970-01-01T01:05:27", 1 ],
        [ "1970-01-01T01:05:27Z", 1 ],
        [ "1970-01-01 01:05:27Z", 1 ],
        [ "1970-1-1Z", 1 ],
        [ "1221-02-28T23:59:59Z", 8 ],
        [ "1221-02-28 23:59:59Z", 8 ],
        [ "1221-02-28Z", 8 ],
        [ "1221-2-28Z", 8 ],
        [ "1000-12-24T04:12:00Z", 52 ],
        [ "1000-12-24Z", 52 ],
        [ "1000-12-24 04:12:00Z", 52 ],
        [ "2016Z", 53 ],
        [ "2016z", 53 ],
        [ "2016", 53 ],
        [ "2016-1Z", 53 ],
        [ "2016-1z", 53 ],
        [ "2016-1-1z", 53 ],
        [ "2016-01-01Z", 53 ],
        [ "2016-01-01Z", 53 ],
        [ "  2016-01-01Z", 53 ],
        [ "  2016-01-01z", 53 ],
        [ 1399395674000, 19 ],
        [ 60123, 1 ],
        [ 1, 1 ],
        [ 0, 1 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_ISOWEEK(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_leapyear function
////////////////////////////////////////////////////////////////////////////////

    testDateLeapYearInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_LEAPYEAR()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_LEAPYEAR(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_LEAPYEAR(null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_LEAPYEAR(false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_LEAPYEAR([])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_LEAPYEAR({})");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_leapyear function
////////////////////////////////////////////////////////////////////////////////

    testDateLeapYear : function () {
      var values = [
        [ "2000-04-29", true ],
        [ "2000-04-29Z", true ],
        [ "2000-12-31", true ],
        [ "2000-12-31Z", true ],
        [ "2100-12-31", false ],
        [ "2100-12-31Z", false ],
        [ "2400-12-31", true ],
        [ "2400-12-31Z", true ],
        [ "2012-02-12 13:24:12", true ],
        [ "2012-02-12 13:24:12Z", true ],
        [ "2012-02-12 23:59:59.991", true ],
        [ "2012-02-12 23:59:59.991Z", true ],
        [ "2012-02-12", true ],
        [ "2012-02-12Z", true ],
        [ "2012-02-12T13:24:12Z", true ],
        [ "2012-02-12Z", true ],
        [ "2012-2-12Z", true ],
        [ "1910-01-02T03:04:05Z", false ],
        [ "1910-01-02 03:04:05Z", false ],
        [ "1910-01-02", false ],
        [ "1910-01-02Z", false ],
        [ "1970-01-01T01:05:27", false ],
        [ "1970-01-01T01:05:27Z", false ],
        [ "1970-01-01 01:05:27Z", false ],
        [ "1970-1-1Z", false ],
        [ "1221-02-28T23:59:59Z", false ],
        [ "1221-02-28 23:59:59Z", false ],
        [ "1221-02-28Z", false ],
        [ "1221-2-28Z", false ],
        [ "1000-12-24T04:12:00Z", false ],
        [ "1000-12-24Z", false ],
        [ "1000-12-24 04:12:00Z", false ],
        [ "2016Z", true ],
        [ "2016z", true ],
        [ "2016", true ],
        [ "2016-1Z", true ],
        [ "2016-1z", true ],
        [ "2016-1-1z", true ],
        [ "2016-01-01Z", true ],
        [ "2016-01-01Z", true ],
        [ "  2016-01-01Z", true ],
        [ "  2016-01-01z", true ],
        [ 1399395674000, false ],
        [ 60123, false ],
        [ 1, false ],
        [ 0, false ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_LEAPYEAR(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      });
    },

// TODO: DATE_QUARTER()
// TODO: DATE_DAYS_IN_MONTH()

// TODO: additional ISO duration tests for DATE_ADD() / DATE_SUBTRACT()
////////////////////////////////////////////////////////////////////////////////
/// @brief test date_add function
////////////////////////////////////////////////////////////////////////////////

    testDateAddInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ADD()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ADD(1, 1, 1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(1, 1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(1, 'P1Y', 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ADD(null, 1, 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ADD(false, 1, 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ADD([], 1, 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ADD({}, 1, 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ADD(DATE_NOW(), 1, 'sugar')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ADD(DATE_NOW(), 1, '')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), '', 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), '1', 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), 'one', 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), null, 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), false, 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), [], 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), {}, 'year')");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_add function
////////////////////////////////////////////////////////////////////////////////

    testDateAdd : function () {
      var values = [
        [ ["2000-04-29", 2, "days"], "2000-05-01T00:00:00.000Z" ],
        [ ["2000-04-29Z", "P2D"], "2000-05-01T00:00:00.000Z" ],
        [ ["2000-04-29Z", 2, "days"], "2000-05-01T00:00:00.000Z" ],
        [ ["2000-12-31", 1, "day"], "2001-01-01T00:00:00.000Z" ],
        [ ["2000-12-31Z", 1, "day"], "2001-01-01T00:00:00.000Z" ],
        [ ["2000-12-31Z", 1, "d"], "2001-01-01T00:00:00.000Z" ],
        [ ["2000-12-31Z", "P1D"], "2001-01-01T00:00:00.000Z" ],
        [ ["2100-12-31", "P3M"], "2101-03-31T00:00:00.000Z" ],
        [ ["2100-12-31", 3, "months"], "2101-03-31T00:00:00.000Z" ],
        [ ["2100-12-31Z", 3, "month"], "2101-03-31T00:00:00.000Z" ],
        [ ["2100-12-31", 3, "m"], "2101-03-31T00:00:00.000Z" ],
        [ ["2100-12-31Z", -3, "m"], "2100-10-01T00:00:00.000Z" ], /* which is 2100-09-30T24:00:00.000Z*/
        [ ["2012-02-12 13:24:12", 10, "minutes"], "2012-02-12T13:34:12.000Z" ],
        [ ["2012-02-12 13:24:12Z", 10, "i"], "2012-02-12T13:34:12.000Z" ],
        [ ["2012-02-12 13:24:12Z", "PT10M"], "2012-02-12T13:34:12.000Z" ],
        [ ["2012-02-12 23:59:59.991", 9, "milliseconds"], "2012-02-13T00:00:00.000Z" ],
        [ ["2012-02-12 23:59:59.991", 9, "f"], "2012-02-13T00:00:00.000Z" ],
        [ ["2012-02-12 23:59:59.991Z", 9, "ms"], "2012-02-13T00:00:00.000Z" ],
        [ ["2012-02-12 23:59:59.991Z", "PT0.009S"], "2012-02-13T00:00:00.000Z" ],
        [ ["2012-02-12", "p1y"], "2013-02-12T00:00:00.000Z" ], /* lower-case ISO durations currently allowed */
        [ ["2012-02-12", 8, "years"], "2020-02-12T00:00:00.000Z" ],
        [ ["2012-02-12Z", 8, "year"], "2020-02-12T00:00:00.000Z" ],
        [ ["2012-02-12T13:24:12Z", 8, "y"], "2020-02-12T13:24:12.000Z" ],
        [ ["2012-02-12Z", "P100Y"], "2112-02-12T00:00:00.000Z" ],
        [ ["2012-02-12Z", -100, "years"], "1912-02-12T00:00:00.000Z" ],
        [ ["2012-2-12Z", -100, "years"], "1912-02-12T00:00:00.000Z" ],
        [ ["1910-01-02T03:04:05Z", 5, "hours"], "1910-01-02T08:04:05.000Z" ],
        [ ["1910-01-02 03:04:05Z", 5, "hour"], "1910-01-02T08:04:05.000Z" ],
        [ ["1910-01-02 03:04:05Z", "PT5H"], "1910-01-02T08:04:05.000Z" ],
        [ ["1910-01-02", 5, "hours"], "1910-01-02T05:00:00.000Z" ],
        [ ["1910-01-02Z", 5, "hours"], "1910-01-02T05:00:00.000Z" ],
        [ ["1910-01-02Z", "PT5H"], "1910-01-02T05:00:00.000Z" ],
        [ ["2015-02-22Z", 1, "w"], "2015-03-01T00:00:00.000Z" ],
        [ ["2015-02-22Z", 1, "weeks"], "2015-03-01T00:00:00.000Z" ],
        [ ["2015-02-22Z", 1, "week"], "2015-03-01T00:00:00.000Z" ],
        [ ["2015-02-22Z", "P1W"], "2015-03-01T00:00:00.000Z" ],
        [ ["2016-02-22Z", 1, "week"], "2016-02-29T00:00:00.000Z" ],
        [ ["2016-02-22Z", "P1W"], "2016-02-29T00:00:00.000Z" ],
        [ ["1221-02-28T23:59:59Z", 800*12, "months"], "2021-02-28T23:59:59.000Z" ],
        [ ["1221-02-28 23:59:59Z", 800, "years"], "2021-02-28T23:59:59.000Z" ],
        [ ["1221-02-28Z", 1000*(60*60*24-1), "ms"], "1221-02-28T23:59:59.000Z" ],
        [ ["1221-2-28Z", 1, "day"], "1221-03-01T00:00:00.000Z" ],
        [ ["1221-2-28Z", "P1D"], "1221-03-01T00:00:00.000Z" ],
        [ ["2000-01-01", "P1Y2M3W4DT5H6M7.890S"], "2001-03-26T05:06:07.890Z" ],
        [ ["2000-01-01", "P1Y2M3W4DT5H6M7.89S"], "2001-03-26T05:06:07.890Z" ],
        [ ["2000-01-01", "PT0.1S"], "2000-01-01T00:00:00.100Z" ],
        [ ["2000-01-01", "PT0.10S"], "2000-01-01T00:00:00.100Z" ],
        [ ["2000-01-01", "PT0.100S"], "2000-01-01T00:00:00.100Z" ],
        [ ["2000-01-01", "PT0.01S"], "2000-01-01T00:00:00.010Z" ],
        [ ["2000-01-01", "PT0.010S"], "2000-01-01T00:00:00.010Z" ],
        [ ["2000-01-01", "PT0.001S"], "2000-01-01T00:00:00.001Z" ],
        [ ["2000-01-01", "PT0.0009999999999999999999999999S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2016Z", -1, "day"], "2015-12-31T00:00:00.000Z" ],
        [ ["2016z", -1, "day"], "2015-12-31T00:00:00.000Z" ],
        [ ["2016", -1, "day"], "2015-12-31T00:00:00.000Z" ],
        [ ["2016-1Z", -1, "day"], "2015-12-31T00:00:00.000Z" ],
        [ ["2016-1z", -1, "day"], "2015-12-31T00:00:00.000Z" ],
        [ ["2016-1-1z", -1, "day"], "2015-12-31T00:00:00.000Z" ],
        [ ["2016-01-01Z", -1, "day"], "2015-12-31T00:00:00.000Z" ],
        [ ["  2016-01-01Z", -1, "day"], "2015-12-31T00:00:00.000Z" ],
        [ ["  2016-01-01z", -1, "day"], "2015-12-31T00:00:00.000Z" ],
        [ [1399395674000, 365, "days"], "2015-05-06T17:01:14.000Z" ],
        [ [1430931674000, 365, "days"], "2016-05-05T17:01:14.000Z" ], /* leap year */
        [ [60123, 7, "days"], "1970-01-08T00:01:00.123Z" ],
        [ [1, -1, "ms"], "1970-01-01T00:00:00.000Z" ],
        [ [0, 0, "ms"], "1970-01-01T00:00:00.000Z" ]
      ];

      values.forEach(function (value) {
        var actual;
        if (value[0][2] === undefined) {
          actual = getQueryResults("RETURN DATE_ADD(@value, @amount)", {
            value: value[0][0],
            amount: value[0][1]
          });
        }
        else {
          actual = getQueryResults("RETURN DATE_ADD(@value, @amount, @unit)", {
            value: value[0][0],
            amount: value[0][1],
            unit: value[0][2],
          });
        }
        assertEqual([ value[1] ], actual);
      }); 
    },

// TODO: DATE_SUBTRACT()
// TODO: DATE_DIFF()
// TODO: DATE_COMPARE()
// TODO: DATE_FORMAT()

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_timestamp function
////////////////////////////////////////////////////////////////////////////////

    testDateTimestampInvalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_TIMESTAMP()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_TIMESTAMP(1, 1, 1, 1, 1, 1, 1, 1)");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_timestamp function
////////////////////////////////////////////////////////////////////////////////

    testDateTimestamp : function () {
      var values = [
        [ "2000-04-29", 956966400000 ],
        [ "2000-04-29Z", 956966400000 ],
        [ "2012-02-12 13:24:12", 1329053052000 ],
        [ "2012-02-12 13:24:12Z", 1329053052000 ],
        [ "2012-02-12 23:59:59.991", 1329091199991 ],
        [ "2012-02-12 23:59:59.991Z", 1329091199991 ],
        [ "2012-02-12", 1329004800000 ],
        [ "2012-02-12Z", 1329004800000 ],
        [ "2012-02-12T13:24:12", 1329053052000 ],
        [ "2012-02-12T13:24:12Z", 1329053052000 ],
        [ "2012-02-12Z", 1329004800000 ],
        [ "2012-2-12Z", 1329004800000 ],
        [ "1910-01-02T03:04:05", -1893358555000 ],
        [ "1910-01-02T03:04:05Z", -1893358555000 ],
        [ "1910-01-02 03:04:05Z", -1893358555000 ],
        [ "1910-01-02", -1893369600000 ],
        [ "1910-01-02Z", -1893369600000 ],
        [ "1970-01-01T01:05:27Z", 3927000 ],
        [ "1970-01-01 01:05:27Z", 3927000 ],
        [ "1970-01-01T12:05:27Z", 43527000 ],
        [ "1970-01-01 12:05:27Z", 43527000 ],
        [ "1970-1-1Z", 0 ],
        [ "1221-02-28T23:59:59Z", -23631004801000 ],
        [ "1221-02-28 23:59:59Z", -23631004801000 ],
        [ "1221-02-28Z", -23631091200000 ],
        [ "1221-2-28Z", -23631091200000 ],
        [ "1000-12-24T04:12:00Z", -30579364080000 ],
        [ "1000-12-24Z", -30579379200000 ],
        [ "1000-12-24 04:12:00Z", -30579364080000 ],
        [ "6789-12-31T23:59:58.99Z", 152104521598990 ],
        [ "6789-12-31Z", 152104435200000 ],
        [ "9999-12-31T23:59:59.999Z", 253402300799999 ],
        [ "9999-12-31Z", 253402214400000 ],
        [ "9999-12-31z", 253402214400000 ],
        [ "9999-12-31", 253402214400000 ],
        [ "2012Z", 1325376000000 ],
        [ "2012z", 1325376000000 ],
        [ "2012", 1325376000000 ],
        [ "2012-2Z", 1328054400000 ],
        [ "2012-1Z", 1325376000000 ],
        [ "2012-1z", 1325376000000 ],
        [ "2012-1-1z", 1325376000000 ],
        [ "2012-01-01Z", 1325376000000 ],
        [ "2012-01-01Z", 1325376000000 ],
        [ "2012-01-02Z", 1325462400000 ],
        [ "2012-01-2Z", 1325462400000 ],
        [ "  2012-01-01Z", 1325376000000 ],
        [ "  2012-01-01z", 1325376000000 ],
        [ 1399395674000, 1399395674000 ],
        [ 3600000, 3600000 ],
        [ 7200000, 7200000 ],
        [ 8200000, 8200000 ],
        [ 61123, 61123 ],
        [ 60123, 60123 ],
        [ 2001, 2001 ],
        [ 2000, 2000 ],
        [ 1000, 1000 ],
        [ 121, 121 ],
        [ 1, 1 ],
        [ 0, 0 ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_TIMESTAMP(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_iso8601 function
////////////////////////////////////////////////////////////////////////////////

    testDateIso8601Invalid : function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISO8601()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISO8601(1, 1, 1, 1, 1, 1, 1, 1)");

      var values = [ 
        "foobar", 
        "2012fjh", 
        "  2012tjjgg", 
        "", 
        " ", 
        "abc2012-01-01", 
        "2000-00-00", 
        "2001-02-11 24:01:01",
        "2001-02-11 25:01:01",
        "2001-02-11 23:60:00",
        "2001-02-11 23:01:60",
        "2001-02-11 23:01:99",
        "2001-00-11",
        "2001-0-11",
        "2001-13-11",
        "2001-13-32",
        "2001-01-0",
        "2001-01-00",
        "2001-01-32",
        "2001-1-32"
      ];
        
      values.forEach(function(value) {
        assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(@value)", { value: value });
      });  
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_iso8601 function
////////////////////////////////////////////////////////////////////////////////

    testDateIso8601 : function () {
      var values = [
        [ "2000-04-29", "2000-04-29T00:00:00.000Z" ],
        [ "2000-04-29Z", "2000-04-29T00:00:00.000Z" ],
        [ "2012-02-12 13:24:12", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12 13:24:12Z", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12 23:59:59.991", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.991Z", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12", "2012-02-12T00:00:00.000Z" ],
        [ "2012-02-12Z", "2012-02-12T00:00:00.000Z" ],
        [ "2012-02-12T13:24:12", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12T13:24:12Z", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12Z", "2012-02-12T00:00:00.000Z" ],
        [ "2012-2-12Z", "2012-02-12T00:00:00.000Z" ],
        [ "1910-01-02T03:04:05", "1910-01-02T03:04:05.000Z" ],
        [ "1910-01-02T03:04:05Z", "1910-01-02T03:04:05.000Z" ],
        [ "1910-01-02 03:04:05", "1910-01-02T03:04:05.000Z" ],
        [ "1910-01-02 03:04:05Z", "1910-01-02T03:04:05.000Z" ],
        [ "1910-01-02", "1910-01-02T00:00:00.000Z" ],
        [ "1910-01-02Z", "1910-01-02T00:00:00.000Z" ],
        [ "1970-01-01T01:05:27+01:00", "1970-01-01T00:05:27.000Z" ],
        [ "1970-01-01T01:05:27-01:00", "1970-01-01T02:05:27.000Z" ],
        [ "1970-01-01T01:05:27", "1970-01-01T01:05:27.000Z" ],
        [ "1970-01-01T01:05:27Z", "1970-01-01T01:05:27.000Z" ],
        [ "1970-01-01 01:05:27Z", "1970-01-01T01:05:27.000Z" ],
        [ "1970-01-01T12:05:27Z", "1970-01-01T12:05:27.000Z" ],
        [ "1970-01-01 12:05:27Z", "1970-01-01T12:05:27.000Z" ],
        [ "1970-1-1Z", "1970-01-01T00:00:00.000Z" ],
        [ "1221-02-28T23:59:59", "1221-02-28T23:59:59.000Z" ],
        [ "1221-02-28T23:59:59Z", "1221-02-28T23:59:59.000Z" ],
        [ "1221-02-28 23:59:59Z", "1221-02-28T23:59:59.000Z" ],
        [ "1221-02-28", "1221-02-28T00:00:00.000Z" ],
        [ "1221-02-28Z", "1221-02-28T00:00:00.000Z" ],
        [ "1221-2-28Z", "1221-02-28T00:00:00.000Z" ],
        [ "1000-12-24T04:12:00Z", "1000-12-24T04:12:00.000Z" ],
        [ "1000-12-24Z", "1000-12-24T00:00:00.000Z" ],
        [ "1000-12-24 04:12:00Z", "1000-12-24T04:12:00.000Z" ],
        [ "6789-12-31T23:59:58.99Z", "6789-12-31T23:59:58.990Z" ],
        [ "6789-12-31Z", "6789-12-31T00:00:00.000Z" ],
        [ "9999-12-31T23:59:59.999Z", "9999-12-31T23:59:59.999Z" ],
        [ "9999-12-31Z", "9999-12-31T00:00:00.000Z" ],
        [ "9999-12-31z", "9999-12-31T00:00:00.000Z" ],
        [ "9999-12-31", "9999-12-31T00:00:00.000Z" ],
        [ "2012Z", "2012-01-01T00:00:00.000Z" ],
        [ "2012z", "2012-01-01T00:00:00.000Z" ],
        [ "2012", "2012-01-01T00:00:00.000Z" ],
        [ "2012-2Z", "2012-02-01T00:00:00.000Z" ],
        [ "2012-1Z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-1z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-1-1z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-01-01", "2012-01-01T00:00:00.000Z" ],
        [ "2012-01-01Z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-01-01Z", "2012-01-01T00:00:00.000Z" ],
        [ "2012-01-02Z", "2012-01-02T00:00:00.000Z" ],
        [ "2012-01-2Z", "2012-01-02T00:00:00.000Z" ],
        [ "  2012-01-01Z", "2012-01-01T00:00:00.000Z" ],
        [ "  2012-01-01z", "2012-01-01T00:00:00.000Z" ],
        [ 1399395674000, "2014-05-06T17:01:14.000Z" ],
        [ 3600000, "1970-01-01T01:00:00.000Z" ],
        [ 7200000, "1970-01-01T02:00:00.000Z" ],
        [ 8200000, "1970-01-01T02:16:40.000Z" ],
        [ 61123, "1970-01-01T00:01:01.123Z" ],
        [ 60123, "1970-01-01T00:01:00.123Z" ],
        [ 2001, "1970-01-01T00:00:02.001Z" ],
        [ 2000, "1970-01-01T00:00:02.000Z" ],
        [ 1000, "1970-01-01T00:00:01.000Z" ],
        [ 121, "1970-01-01T00:00:00.121Z" ],
        [ 1, "1970-01-01T00:00:00.001Z" ],
        [ 0, "1970-01-01T00:00:00.000Z" ]
      ];

      values.forEach(function (value) {
        var actual = getQueryResults("RETURN DATE_ISO8601(@value)", { value: value[0] });
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date_iso8601 function
////////////////////////////////////////////////////////////////////////////////

    testDateIso8601Alternative : function () {
      var values = [
        [ [ 1000, 1, 1, 0, 0, 0, 0 ], "1000-01-01T00:00:00.000Z" ],
        [ [ 9999, 12, 31, 23, 59, 59, 999 ], "9999-12-31T23:59:59.999Z" ],
        [ [ 2012, 1, 1, 13, 12, 14, 95 ], "2012-01-01T13:12:14.095Z" ],
        [ [ 1234, 12, 31, 23, 59, 59, 477 ], "1234-12-31T23:59:59.477Z" ],
        [ [ 2014, 5, 7, 12, 6 ], "2014-05-07T12:06:00.000Z" ],
        [ [ 2014, 5, 7, 12 ], "2014-05-07T12:00:00.000Z" ],
        [ [ 2014, 5, 7 ], "2014-05-07T00:00:00.000Z" ],
        [ [ 2014, 5, 7 ], "2014-05-07T00:00:00.000Z" ],
        [ [ 1000, 1, 1 ], "1000-01-01T00:00:00.000Z" ],
        [ [ "1000", "01", "01", "00", "00", "00", "00" ], "1000-01-01T00:00:00.000Z" ],
        [ [ "1000", "1", "1", "0", "0", "0", "0" ], "1000-01-01T00:00:00.000Z" ],
        [ [ "9999", "12", "31", "23", "59", "59", "999" ], "9999-12-31T23:59:59.999Z" ],
        [ [ "2012", "1", "1", "13", "12", "14", "95" ], "2012-01-01T13:12:14.095Z" ],
        [ [ "1234", "12", "31", "23", "59", "59", "477" ], "1234-12-31T23:59:59.477Z" ],
        [ [ "2014", "05", "07", "12", "06" ], "2014-05-07T12:06:00.000Z" ],
        [ [ "2014", "5", "7", "12", "6" ], "2014-05-07T12:06:00.000Z" ],
        [ [ "2014", "5", "7", "12" ], "2014-05-07T12:00:00.000Z" ],
        [ [ "2014", "5", "7" ], "2014-05-07T00:00:00.000Z" ],
        [ [ "2014", "5", "7" ], "2014-05-07T00:00:00.000Z" ],
        [ [ "1000", "01", "01" ], "1000-01-01T00:00:00.000Z" ],
        [ [ "1000", "1", "1" ], "1000-01-01T00:00:00.000Z" ],
      ];
      
      values.forEach(function (value) {
        var query = "RETURN DATE_ISO8601(" + 
            value[0].map(function(v) {
              return JSON.stringify(v);
            }).join(", ") + ")";
       
        var actual = getQueryResults(query);
        assertEqual([ value[1] ], actual);
      }); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date transformations
////////////////////////////////////////////////////////////////////////////////

    testDateTransformations1 : function () {
      var dt = "2014-05-07T15:23:21.446";
      var actual;
      
      actual = getQueryResults("RETURN DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value))", { value: dt });
      assertEqual([ new Date(dt).getTime() ], actual); 
      
      actual = getQueryResults("RETURN DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value))", { value: dt + "Z" });
      assertEqual([ new Date(dt).getTime() ], actual); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test date transformations
////////////////////////////////////////////////////////////////////////////////

    testDateTransformations2 : function () {
      var dt = "2014-05-07T15:23:21.446";
      var actual;
      
      actual = getQueryResults("RETURN DATE_ISO8601(DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value)))", { value: dt });
      assertEqual([ dt + "Z" ], actual); 

      actual = getQueryResults("RETURN DATE_ISO8601(DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value)))", { value: dt + "Z" });
      assertEqual([ dt + "Z" ], actual); 
    }
  
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlDateFunctionsTestSuite);

return jsunity.done();

