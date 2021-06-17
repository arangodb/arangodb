/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual,
   assertNotEqual, arango, print */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Manuel Pöter
// //////////////////////////////////////////////////////////////////////////////
'use strict';
const _ = require('lodash');
const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const request = require("@arangodb/request");
const db = arangodb.db;
const {
  debugSetFailAt,
  debugClearFailAt,
  deriveTestSuite,
  getEndpointById,
  getServersByType,
  runParallelArangoshTests
} = require('@arangodb/test-helper');

const getDBServers = () => {
  return getServersByType('dbserver');
};

const fetchRevisionTree = (serverUrl, shardId) => {
  let result = request({ method: "POST", url: serverUrl + "/_api/replication/batch", body: {ttl : 3600}, json: true });
  assertEqual(200, result.statusCode);
  const batch = JSON.parse(result.body);
  if (!batch.hasOwnProperty("id")) {
    throw "Could not create batch!";
  }
  
  result = request({ method: "GET",
    url: serverUrl + `/_api/replication/revisions/tree?collection=${encodeURIComponent(shardId)}&verification=true&batchId=${batch.id}`});
  assertEqual(200, result.statusCode);
  request({ method: "DELETE", url: serverUrl + `/_api/replication/batch${batch.id}`});
  return JSON.parse(result.body);
};

const waitForShardsInSync = (cn) => {
  let start = internal.time();
  while (true) {
    if (internal.time() - start > 300) {
      assertTrue(false, "Shards were not getting in sync in time, giving up!");
    }
    let shardDistribution = arango.GET("/_admin/cluster/shardDistribution");
    assertFalse(shardDistribution.error);
    assertEqual(200, shardDistribution.code);
    let collInfo = shardDistribution.results[cn];
    let shards = Object.keys(collInfo.Plan);
    let insync = 0;
    for (let s of shards) {
      if (collInfo.Plan[s].followers.length === collInfo.Current[s].followers.length) {
        ++insync;
      }
    }
    if (insync === shards.length) {
      return;
    }
    console.warn("insync=", insync, ", collInfo=", collInfo, internal.time() - start);
    internal.wait(1);
  }
};

const checkCollectionConsistency = (cn) => {
  const c = db._collection(cn);
  const servers = getDBServers();
  const shardInfo = c.shards(true);
  
  let failed = false;
  const getServerUrl = (serverId) => servers.filter((server) => server.id === serverId)[0].url;
  let tries = 0;
  do {
    failed = false;
    Object.entries(shardInfo).forEach(
      ([shard, [leader, follower]]) => {
        const leaderTree = fetchRevisionTree(getServerUrl(leader), shard);
        leaderTree.computed.nodes = "<reduced>";
        leaderTree.stored.nodes = "<reduced>";
        if (!leaderTree.equal) {
          console.error(`Leader has inconsistent tree for shard ${shard}:`, leaderTree);
          failed = true;
        };
        
        const followerTree = fetchRevisionTree(getServerUrl(follower), shard);
        followerTree.computed.nodes = "<reduced>";
        followerTree.stored.nodes = "<reduced>";
        if (!followerTree.equal) {
          console.error(`Follower has inconsistent tree for shard ${shard}:`, followerTree);
          failed = true;
        };

        if (!_.isEqual(leaderTree, followerTree)) {
          console.error(`Leader and follower have different trees for shard ${shard}`);
          console.error("Leader: ", leaderTree);
          console.error("Follower: ", followerTree);
          failed = true;
        }
      });
    if (failed) {
      if (++tries >= 3) {
        assertFalse(failed, `Cluster still not in sync - giving up!`);
      }
      console.warn(`Found some inconsistencies! Giving cluster some more time to get in sync before checking again... try=${tries}`);
      internal.sleep(10);
    }
  } while(failed);
};

const clearAllFailurePoints = () => {
  for (const server of getDBServers()) {
    debugClearFailAt(getEndpointById(server.id));
  }
};

