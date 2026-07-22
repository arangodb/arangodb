/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertEqual, assertNotEqual, arango */

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
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {};
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const internal = require('internal');
const request = require("@arangodb/request");
const db = internal.db;
let IM = global.instanceManager;

const HTTP_RESPONSE_CODE_METRIC = "arangodb_http_response_code_total";

function getHttpResponseCodeMetric(code) {
  let res = arango.GET_RAW("/_admin/metrics/v2");
  assertEqual(200, res.code);
  let pattern = new RegExp(
    "^" + HTTP_RESPONSE_CODE_METRIC + '\\{[^}]*code="' + code + '"[^}]*\\}\\s+([0-9.]+)');
  let lines = String(res.body).split("\n");
  for (let line of lines) {
    let match = line.match(pattern);
    if (match) {
      return Number(match[1]);
    }
  }
  return 0;
}

function testSuite() {
  return {
    testMetricsApiWhenStatisticsOn: function () {
      const res = arango.GET_RAW("/_admin/metrics");
      assertEqual(200, res.code, "GET /_admin/metrics should return 200");
      const body = typeof res.body === 'string' ? res.body : String(res.body);

      assertTrue(body.indexOf('arangodb_server_statistics_server_uptime_total') !== -1,
        "Response should contain uptime");
      const uptime = internal.parsePrometheusMetric(body, 'arangodb_server_statistics_server_uptime_total');
      assertTrue(typeof uptime === 'number' && !Number.isNaN(uptime) && uptime >= 0,
        "uptime should be present and non-negative");

      assertTrue(uptime > 0, "Statistics should be uptime > 0");

      assertTrue(body.indexOf('arangodb_server_statistics_server_uptime_total') !== -1,
        "Response should contain server statistics");
      assertTrue(uptime !== undefined && uptime >= 0, "server-equivalent: server uptime should be valid");

      assertTrue(body.indexOf('arangodb_process_statistics_resident_set_size') !== -1,
        "Response should contain process/system statistics");
      const residentSetSize = internal.parsePrometheusMetric(body, 'arangodb_process_statistics_resident_set_size');
      assertTrue(residentSetSize !== undefined && residentSetSize >= 0,
        "At least one process statistic should be present and valid");

      assertTrue(body.indexOf('arangodb_http_request_statistics_total_requests_total') !== -1,
        "Response should contain http request statistics");
    },

    testMetricsAlwaysThere: function () {
      let value = IM.getMetric("arangodb_process_statistics_resident_set_size");
      assertTrue(value > 0, value);

      value = IM.getMetric("arangodb_server_statistics_server_uptime_total");
      assertTrue(value > 0, value);
    },

    testHttpMetrics: function () {
      let oldValue = IM.getMetric("arangodb_http_request_statistics_total_requests_total");
      for (let i = 0; i < 10; ++i) {
        arango.GET("/_api/version");
      }
      let newValue = IM.getMetric("arangodb_http_request_statistics_total_requests_total");
      assertTrue(newValue - oldValue >= 10, { oldValue, newValue });
    },

    testHttpResponseCodeMetrics: function () {
      let old200 = getHttpResponseCodeMetric("200");
      for (let i = 0; i < 5; ++i) {
        let res = request.get(global.instanceManager.url + "/_api/version");
        assertEqual(200, res.statusCode);
      }

      let new200;
      let tries = 0;
      while (++tries < 4 * 10) {
        new200 = getHttpResponseCodeMetric("200");
        if (new200 - old200 >= 5) {
          break;
        }
        internal.sleep(0.25);
      }
      assertTrue(new200 - old200 >= 5, { old200, new200 });

      let old404 = getHttpResponseCodeMetric("404");
      for (let i = 0; i < 3; ++i) {
        let res = request.get(global.instanceManager.url +
          "/_api/nonexistent_route_for_http_response_code_metric_test");
        assertEqual(404, res.statusCode);
      }

      let new404;
      tries = 0;
      while (++tries < 4 * 10) {
        new404 = getHttpResponseCodeMetric("404");
        if (new404 - old404 >= 3) {
          break;
        }
        internal.sleep(0.25);
      }
      assertTrue(new404 - old404 >= 3, { old404, new404 });
    },

    testMemoryUsageMetrics: function () {
      // metric values should never be 0 if statistics are enabled
      const connectionsBefore = IM.getMetric("arangodb_connection_statistics_memory_usage");
      assertNotEqual(0, connectionsBefore);
      const requestsBefore = IM.getMetric("arangodb_request_statistics_memory_usage");
      assertNotEqual(0, requestsBefore);

      // issue some random requests to the server
      for (let i = 0; i < 10; ++i) {
        arango.GET_RAW("/_admin/metrics");
      }

      // metrics values shouldn't have changed, because the statistics memory
      // is allocated at startup and shouldn't grow under normal circumstances
      assertEqual(connectionsBefore, IM.getMetric("arangodb_connection_statistics_memory_usage"));
      assertEqual(requestsBefore, IM.getMetric("arangodb_request_statistics_memory_usage"));
    }

  };
}

jsunity.run(testSuite);
return jsunity.done();
