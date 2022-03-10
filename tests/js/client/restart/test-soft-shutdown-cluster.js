/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, arango */

//////////////////////////////////////////////////////////////////////////////
/// @brief test for soft shutdown of a coordinator
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
//////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const pu = require('@arangodb/testutils/process-utils');
const request = require("@arangodb/request");
const internal = require("internal");
const db = require("internal").db;
const time = internal.time;
const wait = internal.wait;
const statusExternal = internal.statusExternal;

var graph_module = require("@arangodb/general-graph");
var EPS = 0.0001;
let pregel = require("@arangodb/pregel");

const graphName = "UnitTest_pregel";
const vColl = "UnitTest_pregel_v", eColl = "UnitTest_pregel_e";

function testAlgoStart(a, p) {
  let pid = pregel.start(a, graphName, p);
  assertTrue(typeof pid === "string");
  return pid;
}

function testAlgoCheck(pid) {
  let stats = pregel.status(pid);
  console.warn("Pregel status:", stats);
  return stats.state !== "running" && stats.state !== "storing";
}

function getServers(role) {
  return global.instanceInfo.arangods.filter((instance) => instance.role === role);
};

function waitForShutdown(arangod, timeout) {
  let startTime = time();
  while (true) {
    if (time() > startTime + timeout) {
      assertTrue(false, "Instance did not shutdown quickly enough!");
      return;
    }
    let status = statusExternal(arangod.pid, false);
    console.warn("External status:", status);
    if (status.status === "TERMINATED") {
      arangod.exitStatus = status;
      delete arangod.pid;
      break;
    }
    wait(0.5);
  }
};

function waitForAlive(timeout, baseurl, data) {
  let res;
  let all = Object.assign(data || {}, { method: "get", timeout: 1, url: baseurl + "/_api/version" });
  const end = time() + timeout;
  while (time() < end) {
    res = request(all);
    if (res.status === 200 || res.status === 401 || res.status === 403) {
      break;
    }
    console.warn("waiting for server response from url " + baseurl);
    wait(0.5);
  }
  return res.status;
};

function restartInstance(arangod) {
  let options = global.testOptions;
  options.skipReconnect = false;
  pu.reStartInstance(options, global.instanceInfo, {});
  waitForAlive(30, arangod.url, {});
};

