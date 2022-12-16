/*jshint globalstrict:false, strict:false */
/* global getOptions, fail, assertEqual, assertNotEqual, assertNotNull, assertTrue, instanceManager, arango */

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

const intervalValue = 15;
const numCoords = 3;

if (getOptions === true) {

  return {
    options: {coordinators: numCoords},
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
      let numAttempts = 6;
      while (numAttempts > 0) {
        let tryAgain = false;
        numAttempts--;
        if (!isServer) {
          const originalEndpoint = arango.getEndpoint();

          const failurePoint1 = 'DisableTelemetricsSenderCoordinator';
          const failurePoint2 = '"DecreaseCoordinatorRecoveryTime"';
          const colName = "_statistics";

          let prepareCoordId = null;
          let lastUpdateDocBefore = "";
          assertTrue(debugSetFailAt(originalEndpoint, failurePoint1), "Failed to run telemetrics with failure point " + failurePoint1);
          try {

            let startTime = Date.now();
            do {
              const telemetricsDoc = db[colName].document("telemetrics");
              assertNotEqual(telemetricsDoc, 'undefined');
              const timeElapsed = (Date.now() - startTime) / 1000;
              assertTrue(timeElapsed < 60, "Unable to get document update within time limit");
              if (telemetricsDoc.hasOwnProperty("serverId")) {
                prepareCoordId = telemetricsDoc["serverId"];
                assertTrue(telemetricsDoc.hasOwnProperty("prepareTimestamp"));
                assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
                lastUpdateDocBefore = telemetricsDoc["lastUpdate"];
              } else {
                wait(1);
              }
            } while (prepareCoordId === null);
          } finally {
            debugRemoveFailAt(originalEndpoint, failurePoint1);
          }
          const coordinators = instanceManager.arangods.filter(arangod => arangod.instanceRole === "coordinator");
          assertEqual(coordinators.length, numCoords);
          let tryAgain = false;
          internal.debugSetFailAt(failurePoint2); // failure point for all coordinators
          try {
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
                  if (arango.getEndpoint() !== newEndpoint) {
                    if (numAttempts === 0) {
                      fail("Unable to connect to coordinator after 6 attempts");
                    } else {
                      tryAgain = true;
                      break;
                    }
                  }
                }
                if (!suspendExternal(coordinator.pid)) {
                  if (numAttempts === 0) {
                    fail("Unable to command server suspension after 6 attempts");
                  } else {
                    tryAgain = true;
                    break;
                  }
                }
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
                  if (timeElapsed >= intervalValue) {
                    if (numAttempts === 0) {
                      fail("Unable to stop server within time limit necessary for the test to be valuable");
                    } else {
                      tryAgain = true;
                      break;
                    }
                  }
                } while (serverHealth !== "FAILED");
                if (tryAgain) {
                  break;
                }
                coordinator.suspended = true;
                assertEqual(serverHealth, "FAILED");

                let telemetricsDoc = "";
                serverHealth = null;
                startTime = Date.now();
                do {
                  telemetricsDoc = db[colName].document("telemetrics");
                  const timeElapsed = (Date.now() - startTime) / 1000;
                  if (timeElapsed >= intervalValue) {
                    if (numAttempts === 0) {
                      fail("Unable to get document update within interval after 6 attempts, making test invaluable");
                    } else {
                      tryAgain = true;
                      break;
                    }
                  }
                  wait(1);
                } while (telemetricsDoc["serverId"] !== null);
                assertTrue(telemetricsDoc.hasOwnProperty("prepareTimestamp"));
                assertTrue(telemetricsDoc.hasOwnProperty("serverId"));
                assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
                if (telemetricsDoc["prepareTimestamp"] !== null) {
                  if (numAttempts === 0) {
                    fail("Failed to get full document update after 6 attempts, making test invaluable");
                  } else {
                    tryAgain = true;
                    break;
                  }
                }
                const lastUpdateDocAfter = telemetricsDoc["lastUpdate"];
                if (lastUpdateDocAfter - lastUpdateDocBefore >= intervalValue) {
                  if (numAttempts === 0) {
                    fail("Failed to read document exactly after the coordinator covers up for the coordinator that went down after 6 attempts, making test invaluable");
                  } else {
                    tryAgain = true;
                    break;
                  }
                } else {
                  numAttempts = 0; // means we were able to read the document exactly after the coordinator covers up for the coordinator that went down and tested what was necessary
                }

                assertTrue(continueExternal(coordinator.pid), "Unable to command server continuation");

                serverHealth = null;
                startTime = Date.now();
                do {
                  wait(2);
                  const result = getServersHealth();
                  assertNotNull(result);
                  serverHealth = result[coordinator.id].Status;
                  assertNotNull(serverHealth);
                  const timeElapsed = (Date.now() - startTime) / 1000;
                  assertTrue(timeElapsed < 60, "Unable to get server " + coordinator.id + " in good state within time limit");
                } while (serverHealth !== "GOOD");
                coordinator.suspended = false;

                break;
              }
            }
          } finally {
            internal.debugClearFailAt(failurePoint2);
          }
        }
        if (tryAgain) {
          continue;
        }
      }
    },
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(TelemetricsTestSuite);
}
return jsunity.done();
