/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, fail, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for "failOnWarning"
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
var jsunity = require("jsunity");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlFailOnWarningTestSuite () {
  var errors = internal.errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test disabled
////////////////////////////////////////////////////////////////////////////////
    
    testDisabledConst : function () {
      var actual = AQL_EXECUTE("RETURN NOOPT(1 / 0)", null, { failOnWarning: false });
      assertEqual([ null ], actual.json);
      assertEqual(1, actual.warnings.length);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, actual.warnings[0].code);
    },
    
    testDisabledNonConst : function () {
      var actual = AQL_EXECUTE("FOR i IN 1..2 RETURN i / 0", null, { failOnWarning: false });
      assertEqual([ null, null ], actual.json);
      assertEqual(2, actual.warnings.length);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, actual.warnings[0].code);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, actual.warnings[1].code);
    },
    
    testDisabledNonConstMaxWarnings : function () {
      var actual = AQL_EXECUTE("FOR i IN 1..5 RETURN i / 0", null, { failOnWarning: false, maxWarningCount: 3 });
      assertEqual([ null, null, null, null, null ], actual.json);
      assertEqual(3, actual.warnings.length);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, actual.warnings[0].code);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, actual.warnings[1].code);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, actual.warnings[2].code);
    },

    testEnabledConst : function () {
      try {
        AQL_EXECUTE("RETURN 1 / 0", null, { failOnWarning: true }).json;
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, err.errorNum);
      }
    },

    testEnabledNonConst : function () {
      try {
        AQL_EXECUTE("FOR i IN 1..2 RETURN i / 0", null, { failOnWarning: true }).json;
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, err.errorNum);
      }
    },

    testEnabledArrayExpected : function () {
      try {
        AQL_EXECUTE("RETURN MEDIAN(7)", null, { failOnWarning: true }).json;
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_ARRAY_EXPECTED.code, err.errorNum);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlFailOnWarningTestSuite);

return jsunity.done();

