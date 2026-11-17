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
let IM = global.instanceManager;

function H2RequestBodySizeLimitSuite() {
  'use strict';
  return {
    tearDown: function () {
      IM.debugClearFailAt();
    },

    testOversizedBodyStreamIsRejectedButConnectionSurvives: function () {
      if (arango.protocol() !== "http2" || !IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugSetFailAt("H2CommTask::lowerBodySizeLimit");

      // failure point lowers the limit to 16 bytes as we don't need the test to go up to 1GB at all
      const body = new Array(65).join("a");
      const res = arango.POST_RAW("/_admin/echo", body);
      assertTrue(res.error, `expected the oversized body to be rejected, got: ${JSON.stringify(res)}`);
      assertEqual(503, res.errorNum);

      // connection must still work after the one reset stream
      const res2 = arango.GET_RAW("/_admin/echo");
      assertEqual(200, res2.code);
    },

    testBodyAtExactLimitSucceeds: function () {
      if (arango.protocol() !== "http2" || !IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugSetFailAt("H2CommTask::lowerBodySizeLimit");

      const body = new Array(17).join("a"); // exactly 16 bytes
      const res = arango.POST_RAW("/_admin/echo", body);
      assertEqual(200, res.code);
    },

    testBodyOneByteOverLimitFails: function () {
      if (arango.protocol() !== "http2" || !IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugSetFailAt("H2CommTask::lowerBodySizeLimit");

      const body = new Array(18).join("a"); // 17 bytes, one over the limit
      const res = arango.POST_RAW("/_admin/echo", body);
      assertTrue(res.error, `expected a one-byte-over body to be rejected, got: ${JSON.stringify(res)}`);
      assertEqual(503, res.errorNum);
    },
  };
}

jsunity.run(H2RequestBodySizeLimitSuite);
return jsunity.done();
