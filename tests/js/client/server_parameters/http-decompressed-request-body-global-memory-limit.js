/* global getOptions, arango */

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

'use strict';
if (getOptions === true) {
  return {
    // 1 MiB: small enough to exceed with one request, large enough for the
    // framework's own requests
    'http.decompressed-request-body-global-memory-limit': '1048576',
  };
}
const jsunity = require('jsunity');
const {assertEqual} = jsunity.jsUnity.assertions;
const internal = require('internal');

function DecompressedRequestBodyGlobalMemoryLimitSuite() {
  let compressTransfer;
  return {
    setUpAll: function() {
      // arangosh sends bodies >= 10 bytes deflate-compressed in tests
      compressTransfer = arango.compressTransfer();
      arango.compressTransfer(true);
    },
    tearDownAll: function() {
      arango.compressTransfer(compressTransfer);
    },

    testBodyOverBudgetGets503: function() {
      const res = arango.POST_RAW('/_admin/echo', 'x'.repeat(4 * 1024 * 1024));
      assertEqual(503, res.code, res);
      assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, res.parsedBody.errorNum);
    },

    testBodyWithinBudgetStillWorks: function() {
      const body = 'x'.repeat(256 * 1024);
      const res = arango.POST_RAW('/_admin/echo', body);
      assertEqual(200, res.code, res);
      assertEqual(body, res.parsedBody.requestBody);
    },

    testBudgetIsReleasedAfterRejection: function() {
      // repeated rejections must not use up the budget
      for (let i = 0; i < 5; ++i) {
        assertEqual(503, arango.POST_RAW('/_admin/echo', 'x'.repeat(4 * 1024 * 1024)).code);
      }
      assertEqual(200, arango.POST_RAW('/_admin/echo', 'x'.repeat(256 * 1024)).code);
    },
  };
}

jsunity.run(DecompressedRequestBodyGlobalMemoryLimitSuite);
return jsunity.done();
