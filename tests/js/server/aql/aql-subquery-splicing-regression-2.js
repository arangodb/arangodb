/*jshint globalstrict:false, strict:false, maxlen: 500 */

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
const {assertEqual, assertTrue} = jsunity.jsUnity.assertions;
const db = require("@arangodb").db;
const _ = require('lodash');

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

// Regression test for https://github.com/arangodb/arangodb/issues/16451
function subquerySplicingLimitDroppingOuterRowsSuite() {
  return {
    testNestedSubqueryInnerLimitDroppingOuterRows: function () {
      // In the noted bug, the second SQS node (which starts the sqInner subquery)
      // got confused when called to write its third batch, misinterpreting the
      // already saturated limit to also drop remaining rows of the outermost query.
      // This lead to a result of size 666.
      const query = `
        FOR i IN 1..1000
        LET sqOuter = (
            LET sqInner = (RETURN null)
            LIMIT 1
          RETURN null)
        RETURN null`;
      const q = db._query(query, {}, { optimizer: { rules: [ "-all" ] } } );
      const res = q.toArray();
      assertEqual(1000, res.length);
    },

    testSubqueryEnd: function () {
      // This brings the SubqueryEnd node in a similar situation as the previous
      // test does the SubqueryStart. It's not a regression test (at the point of
      // writing, it already worked correctly), but just for completeness' sake.

      const query = `
        FOR i IN 1..1000
          LET sq2 = (
            FOR k IN 1..3
              LET sq1 = (
                LET sq0 = (RETURN null)
                LIMIT 1
                RETURN null
              )
            RETURN null
          )
        RETURN null`;

      const q = db._query(query, {}, { optimizer: { rules: [ "-all" ] } } );
      const res = q.toArray();
      assertEqual(1000, res.length);
    },
  };
}

// Regression test for https://arangodb.atlassian.net/browse/BTS-1610
function subquerySplicingNonRelevantShadowRowForwardingAtBlockStart() {
  return {
    testRegressionBTS1610: function () {
      // When an item block starts with a non-relevant shadow row, calls with a lower depth than that need to be popped
      // from the call lists. This didn't happen, causing BTS-1610.
      const query = `
        FOR i IN 1..2
        LET outer = (
          // start with an outer shadow row
          FILTER i == 2
          LET inner = FIRST(
            // the subquery here has to forward the outer shadow rows and return them before getting more from upstream:
            // this holds for both start and end nodes.
            LET innermost = (FILTER false RETURN 1)
            // this calculation node would trigger a maintainer mode assertion (or return too few results in production)
            // when its input block didn't end with the outer shadow row, but instead contained more rows.
            LET x = innermost[0]
            // TODO add an additional test with UPDATE here
            FILTER x == null
            COLLECT WITH COUNT INTO n
            RETURN 1
          )
          RETURN inner
        )
        RETURN outer
      `;
      const res = db._query(query, {}, {optimizer: {rules: ["-all"]}, profile: 4}).toArray();
      assertTrue(_.isEqual(res, [[], [1]]), "Expected [[], [1]], got " + JSON.stringify(res));
    },
  };
}

jsunity.run(subquerySplicingCrashRegressionSuite);
jsunity.run(subquerySplicingLimitDroppingOuterRowsSuite);
jsunity.run(subquerySplicingNonRelevantShadowRowForwardingAtBlockStart);

return jsunity.done();
