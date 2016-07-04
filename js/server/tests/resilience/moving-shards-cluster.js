/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test moving shards in the cluster
///
/// @file js/server/tests/resilience/moving-shards-cluster.js
///
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const db = arangodb.db;
const _ = require("lodash");
const wait = require("internal").wait;
const supervisionState = require("@arangodb/cluster").supervisionState;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function MovingShardsSuite () {
  'use strict';
  var cn = "UnitTestMovingShards";
  var count = 0;
  var c = [];

////////////////////////////////////////////////////////////////////////////////
/// @brief find out servers for a collection
////////////////////////////////////////////////////////////////////////////////

  function findCollectionServers(database, collection) {
    var cinfo = global.ArangoClusterInfo.getCollectionInfo(database, collection);
    var shard = Object.keys(cinfo.shards)[0];
    return cinfo.shards[shard];
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for synchronous replication
////////////////////////////////////////////////////////////////////////////////

  function waitForSynchronousReplication(database) {
    console.info("Waiting for synchronous replication to settle...");
    global.ArangoClusterInfo.flush();
    for (var i = 0; i < c.length; ++i) {
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(database,
                                                             c[i].name());
      var shards = Object.keys(cinfo.shards);
      var replFactor = cinfo.shards[shards[0]].length;
      var count = 0;
      while (++count <= 120) {
        var ccinfo = shards.map(
          s => global.ArangoClusterInfo.getCollectionInfoCurrent(database,
                                                                 c[i].name(), s)
        );
        let replicas = ccinfo.map(s => s.servers.length);
        if (_.every(replicas, x => x === replFactor)) {
          console.info("Replication up and running!");
          break;
        }
        wait(0.5);
        global.ArangoClusterInfo.flush();
      }
      if (count > 120) {
        return false;
      }
    }
    return true;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief get cleaned out servers
////////////////////////////////////////////////////////////////////////////////

  function getCleanedOutServers() {
    var coordEndpoint = global.ArangoClusterInfo.getServerEndpoint("Coordinator001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var res = request({ method: "GET",
                        url: url + "/_admin/cluster/numberOfServers"});
    var body = res.body;
    if (typeof body === "string") {
      body = JSON.parse(body);
    }
    return body;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test whether or not a server is clean
////////////////////////////////////////////////////////////////////////////////

  function testServerEmpty(id, checkList, fromCollNr, toCollNr) {
    if (fromCollNr === undefined) {
      fromCollNr = 0;
    }
    if (toCollNr === undefined) {
      toCollNr = c.length - 1;
    }
    var count;
    var ok;
    for (var i = fromCollNr; i <= toCollNr; ++i) {
      count = 100;
      ok = false;
      while (--count > 0) {
        wait(1.0);
        global.ArangoClusterInfo.flush();
        var servers = findCollectionServers("_system", c[i].name());
        console.info("Seeing servers:", i, c[i].name(), servers);
        if (servers.indexOf(id) === -1) {
          // Now check current as well:
          var collInfo = global.ArangoClusterInfo.getCollectionInfo(
            "_system", c[i].name());
          var shards = collInfo.shards;
          var collInfoCurr = Object.keys(shards).map(s =>
            global.ArangoClusterInfo.getCollectionInfoCurrent(
              "_system", c[i].name(), s).servers);
          var idxs = collInfoCurr.map(l => l.indexOf(id));
          ok = true;
          for (var j = 0; j < idxs.length; j++) {
            if (idxs[j] !== -1) {
              ok = false;
            }
          }
          if (ok) {
            break;
          }
        }
      }
      if (!ok) {
        return false;
      }
    }

    if (checkList) {
      // Wait until the server appears in the list of cleanedOutServers:
      count = 100;
      while (--count > 0) {
        var obj = getCleanedOutServers();
        if (obj.cleanedServers.indexOf(id) < 0) {
          break;
        }
        console.info("cleanedServers:", obj);
        wait(1.0);
      }
      if (count <= 0) {
        ok = false;
      }
    }

    return ok;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief order the cluster to clean out a server:
////////////////////////////////////////////////////////////////////////////////

  function cleanOutServer(id) {
    var coordEndpoint = global.ArangoClusterInfo.getServerEndpoint("Coordinator001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var body = {"server": id};
    return request({ method: "POST",
                   url: url + "/_admin/cluster/cleanOutServer",
                   body: JSON.stringify(body) });
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief order the cluster to reduce number of db servers
////////////////////////////////////////////////////////////////////////////////

  function shrinkCluster(toNum) {
    var coordEndpoint = global.ArangoClusterInfo.getServerEndpoint("Coordinator001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var body = {"numberOfDBServers":toNum};
    return request({ method: "PUT",
                     url: url + "/_admin/cluster/numberOfServers",
                     body: JSON.stringify(body) });
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief order the cluster to clean out a server:
////////////////////////////////////////////////////////////////////////////////

  function resetCleanedOutServers() {
    var coordEndpoint = global.ArangoClusterInfo.getServerEndpoint("Coordinator001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var numberOfDBServers = global.ArangoClusterInfo.getDBServers().length;
    var body = {"cleanedServers":[], "numberOfDBServers":numberOfDBServers};
    try {
      var res = request({ method: "PUT",
                      url: url + "/_admin/cluster/numberOfServers",
                      body: JSON.stringify(body) });
      return res;
    }
    catch (err) {
      console.error("Exception for PUT /_admin/cluster/numberOfServers:",
                    err.stack);
      return false;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief move a single shard
////////////////////////////////////////////////////////////////////////////////

  function moveShard(database, collection, shard, fromServer, toServer) {
    var coordEndpoint = global.ArangoClusterInfo.getServerEndpoint("Coordinator001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var body = {database, collection, shard, fromServer, toServer};
    return request({ method: "POST",
                   url: url + "/_admin/cluster/moveShard",
                   body: JSON.stringify(body) });
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief create some collections
////////////////////////////////////////////////////////////////////////////////

  function createSomeCollections(n, nrShards, replFactor) {
    var systemCollServers = findCollectionServers("_system", "_graphs");
    console.info("System collections use servers:", systemCollServers);
    for (var i = 0; i < n; ++i) {
      ++count;
      while (true) {
        var name = cn + count;
        db._drop(name);
        var coll = db._create(name, {numberOfShards: nrShards,
                                     replicationFactor: replFactor});
        var servers = findCollectionServers("_system", name);
        console.info("Test collections uses servers:", servers);
        if (_.intersection(systemCollServers, servers).length === 0) {
          c.push(coll);
          break;
        }
        console.info("Need to recreate collection to avoid system collection servers.");
        c.push(coll);
        waitForSynchronousReplication("_system");
        c.pop();
        console.info("Synchronous replication has settled, now dropping again.");
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief find server not on a list
////////////////////////////////////////////////////////////////////////////////

  function findServerNotOnList(list) {
    var count = 1;
    var str = "" + count;
    var pad = "000";
    var ans = pad.substring(0, pad.length - str.length) + str;

    var name = "DBServer" + ans;
    while (list.indexOf(name) >= 0) {
      count += 1;
      str = "" + count;
      ans = pad.substring(0, pad.length - str.length) + str;
      name = "DBServer" + ans;
    }
    return name;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for Supervision to finish jobs
////////////////////////////////////////////////////////////////////////////////

  function waitForSupervision() {
    var count = 300;
    while (--count > 0) {
      var state = supervisionState();
      if (!state.error &&
          Object.keys(state.ToDo).length === 0 &&
          Object.keys(state.Pending).length === 0) {
        return true;
      }
      console.info("Waiting for supervision jobs to finish:",
                   "ToDo jobs:", Object.keys(state.ToDo).length,
                   "Pending jobs:", Object.keys(state.Pending).length);
      wait(1.0);
    }
    return false;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual tests
////////////////////////////////////////////////////////////////////////////////

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      createSomeCollections(1, 1, 2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      for (var i = 0; i < c.length; ++i) {
        c[i].drop();
      }
      c = [];
      resetCleanedOutServers();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a synchronously replicated collection gets online
////////////////////////////////////////////////////////////////////////////////

    testSetup : function () {
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with one shard without replication
////////////////////////////////////////////////////////////////////////////////

    testShrinkNoReplication : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      shrinkCluster(4);
      assertTrue(testServerEmpty("DBServer005", true));
      assertTrue(waitForSupervision());
      shrinkCluster(3);
      assertTrue(testServerEmpty("DBServer004", true));
      assertTrue(waitForSupervision());
      shrinkCluster(2);
      assertTrue(testServerEmpty("DBServer003", true));
      assertTrue(waitForSupervision());
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromFollower : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[0].name());
      var fromServer = servers[1];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[0].name());
      var shard = Object.keys(cinfo.shards)[0];
      moveShard("_system", c[0]._id, shard, fromServer, toServer);
      assertTrue(testServerEmpty(fromServer), false);
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeader : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[0].name());
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[0].name());
      var shard = Object.keys(cinfo.shards)[0];
      moveShard("_system", c[0]._id, shard, fromServer, toServer);
      assertTrue(testServerEmpty(fromServer), false);
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader without replication
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderNoReplication : function() {
      createSomeCollections(1, 1, 1);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      moveShard("_system", c[1]._id, shard, fromServer, toServer);
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower with 3 replicas #1
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromFollowerRepl3_1 : function() {
      createSomeCollections(1, 1, 3);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[1];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      moveShard("_system", c[1]._id, shard, fromServer, toServer);
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower with 3 replicas #2
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromRepl3_2 : function() {
      createSomeCollections(1, 1, 3);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[2];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      moveShard("_system", c[1]._id, shard, fromServer, toServer);
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader with 3 replicas
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderRepl : function() {
      createSomeCollections(1, 1, 3);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      moveShard("_system", c[1]._id, shard, fromServer, toServer);
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out a follower
////////////////////////////////////////////////////////////////////////////////

    testCleanOutFollower : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[0].name());
      var toClean = servers[1];
      cleanOutServer(toClean);
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out a leader
////////////////////////////////////////////////////////////////////////////////

    testCleanOutLeader : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[0].name());
      var toClean = servers[0];
      cleanOutServer(toClean);
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testCleanOutMultipleCollections : function() {
      createSomeCollections(10, 1, 2);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[0];
      cleanOutServer(toClean);
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out with a collection with 3 replicas
////////////////////////////////////////////////////////////////////////////////

    testCleanOut3Replicas : function() {
      createSomeCollections(1, 1, 3);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[0];
      cleanOutServer(toClean);
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with multiple shards
////////////////////////////////////////////////////////////////////////////////

    testCleanOutMultipleShards : function() {
      createSomeCollections(1, 10, 2);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[1];
      cleanOutServer(toClean);
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with one shard without replication
////////////////////////////////////////////////////////////////////////////////

    testCleanOutNoReplication : function() {
      createSomeCollections(1, 1, 1);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[0];
      cleanOutServer(toClean);
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief just to allow a trailing comma at the end of the last test
////////////////////////////////////////////////////////////////////////////////

    testDummy : function () {
      assertEqual(12, 12);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(MovingShardsSuite);

return jsunity.done();

