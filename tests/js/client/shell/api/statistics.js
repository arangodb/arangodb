/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global arango, assertTrue, assertEqual, assertNotUndefined */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const jsunity = require('jsunity');

function metricsApiSuite() {
  return {

    test_metrics_returns_200: function () {
      const res = arango.GET_RAW('/_admin/metrics');
      assertEqual(res.code, 200, 'GET /_admin/metrics should return 200');
    },

    test_testing_statistics_wrong_cmd: function () {
      const cmd = "/_admin/statistics/asd123";
      const doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.error);
      assertEqual(doc.errorNum, internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_metrics_contains_expected_metric_names: function () {
      const res = arango.GET_RAW('/_admin/metrics');
      assertEqual(res.code, 200);
      const body = typeof res.body === 'string' ? res.body : String(res.body);
      const expected = [
        'arangodb_server_statistics_server_uptime_total',
        'arangodb_process_statistics_resident_set_size',
        'arangodb_process_statistics_number_of_threads'
      ];
      expected.forEach(function (name) {
        assertTrue(body.indexOf(name) !== -1, 'response should contain metric: ' + name);
      });
    },

    test_testing_async_requests_: function () {
      const ASYNC_METRIC = 'arangodb_http_request_statistics_async_requests_total';

      function getAsyncCount() {
        const res = arango.GET_RAW('/_admin/metrics');
        assertEqual(res.code, 200);
        const body = typeof res.body === 'string' ? res.body : String(res.body);
        const v = internal.parsePrometheusMetric(body, ASYNC_METRIC);
        return (typeof v === 'number' && !Number.isNaN(v)) ? v : 0;
      }

      let async_requests_1 = getAsyncCount();

      let doc = arango.PUT_RAW('/_api/version', '', { 'X-Arango-Async': 'true' });
      assertEqual(doc.code, 202);

      internal.sleep(1);

      let async_requests_2 = getAsyncCount();
      assertTrue(async_requests_2 >= async_requests_1 + 1,
        'async request should increase async counter: ' + async_requests_1 + ' -> ' + async_requests_2);

      doc = arango.PUT_RAW('/_api/version', '');
      assertEqual(doc.code, 200);

      internal.sleep(1);

      let async_requests_3 = getAsyncCount();
      assertEqual(async_requests_2, async_requests_3,
        'sync request should not increase async counter');
    },
  };
}

jsunity.run(metricsApiSuite);
return jsunity.done();