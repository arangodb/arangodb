/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual,
   assertNotEqual, arango, print */
   
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

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
  runParallelArangoshTests,
  waitForShardsInSync
} = require('@arangodb/test-helper');

const getCoordinators = () => {
  return getServersByType('coordinator');
};
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
        // We remove the computed and stored nodes since we may want to print the trees, but we
        // don't want to print the 262k buckets! Equality of the trees is checked using the single
        // combined hash and document count.
        leaderTree.computed.nodes = "<reduced>";
        leaderTree.stored.nodes = "<reduced>";
        if (!leaderTree.equal) {
          console.error(`Leader has inconsistent tree for shard ${shard}:`, leaderTree);
          failed = true;
        }
        
        const followerTree = fetchRevisionTree(getServerUrl(follower), shard);
        followerTree.computed.nodes = "<reduced>";
        followerTree.stored.nodes = "<reduced>";
        if (!followerTree.equal) {
          console.error(`Follower has inconsistent tree for shard ${shard}:`, followerTree);
          failed = true;
        }

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
  for (const server of getCoordinators()) {
    debugClearFailAt(getEndpointById(server.id));
  }
};

function BaseChaosSuite(testOpts) {
  // generate a random collection name
  const cn = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));
  const coordination_cn = cn + "_coord";

  return {
    setUp: function () {
      db._drop(cn);
      db._drop(coordination_cn);
      let rf = Math.max(2, getDBServers().length);
      db._create(cn, { numberOfShards: rf * 2, replicationFactor: rf });
      db._create(coordination_cn);
      
      if (testOpts.withFailurePoints) {
        let servers = getDBServers();
        assertTrue(servers.length > 0);
        for (const server of servers) {
          debugSetFailAt(getEndpointById(server.id), "replicateOperations_randomize_timeout");
          debugSetFailAt(getEndpointById(server.id), "delayed_synchronous_replication_request_processing");
          debugSetFailAt(getEndpointById(server.id), "Query::setupTimeout");
          debugSetFailAt(getEndpointById(server.id), "Query::setupLockTimeout");
          debugSetFailAt(getEndpointById(server.id), "Query::setupTimeoutFailSequence");
          debugSetFailAt(getEndpointById(server.id), "Query::setupTimeoutFailSequenceRandom");
          debugSetFailAt(getEndpointById(server.id), "RocksDBCollection::insertFail1");
          debugSetFailAt(getEndpointById(server.id), "RocksDBCollection::insertFail2");
          debugSetFailAt(getEndpointById(server.id), "RocksDBCollection::modifyFail1");
          debugSetFailAt(getEndpointById(server.id), "RocksDBCollection::modifyFail2");
          debugSetFailAt(getEndpointById(server.id), "RocksDBCollection::modifyFail3");
          debugSetFailAt(getEndpointById(server.id), "RocksDBCollection::removeFail1");
          debugSetFailAt(getEndpointById(server.id), "RocksDBCollection::removeFail2");
        }
        servers = getCoordinators();
        assertTrue(servers.length > 0);
        for (const server of servers) {
          debugSetFailAt(getEndpointById(server.id), "Query::setupTimeout");
          debugSetFailAt(getEndpointById(server.id), "Query::setupTimeoutFailSequence");
        }
      }
      if (testOpts.withDelays) {
        let servers = getDBServers();
        assertTrue(servers.length > 0);
        for (const server of servers) {
          debugSetFailAt(getEndpointById(server.id), "TransactionChaos::blockerOnSync");
          debugSetFailAt(getEndpointById(server.id), "TransactionChaos::randomSleep");
          debugSetFailAt(getEndpointById(server.id), "TransactionChaos::randomSync");
          debugSetFailAt(getEndpointById(server.id), "RocksDBMetaCollection::forceSerialization");
        }
      }
    },

    tearDown: function () {
      clearAllFailurePoints();
      db._drop(cn);
      
      const shells = db[coordination_cn].all();
      if (shells.length > 0) {
        print("Found remaining docs in coordination collection:");
        print(shells);
      }
      db._drop(coordination_cn);
    },
    
    testWorkInParallel: function () {
      let code = (testOpts) => {
        // The idea here is to use the birthday paradox and have a certain amount of collisions.
        // The babies API is supposed to go through and report individual collisions. Same with
        // removes,so we intentionally try to remove lots of documents which are not actually there.
        const key = () => "testmann" + Math.floor(Math.random() * 100000000);
        const docs = () => {
          let result = [];
          const max = 2000;
          let r = Math.floor(Math.random() * max) + 1;
          if (r > (max * 0.8)) {
            // we want ~20% of all requests to be single document operations
            r = 1;
          }
          for (let i = 0; i < r; ++i) {
            result.push({ _key: key() });
          }
          return result;
        };

        let c = db._collection(testOpts.collection);
        const opts = () => {
          let result = {};
          if (testOpts.withIntermediateCommits) {
            if (Math.random() <= 0.5) {
              result.intermediateCommitCount = 7 + Math.floor(Math.random() * 10);
            }
          }
          
          if (testOpts.withVaryingOverwriteMode) {
            const r = Math.random();
            if (r >= 0.75) {
              result.overwriteMode = "replace";
            } else if (r >= 0.5) {
              result.overwriteMode = "update";
            } else if (r >= 0.25) {
              result.overwriteMode = "ignore";
            } 
          }
          return result;
        };
        
        let query = (...args) => db._query(...args);
        let trx = null;
        if (testOpts.withStreamingTransactions && Math.random() < 0.5) {
          trx = db._createTransaction({ collections: { write: [c.name()] } });
          c = trx.collection(testOpts.collection);
          query = (...args) => trx.query(...args);
        }

        const logAllOps = false; // can be set to true for debugging purposes
        const log = (msg) => {
          if (logAllOps) {
            console.info(msg);
          }
        };
        const ops = trx === null ? 1 : Math.floor(Math.random() * 5) + 1;
        for (let op = 0; op < ops; ++op) {
          try {
            const d = Math.random();
            if (d >= 0.98 && testOpts.withTruncate) {
              log("RUNNING TRUNCATE");
              c.truncate();
            } else if (d >= 0.9) {
              let o = opts();
              let d = docs();
              log(`RUNNING AQL INSERT WITH ${d.length} DOCS. OPTIONS: ${JSON.stringify(o)}`);
              query("FOR doc IN @docs INSERT doc INTO " + c.name(), {docs: d}, o);
            } else if (d >= 0.8) {
              let o = opts();
              const limit = Math.floor(Math.random() * 200);
              log(`RUNNING AQL REMOVE WITH LIMIT=${limit}. OPTIONS: ${JSON.stringify(o)}`);
              query("FOR doc IN " + c.name() + " LIMIT @limit REMOVE doc IN " + c.name(), {limit}, o);
            } else if (d >= 0.7) {
              let o = opts();
              const limit = Math.floor(Math.random() * 2000);
              log(`RUNNING AQL REPLACE WITH LIMIT=${limit}. OPTIONS: ${JSON.stringify(o)}`);
              query("FOR doc IN " + c.name() + " LIMIT @limit REPLACE doc WITH { pfihg: 434, fjgjg: 555 } IN " + c.name(), {limit}, o);
            } else if (d >= 0.68) {
              const limit = Math.floor(Math.random() * 10) + 1;
              log(`RUNNING DOCUMENT SINGLE LOOKUP QUERY WITH LIMIT=${limit}`);
              query("FOR doc IN " + c.name() + " LIMIT @limit RETURN DOCUMENT(doc._id)", {limit});
            } else if (d >= 0.66) {
              const limit = Math.floor(Math.random() * 10) + 1;
              log(`RUNNING DOCUMENT ARRAY LOOKUP QUERY WITH LIMIT=${limit}`);
              query("LET keys = (FOR doc IN " + c.name() + " LIMIT @limit RETURN doc._id) RETURN DOCUMENT(keys)", {limit});
            } else if (d >= 0.65) {
              let o = opts();
              const limit = Math.floor(Math.random() * 10) + 1;
              log(`RUNNING DOCUMENT LOOKUP AND WRITE QUERY WITH LIMIT=${limit}. OPTIONS: ${JSON.stringify(0)}`);
              query("FOR doc IN " + c.name() + " LIMIT @limit LET d = DOCUMENT(doc._id) INSERT UNSET(doc, '_key') INTO " + c.name(), {limit}, o);
            } else if (d >= 0.25) {
              let o = opts();
              let d = docs();
              log(`RUNNING INSERT WITH ${d.length} DOCS. OPTIONS: ${JSON.stringify(o)}`);
              d = d.length === 1 ? d[0] : d;
              c.insert(d, o);
            } else {
              let d = docs();
              log(`RUNNING REMOVE WITH ${d.length} DOCS`);
              d = d.length === 1 ? d[0] : d;
              c.remove(d);
            }
          } catch (err) {}
        }
        
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

      // run the suite for 3 minutes
      runParallelArangoshTests(tests, 3 * 60, coordination_cn);

      print("Finished load test - waiting for cluster to get in sync before checking consistency.");
      clearAllFailurePoints();
      waitForShardsInSync(cn);
      checkCollectionConsistency(cn);
    }
  };
}

