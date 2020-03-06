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

const chai = require('chai');
const expect = chai.expect;
const request = require("@arangodb/request");


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
      expect(res.code).is(200);
      return prometheusToJson(res.body);
    };

    it('test', () => {
        
        loadMetrics("agent",[0])
        expect(true).is(false);

    });
});