function BaseChaosSuite(testOpts) {
  // generate a random collection name
  const cn = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));

  return {
    setUp: function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 4, replicationFactor: 2 });
      
      if (testOpts.withFailurePoints) {
        const servers = getDBServers();
        assertTrue(servers.length > 0);
        for (const server of servers) {
          debugSetFailAt(getEndpointById(server.id), "replicateOperations_randomize_timeout");
          debugSetFailAt(getEndpointById(server.id), "delayed_synchronous_replication_request_processing");
        }
      }
    },

    tearDown: function () {
      clearAllFailurePoints();
      db._drop(cn);
    },
    
    testWorkInParallel: function () {
      let code = (testOpts) => {
        const key = () => "testmann" + Math.floor(Math.random() * 100000000);
        const docs = () => {
          let result = [];
          const r = Math.floor(Math.random() * 2000) + 1;
          for (let i = 0; i < r; ++i) {
            result.push({ _key: key() });
          }
          return result;
        };

        let c = db._collection(testOpts.collection);
        const opts = () => {
          let result = {};
          if (testOpts.withIntermediateCommits) {
            const r = Math.random();
            if (r <= 0.5) {
              result.intermediateCommitCount = 7 + Math.floor(r * 10);
            }
          }
          
          if (testOpts.withVaryingOverwriteMode) {
            let r = Math.random();
            if (r >= 0.75) {
              result.overwriteMode = "replace";
            } else if (r >= 0.5) {
              result.overwriteMode = "update";
            } else if (r >= 0.25) {
              result.overwriteMode = "ignore";
            } 
          }
        };
        
        let query = db._query;
        let trx = null;
        let ctx = db;
        if (testOpts.withStreamingTransactions && Math.random() < 0.5) {
          trx = db._createTransaction({ collections: { write: [c.name()] } });
          ctx = trx;
          c = trx.collection(testOpts.collection);
          query = trx.query;
        }

        try {
          const d = Math.random();
          if (d >= 0.98 && testOpts.withTruncate) {
            c.truncate();
          } else if (d >= 0.9) {
            query.call(ctx, "FOR doc IN @docs INSERT doc INTO " + c.name(), {docs: docs()}, opts());
          } else if (d >= 0.8) {
            query.call(ctx, "FOR doc IN " + c.name() + " LIMIT 138 REMOVE doc IN " + c.name(), {}, opts());
          } else if (d >= 0.7) {
            query.call(ctx, "FOR doc IN " + c.name() + " LIMIT 1338 REPLACE doc WITH { pfihg: 434, fjgjg: 555 } IN " + c.name(), {}, opts());
          } else if (d >= 0.25) {
            c.insert(docs(), opts());
          } else {
            c.remove(docs());
          }
        } catch (err) {}
        
        if (trx) {
          if (Math.random() < 0.2) {
            trx.abort();
          } else {
            trx.commit();
          }
        }
      };

      testOpts.collection = cn;
      code = `(${code.toString()})(${JSON.stringify(testOpts)});`;
      
      let tests = [];
      for (let i = 0; i < 3; ++i) {
        tests.push(["p" + i, code]);
      }

      // run the suite for 5 minutes
      runParallelArangoshTests(tests, 5 * 60, cn);

      print("Finished load test - waiting for cluster to get in sync before checking consistency.");
      clearAllFailurePoints();
      waitForShardsInSync(cn);
      checkCollectionConsistency(cn);
    }
  };
}

function BuildChaosSuite(opts, suffix) {
  let suite = {};
  deriveTestSuite(BaseChaosSuite(opts), suite, suffix);
  return suite;
}

const params = ["IntermediateCommits", "FailurePoints", "Truncate", "VaryingOverwriteMode", "StreamingTransactions"];

for (let i = 0; i < (1 << params.length); ++i) {
  let paramValues = [];
  for (let j = params.length - 1; j >= 0; --j) {
    paramValues.push(Boolean(i & (1 << j)));
  }
  
  let suffix = "";
  let opts = {};
  for (let j = 0; j < params.length; ++j) {
    suffix += paramValues[j] ? "_with_" : "_no_";
    suffix += params[j];
    opts["with" + params[j]] = paramValues[j];
  }  
  let func = function() { return BuildChaosSuite(opts, suffix); };
  Object.defineProperty(func, 'name', {value: "ChaosSuite" + suffix, writable: false});

  jsunity.run(func);
}

return jsunity.done();
