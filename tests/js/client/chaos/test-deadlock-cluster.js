/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, print, assertEqual, assertFalse, assertNotEqual */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const _ = require('lodash');
const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const request = require("@arangodb/request");
const db = arangodb.db;
const { getMetric, getDBServers, runParallelArangoshTests, waitForShardsInSync, versionHas } = require('@arangodb/test-helper');

const fetchRevisionTree = (serverUrl, shardId) => {
  let result = request({ method: "POST", url: serverUrl + "/_api/replication/batch", body: {ttl : 3600}, json: true });
  assertEqual(200, result.statusCode);
  const batch = JSON.parse(result.body);
  if (!batch.hasOwnProperty("id")) {
    throw "Could not create batch!";
  }
  
  result = request({ method: "GET",
    url: serverUrl + `/_api/replication/revisions/tree?collection=${encodeURIComponent(shardId)}&verification=true&batchId=${batch.id}`});
  assertEqual(200, result.statusCode, result);
  request({ method: "DELETE", url: serverUrl + `/_api/replication/batch/${batch.id}`});
  return JSON.parse(result.body);
};

const compareTree = function (left, right) {
  const attributes = ["version", "maxDepth", "count", "hash", "initialRangeMin"];
  return _.every(attributes, (attr) => left[attr] === right[attr]);
};

const checkCollectionConsistency = (cn) => {
  const c = db._collection(cn);
  const servers = getDBServers();
  const shardInfo = c.shards(true);
  
  let failed;
  let message;
  const getServerUrl = (serverId) => servers.filter((server) => server.id === serverId)[0].url;
  let tries = 0;
  do {
    failed = false;
    message = "";
    Object.entries(shardInfo).forEach(
      ([shard, [leader, follower]]) => {
        const leaderTree = fetchRevisionTree(getServerUrl(leader), shard);
        // We remove the computed and stored nodes since we may want to print the trees, but we
        // don't want to print the 262k buckets! Equality of the trees is checked using the single
        // combined hash and document count.
        leaderTree.computed.nodes = "<reduced>";
        leaderTree.stored.nodes = "<reduced>";
        if (!leaderTree.equal) {
          message = `Leader has inconsistent tree for shard ${shard}: ${JSON.stringify(leaderTree)}`;
          failed = true;
        }

        // note: with rf > 1, follower is always present. the code is generalized however so
        // that the tests can easily be run with rf = 1 for debugging purposes.
        if (follower) {
          const followerTree = fetchRevisionTree(getServerUrl(follower), shard);
          followerTree.computed.nodes = "<reduced>";
          followerTree.stored.nodes = "<reduced>";
          if (!followerTree.equal) {
            message = `Follower has inconsistent tree for shard ${shard}: ${JSON.stringify(followerTree)}`;
            failed = true;
          }

          if (!compareTree(leaderTree.computed, followerTree.computed)) {
            message = `Leader and follower have different trees for shard ${shard}. Leader: ${JSON.stringify(leaderTree)}, Follower: ${JSON.stringify(followerTree)}`;
            failed = true;
          }
        }

        if (failed) {
          assertNotEqual("", message);
          console.error(message);
        }
      });
    if (failed) {
      if (++tries >= 6) {
        assertFalse(failed, `Cluster still not in sync - giving up! ${message}`);
      }
      console.warn(`Found some inconsistencies! Giving cluster some more time to get in sync before checking again... try=${tries}`);
      internal.sleep(10);
    }
  } while (failed);
};

