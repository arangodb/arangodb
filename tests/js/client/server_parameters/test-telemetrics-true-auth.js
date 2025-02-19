/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, assertNotNull, assertNotUndefined, arango */

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
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jwtSecret = 'abc';

if (getOptions === true) {
  return {
    'server.telemetrics-api': 'true',
    'server.telemetrics-api-max-requests': '15',
    'server.authentication': 'true',
    'server.jwt-secret': jwtSecret,
  };
}

const jsunity = require('jsunity');
const internal = require('internal');
const toArgv = require('internal').toArgv;
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const FoxxManager = require('@arangodb/foxx/manager');
const path = require('path');
const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'redirect');
const arangodb = require('@arangodb');
const db = arangodb.db;
const users = require('@arangodb/users');
const analyzers = require("@arangodb/analyzers");
const isCluster = require("internal").isCluster();
const isEnterprise = require("internal").isEnterprise();
let smartGraph = null;
if (isEnterprise) {
  smartGraph = require("@arangodb/smart-graph");
}
const arangosh = pu.ARANGOSH_BIN;
const userName = "abc";
const databaseName = "databaseTest";
const cn = "testCollection";
const cn2 = "smartGraphTestCollection";
const cn3 = "edgeTestCollection";
const vn1 = "testVertex1";
const vn2 = "testVertex2";


function createUser() {
  users.save(userName, '123', true);
  users.grantDatabase(userName, "_system", 'ro');
  users.reload();
}

function removeUser() {
  users.remove(userName);
}

function getTelemetricsResult() {
  let res;
  let numSecs = 0.5;
  while (true) {
    res = arango.getTelemetricsInfo();
    if (res !== undefined || numSecs >= 16) {
      break;
    }
    internal.sleep(numSecs);
    numSecs *= 2;
  }
  assertNotUndefined(res);
  return res;
}

function getTelemetricsSentToEndpoint() {
  let res;
  let numSecs = 0.5;
  while (true) {
    res = arango.sendTelemetricsToEndpoint("/test-redirect/redirect");
    if (res !== undefined || numSecs >= 16) {
      break;
    }
    internal.sleep(numSecs);
    numSecs *= 2;
  }
  assertNotUndefined(res);
  return res;
}

function parseIndexes(idxs) {
  idxs.forEach(idx => {
    assertTrue(idx.hasOwnProperty("mem"));
    assertTrue(idx.hasOwnProperty("type"));
    assertTrue(idx.hasOwnProperty("sparse"));
    assertTrue(idx.hasOwnProperty("unique"));
  });
}

//n_smart_colls, for now, doesn't give precise smart graph values, as it relies on the collection's
// isSmart() function and collections with no shards, such as the edge collection from the smart graph
// are not seen from the db server's methods, so should be in the coordinator and run a query on the database
function parseCollections(colls) {
  colls.forEach(coll => {
    assertTrue(coll.hasOwnProperty("n_shards"));
    assertTrue(coll.hasOwnProperty("rep_factor"));
    assertTrue(coll.hasOwnProperty("plan_id"));
    assertTrue(coll.hasOwnProperty("n_docs"));
    assertTrue(coll.hasOwnProperty("type"));
    assertTrue(coll.hasOwnProperty("n_edge"));
    assertTrue(coll.hasOwnProperty("n_fulltext"));
    assertTrue(coll.hasOwnProperty("n_geo"));
    assertTrue(coll.hasOwnProperty("n_hash"));
    assertTrue(coll.hasOwnProperty("n_inverted"));
    assertTrue(coll.hasOwnProperty("n_iresearch"));
    assertTrue(coll.hasOwnProperty("n_persistent"));
    assertTrue(coll.hasOwnProperty("n_primary"));
    assertTrue(coll.hasOwnProperty("n_skiplist"));
    assertTrue(coll.hasOwnProperty("n_ttl"));
    assertTrue(coll.hasOwnProperty("n_unknown"));
    assertTrue(coll.hasOwnProperty("n_mdi"));
    const idxs = coll["idxs"];
    parseIndexes(idxs);
  });
}

