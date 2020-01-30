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
var db = require("@arangodb").db, indexId;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function blockReturnRegressionSuite() {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test WITHIN_RECTANGLE as result
////////////////////////////////////////////////////////////////////////////////

    testBlockReuseOkWithSubquery : function () {
      const query = `FOR c in 1..1000 LET su = (FOR d in 1..1000 SORT d RETURN d) RETURN LENGTH(su)`;
      var actual = db._query(query);
      assertEqual(actual.toArray().length , 1000);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(blockReturnRegressionSuite);

return jsunity.done();

