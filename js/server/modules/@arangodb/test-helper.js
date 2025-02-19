/*jshint strict: false */
/*global arango, db, assertTrue */

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
// / @author Lucas Dohmen
// / @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal'); // OK: processCsvFile
const ArangoError = require('@arangodb').ArangoError;;
const request = require('@arangodb/request');
const base64Encode = internal.base64Encode;
const crypto = require('@arangodb/crypto');
const {
  runWithRetry,
  getServerById,
  getServersByType,
  getEndpointById,
  getEndpointsByType,
  helper,
  deriveTestSuite,
  deriveTestSuiteWithnamespace,
  typeName,
  isEqual,
  compareStringIds,
  endpointToURL,
  versionHas,
  isEnterprise,
} = require('@arangodb/test-helper-common');
const clusterInfo = global.ArangoClusterInfo;

exports.runWithRetry = runWithRetry;
exports.isEnterprise = isEnterprise;
exports.versionHas = versionHas;
exports.getServerById = getServerById;
exports.getServersByType = getServersByType;
exports.getEndpointById = getEndpointById;
exports.getEndpointsByType = getEndpointsByType;
exports.helper = helper;
exports.deriveTestSuite = deriveTestSuite;
exports.deriveTestSuiteWithnamespace = deriveTestSuiteWithnamespace;
exports.typeName = typeName;
exports.isEqual = isEqual;
exports.compareStringIds = compareStringIds;
exports.pimpInstanceManager = function() {
  function makeAuthorizationHeaders (options, jwtSecret) {
    if (jwtSecret) {
      let jwt = crypto.jwtEncode(jwtSecret,
                                 {'server_id': 'none',
                                  'iss': 'arangodb'}, 'HS256');
      if (options.extremeVerbosity) {
        console.log(Date() + ' Using jw token:     ' + jwt);
      }
      return {
        'headers': {
          'Authorization': 'bearer ' + jwt
        }
      };
    } else {
      return {
        'headers': {
          'Authorization': 'Basic ' + base64Encode(options.username + ':' +
                                                   options.password)
        }
      };
    }
  }
  global.instanceManager.arangods.forEach(arangod => {
    arangod.isRole = function(compareRole) {
      return this.instanceRole === compareRole;
    };
    if (!arangod.isRole('agent') &&
        !arangod.isRole('single')) {
      let reply;
      let httpOptions = makeAuthorizationHeaders(global.instanceManager.options, arangod.JWT);
      try {
        httpOptions.returnBodyOnError = true;
        httpOptions.method = 'GET';
        reply = internal.download(arangod.url + '/_db/_system/_admin/server/id', '', httpOptions);
      } catch (e) {
        console.log(" error requesting server '" + JSON.stringify(arangod) + "' Error: " + JSON.stringify(e));
        if (e instanceof ArangoError && e.message.search('Connection reset by peer') >= 0) {
          httpOptions.method = 'GET';
          internal.sleep(5);
          reply = internal.download(arangod.url + '/_db/_system/_admin/server/id', '', httpOptions);
        } else {
          throw e;
        }
      }
      if (reply.error || reply.code !== 200) {
        throw new Error("Server has no detectable ID! " +
                        JSON.stringify(reply) + "\n" +
                        JSON.stringify(arangod));
      }
      let res = JSON.parse(reply.body);
      arangod.id = res['id'];
    }
  });
};
/// @brief set failure point
exports.debugCanUseFailAt = function (endpoint) {
  let res = request.get({
    url: endpoint + '/_admin/debug/failat',
  });
  return res.status === 200;
};

/// @brief set failure point
exports.debugSetFailAt = function (endpoint, failAt) {
  let res = request.put({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error setting failure point";
  }
};

exports.debugResetRaceControl = function (endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/raceControl',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error resetting race control.";
  }
};

exports.debugRemoveFailAt = function (endpoint, failAt) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure point";
  }
};

exports.debugClearFailAt = function (endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure points";
  }
};

exports.getChecksum = function (endpoint, name) {
  let res = request.get({
    url: endpoint + '/_api/collection/' + name + '/checksum',
  });
  return JSON.parse(res.body).checksum;
};

function getMetricName(text, name) {
  let re = new RegExp("^" + name);
  let matches = text.split("\n").filter((line) => !line.match(/^#/)).filter((line) => line.match(re));
  if (!matches.length) {
    throw "Metric " + name + " not found";
  }
  return Number(matches[0].replace(/^.*{.*}\s*([0-9.]+)$/, "$1"));
}

exports.getMetric = function (endpoint, name) {
  let res = request.get({
    url: endpoint + "/_admin/metrics",
  });
  return getMetricName(res.body, name);
};

exports.getMetricSingle = function (name) {
  let res = request.get("/_admin/metrics");
  if (res.status !== 200) {
    throw "error fetching metric";
  }
  return getMetricName(res.body, name);
};

exports.waitForShardsInSync = function(cn, timeout) {
  if (!timeout) {
    timeout = 300;
  }
  // The client variant has the database name inside it's scope.
  // TODO: Need to figure out how to get that done on server api. As soon as
  // this is necessary. We can always hand it in as parameter, but then we have
  // differences for client / server on this helper methods api ;(
  let dbName = "_system";
  let start = internal.time();
  const setsAreEqual = (left, right) => {
    // Sets have to be equal in size
    if (left.size !== right.size) {
      return false;
    }
    // Every entry in left, needs to be in right.
    // THere can be no duplicates in a set, so this
    // is equivalent to left and right having exactly the
    // same entries.
    for (const entry of left) {
      if (!right.has(entry)) {
        return false;
      }
    }
    return true;
  };

  while (internal.time() - start < timeout) {
    let insync = 0;
    const { shards } = clusterInfo.getCollectionInfo(dbName, cn);
    for (const [shard, plannedServers] of Object.entries(shards)) {
      const { servers } = clusterInfo.getCollectionInfoCurrent(dbName, cn, shard);
      if (setsAreEqual(new Set(servers), new Set(plannedServers))) {
        ++insync;
      }
    }
    if (insync === Object.keys(shards).length) {
      return;
    }
    if ((internal.time() - start) * 2 > timeout) {
      console.warn(`Only ${insync} in-sync out of ${Object.keys(shards).length} after ${internal.time() - start}, waiting until ${timeout}`);
    }
    internal.wait(1);
  }
  assertTrue(false, "Shards were not getting in sync in time, giving up!");
};

exports.getCoordinators = function () {
  // Note that the client implementation has more information, not all of which
  // we have available.
  return global.ArangoClusterInfo.getCoordinators().map(id => ({id}));
};

exports.getDBServers = function() {
  // Note that the client implementation has more information, not all of which
  // we have available.
  return global.ArangoClusterInfo.getDBServers().map(x => ({id: x.serverId}));
};

exports.triggerMetrics = function () {
  request({
    method: "get",
    url: "/_db/_system/_admin/metrics?mode=write_global",
    headers: {accept: "application/json"},
    body: {}
  });
  request({
    method: "get",
    url: "/_db/_system/_admin/metrics?mode=trigger_global",
    headers: {accept: "application/json"},
    body: {}
  });
  require("internal").sleep(2);
};

exports.uniqid = global.ArangoClusterInfo.uniqid;
exports.agency = global.ArangoAgency;
