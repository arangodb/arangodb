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
const request = require("@arangodb/request");
const cluster = require("@arangodb/cluster");
const supervisionState = require("@arangodb/testutils/cluster-test-helper").supervisionState;
const queryAgencyJob = require("@arangodb/testutils/cluster-test-helper").queryAgencyJob;
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;

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
/// @brief returns "{shardXY: [serverAB, ...], ...}" object of the given
/// collection from the plan
////////////////////////////////////////////////////////////////////////////////

  function collectionShardInfo(database, collection) {
    if (typeof collection !== "string") {
      throw new Error(`argument is not a string: ${collection}`);
    }
    var cinfo = global.ArangoClusterInfo.getCollectionInfo(database, collection);
    return cinfo.shards;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns "{shardXY: serverAB, ...}", with the leader of each shard
/// from the plan
////////////////////////////////////////////////////////////////////////////////

  function shardLeaders(database, collection) {
    const shardInfo = collectionShardInfo(database, collection);

    return Object.keys(shardInfo).reduce(function(result, shard) {
      const servers = shardInfo[shard];
      assertTrue(servers.length > 0);
      result[shard] = servers[0];
      return result;
    }, {});
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief find out servers for a collection
////////////////////////////////////////////////////////////////////////////////

  function findCollectionServers(database, collection) {
    const shardInfo = collectionShardInfo(database, collection);

    const servers = _.flatten(Object.values(shardInfo)).reduce(
      (servers, server) => servers.add(server),
      new Set()
    );

    return [...servers];
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief find out servers for the shards of a collection
////////////////////////////////////////////////////////////////////////////////

  function findCollectionShardServers(database, collection, sort = true, unique = false) {
    var shards = collectionShardInfo(database, collection);
    var sColServers = [];
    for (const [shard, servers] of Object.entries(shards)) {
      sColServers.push(...servers);
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
      var shardInfo = collectionShardInfo(database, c[i].name());
      var shards = Object.keys(shardInfo);
      var replFactor =
          global.ArangoClusterInfo.getCollectionInfo(database, c[i].name())
              .replicationFactor;
      assertTrue(typeof replFactor === "number");
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
      var job = queryAgencyJob(result.json.id);
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
      var job = queryAgencyJob(result.json.id);
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
/// @brief GET url and return the parsed body as JSON. If the parsed body
/// contains an .error property, throws an Error containing the .errorMessage.
/// Also throws if the GET failed, or the body doesn't contain a JSON string.
////////////////////////////////////////////////////////////////////////////////

  function GETAndParseOrThrow (url) {
    let res;
    try {
      const envelope = { method: "GET", url };
      res = request(envelope);
    } catch (err) {
      console.error(`Exception for GET ${url}:`, err.stack);
      throw err;
    }
    const body = res.body;
    if (typeof body !== "string") {
      console.error(`Error after GET ${url}; body is not a string`);
      throw new Error("body is not a string");
    }

    const parsedBody = JSON.parse(body);

    if (parsedBody.hasOwnProperty("error") && parsedBody.error) {
      const error = Object.assign({}, parsedBody);
      error.errorMessage = `GET ${url} returned error: ${error.errorMessage}`;
      throw new arangodb.ArangoError(error);
    }

    return parsedBody;
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief get DBServers per shard via the view api. Returns an object which
/// which keys are shards and which values are sets of DBServers.
////////////////////////////////////////////////////////////////////////////////

  function getViewServersPerShard() {
    const endpointToURL = cluster.endpointToURL;

    global.ArangoClusterInfo.flush();

    let serversPerShard = {};

    getDBServers().forEach(serverId => {
      const dbServerEndpoint =
          global.ArangoClusterInfo.getServerEndpoint(serverId);
      const url = endpointToURL(dbServerEndpoint);

      const views = GETAndParseOrThrow(url + '/_api/view').result;
      views.forEach(v => {
        let links;
        try {
          links =
            GETAndParseOrThrow(url + '/_api/view/' + v.name + '/properties')
              .links;
        } catch (e) {
          // After shards are moved, it takes a little time before the view(s)
          // aren't available via the REST api anymore. Thus there is a race
          // between the HTTP calls to `/_api/view` and
          // `/_api/view/<name>/properties`, and the second GET may fail because
          // the view disappeared between the calls.
          // If that's the case, we treat the view as non-existent on this
          // server.

          if (e instanceof arangodb.ArangoError
            && e.errorNum === internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code) {
            console.info(`Exception during getViewServersPerShard(): ${e}`);

            return;
          }

          throw e;
        }

        if (links === undefined) {
          return;
        }

        for(const shard of Object.keys(links)) {
          if (typeof links[shard] !== "object" || links[shard] === {}) {
            throw new Error(`On DBServer ${serverId}, link to shard ${shard} `
              + `is unexpectedly either not an object or an empty object.`);
          }

          if (!serversPerShard.hasOwnProperty(shard)) {
            serversPerShard[shard] = new Set();
          }

          serversPerShard[shard].add(serverId);
        }
      });
    });

    return serversPerShard;
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief Assert that for each view, the leader has the view available (via the
/// view api)
////////////////////////////////////////////////////////////////////////////////

  function assertAllLeadersHaveTheirViews() {
    c.forEach( c_v => {
      // leaders in plan
      const leadersPerShard = shardLeaders("_system", c_v.name());
      // actual servers that have the corresponding view index
      const serversPerShard = getViewServersPerShard();
      for(const [shard, leader] of Object.entries(leadersPerShard)) {
        assertTrue(serversPerShard.hasOwnProperty(shard)
          && serversPerShard[shard].has(leader),
          `Expected shard ${shard} to be available on ${leader}, but it's not. `
          + `${JSON.stringify({ leadersPerShard, serversPerShard })}`);
      }
    });
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
          assertTrue(view !== undefined, "_createView failed");
          assertTrue(view instanceof internal.ArangoView, "_createView didn't return an ArangoView object");
          view.properties(
            {links:
              {[name]:
                {fields:
                  {'a': { analyzers: ['identity']}, 'b': { analyzers: ['text_en']}}
                }
              }
            });
          assertAllLeadersHaveTheirViews();
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
/// @brief Calls callback() in a loop until callback() returns true. Then,
/// waitUntilWithTimeout() returns true. If callback() doesn't return true
/// until `timeout` seconds pass, waitUntilWithTimeout() returns false. Waits
/// `interval` seconds between two calls of callback().
/// An error message may be passed which is printed on timeout.
////////////////////////////////////////////////////////////////////////////////
  function waitUntilWithTimeout(callback, timeout = 120, interval = 1.0, message = null) {
    const start = Date.now();
    const duration = () => (Date.now() - start) / 1000;

    while (true) {
      if (callback() === true) {
        return true;
      }

      if (duration() > timeout) {
        if (message !== null) {
          console.error(message);
        }
        return false;
      }

      wait(interval);
    }
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief check if all Supervision jobs are finished
////////////////////////////////////////////////////////////////////////////////

  function supervisionDone() {
    const state = supervisionState();

    const isDone = !state.error &&
      Object.keys(state.ToDo).length === 0 &&
      Object.keys(state.Pending).length === 0;

    if (state.error) {
      console.warn(
          'Waiting for supervision jobs to finish:',
          'Currently no agency communication possible.');
    } else if (!isDone) {
      console.info(
          'Waiting for supervision jobs to finish:', 'ToDo jobs:',
          Object.keys(state.ToDo).length,
          'Pending jobs:', Object.keys(state.Pending).length);
    }

    return isDone;
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief wait for Supervision to finish jobs
////////////////////////////////////////////////////////////////////////////////

  function waitForSupervision() {
    return waitUntilWithTimeout(supervisionDone, 300);
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the collection is in sync with the plan
////////////////////////////////////////////////////////////////////////////////

  function planEqualCurrent(collection) {
    global.ArangoClusterInfo.flush();
    const shardDist = internal.getCollectionShardDistribution(collection._id);
    const Plan = shardDist[collection.name()].Plan;
    const Current = shardDist[collection.name()].Current;

    return _.isObject(Plan) && _.isEqual(Plan, Current);
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief waits for the collection to come in sync with the plan
////////////////////////////////////////////////////////////////////////////////

  function waitForPlanEqualCurrent(collection) {
    return waitUntilWithTimeout(
      () => planEqualCurrent(collection),
      120, 1.0,
      `Collection "${collection}" failed to get plan in sync after 120 sec`
    );
  }


////////////////////////////////////////////////////////////////////////////////
/// @brief Waits for views to be available on all dbservers they should be
/// created on, including followers. This also asserts the view is not available
/// on servers it should not be.
////////////////////////////////////////////////////////////////////////////////

  function waitForViewsToGetInSync() {

    const viewInSync = collection => {
      // object of arrays, keys are shards
      const cinfo = collectionShardInfo("_system", collection.name());
      // object of sets, keys are shards
      const ccinfo = getViewServersPerShard();
      // check that for every shard in the plan, all dbservers that are in its
      // plan are also visible via the dbserver's view api.
      return _.every(
        Object.keys(cinfo).map(shard => {
          const planServers = cinfo[shard]; // array
          const currentServers = ccinfo[shard]; // Set

          // Check that for the given `shard`, every DBServer in the Plan has
          // the shard available via the view api. Also check that the number of
          // DBServers having this shard available via the view api is the same
          // as the number of DBServers in the Plan.
          // With the implicit understanding that the list of DBServers in the
          // Plan does not hold duplicates (and currentServers being a Set),
          // this implies they are equal up to order.
          return planServers.length === currentServers.size
            && _.every(planServers, server => currentServers.has(server));
        })
      );
    };

    return waitUntilWithTimeout(() => {
      global.ArangoClusterInfo.flush();
      return _.every(c, viewInSync);
    });
  }

  /*
   * Notes:
   * - Immediately after view creation, the leader-DBServers must have the view
   *   available. So don't wait for anything before that is tested.
   * - The followers may get the view later.
   * - The maintenance creates the arangosearch indexes. This is identical with
   *   the view/link being created.
   */
  function waitAndAssertViewEqualCollectionServers() {
    assertAllLeadersHaveTheirViews();
    assertTrue(waitForSupervision());
    assertTrue(waitForViewsToGetInSync());
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
      v.forEach(v => v.drop());
      c.forEach(c => c.drop());
      c = [];
      v = [];
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
          waitAndAssertViewEqualCollectionServers();
          return;
        }
        console.log("Waiting for 5 dbservers to be present:", JSON.stringify(dbservers));
        wait(1.0);
      }
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
      waitAndAssertViewEqualCollectionServers();
      assertTrue(shrinkCluster(3));
      assertTrue(testServerEmpty(_dbservers[3], true));
      waitAndAssertViewEqualCollectionServers();
      assertTrue(shrinkCluster(2));
      assertTrue(testServerEmpty(_dbservers[2], true));
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
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
      waitAndAssertViewEqualCollectionServers();
    },

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(function MovingShardsWithViewSuite_nodata() {
  let derivedSuite = {};
  deriveTestSuite(MovingShardsWithViewSuite({ useData: false }), derivedSuite, "_nodata");
  return derivedSuite;
});

jsunity.run(function MovingShardsWithViewSuite_data() {
  let derivedSuite = {};
  deriveTestSuite(MovingShardsWithViewSuite({ useData: true }), derivedSuite, "_data");
  return derivedSuite;
});

return jsunity.done();
