/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertEqual, arango */

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
    'server.statistics': "true",
    'server.statistics-history': "false"
  };
}
const jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const internal = require('internal');
const db = internal.db;

function getMetric(name) {
  let res = arango.GET_RAW("/_admin/metrics");
  let re = new RegExp("^" + name + " ");
  let matches = res.body.split('\n').filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
  if (!matches.length) {
    throw "Metric " + name + " not found";
  }
  return Number(matches[0].replace(/^.*? (\d+.*?)$/, '$1'));
}

function testSuite() {
  return {
    testStatisticApi : function() {
      let value = arango.GET("/_admin/statistics");
      assertTrue(value.hasOwnProperty("time"));
      assertTrue(value.hasOwnProperty("enabled"));
      assertTrue(value.hasOwnProperty("server"));
      assertTrue(value.hasOwnProperty("system"));
      assertTrue(value.enabled);
    },

    testMetricsAlwaysThere : function() {
      let value = getMetric("arangodb_process_statistics_resident_set_size");
      assertTrue(value > 0, value);
      
      value = getMetric("arangodb_server_statistics_server_uptime");
      assertTrue(value > 0, value);
    },
    
    testHttpMetrics : function() {
      let oldValue = getMetric("arangodb_http_request_statistics_total_requests");
      for (let i = 0; i < 10; ++i) {
        arango.GET("/_api/version");
      }
      // statistics aggregation on server may take a short while - wait for it
      let newValue;
      let tries = 0;
      while (++tries < 4 * 10) {
        newValue = getMetric("arangodb_http_request_statistics_total_requests");
        if (newValue - oldValue >= 10) {
          break;
        }
        internal.sleep(0.25);
      }
      assertTrue(newValue - oldValue >= 10, { oldValue, newValue });
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

  };
}

jsunity.run(testSuite);
return jsunity.done();