function parseDatabases(databases) {
  databases.forEach(database => {
    assertTrue(database.hasOwnProperty("colls"));
    const colls = database["colls"];
    parseCollections(colls);
    assertTrue(database.hasOwnProperty("single_shard"));
    assertTrue(database.hasOwnProperty("n_views"));
    assertTrue(database.hasOwnProperty("n_disjoint_smart_colls"));
    assertTrue(database.hasOwnProperty("n_doc_colls"));
    assertTrue(database.hasOwnProperty("n_edge_colls"));
    assertTrue(database.hasOwnProperty("n_smart_colls"));
  });

}

function parseServers(servers) {
  servers.forEach(host => {
    assertTrue(host.hasOwnProperty("role"));
    assertTrue(host.hasOwnProperty("maintenance"));
    assertTrue(host.hasOwnProperty("read_only"));
    assertTrue(host.hasOwnProperty("version"));
    assertTrue(host.hasOwnProperty("build"));
    assertTrue(host.hasOwnProperty("os"));
    assertTrue(host.hasOwnProperty("platform"));
    assertTrue(host.hasOwnProperty("phys_mem"));
    assertTrue(host["phys_mem"].hasOwnProperty("value"));
    assertTrue(host["phys_mem"].hasOwnProperty("overridden"));
    assertTrue(host.hasOwnProperty("n_cores"));
    assertTrue(host["n_cores"].hasOwnProperty("value"));
    assertTrue(host["n_cores"].hasOwnProperty("overridden"));
    assertTrue(host.hasOwnProperty("process_stats"));
    let stats = host["process_stats"];
    assertTrue(stats.hasOwnProperty("process_uptime"));
    assertTrue(stats.hasOwnProperty("n_threads"));
    assertTrue(stats.hasOwnProperty("virtual_size"));
    assertTrue(stats.hasOwnProperty("resident_set_size"));
    assertTrue(stats.hasOwnProperty("fileDescrtors"));
    assertTrue(stats.hasOwnProperty("fileDescrtorsLimit"));
    if (host.role !== "COORDINATOR") {
      assertTrue(host.hasOwnProperty("engine_stats"));
      stats = host["engine_stats"];
      assertTrue(stats.hasOwnProperty("cache_limit"));
      assertTrue(stats.hasOwnProperty("cache_allocated"));
      assertTrue(stats.hasOwnProperty("rocksdb_estimate_num_keys"));
      assertTrue(stats.hasOwnProperty("rocksdb_estimate_live_data_size"));
      assertTrue(stats.hasOwnProperty("rocksdb_live_sst_files_size"));
      assertTrue(stats.hasOwnProperty("rocksdb_block_cache_capacity"));
      assertTrue(stats.hasOwnProperty("rocksdb_block_cache_usage"));
      assertTrue(stats.hasOwnProperty("rocksdb_free_disk_space"));
      assertTrue(stats.hasOwnProperty("rocksdb_total_disk_space"));
    }
  });

}

function assertForTelemetricsResponse(telemetrics) {
  assertTrue(telemetrics.hasOwnProperty("deployment"));
  const deployment = telemetrics["deployment"];
  assertTrue(deployment.hasOwnProperty("type"));
  assertTrue(deployment.hasOwnProperty("license"));
  assertTrue(deployment.hasOwnProperty("servers"));
  const servers = deployment["servers"];
  parseServers(servers);
  if (isCluster) {
    assertEqual(deployment["type"], "cluster");
  } else {
    assertEqual(deployment["type"], "single");
  }
  if (deployment.hasOwnProperty("shards_statistics")) {
    const statistics = deployment["shards_statistics"];
    assertTrue(statistics.hasOwnProperty("databases"));
    assertTrue(statistics.hasOwnProperty("collections"));
    assertTrue(statistics.hasOwnProperty("shards"));
    assertTrue(statistics.hasOwnProperty("leaders"));
    assertTrue(statistics.hasOwnProperty("realLeaders"));
    assertTrue(statistics.hasOwnProperty("followers"));
    assertTrue(statistics.hasOwnProperty("servers"));
  }
  assertTrue(deployment.hasOwnProperty("date"));
  assertTrue(deployment.hasOwnProperty("databases"));
  const databases = deployment["databases"];
  parseDatabases(databases);
}

