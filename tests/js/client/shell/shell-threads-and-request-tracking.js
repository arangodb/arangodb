/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertNull, ArangoClusterInfo */

////////////////////////////////////////////////////////////////////////////////
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const internal = require("internal");
const { getClusterMetricsByName, getMetricSingle, getCompleteMetricsValues } = require('@arangodb/test-helper');
const clusterInfo = global.ArangoClusterInfo;
const isCluster = require("internal").isCluster();

  
const cn = "UnitTestsCollection";
const n = 10000;

function TestSuite () {
  'use strict';
       
  return {
    setUp: function () {},

    tearDown: function () {

    },
    
    testMemoryUsageShouldIncrease: function () {

      let metrics;
      if (isCluster) {
        metrics = getClusterMetricsByName("arangodb_requests_memory_usage", "all");
        metrics.forEach(value => {
          assertTrue(value > 0);
        });
      } else {
        metrics = getMetricSingle("arangodb_requests_memory_usage");
        assertTrue(metrics > 0);
      }

      // let scheduler_length, scheduler_memory;
      // for (let i = 0; i < 5; i++) {
      //   [scheduler_length, scheduler_memory] = getCompleteMetricsValues(["arangodb_scheduler_queue_length", "arangodb_scheduler_queue_memory_usage"], "all");

      //   print(scheduler_length, scheduler_memory);
      // }

    },
  };
}

jsunity.run(TestSuite);

return jsunity.done();
