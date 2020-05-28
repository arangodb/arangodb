/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertMatch, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for client/server side transaction invocation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB Inc, Cologne, Germany
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
/// @author Jan Christoph Uhde
/// @author Copyright 2019, ArangodDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////


function CommonStatisticsSuite() {
  'use strict';
  let c;
  return {
    setUp: function () {
      c = db._create("shellCommonStatTestCollection");
    },
    tearDown: function () {
      db._drop("shellCommonStatTestCollection");
    },

    testServerStatsStructure: function () {
      let stats = internal.serverStatistics();
      assertTrue(Number.isInteger(stats.transactions.started));
      assertTrue(Number.isInteger(stats.transactions.committed));
      assertTrue(Number.isInteger(stats.transactions.aborted));
      assertTrue(Number.isInteger(stats.transactions.intermediateCommits));
    },

    testServerStatsCommit: function () {
      let stats1 = internal.serverStatistics();
      c.insert({ "gondel" : "ulf" });
      let stats2 = internal.serverStatistics();

      assertTrue(stats1.transactions.started < stats2.transactions.started
                , "1 started: " + stats1.transactions.started 
                + " -- 2 started: " + stats2.transactions.started);
      assertTrue(stats1.transactions.committed < stats2.transactions.committed);
    },

    testServerStatsAbort: function () {
      let stats1 = internal.serverStatistics();
      let stats2;
      try {
        db._executeTransaction({
          collections: {},
          action: function () {
            var err = new Error('abort on purpose');
            throw err;
          }
        });
        fail();
      } catch (err) {
        stats2 = internal.serverStatistics();
        assertMatch(/abort on purpose/, err.errorMessage);
      }

      assertTrue(stats1.transactions.started < stats2.transactions.started);
      assertTrue(stats1.transactions.aborted < stats2.transactions.aborted);
    },

    testIntermediateCommitsCommit: function () {
      let stats1 = internal.serverStatistics();
      db._query(`FOR i IN 1..3 INSERT { "ulf" : i } IN ${c.name()}`, {}, { "intermediateCommitCount" : 2});
      let stats2 = internal.serverStatistics();

      if (!internal.isCluster()) {
        assertTrue(stats1.transactions.intermediateCommits < stats2.transactions.intermediateCommits);
      } else {
        assertEqual(stats1.transactions.intermediateCommits, 0);
      }
      assertTrue(stats1.transactions.committed < stats2.transactions.committed);
    },

    testIntermediateCommitsAbort: function () {
      let stats1 = internal.serverStatistics();
      let stats2;
      try {
        let rv = db._query(`FOR i IN 1..3
                            FILTER ASSERT(i == 0, "abort on purpose")
                            INSERT { "ulf" : i } IN ${c.name()}
                           `, {}, { "intermediateCommitCount" : 2});
        fail();
      } catch (err) {
        stats2 = internal.serverStatistics();
        assertMatch(/abort on purpose/, err.errorMessage);
      }

      if (!internal.isCluster()) {
        assertTrue(stats1.transactions.intermediateCommits <= stats2.transactions.intermediateCommits);
      } else {
        assertEqual(stats1.transactions.intermediateCommits, 0);
      }
      assertTrue(stats1.transactions.aborted < stats2.transactions.aborted);
    },
  }; //return
} // CommonStatisticsSuite end


jsunity.run(CommonStatisticsSuite);

return jsunity.done();