function telemetricsShellReconnectSmartGraphTestsuite() {
  return {

    setUpAll: function () {
      assertNotNull(smartGraph);
      try {
        smartGraph._drop(cn2, true);
      } catch (err) {
      }
      db._create(cn);
      const vn = "verticesCollection";
      const en = "edgesCollection";
      const graphRelation = [smartGraph._relation(en, vn, vn)];
      smartGraph._create(cn2, graphRelation, null, {smartGraphAttribute: "value1"});
      smartGraph._graph(cn2);
    },

    tearDownAll: function () {
      db._drop(cn);
      try {
        smartGraph._drop(cn2, true);
      } catch (err) {
      }
    },

    setUp: function () {
      // clear access counters
      arango.DELETE("/_admin/telemetrics");
    },

    testTelemetricsShellRequestByUserAuthorized: function () {
      try {
        arango.disableAutomaticallySendTelemetricsToEndpoint();
        arango.startTelemetrics();
        let telemetrics = getTelemetricsResult();
        assertForTelemetricsResponse(telemetrics);
        let deployment = telemetrics["deployment"];
        assertEqual(deployment["databases"].length, 1);


        db._createDatabase(databaseName);
        db._useDatabase(databaseName);

        let numShards = 1;
        if (isCluster) {
          numShards = 2;
        }
        db._create(cn, {numberOfShards: numShards, replicationFactor: 1});
        let docs = [];
        for (let i = 0; i < 2000; ++i) {
          docs.push({value: i + 1, name: "abc"});
        }
        db[cn].insert(docs);
        db[cn].ensureIndex({type: "persistent", fields: ["value"], name: "persistentIdx1"});
        db[cn].ensureIndex({type: "geo", geoJson: true, fields: ["geo"], name: "geoIdx1"});
        db[cn].ensureIndex({type: "geo", geoJson: true, fields: ["otherGeo"], name: "geoIdx2"});
        analyzers.save("testAnalyzer", "delimiter", { delimiter: " " }, [ "frequency"]);


        db._createView('testView1', 'arangosearch', { links: { [cn]: {includeAllFields: true, analyzers: ["testAnalyzer"] }}});
        let db1DocsCount = 0;
        db._collections().forEach((coll) => {
          db1DocsCount += coll.count();
        });
        db._useDatabase("_system");

        arango.restartTelemetrics();

        telemetrics = getTelemetricsResult();
        assertForTelemetricsResponse(telemetrics);
        deployment = telemetrics["deployment"];
        assertEqual(deployment["databases"].length, 2);
        const databases = deployment["databases"];
        const db1Idx = databases[0]["n_views"] === 1 ? 0 : 1;
        assertEqual(databases[db1Idx]["n_views"], 1);
        let numColls = 0;
        databases.forEach((database, idx) => {
          let totalNumDocs = 0;
          if (idx === db1Idx) {
            // there are already the 7 system collections in each database + 1 created here
            assertEqual(database["n_doc_colls"], 9);
            database["colls"].forEach(coll => {
              const nDocs = coll["n_docs"];
              totalNumDocs += nDocs;
              if (nDocs === 2000) {
                assertEqual(coll["n_shards"], numShards);
                assertEqual(coll["rep_factor"], 1);
                assertEqual(coll["n_primary"], 1);
                assertEqual(coll["n_persistent"], 1);
                assertEqual(coll["n_iresearch"], 1);
                assertEqual(coll["n_geo"], 2);
                numColls++;
                assertEqual(coll.idxs.length, 5);
              } else {
                assertTrue(nDocs === 0 || nDocs === 1);
                //system collections would have replication factor 2, our one has 1, so both are allowed
                // We cannot distinguish which variant we analyse.
                assertTrue(coll["rep_factor"] === 1 || coll["rep_factor"] === 2);
                assertEqual(coll["n_primary"], 1);
                assertEqual(coll["n_persistent"], 0);
                assertEqual(coll["n_geo"], 0);
              }
            });
            assertEqual(totalNumDocs, db1DocsCount);
          } else {
            //includes the collections created for the smart graph
            assertEqual(database["n_doc_colls"], 14);
            database["colls"].forEach(coll => {
              assertEqual(coll["n_primary"], 1);
              assertEqual(coll["n_persistent"], 0);
              //system collections would have replication factor 2, our one has 1, so both are allowed
              // We cannot distinguish which variant we analyse.
              assertTrue(coll["rep_factor"] === 1 || coll["rep_factor"] === 2);
              assertEqual(coll["n_geo"], 0);
            });
          }
        });
      } finally {
        db._useDatabase("_system");
        try {
          db._dropDatabase(databaseName);
        } catch (err) {

        }
      }
    },

    testTelemetricsShellRequestByUserNotAuthorized: function () {
      try {
        createUser();
        arango.reconnect(arango.getEndpoint(), '_system', userName, "123");
        arango.disableAutomaticallySendTelemetricsToEndpoint();
        arango.startTelemetrics();
        const res = getTelemetricsResult();
        assertTrue(res.hasOwnProperty("errorNum"));
        assertTrue(res.hasOwnProperty("errorMessage"));
        assertEqual(res.errorNum, internal.errors.ERROR_HTTP_FORBIDDEN.code);
        assertTrue(res.errorMessage.includes("insufficient permissions"));
      } finally {
        arango.reconnect(arango.getEndpoint(), '_system', 'root', '');
        if (users.exists(userName)) {
          removeUser();
        }
      }
    },

  };
}

