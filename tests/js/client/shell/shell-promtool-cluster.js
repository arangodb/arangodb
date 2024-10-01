/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global print, assertNotEqual, assertFalse, assertTrue, assertEqual, fail, arango */

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
/// @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const console = require('console');
const request = require("@arangodb/request");
const expect = require('chai').expect;
const errors = require('@arangodb').errors;

// name of environment variable
const PATH = 'PROMTOOL_PATH';

// detect the path to promtool
let promtoolPath = internal.env[PATH];
if (!promtoolPath) {
  promtoolPath = '.';
}
if (fs.isDirectory(promtoolPath)) {
  promtoolPath = fs.join(promtoolPath, 'promtool' + pu.executableExt);
}

const metricsUrlPath = "/_admin/metrics/v2";
const serverIdPath = "/_admin/server/id";
const healthUrl = "_admin/cluster/health";

function getServerId(server) {
  let res = request.get({
    url: server.url + serverIdPath
  });
  return res.json.id;
}

function getServerShortName(server) {
  let serverId = getServerId(server);
  let clusterHealth = arango.GET_RAW(healthUrl).parsedBody.Health;
  let shortName = Object.keys(clusterHealth)
    .filter(x => {
      return x === serverId;
    })
    .map(x => clusterHealth[x]["ShortName"])[0];
  return shortName;
}

function checkThatServerIsResponsive(server) {
  try {
    let serverName = getServerShortName(server);
    print("Checking if server " + serverName + " is responsive.");
    let res = request.get({
      url: server.url + metricsUrlPath
    });
    if (res.body.includes(serverName) && res.statusCode === 200) {
      print("Server " + serverName + " is OK!");
      return true;
    } else {
      print("Server " + serverName + " doesn't respond properly to requests.");
      return false;
    }
  } catch(error){
    return false;
  }
}

function checkThatAllDbServersAreHealthy() {
  print("Checking that all DB servers are healthy.");
  let clusterHealth = arango.GET_RAW(healthUrl).parsedBody.Health;
  let dbServers = Object.keys(clusterHealth)
    .filter(x => {
      return clusterHealth[x]["Role"] === "DBServer";
    })
    .map(x => clusterHealth[x]);
    for(let i = 0; i < dbServers.length; i++) {
      print("Server "+ dbServers[i].ShortName + " status is " + dbServers[i]["Status"]);
      if(!(dbServers[i]["Status"] === "GOOD")){
        return false;
      }
    }
  return true;
}

function validateMetrics(metrics) {
  let toRemove = [];
  try {
    // store output of /_admin/metrics/v2 into a temp file
    let input = fs.getTempFile();
    fs.writeFileSync(input, metrics);
    toRemove.push(input);

    // this is where we will capture the output from promtool
    let output = fs.getTempFile();
    toRemove.push(output);

    // build command string. this will be unsafe if input/output
    // contain quotation marks and such. but as these parameters are
    // under our control this is very unlikely
    let command = promtoolPath + ' check metrics < "' + input + '" > "' + output + '" 2>&1';
    // pipe contents of temp file into promtool
    let actualRc = internal.executeExternalAndWait('sh', ['-c', command]);

    // provide more context about what exactly went wrong
    let help = "";
    try {
      help = metrics + "\n\n" + String(fs.readFileSync(output));
    } catch (err) {}

    assertTrue(actualRc.hasOwnProperty('exit'));
    assertEqual(0, actualRc.exit, help);

    let promtoolResult = fs.readFileSync(output).toString();
    // no errors found means an empty result file
    assertEqual('', promtoolResult);
  } finally {
    // remove temp files
    toRemove.forEach((f) => {
      fs.remove(f);
    });
  }
}

function checkMetricsBelongToServer(metrics, server) {
  let serverName = getServerShortName(server);
  let positiveRegex = new RegExp('(shortname="(' + serverName + ').*")');
  let negativeRegex = new RegExp('(shortname="(?!' + serverName + ').*")');
  let matchesServerName = metrics.match(positiveRegex);
  let matchesAnyOtherName = metrics.match(negativeRegex);
  assertNotEqual(null, matchesServerName, "Metrics must contain server name, but they don't");
  assertTrue(matchesServerName.length > 0, "Metrics must contain server name, but they don't");
  assertEqual(null, matchesAnyOtherName, "Metrics must NOT contain other servers' names.");
}

function validateMetricsOnServer(server) {
  print("Querying server ", server.name);
  let res = request.get({
    url: server.url + metricsUrlPath
  });
  expect(res).to.be.an.instanceof(request.Response);
  expect(res).to.have.property('statusCode', 200);
  let body = String(res.body);
  validateMetrics(body);
}

