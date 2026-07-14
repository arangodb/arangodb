/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, ArangoAgency, ArangoClusterInfo, GLOBAL, arango, print */
'use strict';

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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");

const arangodb = require("@arangodb");
const db = arangodb.db;
const _ = require("lodash");
const internal = require("internal");
const {errors, wait} = internal;
const supervisionState = require("@arangodb/testutils/cluster-test-helper").supervisionState;
const queryAgencyJob = require("@arangodb/testutils/cluster-test-helper").queryAgencyJob;
const CI = require('@arangodb/cluster-info');
const { deriveTestSuite, versionHas } = require('@arangodb/test-helper-common');
const isCov = versionHas('coverage');
const helper = require("@arangodb/testutils/replicated-logs-helper");
const lpreds = require("@arangodb/testutils/replicated-logs-predicates");
const {
  findCollectionServers
} = require('@arangodb/test-helper');
const { instanceRole } = require("@arangodb/testutils/instance");
const IM = GLOBAL.instanceManager;
const AM = IM.agencyMgr;

// in the `useData` case, use this many documents:
const numDocuments = 1000;

const dbservers = IM.getInstancesRole(instanceRole.dbserver);

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

exports.MovingShardsSuite = function({useData, replVersion}) {
  if (typeof useData !== 'boolean') {
    throw new Error('MovingShardsSuite expects its parameter `useData` to be set and a boolean!');
  }
  const cn = "UnitTestMovingShards";
  const dbn = "UnitTestMovingShards_V" + replVersion;
  var count = 0;
  var c = [];


////////////////////////////////////////////////////////////////////////////////
/// @brief wait for synchronous replication
////////////////////////////////////////////////////////////////////////////////

  function waitForSynchronousReplication(database) {
    print("Waiting for synchronous replication to settle...");
    CI.flush();
    for (var i = 0; i < c.length; ++i) {
      var cinfo = CI.getCollectionInfo(
          database, c[i].name());
      var shards = Object.keys(cinfo.shards);
      var replFactor = cinfo.shards[shards[0]].length;
      var count = 0;
      var maxCount = (isCov)? 1800 : 180;
      while (++count <= maxCount) {
        var ccinfo = shards.map(
            s => CI.getCollectionInfoCurrent(
                database, c[i].name(), s)
        );
        var replicas = ccinfo.map(s => [s.servers.length, s.failoverCandidates.length]);
        if (replicas.every(x => x[0] === replFactor && x[0] === x[1])) {
          // This also checks that there are as many failoverCandidates
          // as there are followers in sync. This should eventually be
          // reached.
          print("Replication up and running!");
          break;
        }
        wait(isCov ? 1.5 : 0.5);
        CI.flush();
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
    print("Waiting for incomplete move shard to settle...");
    CI.flush();
    var cinfo = CI.getCollectionInfo(database, collection);
    var shards = Object.keys(cinfo.shards);
    var count = 0;
    var maxCount = (isCov)? 1800 : 180;
    while (++count <= maxCount) {
      var ccinfo = shards.map(
          s => CI.getCollectionInfoCurrent(
              database, collection, s)
      );
      var replicas = ccinfo.map(s => [s.servers.length, s.failoverCandidates.length]);
      if (replicas.every(x => x[0] === replFactor + 1 && x[0] === x[1])) {
        // This also checks that there are as many failoverCandidates
        // as there are followers in sync. This should eventually be
        // reached.
        print("Incomplete moveShard completed!");
        break;
      }
      wait(isCov ? 1.5 : 0.5);
      CI.flush();
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
    var coord = IM.getInstancesRole(instanceRole.coordinator)[0];
    var result = coord.toThisInstance(() => {
      return arango.GET_RAW("/_admin/cluster/numberOfServers");
    });
    if (result.code !== 200) {
      print(
          "Error for GET /_admin/cluster/cleanOutServer:", result.parsedBody);
      return {cleanedServers: []};
    }
    var body = result.parsedBody;
    if (typeof body === "string") {
      try {
        body = JSON.parse(body);
      } catch (err2) {
      }
    }
    if (typeof body !== "object" || !body.hasOwnProperty("cleanedServers") ||
        typeof body.cleanedServers !== "object") {
      return {cleanedServers: []};
    }
    return body;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief display agency information in case of a bad outcome
////////////////////////////////////////////////////////////////////////////////

  function displayAgencyInformation() {
    var coord = IM.getInstancesRole(instanceRole.coordinator)[0];
    var result = coord.toThisInstance(() => {
      return arango.GET_RAW("/_api/cluster/agency-dump");
    });
    if (result.code !== 200) {
      return;
    }
    print("Agency state after disaster:", result.parsedBody);
  }

  function testServerNoLeader(dbServer, fromCollNr, toCollNr) {
    if (fromCollNr === undefined) {
      fromCollNr = 0;
    }
    if (toCollNr === undefined) {
      toCollNr = c.length - 1;
    }
    for (var i = fromCollNr; i <= toCollNr; ++i) {
      CI.flush();
      var servers = findCollectionServers(dbn, c[i].name(), replVersion);
      if (servers.indexOf(dbServer.id) === 0 && servers.length !== 1) {
        return false;
      }

      // Now check current as well:
      if (replVersion !== "2") {
        var collInfo =
            CI.getCollectionInfo(dbn, c[i].name());
        var shards = collInfo.shards;
        var collInfoCurr =
            Object.keys(shards).map(
                s => CI.getCollectionInfoCurrent(
                    dbn, c[i].name(), s).servers);
        // quick hack here, if the list of servers has length one, we map it to -1
        // thus the check below will not return false in the case replFact === 1
        var idxs = collInfoCurr.map(l => (l.length > 1) ? l.indexOf(dbServer.id) : -1);
        for (var j = 0; j < idxs.length; j++) {
          if (idxs[j] === 0) {
            return false;
          }
        }
      }
    }
    return true;
  }

  function findLeaderShardsForServer(id) {
    let result = [];
    for (var i = 0; i <  c.length ; ++i) {
      CI.flush();
      var servers = findCollectionServers(dbn, c[i].name(), replVersion);
      if (servers.indexOf(id) === 0 && servers.length !== 1) {
        result.push(c[i].name());
      }
    }
    return result;
  }

  function waitForShardLeader(id, expectedShards) {
    helper.waitFor(function () {
      const shards = findLeaderShardsForServer(id);
      if (!_.isEqual(shards, expectedShards)) {
        return Error(`expected shards to be ${expectedShards}, but found ${shards}`);
      }
      return true;
    });
    return true;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief test whether or not a server is clean
////////////////////////////////////////////////////////////////////////////////

  function testServerEmpty(cleanoutServer, checkList, fromCollNr, toCollNr) {
    if (fromCollNr === undefined) {
      fromCollNr = 0;
    }
    if (toCollNr === undefined) {
      toCollNr = c.length - 1;
    }
    var count = (isCov)? 12000 : 600;
    var ok = false;
    let searchID;
    let cleanoutCount;
    if (typeof(cleanoutServer) === 'number') {
      cleanoutCount = cleanoutServer;
    } else if (typeof(cleanoutServer) === 'string') {
      searchID = cleanoutServer;
    } else {
      searchID = cleanoutServer.id;
    }
    print(`Waiting for ${cleanoutCount} server(s) ${searchID} to be cleaned out ...`);

    if (checkList) {

      // Wait until the server appears in the list of cleanedOutServers:
      var obj;
      while (--count > 0) {
        obj = getCleanedOutServers();
        if (!searchID) {
          if (obj.cleanedServers.length === cleanoutCount) {
            ok = true;
            print(`Success: ${cleanoutCount} reached with ${obj}`);
            break;
          }
        } else if (obj.cleanedServers.indexOf(searchID) >= 0) {
          ok = true;
          print("Success: Server " + searchID + " cleaned out after " + (600 - count) + " seconds");
          break;
        }
        wait(isCov ? 1.5 : 1.0);
      }

      if (!ok) {
        print("Failed: Server " + searchID + " was not cleaned out. List of cleaned servers: ["
              + obj.cleanedServers + "]");
        displayAgencyInformation();
      }

    } else {

      for (var i = fromCollNr; i <= toCollNr; ++i) {

        while (--count > 0) {
          wait(isCov ? 1.5 : 1.0);
          CI.flush();
          var servers = findCollectionServers(dbn, c[i].name(), replVersion);
          if (servers.indexOf(searchID) === -1) {
            // Now check current as well:
            /*var collInfo =
                CI.getCollectionInfo(dbn, c[i].name());
            var shards = collInfo.shards;
            var collInfoCurr =
                Object.keys(shards).map(
                  s => CI.getCollectionInfoCurrent(
                    dbn, c[i].name(), s).servers);
            var idxs = collInfoCurr.map(l => l.indexOf(cleanoutServer.id));
            ok = true;
            for (var j = 0; j < idxs.length; j++) {
              if (idxs[j] !== -1) {
                ok = false;
              }
            }
            if (ok) {
              break;
            }*/
            ok = true;
            break;
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

  function cleanOutServer(id, dontwait = false) {
    var coord = IM.getInstancesRole(instanceRole.coordinator)[0];
    const body = {"server": id};
    print("cleanOutServer job:", JSON.stringify(body));
    var result = coord.toThisInstance(() => {
      return arango.POST_RAW("/_admin/cluster/cleanOutServer", body);
    });
    if (result.code !== 202) {
      print(
          "Exception for POST /_admin/cluster/cleanOutServer:", result.parsedBody);
      return false;
    }
    print("result of request:", JSON.stringify(result.parsedBody));
    if (dontwait) {
      return result;
    }
    // Now wait until the job we triggered is finished:
    var count = (isCov)? 24000 : 1200; // seconds
    return IM.waitForAgencyJob(
      result.parsedBody.id,
      count * 10,
      "waiting for cleanOutServer to complete",
      "Finished");
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief request a dbserver to resign:
////////////////////////////////////////////////////////////////////////////////

  function resignLeadership(id, undoMoves = false) {
    assertTrue(id !== undefined);
    var coord = IM.getInstancesRole(instanceRole.coordinator)[0];
    var body = {"server": id, undoMoves};
    var result = coord.toThisInstance(() => {
      return arango.POST_RAW("/_admin/cluster/resignLeadership", body);
    });
    if (result.code !== 202) {
      print("Error for POST /_admin/cluster/numberOfServers:", result.parsedBody);
      return false;
    }

    print("resignLeadership job:", JSON.stringify(body));
    print("result of request:", JSON.stringify(result));
    // Now wait until the job we triggered is finished:
    var count = (isCov)? 24000 : 1200; // seconds
    return IM.waitForAgencyJob(
      result.parsedBody.id,
      count * 10,
      "waiting for resignLeadership to complete");
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief order the cluster to reduce number of db servers
////////////////////////////////////////////////////////////////////////////////

  function shrinkCluster(toNum) {
    var coord = IM.getInstancesRole(instanceRole.coordinator)[0];
    return coord.toThisInstance(() => {
      var body = {"numberOfDBServers": toNum};
      let result = arango.PUT_RAW("/_admin/cluster/numberOfServers", body);
      if (result.code === 200) {
        return true;
      }
      print("Error for PUT /_admin/cluster/numberOfServers:", result.parsedBody);
      return false;
    });
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief order the cluster to clean out a server:
////////////////////////////////////////////////////////////////////////////////

  function resetCleanedOutServers() {
    var coord = IM.getInstancesRole(instanceRole.coordinator)[0];
    var numberOfDBServers = dbservers.length;
    var result = coord.toThisInstance(() => {
      var body = {"cleanedServers": [], "numberOfDBServers": numberOfDBServers};
      return arango.PUT_RAW("/_admin/cluster/numberOfServers", body);
    });
    if (result.code !== 200) {
      print(
        "Exception for PUT /_admin/cluster/numberOfServers:", result.parsedBody);
      return false;
    }
    // To ensure that all our favourite Coordinator0001 has actually
    // updated its agencyCache to reflect the newly empty CleanedServers
    // list, we ask him to create a collection with replicationFactor
    // numberOfDBServers. If this works, we can get rid of the collection
    // again and all is well. If it does not work, we have to wait a bit
    // longer:
    var maxCount = (isCov)? 1200 : 120;
    for (var count = 0; count < maxCount; ++count) {
      try {
        var c = db._create("collectionProbe", {replicationFactor: numberOfDBServers});
        c.drop();
        return result.parsedBody;
      } catch (err2) {
        if (err2.errorNum !== errors.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code) {
          print("Got unexpected error from collection probe: ", err2.stack);
          return false;
        }
      }
      print("Collection probe unsuccessful for replicationFactor=", numberOfDBServers, " count=", count);
      wait(isCov ? 1.5 : 0.5);
    }
    print("Could not successfully create collection probe for 60s, giving up!");
    return false;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief move a single shard
////////////////////////////////////////////////////////////////////////////////

  function moveShard(database, collection, shard, fromServer, toServer, dontwait, expectedResult) {
    var coord = IM.getInstancesRole(instanceRole.coordinator)[0];
    var body = {
      database,
      collection,
      shard,
      fromServer: (typeof(fromServer) === "string") ? fromServer : fromServer.id,
      toServer: (typeof(toServer) === "string") ? toServer : toServer.id
    };
    var result = coord.toThisInstance(() => {
      return arango.POST_RAW("/_admin/cluster/moveShard", body);
    });
    if (result.code !== 202) {
      print(
          "Exception for POST /_admin/cluster/moveShard:", result.parsedBody);
      return false;
    }
    print("moveShard job:", JSON.stringify(body));
    print("result of request:", JSON.stringify(result.parsedBody));
    if (dontwait) {
      return result;
    }
    // Now wait until the job we triggered is finished:
    var count = (isCov)? 18000 : 600; // seconds
    return IM.waitForAgencyJob(
      result.parsedBody.id,
      count * 10,
      "waiting for moveShard to complete",
      expectedResult);
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief Set supervision mode
////////////////////////////////////////////////////////////////////////////////

  function maintenanceMode(mode) {
    print("Switching supervision maintenance " + mode);
    var coord = IM.getInstancesRole(instanceRole.coordinator)[0];
    var result = coord.toThisInstance(() => {
      return arango.PUT_RAW("/_admin/cluster/maintenance", JSON.stringify(mode));
    });
    if (result.code !== 200) {
      print(
          "Exception for PUT /_admin/cluster/maintenance:", result.parsedBody);
      return false;
    }
    print("Supervision maintenance is " + mode);
    return true;
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief Abort job
////////////////////////////////////////////////////////////////////////////////

  function cancelAgencyJob(id) {
    print("Killing job " + id);
    var coord = IM.getInstancesRole(instanceRole.coordinator)[0];
    for (let i = 0; i < 10; i++) {
      var result = coord.toThisInstance(() => {
        return arango.POST_RAW("/_admin/cluster/cancelAgencyJob", {id: id});
      });

      if (result.parsedBody.error === false) {
        return true;
      } else if (result.code !== 404) {
        print("Failed to cancel job " + id + ": " + JSON.stringify(result));
        return false;
      } else {
        print("Job not yet detected by coordinator");
      }

      wait(isCov ? 1.5 : 1.0);
    }
    print("Failed to cancel job" + id + ", job not visible to coordinator.");
    return false;
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief create some collections
////////////////////////////////////////////////////////////////////////////////

  function createSomeCollections(n, nrShards, replFactor, useData, otherNumDocuments = 0) {
    assertEqual('boolean', typeof useData);
    var systemCollServers = findCollectionServers(dbn, "_graphs", replVersion);
    print("System collections use servers:", systemCollServers);
    for (var i = 0; i < n; ++i) {
      ++count;
      while (true) {
        var name = cn + count;
        db._drop(name);
        var coll = db._create(name, {
          numberOfShards: nrShards,
          replicationFactor: replFactor,
          avoidServers: systemCollServers
        });

        if (useData) {
          // insert some documents
          let nd = (otherNumDocuments === 0) ? numDocuments : otherNumDocuments;
          coll.insert(_.range(0, nd).map(v => ({value: v, x: "someString" + v})));
        }

        var servers = findCollectionServers(dbn, name, replVersion);
        print("Test collection uses servers:", servers);
        if (_.intersection(systemCollServers, servers).length === 0) {
          c.push(coll);
          break;
        }
        print("Need to recreate collection to avoid system collection servers.");
        c.push(coll);
        waitForSynchronousReplication(dbn);
        c.pop();
        print("Synchronous replication has settled, now dropping again.");
      }
    }
  }

  function checkCollectionContents(otherNumDocuments = 0) {
    if (replVersion === "2") {
      return; // TODO
    }
    let nd = (otherNumDocuments === 0) ? numDocuments : otherNumDocuments;
    const numDocs = useData ? nd : 0;
    for (const collection of c) {
      assertEqual(numDocs, collection.count());
      const values = db._query(
          'FOR doc IN @@col SORT doc.value RETURN doc.value',
          {'@col': collection.name()}).toArray();
      for (const v of _.range(0, numDocs)) {
        assertEqual(v, values[v], values);
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief find server not on a list
////////////////////////////////////////////////////////////////////////////////

  function findServerNotOnList(idList) {
    return dbservers.filter(arangod => {
      return idList.indexOf(arangod.id) < 0;
    })[0];
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for Supervision to finish jobs
////////////////////////////////////////////////////////////////////////////////

  function waitForSupervision() {
    var count = (isCov)? 1200 : 300; // seconds
    while (--count > 0) {
      var state = supervisionState();
      if (!state.error &&
          Object.keys(state.ToDo).length === 0 &&
          Object.keys(state.Pending).length === 0) {
        return true;
      }
      if (state.error) {
        print("Waiting for supervision jobs to finish:",
            "Currently no agency communication possible.");
      } else {
        print("Waiting for supervision jobs to finish:",
            "ToDo jobs:", Object.keys(state.ToDo).length,
            "Pending jobs:", Object.keys(state.Pending).length);
      }
      wait(isCov ? 1.5 : 1.0);
    }
    return false;
  }

  function waitForRebootIdChanged(server, oldRebootId) {
    var count = (isCov)? 1200 : 300; // seconds
    while(--count > 0) {
      if (AM.getServerRebootId(server) !== oldRebootId) {
        return;
      }
      wait(isCov ? 1.5 : 1.0);
    }
    assertTrue(count > 0);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual tests
////////////////////////////////////////////////////////////////////////////////

  return {

    setUpAll: function () {
      db._createDatabase(dbn, {replicationVersion: replVersion});
      db._useDatabase(dbn);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      createSomeCollections(1, 1, 2, useData);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      for (var i = 0; i < c.length; ++i) {
        c[i].drop();
      }
      c = [];
      resetCleanedOutServers();
      IM.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a synchronously replicated collection gets online
////////////////////////////////////////////////////////////////////////////////

    testSetup: function () {
      for (var count = 0; count < 120; ++count) {
        if (dbservers.length === 5) {
          assertTrue(waitForSynchronousReplication(dbn));
          checkCollectionContents();

          return;
        }
        print("Waiting for 5 dbservers to be present:", JSON.stringify(dbservers));
        wait(isCov ? 1.5 : 1.0);
      }
      assertTrue(false, "Timeout waiting for 5 dbservers.");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with one shard without replication
////////////////////////////////////////////////////////////////////////////////

    testShrinkNoReplication: function () {
      assertTrue(waitForSynchronousReplication(dbn));
      var _dbservers = _.clone(dbservers);
      _dbservers.sort();
      assertTrue(shrinkCluster(4));
      assertTrue(testServerEmpty(1, true));
      assertTrue(waitForSupervision());
      assertTrue(shrinkCluster(3));
      assertTrue(testServerEmpty(2, true));
      assertTrue(waitForSupervision());
      assertTrue(shrinkCluster(2));
      assertTrue(testServerEmpty(3, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromFollower: function () {
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[0].name(), replVersion);
      var fromServer = servers[1];
      var toServer = findServerNotOnList(servers);
      var cinfo = CI.getCollectionInfo(
          dbn, c[0].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard(dbn, c[0]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer), false);
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeader: function () {
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[0].name(), replVersion);
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = CI.getCollectionInfo(
          dbn, c[0].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard(dbn, c[0]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer), false);
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader without replication
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderNoReplication: function () {
      createSomeCollections(1, 1, 1, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = CI.getCollectionInfo(
          dbn, c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard(dbn, c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower with 3 replicas #1
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromFollowerRepl3_1: function () {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var fromServer = servers[1];
      var toServer = findServerNotOnList(servers);
      var cinfo = CI.getCollectionInfo(
          dbn, c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard(dbn, c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a follower with 3 replicas #2
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromRepl3_2: function () {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var fromServer = servers[2];
      var toServer = findServerNotOnList(servers);
      var cinfo = CI.getCollectionInfo(
          dbn, c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard(dbn, c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader with 3 replicas
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderRepl: function () {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = CI.getCollectionInfo(
          dbn, c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard(dbn, c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief moving away a shard from a leader to a follower
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromLeaderToFollower: function () {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var fromServer = servers[0];
      var toServer = servers[1];
      var cinfo = CI.getCollectionInfo(
          dbn, c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(moveShard(dbn, c[1]._id, shard, fromServer, toServer, false, "Finished"));
      assertTrue(testServerNoLeader(fromServer, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief resign leadership for a dbserver
////////////////////////////////////////////////////////////////////////////////

    testResignLeadership: function () {
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[0].name(), replVersion);
      var toResign = servers[0];
      assertTrue(resignLeadership(toResign));
      assertTrue(testServerNoLeader(toResign));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief resign leadership for a dbserver and wait for restore
////////////////////////////////////////////////////////////////////////////////

    testResignLeadershipWithUndo: function () {
      assertTrue(waitForSynchronousReplication(dbn));
      try {
        AM.delaySupervisionFailoverActions(120);

        var servers = findCollectionServers(dbn, c[0].name(), replVersion);
        var toResign = servers[0];
        const shards = findLeaderShardsForServer(toResign);
        assertTrue(resignLeadership(toResign, true));
        assertTrue(testServerNoLeader(toResign));
        assertTrue(waitForSupervision());

        checkCollectionContents();

        // get server reboot id
        const rebootId = AM.getServerRebootId(toResign);

        // now suspend that server
        IM.stopServerWaitFailed(toResign);
        // Wait for the reboot id to be changed.
        waitForRebootIdChanged(toResign, rebootId);

        // restart the server
        IM.continueServerWaitOk(toResign);

        // now wait for the server to become leader again for shards
        assertTrue(waitForShardLeader(toResign, shards));
        assertTrue(waitForSupervision());
        checkCollectionContents();
      } finally {
        AM.delaySupervisionFailoverActions(0);
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out a follower
////////////////////////////////////////////////////////////////////////////////

    testCleanOutFollower: function () {
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[0].name(), replVersion);
      var toClean = servers[1];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out a leader
////////////////////////////////////////////////////////////////////////////////

    testCleanOutLeader: function () {
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[0].name(), replVersion);
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out with multiple collections
////////////////////////////////////////////////////////////////////////////////

    testCleanOutMultipleCollections: function () {
      createSomeCollections(10, 1, 2, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out with a collection with 3 replicas
////////////////////////////////////////////////////////////////////////////////

    testCleanOut3Replicas: function () {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with multiple shards
////////////////////////////////////////////////////////////////////////////////

    testCleanOutMultipleShards: function () {
      createSomeCollections(1, 10, 2, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var toClean = servers[1];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cleaning out collection with one shard without replication
////////////////////////////////////////////////////////////////////////////////

    testCleanOutNoReplication: function () {
      createSomeCollections(1, 1, 1, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var toClean = servers[0];
      assertTrue(cleanOutServer(toClean));
      assertTrue(testServerEmpty(toClean, true));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief pausing supervision for a couple of seconds
////////////////////////////////////////////////////////////////////////////////

    testMaintenanceMode: function () {
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = CI.getCollectionInfo(
          dbn, c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(maintenanceMode("on"));
      assertTrue(moveShard(dbn, c[1]._id, shard, fromServer, toServer, true, "Finished"));
      var first = AM.call("transient", [["/arango/Supervision/State"]])[0].arango.Supervision.State, state;
      var waitUntil = new Date().getTime() + 30.0 * 1000;
      while (true) {
        state = AM.call("transient", [["/arango/Supervision/State"]])[0].arango.Supervision.State;
        assertEqual(state.Timestamp, first.Timestamp);
        wait(isCov ? 1.5 : 5.0);
        if (new Date().getTime() > waitUntil) {
          break;
        }
      }
      assertTrue(maintenanceMode("off"));
      state = AM.call("transient", [["/arango/Supervision/State"]])[0].arango.Supervision.State;
      assertTrue(state.Timestamp !== first.Timestamp);
      assertTrue(testServerEmpty(fromServer, false, 1, 1));
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief pausing supervision for a couple of seconds
////////////////////////////////////////////////////////////////////////////////

    testMoveShardFromFollowerRepl3_failoverCands: function () {
      if (!IM.debugCanUseFailAt()) {
        print("Skipping test for failoverCandidates because failure points are not compiled in!");
        return;
      }
      if (replVersion === "2") {
        print("Skipping failure point test for replication 2, because there is no such failure point");
        return;
      }
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var leaderID = servers[0];
      var leader = IM.getInstanceByID(leaderID);
      var fromServerID = servers[1];
      // Switch off something in the maintenance on the leader to detect
      // followers which are not in Plan. This means that the moveShard
      // below will leave the old server in Current/servers and
      // Current/failoverCandidates.
      try {
        leader.debugSetFailAt("Maintenance::doNotRemoveUnPlannedFollowers");

        var toServer = findServerNotOnList(servers);
        var cinfo = CI.getCollectionInfo(
            dbn, c[1].name());
        var shard = Object.keys(cinfo.shards)[0];
        assertTrue(moveShard(dbn, c[1]._id, shard, fromServerID, toServer, false, "Finished"));
        assertTrue(waitForIncompleteMoveShard(dbn, c[1].name(), 3));
        wait(5);   // After 5 seconds the situation should be unchanged!
        assertTrue(waitForIncompleteMoveShard(dbn, c[1].name(), 3));
        // Now we know that the old follower is not in the plan but is in
        // failoverCandidates (and indeed in Current/servers). Let's now
        // try to move the shard back, this ought to be denied:
        assertTrue(moveShard(dbn, c[1]._id, shard, toServer, fromServerID, false, "Failed"));
        IM.debugClearFailAt();
        // Now we should go back to only 3 servers in Current.
        assertTrue(waitForSynchronousReplication(dbn));
        assertTrue(testServerEmpty(fromServerID, false, 1, 1));
        assertTrue(waitForSupervision());
        checkCollectionContents();
      } catch (err) {
        IM.debugClearFailAt();
        throw err;
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief kill todo moveShard job
////////////////////////////////////////////////////////////////////////////////

    testCancelToDoMoveShard: function () {
      if (replVersion === "2") {
        print("Skipping cancel test for replication 2, because move shards are not cancellable.");
        return;
      }
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var fromServer = servers[0];
      var toServer = findServerNotOnList(servers);
      var cinfo = CI.getCollectionInfo(
          dbn, c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(maintenanceMode("on"));
      let result = moveShard(dbn, c[1]._id, shard, fromServer, toServer, true, "Finished");
      assertEqual(result.code, 202);
      let id = result.parsedBody.id;
      assertTrue(cancelAgencyJob(id));
      assertTrue(maintenanceMode("off"));
      assertTrue(waitForSupervision());
      var job = queryAgencyJob(result.parsedBody.id);
      assertEqual(job.status, "Failed");
      assertEqual(job.abort);
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief kill pending moveShard job
////////////////////////////////////////////////////////////////////////////////

    testCancelPendingMoveShard: function () {
      if (replVersion === "2") {
        print("Skipping cancel test for replication 2, because move shards are not cancellable.");
        return;
      }
      if (useData) {
        for (var i = 0; i < c.length; ++i) {
          c[i].drop();
        }
        c = [];
        let otherNumDocuments = 100000;
        createSomeCollections(1, 1, 3, useData, otherNumDocuments);
        assertTrue(waitForSynchronousReplication(dbn));
        var servers = findCollectionServers(dbn, c[0].name(), replVersion);
        var fromServer = servers[0];
        var toServer = findServerNotOnList(servers);
        var cinfo = CI.getCollectionInfo(
            dbn, c[0].name());
        var shard = Object.keys(cinfo.shards)[0];
        let result = moveShard(dbn, c[0]._id, shard, fromServer, toServer, true, "Finished");
        assertEqual(result.code, 202);
        let id = result.parsedBody.id;
        while (queryAgencyJob(result.parsedBody.id).status === "ToDo") { // wait for job to start
          wait(isCov ? 1 : 0.1);
        }
        assertTrue(cancelAgencyJob(id));
        while (queryAgencyJob(result.parsedBody.id).status === "Pending") { // wait for job to be killed
          wait(isCov ? 1 : 0.1);
        }
        var job = queryAgencyJob(result.parsedBody.id);
        assertEqual(job.status, "Failed");
        assertEqual(job.abort);
        assertTrue(waitForSupervision());
        checkCollectionContents(otherNumDocuments);
      }
    },

    testCancelPendingMoveShardDropCollection: function () {
      if (replVersion === "2") {
        print("Skipping cancel test for replication 2, because move shards are not cancellable.");
        return;
      }
      if (useData) {
        for (var i = 0; i < c.length; ++i) {
          c[i].drop();
        }
        c = [];
        let otherNumDocuments = 100000;
        createSomeCollections(1, 1, 3, useData, otherNumDocuments);
        assertTrue(waitForSynchronousReplication(dbn));
        var servers = findCollectionServers(dbn, c[0].name(), replVersion);
        var fromServer = servers[0];
        var toServer = findServerNotOnList(servers);
        var cinfo = CI.getCollectionInfo(
            dbn, c[0].name());
        var shard = Object.keys(cinfo.shards)[0];
        let result = moveShard(dbn, c[0]._id, shard, fromServer, toServer, true, "Finished");
        assertEqual(result.code, 202);
        let id = result.parsedBody.id;
        while (queryAgencyJob(result.parsedBody.id).status === "ToDo") { // wait for job to start
          wait(isCov ? 0.5 : 0.1);
        }
        assertTrue(maintenanceMode("on"));
        assertEqual(queryAgencyJob(result.parsedBody.id).status, "Pending");
        assertTrue(cancelAgencyJob(id));
        for (var j = 0; j < c.length; ++j) {
          c[j].drop();
        }
        c = [];
        assertTrue(maintenanceMode("off"));
        while (queryAgencyJob(result.parsedBody.id).status === "Finish") { // wait for job to finish
          wait(isCov ? 2 : 0.1);
        }
        var job = queryAgencyJob(result.parsedBody.id);
        assertEqual(job.status, "Failed");
        assertEqual(job.abort);
        assertTrue(waitForSupervision());
        checkCollectionContents(otherNumDocuments);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief kill todo cleanOutServer job
////////////////////////////////////////////////////////////////////////////////

    testCancelToDoCleanOutServer: function () {
      if (replVersion === "2") {
        print("Skipping cancel test for replication 2, because move shards are not cancellable.");
        return;
      }
      createSomeCollections(1, 1, 3, useData);
      assertTrue(waitForSynchronousReplication(dbn));
      var servers = findCollectionServers(dbn, c[1].name(), replVersion);
      var fromServer = servers[0];
      var cinfo = CI.getCollectionInfo(
          dbn, c[1].name());
      var shard = Object.keys(cinfo.shards)[0];
      assertTrue(maintenanceMode("on"));
      let result = cleanOutServer(servers[0], true);
      assertEqual(result.code, 202);
      let id = result.parsedBody.id;
      assertTrue(cancelAgencyJob(id));
      assertTrue(maintenanceMode("off"));
      while (queryAgencyJob(result.parsedBody.id).status === "ToDo") { // wait for job to be killed
        wait(isCov ? 2 : 0.1);
      }
      var job = queryAgencyJob(result.parsedBody.id);
      assertEqual(job.status, "Failed");
      assertEqual(job.abort);
      assertTrue(waitForSupervision());
      checkCollectionContents();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief kill pending moveShard job
////////////////////////////////////////////////////////////////////////////////

    testCancelPendingCleanOutServer: function () {
      if (replVersion === "2") {
        print("Skipping cancel test for replication 2, because move shards are not cancellable.");
        return;
      }
      if (useData) {
        for (var i = 0; i < c.length; ++i) {
          c[i].drop();
        }
        c = [];
        let otherNumDocuments = 100000;
        createSomeCollections(1, 1, 3, useData, otherNumDocuments);
        assertTrue(waitForSynchronousReplication(dbn));
        var servers = findCollectionServers(dbn, c[0].name(), replVersion);
        var fromServer = servers[0];
        var cinfo = CI.getCollectionInfo(
            dbn, c[0].name());
        var shard = Object.keys(cinfo.shards)[0];
        let result = cleanOutServer(fromServer, true);
        assertEqual(result.code, 202);
        let id = result.parsedBody.id;
        while (queryAgencyJob(result.parsedBody.id).status === "ToDo") { // wait for job to start
          wait(isCov ? 2 : 0.1);
        }
        assertTrue(cancelAgencyJob(id));
        while (queryAgencyJob(result.parsedBody.id).status === "Pending") { // wait for job to be killed
          wait(isCov ? 2 : 0.1);
        }
        var job = queryAgencyJob(result.parsedBody.id);
        assertEqual(job.status, "Failed");
        assertEqual(job.abort);
        assertTrue(waitForSupervision());
        checkCollectionContents(otherNumDocuments);
      }
    },

  };
};
