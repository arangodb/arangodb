/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertFalse, assertEqual, fail, instanceInfo */

////////////////////////////////////////////////////////////////////////////////
/// @brief test synchronous replication in the cluster
///
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const fs = require('fs');
const internal = require('internal');

const wait = require("internal").wait;
const continueExternal = require("internal").continueExternal;

function getDBServers() {
  var tmp = global.ArangoClusterInfo.getDBServers();
  var servers = [];
  for (var i = 0; i < tmp.length; ++i) {
    servers[i] = tmp[i].serverId;
  }
  return servers;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function SynchronousReplicationSuite () {
  'use strict';
  const suite = internal.load(fs.join(internal.pathForTesting('server'), 'resilience', 'failover', 'resilience-synchronous-repl-cluster.inc'));

  return {
    setUp : suite.setUp,
    tearDown : suite.tearDown,

////////////////////////////////////////////////////////////////////////////////
/// @brief check inquiry functionality
////////////////////////////////////////////////////////////////////////////////
    testInquiry : function () {
      console.warn("Checking inquiry");
      var writeResult = global.ArangoAgency.write(
        [[{"a":1},{"a":{"oldEmpty":true}},"INTEGRATION_TEST_INQUIRY_ERROR_503"]]);
      console.log(
        "Inquired successfully a matched precondition under 503 response from write");
      assertTrue(typeof writeResult === "object");
      assertTrue(writeResult !== null);
      assertTrue("results" in writeResult);
      assertTrue(writeResult.results[0]>0);
      try {
        writeResult = global.ArangoAgency.write(
          [[{"a":1},{"a":0},"INTEGRATION_TEST_INQUIRY_ERROR_503"]]);
        fail();
      } catch (e1) {
        console.log(
          "Inquired successfully a failed precondition under 503 response from write");
      }
      writeResult = global.ArangoAgency.write(
        [[{"a":1},{"a":1},"INTEGRATION_TEST_INQUIRY_ERROR_0"]]);
      console.log(
        "Inquired successfully a matched precondition under 0 response from write");
      try {
        writeResult = global.ArangoAgency.write(
          [[{"a":1},{"a":0},"INTEGRATION_TEST_INQUIRY_ERROR_0"]]);
        fail();
      } catch (e1) {
        console.log(
          "Inquired successfully a failed precondition under 0 response from write");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check whether we have access to global.instanceManager
////////////////////////////////////////////////////////////////////////////////
    testCheckInstanceInfo : function () {
      assertTrue(global.instanceManager !== undefined);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief check if a synchronously replicated collection gets online
////////////////////////////////////////////////////////////////////////////////

    testSetup : function () {
      for (var count = 0; count < 120; ++count) {
        let dbservers = getDBServers();
        if (dbservers.length === 5) {
          assertTrue(suite.waitForSynchronousReplication("_system"));
          return;
        }
        console.log("Waiting for 5 dbservers to be present:", JSON.stringify(dbservers));
        wait(1.0);
      }
      assertTrue(false, "Timeout waiting for 5 dbservers.");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief run a standard check without failures:
////////////////////////////////////////////////////////////////////////////////

    testBasicOperations : function () {
      assertTrue(suite.waitForSynchronousReplication("_system"));
      suite.runBasicOperations({}, {});
    },

    testLargeTransactionsSplitting : function () {
      let docs = [];
      // We try to create a massive write transaction.
      // This one now is above 6MB
      for (let i = 0; i < 10000; ++i) {
        docs.push({"undderhund": "macht so wau wau wau!"});
      }
      let c = suite.getCollection();
      for (let i = 0; i < 5; ++i) {
        // We trigger 5 of these large transactions
        c.insert(docs);
      }
      let referenceCounter = c.count();
      assertTrue(suite.waitForSynchronousReplication("_system"));

      // Now we trigger failedFollower
      const failedPos = suite.failFollower();
      // We now continuously add more large transaction to trigger tailing
      for (let i = 0; i < 5; ++i) {
        // We trigger 5 more of these large transactions
        c.insert(docs);
      }
 
      // This should trigger a new follower to be added.
      // This follower needs to sync up with at least one splitted transaction
      // The collection will not get back into sync if this splitted transaction
      // fails. Also assertions will be triggered.
      // Wait for it:
      assertTrue(suite.waitForSynchronousReplication("_system"));

      // Heal follower
      assertTrue(continueExternal(global.instanceManager.arangods[failedPos].pid));
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SynchronousReplicationSuite);

return jsunity.done();

