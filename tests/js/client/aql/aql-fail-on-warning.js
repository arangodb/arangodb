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

var internal = require("internal");
var jsunity = require("jsunity");
const db = require('internal').db;

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
      var stmt = db._createStatement({query: "RETURN NOOPT(1 / 0)", bindVars: null, options: { failOnWarning: false }});
      let res = stmt.execute();
      assertEqual([ null ], res.toArray());
      let extra = res.getExtra();
      assertEqual(1, extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, extra.warnings[0].code);
    },
    
    testDisabledNonConst : function () {
      var stmt = db._createStatement({query: "FOR i IN 1..2 RETURN i / 0", bindVars: null, options: { failOnWarning: false }});
      let res = stmt.execute();
      assertEqual([ null, null ], res.toArray());
      let extra = res.getExtra();
      assertEqual(2, extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, extra.warnings[0].code);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, extra.warnings[1].code);
    },
    
    testDisabledNonConstMaxWarnings : function () {
      var stmt = db._createStatement({query: "FOR i IN 1..5 RETURN i / 0", bindVars: null, options: { failOnWarning: false, maxWarningCount: 3 }});
      let res = stmt.execute();
      assertEqual([ null, null, null, null, null ], res.toArray());
      let extra = res.getExtra();
      assertEqual(3, extra.warnings.length);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, extra.warnings[0].code);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, extra.warnings[1].code);
      assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, extra.warnings[2].code);
    },

    testEnabledConst : function () {
      try {
        db._query("RETURN 1 / 0", null, { failOnWarning: true }).toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, err.errorNum);
      }
    },

    testEnabledNonConst : function () {
      try {
        db._query("FOR i IN 1..2 RETURN i / 0", null, { failOnWarning: true }).toArray();
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, err.errorNum);
      }
    },

    testEnabledArrayExpected : function () {
      try {
        db._query("RETURN MEDIAN(7)", null, { failOnWarning: true }).toArray();
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

