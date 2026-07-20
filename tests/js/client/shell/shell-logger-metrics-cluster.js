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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
let { instanceRole } = require('@arangodb/testutils/instance');
let IM = global.instanceManager;

function loggerMetricsSuite() {
  'use strict';

  const metrics = [
    'arangodb_logger_warnings_total',
    'arangodb_logger_errors_total',
  ];
      
  return {
    
    testMetricsOnAgent: function () {
      let agents = IM.getInstancesRole(instanceRole.agent);
      assertTrue(agents.length > 0);

      agents.forEach((agent) => {
        metrics.forEach((m) => {
          // getMetric will throw if the metric is not present 
          // on the target host
          let value = agent.getMetric(m);
          assertEqual("number", typeof value);
        });
      });
    },

    testMetricsOnCoordinator: function () {
      let coordinators = IM.getInstancesRole(instanceRole.coordinator);
      assertTrue(coordinators.length > 0);

      coordinators.forEach((coordinator) => {
        metrics.forEach((m) => {
          // getMetric will throw if the metric is not present 
          // on the target host
          let value = coordinator.getMetric(m);
          assertEqual("number", typeof value);
        });
      });
    },
    
    testMetricsOnDBServer: function () {
      let dbservers = IM.getInstancesRole(instanceRole.dbserver);
      assertTrue(dbservers.length > 0);

      dbservers.forEach((dbserver) => {
        metrics.forEach((m) => {
          // getMetric will throw if the metric is not present 
          // on the target host
          let value = dbserver.getMetric(m);
          assertEqual("number", typeof value);
        });
      });
    },

  };
}

jsunity.run(loggerMetricsSuite);
return jsunity.done();
