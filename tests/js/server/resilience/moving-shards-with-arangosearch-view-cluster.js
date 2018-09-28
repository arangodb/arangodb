/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, ArangoAgency */

////////////////////////////////////////////////////////////////////////////////
/// @brief test moving shards in the cluster
///
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Vadim Kondratyev
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const db = arangodb.db;
const _ = require("lodash");
const internal = require("internal");
const wait = internal.wait;
const supervisionState = require("@arangodb/cluster").supervisionState;

function getDBServers() {
  var tmp = global.ArangoClusterInfo.getDBServers();
  var servers = [];
  for (var i = 0; i < tmp.length; ++i) {
    servers[i] = tmp[i].serverId;
  }
  return servers;
}

const servers = getDBServers();

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function MovingShardsWithViewSuite (options) {
  'use strict';
  var cn = "UnitTestMovingShards";
  var count = 0;
  var c = [], v = [];
  var dbservers = getDBServers();
  var useData = options.useData;

////////////////////////////////////////////////////////////////////////////////
/// @brief find out servers for a collection
////////////////////////////////////////////////////////////////////////////////

  function findCollectionServers(database, collection) {
    var cinfo = global.ArangoClusterInfo.getCollectionInfo(database, collection);
    var shard = Object.keys(cinfo.shards)[0];
    return cinfo.shards[shard];
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief find out servers for the shards of a collection
////////////////////////////////////////////////////////////////////////////////

  function findCollectionShardServers(database, collection, sort = true, unique = false) {
    var cinfo = global.ArangoClusterInfo.getCollectionInfo(database, collection);
    var sColServers = [];
    for(var shard in cinfo.shards) {
      sColServers.push(...cinfo.shards[shard]);
    }

    if (unique) sColServers = Array.from(new Set(sColServers));
    return sort ? sColServers.sort() : sColServers;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for synchronous replication
////////////////////////////////////////////////////////////////////////////////

  function waitForSynchronousReplication(database) {
    console.info("Waiting for synchronous replication to settle...");
    global.ArangoClusterInfo.flush();
    for (var i = 0; i < c.length; ++i) {
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
        database, c[i].name());
      var shards = Object.keys(cinfo.shards);
      var replFactor = cinfo.shards[shards[0]].length;
      var count = 0;
      while (++count <= 180) {
        var ccinfo = shards.map(
          s => global.ArangoClusterInfo.getCollectionInfoCurrent(
            database, c[i].name(), s)
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
    var coordEndpoint =
        global.ArangoClusterInfo.getServerEndpoint("Coordinator0001");

    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);

    var res;
    try {
      var envelope =
          { method: "GET", url: url + "/_admin/cluster/numberOfServers" };
      res = request(envelope);
    } catch (err) {
      console.error(
        "Exception for POST /_admin/cluster/cleanOutServer:", err.stack);
      return {};
    }
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
    var count = 600;
    var ok = false;

    console.info("Waiting for server " + id + " to be cleaned out ...");

    if (checkList) {

      // Wait until the server appears in the list of cleanedOutServers:
      var obj;
      while (--count > 0) {
        obj = getCleanedOutServers();
        if (obj.cleanedServers.indexOf(id) >= 0) {
          ok = true;
          console.info(
            "Success: Server " + id + " cleaned out after " + (600-count) + " seconds");
          break;
        }
        wait(1.0);
      }

      if (!ok) {
        console.info(
          "Failed: Server " + id + " was not cleaned out. List of cleaned servers: ["
            + obj.cleanedServers + "]");
      }

    } else {

      for (var i = fromCollNr; i <= toCollNr; ++i) {

        while (--count > 0) {
          wait(1.0);
          global.ArangoClusterInfo.flush();
          var servers = findCollectionServers("_system", c[i].name());
          if (servers.indexOf(id) === -1) {
            // Now check current as well:
            var collInfo =
                global.ArangoClusterInfo.getCollectionInfo("_system", c[i].name());
            var shards = collInfo.shards;
            var collInfoCurr =
                Object.keys(shards).map(
                  s => global.ArangoClusterInfo.getCollectionInfoCurrent(
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

    }
    return ok;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief order the cluster to clean out a server:
////////////////////////////////////////////////////////////////////////////////

  function cleanOutServer(id) {
    var coordEndpoint =
        global.ArangoClusterInfo.getServerEndpoint("Coordinator0001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var body = {"server": id};
    var result;
    try {
      result = request({ method: "POST",
                         url: url + "/_admin/cluster/cleanOutServer",
                         body: JSON.stringify(body) });
    } catch (err) {
      console.error(
        "Exception for POST /_admin/cluster/cleanOutServer:", err.stack);
      return false;
    }
    console.info("cleanOutServer job:", JSON.stringify(body));
    console.info("result of request:", JSON.stringify(result.json));
    // Now wait until the job we triggered is finished:
    var count = 1200;   // seconds
    while (true) {
      var job = require("@arangodb/cluster").queryAgencyJob(result.json.id);
      console.info("Status of cleanOutServer job:", job.status);
      if (job.error === false && job.status === "Finished") {
        return result;
      }
      if (count-- < 0) {
        console.error(
          "Timeout in waiting for cleanOutServer to complete: "
          + JSON.stringify(body));
        return false;
      }
      require("internal").wait(1.0);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief order the cluster to reduce number of db servers
////////////////////////////////////////////////////////////////////////////////

  function shrinkCluster(toNum) {
    var coordEndpoint =
        global.ArangoClusterInfo.getServerEndpoint("Coordinator0001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var body = {"numberOfDBServers":toNum};
    try {
      return request({ method: "PUT",
                       url: url + "/_admin/cluster/numberOfServers",
                       body: JSON.stringify(body) });
    } catch (err) {
      console.error(
        "Exception for PUT /_admin/cluster/numberOfServers:", err.stack);
      return false;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief order the cluster to clean out a server:
////////////////////////////////////////////////////////////////////////////////

  function resetCleanedOutServers() {
    var coordEndpoint =
        global.ArangoClusterInfo.getServerEndpoint("Coordinator0001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var numberOfDBServers = servers.length;
    var body = {"cleanedServers":[], "numberOfDBServers":numberOfDBServers};
    try {
      var res = request({ method: "PUT",
                          url: url + "/_admin/cluster/numberOfServers",
                          body: JSON.stringify(body) });
      return res;
    }
    catch (err) {
      console.error(
        "Exception for PUT /_admin/cluster/numberOfServers:", err.stack);
      return false;
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief move a single shard
////////////////////////////////////////////////////////////////////////////////

  function moveShard(database, collection, shard, fromServer, toServer, dontwait) {
    var coordEndpoint =
        global.ArangoClusterInfo.getServerEndpoint("Coordinator0001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var body = {database, collection, shard, fromServer, toServer};
    var result;
    try {
      result = request({ method: "POST",
                         url: url + "/_admin/cluster/moveShard",
                         body: JSON.stringify(body) });
    } catch (err) {
      console.error(
        "Exception for PUT /_admin/cluster/moveShard:", err.stack);
      return false;
    }
    if (dontwait) {
      return result;
    }
    console.info("moveShard job:", JSON.stringify(body));
    console.info("result of request:", JSON.stringify(result.json));
    // Now wait until the job we triggered is finished:
    var count = 600;   // seconds
    while (true) {
      var job = require("@arangodb/cluster").queryAgencyJob(result.json.id);
      console.info("Status of moveShard job:", job.status);
      if (job.error === false && job.status === "Finished") {
        return result;
      }
      if (count-- < 0) {
        console.error(
          "Timeout in waiting for moveShard to complete: "
          + JSON.stringify(body));
        return false;
      }
      require("internal").wait(1.0);
    }
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief Set supervision mode
////////////////////////////////////////////////////////////////////////////////

  function maintenanceMode(mode) {
    console.log("Switching supervision maintenance " + mode);
    var coordEndpoint =
        global.ArangoClusterInfo.getServerEndpoint("Coordinator0001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var req;
    try {
      req = request({ method: "PUT",
                      url: url + "/_admin/cluster/maintenance",
                      body: JSON.stringify(mode) });
    } catch (err) {
      console.error(
        "Exception for PUT /_admin/cluster/maintenance:", err.stack);
      return false;
    }
    console.log("Supervision maintenance is " + mode);
    return true;
  }



////////////////////////////////////////////////////////////////////////////////
/// @brief create some collections
////////////////////////////////////////////////////////////////////////////////

  function createSomeCollectionsWithView(n, nrShards, replFactor, withData = false) {
    var systemCollServers = findCollectionServers("_system", "_graphs");
    console.info("System collections use servers:", systemCollServers);
    for (var i = 0; i < n; ++i) {
      ++count;
      while (true) {
        var name = cn + count;
        db._drop(name);
        var coll = db._create(name, {numberOfShards: nrShards,
                                     replicationFactor: replFactor,
                                     avoidServers: systemCollServers});
        var servers = findCollectionServers("_system", name);
        console.info("Test collection uses servers:", servers);
        if (_.intersection(systemCollServers, servers).length === 0) {
          c.push(coll);
          var vname = "v" + name;
          try {
            db._view(vname).drop();
          } catch (ignored) {}
          var view = db._createView(vname, "arangosearch", {});
          view.properties(
            {links:
              {[name]:
                {fields:
                  {'a': { analyzers: ['identity']}, 'b': { analyzers: ['text_en']}}
                }
              }
            });
          v.push(view);
          if (withData) {
            for (let i = 0; i < 1000; ++i) {
              coll.save({a: i, b: "text" + i});
            }
          }
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
    var count = 0;
    while (list.indexOf(dbservers[count]) >= 0) {
      count += 1;
    }
    return dbservers[count];
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
      if (state.error) {
        console.warn("Waiting for supervision jobs to finish:",
                     "Currently no agency communication possible.");
      } else {
        console.info("Waiting for supervision jobs to finish:",
                     "ToDo jobs:", Object.keys(state.ToDo).length,
                     "Pending jobs:", Object.keys(state.Pending).length);
      }
      wait(1.0);
    }
    return false;
  }

  function waitForPlanEqualCurrent(collection) {
    const iterations = 120;
    const waitTime = 1.0;
    const maxTime = iterations * waitTime;

    for (let i = 0; i < iterations; i++) {
      global.ArangoClusterInfo.flush();
      const shardDist = internal.getCollectionShardDistribution(collection._id);
      const Plan = shardDist[collection.name()].Plan;
      const Current = shardDist[collection.name()].Current;

      if (_.isObject(Plan) && _.isEqual(Plan, Current)) {
        return true;
      }

      wait(waitTime);
    }

    console.error(`Collection "${collection}" failed to get plan in sync after ${maxTime} sec`);
    return false;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief get DBServers for a shard view of collection
////////////////////////////////////////////////////////////////////////////////

  function getShardedViewServers(database, collection, sort = true, unique = false) {
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var sViewServers = [];

    global.ArangoClusterInfo.flush();

    var cinfo = global.ArangoClusterInfo.getCollectionInfo(
        database, collection.name());
    for(var shard in cinfo.shards) {
      getDBServers().forEach(serverId => {
        var dbServerEndpoint = global.ArangoClusterInfo.getServerEndpoint(serverId);
        var url = endpointToURL(dbServerEndpoint);

        var res;
        try {
          var envelope =
              { method: "GET", url: url + "/_api/view" };
          res = request(envelope);
        } catch (err) {
          console.error(
            "Exception for GET /_api/view:", err.stack);
          return {};
        }
        var body = res.body;
        if (typeof body === "string") {
          var views = JSON.parse(body).result;
        }
        views.forEach(v => {
          var res;
          try {
            var envelope =
                { method: "GET", url: url + "/_api/view/" + v.name + "/properties" };
            res = request(envelope);
          } catch (err) {
            console.error(
              "Exception for GET /_api/view/" + v.name + "/properties:", err.stack);
            return {};
          }
          var body = res.body;
          if (typeof body === "string") {
            var links = JSON.parse(body).links;
            if (links !== undefined
                && links.hasOwnProperty(shard)
                && links.shard !== {}) {
              sViewServers.push(serverId);
            }
          }
        });
      });
    }

    if (unique) sViewServers = Array.from(new Set(sViewServers));
    return sort ? sViewServers.sort() : sViewServers;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual tests
////////////////////////////////////////////////////////////////////////////////

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      createSomeCollectionsWithView(1, 1, 2, useData);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      for (var i = 0; i < c.length; ++i) {
        v[i].drop();
        c[i].drop();
      }
      c = [];
      v = [];
      resetCleanedOutServers();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a synchronously replicated collection gets online
////////////////////////////////////////////////////////////////////////////////

    testSetup : function () {
      dbservers = getDBServers();
      assertTrue(waitForSynchronousReplication("_system"));
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with one shard without replication
////////////////////////////////////////////////////////////////////////////////

    testShrinkNoReplication : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var _dbservers = servers;
      _dbservers.sort();
      assertTrue(shrinkCluster(4));
      assertTrue(testServerEmpty(_dbservers[4], true));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
      assertTrue(shrinkCluster(3));
      assertTrue(testServerEmpty(_dbservers[3], true));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
      assertTrue(shrinkCluster(2));
      assertTrue(testServerEmpty(_dbservers[2], true));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
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
      assertTrue(moveShard("_system", c[0]._id, shard, fromServer, toServer, false));
      assertTrue(testServerEmpty(fromServer), false);
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
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
      assertTrue(moveShard("_system", c[0]._id, shard, fromServer, toServer, false));
      assertTrue(testServerEmpty(fromServer), false);
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader without replication
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderNoReplication : function() {
      createSomeCollectionsWithView(1, 1, 1, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower with 3 replicas #1
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromFollowerRepl3_1 : function() {
      createSomeCollectionsWithView(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[1];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower with 3 replicas #2
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromRepl3_2 : function() {
      createSomeCollectionsWithView(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[2];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader with 3 replicas
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderRepl : function() {
      createSomeCollectionsWithView(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out a follower
////////////////////////////////////////////////////////////////////////////////

    testCleanOutFollower : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[0].name());
      var toClean = servers[1];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out a leader
////////////////////////////////////////////////////////////////////////////////

    testCleanOutLeader : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[0].name());
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testCleanOutMultipleCollections : function() {
      createSomeCollectionsWithView(10, 1, 2, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out with a collection with 3 replicas
////////////////////////////////////////////////////////////////////////////////

    testCleanOut3Replicas : function() {
      createSomeCollectionsWithView(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with multiple shards
////////////////////////////////////////////////////////////////////////////////

    testCleanOutMultipleShards : function() {
      createSomeCollectionsWithView(1, 10, 2, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[1];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with one shard without replication
////////////////////////////////////////////////////////////////////////////////

    testCleanOutNoReplication : function() {
      createSomeCollectionsWithView(1, 1, 1, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief pausing supervision for a couple of seconds
////////////////////////////////////////////////////////////////////////////////

    testMaintenanceMode : function() {
      createSomeCollectionsWithView(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(maintenanceMode("on"));
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, true));
      var first = global.ArangoAgency.transient([["/arango/Supervision/State"]])[0].
          arango.Supervision.State, state;
      var waitUntil = new Date().getTime() + 30.0*1000;
      while(true) {
        state = global.ArangoAgency.transient([["/arango/Supervision/State"]])[0].
          arango.Supervision.State;
        assertEqual(state.Timestamp, first.Timestamp);
        wait(5.0);
        if (new Date().getTime() > waitUntil) {
          break;
        }
      }
      assertTrue(maintenanceMode("off"));
      state = global.ArangoAgency.transient([["/arango/Supervision/State"]])[0].
        arango.Supervision.State;
      assertTrue(state.Timestamp !== first.Timestamp);
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      c.forEach( c_v => {
        assertTrue(waitForPlanEqualCurrent(c_v));
        assertEqual(getShardedViewServers("_system", c_v),
                  findCollectionShardServers("_system", c_v.name()));
      });
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

jsunity.run(function MovingShardsWithViewSuite_nodata() {
  return MovingShardsWithViewSuite({ useData: false });
});

jsunity.run(function MovingShardsWithViewSuite_data() {
  return MovingShardsWithViewSuite({ useData: true });
});

return jsunity.done();
