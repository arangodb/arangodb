/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, arango */

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
const arangodb = require("@arangodb");
const internal = require("internal");
let {debugRemoveFailAt, debugSetFailAt} = require('@arangodb/test-helper');
const db = arangodb.db;

const wait = require("internal").wait;

//const intervalValue = 6;

const failurePoint1 = 'DisableTelemetricsSender';
const failurePoint2 = 'DecreaseTelemetricsInterval';

if (getOptions === true) {

  return {

    'server.send-telemetrics': "true",
    'server.failure-point': failurePoint2,
    //   'server.telemetrics-interval': intervalValue,
  };


}


function TelemetricsTestSuite() {
  'use strict';

  return {

    testGetTelemetrics: function() {

      const colName = "_statistics";

      let lastUpdateDocBefore = null;
      let lastUpdateDocAfter = "";
      let telemetricsDoc = null;

      const originalEndpoint = arango.getEndpoint();

      assertTrue(debugSetFailAt(originalEndpoint, failurePoint1), "Failed to run telemetrics with failure point " + failurePoint1);
      //  assertTrue(debugSetFailAt(originalEndpoint, failurePoint2), "Failed to run telemetrics with failure point " + failurePoint2);
      try {
        let startTime = Date.now();
        do {
          telemetricsDoc = db[colName].document("telemetrics");
          const timeElapsed = (Date.now() - startTime) / 1000;
          assertTrue(timeElapsed < 60, "Unable to get document update within time limit");
          if (telemetricsDoc !== 'undefined') {
            assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
            lastUpdateDocBefore = telemetricsDoc["lastUpdate"];
          } else {
            wait(1);
          }
        } while (lastUpdateDocBefore === null);
        startTime = Date.now();
        do {
          wait(1);
          const timeElapsed = (Date.now() - startTime) / 1000;
          assertTrue(timeElapsed < 60, "Unable to get document update past interval within time limit");
          telemetricsDoc = db[colName].document("telemetrics");
          assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
          lastUpdateDocAfter = telemetricsDoc["lastUpdate"];
        } while (lastUpdateDocAfter === lastUpdateDocBefore) ; //until we have an update
      } finally {
        debugRemoveFailAt(originalEndpoint, failurePoint1);
        debugRemoveFailAt(originalEndpoint, failurePoint2);
      }
    },
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(TelemetricsTestSuite);
}
return jsunity.done();