function testSuite() {
  let cn = "UnitTestSoftShutdown";

  return {
    setUp : function() {
      db._drop(cn);
      let collection = db._create(cn, {numberOfShards:2, replicationFactor:2});
      for (let i = 0; i < 10; ++i) {
        collection.insert({Hallo:i});
      }
    },

    tearDown : function() {
      db._drop(cn);
    },

    testSoftShutdownWithoutTraffic : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      // Now use soft shutdown API to shut coordinator down:
      let status = arango.GET("/_admin/shutdown");
      assertFalse(status.softShutdownOngoing);
      let res = arango.DELETE("/_admin/shutdown?soft=true");
      assertEqual("OK", res);
      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(0, status.AQLcursors);
      assertEqual(0, status.transactions);
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },

    testSoftShutdownWithAQLCursor : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      // Create an AQL cursor:
      let data = {
        query: `FOR x in ${cn} RETURN x`,
        batchSize: 1
      };

      let resp = arango.POST("/_api/cursor", data);
      console.warn("Produced AQL cursor:", resp);
      // Now use soft shutdown API to shut coordinator down:
      let res = arango.DELETE("/_admin/shutdown?soft=true");
      assertEqual("OK", res);
      let status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(1, status.AQLcursors);
      assertEqual(1, status.transactions);

      // It should fail to create a new cursor:
      let respFailed = arango.POST("/_api/cursor", data);
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now slowly read the cursor through:
      for (let i = 0; i < 8; ++i) {
        wait(2);
        let next = arango.PUT("/_api/cursor/" + resp.id,{});
        console.warn("Read document:", next);
        assertTrue(next.hasMore);
      }
      // And the last one:
      wait(2);
      let next = arango.PUT("/_api/cursor/" + resp.id,{});
      console.warn("Read last document:", next, "awaiting shutdown...");
      assertFalse(next.hasMore);
      assertFalse(next.error);
      assertEqual(200, next.code);

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },

    testSoftShutdownWithAQLCursorDeleted : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      // Create an AQL cursor:
      let data = {
        query: `FOR x in ${cn} RETURN x`,
        batchSize: 1
      };

      let resp = arango.POST("/_api/cursor", data);
      console.warn("Produced AQL cursor:", resp);
      let status = arango.GET("/_admin/shutdown");
      assertFalse(status.softShutdownOngoing);

      // Now use soft shutdown API to shut coordinator down:
      let res = arango.DELETE("/_admin/shutdown?soft=true");
      assertEqual("OK", res);
      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(1, status.AQLcursors);
      assertEqual(1, status.transactions);

      // It should fail to create a new cursor:
      let respFailed = arango.POST("/_api/cursor", data);
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now slowly read the cursor through:
      for (let i = 0; i < 8; ++i) {
        wait(2);
        let next = arango.PUT("/_api/cursor/" + resp.id,{});
        console.warn("Read document:", next);
        assertTrue(next.hasMore);
      }
      // Instead of the last one, now delete the cursor:
      wait(2);
      let next = arango.DELETE("/_api/cursor/" + resp.id,{});
      console.warn("Deleted cursor:", next, "awaiting shutdown...");
      assertFalse(next.hasMore);
      assertFalse(next.error);
      assertEqual(202, next.code);

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },

    testSoftShutdownWithStreamingTrx : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      // Create a streaming transaction:
      let data = { collections: {write: [cn]} };

      let resp = arango.POST("/_api/transaction/begin", data);
      console.warn("Produced transaction:", resp);

      // Now use soft shutdown API to shut coordinator down:
      let status = arango.GET("/_admin/shutdown");
      assertFalse(status.softShutdownOngoing);
      let res = arango.DELETE("/_admin/shutdown?soft=true");
      assertEqual("OK", res);
      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(0, status.AQLcursors);
      assertEqual(1, status.transactions);

      // It should fail to create a new trx:
      let respFailed = arango.POST("/_api/transaction/begin", data);
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now wait for some seconds:
      wait(10);

      // And commit the transaction:
      resp = arango.PUT(`/_api/transaction/${resp.result.id}`, {});
      assertFalse(resp.error);
      assertEqual(200, resp.code);

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },

    testSoftShutdownWithAQLStreamingTrxAborted : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      // Create a streaming transaction:
      let data = { collections: {write: [cn]} };

      let resp = arango.POST("/_api/transaction/begin", data);
      console.warn("Produced transaction:", resp);

      let status = arango.GET("/_admin/shutdown");
      assertFalse(status.softShutdownOngoing);

      // Now use soft shutdown API to shut coordinator down:
      let res = arango.DELETE("/_admin/shutdown?soft=true");
      assertEqual("OK", res);
      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(0, status.AQLcursors);
      assertEqual(1, status.transactions);

      // It should fail to create a new trx:
      let respFailed = arango.POST("/_api/transaction/begin", data);
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now wait for some seconds:
      wait(10);

      // And abort the transaction:
      resp = arango.DELETE(`/_api/transaction/${resp.result.id}`);
      assertFalse(resp.error);
      assertEqual(200, resp.code);

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },

    testSoftShutdownWithAsyncRequest : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      // Create a streaming transaction:
      let data = { query: `RETURN SLEEP(15)` };

      let resp = arango.POST_RAW("/_api/cursor", data, {"x-arango-async":"store"});
      console.warn("Produced cursor asynchronously:", resp);

      // Now use soft shutdown API to shut coordinator down:
      let status = arango.GET("/_admin/shutdown");
      assertFalse(status.softShutdownOngoing);
      let res = arango.DELETE("/_admin/shutdown?soft=true");
      assertEqual("OK", res);
      status = arango.GET("/_admin/shutdown");
      console.warn("status1:", status);
      assertTrue(status.softShutdownOngoing);
      assertEqual(0, status.AQLcursors);
      assertEqual(1, status.transactions);
      assertEqual(1, status.pendingJobs);
      assertEqual(0, status.doneJobs);
      assertFalse(status.allClear);

      // It should fail to create a new cursor:
      let respFailed = arango.POST_RAW("/_api/cursor", data, {"x-arango-async":"store"});
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now wait for some seconds:
      wait(20);   // Let query terminate, then the job should be done

      status = arango.GET("/_admin/shutdown");
      console.warn("status2:", status);
      assertTrue(status.softShutdownOngoing);
      assertEqual(0, status.AQLcursors);
      assertEqual(0, status.transactions);
      assertEqual(0, status.pendingJobs);
      assertEqual(1, status.doneJobs);
      assertFalse(status.allClear);

      // And collect the job result:
      resp = arango.PUT(`/_api/job/${resp.headers["x-arango-async-id"]}`, {});
      assertFalse(resp.error);
      assertEqual(201, resp.code);

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },

    testSoftShutdownWithQueuedLowPrio : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      // Create a streaming transaction:
      let op = `require("internal").wait(1); return 1;`;

      let jobs = [];
      for (let i = 0; i < 128; ++i) {
        // that is more than we have threads, so there should be a queue
        let resp = arango.POST_RAW("/_admin/execute", op, {"x-arango-async":"store"});
        jobs.push(resp.headers["x-arango-async-id"]);
      }
      console.warn("Produced 128 jobs asynchronously:", jobs);

      // Now use soft shutdown API to shut coordinator down:
      let status = arango.GET("/_admin/shutdown");
      console.warn("status0:", status);
      assertFalse(status.softShutdownOngoing);
      assertTrue(status.lowPrioOngoingRequests > 0);
      assertTrue(status.lowPrioQueuedRequests > 0);

      let res = arango.DELETE("/_admin/shutdown?soft=true");
      assertEqual("OK", res);
      status = arango.GET("/_admin/shutdown");
      console.warn("status1:", status);
      assertTrue(status.softShutdownOngoing);
      assertTrue(status.lowPrioOngoingRequests > 0);
      assertTrue(status.lowPrioQueuedRequests > 0);
      assertFalse(status.allClear);

      // Now until all jobs are done:
      let startTime = time();
      while (true) {
        status = arango.GET("/_admin/shutdown");
        if (status.lowPrioQueuedRequests === 0 &&
            status.lowPrioOngoingRequests === 0) {
          break;   // all good!
        }
        if (time() - startTime > 90) {
          // 45 seconds should be enough for at least 2 threads
          // to execute 128 requests, usually, it will be much faster.
          assertTrue(false, "finishing jobs took too long");
        }
        console.warn("status2:", status);
        assertTrue(status.softShutdownOngoing);
        assertEqual(128, status.pendingJobs + status.doneJobs);
        assertFalse(status.allClear);
        wait(0.5);
      }

      // Now collect the job results:
      for (let j of jobs) {
        assertEqual(1, arango.PUT(`/_api/job/${j}`, {}));
      }

      status = arango.GET("/_admin/shutdown");
      console.warn("status3:", status);
      assertTrue(status.softShutdownOngoing);
      assertEqual(0, status.pendingJobs);
      assertEqual(0, status.doneJobs);
      assertTrue(status.allClear);

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },

  };
}

