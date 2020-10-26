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

function collectionShardDistribution (name) {
  return {
    results: require('internal').getCollectionShardDistribution(name)
  };
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief move shard
// /////////////////////////////////////////////////////////////////////////////

function moveShard (info) {
  var isLeader;
  var collInfo;
  try {
    // First translate server names from short names to long names:
    var servers = global.ArangoClusterInfo.getDBServers();
    for (let i = 0; i < servers.length; i++) {
      if (servers[i].serverId !== info.fromServer) {
        if (servers[i].serverName === info.fromServer) {
          info.fromServer = servers[i].serverId;
        }
      }
      if (servers[i].serverId !== info.toServer) {
        if (servers[i].serverName === info.toServer) {
          info.toServer = servers[i].serverId;
        }
      }
    }
    collInfo = global.ArangoClusterInfo.getCollectionInfo(info.database,
      info.collection);
    if (collInfo.distributeShardsLike !== undefined) {
      return {error:true, errorMessage:'MoveShard only allowed for collections which have distributeShardsLike unset.'};
    }
    var shards = collInfo.shards;
    var shard = shards[info.shard];
    var pos = shard.indexOf(info.fromServer);
    if (pos === -1) {
      throw 'Banana';
    } else if (pos === 0) {
      isLeader = true;
    } else {
      isLeader = false;
    }
  } catch (e2) {
    return {error:true, errorMessage:'Combination of database, collection, shard and fromServer does not make sense.'};
  }

  var id;
  try {
    id = global.ArangoClusterInfo.uniqid();
    var todo = { 'type': 'moveShard',
      'database': info.database,
      'collection': collInfo.id,
      'shard': info.shard,
      'fromServer': info.fromServer,
      'toServer': info.toServer,
      'jobId': id,
      'timeCreated': (new Date()).toISOString(),
      'creator': ArangoServerState.id(),
      'isLeader': isLeader,
      'remainsFollower': isLeader};
    global.ArangoAgency.set('Target/ToDo/' + id, todo);
  } catch (e1) {
    return {error: true, errorMessage: 'Cannot write to agency.'};
  }

  return {error: false, id: id};
}

// /////////////////////////////////////////////////////////////////////////////
// / @brief rebalance shards
// /////////////////////////////////////////////////////////////////////////////

function rebalanceShards () {
  var dbServers = global.ArangoClusterInfo.getDBServers();
  var dbTab = {};
  var i, j, k, l;
  for (i = 0; i < dbServers.length; ++i) {
    dbTab[dbServers[i].serverId] = [];
  }
  var shardMap = {};

  // First count and collect:
  var db = require('internal').db;

  var colls = db._collections();
  for (j = 0; j < colls.length; ++j) {
    var collName = colls[j].name();
    if (collName.substr(0, 1) === '_') {
      continue;
    }
    var collInfo = global.ArangoClusterInfo.getCollectionInfo(
      db._name(), collName);
    if (collInfo.distributeShardsLike === undefined) {
      // Only consider those collections that do not follow another one
      // w.r.t. their shard distribution.
      var shardNames = Object.keys(collInfo.shards);
      for (k = 0; k < shardNames.length; k++) {
        var shardName = shardNames[k];
        shardMap[shardName] = {
          database: db._name(), collection: collName,
          servers: collInfo.shards[shardName], weight: 1 };
        dbTab[collInfo.shards[shardName][0]].push(
          { shard: shardName, leader: true,
            weight: shardMap[shardName].weight });
        for (l = 1; l < collInfo.shards[shardName].length; ++l) {
          dbTab[collInfo.shards[shardName][l]].push(
            { shard: shardName, leader: false,
              weight: shardMap[shardName].weight });
        }
      }
    }
  }
  
  console.topic('heartbeat=debug', "Rebalancing shards");
  console.topic('heartbeat=debug', shardMap);
  console.topic('heartbeat=debug', dbTab);

  // Compute total weight for each DBServer:
  var totalWeight = [];
  for (i = 0; i < dbServers.length; ++i) {
    totalWeight.push({'server': dbServers[i].serverId,
      'weight': _.reduce(dbTab[dbServers[i].serverId],
      (sum, x) => sum + x.weight, 0)});
  }
  totalWeight = _.sortBy(totalWeight, x => x.weight);

  var shardList = Object.keys(shardMap);
  var countMoved = 0;

  for (i = 0; i < shardList.length; i++) {
    var last = totalWeight.length - 1;
    var fullest = totalWeight[last].server;
    var emptiest = totalWeight[0].server;
    var weightDiff = totalWeight[last].weight - totalWeight[0].weight;
    if (weightDiff < 1.0) {
      console.topic('heartbeat=info', 'rebalanceShards: cluster is balanced');
      return true;
    }
    var shard = shardList[i];
    console.topic('heartbeat=info', 'rebalanceShards: considering shard', shard,
      'totalWeight=', totalWeight);
    if (shardMap[shard].servers.indexOf(fullest) >= 0 &&
      shardMap[shard].servers.indexOf(emptiest) === -1 &&
      shardMap[shard].weight < 0.9 * weightDiff) {
      var shardInfo = shardMap[shard];
      var todo = { database: shardInfo.database,
        collection: shardInfo.collection,
        shard: shard,
        fromServer: fullest,
      toServer: emptiest };
      var res = moveShard(todo);
      if (!res.error) {
        console.topic('heartbeat=debug', 'rebalanceShards: moveShard(', todo, ')');
        totalWeight[last].weight -= shardInfo.weight;
        totalWeight[0].weight += shardInfo.weight;
        totalWeight = _.sortBy(totalWeight, x => x.weight);
        countMoved += 1;
        if (countMoved >= 10) {
          break;
        }
      } else {
        console.topic('heartbeat=error', 'rebalanceShards: moveShard(', todo, ') produced:',
                      res.errorMessage);
      }
    }
  }
  if (countMoved === 0) {
    console.topic('heartbeat=info', 'rebalanceShards: no sensible moves found');
    return true;
  }
  console.topic('heartbeat=info', 'rebalanceShards: scheduled', countMoved, ' shard moves.');
  return true;
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
exports.collectionShardDistribution = collectionShardDistribution;
exports.rebalanceShards = rebalanceShards;
exports.moveShard = moveShard;
exports.supervisionState = supervisionState;
exports.waitForSyncRepl = waitForSyncRepl;
exports.endpoints = endpoints;
exports.queryAgencyJob = queryAgencyJob;
