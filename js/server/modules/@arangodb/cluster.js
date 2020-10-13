/* global ArangoServerState, ArangoClusterInfo */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// / @brief JavaScript cluster functionality
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// /////////////////////////////////////////////////////////////////////////////

var console = require('console');
var arangodb = require('@arangodb');
var ArangoError = arangodb.ArangoError;
var errors = require("internal").errors;
var wait = require('internal').wait;
var isEnterprise = require('internal').isEnterprise();
var _ = require('lodash');

var endpointToURL = function (endpoint) {
  if (endpoint.substr(0, 6) === 'ssl://') {
    return 'https://' + endpoint.substr(6);
  }
  var pos = endpoint.indexOf('://');
  if (pos === -1) {
    return 'http://' + endpoint;
  }
  return 'http' + endpoint.substr(pos);
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief throw an ArangoError
// /////////////////////////////////////////////////////////////////////////////

var raiseError = function (code, msg) {
  var err = new ArangoError();
  err.errorNum = code;
  err.errorMessage = msg;

  throw err;
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief retrieve a list of shards for a collection
// /////////////////////////////////////////////////////////////////////////////

var shardList = function (dbName, collectionName) {
  let ci = global.ArangoClusterInfo.getCollectionInfo(dbName, collectionName);

  if (ci === undefined || typeof ci !== 'object') {
    throw "unable to determine shard list for '" + dbName + '/' + collectionName + "'";
  }

  let shards = [];
  for (let shard in ci.shards) {
    if (ci.shards.hasOwnProperty(shard)) {
      shards.push(shard);
    }
  }

  if (shards.length === 0 && isEnterprise) {
    if (isEnterprise) {
      return require('@arangodb/clusterEE').getSmartShards(dbName, collectionName, ci);
    } else {
      raiseError(arangodb.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code,
        arangodb.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.message);
    }
  }

  return shards;
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief whether or not clustering is enabled
// /////////////////////////////////////////////////////////////////////////////

var isCluster = function () {
  var role = global.ArangoServerState.role();

  return (role !== undefined && role !== 'SINGLE' && role !== 'AGENT');
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief whether or not we are a coordinator
// /////////////////////////////////////////////////////////////////////////////

var isCoordinator = function () {
  return global.ArangoServerState.isCoordinator();
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief role
// /////////////////////////////////////////////////////////////////////////////

var role = function () {
  var role = global.ArangoServerState.role();

  if (role !== 'SINGLE') {
    return role;
  }
  return undefined;
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief status
// /////////////////////////////////////////////////////////////////////////////

var status = function () {
  if (!isCluster() || !global.ArangoServerState.initialized()) {
    return undefined;
  }

  return global.ArangoServerState.status();
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief coordinatorId
// /////////////////////////////////////////////////////////////////////////////

var coordinatorId = function () {
  if (!isCoordinator()) {
    console.topic('heartbeat=error', 'not a coordinator');
  }
  return global.ArangoServerState.id();
};

// /////////////////////////////////////////////////////////////////////////////
// / @brief shard distribution
// /////////////////////////////////////////////////////////////////////////////

function shardDistribution () {
  return {
    results: require('internal').getShardDistribution()
  };
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief supervision state
// /////////////////////////////////////////////////////////////////////////////

function supervisionState () {
  try {
    var result = global.ArangoAgency.get('Target');
    result = result.arango.Target;
    var proj = { ToDo: result.ToDo, Pending: result.Pending,
      Failed: result.Failed, Finished: result.Finished,
    error: false };
    return proj;
  } catch (err) {
    return { error: true, errorMsg: 'could not read /Target in agency',
    exception: err };
  }
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief wait for synchronous replication to settle
// /////////////////////////////////////////////////////////////////////////////

function checkForSyncReplOneCollection (dbName, collName) {
  try {
    let cinfo = global.ArangoClusterInfo.getCollectionInfo(dbName, collName);
    let shards = Object.keys(cinfo.shards);
    let ccinfo = shards.map(function (s) {
      return global.ArangoClusterInfo.getCollectionInfoCurrent(dbName,
        collName, s).servers;
    });
    console.topic('heartbeat=debug', 'checkForSyncReplOneCollection:', dbName, collName, shards,
                  cinfo.shards, ccinfo);
    let ok = true;
    for (let i = 0; i < shards.length; ++i) {
      if (cinfo.shards[shards[i]].length !== ccinfo[i].length) {
        ok = false;
      }
    }
    if (ok) {
      console.topic('heartbeat=debug', 'checkForSyncReplOneCollection: OK:', dbName, collName,
                    shards);
      return true;
    }
    console.topic('heartbeat=debug', 'checkForSyncReplOneCollection: not yet:', dbName, collName,
                  shards);
    return false;
  } catch (err) {
    console.topic('heartbeat=error', 'checkForSyncReplOneCollection: exception:', dbName, collName,
                  JSON.stringify(err));
  }
  return false;
}

function waitForSyncRepl (dbName, collList) {
  if (!isCoordinator()) {
    return true;
  }
  let n = collList.length;
  let count = 30 * n;   // wait for up to 30 * collList.length seconds
                        // usually, this is much faster, but under load
                        // when many unittests run, things may take longer
  let ok = [...Array(n)].map(v => false);
  while (--count > 0) {
    let allOk = true;
    for (var i = 0; i < n; ++i) {
      if (!ok[i]) {
        ok[i] = checkForSyncReplOneCollection(dbName, collList[i].name());
        allOk = allOk && ok[i];
      }
    }
    if (allOk) {
      return true;
    }
    require('internal').wait(1, false);
  }
  console.topic('heartbeat=warn', 'waitForSyncRepl: timeout:', dbName, collList);
  return false;
}

function endpoints() {
  try {
    let coords = global.ArangoClusterInfo.getCoordinators();
    let endpoints = coords.map(c => global.ArangoClusterInfo.getServerEndpoint(c));
    return { "endpoints": endpoints.map(function(e) {
                                          return {"endpoint": e};
                                        }) };
  } catch (err) {
    return { error: true, exception: err };
  }
}

function queryAgencyJob(id) {
  let job = null;
  let states = ["Finished", "Pending", "Failed", "ToDo"];
  for (let s of states) {
    try {
      job = global.ArangoAgency.get('Target/' + s + '/' + id);
      job = job.arango.Target[s];
      if (Object.keys(job).length !== 0 && job.hasOwnProperty(id)) {
        return {error: false, id, status: s, job: job[id]};
      }
    } catch (err) {
    }
  }
  return {error: true, errorMsg: "Did not find job.", id, job: null};
}

exports.coordinatorId = coordinatorId;
exports.isCluster = isCluster;
exports.isCoordinator = isCoordinator;
exports.role = role;
exports.shardList = shardList;
exports.status = status;
exports.endpointToURL = endpointToURL;
exports.shardDistribution = shardDistribution;
exports.supervisionState = supervisionState;
exports.waitForSyncRepl = waitForSyncRepl;
exports.endpoints = endpoints;
exports.queryAgencyJob = queryAgencyJob;
