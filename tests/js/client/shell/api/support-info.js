/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertMatch, assertTrue, assertFalse, arango */

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
const isCluster = require("internal").isCluster();
const { getEndpointsByType } = require('@arangodb/test-helper');

function supportInfoApiSuite() {
  'use strict';

  let validateHost = function(host) {
    assertTrue(host.hasOwnProperty("maintenance"));
    assertEqual("boolean", typeof host.maintenance);
    assertFalse(host.maintenance);
    assertTrue(host.hasOwnProperty("readOnly"));
    assertEqual("boolean", typeof host.readOnly);
    assertFalse(host.readOnly);
    assertTrue(host.hasOwnProperty("os"));
    assertTrue(host.hasOwnProperty("platform"));
    assertTrue(host.hasOwnProperty("physicalMemory"));
    assertEqual("boolean", typeof host.physicalMemory.overridden);
    assertEqual("number", typeof host.physicalMemory.value);
    assertTrue(host.hasOwnProperty("numberOfCores"));
    assertEqual("boolean", typeof host.numberOfCores.overridden);
    assertEqual("number", typeof host.numberOfCores.value);
    assertTrue(host.hasOwnProperty("processStats"));
    assertEqual("number", typeof host.processStats.processUptime);
    assertEqual("number", typeof host.processStats.numberOfThreads);
    assertEqual("number", typeof host.processStats.virtualSize);
    assertEqual("number", typeof host.processStats.residentSetSize);
    
    if (host.hasOwnProperty("cpuStats")) {
      // cpuStats is not present on all OSes
      assertEqual("number", typeof host.cpuStats.userPercent);
      assertEqual("number", typeof host.cpuStats.systemPercent);
      assertEqual("number", typeof host.cpuStats.idlePercent);
    }
    if (host.hasOwnProperty("engineStats")) {
      // engineStats is not present on coordinators
      Object.keys(host.engineStats).forEach((key) => {
        assertEqual("number", typeof host.engineStats[key]);
      });
    }
  };

  return {

    testAccessOnSingleOrCoordinator: function () {
      let res = arango.GET("/_admin/support-info");
      assertTrue(res.hasOwnProperty("deployment"));
      assertTrue(res.hasOwnProperty("date"));
      assertMatch(/^\d+-\d+-\d+T\d+:\d+:\d+/, res.date);

      if (isCluster) {
        assertEqual("cluster", res.deployment.type);
        assertTrue(res.deployment.hasOwnProperty("servers"));
        Object.keys(res.deployment.servers).forEach((server) => {
          let host = res.deployment.servers[server];
      
          assertTrue(host.hasOwnProperty("id"));
          assertTrue(host.hasOwnProperty("alias"));
          assertTrue(host.hasOwnProperty("endpoint"));
          assertTrue(host.hasOwnProperty("role"));
          assertTrue(host.role === "COORDINATOR" || host.role === "PRIMARY");
          validateHost(host);
        });
        assertEqual("number", typeof res.deployment.agents);
        assertEqual("number", typeof res.deployment.coordinators);
        assertEqual("number", typeof res.deployment.dbServers);
        assertTrue(res.deployment.hasOwnProperty("shards"));
        assertEqual("number", typeof res.deployment.shards.databases);
        assertEqual("number", typeof res.deployment.shards.collections);
        assertEqual("number", typeof res.deployment.shards.shards);
      } else {
        assertEqual("single", res.deployment.type);
        assertTrue(res.hasOwnProperty("host"));
        assertTrue(res.host.hasOwnProperty("role"));
        assertTrue(res.host.role === "SINGLE");
        validateHost(res.host);
      }
    },

    testAccessOnDBServers: function () {
      if (!isCluster) {
        return;
      }

      const getUrl = endpoint => endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');

      let dbs = getEndpointsByType('dbserver');
      assertTrue(dbs.length > 1);

      dbs.forEach((ep) => {
        const res = request.get({
          url: getUrl(ep) + '/_admin/support-info'
        });
        assertFalse(res.json.hasOwnProperty("deployment"));
        assertTrue(res.json.hasOwnProperty("host"));
        assertTrue(res.json.host.hasOwnProperty("role"));
        assertTrue(res.json.host.role === "PRIMARY");
        validateHost(res.json.host);
      });
    },
  };
}

jsunity.run(supportInfoApiSuite);
return jsunity.done();
