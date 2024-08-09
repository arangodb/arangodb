/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual, assertTrue */

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

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
var internal = require("internal");
let IM = global.instanceManager;

const {ERROR_QUERY_COLLECTION_LOCK_FAILED, ERROR_CLUSTER_TIMEOUT} = internal.errors;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlFailureSuite () {
  'use strict';
  const cn = "UnitTestsAhuacatlFailures";
  const en = "UnitTestsAhuacatlEdgeFailures";
  
  let  assertFailingQuery = function (query, error) {
    if (error === undefined) {
      error = internal.errors.ERROR_DEBUG;
    }
    try {
      db._query(query);
      fail();
    } catch (err) {
      assertEqual(error.code, err.errorNum, query);
    }
  };
        
  return {
    setUpAll: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      db._create(cn, { numberOfShards: 3 });
      db._drop(en);
      db._createEdgeCollection(en);
    },

    setUp: function () {
      IM.debugClearFailAt();
    },

    tearDownAll: function () {
      IM.debugClearFailAt();
      db._drop(cn);
      db._drop(en);
    },

    tearDown: function () {
      IM.debugClearFailAt();
    },

    testQueryRegistryInsert1: function () {
      IM.debugSetFailAt("QueryRegistryInsertException1");
      // we need a diamond-query to trigger an insertion into the q-registry
      assertFailingQuery(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          cn +
          " FILTER doc1._key != doc2._key RETURN 1",
        internal.errors.ERROR_DEBUG
      );
    },

    testQueryRegistryInsert2: function () {
      IM.debugSetFailAt("QueryRegistryInsertException2");
      // we need a diamond-query to trigger an insertion into the q-registry
      assertFailingQuery(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          cn +
          " FILTER doc1._key != doc2._key RETURN 1",
        internal.errors.ERROR_DEBUG
      );
    },

    testShutdownSync: function () {
      IM.debugSetFailAt("ExecutionEngine::shutdownSync");

      let res = db._query("FOR doc IN " + cn + " RETURN doc").toArray();
      // no real test expectations here, just that the query works and doesn't fail on shutdown
      assertEqual(0, res.length);
    },

    testShutdownSyncDiamond: function () {
      IM.debugSetFailAt("ExecutionEngine::shutdownSync");

      let res = db._query(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          en +
          " FILTER doc1._key == doc2._key RETURN doc1"
      ).toArray();
      // no real test expectations here, just that the query works and doesn't fail on shutdown
      assertEqual(0, res.length);
    },

    testShutdownSyncFailInGetSome: function () {
      IM.debugSetFailAt("ExecutionEngine::shutdownSync");
      IM.debugSetFailAt("RestAqlHandler::getSome");

      assertFailingQuery("FOR doc IN " + cn + " RETURN doc");
    },

    testShutdownSyncDiamondFailInGetSome: function () {
      IM.debugSetFailAt("ExecutionEngine::shutdownSync");
      IM.debugSetFailAt("RestAqlHandler::getSome");

      assertFailingQuery(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          en +
          " FILTER doc1._key == doc2._key RETURN doc1"
      );
    },

    testNoShardsReturned: function () {
      IM.debugSetFailAt("ClusterInfo::failedToGetShardList");
      assertFailingQuery(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          en +
          " FILTER doc1._key == doc2._key RETURN doc1",
        ERROR_QUERY_COLLECTION_LOCK_FAILED
      );
    }
  };
}

function ahuacatlFailureOneShardSuite () {
  'use strict';
  const cn = "UnitTestsAhuacatlFailures";
  const dbn = "UnitTestsFailuresDB";

  let  assertFailingQuery = function (query, error) {
    if (error === undefined) {
      error = internal.errors.ERROR_DEBUG;
    }
    try {
      db._query(query);
      fail();
    } catch (err) {
      assertEqual(error.code, err.errorNum, query);
    }
  };

  return {
    setUpAll: function () {
      // Note: We use a OneShardDatabase here to force the SLEEP onto the DBServer
      // This is only convenience for the test but not a requirement
      IM.debugClearFailAt();
      db._createDatabase(dbn, { sharding: "single" });
      db._useDatabase(dbn);
      const c = db._create(cn);
      const docs = [];
      for (let i = 0; i <  100; ++i) {
        docs.push({});
      }
      c.save(docs);
    },

    setUp: function () {
      IM.debugClearFailAt();
    },

    tearDownAll: function () {
      IM.debugClearFailAt();
      db._useDatabase("_system");
      db._dropDatabase(dbn);
    },

    tearDown: function () {
      IM.debugClearFailAt();
    },

    testAbortQuerySnippetsOnTimeout: function () {
      IM.debugSetFailAt("RemoteExecutor::impatienceTimeout");
      // This query sleeps 100 * 10 seconds. So it should give us a lot of time
      // to abort it early and still assert we do not overshoot on time.

      const time = require("internal").time;
      const startTime = time();
      assertFailingQuery(`
        FOR x IN ${cn}
          LET a = SLEEP(10)
          RETURN {x, a}
      `,
        ERROR_CLUSTER_TIMEOUT
      );
      const endTime = time();
      // Unfortunately we have to do this time-based assertion.
      // From the outside we cannot distinguish a timeout that happened in AQL to the one
      // that has happened in the "abort" roundtrip.
      // So we make the AQL timeout smallish, and check if we return with the timeout at a timepoint before
      // the "abort" one could have kicked in.
      assertTrue(endTime - startTime < 12.0, `We have 2sec request timeout, and 10sec req timeout on finish, we need to return earlier`);
    }
  };
}
 
if (IM.debugCanUseFailAt()) {
  jsunity.run(ahuacatlFailureSuite);
  jsunity.run(ahuacatlFailureOneShardSuite);
}

return jsunity.done();
