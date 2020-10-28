/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure scenarios
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

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
var internal = require("internal");

const {ERROR_QUERY_COLLECTION_LOCK_FAILED} = internal.errors;


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
      AQL_EXECUTE(query);
      fail();
    } catch (err) {
      assertEqual(error.code, err.errorNum, query);
    }
  };
        
  return {
    setUpAll: function () {
      internal.debugClearFailAt();
      db._drop(cn);
      db._create(cn, { numberOfShards: 3 });
      db._drop(en);
      db._createEdgeCollection(en);
    },

    setUp: function () {
      internal.debugClearFailAt();
    },

    tearDownAll: function () {
      internal.debugClearFailAt();
      db._drop(cn);
      db._drop(en);
    },

    tearDown: function () {
      internal.debugClearFailAt();
    },

    testQueryRegistryInsert1: function () {
      internal.debugSetFailAt("QueryRegistryInsertException1");
      // we need a diamond-query to trigger an insertion into the q-registry
      assertFailingQuery(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          cn +
          " FILTER doc1._key != doc2._key RETURN 1",
        internal.errors.ERROR_CLUSTER_AQL_COMMUNICATION
      );
    },

    testQueryRegistryInsert2: function () {
      internal.debugSetFailAt("QueryRegistryInsertException2");
      // we need a diamond-query to trigger an insertion into the q-registry
      assertFailingQuery(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          cn +
          " FILTER doc1._key != doc2._key RETURN 1",
        internal.errors.ERROR_CLUSTER_AQL_COMMUNICATION
      );
    },

    testShutdownSync: function () {
      internal.debugSetFailAt("ExecutionEngine::shutdownSync");

      let res = AQL_EXECUTE("FOR doc IN " + cn + " RETURN doc").json;
      // no real test expectations here, just that the query works and doesn't fail on shutdown
      assertEqual(0, res.length);
    },

    testShutdownSyncDiamond: function () {
      internal.debugSetFailAt("ExecutionEngine::shutdownSync");

      let res = AQL_EXECUTE(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          en +
          " FILTER doc1._key == doc2._key RETURN doc1"
      ).json;
      // no real test expectations here, just that the query works and doesn't fail on shutdown
      assertEqual(0, res.length);
    },

    testShutdownSyncFailInGetSome: function () {
      internal.debugSetFailAt("ExecutionEngine::shutdownSync");
      internal.debugSetFailAt("RestAqlHandler::getSome");

      assertFailingQuery("FOR doc IN " + cn + " RETURN doc");
    },

    testShutdownSyncDiamondFailInGetSome: function () {
      internal.debugSetFailAt("ExecutionEngine::shutdownSync");
      internal.debugSetFailAt("RestAqlHandler::getSome");

      assertFailingQuery(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          en +
          " FILTER doc1._key == doc2._key RETURN doc1"
      );
    },

    testNoShardsReturned: function () {
      internal.debugSetFailAt("ClusterInfo::failedToGetShardList");
      assertFailingQuery(
        "FOR doc1 IN " +
          cn +
          " FOR doc2 IN " +
          en +
          " FILTER doc1._key == doc2._key RETURN doc1",
        ERROR_QUERY_COLLECTION_LOCK_FAILED
      );
    },
  };
}
 
if (internal.debugCanUseFailAt()) {
  jsunity.run(ahuacatlFailureSuite);
}

return jsunity.done();
