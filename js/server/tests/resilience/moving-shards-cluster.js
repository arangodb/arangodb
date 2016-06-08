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


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function MovingShardsSuite () {
  'use strict';
  var cn = "UnitTestMovingShards";
  var c;
  var cinfo;
  var ccinfo;
  var shards;

////////////////////////////////////////////////////////////////////////////////
/// @brief find out servers for the system collections
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
    console.warn("Waiting for synchronous replication to settle...");
    global.ArangoClusterInfo.flush();
    cinfo = global.ArangoClusterInfo.getCollectionInfo(database, cn);
    shards = Object.keys(cinfo.shards);
    var count = 0;
    while (++count <= 120) {
      ccinfo = shards.map(
        s => global.ArangoClusterInfo.getCollectionInfoCurrent(database, cn, s)
      );
      let replicas = ccinfo.map(s => s.servers.length);
      if (_.all(replicas, x => x === 2)) {
        console.warn("Replication up and running!");
        return true;
      }
      wait(0.5);
      global.ArangoClusterInfo.flush();
    }
    return false;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test whether or not a server is clean
////////////////////////////////////////////////////////////////////////////////

  function testServerEmpty(id) {
    var count = 10;
    while (--count > 0) {
      wait(1.0);
      var servers = findCollectionServers("_system", cn);
      if (servers.indexOf(id) === -1) {
        // Now check current as well:
        var collInfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", cn);
        var shards = collInfo.shards;
        var collInfoCurr = Object.keys(shards).map(s =>
          global.ArangoClusterInfo.getCollectionInfoCurrent(
            "_system", cn, s).servers);
        var idxs = collInfoCurr.map(l => l.indexOf(id));
        var ok = true;
        for (var i = 0; i < idxs.length; i++) {
          if (idxs[i] !== -1) {
            ok = false;
          }
        }
        if (ok) {
          return true;
        }
      }
    }
    return false;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief order the cluster to clean out a server:
////////////////////////////////////////////////////////////////////////////////

  function cleanOutServer(id) {
    var coordEndpoint = global.ArangoClusterInfo.getServerEndpoint("Coordinator1");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var body = {"server": id};
    return request({ method: "POST",
                   url: url + "/_admin/cluster/cleanOutServer",
                   body: JSON.stringify(body) });
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual tests
////////////////////////////////////////////////////////////////////////////////

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var systemCollServers = findCollectionServers("_system", "_graphs");
      console.warn("System collections use servers:", systemCollServers);
      while (true) {
        db._drop(cn);
        c = db._create(cn, {numberOfShards: 1, replicationFactor: 2});
        var servers = findCollectionServers("_system", cn);
        console.warn("Test collections uses servers:", servers);
        if (_.intersection(systemCollServers, servers).length === 0) {
          return;
        }
        console.warn("Need to recreate collection to avoid system collection servers.");
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a synchronously replicated collection gets online
////////////////////////////////////////////////////////////////////////////////

    testSetup : function () {
      assertTrue(waitForSynchronousReplication("_system"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFollower : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", cn);
      var toClean = servers[1];
      cleanOutServer(toClean);
      assertTrue(testServerEmpty(toClean));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader
////////////////////////////////////////////////////////////////////////////////

    testMoveShardLeader : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", cn);
      var toClean = servers[0];
      cleanOutServer(toClean);
      assertTrue(testServerEmpty(toClean));
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

