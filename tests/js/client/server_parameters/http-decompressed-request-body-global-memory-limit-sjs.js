/*jshint globalstrict:false, strict:false */
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

if (getOptions === true) {
  return {
    'http.max-decompressed-request-body-size': String(768 * 1024),
    'http.decompressed-request-body-global-memory-limit': String(1024 * 1024),
  };
}
const jsunity = require('jsunity');
const { assertEqual, assertTrue } = jsunity.jsUnity.assertions;
const internal = require('internal');

// bodies are deflate-compressed by arangosh (compress-request-threshold = 10)
const padding = 'x'.repeat(600 * 1024);

// keeps failure messages small: echo responses contain the whole body
const summary = (res) => JSON.stringify({
  code: res.code,
  errorNum: res.parsedBody && res.parsedBody.errorNum,
  errorMessage: res.parsedBody && res.parsedBody.errorMessage,
});

function DecompressedRequestBodyGlobalMemoryLimitSuite() {
  let compressTransfer;

  // keeps ~600 KiB reserved until the query finishes
  const startSlowRequest = () => {
    const res = arango.POST_RAW('/_api/cursor',
      { query: 'LET p = @pad RETURN SLEEP(5)', bindVars: { pad: padding } },
      { 'x-arango-async': 'store' });
    assertEqual(202, res.code, summary(res));
    return res.headers['x-arango-async-id'];
  };

  const waitForJob = (id) => {
    for (let i = 0; i < 300; ++i) {
      const res = arango.PUT_RAW(`/_api/job/${id}`, {});
      if (res.code !== 204) {  // 204: job still running
        return res;
      }
      internal.sleep(0.1);
    }
    assertTrue(false, 'job did not finish');
  };

  return {
    setUpAll: function () {
      compressTransfer = arango.compressTransfer();
      arango.compressTransfer(true);
    },
    tearDownAll: function () {
      arango.compressTransfer(compressTransfer);
    },

    testBodyWithinBudgetStillWorks: function () {
      const res = arango.POST_RAW('/_admin/echo', padding);
      assertEqual(200, res.code, summary(res));
      assertEqual(padding.length, res.parsedBody.requestBody.length);
    },

    testConcurrentBodiesOverBudgetGet503: function () {
      const id = startSlowRequest();
      // 600 KiB held + 600 KiB more > 1 MiB budget
      const res = arango.POST_RAW('/_admin/echo', padding);
      assertEqual(503, res.code, summary(res));
      assertEqual(internal.errors.ERROR_OUT_OF_MEMORY.code, res.parsedBody.errorNum);

      const job = waitForJob(id);
      assertEqual(201, job.code, summary(job));
      // the finished request released its share
      const after = arango.POST_RAW('/_admin/echo', padding);
      assertEqual(200, after.code, summary(after));
    },
  };
}

jsunity.run(DecompressedRequestBodyGlobalMemoryLimitSuite);
return jsunity.done();