function telemetricsShellReconnectGraphTestsuite() {
  return {
    setUpAll: function () {
      db._create(cn);
      let coll = db._createEdgeCollection(cn3, {numberOfShards: 2});
      coll.insert({_from: vn1 + "/test1", _to: vn2 + "/test2"});
    },

    tearDownAll: function () {
      db._drop(cn);
      db._drop(cn3);
    },

    setUp: function () {
      // clear access counters
      arango.DELETE("/_admin/telemetrics");
    },

    testTelemetricsShellRequestByUserAuthorized2: function () {
      try {
        arango.disableAutomaticallySendTelemetricsToEndpoint();
        arango.startTelemetrics();
        let telemetrics = getTelemetricsResult();
        assertForTelemetricsResponse(telemetrics);
        let deployment = telemetrics["deployment"];
        assertEqual(deployment["databases"].length, 1);


        db._createDatabase(databaseName);
        db._useDatabase(databaseName);

        let numShards = 1;
        if (isCluster) {
          numShards = 2;
        }
        db._create(cn, {numberOfShards: numShards, replicationFactor: 1});
        let docs = [];
        for (let i = 0; i < 2000; ++i) {
          docs.push({value: i + 1, name: "abc"});
        }
        db[cn].insert(docs);
        db[cn].ensureIndex({type: "persistent", fields: ["value"], name: "persistentIdx1"});
        db[cn].ensureIndex({type: "geo", geoJson: true, fields: ["geo"], name: "geoIdx1"});
        db[cn].ensureIndex({type: "geo", geoJson: true, fields: ["otherGeo"], name: "geoIdx2"});

        analyzers.save("testAnalyzer", "delimiter", { delimiter: " " }, [ "frequency"]);

        db._createView('testView1', 'arangosearch', { links: { [cn]: {includeAllFields: true, analyzers: ["testAnalyzer"] }}});
        let db1DocsCount = 0;
        db._collections().forEach((coll) => {
          db1DocsCount += coll.count();
        });
        db._useDatabase("_system");

        arango.restartTelemetrics();

        telemetrics = getTelemetricsResult();
        assertForTelemetricsResponse(telemetrics);
        deployment = telemetrics["deployment"];
        assertEqual(deployment["databases"].length, 2);
        const databases = deployment["databases"];
        const db1Idx = databases[0]["n_views"] === 1 ? 0 : 1;
        assertEqual(databases[db1Idx]["n_views"], 1);
        let numColls = 0;
        databases.forEach((database, idx) => {
          let totalNumDocs = 0;
          if (idx === db1Idx) {
            // there are already the 8 system collections in the database + 1 created here
            assertEqual(database["n_doc_colls"], 9);
            database["colls"].forEach(coll => {
              const nDocs = coll["n_docs"];
              totalNumDocs += nDocs;
              if (nDocs === 2000) {
                assertEqual(coll["n_shards"], numShards);
                assertEqual(coll["rep_factor"], 1);
                assertEqual(coll["n_primary"], 1);
                assertEqual(coll["n_persistent"], 1);
                assertEqual(coll["n_iresearch"], 1);
                assertEqual(coll["n_geo"], 2);
                numColls++;
                assertEqual(coll.idxs.length, 5);
              } else {
                assertTrue(nDocs === 0 || nDocs === 1);
                //system collections have replication factor 2
                //system collections would have replication factor 2, our one has 1, so both are allowed
                // We cannot distinguish which variant we analyse.
                assertTrue(coll["rep_factor"] === 1 || coll["rep_factor"] === 2);
                assertEqual(coll["n_primary"], 1);
                assertEqual(coll["n_persistent"], 0);
                assertEqual(coll["n_geo"], 0);
              }
            });
            assertEqual(totalNumDocs, db1DocsCount);
          } else {
            // there are already 11 collections in the _system database + 2 created here
            assertEqual(database["n_doc_colls"], 13);
            assertEqual(database["n_edge_colls"], 1);
            database["colls"].forEach(coll => {
              assertEqual(coll["n_primary"], 1);
              assertEqual(coll["n_persistent"], 0);
              //system collections would have replication factor 2, our one has 1, so both are allowed
              // We cannot distinguish which variant we analyse.
              assertTrue(coll["rep_factor"] === 1 || coll["rep_factor"] === 2);
              assertEqual(coll["n_geo"], 0);
            });
          }
        });
      } finally {
        db._useDatabase("_system");
        try {
          db._dropDatabase(databaseName);
        } catch (err) {

        }
      }
    },

    testTelemetricsShellExecuteScriptLeave: function () {
      // this is for when the user logs into the shell to run a script and then leaves the shell immediately. The shell
      // should not hang because telemetrics is still in progress.
      // The telemetrics process should be transparent to the user and not influence user experience.
      // The shell should be closed as soon as the script executes despite of telemetrics being in progress, not
      // mattering whether telemetrics is still in progress by fetching info from servers or by sending the info to
      // the endpoint.
      // It could happen, for example, that the script executes, and telemetrics is still in progress, the data is about
      // to be sent to the warehouse and could hang until a connection timeout (30s) is reached. Therefore, the
      // connecting socket is made non-blocking to be able to abort the request.The shell should be closed immediately
      // as it would behave had the telemetrics process not taken place.
      let file = fs.getTempFile() + "-telemetrics";
      fs.write(file, `(function() { const x = 0;})();`);
      let options = internal.options();
      let endpoint = arango.getEndpoint().replace(/\+vpp/, '').replace(/^http:/, 'tcp:').replace(/^https:/, 'ssl:').replace(/^h2:/, 'tcp:');
      const args = {
        'javascript.startup-directory': options['javascript.startup-directory'],
        'server.endpoint': endpoint,
        'server.username': arango.connectedUser(),
        'server.password': '',
        'javascript.execute': file,
        'client.failure-points': 'startTelemetricsForTest',
      };
      const argv = toArgv(args);

      for (let o in options['javascript.module-directory']) {
        argv.push('--javascript.module-directory');
        argv.push(options['javascript.module-directory'][o]);
      }

      let result = internal.executeExternal(arangosh, argv, false /*usePipes*/);
      assertTrue(result.hasOwnProperty('pid'));
      let numSecs = 0.5;
      let status = {};
      while (true) {
        status = internal.statusExternal(result.pid);
        if (status.status === "TERMINATED" || numSecs >= 16) {
          break;
        }
        internal.sleep(numSecs);
        numSecs *= 2;
      }
      assertEqual(status.status, "TERMINATED", "couldn't leave shell immediately after executing script");
    }
  };
}


