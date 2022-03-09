/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const request = require('@arangodb/request');
const { getMetric, getEndpointsByType } = require('@arangodb/test-helper');

function loggerMetricsSuite() {
  'use strict';

  const metrics = [
    'arangodb_logger_warnings_total',
    'arangodb_logger_errors_total',
  ];
      
  return {
    
    testMetricsOnAgent: function () {
      let endpoints = getEndpointsByType('agent');
      assertTrue(endpoints.length > 0);

      endpoints.forEach((ep) => {
        metrics.forEach((m) => {
          // getMetric will throw if the metric is not present 
          // on the target host
          let value = getMetric(ep, m);
          assertEqual("number", typeof value);
        });
      });
    },

    testMetricsOnCoordinator: function () {
      let endpoints = getEndpointsByType('coordinator');
      assertTrue(endpoints.length > 0);

      endpoints.forEach((ep) => {
        metrics.forEach((m) => {
          // getMetric will throw if the metric is not present 
          // on the target host
          let value = getMetric(ep, m);
          assertEqual("number", typeof value);
        });
      });
    },
    
    testMetricsOnDBServer: function () {
      let endpoints = getEndpointsByType('dbserver');
      assertTrue(endpoints.length > 0);

      endpoints.forEach((ep) => {
        metrics.forEach((m) => {
          // getMetric will throw if the metric is not present 
          // on the target host
          let value = getMetric(ep, m);
          assertEqual("number", typeof value);
        });
      });
    },

  };
}

jsunity.run(loggerMetricsSuite);
return jsunity.done();
