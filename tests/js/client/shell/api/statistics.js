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

    test_metrics_contains_uptime: function () {
      const res = arango.GET_RAW('/_admin/metrics');
      assertEqual(res.code, 200);
      const body = typeof res.body === 'string' ? res.body : String(res.body);
      assertTrue(body.indexOf('arangodb_server_statistics_server_uptime_total') !== -1,
        'response should contain arangodb_server_statistics_server_uptime_total');
      const uptime = internal.parsePrometheusMetric(body, 'arangodb_server_statistics_server_uptime_total');
      assertNotUndefined(uptime);
      assertTrue(uptime >= 0, 'uptime should be non-negative');
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
  };
}

jsunity.run(metricsApiSuite);
return jsunity.done();