function DeadlockSuite() { 
  // generate a random collection name
  const cn = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32)).substring(0, 8);
  const coordination_cn = cn + "_coord";

  const numCollections = 3;

  return {
    setUp: function () {
      db._drop(coordination_cn);
      let rf = Math.max(2, getDBServers().length);
      for (let i = 0; i < numCollections; ++i) {
        db._create(cn + i, {numberOfShards: rf * 2, replicationFactor: rf});
      }
      db._create(coordination_cn);
    },

    tearDown: function () {
      for (let i = 0; i < numCollections; ++i) {
        db._drop(cn + i);
      }
      db._drop(coordination_cn);
    },
    
    testWorkInParallel: function () {
      let code = (testOpts) => {
        const pid = require("internal").getPid();
        
        const generateAttributes = (n) => {
          let attrs = "";
          // start letter
          let s = 65 + Math.floor(Math.random() * 20);
          for (let i = 0; i < n; ++i) {
            if (attrs !== "") {
              attrs += ", ";
            }
            attrs += String.fromCharCode(s + i) + ": ";
            if (Math.random() > 0.66) {
              attrs += Math.random().toFixed(8);
            } else if (Math.random() >= 0.33) {
              attrs += '"' + require('internal').genRandomAlphaNumbers(Math.floor(Math.random() * 100) + 1) + '"';
            } else {
              attrs += "null";
            }
          }
          // returns "a: null, b: 24.534, c: "abhtr"
          return attrs;
        };
        // The idea here is to use the birthday paradox and have a certain amount of collisions.
        // The babies API is supposed to go through and report individual collisions. Same with
        // removes,so we intentionally try to remove lots of documents which are not actually there.
        const key = () => "testmann" + Math.floor(Math.random() * 100000000);
        const docs = () => {
          let result = [];
          const max = 4000;
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

        let collectionName = testOpts.collection + Math.floor(Math.random() * testOpts.numCollections);
        let c = db._collection(collectionName);
        const opts = (keepNull = false) => {
          let result = {};
          const r = Math.random();
          if (r >= 0.75) {
            result.overwriteMode = "replace";
          } else if (r >= 0.5) {
            result.overwriteMode = "update";
          } else if (r >= 0.25) {
            result.overwriteMode = "ignore";
          } 

          if (keepNull && Math.random() >= 0.5) {
            result.keepNull = true;
          }
          return result;
        };
        const queryOptions = (fullCount = false) => {
          let result = {};
          if (fullCount && Math.random() >= 0.5) {
            result.fullCount = true;
          }
          if (Math.random() >= 0.5) {
            result.maxRuntime = Math.random() * 10;
          }
          return result;
        };
        
        let query = (...args) => db._query(...args);
        let trx = null;
        
        const logAllOps = false; // can be set to true for debugging purposes
        const log = (msg) => {
          if (logAllOps) {
            if (trx) {
              console.info(`${pid}: trx ${trx.id()}, cn ${collectionName}: ${msg}`);
            } else {
              console.info(`${pid}: cn ${collectionName}: ${msg}`);
            }
          }
        };

        if (Math.random() < 0.5) {
          let mode = "write";
          if (Math.random() >= 0.75) {
            mode = "exclusive";
          }
          trx = db._createTransaction({ collections: { [mode]: [c.name()] } });
          log(`CREATED TRX`);
          c = trx.collection(c.name());
          query = (...args) => trx.query(...args);
        }

        const ops = trx === null ? 1 : Math.floor(Math.random() * 10) + 1;
        for (let op = 0; op < ops; ++op) {
          try {
            const d = Math.random();
            if (d >= 0.99) {
              log("RUNNING TRUNCATE");
              c.truncate();
            } else if (d >= 0.9) {
              let d = docs();
              let o = opts();
              let qo = queryOptions();
              log(`RUNNING AQL INSERT WITH ${d.length} DOCS. OPTIONS: ${JSON.stringify(o)}, QUERY OPTIONS: ${JSON.stringify(qo)}`);
              query(`FOR doc IN @docs INSERT doc INTO ${c.name()} OPTIONS ${JSON.stringify(o)}`, {docs: d}, qo);
            } else if (d >= 0.8) {
              const limit = Math.floor(Math.random() * 200);
              let o = opts();
              let qo = queryOptions();
              log(`RUNNING AQL REMOVE WITH LIMIT=${limit}. OPTIONS: ${JSON.stringify(o)}, QUERY OPTIONS: ${JSON.stringify(qo)}`);
              query(`FOR doc IN ${c.name()} LIMIT @limit REMOVE doc IN ${c.name()}`, {limit}, qo);
            } else if (d >= 0.75) {
              const limit = Math.floor(Math.random() * 2000);
              let o = opts();
              let qo = queryOptions();
              log(`RUNNING AQL REPLACE WITH LIMIT=${limit}. OPTIONS: ${JSON.stringify(o)}, QUERY OPTIONS: ${JSON.stringify(qo)}`);
              // generate random attribute values for random attributes
              query(`FOR doc IN ${c.name()} LIMIT @limit REPLACE doc WITH { pfihg: 434, fjgjg: RAND(), ${generateAttributes(3)} } IN ${c.name()}`, {limit}, qo);
            } else if (d >= 0.70) {
              const limit = Math.floor(Math.random() * 2000);
              let o = opts(/*keepNull*/ true);
              let qo = queryOptions();
              log(`RUNNING AQL UPDATE WITH LIMIT=${limit}. OPTIONS: ${JSON.stringify(o)}, QUERY OPTIONS: ${JSON.stringify(qo)}`);
              // generate random attribute values for random attributes
              query(`FOR doc IN ${c.name()} LIMIT @limit UPDATE doc WITH { pfihg: RAND(), ${generateAttributes(3)} } IN ${c.name()} OPTIONS ${JSON.stringify(o)}`, {limit}, qo);
            } else if (d >= 0.68) {
              const limit = Math.floor(Math.random() * 10) + 1;
              log(`RUNNING DOCUMENT SINGLE LOOKUP QUERY WITH LIMIT=${limit}`);
              query(`FOR doc IN ${c.name()} LIMIT @limit RETURN DOCUMENT(doc._id)`, {limit});
            } else if (d >= 0.66) {
              const limit = Math.floor(Math.random() * 10) + 1;
              log(`RUNNING DOCUMENT ARRAY LOOKUP QUERY WITH LIMIT=${limit}`);
              query(`LET keys = (FOR doc IN ${c.name()} LIMIT @limit RETURN doc._id) RETURN DOCUMENT(keys)`, {limit});
            } else if (d >= 0.65) {
              const limit = Math.floor(Math.random() * 10) + 1;
              let o = opts();
              let qo = queryOptions();
              log(`RUNNING DOCUMENT LOOKUP AND WRITE QUERY WITH LIMIT=${limit}. OPTIONS: ${JSON.stringify(o)}, QUERY OPTIONS: ${JSON.stringify(qo)}`);
              query(`FOR doc IN ${c.name()} LIMIT @limit LET d = DOCUMENT(doc._id) INSERT UNSET(doc, '_key') INTO ${c.name()} OPTIONS ${JSON.stringify(o)}`, {limit}, qo);
            } else if (d >= 0.63) {
              const skip = Math.floor(Math.random() * 100) + 1;
              const limit = Math.floor(Math.random() * 100) + 1;
              let qo = queryOptions(/*fullCount*/ true);
              log(`RUNNING DOCUMENT LOOKUP WITH LIMIT ${skip},${limit}. QUERY OPTIONS: ${JSON.stringify(qo)}`);
              query(`FOR doc IN ${c.name()} LIMIT @skip, @limit RETURN doc`, {skip, limit}, qo);
            } else if (d >= 0.25) {
              let d = docs();
              let o = Object.assign(opts(), queryOptions());
              log(`RUNNING INSERT WITH ${d.length} DOCS. OPTIONS: ${JSON.stringify(o)}`);
              d = d.length === 1 ? d[0] : d;
              c.insert(d, o);
            } else {
              let d = docs();
              log(`RUNNING REMOVE WITH ${d.length} DOCS`);
              d = d.length === 1 ? d[0] : d;
              c.remove(d);
            }
          } catch (err) {
            log(`executing previous command triggered exception ${err}`);
            // none of the following errors are expected in this test:
            if (require("@arangodb").errors.ERROR_CLUSTER_TIMEOUT.code === err.errorNum ||
                require("@arangodb").errors.ERROR_CLUSTER_BACKEND_UNAVAILABLE === err.errorNum ||
                require("@arangodb").errors.ERROR_CLUSTER_CONNECTION_LOST === err.errorNum ||
                require("@arangodb").errors.ERROR_LOCK_TIMEOUT.code === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_PARSE === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_EMPTY === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_COMPILE_TIME_OPTIONS === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_BIND_PARAMETERS_INVALID === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_BIND_PARAMETER_MISSING === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_BIND_PARAMETER_TYPE === err.errorNum ||
                require("@arangodb").errors.ERROR_QUEUE_FULL === err.errorNum) {
              throw err;
            }
          }
        }
        
        let willAbort = Math.random() < 0.2;
        while (trx) {
          try {
            if (willAbort) {
              log(`ABORTING`);
              trx.abort();
            } else {
              log(`COMMITING`);
              trx.commit();
            }
            trx = null;
          } catch (e) {
            if (require("@arangodb").errors.ERROR_TRANSACTION_NOT_FOUND.code === e.errorNum) {
              log(`aborting trx abort/commit loop because trx was not found`);
              break;
            }
            // due to contention we could have a lock timeout here
            if (require("@arangodb").errors.ERROR_LOCKED.code !== e.errorNum) {
              log(`aborting trx abort/commit loop because of unexpected error ${JSON.stringify(e)}`);
              throw e;
            }
            log("unable to " + (willAbort ? "abort" : "commit") + " transaction: " + String(e));
            require('internal').sleep(1);
          }
        }
      };
     
      let droppedFollowersBefore = {};
      getDBServers().forEach((s) => {
        droppedFollowersBefore[s.id] = getMetric(s.url, "arangodb_dropped_followers_total");
      });

      let testOpts = {
        collection: cn,
        numCollections
      };
      code = `(${code.toString()})(${JSON.stringify(testOpts)});`;
      
      const concurrency = 12;
      let tests = [];
      for (let i = 0; i < concurrency; ++i) {
        tests.push(["p" + i, code]);
      }

      // run the suite for a few minutes
      let client_ret = runParallelArangoshTests(tests, 3 * 60, coordination_cn);
      client_ret.forEach(client => { 
        if (client.failed) { 
          throw new Error("clients did not finish successfully"); 
        }
      });
     
      // assume no followers were dropped
      let droppedFollowersAfter = {};
      getDBServers().forEach((s) => {
        droppedFollowersAfter[s.id] = getMetric(s.url, "arangodb_dropped_followers_total");
      });
      assertEqual(droppedFollowersBefore, droppedFollowersAfter, JSON.stringify({droppedFollowersBefore, droppedFollowersAfter}));

      print(Date() + " Waiting for shards to get in sync");
      for (let i = 0; i < numCollections; ++i) {
        waitForShardsInSync(cn + i, 300, db[cn + i].properties().replicationFactor - 1);
      }
      print(Date() + " checking consistency");
      for (let i = 0; i < numCollections; ++i) {
        checkCollectionConsistency(cn + i);
      }
    }
  };
}

if (!versionHas('asan') && !versionHas('tsan') && !versionHas('coverage')) {
  jsunity.run(DeadlockSuite);
}
return jsunity.done();
