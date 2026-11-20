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

// must match GeneralServerFeature::maxGlobalBodySize under the failure point,
// more than one HTTP/2 DATA frame (16 KiB)
const FAILPOINT_LIMIT_BYTES = 32768;

function metric(name) {
  const res = arango.GET_RAW("/_admin/metrics");
  assertEqual(200, res.code);
  const value = internal.parsePrometheusMetric(String(res.body), name);
  assertTrue(value !== undefined, `metric ${name} not found`);
  return value;
}

function bodyBytes() {
  return metric("arangodb_http_global_body_bytes");
}

function rejections() {
  return metric("arangodb_http_global_body_rejections_total");
}

function canRun() {
  const protocol = arango.protocol();
  return (protocol === "http" || protocol === "http2") && IM.debugCanUseFailAt();
}

function body(size) {
  return new Array(size + 1).join("a");
}

// any response from the handler means the body got through, GET only
function assertAccepted(res) {
  assertEqual(405, res.code, JSON.stringify(res));
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

    setUp: function () {
      if (canRun()) {
        IM.debugSetFailAt("GeneralServerFeature::lowerGlobalBodySizeLimit");
        // HTTP/1.1 bodies then arrive in several parser calls
        IM.debugSetFailAt("HttpCommTask<T>::readCallback_in_small_chunks");
      }
    },

    tearDown: function () {
      IM.debugClearFailAt();
    },

    testBodyAtExactLimitIsAcceptedAndReleased: function () {
      if (!canRun()) {
        return;
      }
      const before = bodyBytes();
      const rejectedBefore = rejections();

      assertAccepted(arango.POST_RAW("/_api/version", body(FAILPOINT_LIMIT_BYTES)));

      assertEqual(before, bodyBytes());
      assertEqual(rejectedBefore, rejections());
    },

    testBodyOverLimitIsRejectedAndReleased: function () {
      if (!canRun()) {
        return;
      }
      const before = bodyBytes();
      const rejectedBefore = rejections();

      // HTTP/2 resets after two full frames were reserved, HTTP/1.1 answers
      // 413 before reading the body, but a TCP reset can beat that response
      const res = arango.POST_RAW("/_api/version", body(FAILPOINT_LIMIT_BYTES + 1));
      assertTrue(res.error, JSON.stringify(res));

      assertEqual(rejectedBefore + 1, rejections());
      assertEqual(before, bodyBytes());
    },

    testServerStillServesAfterRejection: function () {
      if (!canRun()) {
        return;
      }
      arango.POST_RAW("/_api/version", body(FAILPOINT_LIMIT_BYTES * 2));

      // HTTP/1.1 reconnects here
      assertEqual(200, arango.GET_RAW("/_api/version").code);
      assertAccepted(arango.POST_RAW("/_api/version", body(16)));
    },
  };
}

jsunity.run(GlobalRequestBodySizeLimitSuite);
return jsunity.done();
