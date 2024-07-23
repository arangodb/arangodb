/* jshint globalstrict:false, strict:false, maxlen: 500 */
/* global assertEqual, assertNotEqual, assertNotNull */
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
// / @author Jan Steemann
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const errors = require("internal").errors;
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;
const assertQueryError = helper.assertQueryError;
const assertQueryWarningAndNull = helper.assertQueryWarningAndNull;
const _ = require("lodash");

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function ahuacatlDateFunctionsTestSuite () {
  return {
    testRangesExtractFunctions () {
      let values = [
        [true, -62167219200000],
        [true, 253402300799999],
        [true, "0000-01-01T00:00:00.000Z"],
        [true, "0000-01-01T00:00:00.999Z"],
        [true, "0000-01-01T23:59:59.000Z"],
        [true, "9999-12-31T23:59:59.999Z"],
        [true, "0000-01-01T00:00:00-01:00"],
        [true, "0000-01-01T00:01:00-02:00"],
        [true, "9999-12-31T23:59:59+01:00"],
        [false, -999999999999999],
        [false,  -99999999999999],
        [false, -62167219200001],
        [false, "-9999-01-01"],
        [false, "-9999-01-01T12:23:34Z"],
        [false, "-300-01-01T12:23:34Z"],
        [false, "-01-01-01T12:23:34Z"],
        [false, "-00-01-01T12:23:34Z"],
        [false, "0000-01-01T00:00:00+01:00"],
        [false, "0000-01-01T00:01:00+02:00"],
        [false, "9999-12-31T23:59:59-01:00"],
        [false, "10000-01-01T12:23:34Z"],
        [false, "10000-01-01"],
        [false, "100000-01-01T12:23:34Z"],
        [false, 253402300800000],
        [false, 999999999999999],
        [false, 9999999999999999],
      ];

      let functions = [
        "DATE_TIMESTAMP",
        "DATE_ISO8601",
        "DATE_DAYOFWEEK",
        "DATE_YEAR",
        "DATE_MONTH",
        "DATE_DAY",
        "DATE_MINUTE",
        "DATE_HOUR",
        "DATE_SECOND",
        "DATE_MILLISECOND",
        "DATE_DAYOFYEAR",
        "DATE_ISOWEEK",
        "DATE_ISOWEEKYEAR",
        "DATE_LEAPYEAR",
        "DATE_QUARTER",
        "DATE_DAYS_IN_MONTH",
      ];

      functions.forEach(function(fn) {
        values.forEach(function(values) {
          values = _.clone(values);
          if (values.shift()) {
            assertNotEqual([ null ], getQueryResults("RETURN " + fn + "(@value)", { value: values[0] }), { fn, value: values[0] });
          } else {
            assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN " + fn  + "(@value)", { value: values[0] });
          }
        });
      });
    },
    
    testDateIso8601Ranges () {
      let values = [
        [false, -1000, 1, 1],
        [false, -100, 1, 1],
        [false, -10, 1, 1],
        [false, -1, 1, 1],
        [false, 10000, 1, 1],
        [false, 99999, 1, 1],
        [true, 0, 1, 1],
        [true, 1, 1, 1],
        [true, 9999, 12,31],
      ];
      values.forEach(function(values) {
        const works = values.shift();
        values = values.map(function(value) { return JSON.stringify(value); });
        if (works) {
          assertNotEqual([ null ], getQueryResults("RETURN DATE_ISO8601(" + values.join(", ") + ")"));
        } else {
          assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(" + values.join(", ") + ")");
        }
      });
    },
    
    testDateAddRanges () {
      let values = [
        [false, "-0001-01-01T00:00:00", 1, "day"],
        [false, "-0000-01-01T00:00:00", 1, "day"],
        [false, "-0000-01-01T00:00:00", 2, "day"],
        [false, "9999-12-31T23:59:59", 1, "day"],
        [false, "9999-12-31T23:59:59", 1, "second"],
        [false, "10000-12-31T23:59:59", 1, "day"],
        [true, "0000-01-01T00:00:00", 1, "second"],
        [true, "0000-01-01T00:00:01", 1, "second"],
        [true, "0000-01-01T00:00:00", 1, "day"],
        [true, "0000-01-01T00:00:00", 9999, "year"],
        [true, "9999-12-31T23:58:00", 1, "minute"],
        [true, "9999-12-31T23:59:58", 1, "second"],
      ];
      values.forEach(function(values) {
        const works = values.shift();
        values = values.map(function(value) { return JSON.stringify(value); });
        if (works) {
          assertNotEqual([ null ], getQueryResults("RETURN DATE_ADD(" + values.join(", ") + ")"));
        } else {
          assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ADD(" + values.join(", ") + ")");
        }
      });
    },
    
    testDateSubRanges () {
      let values = [
        [false, "-0001-01-01T00:00:00", 1, "day"],
        [false, "-0000-01-01T00:00:00", 1, "day"],
        [false, "-0000-01-01T00:00:00", 2, "day"],
        [false, "0000-01-01T00:00:00", 1, "day"],
        [false, "0000-01-01T00:00:00", 1, "second"],
        [false, "0000-01-01T00:00:00", 2, "day"],
        [false, "10000-01-01T12:34:40", 1, "day"],
        [false, "10000-12-31T23:59:59", 1, "day"],
        [true, "0000-01-01T00:00:01", 1, "second"],
        [true, "9999-12-31T23:59:59", 1, "second"],
        [true, "9999-12-31T23:59:59", 9999, "year"],
      ];
      values.forEach(function(values) {
        const works = values.shift();
        values = values.map(function(value) { return JSON.stringify(value); });
        if (works) {
          assertNotEqual([ null ], getQueryResults("RETURN DATE_SUBTRACT(" + values.join(", ") + ")"));
        } else {
          assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_SUBTRACT(" + values.join(", ") + ")");
        }
      });
    },
    
    testDateDiffRanges () {
      let values = [
        [false, "-0001-01-01T00:00:00", "0000-01-01T00:00:00", "day"],
        [false, "0001-01-01T00:00:00", "-0000-01-01T00:00:00", "day"],
        [false, "10000-01-01T00:00:00", "0000-01-01T00:00:00", "day"],
        [false, "0000-01-01T00:00:00", "10000-01-01T00:00:00", "day"],
        [false, "10000-01-01T00:00:00", "-0000-01-01T00:00:00", "day"],
      ];
      values.forEach(function(values) {
        const works = values.shift();
        values = values.map(function(value) { return JSON.stringify(value); });
        if (works) {
          assertNotEqual([ null ], getQueryResults("RETURN DATE_DIFF(" + values.join(", ") + ")"));
        } else {
          assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DIFF(" + values.join(", ") + ")");
        }
      });
    },
    
    testRangesDateFormat () {
      let values = [
        [true, -62167219200000],
        [true, 253402300799999],
        [true, "0000-01-01T00:00:00.000Z"],
        [true, "0000-01-01T00:00:00.999Z"],
        [true, "0000-01-01T23:59:59.000Z"],
        [true, "9999-12-31T23:59:59.999Z"],
        [true, "0000-01-01T00:00:00-01:00"],
        [true, "0000-01-01T00:01:00-02:00"],
        [true, "9999-12-31T23:59:59+01:00"],
        [false, -999999999999999],
        [false,  -99999999999999],
        [false, -62167219200001],
        [false, "-9999-01-01"],
        [false, "-9999-01-01T12:23:34Z"],
        [false, "-300-01-01T12:23:34Z"],
        [false, "-01-01-01T12:23:34Z"],
        [false, "-00-01-01T12:23:34Z"],
        [false, "0000-01-01T00:00:00+01:00"],
        [false, "0000-01-01T00:01:00+02:00"],
        [false, "9999-12-31T23:59:59-01:00"],
        [false, "10000-01-01T12:23:34Z"],
        [false, "10000-01-01"],
        [false, "100000-01-01T12:23:34Z"],
        [false, 253402300800000],
        [false, 999999999999999],
        [false, 9999999999999999],
      ];

      values.forEach(function(values) {
        values = _.clone(values);
        if (values.shift()) {
          assertNotEqual([ null ], getQueryResults("RETURN DATE_FORMAT(@value, '%f')", { value: values[0] }), { value: values[0] });
        } else {
          assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_FORMAT(@value, '%f')", { value: values[0] });
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_format function
// //////////////////////////////////////////////////////////////////////////////

    testDateFormatInvalid () {
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR({})");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_FORMAT({}, {})");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_FORMAT([], [])");
    },

    testDateFormat () {
      const date = "6789-12-31T23:59:58.990Z";
      const dateTimezoned = "6790-01-01T00:59:58.990Z";
      assertEqual([ date ], getQueryResults("RETURN DATE_FORMAT(@value, '%z')", { value: date }));
      assertEqual([ dateTimezoned ], getQueryResults("RETURN DATE_FORMAT(@value, '%z', 'Europe/Berlin')", { value: date }));

      const values = [
        ["7200-04-29", "7200", "7200", "00", "+007200", "04", "29"],
        ["200-04-29", "200", "0200", "00", "+000200", "04", "29"],
        ["20-04-29", "20", "0020", "20", "+000020", "04", "29"],
        ["2-12-29", "2", "0002", "02", "+000002", "12", "29"],
        ["0-04-07", "0", "0000", "00", "+000000", "04", "07"]
      ];
      values.forEach(function (value) {
        assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%y')", { value: value[0] }), "%y");
        assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@value, '%yyyy')", { value: value[0] }), "%yyyy");
        assertEqual([ value[3] ], getQueryResults("RETURN DATE_FORMAT(@value, '%yy')", { value: value[0] }), "%yy");
        assertEqual([ value[4] ], getQueryResults("RETURN DATE_FORMAT(@value, '%yyyyyy')", { value: value[0] }), "%yyyyyyy");
        assertEqual([ value[5] ], getQueryResults("RETURN DATE_FORMAT(@value, '%mm')", { value: value[0] }), "%mm");
        assertEqual([ value[6] ], getQueryResults("RETURN DATE_FORMAT(@value, '%dd')", { value: value[0] }), "%dd");
      });
      const timezonedValues = [
        ["2023-03-25T23:00:00.000Z", "Europe/Berlin", "2023", "2023", "23", "+002023", "03", "26"],
        ["7199-12-31T23:59:59.000Z", "Europe/Berlin", "7200", "7200", "00", "+007200", "01", "01"],
        ["199-12-31T23:59:59.000Z", "Europe/Berlin", "200", "0200", "00", "+000200", "01", "01"]
      ];
      timezonedValues.forEach(function (value) {
        assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%y', @timezone)", { date: value[0], timezone: value[1] }), "%y");
        assertEqual([ value[3] ], getQueryResults("RETURN DATE_FORMAT(@date, '%yyyy', @timezone)", { date: value[0], timezone: value[1] }), "%yyyy");
        assertEqual([ value[4] ], getQueryResults("RETURN DATE_FORMAT(@date, '%yy', @timezone)", { date: value[0], timezone: value[1] }), "%yy");
        assertEqual([ value[5] ], getQueryResults("RETURN DATE_FORMAT(@date, '%yyyyyy', @timezone)", { date: value[0], timezone: value[1] }), "%yyyyyyy");
        assertEqual([ value[6] ], getQueryResults("RETURN DATE_FORMAT(@date, '%mm', @timezone)", { date: value[0], timezone: value[1] }), "%mm");
        assertEqual([ value[7] ], getQueryResults("RETURN DATE_FORMAT(@date, '%dd', @timezone)", { date: value[0], timezone: value[1] }), "%dd");
      });
      const tvalues = [
        ["2012-01-01T17:17:17.177Z", "17", "17", "17", "177", "001", "52"],
        ["2012-12-31T09:09:09.001Z", "09", "09", "09", "001", "366", "01"],
        ["2013-12-31T09:09:09.001Z", "09", "09", "09", "001", "365", "01"],
        ["2013-12-31T09:09:09.999Z", "09", "09", "09", "999", "365", "01"],
        ["2013-12-31T09:09:09.123456789Z", "09", "09", "09", "123", "365", "01"],
        ["2013-12-31T09:09:09.12345678912984378577834767363", "09", "09", "09", "123", "365", "01"]
      ];
      tvalues.forEach(function (value) {
        assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%hh')", { value: value[0] }), "hh");
        assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@value, '%ii')", { value: value[0] }), "ii");
        assertEqual([ value[3] ], getQueryResults("RETURN DATE_FORMAT(@value, '%ss')", { value: value[0] }), "ss");
        assertEqual([ value[4] ], getQueryResults("RETURN DATE_FORMAT(@value, '%fff')", { value: value[0] }), "fff");
        assertEqual([ value[5] ], getQueryResults("RETURN DATE_FORMAT(@value, '%xxx')", { value: value[0] }), "xxx");
        assertEqual([ value[6] ], getQueryResults("RETURN DATE_FORMAT(@value, '%kk')", { value: value[0] }), "kk"); // TODO: WTF?
      });
      const timezonedTValues = [
        ["2023-03-25T23:00:00.000Z", "Europe/Berlin", "00", "00", "00", "000", "085", "12"],
        ["7199-12-31T23:59:59.000Z", "Europe/Berlin", "00", "59", "59", "000", "001", "52"],
        // this is very different and will parse to 0200-01-01T00:53:27.000+00:53 + 27 seconds
        ["199-12-31T23:59:59.000Z", "Europe/Berlin", "00", "53", "27", "000", "001", "01"],
        ["2023-12-31T23:00:00.000Z", "Asia/Kathmandu", "04", "45", "00", "000", "001", "01"]
      ];
      timezonedTValues.forEach(function (value) {
        assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%hh', @timezone)", { date: value[0], timezone: value[1] }), "hh " + value[0] + " " + value[1]);
        assertEqual([ value[3] ], getQueryResults("RETURN DATE_FORMAT(@date, '%ii', @timezone)", { date: value[0], timezone: value[1] }), "ii " + value[0] + " " + value[1]);
        assertEqual([ value[4] ], getQueryResults("RETURN DATE_FORMAT(@date, '%ss', @timezone)", { date: value[0], timezone: value[1] }), "ss " + value[0] + " " + value[1]);
        assertEqual([ value[5] ], getQueryResults("RETURN DATE_FORMAT(@date, '%fff', @timezone)", { date: value[0], timezone: value[1] }), "fff " + value[0] + " " + value[1]);
        assertEqual([ value[6] ], getQueryResults("RETURN DATE_FORMAT(@date, '%xxx', @timezone)", { date: value[0], timezone: value[1] }), "xxx " + value[0] + " " + value[1]);
        assertEqual([ value[7] ], getQueryResults("RETURN DATE_FORMAT(@date, '%kk', @timezone)", { date: value[0], timezone: value[1] }), "kk " + value[0] + " " + value[1]);
      });
      const dates = [
        ["2012-01-01", "Jan", "January", "Sun", "Sunday"],
        ["2012-02-06", "Feb", "February", "Mon", "Monday"],
        ["2012-03-06", "Mar", "March", "Tue", "Tuesday"],
        ["2012-04-04", "Apr", "April", "Wed", "Wednesday"],
        ["2012-05-03", "May", "May", "Thu", "Thursday"],
        ["2012-06-08", "Jun", "June", "Fri", "Friday"],
        ["2012-07-07", "Jul", "July", "Sat", "Saturday"],
        ["2012-08-05", "Aug", "August", "Sun", "Sunday"],
        ["2012-09-03", "Sep", "September", "Mon", "Monday"],
        ["2012-10-02", "Oct", "October", "Tue", "Tuesday"],
        ["2012-11-07", "Nov", "November", "Wed", "Wednesday"],
        ["2012-12-06", "Dec", "December", "Thu", "Thursday"]
      ];
      dates.forEach(function (value) {
        assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%mmm')", { value: value[0] }), "mmm " + value[0]);
        assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@value, '%mmmm')", { value: value[0] }), "mmmm " + value[0]);
        assertEqual([ value[3] ], getQueryResults("RETURN DATE_FORMAT(@value, '%www')", { value: value[0] }), "www " + value[0]);
        assertEqual([ value[4] ], getQueryResults("RETURN DATE_FORMAT(@value, '%wwww')", { value: value[0] }), "wwww " + value[0]);
      });
      const timezonedDates = [
        ["1985-01-02T23:00:00.000Z", "Europe/Berlin", "Jan", "January", "Thu", "Thursday"]
      ];
      timezonedDates.forEach(function (value) {
        assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%mmm', @timezone)", { date: value[0],timezone: value[1] }), "mmm " + value[0]);
        assertEqual([ value[3] ], getQueryResults("RETURN DATE_FORMAT(@date, '%mmmm', @timezone)", { date: value[0],timezone: value[1] }), "mmmm " + value[0]);
        assertEqual([ value[4] ], getQueryResults("RETURN DATE_FORMAT(@date, '%www', @timezone)", { date: value[0],timezone: value[1] }), "www " + value[0]);
        assertEqual([ value[5] ], getQueryResults("RETURN DATE_FORMAT(@date, '%wwww', @timezone)", { date: value[0],timezone: value[1] }), "wwww " + value[0]);
      });
      assertEqual([ " - %  " ], getQueryResults("RETURN DATE_FORMAT(@value, '%& - %% % ')", { value: "2012-01-01" }), "format ");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test is_datestring function
// //////////////////////////////////////////////////////////////////////////////

    testIsDateString () {
      const values = [
        [ "2000-04-29", true ],
        [ "2000-04-29Z", true ],
        [ "2012-02-12 13:24:12", true ],
        [ "2012-02-12 13:24:12Z", true ],
        [ "2012-02-12 23:59:59.991", true ],
        [ "2012-02-12 23:59:59.991Z", true ],
        [ "2012-02-12 23:59:59.991+0:00", true ],
        [ "2012-02-12 23:59:59.991+1:00", true ],
        [ "2012-02-12 23:59:59.991+01:00", true ],
        [ "2012-02-12 23:59:59.991+08:30", true ],
        [ "2012-02-12 23:59:59.991+02:00", true ],
        [ "2012-02-12 23:59:59.991-0:30", true ],
        [ "2012-02-12 23:59:59.991-1:30", true ],
        [ "2012-02-12 23:59:59.991-01:00", true ],
        [ "2012-02-12 23:59:59.991-08:30", true ],
        [ "2012-02-12 23:59:59.991-02:00", true ],
        [ "2012-02-12", true ],
        [ "2012-02-12Z", true ],
        [ "2012-02-12T13:24:12Z", true ],
        [ "2012-02-12Z", true ],
        [ "2012-2-12Z", true ],
        [ "1910-01-02T03:04:05Z", true ],
        [ "1910-01-02 03:04:05Z", true ],
        [ "1910-01-02", true ],
        [ "1910-02-31", true ],
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
        [ "9999-12-31T22:59:59.999-1:00", true ],
        [ "9999-12-31T22:59:59.999-01:00", true ],
        [ "9999-12-31T23:59:59.999+1:00", true ],
        [ "9999-12-31T23:59:59.999+01:00", true ],
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
        [ "  2012-12-31z   ", true ],
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
        [ "9999-12-31T23:59:59.999-1:00", false ],
        [ "9999-12-31T23:59:59.999-01:00", false ],
        [ { }, false ]
      ];

      values.forEach(function (value) {
        assertEqual([ value[1] ], getQueryResults("RETURN IS_DATESTRING(@value)", { value: value[0] }), value);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test is_datestring function
// //////////////////////////////////////////////////////////////////////////////

    testIsDateStringInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_DATESTRING()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN IS_DATESTRING('foo', 'bar')");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_now function
// //////////////////////////////////////////////////////////////////////////////

    testDateNowInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_NOW(1)");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_NOW(1, 1)");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_now function
// //////////////////////////////////////////////////////////////////////////////

    testDateNow () {
      const t1 = new Date();
      const t2 = getQueryResults("RETURN DATE_NOW()")[0];
      assertEqual(true, Math.abs(t1 - t2) < 1000, t1, t2);
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_dayofweek function
// //////////////////////////////////////////////////////////////////////////////
    testDateRoundInvalid () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ROUND()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ROUND(1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, 1, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, 1, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, 1, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, 1, {})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, null, 'day')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, false, 'day')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, [], 'day')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, {}, 'day')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(null, 1, 'day')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(false, 1, 'day')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND([], 1, 'day')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND({}, 1, 'day')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, 1, 'day', null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, 1, 'day', false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, 1, 'day', [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ROUND(0, 1, 'day', {})");
    },

    testDateBucket: function () {
      var values = [
        [ "2000-04-28", 1, "days", "2000-04-28T00:00:00.000Z" ],
        [ "2000-04-29", 1, "days", "2000-04-29T00:00:00.000Z" ],
        [ "2000-04-29", 1, "day", "2000-04-29T00:00:00.000Z" ],
        [ "2000-04-29Z", 1, "day", "2000-04-29T00:00:00.000Z" ],
        [ "2000-04-29T01:20:34Z", 1, "day", "2000-04-29T00:00:00.000Z" ],
        [ "2000-04-28Z", 2, "day", "2000-04-27T00:00:00.000Z" ],
        [ "2000-04-29Z", 2, "day", "2000-04-29T00:00:00.000Z" ],
        [ "2000-04-30Z", 2, "day", "2000-04-29T00:00:00.000Z" ],
        [ "2000-03-31Z", 2, "day", "2000-03-30T00:00:00.000Z" ],
        [ "2000-04-01Z", 2, "day", "2000-04-01T00:00:00.000Z" ],
        [ "2000-04-01Z", 2, "day", "2000-04-01T00:00:00.000Z" ],
        [ "2000-04-02T02:34:56Z", 2, "day", "2000-04-01T00:00:00.000Z" ],
        [ "2000-04-01Z", 10, "day", "2000-03-24T00:00:00.000Z" ],
        [ "2000-04-02Z", 10, "day", "2000-03-24T00:00:00.000Z" ],
        [ "2000-04-09Z", 10, "day", "2000-04-03T00:00:00.000Z" ],
        [ "2000-04-10Z", 10, "day", "2000-04-03T00:00:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 10, "day", "2000-04-03T00:00:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 1, "hours", "2000-04-10T11:00:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 1, "hour", "2000-04-10T11:00:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 2, "hour", "2000-04-10T10:00:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 4, "hour", "2000-04-10T08:00:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 10, "hour", "2000-04-10T02:00:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 15, "hour", "2000-04-09T21:00:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 1, "minutes", "2000-04-10T11:39:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 1, "minute", "2000-04-10T11:39:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 2, "minute", "2000-04-10T11:38:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 4, "minute", "2000-04-10T11:36:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 10, "minute", "2000-04-10T11:30:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 15, "minute", "2000-04-10T11:30:00.000Z" ],
        [ "2000-04-10T11:39:29Z", 1, "seconds", "2000-04-10T11:39:29.000Z" ],
        [ "2000-04-10T11:39:29Z", 1, "second", "2000-04-10T11:39:29.000Z" ],
        [ "2000-04-10T11:39:29Z", 2, "second", "2000-04-10T11:39:28.000Z" ],
        [ "2000-04-10T11:39:29Z", 4, "second", "2000-04-10T11:39:28.000Z" ],
        [ "2000-04-10T11:39:29Z", 10, "second", "2000-04-10T11:39:20.000Z" ],
        [ "2000-04-10T11:39:29Z", 15, "second", "2000-04-10T11:39:15.000Z" ],
        [ "2000-04-10T11:39:29.123Z", 1, "milliseconds", "2000-04-10T11:39:29.123Z" ],
        [ "2000-04-10T11:39:29.123Z", 1, "millisecond", "2000-04-10T11:39:29.123Z" ],
        [ "2000-04-10T11:39:29.123Z", 2, "millisecond", "2000-04-10T11:39:29.122Z" ],
        [ "2000-04-10T11:39:29.123Z", 4, "millisecond", "2000-04-10T11:39:29.120Z" ],
        [ "2000-04-10T11:39:29.123Z", 10, "millisecond", "2000-04-10T11:39:29.120Z" ],
        [ "2000-04-10T11:39:29.123Z", 15, "millisecond", "2000-04-10T11:39:29.115Z" ],
        
        [ "2000-04-10T11:39:29.123Z", 0, "day", null ],
        [ "2000-04-10T11:39:29.123Z", -1, "day", null ],
        [ "2000-04-10T11:39:29.123Z", "foobar", "day", null ],
        [ "2000-04-10T11:39:29.123Z", 15, "", null ],
        [ "2000-04-10T11:39:29.123Z", 15, "pork", null ],
        [ "", 15, "day", null ],
        [ "foobar", 15, "day", null ],
        [ 1679788555555, 1, "day", "2023-03-25T00:00:00.000Z" ],
        [ "2023-03-25T23:55:55.555Z", 1, "day", "2023-03-25T00:00:00.000Z" ],
        [ 1679788555555, 1, "day", "Europe/Berlin", "2023-03-25T23:00:00.000Z" ],
        [ "2023-03-25T23:55:55.555Z", 1, "day", "Europe/Berlin", "2023-03-25T23:00:00.000Z" ],
        [ "2023-03-26T00:55:55.555+01:00", 1, "day", "Europe/Berlin", "2023-03-25T23:00:00.000Z" ]
      ];

      values.forEach(function (value) {
        if (value.length === 4) {
          assertEqual([value[3]], getQueryResults("RETURN DATE_ROUND(@value, @mult, @unit)", {
            value: value[0],
            mult: value[1],
            unit: value[2]
          }));
        } else {
          assertEqual([value[4]], getQueryResults("RETURN DATE_ROUND(@value, @mult, @unit, @timezone)", {
            value: value[0],
            mult: value[1],
            unit: value[2],
            timezone: value[3]
          }));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_dayofweek function
// //////////////////////////////////////////////////////////////////////////////
    testDateDayOfWeekInvalid () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFWEEK()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFWEEK(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFWEEK(0, {})");
    },

    testDateDayOfWeek: function () {
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
        [ 0, 4 ],
        [ "2023-03-25T23:00:00.000Z", 6 ],
        [ 1679785200000, 6 ],
        [ "2023-03-25T23:00:00.000Z", "Europe/Berlin", 0 ],
        [ 1679785200000, "Europe/Berlin", 0 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([value[1]], getQueryResults("RETURN DATE_DAYOFWEEK(@value)", {value: value[0]}));
          assertEqual([value[1]], getQueryResults("RETURN DATE_FORMAT(@value, '%w')", {value: value[0]}));
        } else {
          assertEqual([value[2]], getQueryResults("RETURN DATE_DAYOFWEEK(@date,@timezone)", {date: value[0], timezone: value[1]}));
          assertEqual([value[2]], getQueryResults("RETURN DATE_FORMAT(@date, '%w',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_year function
// //////////////////////////////////////////////////////////////////////////////

    testDateYearInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_YEAR()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_YEAR(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_YEAR(0, {})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_year function
// //////////////////////////////////////////////////////////////////////////////

    testDateYear: function () {
      const values = [
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
        [ 0, 1970 ],
        [ "2023-12-31T23:00:00.000Z", 2023 ],
        [ "2023-12-31T23:00:00.000Z", "Europe/Berlin", 2024 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_YEAR(@value)", { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%yyyy')", { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_YEAR(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([value[2]], getQueryResults("RETURN DATE_FORMAT(@date, '%yyyy',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_month function
// //////////////////////////////////////////////////////////////////////////////

    testDateMonthInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MONTH()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MONTH(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MONTH(0, {})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_month function
// //////////////////////////////////////////////////////////////////////////////

    testDateMonth: function () {
      const values = [
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
        [ 0, 1 ],
        [ "2023-12-31T23:00:00.000Z", 12 ],
        [ "2023-12-31T23:00:00.000Z", "Europe/Berlin", 1 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults('RETURN DATE_MONTH(@value)', { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults('RETURN DATE_FORMAT(@value, "%m")', { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_MONTH(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%m',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_day function
// //////////////////////////////////////////////////////////////////////////////

    testDateDayInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAY()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAY(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAY(0, {})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_day function
// //////////////////////////////////////////////////////////////////////////////

    testDateDay: function () {
      const values = [
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
        [ 0, 1 ],
        [ "2023-12-31T23:00:00.000Z", 31 ],
        [ "2023-12-31T23:00:00.000Z", "Europe/Berlin", 1 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults('RETURN DATE_DAY(@value)', { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults('RETURN DATE_FORMAT(@value, "%d")', { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_DAY(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%d',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_hour function
// //////////////////////////////////////////////////////////////////////////////

    testDateHourInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_HOUR()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_HOUR(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_HOUR(0, {})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_hour function
// //////////////////////////////////////////////////////////////////////////////

    testDateHour: function () {
      const values = [
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
        [ 0, 0 ],
        [ "2023-12-31T23:00:00.000Z", 23 ],
        [ "2023-12-31T23:00:00.000Z", "Europe/Berlin", 0 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults('RETURN DATE_HOUR(@value)', { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults('RETURN DATE_FORMAT(@value, "%h")', { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_HOUR(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%h',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_minute function
// //////////////////////////////////////////////////////////////////////////////

    testDateMinuteInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MINUTE()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MINUTE(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MINUTE(0, {})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_minute function
// //////////////////////////////////////////////////////////////////////////////

    testDateMinute: function () {
      const values = [
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
        [ 0, 0 ],
        [ "2023-12-31T23:00:00.000Z", 0 ],
        [ "2023-12-31T23:00:00.000Z", "Asia/Kathmandu", 45 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_MINUTE(@value)", { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%i')", { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_MINUTE(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%i',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_second function
// //////////////////////////////////////////////////////////////////////////////

    testDateSecondInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_SECOND()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_SECOND(1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SECOND(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SECOND(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SECOND([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SECOND({})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_second function
// //////////////////////////////////////////////////////////////////////////////

    testDateSecond: function () {
      const values = [
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
        actual = getQueryResults("RETURN DATE_FORMAT(@value, '%s')", { value: value[0] });
        assertEqual([ value[1] ], actual);
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_millisecond function
// //////////////////////////////////////////////////////////////////////////////

    testDateMillisecondInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MILLISECOND()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_MILLISECOND(1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MILLISECOND(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MILLISECOND(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MILLISECOND([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_MILLISECOND({})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_millisecond function
// //////////////////////////////////////////////////////////////////////////////

    testDateMillisecond: function () {
      const values = [
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
        assertEqual([ value[1] ], getQueryResults("RETURN DATE_MILLISECOND(@value)", { value: value[0] }));
        assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%f')", { value: value[0] }));
      });
    },

// TODO: verify all assertions of the following functions below are actually correct:
// DATE_DAYOFYEAR(), DATE_ISOWEEK(), DATE_LEAPYEAR(), DATE_QUARTER(), DATE_ADD(),
// DATE_SUBTRACT(), DATE_DIFF(). Unaffected should be: DATE_COMPARE(), DATE_FORMAT().
// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_dayofyear function
// //////////////////////////////////////////////////////////////////////////////

    testDateDayOfYearInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFYEAR()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYOFYEAR(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFYEAR(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFYEAR(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFYEAR([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFYEAR({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFYEAR(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFYEAR(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFYEAR(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYOFYEAR(0, {})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_dayofyear function
// //////////////////////////////////////////////////////////////////////////////

    testDateDayOfYear: function () {
      const values = [
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
        [ 0, 1 ],
        [ "2023-12-31T23:00:00.000Z", 365 ],
        [ "2023-12-31T23:00:00.000Z", "Europe/Berlin", 1 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_DAYOFYEAR(@value)", { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%x')", { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_DAYOFYEAR(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%x',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_isoweek function
// //////////////////////////////////////////////////////////////////////////////

    testDateISOWeekInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISOWEEK()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISOWEEK(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEK(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEK(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEK([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEK({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEK(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEK(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEK(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEK(0, {})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_isoweekyear function
// //////////////////////////////////////////////////////////////////////////////

    testDateISOWeekYearInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ISOWEEKYEAR(0, {})");
    },


// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_isoweek function
// //////////////////////////////////////////////////////////////////////////////

    testDateISOWeek: function () {
      const values = [
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
        [ 0, 1 ],
        [ "2023-12-31T23:00:00.000Z", 52 ],
        [ "2023-12-31T23:00:00.000Z", "Europe/Berlin", 1 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_ISOWEEK(@value)", { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%k')", { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_ISOWEEK(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%k',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_isoweekyear function
// //////////////////////////////////////////////////////////////////////////////

    testDateISOWeekYear: function () {
      const values = [
        [ "2000-04-29", 17, 2000 ],
        [ "2000-04-29Z", 17, 2000 ],
        [ "2000-12-31", 52, 2000 ],
        [ "2000-12-31Z", 52, 2000 ],
        [ "2100-12-31", 52, 2100 ],
        [ "2100-12-31Z", 52, 2100 ],
        [ "2400-12-31", 52, 2400 ],
        [ "2400-12-31Z", 52, 2400 ],
        [ "2012-02-12 13:24:12", 6, 2012 ],
        [ "2012-02-12 13:24:12Z", 6, 2012 ],
        [ "2012-02-12 23:59:59.991", 6, 2012 ],
        [ "2012-02-12 23:59:59.991Z", 6, 2012 ],
        [ "2012-02-12", 6, 2012 ],
        [ "2012-02-12Z", 6, 2012 ],
        [ "2012-02-12T13:24:12Z", 6, 2012 ],
        [ "2012-02-12Z", 6, 2012 ],
        [ "2012-2-12Z", 6, 2012 ],
        [ "1910-01-02T03:04:05Z", 52, 1909 ],
        [ "1910-01-02 03:04:05Z", 52, 1909 ],
        [ "1910-01-02", 52, 1909 ],
        [ "1910-01-02Z", 52, 1909 ],
        [ "1970-01-01T01:05:27", 1, 1970 ],
        [ "1970-01-01T01:05:27Z", 1, 1970 ],
        [ "1970-01-01 01:05:27Z", 1, 1970 ],
        [ "1970-1-1Z", 1, 1970 ],
        [ "1221-02-28T23:59:59Z", 8, 1221 ],
        [ "1221-02-28 23:59:59Z", 8, 1221 ],
        [ "1221-02-28Z", 8, 1221 ],
        [ "1221-2-28Z", 8, 1221 ],
        [ "1000-12-24T04:12:00Z", 52, 1000 ],
        [ "1000-12-24Z", 52, 1000 ],
        [ "1000-12-24 04:12:00Z", 52, 1000 ],
        [ "2016Z", 53, 2015 ],
        [ "2016z", 53, 2015 ],
        [ "2016", 53, 2015 ],
        [ "2016-1Z", 53, 2015 ],
        [ "2016-1z", 53, 2015 ],
        [ "2016-1-1z", 53, 2015 ],
        [ "2016-01-01Z", 53, 2015 ],
        [ "2016-01-01Z", 53, 2015 ],
        [ "  2016-01-01Z", 53, 2015 ],
        [ "  2016-01-01z", 53, 2015 ],
        [ 1399395674000, 19, 2014],
        [ 60123, 1, 1970],
        [ 1, 1, 1970 ],
        [ 0, 1, 1970 ],
        [ "2023-12-31T23:00:00.000Z", 52, 2023],
        [ "2023-12-31T23:00:00.000Z", "Europe/Berlin", 1, 2024]
      ];

      values.forEach(function (value) {
        if (value.length === 3) {
          assertEqual([{week: value[1], year: value[2]}], getQueryResults("RETURN DATE_ISOWEEKYEAR(@value)", { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%k')", { value: value[0] }));
        } else {
          assertEqual([{week: value[2], year: value[3]}], getQueryResults("RETURN DATE_ISOWEEKYEAR(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%k',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_leapyear function
// //////////////////////////////////////////////////////////////////////////////

    testDateLeapYearInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_LEAPYEAR()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_LEAPYEAR(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_LEAPYEAR(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_LEAPYEAR(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_LEAPYEAR([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_LEAPYEAR({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_LEAPYEAR(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_LEAPYEAR(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_LEAPYEAR(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_LEAPYEAR(0, {})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_leapyear function
// //////////////////////////////////////////////////////////////////////////////

    testDateLeapYear: function () {
      const values = [
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
        [ 0, false ],
        [ "2016-12-31T23:00:00.000Z", true ],
        [ "2016-12-31T23:00:00.000Z", "Europe/Berlin", false ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_LEAPYEAR(@value)", { value: value[0] }));
          assertEqual([ value[1] ? 1 : 0 ], getQueryResults("RETURN DATE_FORMAT(@value, '%l')", { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_LEAPYEAR(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ? 1 : 0  ], getQueryResults("RETURN DATE_FORMAT(@date, '%l',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_quarter function
// //////////////////////////////////////////////////////////////////////////////

    testDateQuarterInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_QUARTER()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_QUARTER(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_QUARTER(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_QUARTER(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_QUARTER([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_QUARTER({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_QUARTER(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_QUARTER(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_QUARTER(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_QUARTER(0, {})");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_trunc function
// //////////////////////////////////////////////////////////////////////////////

    testDateTruncInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_TRUNC()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_TRUNC(1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(1, 1)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(false, 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(1, true)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(1, false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(null, 'y')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(1, null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC([], 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(1, [])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC({}, 'year')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(1, {})");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(DATE_NOW(), 'year', null)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(DATE_NOW(), 'year', false)");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(DATE_NOW(), 'year', [])");
      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_TRUNC(DATE_NOW(), 'year', {})");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_TRUNC(DATE_NOW(), 'sugar')");
      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_TRUNC(DATE_NOW(), '')");
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_trunc function
// //////////////////////////////////////////////////////////////////////////////

    testDateTrunc: function () {
      const values = [
        [ ["2000-04-29", "years"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-04-29 20:49:59.123", "year"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-04-29 12:34:56", "y"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-04-29", "months"], "2000-04-01T00:00:00.000Z" ],
        [ ["2000-04-29", "month"], "2000-04-01T00:00:00.000Z" ],
        [ ["2000-04-29 23:59:59.999", "m"], "2000-04-01T00:00:00.000Z" ],
        [ ["2000-04-29", "days"], "2000-04-29T00:00:00.000Z" ],
        [ ["2000-04-29 19:20:30.405", "day"], "2000-04-29T00:00:00.000Z" ],
        [ ["2000-04-29 03:04:05.067", "d"], "2000-04-29T00:00:00.000Z" ],
        [ ["2000-04-29 12:34:56.789", "hours"], "2000-04-29T12:00:00.000Z" ],
        [ ["2000-04-29 12:34:56.789", "hour"], "2000-04-29T12:00:00.000Z" ],
        [ ["2000-04-29 12:34:56.789", "h"], "2000-04-29T12:00:00.000Z" ],
        [ ["2000-04-29 12:34:56.789", "minutes"], "2000-04-29T12:34:00.000Z" ],
        [ ["2000-04-29 12:34:56.789", "minute"], "2000-04-29T12:34:00.000Z" ],
        [ ["2000-04-29 12:34:56.789", "i"], "2000-04-29T12:34:00.000Z" ],
        [ ["2000-04-29 12:34:56.789", "seconds"], "2000-04-29T12:34:56.000Z" ],
        [ ["2000-04-29 12:34:56.789", "second"], "2000-04-29T12:34:56.000Z" ],
        [ ["2000-04-29 12:34:56.789", "s"], "2000-04-29T12:34:56.000Z" ],
        [ ["2000-04-29 12:34:56.789", "milliseconds"], "2000-04-29T12:34:56.789Z" ],
        [ ["2000-04-29 12:34:56.789", "millisecond"], "2000-04-29T12:34:56.789Z" ],
        [ ["2000-04-29 12:34:56.789", "f"], "2000-04-29T12:34:56.789Z" ],
        [ [1679785200000, "day"], "2023-03-25T00:00:00.000Z" ],
        [ [1679785200000, "day", "Europe/Berlin"], "2023-03-25T23:00:00.000Z" ]
      ];

      values.forEach(function (value) {
        if (value[0].length === 2) {
          assertEqual([ value[1] ], getQueryResults(`RETURN DATE_TRUNC(@val[0], @val[1])`, {val: value[0]}));
        } else {
          assertEqual([ value[1] ], getQueryResults(`RETURN DATE_TRUNC(@val[0], @val[1], @val[2])`, {val: value[0]}));
        }
      });
    },

// //////////////////////////////////////////////////////////////////////////////
// / @brief test date_quarter function
// //////////////////////////////////////////////////////////////////////////////

    testDateQuarter: function () {
      const values = [
        [ "2000-04-29", 2 ],
        [ "2000-04-29Z", 2 ],
        [ "2000-12-31", 4 ],
        [ "2000-12-31Z", 4 ],
        [ "2100-12-31", 4 ],
        [ "2100-12-31Z", 4 ],
        [ "2400-12-31", 4 ],
        [ "2400-12-31Z", 4 ],
        [ "2012-02-12 13:24:12", 1 ],
        [ "2012-02-12 13:24:12Z", 1 ],
        [ "2012-02-12 23:59:59.991", 1 ],
        [ "2012-02-12 23:59:59.991Z", 1 ],
        [ "2012-02-12", 1 ],
        [ "2012-02-12Z", 1 ],
        [ "2012-02-12T13:24:12Z", 1 ],
        [ "2012-02-12Z", 1 ],
        [ "2012-2-12Z", 1 ],
        [ "1910-01-02T03:04:05Z", 1 ],
        [ "1910-01-02 03:04:05Z", 1 ],
        [ "1910-01-02", 1 ],
        [ "1910-01-02Z", 1 ],
        [ "1970-01-01T01:05:27", 1 ],
        [ "1970-01-01T01:05:27Z", 1 ],
        [ "1970-01-01 01:05:27Z", 1 ],
        [ "1970-1-1Z", 1 ],
        [ "1221-02-28T23:59:59Z", 1 ],
        [ "1221-02-28 23:59:59Z", 1 ],
        [ '2017-08-28', 3 ],
        [ '2017-09-11', 3 ],
        [ "1221-02-28Z", 1 ],
        [ "1221-2-28Z", 1 ],
        [ "1000-12-24T04:12:00Z", 4 ],
        [ "1000-12-24Z", 4 ],
        [ "1000-12-24 04:12:00Z", 4 ],
        [ "2016Z", 1 ],
        [ "2016z", 1 ],
        [ "2016", 1 ],
        [ "2016-1Z", 1 ],
        [ "2016-1z", 1 ],
        [ "2016-1-1z", 1 ],
        [ "2016-01-01Z", 1 ],
        [ "2016-01-01Z", 1 ],
        [ "  2016-01-01Z", 1 ],
        [ "  2016-01-01z", 1 ],
        [ 1399395674000, 2 ],
        [ 60123, 1 ],
        [ 1, 1 ],
        [ 0, 1 ],
        [ "2023-12-31T23:00:00.000Z", 4 ],
        [ "2023-12-31T23:00:00.000Z", "Europe/Berlin", 1 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_QUARTER(@value)", { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%q')", { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_QUARTER(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%q', @timezone)", { date: value[0], timezone: value[1] }));
        }
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test DATE_DAYS_IN_MONTH function
    // //////////////////////////////////////////////////////////////////////////////

    testDateDaysInMonthInvalid () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH(null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH(false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH([])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH({})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH(0, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH(0, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH(0, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DAYS_IN_MONTH(0, {})");
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test DATE_DAYS_IN_MONTH function
    // //////////////////////////////////////////////////////////////////////////////

    testDateDaysInMonth () {
      const values = [
        [ "2000-04-29", 30 ],
        [ "2000-04-30Z", 30 ],
        [ "2000-12-24", 31 ],
        [ "2000-12-11Z", 31 ],
        [ "2100-12-01", 31 ],
        [ "2100-12-13Z", 31 ],
        [ "2400-12-01", 31 ],
        [ "2400-12-10Z", 31 ],
        [ "2012-02-12 13:24:12", 29 ],
        [ "2012-02-12 13:24:12Z", 29 ],
        [ "2012-02-12 23:59:59.991", 29 ],
        [ "2012-02-12 23:59:59.991Z", 29 ],
        [ "2012-02-12", 29 ],
        [ "2012-02-12Z", 29 ],
        [ "2012-02-12T13:24:12Z", 29 ],
        [ "2012-02-12Z", 29 ],
        [ "2012-2-12Z", 29 ],
        [ "1910-01-02T03:04:05Z", 31 ],
        [ "1910-01-02 03:04:05Z", 31 ],
        [ "1910-01-02", 31 ],
        [ "1910-01-02Z", 31 ],
        [ "1970-01-01T01:05:27", 31 ],
        [ "1970-01-01T01:05:27Z", 31 ],
        [ "1970-01-01 01:05:27Z", 31 ],
        [ "1970-1-1Z", 31 ],
        [ "1221-02-28T23:59:59Z", 28 ],
        [ "1221-02-28 23:59:59Z", 28 ],
        [ '2017-08-28', 31 ],
        [ '2017-09-11', 30 ],
        [ "1221-02-28Z", 28 ],
        [ "1221-2-28Z", 28 ],
        [ "1000-12-24T04:12:00Z", 31 ],
        [ "1000-12-24Z", 31 ],
        [ "1000-12-24 04:12:00Z", 31 ],
        [ "2016Z", 31 ],
        [ "2016z", 31 ],
        [ "2016", 31 ],
        [ "2016-1Z", 31 ],
        [ "2016-1z", 31 ],
        [ "2016-1-1z", 31 ],
        [ "2016-01-01Z", 31 ],
        [ "2016-01-01Z", 31 ],
        [ "  2016-01-01Z", 31 ],
        [ "  2016-01-01z", 31 ],
        [ 1399395674000, 31 ],
        [ 60123, 31 ],
        [ 1, 31 ],
        [ 0, 31 ],
        [ "2023-11-30T23:00:00.000Z", 30 ],
        [ "2023-11-30T23:00:00.000Z", "Europe/Berlin", 31 ]
      ];

      values.forEach(function (value) {
        if (value.length === 2) {
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_DAYS_IN_MONTH(@value)", { value: value[0] }));
          assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%a')", { value: value[0] }));
        } else {
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_DAYS_IN_MONTH(@date,@timezone)", { date: value[0], timezone: value[1] }));
          assertEqual([ value[2] ], getQueryResults("RETURN DATE_FORMAT(@date, '%a',@timezone)", {date: value[0], timezone: value[1]}));
        }
      });
    },

    // TODO: additional ISO duration tests for DATE_ADD() / DATE_SUBTRACT()
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_add function
    // //////////////////////////////////////////////////////////////////////////////

    testDateAddInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ADD()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ADD(1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(1, 'P1Y', 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(null, 1, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(false, 1, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD([], 1, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD({}, 1, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ADD(DATE_NOW(), 1, 'sugar')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ADD(DATE_NOW(), 1, '')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), null, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), false, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), [], 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), {}, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), 1, 'year', 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), 1, 'year', null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), 1, 'year', false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), 1, 'year', [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_ADD(DATE_NOW(), 1, 'year', {})");
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_add function
    // //////////////////////////////////////////////////////////////////////////////

    testDateAdd: function () {
      const values = [
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
        [ ["2012-02-12 23:59:59.991Z", 9, "f"], "2012-02-13T00:00:00.000Z" ],
        [ ["2012-02-12 23:59:59.991Z", "PT0.009S"], "2012-02-13T00:00:00.000Z" ],
        [ ["2012-02-12", "P1Y"], "2013-02-12T00:00:00.000Z" ],
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
        [ ["1221-02-28T23:59:59Z", 800 * 12, "months"], "2021-02-28T23:59:59.000Z" ],
        [ ["1221-02-28 23:59:59Z", 800, "years"], "2021-02-28T23:59:59.000Z" ],
        [ ["1221-02-28Z", 1000 * (60 * 60 * 24 - 1), "f"], "1221-02-28T23:59:59.000Z" ],
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
        [ ["2000-01-01", "PT0.000S"], "2000-01-01T00:00:00.000Z" ],
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
        [ [1, -1, "f"], "1970-01-01T00:00:00.000Z" ],
        [ [0, 0, "f"], "1970-01-01T00:00:00.000Z" ],
        [ ["2023-03-25T23:00:00.000Z", 1, "d", "Europe/Berlin"], "2023-03-26T22:00:00.000Z" ],
        [ [1679785200000, 1, "d", "Europe/Berlin"], "2023-03-26T22:00:00.000Z" ],
        [ ["2023-03-25T23:00:00.000Z", "P1D", "Europe/Berlin"], "2023-03-26T22:00:00.000Z" ],
        [ [1679785200000, "P1D", "Europe/Berlin"], "2023-03-26T22:00:00.000Z" ]
      ];

      values.forEach(function (value) {
        let dateParams = `${value[0].map((val, idx) => `@val${idx}`).join(', ')}`;
        let query = `RETURN DATE_ADD(${dateParams})`;
        let bindVars = value[0].reduce((prev, val, idx) => { prev[`val${idx}`] = val; return prev; }, {});
        let actual = getQueryResults(query, bindVars);
        assertEqual([ value[1] ], actual, `${query} using ${JSON.stringify(bindVars)}`);
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_subtract function
    // //////////////////////////////////////////////////////////////////////////////

    testDateSubtractInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_SUBTRACT()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_SUBTRACT(1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(1, 'P1Y', 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(null, 1, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(false, 1, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT([], 1, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT({}, 1, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_SUBTRACT(DATE_NOW(), 1, 'sugar')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_SUBTRACT(DATE_NOW(), 1, '')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(DATE_NOW(), null, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(DATE_NOW(), false, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(DATE_NOW(), [], 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(DATE_NOW(), {}, 'year')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(DATE_NOW(), 1, 'year', 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(DATE_NOW(), 1, 'year', null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(DATE_NOW(), 1, 'year', false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(DATE_NOW(), 1, 'year', [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_SUBTRACT(DATE_NOW(), 1, 'year', {})");
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_subtract function
    // //////////////////////////////////////////////////////////////////////////////

    testDateSubtract: function () {
      const values = [
        [ ["2000-05-01", 2, "days"], "2000-04-29T00:00:00.000Z" ],
        [ ["2000-05-01Z", "P2D"], "2000-04-29T00:00:00.000Z" ],
        [ ["2000-05-01Z", 2, "days"], "2000-04-29T00:00:00.000Z" ],
        [ ["2001-01-01", 1, "day"], "2000-12-31T00:00:00.000Z" ],
        [ ["2001-01-01Z", 1, "day"], "2000-12-31T00:00:00.000Z" ],
        [ ["2001-01-01Z", 1, "d"], "2000-12-31T00:00:00.000Z" ],
        [ ['2012-02-12 13:34:12Z', 0.5, "d"], "2012-02-12T01:34:12.000Z" ],
        [ ["2001-01-01Z", "P1D"], "2000-12-31T00:00:00.000Z" ],
        [ ["2101-03-31", "P3M"], "2100-12-31T00:00:00.000Z" ],
        [ ["2101-03-31", 3, "months"], "2100-12-31T00:00:00.000Z" ],
        [ ["2101-03-31Z", 3, "month"], "2100-12-31T00:00:00.000Z" ],
        [ ["2101-03-31", 3, "m"], "2100-12-31T00:00:00.000Z" ],
        [ ["2100-10-01Z", -3, "m"], "2101-01-01T00:00:00.000Z" ],
        [ ["2012-02-12 13:34:12", 10, "minutes"], "2012-02-12T13:24:12.000Z" ],
        [ ["2012-02-12 13:34:12Z", 10, "i"], "2012-02-12T13:24:12.000Z" ],
        [ ["2012-02-12 13:34:12Z", "PT10M"], "2012-02-12T13:24:12.000Z" ],
        [ ["2012-02-13 00:00:00.000", 9, "milliseconds"], "2012-02-12T23:59:59.991Z" ],
        [ ["2012-02-13 00:00:00.000", 9, "f"], "2012-02-12T23:59:59.991Z" ],
        [ ["2012-02-13 00:00:00.000Z", 9, "f"], "2012-02-12T23:59:59.991Z" ],
        [ ["2012-02-13 00:00:00.000Z", "PT0.009S"], "2012-02-12T23:59:59.991Z" ],
        [ ["2013-02-12", "P1Y"], "2012-02-12T00:00:00.000Z" ],
        [ ["2020-02-12", 8, "years"], "2012-02-12T00:00:00.000Z" ],
        [ ["2020-02-12Z", 8, "year"], "2012-02-12T00:00:00.000Z" ],
        [ ["2020-02-12T13:24:12Z", 8, "y"], "2012-02-12T13:24:12.000Z" ],
        [ ["2112-02-12Z", "P100Y"], "2012-02-12T00:00:00.000Z" ],
        [ ["1912-02-12Z", -100, "years"], "2012-02-12T00:00:00.000Z" ],
        [ ["1912-2-12Z", -100, "years"], "2012-02-12T00:00:00.000Z" ],
        [ ["1910-01-02T08:04:05Z", 5, "hours"], "1910-01-02T03:04:05.000Z" ],
        [ ["1910-01-02 08:04:05Z", 5, "hour"], "1910-01-02T03:04:05.000Z" ],
        [ ["1910-01-02 08:04:05Z", "PT5H"], "1910-01-02T03:04:05.000Z" ],
        [ ["1910-01-02 05:00:00", 5, "hours"], "1910-01-02T00:00:00.000Z" ],
        [ ["1910-01-02 05:00:00Z", 5, "hours"], "1910-01-02T00:00:00.000Z" ],
        [ ["1910-01-02 05:00:00Z", "PT5H"], "1910-01-02T00:00:00.000Z" ],
        [ ["2015-03-01Z", 1, "w"], "2015-02-22T00:00:00.000Z" ],
        [ ["2015-03-01Z", 1, "weeks"], "2015-02-22T00:00:00.000Z" ],
        [ ["2015-03-01Z", 1, "week"], "2015-02-22T00:00:00.000Z" ],
        [ ["2015-03-01Z", "P1W"], "2015-02-22T00:00:00.000Z" ],
        [ ["2016-02-29Z", 1, "week"], "2016-02-22T00:00:00.000Z" ],
        [ ["2016-02-29Z", "P1W"], "2016-02-22T00:00:00.000Z" ],
        [ ["2021-02-28T23:59:59Z", 800 * 12, "months"], "1221-02-28T23:59:59.000Z" ],
        [ ["2021-02-28 23:59:59Z", 800, "years"], "1221-02-28T23:59:59.000Z" ],
        [ ["1221-02-28 23:59:59Z", 1000 * (60 * 60 * 24 - 1), "f"], "1221-02-28T00:00:00.000Z" ],
        [ ["1221-3-01Z", 1, "day"], "1221-02-28T00:00:00.000Z" ],
        [ ["1221-3-01Z", "P1D"], "1221-02-28T00:00:00.000Z" ],
        [ ["2001-03-26 05:06:07.890", "P1Y2M3W4DT5H6M7.890S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2001-03-26 05:06:07.890", "P1Y2M3W4DT5H6M7.89S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-01-01 00:00:00.100", "PT0.1S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-01-01 00:00:00.100", "PT0.10S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-01-01 00:00:00.100", "PT0.100S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-01-01 00:00:00.010", "PT0.01S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-01-01 00:00:00.010", "PT0.010S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-01-01 00:00:00.001", "PT0.001S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2000-01-01 00:00:00.000", "PT0.000S"], "2000-01-01T00:00:00.000Z" ],
        [ ["2015-12-31 00:00:00.000", -1, "day"], "2016-01-01T00:00:00.000Z" ],
        [ ["2015-12-31 00:00:00.000z", -1, "day"], "2016-01-01T00:00:00.000Z" ],
        [ ["2015-12-31T00:00:00.000Z", -1, "day"], "2016-01-01T00:00:00.000Z" ],
        [ ["2016-1Z", -1, "day"], "2016-01-02T00:00:00.000Z" ],
        [ ["2016-1z", -1, "day"], "2016-01-02T00:00:00.000Z" ],
        [ ["2016-1-1z", -1, "day"], "2016-01-02T00:00:00.000Z" ],
        [ ["2016-01-01Z", -1, "day"], "2016-01-02T00:00:00.000Z" ],
        [ ["  2016-01-01Z", -1, "day"], "2016-01-02T00:00:00.000Z" ],
        [ ["  2016-01-01z", -1, "day"], "2016-01-02T00:00:00.000Z" ],
        [ [1399395674000, 365, "days"], "2013-05-06T17:01:14.000Z" ],
        [ [1430931674000, 365, "days"], "2014-05-06T17:01:14.000Z" ], /* leap year */
        [ [60123, 7, "days"], "1969-12-25T00:01:00.123Z" ],
        [ [1, -1, "f"], "1970-01-01T00:00:00.002Z" ],
        [ [0, 0, "f"], "1970-01-01T00:00:00.000Z" ],
        [ ["2023-03-26T22:00:00.000Z", 1, "d", "Europe/Berlin"], "2023-03-25T23:00:00.000Z" ], // timezoned
        [ [1679868000000, 1, "d", "Europe/Berlin"], "2023-03-25T23:00:00.000Z" ], // timezoned
        [ ["2023-03-26T22:00:00.000Z", "P1D", "Europe/Berlin"], "2023-03-25T23:00:00.000Z" ], // timezoned
        [ [1679868000000, "P1D", "Europe/Berlin"], "2023-03-25T23:00:00.000Z" ] // timezoned
      ];

      values.forEach(function (value) {
        let actual = getQueryResults(`RETURN DATE_SUBTRACT(${value[0].map((val, idx) => `@val${idx}`).join(', ')})`,
                                     value[0].reduce((prev, val, idx) => { prev[`val${idx}`] = val; return prev; }, {}));
        assertEqual([ value[1] ], actual);
      });
    },

    testDateDiffInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DIFF()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_DIFF(1, 1, 1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(null, 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, null, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, 1, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, 1, false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, 1, [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, 1, {})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, 1, 'y', null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, 1, 'y', [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, 1, 'y', {})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DIFF(1, 1, 'yo')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(false, 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, true, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF([], 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, [], 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF({}, 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_DIFF(1, {}, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DIFF('', 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_DIFF(1, '', 'y')");

      assertQueryError(errors.ERROR_INTERNAL.code, "RETURN DATE_DIFF(1, 1, 'y', 'Wrong Timezone' )");

      assertQueryError(errors.ERROR_INTERNAL.code, "RETURN DATE_DIFF(1, 1, 'y', false, 'Wrong Timezone' )");

      assertQueryError(errors.ERROR_INTERNAL.code, "RETURN DATE_DIFF(1, 1, 'y', false, 'Europe/Berlin','Wrong Timezone' )");
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_subtract function
    // //////////////////////////////////////////////////////////////////////////////

    testDateDiff: function () {
      const values = [
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "milliseconds", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "millisecond", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "f", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "seconds", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "second", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "s", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "minutes", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "minute", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "i", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "hours", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "hour", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "h", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "days", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "day", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "d", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "weeks", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "week", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "w", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "months", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "month", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "m", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "years", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "year", true], 0 ],
        [ ["2000-05-01T23:48:42.123", "2000-05-01T23:48:42.123Z", "y", true], 0 ],

        [ ["2000-05-01", "2000-05-01 00:00:00.500", "milliseconds", true], 500 ],
        [ ["2000-05-01", "2000-05-01 00:00:00.500", "millisecond", true], 500 ],
        [ ["2000-05-01", "2000-05-01 00:00:00.500", "f", true], 500 ],
        [ ["2000-05-01", "2000-05-01 00:00:59.500", "seconds", true], 59.5 ],
        [ ["2000-05-01", "2000-05-01 00:00:59.500", "second", true], 59.5 ],
        [ ["2000-05-01", "2000-05-01 00:01:59.500", "s", true], 119.5 ],
        [ ["2000-05-01", "2000-05-01 00:00:30.000", "minutes", true], 0.5 ],
        [ ["2000-05-01", "2000-05-01 00:00:30.000", "minute", true], 0.5 ],
        [ ["2000-05-01", "2000-05-01 00:00:30.000", "i", true], 0.5 ],
        [ ["2000-05-01", "2000-05-02 00:00:00.000", "minutes", true], 1440 ],
        [ ["2000-05-01", "2000-05-02 00:00:00.000", "minute", true], 1440 ],
        [ ["2000-05-01", "2000-05-02 00:00:00.000", "i", true], 1440 ],
        [ ["2000-05-01", "2000-05-02 00:00:00.000", "hours", true], 24 ],
        [ ["2000-05-01", "2000-05-02 00:00:00.000", "hour", true], 24 ],
        [ ["2000-05-01", "2000-05-02 00:00:00.000", "h", true], 24 ],
        [ ["2000-05-01", "2000-05-01 02:30:00.000", "hours", true], 2.5 ],
        [ ["2000-05-01", "2000-05-01 02:30:00.000", "hour", true], 2.5 ],
        [ ["2000-05-01", "2000-05-01 02:30:00.000", "h", true], 2.5 ],
        [ ["2000-05-01", "2000-05-03 12:00:00.000", "days", true], 2.5 ],
        [ ["2000-05-01", "2000-05-03 12:00:00.000", "day", true], 2.5 ],
        [ ["2000-05-01", "2000-05-03 12:00:00.000", "d", true], 2.5 ],
        [ ["2000-05-01", "2000-05-08 00:00:00.000", "weeks", true], 1 ],
        [ ["2000-05-01", "2000-05-08 00:00:00.000", "week", true], 1 ],
        [ ["2000-05-01", "2000-05-08 00:00:00.000", "w", true], 1 ],
        [ ["2000-05-01", "2000-06-01 00:00:00.000", "months", false], 1 ],
        [ ["2000-05-01", "2000-06-01 00:00:00.000", "month", false], 1 ],
        [ ["2000-05-01", "2000-06-01 00:00:00.000", "m", false], 1 ],
        [ ["2000-05-01", "2050-05-01 00:00:00.000", "years", false], 50 ],
        [ ["2000-05-01", "2050-05-01 00:00:00.000", "year", false], 50 ],
        [ ["2000-05-01", "2050-05-01 00:00:00.000", "y", false], 50 ],
        [ ["2000-05-01", "2400-05-01 00:00:00.000", "years", true], 400 ],
        [ ["2000-05-01", "2400-05-01 00:00:00.000", "year", true], 400 ],
        [ ["2000-05-01", "2400-05-01 00:00:00.000", "y", true], 400 ],
        [ ["2000-05-01", "2000-04-30 00:00:00.000", "days", true], -1 ],
        [ ["2000-05-01", "2000-04-30 18:00:00.000", "days", true], -0.25 ],

        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", true], 1/24 ], // #60
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", true], 1 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", false], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", false], 1 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours"], 1 ],// #65
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", true, "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", true, "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", false, "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", false, "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", "Europe/Berlin"], 0 ],// #70
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", true, "UTC", "Europe/Berlin"], 2/24 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", true, "UTC", "Europe/Berlin"], 2 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", false, "UTC", "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", false, "UTC", "Europe/Berlin"], 2 ],// #75
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", "UTC", "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", "UTC", "Europe/Berlin"], 2 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", true, "Europe/Berlin", "UTC"], -1/24 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", true, "Europe/Berlin", "UTC"], -1 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", false, "Europe/Berlin", "UTC"], 0 ],// #80
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", false, "Europe/Berlin", "UTC"], -1 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", "Europe/Berlin", "UTC"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", "Europe/Berlin", "UTC"], -1 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", true, "Europe/Berlin", "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", true, "Europe/Berlin", "Europe/Berlin"], 0 ],// #85
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", false, "Europe/Berlin", "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", false, "Europe/Berlin", "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", "Europe/Berlin", "Europe/Berlin"], 0 ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "hours", "Europe/Berlin", "Europe/Berlin"], 0 ]
      ];

      values.forEach(function (value) {
        let actual = getQueryResults(`RETURN DATE_DIFF(${value[0].map((val, idx) => `@val${idx}`).join(', ')})`,
                                     value[0].reduce((prev, val, idx) => { prev[`val${idx}`] = val; return prev; }, {}));
        assertEqual([ value[1] ], actual);
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_compare function
    // //////////////////////////////////////////////////////////////////////////////

    testDateCompareInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_COMPARE()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(null, 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, null, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'yo')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(false, 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, true, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE([], 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, [], 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE({}, 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, {}, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', {})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', {})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', 'Europe/Berlin',  null)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', 'Europe/Berlin',  false)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', 'Europe/Berlin',  [])");

      assertQueryWarningAndNull(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', 'Europe/Berlin',  {})");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_COMPARE('', 1, 'y')");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_COMPARE(1, '', 'y')");

      assertQueryError(errors.ERROR_INTERNAL.code, "RETURN DATE_COMPARE(1, 1, 'y', 'Wrong Timezone')");

      assertQueryError(errors.ERROR_INTERNAL.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', 'Wrong Timezone')");

      assertQueryError(errors.ERROR_INTERNAL.code, "RETURN DATE_COMPARE(1, 1, 'y', 'y', 'Europe/Berlin', 'Wrong Timezone')");
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_compare function
    // //////////////////////////////////////////////////////////////////////////////

    testDateCompare: function () {
      const values = [
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "years"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-06-25T12:13:14.156", "years"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "year"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-06-25T12:13:14.156", "year"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "y"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-06-25T12:13:14.156", "y"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "months"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:13:14.156", "months"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "month"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:13:14.156", "month"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "m"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:13:14.156", "m"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "days"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-26T12:13:14.156", "days"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "day"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-26T12:13:14.156", "day"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "d"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-26T12:13:14.156", "d"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "hours"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T13:13:14.156", "hours"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "hour"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T13:13:14.156", "hour"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "h"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T13:13:14.156", "h"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "minutes"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:14:14.156", "minutes"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "minute"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:14:14.156", "minute"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "i"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:14:14.156", "i"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:13:15.156", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "second"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:13:15.156", "second"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "s"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:13:15.156", "s"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:13:14.157", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "millisecond"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:13:14.157", "millisecond"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "f"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-05-25T12:13:14.157", "f"], false ],

        [ ["2010-06-25T12:13:14.156", "2010-06-25T13:14:15.157", "years", "months"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "months"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-06-25T13:14:15.157", "months", "days"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "days"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-25T12:14:15.157", "days", "hours"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "days", "hours"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T12:13:15.157", "hours", "minutes"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "hours", "minutes"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-25T12:13:14.157", "minutes", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "minutes", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:14.156", "seconds", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "seconds", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T13:14:15.157", "years", "days"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "days"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-06-25T12:14:15.157", "months", "hours"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "hours"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-25T12:13:15.157", "days", "minutes"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "days", "minutes"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T12:13:14.157", "hours", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "hours", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:13:14.156", "minutes", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "minutes", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:14:15.157", "years", "hours"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "hours"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-06-25T12:13:15.157", "months", "minutes"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "minutes"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-25T12:13:14.157", "days", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "days", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T12:13:14.156", "hours", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "hours", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:15.157", "years", "minutes"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "minutes"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-06-25T12:13:14.157", "months", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-07-25T12:13:14.156", "days", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "days", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.157", "years", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2011-06-25T12:13:14.156", "months", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "years", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "milliseconds"], false ],

        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "years", "months"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "months"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "months", "days"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "days"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "days", "hours"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "days", "hours"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "hours", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "hours", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "minutes", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "minutes", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "years", "days"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "days"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "months", "hours"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "hours"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "days", "minutes"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "days", "minutes"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "hours", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "hours", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "minutes", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "minutes", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "years", "hours"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "hours"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "months", "minutes"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "minutes"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "days", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "days", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "hours", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "hours", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "years", "minutes"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "minutes"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "months", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "days", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "days", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "years", "seconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "seconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "months", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "months", "milliseconds"], false ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156", "years", "milliseconds"], true ],
        [ ["2010-06-25T12:13:14.156", "2011-07-26T13:14:15.157", "years", "milliseconds"], false ],

        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156+02:00", "days", "milliseconds", "UTC", "Europe/Berlin"], true ],
        [ ["2010-06-25T12:13:14.156+02:00", "2010-06-25T12:13:14.156", "days", "milliseconds", "Europe/Berlin", "UTC"], true ],
        [ ["2010-06-25T12:13:14.156", "2010-06-25T12:13:14.156+02:00", "days", "UTC", "Europe/Berlin"], true ],
        [ ["2010-06-25T12:13:14.156+02:00", "2010-06-25T12:13:14.156", "days", "Europe/Berlin", "UTC"], true ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", "milliseconds"], false ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", "milliseconds", "UTC"], false ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", "milliseconds", "Europe/Berlin"], true ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", "Europe/Berlin"], true ],
        [ ["2023-10-29T00:00:00.000Z", "2023-10-29T01:00:00.000Z", "days", "UTC"], true ]
      ];

      values.forEach(function (value) {
        let actual = getQueryResults(`RETURN DATE_COMPARE(${value[0].map((val, idx) => `@val${idx}`).join(', ')})`,
                                     value[0].reduce((prev, val, idx) => { prev[`val${idx}`] = val; return prev; }, {}));
        assertEqual([ value[1] ], actual);
      });
    },

    // TODO: DATE_FORMAT()

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_timestamp function
    // //////////////////////////////////////////////////////////////////////////////

    testDateTimestampInvalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_TIMESTAMP()");

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_TIMESTAMP(1, 1, 1, 1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_TIMESTAMP(-1, 1, 1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_TIMESTAMP(1, -1, 1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_TIMESTAMP(1, 1, -1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_TIMESTAMP(1, 1, 1, -1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_TIMESTAMP(1, 1, 1, 1, -1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_TIMESTAMP(1, 1, 1, 1, 1, -1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_TIMESTAMP(1, 1, 1, 1, 1, 1, -1)");
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_timestamp function
    // //////////////////////////////////////////////////////////////////////////////

    testDateTimestamp: function () {
      const values = [
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
        [ "6789-12-31T23:59:58.099Z", 152104521598099 ],
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
        assertEqual([ value[1] ], getQueryResults("RETURN DATE_TIMESTAMP(@value)", { value: value[0] }));
        assertEqual([ value[1] ], getQueryResults("RETURN DATE_FORMAT(@value, '%t')", { value: value[0] }));
      });
    },

    testDateTimestampAlternative: function () {
      const values = [
        [ [2000, 4, 29], 956966400000 ],
        [ [2012, 2, 12, 13, 24, 12], 1329053052000 ],
        [ [2012, 2, 12, 23, 59, 59, 991], 1329091199991 ],
        [ [2012, 2, 12], 1329004800000 ],
        [ [2012, 2, 12, 13, 24, 12], 1329053052000 ],
        [ [2012, 2, 12], 1329004800000 ],
        [ [1910, 1, 2, 3, 4, 5], -1893358555000 ],
        [ [1910, 1, 2], -1893369600000 ],
        [ [1970, 1, 1, 1, 5, 27], 3927000 ],
        [ [1970, 1, 1, 12, 5, 27], 43527000 ],
        [ [1970, 1, 1], 0 ],
        [ [1221, 2, 28, 23, 59, 59], -23631004801000 ],
        [ [1221, 2, 28, 23, 59, 59], -23631004801000 ],
        [ [1221, 2, 28], -23631091200000 ],
        [ [1221, 2, 28], -23631091200000 ],
        [ [1000, 12, 24, 4, 12, 0], -30579364080000 ],
        [ [1000, 12, 24], -30579379200000 ],
        [ [1000, 12, 24, 4, 12, 0], -30579364080000 ],
        [ [6789, 12, 31, 23, 59, 58, 99], 152104521598099 ],
        [ [6789, 12, 31], 152104435200000 ],
        [ [9999, 12, 31, 23, 59, 59, 999], 253402300799999 ],
        [ [9999, 12, 31], 253402214400000 ],
        [ [9999, 12, 31], 253402214400000 ],
        [ [9999, 12, 31], 253402214400000 ],

        [ ["2000", "4", "29"], 956966400000 ],
        [ ["2012", "2", "12", "13", "24", "12"], 1329053052000 ],
        [ ["2012", "2", "12", "23", "59", "59", "991"], 1329091199991 ],
        [ ["2012", "2", "12"], 1329004800000 ],
        [ ["2012", "2", "12", "13", "24", "12"], 1329053052000 ],
        [ ["2012", "2", "12"], 1329004800000 ],
        [ ["1910", "1", "2", "3", "4", "5"], -1893358555000 ],
        [ ["1910", "1", "2"], -1893369600000 ],
        [ ["1970", "1", "1", "1", "5", "27"], 3927000 ],
        [ ["1970", "1", "1", "12", "5", "27"], 43527000 ],
        [ ["1970", "1", "1"], "0" ],
        [ ["1221", "2", "28", "23", "59", "59"], -23631004801000 ],
        [ ["1221", "2", "28", "23", "59", "59"], -23631004801000 ],
        [ ["1221", "2", "28"], -23631091200000 ],
        [ ["1221", "2", "28"], -23631091200000 ],
        [ ["1000", "12", "24", "4", "12", "0"], -30579364080000 ],
        [ ["1000", "12", "24"], -30579379200000 ],
        [ ["1000", "12", "24", "4", "12", "0"], -30579364080000 ],
        [ ["6789", "12", "31", "23", "59", "58", "99"], 152104521598099 ],
        [ ["6789", "12", "31"], 152104435200000 ],
        [ ["9999", "12", "31", "23", "59", "59", "999"], 253402300799999 ],
        [ ["9999", "12", "31"], 253402214400000 ],
        [ ["9999", "12", "31"], 253402214400000 ],
        [ ["9999", "12", "31"], 253402214400000 ]
      ];

      values.forEach(function (value) {
        let query = "RETURN DATE_TIMESTAMP(" +
            value[0].map(function (v) {
              return JSON.stringify(v);
            }).join(", ") + ")";
        assertEqual([ value[1] ], getQueryResults(query));
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_iso8601 function
    // //////////////////////////////////////////////////////////////////////////////

    testDateIso8601Invalid: function () {
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISO8601()");
      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code, "RETURN DATE_ISO8601(1, 1, 1, 1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(-1, 1, 1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(1, -1, 1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(1, 1, -1, 1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(1, 1, 1, -1, 1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(1, 1, 1, 1, -1, 1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(1, 1, 1, 1, 1, -1, 1)");

      assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(1, 1, 1, 1, 1, 1, -1)");

      const values = [
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
        "2001-02-11 23:01:38.abc",
        "2001-02-11 23:01:38.",
        "2001-02-11 23:01:38.0a",
        "2001-02-11 23:01:38.01247754875767x",
        "2001-02-11 23:01:38.abcb",
        "2001-00-11",
        "2001-0-11",
        "2001-13-11",
        "2001-13-32",
        "2001-01-0",
        "2001-01-00",
        "2001-01-32",
        "2001-1-32"
      ];

      values.forEach(function (value) {
        assertQueryWarningAndNull(errors.ERROR_QUERY_INVALID_DATE_VALUE.code, "RETURN DATE_ISO8601(@value)", { value: value });
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_iso8601 function
    // //////////////////////////////////////////////////////////////////////////////

    testDateIso8601: function () {
      var values = [
        [ "2000-04-29", "2000-04-29T00:00:00.000Z" ],
        [ "2000-04-29Z", "2000-04-29T00:00:00.000Z" ],
        [ "2012-02-12 13:24:12", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12 13:24:12Z", "2012-02-12T13:24:12.000Z" ],
        [ "2012-02-12 23:59:59.991", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.991Z", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.9910", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.9910Z", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.99100", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.99100Z", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.9995454563666664", "2012-02-12T23:59:59.999Z" ],
        [ "2012-02-12 23:59:59.9995454563666664Z", "2012-02-12T23:59:59.999Z" ],
        [ "2012-02-12 23:59:59.9910000000000000000000000000000", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.9910000000000000000000000000000Z", "2012-02-12T23:59:59.991Z" ],
        [ "2012-02-12 23:59:59.1234757476945568239468685238468253868852346858888578293469929395769204679023067056237562573Z", "2012-02-12T23:59:59.123Z" ],
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
        assertEqual([ value[1] ], getQueryResults("RETURN DATE_ISO8601(@value)", { value: value[0] }));
      });
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_utctolocal and date_localtoutc functions
    // //////////////////////////////////////////////////////////////////////////////

    testDateUtcToLocalAndLocalToUtc: function () {
      const withoutZoneInfo = [
        [ "2020-03-15T01:00:00.000", "Europe/Berlin", "2020-03-15T00:00:00.000Z"],
        [ "2020-04-15T02:00:00.000", "Europe/Berlin", "2020-04-15T00:00:00.000Z"],
        [ "2020-10-14T21:00:00.999", "America/New_York", "2020-10-15T01:00:00.999Z"],
        [ "2020-11-14T20:00:00.999", "America/New_York", "2020-11-15T01:00:00.999Z"],
        [ "1991-09-10T09:00:00.999", "Asia/Shanghai", "1991-09-10T00:00:00.999Z"],
        [ "1991-09-20T08:00:00.999", "Asia/Shanghai", "1991-09-20T00:00:00.999Z"],
        [ "1992-09-10T08:00:00.999", "Asia/Shanghai", "1992-09-10T00:00:00.999Z"],
        [ "1992-09-20T08:00:00.999", "Asia/Shanghai", "1992-09-20T00:00:00.999Z"],        
        [ "2020-07-07T09:30:00.333", "Australia/Darwin", "2020-07-07T00:00:00.333Z" ],
        [ "2020-07-07T08:45:00.333", "Australia/Eucla", "2020-07-07T00:00:00.333Z" ],
        [ "2020-04-15T06:06:06.111Z", "UTC", "2020-04-15T06:06:06.111Z" ],
        [ "2020-07-06T18:00:00.333", "Etc/GMT+6", "2020-07-07T00:00:00.333Z" ],
        [ "2020-07-07T06:00:00.333", "Etc/GMT-6", "2020-07-07T00:00:00.333Z" ]
      ];

      withoutZoneInfo.forEach(function (value) {
        assertEqual([ value[0] ], getQueryResults("RETURN DATE_UTCTOLOCAL(@value,@tz)", { value: value[2], tz: value[1] }));
        assertEqual([ value[2] ], getQueryResults("RETURN DATE_LOCALTOUTC(@value,@tz)", { value: value[0], tz: value[1] }));
      });
            
      assertEqual([ "2020-02-29T23:00:00.000Z" ], 
        getQueryResults("RETURN DATE_LOCALTOUTC(DATE_ADD(DATE_UTCTOLOCAL('2020-01-31T23:00:00.000Z', 'Europe/Berlin'), 1, 'months'), 'Europe/Berlin')"));

	  const withZoneInfoToLocal = [
        [ "2021-01-01", "Europe/Berlin", {
            local: "2021-01-01T01:00:00.000",
            zoneInfo: {
              name: "CET",
              begin: "2020-10-25T01:00:00.000Z",
              end: "2021-03-28T01:00:00.000Z",
              dst: false,
              offset: 3600
            }
          }
        ], [ "2021-07-01", "Europe/Berlin", {
            local: "2021-07-01T02:00:00.000",
            zoneInfo: {
              name: "CEST",
              begin: "2021-03-28T01:00:00.000Z",
              end: "2021-10-31T01:00:00.000Z",
              dst: true,
              offset: 7200
            }
          }
        ], [ "2021-01-01", "Etc/UTC", {
            local: "2021-01-01T00:00:00.000Z",
            zoneInfo: {
              name: "UTC",
              begin: null,
              end: null,
              dst: false,
              offset: 0
            }
          }
        ]
      ];

      withZoneInfoToLocal.forEach(function (value) {
        var res = getQueryResults("RETURN DATE_UTCTOLOCAL(@value,@tz,@detail)", { value: value[0], tz: value[1], detail: true })[0];
        // ignore tzdata version
        delete res.tzdb;
        assertEqual(value[2], res);
      });
      
      const withZoneInfoToUTC = [
        [ "2021-01-01T00:00:00.000Z", "America/Los_Angeles", {
            utc: "2021-01-01T08:00:00.000Z",
            zoneInfo: {
              name: "PST",
              begin: "2020-11-01T09:00:00.000Z",
              end: "2021-03-14T10:00:00.000Z",
              dst: false,
              offset: -28800
            }
          }
        ], [ "2021-08-01T00:00:00.000Z", "America/Los_Angeles", {
            utc: "2021-08-01T07:00:00.000Z",
            zoneInfo: {
              name: "PDT",
              begin: "2021-03-14T10:00:00.000Z",
              end: "2021-11-07T09:00:00.000Z",
              dst: true,
              offset: -25200
            }
          }
        ], [ "2021-01-01", "Etc/UTC", {
            utc: "2021-01-01T00:00:00.000Z",
            zoneInfo: {
              name: "UTC",
              begin: null,
              end: null,
              dst: false,
              offset: 0
            }
          }
        ]
      ];

      withZoneInfoToUTC.forEach(function (value) {
        var res = getQueryResults("RETURN DATE_LOCALTOUTC(@value,@tz,@detail)", { value: value[0], tz: value[1], detail: true })[0];
        // ignore tzdata version
		delete res.tzdb;
        assertEqual(value[2], res);
      });	
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_timezone
    // //////////////////////////////////////////////////////////////////////////////

    testDateTimeZone: function () {
      let systemtz = null;
      let res = getQueryResults("RETURN DATE_TIMEZONE()", {})[0];
      
      try {
        systemtz = Intl.DateTimeFormat().resolvedOptions().timeZone;
      } catch (err) {
      }
        
      if (systemtz) {
        // normalize UTC timezone names
        if (systemtz.match(/UTC/)) {
          systemtz = "UTC";
        } 
        if (res.match(/UTC/)) {
          res = "UTC";
        }
        assertEqual(systemtz, res);
      } else {
        assertNotNull(res);
      }
    },
    
    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_timezones
    // //////////////////////////////////////////////////////////////////////////////
    
    testDateTimeZones: function () {
      let res = getQueryResults("RETURN DATE_TIMEZONES()", {})[0];
      assertNotEqual(-1, res.indexOf("America/New_York"));
      assertNotEqual(-1, res.indexOf("Europe/Berlin"));
      assertNotEqual(-1, res.indexOf("Asia/Shanghai"));
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date_iso8601 function
    // //////////////////////////////////////////////////////////////////////////////

    testDateIso8601Alternative: function () {
      const values = [
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
        [ [ "1000", "1", "1" ], "1000-01-01T00:00:00.000Z" ]
      ];

      values.forEach(function (value) {
        let query = "RETURN DATE_ISO8601(" +
            value[0].map(function (v) {
              return JSON.stringify(v);
            }).join(", ") + ")";
        assertEqual([ value[1] ], getQueryResults(query));
      });
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date transformations
    // //////////////////////////////////////////////////////////////////////////////

    testDateTransformations1: function () {
      var dt = "2014-05-07T15:23:21.446";
      var actual;

      actual = getQueryResults("RETURN DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value))", { value: dt });
      assertEqual([ new Date(dt+"Z").getTime() ], actual);

      actual = getQueryResults("RETURN DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value))", { value: dt + "Z" });
      assertEqual([ new Date(dt+"Z").getTime() ], actual);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test date transformations
    // //////////////////////////////////////////////////////////////////////////////

    testDateTransformations2: function () {
      var dt = "2014-05-07T15:23:21.446";
      var actual;

      actual = getQueryResults("RETURN DATE_ISO8601(DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value)))", { value: dt });
      assertEqual([ dt + "Z" ], actual);

      actual = getQueryResults("RETURN DATE_ISO8601(DATE_TIMESTAMP(DATE_YEAR(@value), DATE_MONTH(@value), DATE_DAY(@value), DATE_HOUR(@value), DATE_MINUTE(@value), DATE_SECOND(@value), DATE_MILLISECOND(@value)))", { value: dt + "Z" });
      assertEqual([ dt + "Z" ], actual);
    }

  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlDateFunctionsTestSuite);

return jsunity.done();
