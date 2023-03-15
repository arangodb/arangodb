/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, assertEqual, assertTrue, assertNotUndefined, arango */

// //////////////////////////////////////////////////////////////////////////////
// / @brief dropping followers while replicating
// /
// / DISCLAIMER
// /
// / Copyright 2023 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Julia Puget
// //////////////////////////////////////////////////////////////////////////////

const jwtSecret = 'abc';

if (getOptions === true) {
  return {
    'server.send-telemetrics': 'false',
    'server.authentication': 'true',
    'server.jwt-secret': jwtSecret,
  };
}

let jsunity = require('jsunity');
let internal = require('internal');

function getTelemetricsResult() {
  try {
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
  } catch (err) {
  }
}

function telemetricsOnShellTestsuite() {

  return {

    testTelemetricsShellRequestByUserNotEnabled: function () {
      try {
        arango.startTelemetrics();
        const res = getTelemetricsResult();
        assertTrue(res.hasOwnProperty("errorNum"));
        assertTrue(res.hasOwnProperty("errorMessage"));
        assertEqual(res.errorNum, internal.errors.ERROR_HTTP_FORBIDDEN.code);
        assertTrue(res.errorMessage.includes("telemetrics is disabled"));
      } catch (err) {
      }
    },

  };
}

function telemetricsApiUsageTestsuite() {

  return {

    testTelemetricsApiRequestByUserNotEnabled: function () {
      arango.startTelemetrics();
      const res = arango.GET("/_admin/telemetrics");
      assertEqual(res.errorNum, internal.errors.ERROR_HTTP_FORBIDDEN.code);
      assertTrue(res.errorMessage.includes("telemetrics is disabled"));
    },

  };
}

jsunity.run(telemetricsOnShellTestsuite);
jsunity.run(telemetricsApiUsageTestsuite);
return jsunity.done();
