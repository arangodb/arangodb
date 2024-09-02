
/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, arango */

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
/// @author Max Neunhoeffer
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
const internal = require("internal");
const db = require("internal").db;
const time = internal.time;
const wait = internal.wait;
const {
  getCtrlCoordinators
} = require('@arangodb/test-helper');

function testSuite() {
  let cn = "UnitTestSoftShutdown";

  let coordinator;

  return {
    setUp : function() {
      if (coordinator && !coordinator.checkArangoAlive()) {
        throw new Error("coordinator not healthy!");
      }
      db._drop(cn);
      let collection = db._create(cn, {numberOfShards:2, replicationFactor:2});
      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({Hallo:i});
      }
      collection.save(docs);

      let coordinators = getCtrlCoordinators();
      assertTrue(coordinators.length > 0, "expect coordinators.length > 0");

      // TODO: Pick a random coordinator?
      coordinator = coordinators[0];
    },

    tearDown : function() {
      // Every test is supposed to initiate a soft shutdown
      // hence, once we tear down the test we expect
      // the coordinator to be gone
      if (coordinator.waitForInstanceShutdown(30)) {
        coordinator.restartOneInstance();
        coordinator.checkArangoConnection(10);
      } else {
        throw new Error("instance shutdown of coordinator failed!");
      }
      // we might have been connected to the coordinator that
      // has been shut down
      db._drop(cn);
    },

    testSoftShutdownWithoutTraffic : function() {
      // Now use soft shutdown API to shut coordinator down:
      let status = arango.GET("/_admin/shutdown");
      assertFalse(status.softShutdownOngoing, "expect status.softShutdownOngoing == false");
      let res = arango.DELETE("/_admin/shutdown?soft=true");
      assertEqual("OK", res, `expect res = ${res} == \"OK\"`);
      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing, `expect status.softShutdownOngoing = ${status.softShutdownOngoing} == true`);
      assertEqual(0, status.AQLcursors, "expect status.AQLcursors == 0");
    },

    testSoftShutdownWithAQLCursor : function() {
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
      // Expect our cursor and transaction to still be there
      assertTrue(status.AQLcursors >= 1, `expect status.AQLcursors = ${status.AQLcursors} >= 1`);
      assertTrue(status.transactions >= 1, `expect status.transactions = ${status.transactions} >= 1`);

      // It should fail to create a new cursor:
      let respFailed = arango.POST("/_api/cursor", data);
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now slowly read the cursor through:
      for (let i = 0; i < 8; ++i) {
        wait(2);
        let next = arango.PUT("/_api/cursor/" + resp.id, {});
        console.warn("Read document:", next);
        assertTrue(next.hasMore);
      }
      // And the last one:
      wait(2);
      let next = arango.PUT("/_api/cursor/" + resp.id, {});
      console.warn("Read last document:", next, "awaiting shutdown...");
      assertFalse(next.hasMore);
      assertFalse(next.error);
      assertEqual(200, next.code);
    },

    testSoftShutdownWithAQLCursorDeleted : function() {
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
      assertTrue(status.AQLcursors >= 1, `expect status.AQLcursors = ${status.AQLcursors} >= 1`);
      assertTrue(status.transactions >= 1, `expect status.transactions = ${status.transactions} >= 1`);

      // It should fail to create a new cursor:
      let respFailed = arango.POST("/_api/cursor", data);
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now slowly read the cursor through:
      for (let i = 0; i < 8; ++i) {
        wait(2);
        let next = arango.PUT("/_api/cursor/" + resp.id, {});
        console.warn("Read document:", next);
        assertTrue(next.hasMore);
      }
      // Instead of the last one, now delete the cursor:
      wait(2);
      let next = arango.DELETE("/_api/cursor/" + resp.id, {});
      console.warn("Deleted cursor:", next, "awaiting shutdown...");
      assertFalse(next.hasMore);
      assertFalse(next.error);
      assertEqual(202, next.code);
    },

    testSoftShutdownWithStreamingTrx : function() {
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
      assertTrue(status.softShutdownOngoing, "expect status.softShutdownOngoing == true");
      assertTrue(status.transactions >= 1, `expect status.transactions = ${status.transactions} >= 1`);

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
    },

    testSoftShutdownWithAQLStreamingTrxAborted : function() {
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
      assertTrue(status.softShutdownOngoing, "expect status.softShutdownOngoing == true");
      assertTrue(status.transactions >= 1, `expect status.transactions = ${status.transactions} >= 1`);

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
    },

    testSoftShutdownWithAsyncRequest : function() {
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
      assertTrue(status.softShutdownOngoing, "expect status.softShutdownOngoing == true");
      assertTrue(status.transactions >= 1, `expect status.transactions = ${status.transactions} >= 1`);
      // TODO: could the job already be done here? (yes if we for some reason slept for 15 secs)
      assertEqual(1, status.pendingJobs, `expect status.pendingJobs = ${status.pendingJobs} == 1`);
      assertEqual(0, status.doneJobs, `expect status.doneJobs = ${status.doneJobs} == 0`);
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
      // TODO: could there be pending jobs? probably not if there was just one
      // ongoing up when we started the soft shutdown (number of jobs should
      // only be going down)
      assertEqual(0, status.pendingJobs, `expect status.pendingJobs = ${status.pendingJobs} == 0`);
      assertEqual(1, status.doneJobs, `expect status.doneJobs = ${status.doneJobs}" == 1`);
      assertFalse(status.allClear);

      // And collect the job result:
      resp = arango.PUT(`/_api/job/${resp.headers["x-arango-async-id"]}`, {});
      assertFalse(resp.error);
      assertEqual(201, resp.code);
    },

    testSoftShutdownWithQueuedLowPrio : function() {
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
      assertFalse(status.softShutdownOngoing, "expecting status.softShutdownOngoing == false");
      assertTrue(status.lowPrioOngoingRequests > 0, `expect status.lowPrioOngoingRequests = ${status.lowPrioOngoingRequests} > 0`);
      assertTrue(status.lowPrioQueuedRequests > 0, `expect status.lowPrioQueuedRequests = ${status.lowPrioQueuedRequests} > 0`);

      let res = arango.DELETE("/_admin/shutdown?soft=true");
      assertEqual("OK", res);
      status = arango.GET("/_admin/shutdown");
      console.warn("status1:", status);
      assertTrue(status.softShutdownOngoing);
      assertTrue(status.lowPrioOngoingRequests > 0, `expect status.lowPrioOngoingRequests = ${status.lowPrioOngoingRequests} > 0`);
      assertTrue(status.lowPrioQueuedRequests > 0, `expect status.lowPrioQueuedRequests = ${status.lowPrioQueuedRequests} > 0`);
      assertFalse(status.allClear);

      // Now until all jobs are done:
      let startTime = time();
      const isCov = require("@arangodb/test-helper").versionHas('coverage');
      const timeout = (isCov) ? 90*4 : 90;
      while (true) {
        status = arango.GET("/_admin/shutdown");
        if (status.lowPrioQueuedRequests === 0 &&
            status.lowPrioOngoingRequests === 0) {
          break;   // all good!
        }
        if (time() - startTime > timeout) {
          // 45 seconds should be enough for at least 2 threads
          // to execute 128 requests, usually, it will be much faster.
          assertTrue(false, "finishing jobs took too long");
        }
        console.warn("status2:", status);
        assertTrue(status.softShutdownOngoing, "expect status.softShutdownOngoing == true");
        assertEqual(128, status.pendingJobs + status.doneJobs, `expect status.pendingJobs + status.doneJobs == 128; status.pendingJobs = ${status.pendingJobs}, status.doneJobs = ${status.doneJobs}`);
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
      assertEqual(0, status.pendingJobs, `expect status.pendingJobs = ${status.pendingJobs} == 0`);
      assertEqual(0, status.doneJobs, `expect status.doneJobs = ${status.doneJobs} == 0`);
      assertTrue(status.allClear);
    },

  };
}

jsunity.run(testSuite);
return jsunity.done();
