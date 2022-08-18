/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertLess, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2021 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2021, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = require("@arangodb").db;

// Regression test suite for https://github.com/arangodb/arangodb/issues/15107
function subquerySplicingCrashRegressionSuite() {
  return {
    setUpAll: function() {},
    tearDownAll: function() {},

    testSubqueryEndHitShadowRowFirst: function() {
      // This tests needs to fill an AQL itemBlock, s.t. one shadowRow
      // for the subqueryEnd is the start of the next AQLItemBlock
      // This will cause the upstream on subquery to be "DONE" but
      // Bot not on the main query.
      const query = `
        FOR record in 1..2000
          LET response =(LET in_out_base = (FOR r_h_ind in 0..1
                                              FILTER false
                                              RETURN { } )
                   LET foo = (RETURN LENGTH(in_out_base))
                   RETURN [] )
       RETURN response`;
      // Data
      const q = db._query(query, {}, { optimizer: { rules: [ "-all" ] } } );
      const res = q.toArray();
    },
  };
}

jsunity.run(subquerySplicingCrashRegressionSuite);

return jsunity.done();
