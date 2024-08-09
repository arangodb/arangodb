/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
const request = require('@arangodb/request');
const getMetric = require('@arangodb/test-helper').getMetric;

function getEndpointsByType(type) {
  const isType = (d) => (d.instanceRole === type);
  const toEndpoint = (d) => (d.endpoint);
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    let pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const IM = global.instanceManager;
  return IM.arangods.filter(isType)
    .map(toEndpoint)
    .map(endpointToURL);
}

function processMetricsSuite() {
  'use strict';
      
  const metrics = [
    "arangodb_process_statistics_user_time",
    "arangodb_process_statistics_system_time",
    "arangodb_process_statistics_number_of_threads",
    "arangodb_process_statistics_resident_set_size",
    "arangodb_process_statistics_virtual_memory_size",
  ];

  return {
    
    testMetricsOnAgent: function () {
      let endpoints = getEndpointsByType('agent');
      assertTrue(endpoints.length > 0);

      endpoints.forEach((ep) => {
        metrics.forEach((m) => {
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
          let value = getMetric(ep, m);
          assertEqual("number", typeof value);
        });
      });
    },

  };
}

jsunity.run(processMetricsSuite);
return jsunity.done();