function validateMetricsViaCoordinator(coordinator, server) {
  let serverId = getServerId(server);
  let metricsUrl = coordinator.url + metricsUrlPath + "?serverId=" + serverId;
  let res = request.get({ url: metricsUrl });
  expect(res).to.be.an.instanceof(request.Response);
  expect(res).to.have.property('statusCode', 200);
  let body = String(res.body);
  validateMetrics(body);
  checkMetricsBelongToServer(body, server);
}

function promtoolClusterSuite() {
  'use strict';

  let dbServers = global.instanceManager.arangods.filter(arangod => arangod.instanceRole === "dbserver");
  let agents = global.instanceManager.arangods.filter(arangod => arangod.instanceRole === "agent");
  let coordinators = global.instanceManager.arangods.filter(arangod => arangod.instanceRole === "coordinator");

  return {
    testMetricsOnDbServers: function () {
      print("Validating metrics from db servers...");
      for (let i = 0; i < dbServers.length; i++) {
        validateMetricsOnServer(dbServers[i]);
      }
    },
    testMetricsOnAgents: function () {
      print("Validating metrics from agents...");
      for (let i = 0; i < agents.length; i++) {
        validateMetricsOnServer(agents[i]);
      }
    },
    testMetricsOnCoordinators: function () {
      print("Validating metrics from coordinators...");
      for (let i = 0; i < coordinators.length; i++) {
        validateMetricsOnServer(coordinators[i]);
      }
    },
    testMetricsViaCoordinator: function () {
      //exclude agents, because agents cannot be queried via coordinator
      let servers = dbServers.concat(coordinators);
      for (let i = 0; i < servers.length; i++) {
        validateMetricsViaCoordinator(coordinators[0], servers[i]);
      }
    },
    testInvalidServerId: function () {
      //query metrics from coordinator, supplying invalid server id
      let coordinator = coordinators[0];
      let metricsUrl = coordinator.url + metricsUrlPath + "?serverId=" + "invalid-server-id";
      let res = request.get({ url: metricsUrl });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 404);
      expect(res.json.errorNum).to.equal(errors.ERROR_HTTP_BAD_PARAMETER.code);
    },
    testServerDoesntRespond: function () {
      //query metrics from coordinator, supplying id of a server, that is shut down
      let dbServer = dbServers[0];
      let serverId = getServerId(dbServer);
      assertTrue(dbServer.suspend());
      try {
        let metricsUrl = metricsUrlPath + "?serverId=" + serverId;
        let res = arango.GET_RAW(metricsUrl);
        assertTrue(res.code === 500 || res.code === 503);
        //Do not validate response body because errorNum and errorMessage can differ depending on whether there was an open connection
        //between coordinator and db server when the later stopped responding. This cannot be reproduced consistently in the test.
        //        let body = res.parsedBody;
        //        expect(body.errorNum).to.equal(errors.ERROR_CLUSTER_CONNECTION_LOST.code);
      } finally {
        let clusterHealthOk = false;
        assertTrue(dbServer.resume());
        for (let i = 0; i < 60; i++) {
          if (checkThatServerIsResponsive(dbServer)) {
            break;
          }
          internal.sleep(1);
        }
        for (let i = 0; i < 60; i++) {
          if (checkThatAllDbServersAreHealthy()) {
            clusterHealthOk = true;
            break;
          }
          internal.sleep(1);
        }
        assertFalse(dbServer.suspended, "Couldn't recover server after suspension.");
        assertTrue(clusterHealthOk, "Some db servers are not ready according to " + healthUrl);
      }
    }
  };
}

if (internal.platform === 'linux') {
  // this test intentionally only executes on Linux, and only if PROMTOOL_PATH
  // is set to the path containing the `promtool` executable. if the PROMTOOL_PATH
  // is set, but the executable cannot be found, the test will error out.
  // the test also requires `sh` to be a shell that supports input/output redirection,
  // and `true` to be an executable that returns exit code 0 (we use sh -c true` as a
  // test to check the shell functionality).
  if (fs.exists(promtoolPath)) {
    let actualRc = internal.executeExternalAndWait('sh', ['-c', 'true']);
    if (actualRc.hasOwnProperty('exit') && actualRc.exit === 0) {
      jsunity.run(promtoolClusterSuite);
    } else {
      console.warn('skipping test because no working sh can be found');
    }
  } else if (!internal.env.hasOwnProperty(PATH)) {
    console.warn('skipping test because promtool is not found. you can set ' + PATH + ' accordingly');
  } else {
    fail('promtool not found in PROMTOOL_PATH (' + internal.env[PATH] + ')');
  }
} else {
  console.warn('skipping test because we are not on Linux');
}

return jsunity.done();
