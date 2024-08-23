/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual,
   assertNotEqual, arango, print */
   
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
/// @author Manuel Pöter
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const _ = require('lodash');
const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const analyzers = require("@arangodb/analyzers");
const request = require("@arangodb/request");
const db = arangodb.db;
const {
  clearAllFailurePoints,
  debugSetFailAt,
  debugClearFailAt,
  deriveTestSuite,
  getEndpointById,
  getServersByType,
  getCoordinators,
  getDBServers,
  runParallelArangoshTests,
  waitForShardsInSync
} = require('@arangodb/test-helper');

const fetchRevisionTree = (serverUrl, shardId) => {
  let result = request({ method: "POST", url: serverUrl + "/_api/replication/batch", body: {ttl : 3600}, json: true });
  assertEqual(200, result.statusCode);
  const batch = JSON.parse(result.body);
  if (!batch.hasOwnProperty("id")) {
    throw "Could not create batch!";
  }
  
  result = request({ method: "GET",
    url: serverUrl + `/_api/replication/revisions/tree?collection=${encodeURIComponent(shardId)}&verification=true&batchId=${batch.id}&onlyPopulated=true`});
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
  let message = "";
  const getServerUrl = (serverId) => servers.filter((server) => server.id === serverId)[0].url + '/_db/' + encodeURIComponent(db._name());
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
        assertFalse(failed, `Cluster still not in sync - giving up!`);
      }
      console.warn(`Found some inconsistencies! Giving cluster some more time to get in sync before checking again... try=${tries}`);
      internal.sleep(10);
    }
  } while (failed);
};

const viewName = (name, isSearchAlias) => {
  if (isSearchAlias) {
    return name + "_sa";
  } else {
    return name + "_as";
  }
};

const checkViewConsistency = (cn, isSearchAlias, isMaterialize, viewCounts) => {
  let runQuery = (query) => {
    return db._query(query).toArray();
  };
  let expected;
  if (!isMaterialize) {
    expected = runQuery("FOR d IN " + cn + " COLLECT WITH COUNT INTO length RETURN length")[0];
  } else {
    expected = runQuery("FOR d IN " + cn + " RETURN d").length;
  }
  const checkView = (vn) => {
    let result;
    if (isMaterialize) {
      result = runQuery("FOR d IN " + vn + " OPTIONS {waitForSync: true} RETURN d._key").length;
    } else {
      result = runQuery("FOR d IN " + vn + " OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO length RETURN length")[0];
    }
    viewCounts.push({viewName: vn, viewCount: result, collectionName: cn, collectionCount: expected, isMaterialize});
  };

  checkView(viewName(cn + "_view", isSearchAlias));
  checkView(viewName(cn + "_view_ps", isSearchAlias));
  checkView(viewName(cn + "_view_sv", isSearchAlias));
  checkView(viewName(cn + "_view_ps_sv", isSearchAlias));
};

