/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global getOptions, fail, assertEqual, assertTrue, assertFalse, assertNotNull, assertNotUndefined, arango */

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

if (getOptions === true) {
  return {
    'server.send-telemetrics': 'false',
    'server.authentication': 'true',
  };
}

let jsunity = require('jsunity');
let internal = require('internal');
const {HTTP_FORBIDDEN} = require("../../../../js/server/modules/@arangodb/actions");

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

function telemetricsOnShellReconnectTestsuite() {
  'use strict';

  return {

    testTelemetricsRequestByUserNotEnabled: function () {
      try {
        const res = getTelemetricsResult();
        assertTrue(res.hasOwnProperty("errorNum"));
        assertTrue(res.hasOwnProperty("errorMessage"));
        assertEqual(res.errorNum, HTTP_FORBIDDEN);
        assertTrue(res.errorMessage.includes("telemetrics is disabled"));
      } catch (err) {
      }
    },

  };
}

jsunity.run(telemetricsOnShellReconnectTestsuite);
return jsunity.done();
