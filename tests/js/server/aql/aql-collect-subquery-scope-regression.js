/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function subqueryCollectScopeSuite() {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test regression with incorrect scoping of collect variable
///        inside subquery
////////////////////////////////////////////////////////////////////////////////

    testSubqueryCollectScopeOk : function () {
      const query = `LET t = (COLLECT foo = undefined RETURN true) RETURN true`;
      try {
        var actual = db._query(query);
        fail();
      } catch(e) {
        assertEqual(e.errorNum, errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      }
    },

    testSubqueryCollectScopeOk2 : function () {
      const query = `LET t = (COLLECT foo = 5 RETURN true) RETURN true`;
      var actual = db._query(query);
      assertEqual(actual.toArray(), [ true ]);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(subqueryCollectScopeSuite);

return jsunity.done();

