/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for soft shutdown of a coordinator
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2021, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
//const _ = require('lodash');
const pu = require('@arangodb/testutils/process-utils');
const request = require("@arangodb/request");
const internal = require("internal");
const time = internal.time;
const wait = internal.wait;
const statusExternal = internal.statusExternal;

function testSuite() {
  let cn = "UnitTestSoftShutdown";

  let getServers = function (role) {
    return global.instanceInfo.arangods.filter((instance) => instance.role === role);
  };

  let waitForShutdown = function(arangod, timeout) {
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

  let waitForAlive = function (timeout, baseurl, data) {
    let tries = 0, res;
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

  let restartInstance = function(arangod) {
    let options = global.testOptions;
    pu.reStartInstance(options, instanceInfo, {});
    waitForAlive(30, arangod.url, {});
  };

  return {
    setUp : function() {
      db._drop(cn);
      collection = db._create(cn, {numberOfShards:2, replicationFactor:2});
      for (i = 0; i < 10; ++i) {
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
      let instanceInfo = global.instanceInfo;

      // Now use soft shutdown API to shut coordinator down:
      let status = arango.GET("/_admin/shutdown");
      assertFalse(status.softShutdownOngoing);
      arango.DELETE("/_admin/shutdown?soft=true");
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
      let instanceInfo = global.instanceInfo;

      // Create an AQL cursor:
      let data = {
        query: `FOR x in ${cn} RETURN x`,
        batchSize: 1
      };

      let resp = arango.POST("/_api/cursor", data);
      console.warn("Produced AQL cursor:", resp);
      // Now use soft shutdown API to shut coordinator down:
      arango.DELETE("/_admin/shutdown?soft=true");
      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(1, status.AQLcursors);
      assertEqual(0, status.transactions);

      // It should fail to create a new cursor:
      let respFailed = arango.POST("/_api/cursor", data);
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now slowly read the cursor through:
      for (i = 0; i < 8; ++i) {
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

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },
    
    testSoftShutdownWithAQLCursorDeleted : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      let instanceInfo = global.instanceInfo;

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
      arango.DELETE("/_admin/shutdown?soft=true");
      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(1, status.AQLcursors);
      assertEqual(0, status.transactions);

      // It should fail to create a new cursor:
      let respFailed = arango.POST("/_api/cursor", data);
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now slowly read the cursor through:
      for (i = 0; i < 8; ++i) {
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

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },
    
    testSoftShutdownWithStreamingTrx : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      let instanceInfo = global.instanceInfo;

      // Create a streaming transaction:
      let data = { collections: {write: [cn]} };

      let resp = arango.POST("/_api/transaction/begin", data);
      console.warn("Produced transaction:", resp);

      // Now use soft shutdown API to shut coordinator down:
      let status = arango.GET("/_admin/shutdown");
      assertFalse(status.softShutdownOngoing);
      arango.DELETE("/_admin/shutdown?soft=true");
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
      arango.PUT(`/_api/transaction/${resp.result.id}`, {});

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },
    
    testSoftShutdownWithAQLStreamingTrxAborted : function() {
      let coordinators = getServers('coordinator');
      assertTrue(coordinators.length > 0);
      let coordinator = coordinators[0];
      let instanceInfo = global.instanceInfo;

      // Create a streaming transaction:
      let data = { collections: {write: [cn]} };

      let resp = arango.POST("/_api/transaction/begin", data);
      console.warn("Produced transaction:", resp);

      let status = arango.GET("/_admin/shutdown");
      assertFalse(status.softShutdownOngoing);

      // Now use soft shutdown API to shut coordinator down:
      arango.DELETE("/_admin/shutdown?soft=true");
      status = arango.GET("/_admin/shutdown");
      assertTrue(status.softShutdownOngoing);
      assertEqual(1, status.AQLcursors);
      assertEqual(0, status.transactions);

      // It should fail to create a new trx:
      let respFailed = arango.POST("/_api/transaction/begin", data);
      assertTrue(respFailed.error);
      assertEqual(503, respFailed.code);

      // Now wait for some seconds:
      wait(10);

      // And abort the transaction:
      arango.DELETE(`/_api/transaction/${resp.result.id}`);

      // And now it should shut down in due course...
      waitForShutdown(coordinator, 30);
      restartInstance(coordinator);
    },
    
  };
}
jsunity.run(testSuite);
return jsunity.done();