function testSuitePregel() {
  'use strict';
  return {

    /////////////////////////////////////////////////////////////////////////
    /// @brief set up
    /////////////////////////////////////////////////////////////////////////

    setUp: function () {

      var guck = graph_module._list();
      var exists = guck.indexOf("demo") !== -1;
      if (exists || db.demo_v) {
        console.warn("Attention: graph_module gives:", guck, "or we have db.demo_v:", db.demo_v);
        return;
      }
      var graph = graph_module._create(graphName);
      db._create(vColl, { numberOfShards: 4 });
      graph._addVertexCollection(vColl);
      db._createEdgeCollection(eColl, {
        numberOfShards: 4,
        replicationFactor: 1,
        shardKeys: ["vertex"],
        distributeShardsLike: vColl
      });

      var rel = graph_module._relation(eColl, [vColl], [vColl]);
      graph._extendEdgeDefinitions(rel);

      var vertices = db[vColl];
      var edges = db[eColl];


      vertices.insert([{ _key: 'A', sssp: 3, pagerank: 0.027645934 },
                       { _key: 'B', sssp: 2, pagerank: 0.3241496 },
                       { _key: 'C', sssp: 3, pagerank: 0.289220 },
                       { _key: 'D', sssp: 2, pagerank: 0.0329636 },
                       { _key: 'E', sssp: 1, pagerank: 0.0682141 },
                       { _key: 'F', sssp: 2, pagerank: 0.0329636 },
                       { _key: 'G', sssp: -1, pagerank: 0.0136363 },
                       { _key: 'H', sssp: -1, pagerank: 0.01363636 },
                       { _key: 'I', sssp: -1, pagerank: 0.01363636 },
                       { _key: 'J', sssp: -1, pagerank: 0.01363636 },
                       { _key: 'K', sssp: 0, pagerank: 0.013636363 }]);

      edges.insert([{ _from: vColl + '/B', _to: vColl + '/C', vertex: 'B' },
                    { _from: vColl + '/C', _to: vColl + '/B', vertex: 'C' },
                    { _from: vColl + '/D', _to: vColl + '/A', vertex: 'D' },
                    { _from: vColl + '/D', _to: vColl + '/B', vertex: 'D' },
                    { _from: vColl + '/E', _to: vColl + '/B', vertex: 'E' },
                    { _from: vColl + '/E', _to: vColl + '/D', vertex: 'E' },
                    { _from: vColl + '/E', _to: vColl + '/F', vertex: 'E' },
                    { _from: vColl + '/F', _to: vColl + '/B', vertex: 'F' },
                    { _from: vColl + '/F', _to: vColl + '/E', vertex: 'F' },
                    { _from: vColl + '/G', _to: vColl + '/B', vertex: 'G' },
                    { _from: vColl + '/G', _to: vColl + '/E', vertex: 'G' },
                    { _from: vColl + '/H', _to: vColl + '/B', vertex: 'H' },
                    { _from: vColl + '/H', _to: vColl + '/E', vertex: 'H' },
                    { _from: vColl + '/I', _to: vColl + '/B', vertex: 'I' },
                    { _from: vColl + '/I', _to: vColl + '/E', vertex: 'I' },
                    { _from: vColl + '/J', _to: vColl + '/E', vertex: 'J' },
                    { _from: vColl + '/K', _to: vColl + '/E', vertex: 'K' }]);
    },

    //////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    //////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      graph_module._drop(graphName, true);
    },

    testPageRank: function () {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];

      // Start a pregel run:
      let pid = testAlgoStart("pagerank", { threshold: EPS / 1000, resultField: "result", store: true });

      console.warn("Started pregel run:", pid);

      // Now use soft shutdown API to shut coordinator down:
      let res = arango.DELETE("/_admin/shutdown?soft=true");
      console.warn("Shutdown got:", res);
      assertEqual("OK", res);
      let status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(1, status.pregelConductors);

      // It should fail to create a new pregel run:
      let gotException = false;
      try {
        testAlgoStart("pagerank", { threshold: EPS / 10, resultField: "result", store: true });
      } catch(e) {
        console.warn("Got exception for new run, good!", JSON.stringify(e));
        gotException = true;
      }
      assertTrue(gotException);

      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);

      let startTime = time();
      while (!testAlgoCheck(pid)) {
        console.warn("Pregel still running...");
        wait(0.2);
        if (time() - startTime > 60) {
          assertTrue(false, "Pregel did not finish in time.");
        }
      }

      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },

  };
}

jsunity.run(testSuite);
jsunity.run(testSuitePregel);
return jsunity.done();
