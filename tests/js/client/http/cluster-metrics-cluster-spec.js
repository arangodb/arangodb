/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global describe, beforeEach, afterEach, it, global */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// //////////////////////////////////////////////////////////////////////////////

const {expect} = require('chai');
const request = require("@arangodb/request");


const MetricNames = {
  HEARTBEAT_BUCKET: "heartbeat_send_time_msec_bucket",
  QUERY_TIME: "arangodb_aql_total_query_time_ms",
  PHASE_1_BUCKET: "maintenance_phase1_runtime_msec_bucket",
  PHASE_1_COUNT: "maintenance_phase1_runtime_msec_count",
  PHASE_2_BUCKET: "maintenance_phase2_runtime_msec_bucket",
  PHASE_2_COUNT: "maintenance_phase2_runtime_msec_count", 
};


describe('_admin/metrics', () => {

  const getServers = () => {
    const endpointToURL = (endpoint) => {
      if (endpoint.substr(0, 6) === 'ssl://') {
        return 'https://' + endpoint.substr(6);
      }
      var pos = endpoint.indexOf('://');
      if (pos === -1) {
        return 'http://' + endpoint;
      }
      return 'http' + endpoint.substr(pos);
    };
    const {instanceInfo} = global;
    const list = new Map();
    list.set("coordinator", []);
    list.set("dbserver", []);
    list.set("agent", []);
    for (const d of instanceInfo.arangods) {
      const {role, endpoint} = d;
      list.get(role).push(endpointToURL(endpoint));
    }
    return list;
};


    let servers;
    before(() => {
      servers = getServers();
    });
    
    const prometheusToJson = (prometheus) => {
      const lines = prometheus.split('\n').filter((s) => !s.startsWith('#') && s !== '');
      const res = {};
      for (const l of lines) {
        const [key, count] = l.split(' ');
        if (key.indexOf('{le="') !== -1) {
          // Bucket case
          // We only check for:
          // identifier{le="range"}
          // For all other buckt-types this code will fail
          const [base, bucket] = key.split('{le="');
          res[base] = res[base] || {};
          res[base][bucket.substr(0, bucket.length -2)] = parseFloat(count);
        } else {
          // evertyhing else
          res[key] = parseFloat(count);
        }

      }
      return res;
    };

    const loadMetrics = (role, idx) =>  {
      const url = `${servers.get(role)[idx]}/_admin/metrics`;
      
      const res = request({
        json: true,
        method: 'GET',
        url
      });
      expect(res.statusCode).to.equal(200);
      return prometheusToJson(res.body);
    };

    const joinMetrics = (lhs, rhs) => {
      if (Object.entries(lhs).length === 0) {
        return rhs;
      }
      for (const [key, value] of Object.entries(rhs)) {
        if (value instanceof Object) {
          for (const [bucket, content] of Object.entries(value)) {
            lhs[key][bucket] += content 
          }
        } else {
          lhs[key] += rhs[key];
        }
      }
      return lhs;
    }

    const loadAllMetrics = (role) => {
      return servers.get(role).map((_, i) => loadMetrics(role, i)).reduce(joinMetrics, {});
     };

     const loadAllMetricsFiltered = (role, keys) => {
       const mets = loadAllMetrics(role);
       return Object.assign({}, ...keys.map(key => key in mets ? {[key]: mets[key]} : {}));
     };

     const expectOneBucketChanged = (actual, old) => {
       let foundOne = false;
       for (const [key, value] of Object.entries(actual)) {
         if (old[key] < value) {
           foundOne = true;
         }
         expect(old[key]).to.be.most(value);

       }
       expect(foundOne).to.equal(true);
     };

    it('aql query time', () => {
      const before = loadAllMetrics("coordinator")[MetricNames.QUERY_TIME];
        
      const query = `return sleep(1)`;
      db._query(query);

      const after = loadAllMetrics("coordinator")[MetricNames.QUERY_TIME];
      expect(before + 1000).to.be.lessThan(after);
    });

    it('maintenance', () => {

      try {
        const before = loadAllMetricsFiltered("dbserver", [MetricNames.PHASE_1_BUCKET, MetricNames.PHASE_1_COUNT, MetricNames.PHASE_2_BUCKET, MetricNames.PHASE_2_COUNT]);
        // Collection Creation requires Phase 1, Phase 2
        db._create("UnitTestCollection", {numberOfShards: 9});
        const intermediate = loadAllMetricsFiltered("dbserver",[MetricNames.PHASE_1_BUCKET, MetricNames.PHASE_1_COUNT, MetricNames.PHASE_2_BUCKET, MetricNames.PHASE_2_COUNT]);


        expect(intermediate[MetricNames.PHASE_1_COUNT]).to.be.greaterThan(before[MetricNames.PHASE_1_COUNT]);
        expect(intermediate[MetricNames.PHASE_2_COUNT]).to.be.greaterThan(before[MetricNames.PHASE_2_COUNT]);

        expectOneBucketChanged(intermediate[MetricNames.PHASE_1_BUCKET], before[MetricNames.PHASE_1_BUCKET]);
        expectOneBucketChanged(intermediate[MetricNames.PHASE_2_BUCKET], before[MetricNames.PHASE_2_BUCKET]);

        // Index creation requires Phase 1 and Phase 2
        db["UnitTestCollection"].ensureHashIndex("temp");
        const after = loadAllMetricsFiltered("dbserver", [MetricNames.PHASE_1_BUCKET, MetricNames.PHASE_1_COUNT, MetricNames.PHASE_2_BUCKET, MetricNames.PHASE_2_COUNT]);

        expect(after[MetricNames.PHASE_1_COUNT]).to.be.greaterThan(intermediate[MetricNames.PHASE_1_COUNT]);
        expect(after[MetricNames.PHASE_2_COUNT]).to.be.greaterThan(intermediate[MetricNames.PHASE_2_COUNT]);

        expectOneBucketChanged(after[MetricNames.PHASE_1_BUCKET], intermediate[MetricNames.PHASE_1_BUCKET]);
        expectOneBucketChanged(after[MetricNames.PHASE_2_BUCKET], intermediate[MetricNames.PHASE_2_BUCKET]);

      } finally {
        db._drop("UnitTestCollection");
      }


    });
});