function telemetricsApiReconnectSmartGraphTestsuite() {
  return {

    setUpAll: function () {
      assertNotNull(smartGraph);
      try {
        smartGraph._drop(cn2, true);
      } catch (err) {
      }
      db._create(cn);
      const vn = "verticesCollection";
      const en = "edgesCollection";
      const graphRelation = [smartGraph._relation(en, vn, vn)];
      smartGraph._create(cn2, graphRelation, null, {smartGraphAttribute: "value1", replicationFactor: 1});
      smartGraph._graph(cn2);
    },

    tearDownAll: function () {
      db._drop(cn);
      try {
        smartGraph._drop(cn2, true);
      } catch (err) {
      }
    },

    setUp: function () {
      // clear access counters
      arango.DELETE("/_admin/telemetrics");
    },

    testTelemetricsApiRequestByUserAuthorized: function () {
      try {
        arango.disableAutomaticallySendTelemetricsToEndpoint();
        arango.startTelemetrics();
        let telemetrics = arango.GET("/_admin/telemetrics");
        assertForTelemetricsResponse(telemetrics);
        let deployment = telemetrics["deployment"];
        assertEqual(deployment["databases"].length, 1);


        db._createDatabase(databaseName);
        db._useDatabase(databaseName);

        let numShards = 1;
        if (isCluster) {
          numShards = 2;
        }
        db._create(cn, {numberOfShards: numShards, replicationFactor: 1});
        let docs = [];
        for (let i = 0; i < 2000; ++i) {
          docs.push({value: i + 1, name: "abc"});
        }
        db[cn].insert(docs);
        db[cn].ensureIndex({type: "persistent", fields: ["value"], name: "persistentIdx1"});
        db[cn].ensureIndex({type: "geo", geoJson: true, fields: ["geo"], name: "geoIdx1"});
        db[cn].ensureIndex({type: "geo", geoJson: true, fields: ["otherGeo"], name: "geoIdx2"});
        analyzers.save("testAnalyzer", "delimiter", { delimiter: " " }, [ "frequency"]);


        db._createView('testView1', 'arangosearch',{ links: { [cn]: {includeAllFields: true, analyzers: ["testAnalyzer"] }}});
        let db1DocsCount = 0;
        db._collections().forEach((coll) => {
          db1DocsCount += coll.count();
        });
        db._useDatabase("_system");
        telemetrics = arango.GET("/_admin/telemetrics");

        assertForTelemetricsResponse(telemetrics);
        deployment = telemetrics["deployment"];
        assertEqual(deployment["databases"].length, 2);
        const databases = deployment["databases"];
        const db1Idx = databases[0]["n_views"] === 1 ? 0 : 1;
        assertEqual(databases[db1Idx]["n_views"], 1);
        let numColls = 0;
        databases.forEach((database, idx) => {
          let totalNumDocs = 0;
          if (idx === db1Idx) {
            // there are already the 7 system collections in each database + 1 created here
            assertEqual(database["n_doc_colls"], 9);
            database["colls"].forEach(coll => {
              const nDocs = coll["n_docs"];
              totalNumDocs += nDocs;
              if (nDocs === 2000) {
                assertEqual(coll["n_shards"], numShards);
                assertEqual(coll["rep_factor"], 1);
                assertEqual(coll["n_primary"], 1);
                assertEqual(coll["n_persistent"], 1);
                assertEqual(coll["n_iresearch"], 1);
                assertEqual(coll["n_geo"], 2);
                numColls++;
                assertEqual(coll.idxs.length, 5);
              } else {
                assertTrue(nDocs === 0 || nDocs === 1);
                //system collections would have replication factor 2
                //system collections would have replication factor 2, our one has 1, so both are allowed
                // We cannot distinguish which variant we analyse.
                assertTrue(coll["rep_factor"] === 1 || coll["rep_factor"] === 2);
                assertEqual(coll["n_primary"], 1);
                assertEqual(coll["n_persistent"], 0);
                assertEqual(coll["n_geo"], 0);
              }
            });
            assertEqual(totalNumDocs, db1DocsCount);
          } else {
            //includes the collections created for the smart graph
            assertEqual(database["n_doc_colls"], 14);
            database["colls"].forEach(coll => {
              assertEqual(coll["n_primary"], 1);
              assertEqual(coll["n_persistent"], 0);
              //system collections would have replication factor 2, our one has 1, so both are allowed
              // We cannot distinguish which variant we analyse.
              assertTrue(coll["rep_factor"] === 1 || coll["rep_factor"] === 2);
              assertEqual(coll["n_geo"], 0);
            });
          }
        });
      } finally {
        db._useDatabase("_system");
        try {
          db._dropDatabase(databaseName);
        } catch (err) {

        }
      }
    },

    testTelemetricsApiRequestByUserNotAuthorized: function () {
      try {
        createUser();
        arango.reconnect(arango.getEndpoint(), '_system', userName, "123");
        arango.disableAutomaticallySendTelemetricsToEndpoint();
        arango.startTelemetrics();
        const res = arango.GET("/_admin/telemetrics");
        assertTrue(res.hasOwnProperty("errorNum"));
        assertTrue(res.hasOwnProperty("errorMessage"));
        assertEqual(res.errorNum, internal.errors.ERROR_HTTP_FORBIDDEN.code);
        assertTrue(res.errorMessage.includes("insufficient permissions"));
      } finally {
        arango.reconnect(arango.getEndpoint(), '_system', 'root', '');
        if (users.exists(userName)) {
          removeUser();
        }
      }
    },

    testTelemetricsInsertingEndpointReqBodyAsDocument: function () {
      arango.disableAutomaticallySendTelemetricsToEndpoint();
      arango.startTelemetrics();
      const telemetrics = arango.GET("/_admin/telemetrics");
      assertForTelemetricsResponse(telemetrics);
      const doc = arango.POST("/_api/document/" + cn, telemetrics);
      assertTrue(doc.hasOwnProperty("_id"));
      assertTrue(doc.hasOwnProperty("_key"));
      assertTrue(doc.hasOwnProperty("_rev"));
      assertForTelemetricsResponse(db[cn].document(doc["_id"]));
    },

  };
}

