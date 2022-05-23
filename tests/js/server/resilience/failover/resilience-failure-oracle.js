/*jshint strict: true */
/*global assertTrue, assertEqual*/
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const request = require('@arangodb/request');
const {sleep} = require('internal');
const helper = require("@arangodb/testutils/replicated-logs-helper");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");

const {
  waitFor,
  dbservers,
  registerAgencyTestBegin,
  registerAgencyTestEnd,
  checkRequestResult,
  getServerHealth,
  getServerUrl,
  continueServerWaitOk,
  stopServerWaitFailed,
} = helper;
const {
  allServersHealthy,
} = lpreds;

function getFailureOracleStatus(url) {
  const status = request.get(`${url}/_admin/cluster/failureOracle/status`);
  checkRequestResult(status);
  return status.json.result;
}

function flushFailureOracle(url, global = false) {
  const status = request.post(`${url}/_admin/cluster/failureOracle/flush?global=${global}`);
  checkRequestResult(status);
  return status.json.result;
}

function getAgencyFailureStatus(serverId) {
  return getServerHealth(serverId) !== "GOOD";
}

const FailureOracleSuite = function () {
  return {
    setUp: registerAgencyTestBegin,
    tearDown: function (test) {
      waitFor(allServersHealthy());
      registerAgencyTestEnd(test);
    },

    testFailureOracleStatus: function () {
      let coordinators = global.instanceManager.arangods.filter(arangod => arangod.instanceRole === 'coordinator');
      assertTrue(coordinators.length > 0);
      let coord = coordinators[0];
      const coordUrl = coord.url;
      const dbs0Url = getServerUrl(dbservers[0]);
      const dbs1Url = getServerUrl(dbservers[1]);

      waitFor(allServersHealthy());

      // Check the existence of time field
      let statusOnCoord = getFailureOracleStatus(coordUrl);
      let statusOnDbs = getFailureOracleStatus(dbs1Url);
      assertTrue(statusOnCoord.lastUpdated !== undefined);
      assertTrue(statusOnDbs.lastUpdated !== undefined);

      // All servers should be healthy
      waitFor(function () {
        statusOnCoord = getFailureOracleStatus(coordUrl);
        for (const [serverId, failureStatus] of Object.entries(statusOnCoord.isFailed)) {
          let agencyStatus = getAgencyFailureStatus(serverId);
          if (failureStatus !== agencyStatus) {
            return Error(`Server ${serverId} is reported as ${failureStatus} by the oracle, but appears as ${agencyStatus} in the agency`);
          }
        }
        return true;
      });

      // Stop server 0 and check the status on server 1
      stopServerWaitFailed(dbservers[0]);
      waitFor(function () {
        statusOnDbs = getFailureOracleStatus(dbs1Url);
        if (!statusOnDbs.isFailed[dbservers[0]]) {
          return Error(`Server ${dbservers[0]} is reported as healthy by the oracle, but appears failed in the agency`);
        }
        return true;
      });

      // While server 0 is stopped, stop server 2, then resume server 0 and check the status on server 0
      stopServerWaitFailed(dbservers[2]);
      continueServerWaitOk(dbservers[0]);
      waitFor(function () {
        statusOnDbs = getFailureOracleStatus(dbs0Url);
        if (!statusOnDbs.isFailed[dbservers[2]]) {
          return Error(`Server ${dbservers[2]} is reported as healthy by the oracle, but appears failed in the agency`);
        }
        return true;
      });

      // Resume server 2 and check status on server 1
      continueServerWaitOk(dbservers[2]);
      waitFor(function () {
        statusOnDbs = getFailureOracleStatus(dbs1Url);
        if (statusOnDbs.isFailed[dbservers[2]]) {
          return Error(`Server ${dbservers[2]} is reported as failed by the oracle, but appears healthy in the agency`);
        }
        return true;
      });
    },

    testFailureOracleFlush: function () {
      let coordinators = global.instanceManager.arangods.filter(arangod => arangod.instanceRole === 'coordinator');
      assertTrue(coordinators.length > 0);
      let coord = coordinators[0];
      const coordUrl = coord.url;
      const dbs0Url = getServerUrl(dbservers[0]);
      waitFor(allServersHealthy());

      // Check if a flush took place by comparing the last updated time
      let statusOnDbs = getFailureOracleStatus(dbs0Url);
      let lastUpdated = statusOnDbs.lastUpdated;
      sleep(1);
      flushFailureOracle(dbs0Url);
      statusOnDbs = getFailureOracleStatus(dbs0Url);
      assertTrue(statusOnDbs !== lastUpdated);

      // Check if the global flush works
      lastUpdated = statusOnDbs.lastUpdated;

      sleep(1);
      let response = flushFailureOracle(coordUrl, true);
      statusOnDbs = getFailureOracleStatus(dbs0Url);
      assertTrue(statusOnDbs !== lastUpdated);
      for (const dbs of dbservers) {
        assertEqual(response[dbs], 200);
      }
    },
  };
};

jsunity.run(FailureOracleSuite);
return jsunity.done();
