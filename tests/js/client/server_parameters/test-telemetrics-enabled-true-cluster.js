/*jshint globalstrict:false, strict:false */
/* global getOptions, fail, assertEqual, assertNotEqual, assertNull, assertNotNull, assertTrue, instanceManager, arango */

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
let {debugRemoveFailAt, debugSetFailAt} = require('@arangodb/test-helper');
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require("internal");
const isServer = typeof internal.arango === 'undefined';

const suspendExternal = internal.suspendExternal;
const continueExternal = require("internal").continueExternal;
const wait = require("internal").wait;

const intervalValue = 40;

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
      if (!isServer) {
        const originalEndpoint = arango.getEndpoint();

        const failurePoint = 'DisableTelemetricsSenderCoordinator';
        const colName = "_statistics";

        let prepareCoordId = null;
        let lastUpdateDocBefore = "";
        try {
          debugSetFailAt(originalEndpoint, failurePoint);
          do {
            wait(1);
            const telemetricsDoc = db[colName].document("telemetrics");
            assertNotEqual(telemetricsDoc, 'undefined');

            if (telemetricsDoc.hasOwnProperty("serverId")) {
              prepareCoordId = telemetricsDoc["serverId"];
              assertTrue(telemetricsDoc.hasOwnProperty("prepareTimestamp"));
            }
            assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
            lastUpdateDocBefore = telemetricsDoc["lastUpdate"];
          } while (prepareCoordId === null);
        } catch (err) {
          fail("Failed to run telemetrics with failure point " + err.errorMessage);
        } finally {
          debugRemoveFailAt(originalEndpoint, failurePoint);
        }
        const coordinators = instanceManager.arangods.filter(arangod => arangod.instanceRole === "coordinator");
        assertEqual(coordinators.length, 3);

        for (let i = 0; i < coordinators.length; ++i) {
          const coordinator = coordinators[i];
          if (coordinator.id === prepareCoordId) {
            if (coordinator.endpoint === originalEndpoint) {
              let newEndpoint = null;
              if (i === coordinators.length - 1) {
                newEndpoint = coordinators[i - 1].endpoint;
              } else {
                newEndpoint = coordinators[i + 1].endpoint;
              }
              arango.reconnect(newEndpoint, db._name(), "root", "");
            }
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
            } catch (err) {
              fail("Failed to stop server " + err.errorMessage);
            }
            let telemetricsDoc = "";
            do {
              wait(1);
              telemetricsDoc = db[colName].document("telemetrics");

              assertTrue(telemetricsDoc.hasOwnProperty("prepareTimestamp"));
              assertTrue(telemetricsDoc.hasOwnProperty("serverId"));
              assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
            } while (telemetricsDoc["serverId"] !== null);
            assertNull(telemetricsDoc["prepareTimestamp"]);
            const lastUpdateDocAfter = telemetricsDoc["lastUpdate"];
            assertTrue(lastUpdateDocAfter - lastUpdateDocBefore < intervalValue);

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

            break;
          }
        }
      }
    },
  };
}

if (internal.debugCanUseFailAt(arango.getEndpoint())) {
  jsunity.run(TelemetricsTestSuite);
}
return jsunity.done();