function telemetricsApiReconnectGraphTestsuite() {
  return {

    setUpAll: function () {
      db._create(cn);
      let coll = db._createEdgeCollection(cn3, {numberOfShards: 2});
      coll.insert({_from: vn1 + "/test1", _to: vn2 + "/test2"});
    },

    tearDownAll: function () {
      db._drop(cn);
      db._drop(cn3);
    },

    setUp: function () {
      // clear access counters
      arango.DELETE("/_admin/telemetrics");
    },

    testTelemetricsApiRequestByUserAuthorized2: function () {
      try {
        arango.disableAutomaticallySendTelemetricsToEndpoint();
        arango.startTelemetrics();
        let telemetrics = arango.GET("/_admin/telemetrics");
        assertForTelemetricsResponse(telemetrics);
        let deployment = telemetrics["deployment"];
        assertEqual(deployment["databases"].length, 1);

        db._createDatabase(databaseName);
        db._useDatabase(databaseName);

        let numShards = 1;
        if (isCluster) {
          numShards = 2;
        }
        db._create(cn, {numberOfShards: numShards, replicationFactor: 1});
        let docs = [];
        for (let i = 0; i < 2000; ++i) {
          docs.push({value: i + 1, name: "abc"});
        }
        db[cn].insert(docs);
        db[cn].ensureIndex({type: "persistent", fields: ["value"], name: "persistentIdx1"});
        db[cn].ensureIndex({type: "geo", geoJson: true, fields: ["geo"], name: "geoIdx1"});
        db[cn].ensureIndex({type: "geo", geoJson: true, fields: ["otherGeo"], name: "geoIdx2"});

        analyzers.save("testAnalyzer", "delimiter", { delimiter: " " }, [ "frequency"]);


        db._createView('testView1', 'arangosearch', { links: { [cn]: {includeAllFields: true, analyzers: ["testAnalyzer"] }}});
        let db1DocsCount = 0;
        db._collections().forEach((coll) => {
          db1DocsCount += coll.count();
        });
        telemetrics = arango.GET("/_admin/telemetrics");

        assertForTelemetricsResponse(telemetrics);
        deployment = telemetrics["deployment"];
        assertEqual(deployment["databases"].length, 2);
        const databases = deployment["databases"];
        const db1Idx = databases[0]["n_views"] === 1 ? 0 : 1;
        assertEqual(databases[db1Idx]["n_views"], 1);
        let numColls = 0;
        databases.forEach((database, idx) => {
          let totalNumDocs = 0;
          if (idx === db1Idx) {
            // there are already the 7 system collections in each database + 1 created here
            assertEqual(database["n_doc_colls"], 9);
            database["colls"].forEach(coll => {
              const nDocs = coll["n_docs"];
              totalNumDocs += nDocs;
              if (nDocs === 2000) {
                assertEqual(coll["n_shards"], numShards);
                assertEqual(coll["rep_factor"], 1);
                assertEqual(coll["n_primary"], 1);
                assertEqual(coll["n_persistent"], 1);
                assertEqual(coll["n_iresearch"], 1);
                assertEqual(coll["n_geo"], 2);
                numColls++;
                assertEqual(coll.idxs.length, 5);
              } else {
                assertTrue(nDocs === 0 || nDocs === 1);
                //system collections would have replication factor 2, our one has 1, so both are allowed
                // We cannot distinguish which variant we analyse.
                assertTrue(coll["rep_factor"] === 1 || coll["rep_factor"] === 2);
                assertEqual(coll["n_primary"], 1);
                assertEqual(coll["n_persistent"], 0);
                assertEqual(coll["n_geo"], 0);
              }
            });
            assertEqual(totalNumDocs, db1DocsCount);
          } else {
            assertEqual(database["n_doc_colls"], 13);
            assertEqual(database["n_edge_colls"], 1);
            database["colls"].forEach(coll => {
              assertEqual(coll["n_primary"], 1);
              assertEqual(coll["n_persistent"], 0);
              assertTrue(coll["rep_factor"] === 1 || coll["rep_factor"] === 2);
              assertEqual(coll["n_geo"], 0);
            });
          }
        });
      } finally {
        db._useDatabase("_system");
        try {
          db._dropDatabase(databaseName);
        } catch (err) {

        }
      }
    },
  };
}