function BaseChaosSuite(testOpts) {
  const dbname = "test";
  // generate a random collection name
  const cn = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));
  const coordination_cn = cn + "_coord";

  const createAnalyzers = () => {
    try {
      analyzers.remove("testAnalyzer", true);
    } catch (e) {
    }
    analyzers.save("testAnalyzer", "delimiter", { delimiter: "/" });
  };

  const removeViews = (isSearchAlias) => {
    print(Date() + " removeViews: 1 deleting");
    let vn = viewName(cn + "_view", isSearchAlias);
    db._dropView(vn);

    print(Date() + " removeViews: 2 deleting");
    vn = viewName(cn + "_view_ps", isSearchAlias);
    db._dropView(vn);

    print(Date() + " removeViews: 3 deleting");
    vn = viewName(cn + "_view_sv", isSearchAlias);
    db._dropView(vn);

    print(Date() + " removeViews: 4 deleting");
    vn = viewName(cn + "_view_ps_sv", isSearchAlias);
    db._dropView(vn);
    print(Date() + " removeViews: done");
  };

  const createViews = (isSearchAlias) => {
    // TODO When will be fixed SEARCH-418 add to sort _id
    removeViews(isSearchAlias);
    let vn;

    vn = viewName(cn + "_view", isSearchAlias);
    if (isSearchAlias) {
      let i = db._collection(cn).ensureIndex({
        type: "inverted",
        includeAllFields: true,
        analyzer: "testAnalyzer",
      });
      db._createView(vn, "search-alias", {
        indexes: [{collection: cn, index: i.name}]
      });
    } else {
      db._createView(vn, "arangosearch", {
        links: {[cn]: {analyzers: ["testAnalyzer", "identity"], includeAllFields: true}}
      });
    }

    vn = viewName(cn + "_view_ps", isSearchAlias);
    if (isSearchAlias) {
      let i = db._collection(cn).ensureIndex({
        type: "inverted",
        includeAllFields: true,
        analyzer: "testAnalyzer",
        primarySort: {fields: [{field: "_id", asc: false}, {field: "_key", asc: true}]},
      });
      db._createView(vn, "search-alias", {
        indexes: [{collection: cn, index: i.name}]
      });
    } else {
      db._createView(vn, "arangosearch", {
        links: {[cn]: {includeAllFields: true}},
        primarySort: [{field: "_id", asc: false}, {field: "_key", asc: true}],
      });
    }

    vn = viewName(cn + "_view_sv", isSearchAlias);
    if (isSearchAlias) {
      let i = db._collection(cn).ensureIndex({
        type: "inverted",
        includeAllFields: true,
        analyzer: "testAnalyzer",
        storedValues: [["_id"], ["_key"]],
      });
      db._createView(vn, "search-alias", {
        indexes: [{collection: cn, index: i.name}]
      });
    } else {
      db._createView(vn, "arangosearch", {
        links: {[cn]: {includeAllFields: true}},
        storedValues: [["_id"], ["_key"]],
      });
    }

    vn = viewName(cn + "_view_ps_sv", isSearchAlias);
    if (isSearchAlias) {
      let i = db._collection(cn).ensureIndex({
        type: "inverted",
        includeAllFields: true,
        analyzer: "testAnalyzer",
        storedValues: [["_id"], ["_key"]],
        primarySort: {fields: [{field: "_id", asc: false}, {field: "_key", asc: true}]},
      });
      db._createView(vn, "search-alias", {
        indexes: [{collection: cn, index: i.name}]
      });
    } else {
      db._createView(vn, "arangosearch", {
        links: {[cn]: {includeAllFields: true}},
        storedValues: [["_id"], ["_key"]],
        primarySort: [{field: "_id", asc: false}, {field: "_key", asc: true}],
      });
    }
  };

  return {
    setUp: function () {
      if (testOpts.withOneShard) {
        db._createDatabase(dbname,  {sharding:"single"});
      } else {
        db._createDatabase(dbname);
      }
      db._useDatabase(dbname);

      if (testOpts.withViews) {
        createAnalyzers();
      }
      db._drop(cn);
      db._drop(coordination_cn);
      const replicationFactor = Math.max(2, getDBServers().length);
      const numberOfShards = (testOpts.withOneShard ? 1 : replicationFactor * 2);
      db._create(cn, {numberOfShards, replicationFactor});
      if (testOpts.withViews) {
        createViews(false);
        createViews(true);
      }
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
          debugSetFailAt(getEndpointById(server.id), "Query::finishTimeout");
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
          debugSetFailAt(getEndpointById(server.id), "lowCountCacheTimeout");
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
      db._useDatabase(dbname);
      try {
        print(Date() + " tearDown: flushing failurepoints");
        clearAllFailurePoints();
        if (testOpts.withViews) {
          removeViews(false);
          removeViews(true);
        }
        print(Date() + " tearDown: dropping " + cn);
        db._drop(cn);

        const shells = db[coordination_cn].all();
        if (shells.length > 0) {
          print("Found remaining docs in coordination collection:");
          print(shells);
        }
        print(Date() + " tearDown: dropping " + coordination_cn);
        db._drop(coordination_cn);
        print(Date() + " tearDown: done.");
        if (testOpts.withViews) {
          analyzers.remove('testAnalyzer');
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbname);
      }
    },
    
    testRunChaos: function () {
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
        const opts = (keepNull = false) => {
          let result = {};
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

          if (keepNull && Math.random() >= 0.5) {
            result.keepNull = true;
          }
          return result;
        };
        const queryOptions = (length) => {
          let result = {};
          if (testOpts.withIntermediateCommits) {
            if (Math.random() <= 0.5) {
              result.intermediateCommitCount = 7 + Math.floor(Math.random() * 10);
              if (2 <= length && length <= result.intermediateCommitCount) {
                result.intermediateCommitCount = length / 2;
              }
            }
          }
          return result;
        };
        
        let query = (...args) => db._query(...args);
        let trx = null;
        
        const logAllOps = true; // can be set to true for debugging purposes
        const log = (msg) => {
          if (logAllOps) {
            if (trx) {
              console.info(`${pid}: TRX ${trx.id()}: ${msg}`);
            } else {
              console.info(`${pid}: ${msg}`);
            }
          }
        };

        if (testOpts.withStreamingTransactions && Math.random() < 0.5) {
          trx = db._createTransaction({ collections: { write: [c.name()] } });
          log(`CREATED TRX`);
          c = trx.collection(testOpts.collection);
          query = (...args) => trx.query(...args);
        }

        const ops = trx === null ? 1 : Math.floor(Math.random() * 5) + 1;
        for (let op = 0; op < ops; ++op) {
          try {
            const d = Math.random();
            if (d >= 0.98 && testOpts.withTruncate) {
              log("RUNNING TRUNCATE");
              c.truncate();
            } else if (d >= 0.9) {
              let d = docs();
              let o = opts();
              let qo = queryOptions(d.length);
              log(`RUNNING AQL INSERT WITH ${d.length} DOCS. OPTIONS: ${JSON.stringify(o)}, QUERY OPTIONS: ${JSON.stringify(qo)}`);
              query(`FOR doc IN @docs INSERT doc INTO ${c.name()} OPTIONS ${JSON.stringify(o)}`, {docs: d}, qo);
            } else if (d >= 0.8) {
              const limit = Math.floor(Math.random() * 200);
              let qo = queryOptions(limit);
              log(`RUNNING AQL REMOVE WITH LIMIT=${limit}. QUERY OPTIONS: ${JSON.stringify(qo)}`);
              query(`FOR doc IN ${c.name()} LIMIT @limit REMOVE doc IN ${c.name()}`, {limit}, qo);
            } else if (d >= 0.75) {
              const limit = Math.floor(Math.random() * 2000);
              let qo = queryOptions(limit);
              log(`RUNNING AQL REPLACE WITH LIMIT=${limit}. QUERY OPTIONS: ${JSON.stringify(qo)}`);
              // generate random attribute values for random attribures
              query(`FOR doc IN ${c.name()} LIMIT @limit REPLACE doc WITH { pfihg: 434, fjgjg: RAND(), ${generateAttributes(3)} } IN ${c.name()}`, {limit}, qo);
            } else if (d >= 0.70) {
              const limit = Math.floor(Math.random() * 2000);
              let o = opts(/*keepNull*/ true);
              let qo = queryOptions(limit);
              log(`RUNNING AQL UPDATE WITH LIMIT=${limit}. OPTIONS: ${JSON.stringify(o)}, QUERY OPTIONS: ${JSON.stringify(qo)}`);
              // generate random attribute values for random attribures
              query(`FOR doc IN ${c.name()} LIMIT @limit UPDATE doc WITH { pfihg: RAND(), ${generateAttributes(3)} } IN ${c.name()} OPTIONS ${JSON.stringify(o)}`, {limit}, qo);
            } else if (d >= 0.68) {
              const limit = Math.floor(Math.random() * 10) + 1;
              log(`RUNNING DOCUMENT SINGLE LOOKUP QUERY WITH LIMIT=${limit}`);
              query(`FOR doc IN ${c.name()} LIMIT @limit RETURN DOCUMENT(doc._id)`, {limit});
            } else if (d >= 0.66) {
              const limit = Math.floor(Math.random() * 10) + 1;
              log(`RUNNING DOCUMENT ARRAY LOOKUP QUERY WITH LIMIT=${limit}`);
              query(`LET keys = (FOR doc IN ${c.name()} LIMIT @limit RETURN doc._id) RETURN DOCUMENT(keys)`, {limit});
              const lookupViews = (isSearchAlias) => {
                const r = Math.random();
                if (r < 0.25) {
                  query(`LET keys = (FOR doc IN ${viewName(cn + "_view", isSearchAlias)} LIMIT @limit RETURN doc._id) RETURN DOCUMENT(keys)`, {limit});
                } else if (r < 0.5) {
                  query(`LET keys = (FOR doc IN ${viewName(cn + "_view_ps", isSearchAlias)} LIMIT @limit RETURN doc._id) RETURN DOCUMENT(keys)`, {limit});
                } else if (r < 0.75) {
                  query(`LET keys = (FOR doc IN ${viewName(cn + "_view_sv", isSearchAlias)} LIMIT @limit RETURN doc._id) RETURN DOCUMENT(keys)`, {limit});
                } else {
                  query(`LET keys = (FOR doc IN ${viewName(cn + "_view_ps_sv", isSearchAlias)} LIMIT @limit RETURN doc._id) RETURN DOCUMENT(keys)`, {limit});
                }
              };
              if (testOpts.withViews) {
                if (Math.random() < 0.5) {
                  lookupViews(false);
                } else {
                  lookupViews(true);
                }
              }
            } else if (d >= 0.65) {
              const limit = Math.floor(Math.random() * 10) + 1;
              let o = opts();
              let qo = queryOptions(limit);
              log(`RUNNING DOCUMENT LOOKUP AND WRITE QUERY WITH LIMIT=${limit}. OPTIONS: ${JSON.stringify(o)}, QUERY OPTIONS: ${JSON.stringify(qo)}`);
              query(`FOR doc IN ${c.name()} LIMIT @limit LET d = DOCUMENT(doc._id) INSERT UNSET(doc, '_key') INTO ${c.name()} OPTIONS ${JSON.stringify(o)}`, {limit}, qo);
            } else if (d >= 0.60) {
              const limit = Math.floor(Math.random() * 100) + 1;
              let keys = [];
              for (let i = 0; i < keys; ++i) {
                keys.push(key());
              }
              log(`RUNNING DOCUMENT BATCH LOOKUP.`);
              c.document(keys);
            } else if (d >= 0.55) {
              const limit = Math.floor(Math.random() * 100) + 1;
              let keys = [];
              for (let i = 0; i < keys; ++i) {
                keys.push(key());
              }
              log(`RUNNING DOCUMENT AQL LOOKUP.`);
              query(`FOR doc IN ${c.name()} FILTER doc._key IN @keys RETURN doc`, {keys});
            } else if (d >= 0.25) {
              let d = docs();
              let o = Object.assign(opts(), queryOptions(d.length));
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
            if (require("@arangodb").errors.ERROR_QUERY_PARSE === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_EMPTY === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_COMPILE_TIME_OPTIONS === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_ACCESS_AFTER_MODIFICATION === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_BIND_PARAMETERS_INVALID === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_BIND_PARAMETER_MISSING === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_BIND_PARAMETER_UNDECLARED === err.errorNum ||
                require("@arangodb").errors.ERROR_QUERY_BIND_PARAMETER_TYPE === err.errorNum) {
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

      testOpts.collection = cn;
      code = `(${code.toString()})(${JSON.stringify(testOpts)});`;
      
      const concurrency = 4;
      let tests = [];
      for (let i = 0; i < concurrency; ++i) {
        tests.push(["p" + i, code]);
      }

      const old = db._name();
      db._useDatabase(dbname);

      try {
        // run the suite for a few minutes
        let client_ret = runParallelArangoshTests(tests, 3 * 60, coordination_cn);

        print(Date() + " Finished load test; Clearing failurepoints");
        clearAllFailurePoints();
        print(Date() + " Waiting for shards to get in sync");
        waitForShardsInSync(cn, 300, db[cn].properties().replicationFactor - 1);
        print(Date() + " checking consistency");
        checkCollectionConsistency(cn);
        let viewCounts = [];
        if (testOpts.withViews) {
          print(Date() + " checking view 1 consistency");
          checkViewConsistency(cn, false, false, viewCounts);
          print(Date() + " checking view 2 consistency");
          checkViewConsistency(cn, true, false, viewCounts);
          print(Date() + " checking view 3 consistency");
          checkViewConsistency(cn, false, true, viewCounts);
          print(Date() + " checking view 4 consistency");
          checkViewConsistency(cn, true, true, viewCounts);
        }
        print(Date() + " checking consistency done.");
        let failedViews = "";
        for (const c of viewCounts) {
          if (testOpts.withViews && testOpts.withTruncate) {
            // view counts can differ from collection counts if a DeleteRange
            // truncate operation was used. this is a known issue, noted in
            // BTS-1775. until this issue is fixed, we disable checking the
            // view counts here to avoid spurious errors.
            // TODO: remove the "continue" here once BTS-1775 is fixed, so that
            // view counts are again checked!
            continue;
          }

          if (c.viewCount !== c.collectionCount) {
            print(viewCounts);
            failedViews = " and views count mismatch";
            break;
          }
        }
        client_ret.forEach(client => { if (client.failed) { throw new Error("clients did not finish successfully " + failedViews); }});
        assertEqual(failedViews, "");
      } finally {
        // always change back into original database
        db._useDatabase(old);
      }
    }
  };
}

const params = ["IntermediateCommits", "FailurePoints", "Delays", "StreamingTransactions", "OneShard"];
const fixedParams = ["Truncate", "VaryingOverwriteMode", "Views"]; // these parameters are always enabled

// these aliases are used to shorten the names of test cases and filenames.
// if we use the full-length names, we would create filenames which are too 
// long on some filesystems.
const aliases = {
  "IntermediateCommits": "IntermComm",
  "FailurePoints": "Failures",
  "StreamingTransactions": "StreamTrx",
  "OneShard": "1Shard",
  "VaryingOverwriteMode": "OverwrMode",
};

const makeConfig = (paramValues) => {
  let suffix = "";
  let options = {};

  const build = (name, value) => {
    suffix += value ? "_with" : "_no";
    if (aliases.hasOwnProperty(name)) {
      // use alias to shorten filename
      suffix += aliases[name];
    } else {
      suffix += name;
    }
    // use full-length name for options
    options["with" + name] = value;
  };

  // variable parameters
  for (let j = 0; j < params.length; ++j) {
    build(params[j], paramValues[j]);
  }
  // fixed parameters
  for (const p of fixedParams) {
    build(p, true);
  }
  return { suffix, options };
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
