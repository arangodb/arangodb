/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, ArangoAgency, ArangoClusterInfo */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief test moving shards in the cluster
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
const internal = require("internal");
const wait = internal.wait;
const supervisionState = require("@arangodb/cluster").supervisionState;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const errors = internal.errors;
const request = require('@arangodb/request');

// in the `useData` case, use this many documents:
const numDocuments = 1000;

function getDBServers() {
  var tmp = global.ArangoClusterInfo.getDBServers();
  var servers = [];
  for (var i = 0; i < tmp.length; ++i) {
    servers[i] = tmp[i].serverId;
  }
  return servers;
}

var servers = getDBServers();

function getEndpointById(id) {
  const endpointToURL = (endpoint) => {
    if (endpoint.substr(0, 6) === 'ssl://') {
      return 'https://' + endpoint.substr(6);
    }
    let pos = endpoint.indexOf('://');
    if (pos === -1) {
      return 'http://' + endpoint;
    }
    return 'http' + endpoint.substr(pos);
  };

  const endpoint = ArangoClusterInfo.getServerEndpoint(id);
  return endpointToURL(endpoint);
}

/// @brief set failure point
function debugSetFailAt(endpoint, failAt) {
  let res = request.put({
    url: endpoint + '/_admin/debug/failat/' + failAt,
    body: ""
  });
  if (res.status !== 200) {
    throw "Error setting failure point";
  }
}