const params = ["IntermediateCommits", "FailurePoints", "Delays", "StreamingTransactions"];

const fixedParams = ["Truncate", "VaryingOverwriteMode"]; // these parameters are always enabled

const makeConfig = (paramValues) => {
  let suffix = "";
  let opts = {};
  for (let j = 0; j < params.length; ++j) {
    suffix += paramValues[j] ? "_with_" : "_no_";
    suffix += params[j];
    opts["with" + params[j]] = paramValues[j];
  }
  for (const p of fixedParams) {
    suffix += "_with_" + p;
    opts["with" + p] = true;
  }
  return { suffix: suffix, options: opts };
};

const run = () => {
  if (!global.currentTestConfig) {
    throw "Chaos test requires global currentTestConfig to be defined!";
  }
  const { options, suffix } = global.currentTestConfig;
  print("Running chaos test with config ", options);

  let func = function() {
    let suite = {};
    deriveTestSuite(BaseChaosSuite(options), suite, suffix);
    return suite;
  };
  // define the function name as it shows up as suiteName
  Object.defineProperty(func, 'name', {value: "ChaosSuite" + suffix, writable: false});

  jsunity.run(func);

  return jsunity.done();
};

module.exports.parameters = params;
module.exports.makeConfig = makeConfig;
module.exports.run = run;