function telemetricsSendToEndpointRedirectTestsuite() {
  const mount = '/test-redirect';

  return {
    setUpAll: function () {
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch (err) {
      }
      try {
        FoxxManager.install(basePath, mount);
      } catch (err) {
      }
      db._create(cn);
      let coll = db._createEdgeCollection(cn3, {numberOfShards: 2});
      coll.insert({_from: vn1 + "/test1", _to: vn2 + "/test2"});
    },

    tearDownAll: function () {
      db._drop(cn);
      db._drop(cn3);
      try {
        FoxxManager.uninstall(mount, {force: true});
      } catch (err) {
      }
    },

    setUp: function () {
      // clear access counters
      arango.DELETE("/_admin/telemetrics");
    },

    testTelemetricsSendToEndpointWithRedirection: function () {
      arango.disableAutomaticallySendTelemetricsToEndpoint();
      arango.startTelemetrics();
      getTelemetricsResult();
      const telemetrics = getTelemetricsSentToEndpoint();
      assertForTelemetricsResponse(telemetrics);
    },
  };
}

function telemetricsEnhancingOurCalm() {
  return {
    setUp: function () {
      // clear access counters
      arango.DELETE("/_admin/telemetrics");
    },

    testTelemetricsApiCallWithoutArangoshUserAgent: function () {
      for (let i = 0; i < 20; ++i) {
        let res = arango.GET_RAW("/_admin/telemetrics");
        assertEqual(200, res.code);
      }
    },

    testTelemetricsApiCallAndSetArangoshUserAgent: function () {
      const n = 50;

      let successful = 0;
      let failed = 0;
      for (let i = 0; i < n; ++i) {
        let res = arango.GET_RAW("/_admin/telemetrics", {"user-agent": "arangosh/3.11.0"});
        if (res.code === 200) {
          ++successful;
        }
        if (res.code === 420) {
          assertEqual(420, res.errorNum);
          ++failed;
          break;
        }
      }
      assertNotEqual(failed, 0);
      assertNotEqual(successful, n);
    },

  };
}

if (isEnterprise) {
  jsunity.run(telemetricsShellReconnectSmartGraphTestsuite);
  jsunity.run(telemetricsApiReconnectSmartGraphTestsuite);
}
jsunity.run(telemetricsShellReconnectGraphTestsuite);
jsunity.run(telemetricsApiReconnectGraphTestsuite);
jsunity.run(telemetricsSendToEndpointRedirectTestsuite);
jsunity.run(telemetricsEnhancingOurCalm);

return jsunity.done();
