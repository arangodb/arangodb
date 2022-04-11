/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
///
/// Fuzzing tests for nested subquery execution. Generates random nested subqueries
/// and then runs spliced subqueries against "old style" subqueries and compares
/// results.
///
/// DISCLAIMER
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
////////////////////////////////////////////////////////////////////////////////
///
/// This test runs randomly generated queries, but prints the code that is
/// executed to run the test, so if you find yourself in the unfortunate situation
/// that you have to debug a failing instance of this test, just copy and paste
/// the line that preceded the crash/failure into arangosh or an arangod console
/// and you should reproduce what happened in the random run.
///

const jsunity = require("jsunity");
const ct = require("@arangodb/test-generators/subquery-chaos-test");
const helper = require("@arangodb/aql-helper");

const numberOfQueriesGenerated = 100;
const randomDepth = () => {
  const val = Math.random();
  if (val < 0.3) {
    // 30% depth 2
    return 2;
  }
  if (val < 0.5) {
    // 20% depth 3
    return 3;
  }
  if (val < 0.7) {
    // 20% depth 4
    return 4;
  }
  if (val < 0.8) {
    // 10% depth 5
    return 5;
  }
  if (val < 0.9) {
    // 10% depth 6
    return 6;
  }
  return 7;
};

const randomModificationDepth = () => {
  const val = Math.random();
  if (val < 0.7) {
    // 70% depth 2
    return 2;
  }
  // 30% depth 3
  return 3;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for cross-collection queries
////////////////////////////////////////////////////////////////////////////////

function ahuacatlSubqueryChaos() {
  /// Some queries that caused errors before. We don't have the seeds
  /// to create them, unfortunately, so we test them in here verbatim.
  const specificQueries = {
    q1: {
      queryString: `FOR x IN 1..10
           LET sub = (FOR y in 1..10 RETURN y)
           LIMIT 3, 0
           RETURN sub`,
      collectionNames: [],
    },
    q2: {
      queryString: `FOR x IN 1..10
           LET sub = (FOR y in 1..10 RETURN y)
           LIMIT 3, 1
           RETURN sub`,
      collectionNames: [],
    },
    q3: {
      queryString: `FOR fv62 IN 1..100
          LET sq63 = (FOR fv64 IN 1..100
            LET sq65 = (FOR fv66 IN 1..100
              LET sq67 = (FOR fv68 IN 1..100
                          LIMIT 11,19
                          RETURN {fv68})
              LET sq69 = (FOR fv70 IN 1..100
                LET sq71 = (FOR fv72 IN 1..100
                            LIMIT 12,10
                            RETURN {fv72, sq67})
                         FILTER fv70 < 7
                         LIMIT 11,0
                         RETURN {fv70, sq67, sq71})
                  LIMIT 2,6
                  RETURN {fv66, sq67, sq69})
            LET sq73 = (FOR fv74 IN 1..100
                        LIMIT 15,15
                        RETURN {fv74, sq65})
            FILTER fv62 < 13
            LIMIT 4,13
            COLLECT WITH COUNT INTO counter
            RETURN {counter})
          LIMIT 6,19
          RETURN {fv62, sq63}`,
      collectionNames: [],
    },
    q4: {
      queryString: `FOR fv0 IN 1..20
           LET sq1 = (FOR fv2 IN 1..20
             LET sq3 = (FOR fv4 IN 1..20
               LET sq5 = (FOR fv6 IN 1..20
                 LIMIT 2,12
                 COLLECT WITH COUNT INTO counter
                 RETURN {counter})
               LIMIT 4,0
               COLLECT WITH COUNT INTO counter
               RETURN {counter})
             FILTER fv2 < 1
             LIMIT 18,8
             RETURN {fv2, sq3})
           LET sq7 = (FOR fv8 IN 1..20
             LET sq9 = (FOR fv10 IN 1..20
               LET sq11 = (FOR fv12 IN 1..20
                 LIMIT 7,3
                 RETURN {fv12, sq1})
               FILTER fv0 < 9
               LIMIT 2,6
               RETURN {fv10, sq1, sq11})
             LIMIT 17,15
             RETURN {fv8, sq1, sq9})
           LIMIT 13,14
           RETURN {fv0, sq1, sq7}`,
      collectionNames: [],
    },
  };
  return {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////
    setUp: function () {},

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////
    tearDown: function () {},

    testSpecificQueries: function () {
      for (const [key, value] of Object.entries(specificQueries)) {
        ct.testQuery(value, {});
      }
    },

    testSomeSubqueryChaos: function () {
      for (var i = 0; i < numberOfQueriesGenerated; i++) {
        ct.testQueryWithSeed({
          numberSubqueries: randomDepth(),
          seed: Math.trunc(Math.random() * 10000),
          showReproduce: true,
          throwOnMismatch: true,
        });
      }
    },

    testSomeSubqueryModificationChaos: function () {
      for (var i = 0; i < numberOfQueriesGenerated; i++) {
        ct.testModifyingQueryWithSeed({
          numberSubqueries: randomModificationDepth(),
          seed: Math.trunc(Math.random() * 10000),
          showReproduce: true,
          throwOnMismatch: true,
        });
      }
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlSubqueryChaos);

return jsunity.done();
