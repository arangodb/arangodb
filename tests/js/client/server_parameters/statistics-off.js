/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertEqual, arango, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server startup options
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.statistics': "false"
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const internal = require('internal');
const getMetric = require('@arangodb/test-helper').getMetricSingle;
const db = internal.db;

function testSuite() {
  return {
    testStatisticApi : function() {
      let value = arango.GET("/_admin/statistics");
      assertTrue(value.error);
      assertEqual(404, value.code);
    },

    testMetricsAlwaysThere : function() {
      let value = getMetric("arangodb_process_statistics_resident_set_size");
      assertTrue(value > 0, value);
      
      value = getMetric("arangodb_server_statistics_server_uptime_total");
      assertTrue(value > 0, value);
    },

    testHttpMetrics : function() {
      try {
        getMetric("arangodb_http_request_statistics_total_requests_total");
        fail();
      } catch (err) {
        assertEqual("Metric arangodb_http_request_statistics_total_requests_total not found", err);
      }
    },
    
    testStatisticsHistory : function() {
      let count;
      let tries = 0;
      // wait until some document has been written into statistics collection
      while (++tries < 4 * 10) {
        count = db._statisticsRaw.count();
        if (count > 0) {
          break;
        }
        internal.sleep(0.25);
      }
      assertEqual(0, count);
    },

    testMemoryUsage : function() {
      // issue some random requests to the server
      for (let i = 0; i < 10; ++i) {
         arango.GET_RAW("/_admin/metrics");
      }
      // metric values should always be 0 if statistics are disabled
      assertEqual(0, getMetric("arangodb_connection_statistics_memory_usage"));
      assertEqual(0, getMetric("arangodb_request_statistics_memory_usage"));
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