/// @brief remove failure points
function debugClearFailAt(endpoint) {
  let res = request.delete({
    url: endpoint + '/_admin/debug/failat',
    body: ""
  });
  if (res.status !== 200) {
    throw "Error removing failure points";
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function MovingShardsSuite ({useData}) {
  if (typeof useData !== 'boolean') {
    throw new Error('MovingShardsSuite expects its parameter `useData` to be set and a boolean!');
  }
  var cn = "UnitTestMovingShards";
  var count = 0;
  var c = [];
  var dbservers = getDBServers();

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
        var replicas = ccinfo.map(s => [s.servers.length, s.failoverCandidates.length]);
        if (replicas.every(x => x[0] === replFactor && x[0] === x[1])) {
          // This also checks that there are as many failoverCandidates
          // as there are followers in sync. This should eventually be
          // reached.
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
/// @brief wait for an incomplete moveShard to have happened, this is a
/// very special test for "testMoveShardFromFollowerRepl3_failoverCands".
////////////////////////////////////////////////////////////////////////////////

  function waitForIncompleteMoveShard(database, collection, replFactor) {
    console.info("Waiting for incomplete move shard to settle...");
    global.ArangoClusterInfo.flush();
    var cinfo = global.ArangoClusterInfo.getCollectionInfo( database, collection);
    var shards = Object.keys(cinfo.shards);
    var count = 0;
    while (++count <= 180) {
      var ccinfo = shards.map(
        s => global.ArangoClusterInfo.getCollectionInfoCurrent(
          database, collection, s)
      );
      var replicas = ccinfo.map(s => [s.servers.length, s.failoverCandidates.length]);
      if (replicas.every(x => x[0] === replFactor + 1 && x[0] === x[1])) {
        // This also checks that there are as many failoverCandidates
        // as there are followers in sync. This should eventually be
        // reached.
        console.info("Incomplete moveShard completed!");
        break;
      }
      wait(0.5);
      global.ArangoClusterInfo.flush();
    }
    if (count > 120) {
      return false;
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
        "Exception for GET /_admin/cluster/cleanOutServer:", err.stack);
      return {cleanedServers:[]};
    }
    if (res.statusCode !== 200) {
      return {cleanedServers:[]};
    }
    var body = res.body;
    if (typeof body === "string") {
      try {
        body = JSON.parse(body);
      } catch (err2) {
      }
    }
    if (typeof body !== "object" || !body.hasOwnProperty("cleanedServers") ||
        typeof body.cleanedServers !== "object") {
      return {cleanedServers:[]};
    }
    return body;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief display agency information in case of a bad outcome
////////////////////////////////////////////////////////////////////////////////

  function displayAgencyInformation() {
    var coordEndpoint =
        global.ArangoClusterInfo.getServerEndpoint("Coordinator0001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);

    var res;
    try {
      var envelope = { method: "GET", url: url + "/_api/cluster/agency-dump" };
      res = request(envelope);
    } catch (err) {
      console.error(
        "Exception for GET /_api/cluster/agency-dump:", err.stack);
      return;
    }
    if (res.statusCode !== 200) {
      return;
    }
    var body = res.body;
    console.error("Agency state after disaster:", body);
  }

  function testServerNoLeader(id, fromCollNr, toCollNr) {
    if (fromCollNr === undefined) {
      fromCollNr = 0;
    }
    if (toCollNr === undefined) {
      toCollNr = c.length - 1;
    }
    for (var i = fromCollNr; i <= toCollNr; ++i) {
      global.ArangoClusterInfo.flush();
      var servers = findCollectionServers("_system", c[i].name());
      if (servers.indexOf(id) === 0 && servers.length !== 1) {
        return false;
      }

      // Now check current as well:
      var collInfo =
          global.ArangoClusterInfo.getCollectionInfo("_system", c[i].name());
      var shards = collInfo.shards;
      var collInfoCurr =
          Object.keys(shards).map(
            s => global.ArangoClusterInfo.getCollectionInfoCurrent(
              "_system", c[i].name(), s).servers);
      // quick hack here, if the list of servers has length one, we map it to -1
      // thus the check below will not return false in the case replFact === 1
      var idxs = collInfoCurr.map(l => (l.length > 1) ? l.indexOf(id) : -1);
      for (var j = 0; j < idxs.length; j++) {
        if (idxs[j] === 0) {
          return false;
        }
      }
    }
    return true;
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
        displayAgencyInformation();
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
          displayAgencyInformation();
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
      wait(1.0);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief request a dbserver to resign:
////////////////////////////////////////////////////////////////////////////////

  function resignLeadership(id) {
    var coordEndpoint =
        global.ArangoClusterInfo.getServerEndpoint("Coordinator0001");
    var request = require("@arangodb/request");
    var endpointToURL = require("@arangodb/cluster").endpointToURL;
    var url = endpointToURL(coordEndpoint);
    var body = {"server": id};
    var result;
    try {
      result = request({ method: "POST",
                         url: url + "/_admin/cluster/resignLeadership",
                         body: JSON.stringify(body) });
    } catch (err) {
      console.error(
        "Exception for POST /_admin/cluster/resignLeadership:", err.stack);
      return false;
    }
    console.info("resignLeadership job:", JSON.stringify(body));
    console.info("result of request:", JSON.stringify(result.json));
    // Now wait until the job we triggered is finished:
    var count = 1200;   // seconds
    while (true) {
      var job = require("@arangodb/cluster").queryAgencyJob(result.json.id);
      console.info("Status of resignLeadership job:", job.status);
      if (job.error === false && job.status === "Finished") {
        return result;
      }
      if (count-- < 0) {
        console.error(
          "Timeout in waiting for resignLeadership to complete: "
          + JSON.stringify(body));
        return false;
      }
      wait(1.0);
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
    let res = {};
    try {
      res = request({ method: "PUT",
                      url: url + "/_admin/cluster/numberOfServers",
                      body: JSON.stringify(body) });
    } catch (err) {
      console.error(
        "Exception for PUT /_admin/cluster/numberOfServers:", err.stack);
      return false;
    }
    return res.hasOwnProperty("statusCode") && res.statusCode === 200;
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
      // To ensure that all our favourite Coordinator0001 has actually
      // updated its agencyCache to reflect the newly empty CleanedServers
      // list, we ask him to create a collection with replicationFactor
      // numberOfDBServers. If this works, we can get rid of the collection
      // again and all is well. If it does not work, we have to wait a bit
      // longer:
      for (var count = 0; count < 120; ++count) {
        try {
          var c = db._create("collectionProbe", {replicationFactor:numberOfDBServers});
          c.drop();
          return res;
        } catch(err2) {
          if (err2.errorNum !== errors.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code) {
            console.error("Got unexpected error from collection probe: ", err2.stack);
            return false;
          }
        }
        console.log("Collection probe unsuccessful for replicationFactor=", numberOfDBServers, " count=", count);
        wait(0.5);
      }
      console.error("Could not successfully create collection probe for 60s, giving up!");
      return false;
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

  function moveShard(database, collection, shard, fromServer, toServer, dontwait, expectedResult) {
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
      if (job.error === false && job.status === expectedResult) {
        return result;
      }
      if (count-- < 0) {
        console.error(
          "Timeout in waiting for moveShard to complete: "
          + JSON.stringify(body));
        return false;
      }
      wait(1.0);
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

  function createSomeCollections(n, nrShards, replFactor, useData) {
    assertEqual('boolean', typeof useData);
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

        if (useData) {
          // insert some documents
          coll.insert(_.range(0, numDocuments).map(v => ({ value: v, x: "someString" + v })));
        }

        var servers = findCollectionServers("_system", name);
        console.info("Test collection uses servers:", servers);
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

  function checkCollectionContents() {
    const numDocs = useData ? numDocuments : 0;
    for(const collection of c) {
      assertEqual(numDocs, collection.count());
      const values = db._query(
        'FOR doc IN @@col SORT doc.value RETURN doc.value',
        { '@col': collection.name() }).toArray();
      for (const v of _.range(0, numDocs)) {
        assertEqual(v, values[v], values);
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

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual tests
////////////////////////////////////////////////////////////////////////////////

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      createSomeCollections(1, 1, 2, useData);
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
      for (var count = 0; count < 120; ++count) {
        dbservers = getDBServers();
        if (dbservers.length === 5) {
          assertTrue(waitForSynchronousReplication("_system"));
          checkCollectionContents();

          servers = dbservers;
          return;
        }
        console.log("Waiting for 5 dbservers to be present:", JSON.stringify(dbservers));
        wait(1.0);
      }
      assertTrue(false, "Timeout waiting for 5 dbservers.");
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
      assertTrue(shrinkCluster(3));
      assertTrue(testServerEmpty(_dbservers[3], true));
      assertTrue(waitForSupervision());
      assertTrue(shrinkCluster(2));
      assertTrue(testServerEmpty(_dbservers[2], true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
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
      assertTrue(moveShard("_system", c[0]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer), false);
      assertTrue(waitForSupervision());
      checkCollectionContents();
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
      assertTrue(moveShard("_system", c[0]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer), false);
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader without replication
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderNoReplication : function() {
      createSomeCollections(1, 1, 1, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower with 3 replicas #1
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromFollowerRepl3_1 : function() {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[1];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower with 3 replicas #2
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromRepl3_2 : function() {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[2];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader with 3 replicas
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderRepl : function() {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader to a follower
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderToFollower : function() {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[0];
      var toServer = servers[1];
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerNoLeader(fromServer, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief resign leadership for a dbserver
////////////////////////////////////////////////////////////////////////////////

    testResignLeadership : function() {
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[0].name());
      var toResign = servers[1];
      assertTrue(resignLeadership(toResign));
      assertTrue(testServerNoLeader(toResign));
      assertTrue(waitForSupervision());
      checkCollectionContents();
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
      checkCollectionContents();
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
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testCleanOutMultipleCollections : function() {
      createSomeCollections(10, 1, 2, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out with a collection with 3 replicas
////////////////////////////////////////////////////////////////////////////////

    testCleanOut3Replicas : function() {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with multiple shards
////////////////////////////////////////////////////////////////////////////////

    testCleanOutMultipleShards : function() {
      createSomeCollections(1, 10, 2, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[1];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with one shard without replication
////////////////////////////////////////////////////////////////////////////////

    testCleanOutNoReplication : function() {
      createSomeCollections(1, 1, 1, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief pausing supervision for a couple of seconds
////////////////////////////////////////////////////////////////////////////////

    testMaintenanceMode : function() {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(maintenanceMode("on"));
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, true, "Finished"));
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
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief pausing supervision for a couple of seconds
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromFollowerRepl3_failoverCands : function() {
      if (!internal.debugCanUseFailAt()) {
        console.log("Skipping test for failoverCandidates because failure points are not compiled in!");
        return;
      }
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication("_system"));
      var servers = findCollectionServers("_system", c[1].name());
      var leader = servers[0];
      var fromServer = servers[1];
      var leaderEndpoint = getEndpointById(leader);
      // Switch off something in the maintenance on the leader to detect
      // followers which are not in Plan. This means that the moveShard
      // below will leave the old server in Current/servers and
      // Current/failoverCandidates.
      debugSetFailAt(leaderEndpoint, "Maintenance::doNotRemoveUnPlannedFollowers");

      var toServer = findServerNotOnList(servers);
      var cinfo = global.ArangoClusterInfo.getCollectionInfo(
          "_system", c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard("_system", c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(waitForIncompleteMoveShard("_system", c[1].name(), 3));
      wait(5);   // After 5 seconds the situation should be unchanged!
      assertTrue(waitForIncompleteMoveShard("_system", c[1].name(), 3));
      // Now we know that the old follower is not in the plan but is in
      // failoverCandidates (and indeed in Current/servers). Let's now
      // try to move the shard back, this ought to be denied:
      assertTrue(moveShard("_system", c[1]._id, shard, toServer, fromServer, false, "Failed"));
      debugClearFailAt(leaderEndpoint);
      // Now we should go back to only 3 servers in Current.
      assertTrue(waitForSynchronousReplication("_system"));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(function MovingShardsSuite_nodata() {
  let derivedSuite = {};
  deriveTestSuite(MovingShardsSuite({ useData: false }), derivedSuite, "_nodata");
  return derivedSuite;
});

jsunity.run(function MovingShardsSuite_data() {
  let derivedSuite = {};
  deriveTestSuite(MovingShardsSuite({ useData: true }), derivedSuite, "_data");
  return derivedSuite;
});

return jsunity.done();
