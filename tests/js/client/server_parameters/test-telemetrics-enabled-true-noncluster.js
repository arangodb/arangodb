/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNull, assertTrue */

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
const db = arangodb.db;

const wait = require("internal").wait;

const intervalValue = 6;

if (getOptions === true) {

  return {
    'server.send-telemetrics': "true",
    'server.telemetrics-interval': intervalValue,
  };
}


function TelemetricsTestSuite() {
  'use strict';

  return {

    testGetTelemetrics: function() {

      const colName = "_statistics";

      let prepareCoordId = null;
      let lastUpdateDocBefore = null;
      let lastUpdateDocAfter = "";
      let telemetricsDoc = null;
      do {
        wait(1);
        telemetricsDoc = db[colName].document("telemetrics");
        if (telemetricsDoc !== 'undefined') {
          assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
          lastUpdateDocBefore = telemetricsDoc["lastUpdate"];
        }
      } while (lastUpdateDocBefore === null);

      do {
        wait(1);
        telemetricsDoc = db[colName].document("telemetrics");
        assertTrue(telemetricsDoc.hasOwnProperty("lastUpdate"));
        lastUpdateDocAfter = telemetricsDoc["lastUpdate"];
      } while (lastUpdateDocAfter === lastUpdateDocBefore) ;
      assertEqual(lastUpdateDocAfter - lastUpdateDocBefore, intervalValue);
    },
  };
}

jsunity.run(TelemetricsTestSuite);

return jsunity.done();
