/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, assertFalse, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const db = arangodb.db;
const { getMetric, getCoordinators } = require('@arangodb/test-helper');

function ClusterInfoMemoryTrackingSuite() {
  'use strict';
  const cn = 'UnitTestsCollection';
  const m = 'arangodb_internal_cluster_info_memory_usage';

  const coords = getCoordinators();
  assertNotEqual(0, coords.length, coords);
  const ep = coords[0].endpoint;

  return {
    tearDown: function () {
      db._collections().filter((c) => c.name().startsWith(cn)).forEach((c) => {
        c.drop();
      });
    },
    
    testCreateCollections: function() {
      const memoryUsageBefore = getMetric(ep, m);

      const n = 30;
      for (let i = 0; i < n; ++i) {
        db._create(cn + i, { numberOfShards: 1, replicationFactor: 2 });
      }
      
      const memoryUsageAfter = getMetric(ep, m);

      // expect at least 100 bytes for each additional collection. the
      // exact number is unknown and depends on a lot of factors (e.g. STL
      // implementation details, internal state of the containers inside
      // ClusterInfo etc.) - just assume here that it grows very modestly
      const sizePerCollection = 50;
      assertTrue(memoryUsageAfter > memoryUsageBefore + n * sizePerCollection, { memoryUsageBefore, memoryUsageAfter });
    },
    
    testCreateShards: function() {
      const memoryUsageBefore = getMetric(ep, m);

      db._create(cn, { numberOfShards: 100, replicationFactor: 2 });
      
      const memoryUsageAfter = getMetric(ep, m);

      // expect at least 100 bytes for each additional shard. the
      // exact number is unknown and depends on a lot of factors (e.g. STL
      // implementation details, internal state of the containers inside
      // ClusterInfo etc.) - just assume here that it grows very modestly
      const sizePerShard = 50;
      assertTrue(memoryUsageAfter > memoryUsageBefore + 100 * sizePerShard, { memoryUsageBefore, memoryUsageAfter });
    },
    
  };
}

jsunity.run(ClusterInfoMemoryTrackingSuite);
return jsunity.done();
