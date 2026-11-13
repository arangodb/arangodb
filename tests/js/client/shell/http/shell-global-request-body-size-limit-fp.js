/*jshint globalstrict:false, strict:false */
/*global arango, assertTrue, assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
let IM = global.instanceManager;

// must match the hardcoded limit in GeneralServerFeature::lowerGlobalBodySizeLimit
const FAILPOINT_LIMIT_BYTES = 16;

function globalBodyBytes() {
  const res = arango.GET_RAW("/_admin/metrics");
  assertEqual(200, res.code);
  const body = typeof res.body === 'string' ? res.body : String(res.body);
  return internal.parsePrometheusMetric(body, "arangodb_http_global_body_bytes") || 0;
}

function canRun() {
  const protocol = arango.protocol();
  return (protocol === "http" || protocol === "http2") && IM.debugCanUseFailAt();
}

// h2 resets the stream, http1 sends a real 503
function assertRejected(res) {
  assertTrue(res.error, `expected the body to be rejected, got: ${JSON.stringify(res)}`);
  if (arango.protocol() === "http2") {
    assertEqual(503, res.errorNum);
  } else {
    assertEqual(503, res.code);
  }
}

function GlobalRequestBodySizeLimitSuite() {
  'use strict';
  let compressTransfer;
  return {
    setUpAll: function () {
      compressTransfer = arango.compressTransfer();
      arango.compressTransfer(false);
    },

    tearDownAll: function () {
      arango.compressTransfer(compressTransfer);
    },

    tearDown: function () {
      IM.debugClearFailAt();
    },

    testOversizedBodyExceedsGlobalLimitButServerStillServes: function () {
      if (!canRun()) {
        return;
      }
      IM.debugSetFailAt("GeneralServerFeature::lowerGlobalBodySizeLimit");
      const before = globalBodyBytes();

      const body = new Array(FAILPOINT_LIMIT_BYTES * 4 + 1).join("a");
      const res = arango.POST_RAW("/_admin/auth/reload", body);
      assertRejected(res);

      // http1 reconnects here
      const res2 = arango.GET_RAW("/_admin/auth/reload");
      assertEqual(200, res2.code);

      // a rejected request must not leak its reservation
      assertEqual(before, globalBodyBytes());
    },

    testBodyAtExactGlobalLimitSucceeds: function () {
      if (!canRun()) {
        return;
      }
      IM.debugSetFailAt("GeneralServerFeature::lowerGlobalBodySizeLimit");
      const before = globalBodyBytes();

      const body = new Array(FAILPOINT_LIMIT_BYTES + 1).join("a");
      const res = arango.POST_RAW("/_admin/auth/reload", body);
      assertEqual(200, res.code);

      // a completed request must release its reservation
      assertEqual(before, globalBodyBytes());
    },

    testBodyOneByteOverGlobalLimitFails: function () {
      if (!canRun()) {
        return;
      }
      IM.debugSetFailAt("GeneralServerFeature::lowerGlobalBodySizeLimit");
      const before = globalBodyBytes();

      const body = new Array(FAILPOINT_LIMIT_BYTES + 2).join("a");
      const res = arango.POST_RAW("/_admin/auth/reload", body);
      assertRejected(res);

      assertEqual(before, globalBodyBytes());
    },
  };
}

jsunity.run(GlobalRequestBodySizeLimitSuite);
return jsunity.done();
