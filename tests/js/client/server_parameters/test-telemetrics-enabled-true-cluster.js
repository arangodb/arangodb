/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, AssertNull, assertNotNull, assertTrue, instanceManager, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
let {debugRemoveFailAt, debugSetFailAt, debugCanUseFailAt} = require('@arangodb/test-helper');
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require("internal");
const isServer = typeof internal.arango === 'undefined';

const suspendExternal = internal.suspendExternal;
const continueExternal = require("internal").continueExternal;
const wait = require("internal").wait;

const intervalValue = 30;

if (getOptions === true) {
  return {
    options: {coordinators: 3},
    'server.send-telemetrics': "true",
    'server.telemetrics-interval': intervalValue,
  };
}

function getServersHealth() {
  let result = arango.GET_RAW('/_admin/cluster/health');
  assertTrue(result.parsedBody.hasOwnProperty("Health"));
  return result.parsedBody.Health;
}


function TelemetricsTestSuite() {
  'use strict';

  return {
    testGetTelemetricsWithFailover: function() {
      const coordinatorEndpoint = arango.getEndpoint();
      if (!debugCanUseFailAt(coordinatorEndpoint)) {
        print("will return");
        return;
      }

      if (!isServer) {
        print("is coordinator");

        const arango = internal.arango;
        const coordinatorEndpoint = arango.getEndpoint();
        print("ENDPOINT " + coordinatorEndpoint);
        /*
        if (debugCanUseFailAt(coordinatorEndpoint)) {
          setFailAt = failurePoint => debugSetFailAt(coordinatorEndpoint, failurePoint);
          removeFailAt = failurePoint => debugRemoveFailAt(coordinatorEndpoint, failurePoint);
        }

         */
        wait(intervalValue);
        const failurePoint = 'DisableTelemetricsSenderCoordinator';
        let prepareCoordId = null;
        let lastUpdateDocBefore = "";
        const colName = "_statistics";

        try {
          debugSetFailAt(coordinatorEndpoint, failurePoint);
          /*
          print("AFTER");
          wait(intervalValue + 1);
          let res = null;
          print("prepareCoordId " + prepareCoordId === "null");
          do {

            res = db._query("FOR doc IN " + colName + " FILTER doc._key == 'telemetrics' RETURN doc").toArray();
            assertNotNull(res);
            assertNotEqual(res.length, 0);
            const result = res[0];
            print(result);
            const telemetricsDoc = db["_statistics"].document("telemetrics");


            print(telemetricsDoc);
            assertTrue(telemetricsDoc.hasOwnProperty("prepareTimestamp"));
            assertTrue(telemetricsDoc.hasOwnProperty("serverId"));
            assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
            prepareCoordId = telemetricsDoc["serverId"];
            print("id ", prepareCoordId);
            lastUpdateDocBefore = telemetricsDoc["lastUpdate"];
            wait(1);
          } while (prepareCoordId === "null");
*/
        } catch (err) {

        } finally {
          print("finally");
          debugRemoveFailAt(coordinatorEndpoint, failurePoint);
        }

      /*  const coordinators = instanceManager.arangods.filter(arangod => arangod.instanceRole === "coordinator");

        assertEqual(coordinators.length, 3);

        for (let i = 0; i < coordinators.length; ++i) {
          const coordinator = coordinators[i];
          if (coordinator.id == prepareCoordId) {

            print("will stop server");
            assertTrue(suspendExternal(coordinator.pid));
            try {
              let serverHealth = null;
              let startTime = Date.now();
              let result = null;
              do {
                wait(2);
                result = getServersHealth();
                assertNotNull(result);
                serverHealth = result[coordinator.id].Status;
                assertNotNull(serverHealth);
                const timeElapsed = (Date.now() - startTime) / 1000;
                assertTrue(timeElapsed < intervalValue, "Server expected status not acquired");
              } while (serverHealth !== "FAILED");
              coordinator.suspended = true;
              assertEqual(serverHealth, "FAILED");
            } finally {
              print("finally 2");
              const res = db._query("FOR doc IN " + colName + " FILTER doc._key == 'telemetrics' RETURN doc").toArray();
              assertNotEqual(res.length, 0);
              const result = res[0];
              print(result);
              const telemetricsDoc = db[colName].document("telemetrics");

              print(telemetricsDoc);
              assertTrue(telemetricsDoc.hasOwnProperty("prepareTimestamp"));
              assertTrue(telemetricsDoc.hasOwnProperty("serverId"));
              assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
              assertNull(telemetricsDoc["prepareTimestamp"]);
              assertNull(telemetricsDoc["serverId"]);
              const lastUpdateDocAfter = telemetricsDoc["lastUpdate"];
              assertFalse(lastUpdateDocAfter - lastUpdateDocBefore < intervalValue);



              assertTrue(continueExternal(coordinator.pid));
              let serverHealth = null;
              const startTime = Date.now();
              do {
                wait(2);
                const result = getServersHealth();
                assertNotNull(result);
                serverHealth = result[coordinator.id].Status;
                assertNotNull(serverHealth);
                const timeElapsed = (Date.now() - startTime) / 1000;
                assertTrue(timeElapsed < intervalValue, "Unable to get server " + coordinator.id + " in good state");
              } while (serverHealth !== "GOOD");
              coordinator.suspended = false;
            }
          }
          print("coordinator ", coordinators[i].id);
        }
        */


      } else {
        print("is dbserver");
      }

      // will have a failure point and recovery
    },
  };
}

jsunity.run(TelemetricsTestSuite);
return jsunity.done();
