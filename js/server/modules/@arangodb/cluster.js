/*global ArangoServerState, ArangoClusterInfo */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript cluster functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var console = require("console");
var arangodb = require("@arangodb");
var ArangoCollection = arangodb.ArangoCollection;
var ArangoError = arangodb.ArangoError;
var PortFinder = require("@arangodb/cluster/planner").PortFinder;
var Planner = require("@arangodb/cluster/planner").Planner;
var Kickstarter = require("@arangodb/cluster/kickstarter").Kickstarter;
var endpointToURL = require("@arangodb/cluster/planner").endpointToURL;
var request = require("@arangodb/request").request;

////////////////////////////////////////////////////////////////////////////////
/// @brief create a unique identifier
////////////////////////////////////////////////////////////////////////////////

function getUUID () {
  var rand = require("internal").rand;
  var sha1 = require("internal").sha1;
  return sha1("blabla"+rand());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create and post an AQL query that will fetch a READ lock on a
/// collection and will time out after a number of seconds
////////////////////////////////////////////////////////////////////////////////

function startReadingQuery (endpoint, collName, timeout) {
  var uuid = getUUID();
  var query = {"query": `LET t=SLEEP(${timeout})
                         FOR x IN ${collName}
                         LIMIT 1
                         RETURN "${uuid}" + t`};
  var url = endpointToURL(endpoint);
  var r = request({ url: url + "/_api/cursor", body: JSON.stringify(query),
                    method: "POST", headers:  {"x-arango-async": true} });
  if (r.status !== 202) {
    console.error("startReadingQuery: Could not start read transaction for shard", collName, r);
    return false;
  }
  var count = 0;
  while (true) {
    count += 1;
    if (count > 500) {
      console.error("startReadingQuery: Read transaction did not begin. Giving up");
      return false;
    }
    require("internal").wait(0.2);
    r = request({ url: url + "/_api/query/current", method: "GET" });
    if (r.status !== 200) {
      console.error("startReadingQuery: Bad response from /_api/query/current",
                    r);
      continue;
    }
    try {
      r = JSON.parse(r.body);
    }
    catch (err) {
      console.error("startReadingQuery: Bad response body from",
                    "/_api/query/current", r);
      continue;
    }
    for (var i = 0; i < r.length; i++) {
      if (r[i].query.indexOf(uuid) !== -1) {
        // Bingo, found it: 
        if (r[i].state === "executing") {
          console.info("startReadingQuery: OK");
          return r[i].id;
        }
        console.info("startReadingQuery: query found but not yet executing");
        break;
      }
    }
    console.error("startReadingQuery: Did not find query.");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel such a query, return whether or not the query was found
////////////////////////////////////////////////////////////////////////////////

function cancelReadingQuery (endpoint, queryid) {
  var url = endpointToURL(endpoint) + "/_api/query/" + queryid;
  var r = request({url, method: "DELETE" });
  if (r.status !== 200) {
    console.error("CancelReadingQuery: error", r);
    return false;
  }
  console.info("CancelReadingQuery: success");
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel barrier from sync
////////////////////////////////////////////////////////////////////////////////

function cancelBarrier (endpoint, barrierId) {
  var url = endpointToURL(endpoint) + "/_api/replication/barrier/" + barrierId;
  var r = request({url, method: "DELETE" });
  if (r.status !== 200 && r.status !== 204) {
    console.error("CancelBarrier: error", r);
    return false;
  }
  console.info("cancelBarrier: success");
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tell leader that we are in sync
////////////////////////////////////////////////////////////////////////////////

function addShardFollower(endpoint, shard) {
  console.info("addShardFollower: tell the leader to put us into the follower list...");
  var url = endpointToURL(endpoint) + "/_api/replication/addFollower";
  var body = {followerId: ArangoServerState.id(), shard };
  var r = request({url, body: JSON.stringify(body), method: "PUT"});
  if (r.status !== 200) {
    console.error("addShardFollower: could not add us to the leader's follower list.", r);
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get values from Plan or Current by a prefix
////////////////////////////////////////////////////////////////////////////////

function getByPrefix (values, prefix) {
  var result = { };
  var a;
  var n = prefix.length;

  for (a in values) {
    if (values.hasOwnProperty(a)) {
      if (a.substr(0, n) === prefix) {
        result[a.substr(n)] = values[a];
      }
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get values from Plan or Current by a prefix
////////////////////////////////////////////////////////////////////////////////

function getByPrefix3d (values, prefix) {
  var result = { };
  var a;
  var n = prefix.length;

  for (a in values) {
    if (values.hasOwnProperty(a)) {
      if (a.substr(0, n) === prefix) {
        var key = a.substr(n);
        var parts = key.split('/');
        if (parts.length >= 2) {
          if (! result.hasOwnProperty(parts[0])) {
            result[parts[0]] = { };
          }
          result[parts[0]][parts[1]] = values[a];
        }
      }
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get values from Plan or Current by a prefix
////////////////////////////////////////////////////////////////////////////////

function getByPrefix4d (values, prefix) {
  var result = { };
  var a;
  var n = prefix.length;

  for (a in values) {
    if (values.hasOwnProperty(a)) {
      if (a.substr(0, n) === prefix) {
        var key = a.substr(n);
        var parts = key.split('/');
        if (parts.length >= 3) {
          if (! result.hasOwnProperty(parts[0])) {
            result[parts[0]] = { };
          }
          if (! result[parts[0]].hasOwnProperty(parts[1])) {
            result[parts[0]][parts[1]] = { };
          }
          result[parts[0]][parts[1]][parts[2]] = values[a];
        }
      }
    }
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup for 4-dimensional nested dictionary data
////////////////////////////////////////////////////////////////////////////////

function lookup4d (data, a, b, c) {
  if (! data.hasOwnProperty(a)) {
    return undefined;
  }
  if (! data[a].hasOwnProperty(b)) {
    return undefined;
  }
  if (! data[a][b].hasOwnProperty(c)) {
    return undefined;
  }
  return data[a][b][c];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a shardId => server map
////////////////////////////////////////////////////////////////////////////////

function getShardMap (plannedCollections) {
  var shardMap = { };

  var database;

  for (database in plannedCollections) {
    if (plannedCollections.hasOwnProperty(database)) {
      var collections = plannedCollections[database];
      var collection;

      for (collection in collections) {
        if (collections.hasOwnProperty(collection)) {
          var shards = collections[collection].shards;
          var shard;

          for (shard in shards) {
            if (shards.hasOwnProperty(shard)) {
              shardMap[shard] = shards[shard];
            }
          }
        }
      }
    }
  }

  return shardMap;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the indexes of a collection as a map
////////////////////////////////////////////////////////////////////////////////

function getIndexMap (shard) {
  var indexes = { }, i;
  var idx = arangodb.db._collection(shard).getIndexes();

  for (i = 0; i < idx.length; ++i) {
    // fetch id without collection name
    var id = idx[i].id.replace(/^[a-zA-Z0-9_\-]*?\/([0-9]+)$/, '$1');

    idx[i].id = id;
    indexes[id] = idx[i];
  }

  return indexes;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute an action under a write-lock
////////////////////////////////////////////////////////////////////////////////

function writeLocked (lockInfo, cb, args) {
  var timeout = lockInfo.timeout;
  if (timeout === undefined) {
    timeout = 60;
  }

  var ttl = lockInfo.ttl;
  if (ttl === undefined) {
    ttl = 120;
  }
  if (require("internal").coverage || require("internal").valgrind) {
    ttl *= 10;
    timeout *= 10;
  }

  global.ArangoAgency.lockWrite(lockInfo.part, ttl, timeout);

  try {
    cb.apply(null, args);
    global.ArangoAgency.increaseVersion(lockInfo.part + "/Version");
    global.ArangoAgency.unlockWrite(lockInfo.part, timeout);
  }
  catch (err) {
    global.ArangoAgency.unlockWrite(lockInfo.part, timeout);
    throw err;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a hash with the local databases
////////////////////////////////////////////////////////////////////////////////

function getLocalDatabases () {
  var result = { };
  var db = require("internal").db;

  db._listDatabases().forEach(function (database) {
    result[database] = { name: database };
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a hash with the local collections
////////////////////////////////////////////////////////////////////////////////

function getLocalCollections () {
  var result = { };
  var db = require("internal").db;

  db._collections().forEach(function (collection) {
    var name = collection.name();

    if (name.substr(0, 1) !== '_') {
      var data = {
        id: collection._id,
        name: name,
        type: collection.type(),
        status: collection.status(),
        planId: collection.planId()
      };

      // merge properties
      var properties = collection.properties();
      var p;
      for (p in properties) {
        if (properties.hasOwnProperty(p)) {
          data[p] = properties[p];
        }
      }

      result[name] = data;
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create databases if they exist in the plan but not locally
////////////////////////////////////////////////////////////////////////////////

function createLocalDatabases (plannedDatabases) {
  var ourselves = global.ArangoServerState.id();
  var createDatabaseAgency = function (payload) {
    global.ArangoAgency.set("Current/Databases/" + payload.name + "/" + ourselves,
                     payload);
  };

  var db = require("internal").db;
  db._useDatabase("_system");

  var localDatabases = getLocalDatabases();
  var name;

  // check which databases need to be created locally
  for (name in plannedDatabases) {
    if (plannedDatabases.hasOwnProperty(name)) {
      var payload = plannedDatabases[name];
      payload.error = false;
      payload.errorNum = 0;
      payload.errorMessage = "no error";

      if (! localDatabases.hasOwnProperty(name)) {
        // must create database

        // TODO: handle options and user information

        console.info("creating local database '%s'", payload.name);

        try {
          db._createDatabase(payload.name);
          payload.error = false;
          payload.errorNum = 0;
          payload.errorMessage = "no error";
        }
        catch (err) {
          payload.error = true;
          payload.errorNum = err.errorNum;
          payload.errorMessage = err.errorMessage;
        }
      }

      writeLocked({ part: "Current" },
                  createDatabaseAgency,
                  [ payload ]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop databases if they do exist locally but not in the plan
////////////////////////////////////////////////////////////////////////////////

function dropLocalDatabases (plannedDatabases) {
  var ourselves = global.ArangoServerState.id();

  var dropDatabaseAgency = function (payload) {
    try {
      global.ArangoAgency.remove("Current/Databases/" + payload.name + "/" + ourselves);
    }
    catch (err) {
      // ignore errors
    }
  };

  var db = require("internal").db;
  db._useDatabase("_system");

  var localDatabases = getLocalDatabases();
  var name;

  // check which databases need to be deleted locally
  for (name in localDatabases) {
    if (localDatabases.hasOwnProperty(name)) {
      if (! plannedDatabases.hasOwnProperty(name) && name.substr(0, 1) !== '_') {
        // must drop database

        console.info("dropping local database '%s'", name);

        // Do we have to stop a replication applier first?
        if (ArangoServerState.role() === "SECONDARY") {
          try {
            db._useDatabase(name);
            var rep = require("@arangodb/replication");
            var state = rep.applier.state();
            if (state.state.running === true) {
              console.info("stopping replication applier first");
              rep.applier.stop();
            }
            db._useDatabase("_system");
          }
          catch (err) {
            db._useDatabase("_system");
          }
        }
        db._dropDatabase(name);

        writeLocked({ part: "Current" },
                    dropDatabaseAgency,
                    [ { name: name } ]);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clean up what's in Current/Databases for ourselves
////////////////////////////////////////////////////////////////////////////////

function cleanupCurrentDatabases () {
  var ourselves = global.ArangoServerState.id();

  var dropDatabaseAgency = function (payload) {
    try {
      global.ArangoAgency.remove("Current/Databases/" + payload.name + "/" + ourselves);
    }
    catch (err) {
      // ignore errors
    }
  };

  var db = require("internal").db;
  db._useDatabase("_system");

  var all = global.ArangoAgency.get("Current/Databases", true);
  var currentDatabases = getByPrefix3d(all, "Current/Databases/");
  var localDatabases = getLocalDatabases();
  var name;

  for (name in currentDatabases) {
    if (currentDatabases.hasOwnProperty(name) && name.substr(0, 1) !== '_') {
      if (! localDatabases.hasOwnProperty(name)) {
        // we found a database we don't have locally

        if (currentDatabases[name].hasOwnProperty(ourselves)) {
          // we are entered for a database that we don't have locally
          console.info("cleaning up entry for unknown database '%s'", name);

          writeLocked({ part: "Current" },
                      dropDatabaseAgency,
                      [ { name: name } ]);
        }

      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle database changes
////////////////////////////////////////////////////////////////////////////////

function handleDatabaseChanges (plan) {
  var plannedDatabases = getByPrefix(plan, "Plan/Databases/");

  createLocalDatabases(plannedDatabases);
  dropLocalDatabases(plannedDatabases);
  cleanupCurrentDatabases();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create collections if they exist in the plan but not locally
////////////////////////////////////////////////////////////////////////////////

function createLocalCollections (plannedCollections, planVersion) {
  var ourselves = global.ArangoServerState.id();

  var createCollectionAgency = function (database, shard, collInfo, error) {
    var payload = { error: error.error,
                    errorNum: error.errorNum,
                    errorMessage: error.errorMessage,
                    indexes: collInfo.indexes,
                    servers: [ ourselves ],
                    planVersion: planVersion };
    global.ArangoAgency.set("Current/Collections/" + database + "/" + 
                            collInfo.planId + "/" + shard,
                            payload);
  };

  var db = require("internal").db;
  db._useDatabase("_system");
  var localDatabases = getLocalDatabases();
  var database;
  var i;

  // iterate over all matching databases
  for (database in plannedCollections) {
    if (plannedCollections.hasOwnProperty(database)) {
      if (localDatabases.hasOwnProperty(database)) {
        // save old database name
        var previousDatabase = db._name();
        // switch into other database
        db._useDatabase(database);

        try {
          // iterate over collections of database
          var localCollections = getLocalCollections();

          var collections = plannedCollections[database];
          var collection;

          // diff the collections
          for (collection in collections) {
            if (collections.hasOwnProperty(collection)) {
              var collInfo = collections[collection];
              var shards = collInfo.shards;
              var shard;

              collInfo.planId = collInfo.id;
              var save = [collInfo.id, collInfo.name];
              delete collInfo.id;  // must not actually set it here
              delete collInfo.name;  // name is now shard

              for (shard in shards) {
                if (shards.hasOwnProperty(shard)) {
                  if (shards[shard][0] === ourselves) {
                    // found a shard we are responsible for

                    var error = { error: false, errorNum: 0,
                                  errorMessage: "no error" };

                    if (! localCollections.hasOwnProperty(shard)) {
                      // must create this shard
                      console.info("creating local shard '%s/%s' for central '%s/%s'",
                                   database,
                                   shard,
                                   database,
                                   collInfo.planId);

                      try {
                        if (collInfo.type === ArangoCollection.TYPE_EDGE) {
                          db._createEdgeCollection(shard, collInfo);
                        }
                        else {
                          db._create(shard, collInfo);
                        }
                      }
                      catch (err2) {
                        error = { error: true, errorNum: err2.errorNum,
                                  errorMessage: err2.errorMessage };
                        console.error("creating local shard '%s/%s' for central '%s/%s' failed: %s",
                                      database,
                                      shard,
                                      database,
                                      collInfo.planId,
                                      JSON.stringify(err2));
                      }

                      writeLocked({ part: "Current" },
                                  createCollectionAgency,
                                  [ database, shard, collInfo, error ]);
                    }
                    else {
                      if (localCollections[shard].status !== collInfo.status) {
                        console.info("detected status change for local shard '%s/%s'",
                                     database,
                                     shard);

                        if (collInfo.status === ArangoCollection.STATUS_UNLOADED) {
                          console.info("unloading local shard '%s/%s'",
                                       database,
                                       shard);
                          db._collection(shard).unload();
                        }
                        else if (collInfo.status === ArangoCollection.STATUS_LOADED) {
                          console.info("loading local shard '%s/%s'",
                                       database,
                                       shard);
                          db._collection(shard).load();
                        }
                        writeLocked({ part: "Current" },
                                    createCollectionAgency,
                                    [ database, shard, collInfo, error ]);
                      }

                      // collection exists, now compare collection properties
                      var properties = { };
                      var cmp = [ "journalSize", "waitForSync", "doCompact",
                                  "indexBuckets" ];
                      for (i = 0; i < cmp.length; ++i) {
                        var p = cmp[i];
                        if (localCollections[shard][p] !== collInfo[p]) {
                          // property change
                          properties[p] = collInfo[p];
                        }
                      }

                      if (Object.keys(properties).length > 0) {
                        console.info("updating properties for local shard '%s/%s'",
                                     database,
                                     shard);

                        try {
                          db._collection(shard).properties(properties);
                        }
                        catch (err3) {
                          error = { error: true, errorNum: err3.errorNum,
                                    errorMessage: err3.errorMessage };
                        }
                        writeLocked({ part: "Current" },
                                    createCollectionAgency,
                                    [ database, shard, collInfo, error ]);
                      }
                    }

                    if (error.error) {
                      continue;  // No point to look for properties and
                                 // indices, if the creation has not worked
                    }

                    var indexes = getIndexMap(shard);
                    var idx;
                    var index;

                    if (collInfo.hasOwnProperty("indexes")) {
                      for (i = 0; i < collInfo.indexes.length; ++i) {
                        index = collInfo.indexes[i];

                        var changed = false;

                        if (index.type !== "primary" && index.type !== "edge" &&
                            ! indexes.hasOwnProperty(index.id)) {
                          console.info("creating index '%s/%s': %s",
                                       database,
                                       shard,
                                       JSON.stringify(index));

                          try {
                            arangodb.db._collection(shard).ensureIndex(index);
                            index.error = false;
                            index.errorNum = 0;
                            index.errorMessage = "";
                          }
                          catch (err5) {
                            index.error = true;
                            index.errorNum = err5.errorNum;
                            index.errorMessage = err5.errorMessage;
                          }

                          changed = true;
                        }
                        if (changed) {
                          writeLocked({ part: "Current" },
                                      createCollectionAgency,
                                      [ database, shard, collInfo, error ]);
                        }
                      }

                      var changed2 = false;
                      for (idx in indexes) {
                        if (indexes.hasOwnProperty(idx)) {
                          // found an index in the index map, check if it must be deleted

                          if (indexes[idx].type !== "primary" && indexes[idx].type !== "edge") {
                            var found = false;
                            for (i = 0; i < collInfo.indexes.length; ++i) {
                              if (collInfo.indexes[i].id === idx) {
                                found = true;
                                break;
                              }
                            }

                            if (! found) {
                              // found an index to delete locally
                              changed2 = true;
                              index = indexes[idx];

                              console.info("dropping index '%s/%s': %s",
                                            database,
                                            shard,
                                            JSON.stringify(index));

                              arangodb.db._collection(shard).dropIndex(index);

                              delete indexes[idx];
                              collInfo.indexes.splice(i, i);
                            }
                          }
                        }
                      }
                      if (changed2) {
                        writeLocked({ part: "Current" },
                                    createCollectionAgency,
                                    [ database, shard, collInfo, error ]);
                      }
                    }

                  }
                }
              }
              collInfo.id = save[0];
              collInfo.name = save[1];
            }
          }
        }
        catch (err) {
          // always return to previous database
          db._useDatabase(previousDatabase);
          throw err;
        }

      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop collections if they exist locally but not in the plan
////////////////////////////////////////////////////////////////////////////////

function dropLocalCollections (plannedCollections) {
  var ourselves = global.ArangoServerState.id();

  var dropCollectionAgency = function (database, shardID, id) {
    try {
      global.ArangoAgency.remove("Current/Collections/" + database + "/" + id + "/" + shardID);
    }
    catch (err) {
      // ignore errors
    }
  };

  var db = require("internal").db;
  db._useDatabase("_system");
  var shardMap = getShardMap(plannedCollections);

  var localDatabases = getLocalDatabases();
  var database;

  // iterate over all databases
  for (database in localDatabases) {
    if (localDatabases.hasOwnProperty(database)) {
      var removeAll = ! plannedCollections.hasOwnProperty(database);

      // save old database name
      var previousDatabase = db._name();
      // switch into other database
      db._useDatabase(database);

      try {
        // iterate over collections of database
        var collections = getLocalCollections();
        var collection;

        for (collection in collections) {
          if (collections.hasOwnProperty(collection)) {
            // found a local collection
            // check if it is in the plan and we are responsible for it

            var remove = removeAll ||
                         (! shardMap.hasOwnProperty(collection)) ||
                         (shardMap[collection].indexOf(ourselves) === -1);

            if (remove) {
              console.info("dropping local shard '%s/%s' of '%s/%s",
                           database,
                           collection,
                           database,
                           collections[collection].planId);

              db._drop(collection);

              writeLocked({ part: "Current" },
                          dropCollectionAgency,
                          [ database, collection, collections[collection].planId ]);
            }
          }
        }
      }
      catch (err) {
        db._useDatabase(previousDatabase);
        throw err;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clean up what's in Current/Collections for ourselves
////////////////////////////////////////////////////////////////////////////////

function cleanupCurrentCollections (plannedCollections) {
  var ourselves = global.ArangoServerState.id();

  var dropCollectionAgency = function (database, collection, shardID) {
    try {
      global.ArangoAgency.remove("Current/Collections/" + database + "/" + collection + "/" + shardID);
    }
    catch (err) {
      // ignore errors
    }
  };

  var db = require("internal").db;
  db._useDatabase("_system");

  var all = global.ArangoAgency.get("Current/Collections", true);
  var currentCollections = getByPrefix4d(all, "Current/Collections/");
  var shardMap = getShardMap(plannedCollections);
  var database;

  for (database in currentCollections) {
    if (currentCollections.hasOwnProperty(database)) {
      var collections = currentCollections[database];
      var collection;

      for (collection in collections) {
        if (collections.hasOwnProperty(collection)) {
          var shards = collections[collection];
          var shard;

          for (shard in shards) {
            if (shards.hasOwnProperty(shard)) {

              if (shards[shard].servers[0] === ourselves &&
                  (! shardMap.hasOwnProperty(shard) ||
                   shardMap[shard].indexOf(ourselves) !== 0)) {
                // found an entry in current of a shard that we used to be 
                // leader for but that we are no longer leader for
                console.info("cleaning up entry for shard '%s' of '%s/%s",
                             shard,
                             database,
                             collection);

                writeLocked({ part: "Current" },
                            dropCollectionAgency,
                            [ database, collection, shard ]);
              }
            }
          }
        }
      }

    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief synchronize collections for which we are followers (synchronously
/// replicated shards)
////////////////////////////////////////////////////////////////////////////////

function synchronizeLocalFollowerCollections (plannedCollections) {
  var ourselves = global.ArangoServerState.id();

  var db = require("internal").db;
  db._useDatabase("_system");
  var localDatabases = getLocalDatabases();
  var database;

  // Get current information about collections from agency:
  var all = global.ArangoAgency.get("Current/Collections", true);
  var currentCollections = getByPrefix4d(all, "Current/Collections/");

  var rep = require("@arangodb/replication");

  // iterate over all matching databases
  for (database in plannedCollections) {
    if (plannedCollections.hasOwnProperty(database)) {
      if (localDatabases.hasOwnProperty(database)) {
        // save old database name
        var previousDatabase = db._name();
        // switch into other database
        db._useDatabase(database);

        try {
          // iterate over collections of database
          var collections = plannedCollections[database];
          var collection;

          // diff the collections
          for (collection in collections) {
            if (collections.hasOwnProperty(collection)) {
              var collInfo = collections[collection];
              var shards = collInfo.shards;   // this is the Plan
              var shard;

              collInfo.planId = collInfo.id;

              for (shard in shards) {
                if (shards.hasOwnProperty(shard)) {
                  var pos = shards[shard].indexOf(ourselves);
                  if (pos > 0) {   // found and not in position 0
                    // found a shard we have to replicate synchronously
                    // now see whether we are in sync by looking at the
                    // current entry in the agency:
                    var inCurrent = lookup4d(currentCollections, database, 
                                             collection, shard);
                    var count = 0;
                    while (inCurrent === undefined ||
                           ! inCurrent.hasOwnProperty("servers") ||
                           typeof inCurrent.servers !== "object" ||
                           typeof inCurrent.servers[0] !== "string" ||
                           inCurrent.servers[0] === "") {
                      count++;
                      if (count > 30) {
                        throw new Error("timeout waiting for leader for shard "
                                        + shard);
                      }
                      console.info("Waiting for 3 seconds for leader to create shard...");
                      require("internal").wait(3);
                      all = global.ArangoAgency.get("Current/Collections",
                                                    true);
                      currentCollections = getByPrefix4d(all,
                                                   "Current/Collections/");
                      inCurrent = lookup4d(currentCollections, database, 
                                           collection, shard);
                    }
                    if (inCurrent.servers.indexOf(ourselves) === -1) {
                      // we not in there - must synchronize this shard from
                      // the leader
                      console.info("trying to synchronize local shard '%s/%s' for central '%s/%s'",
                                   database,
                                   shard,
                                   database,
                                   collInfo.planId);
                      try {
                        var ep = ArangoClusterInfo.getServerEndpoint(
                              inCurrent.servers[0]);
                        // First once without a read transaction:
                        var sy = rep.syncCollection(shard, 
                          { endpoint: ep, incremental: true,
                            keepBarrier: true });
                        // Now start a read transaction to stop writes:
                        var queryid;
                        try {
                          queryid = startReadingQuery(ep, shard, 300);
                        }
                        finally {
                          cancelBarrier(ep, sy.barrierId);
                        }
                        var ok = false;
                        try {
                          var sy2 = rep.syncCollectionFinalize(
                            shard, sy.collections[0].id, sy.lastLogTick, 
                            { endpoint: ep });
                          if (sy2.error) {
                            console.error("Could not synchronize shard", shard,
                                          sy2);
                          }
                          ok = addShardFollower(ep, shard);
                        }
                        finally {
                          if (!cancelReadingQuery(ep, queryid)) {
                            console.error("Read transaction has timed out for shard", shard);
                            ok = false;
                          }
                        }
                        if (ok) {
                          console.info("Synchronization worked for shard", shard);
                        } else {
                          throw "Did not work.";  // just to log below in catch
                        }
                      }
                      catch (err2) {
                        console.error("synchronization of local shard '%s/%s' for central '%s/%s' failed: %s",
                                      database,
                                      shard,
                                      database,
                                      collInfo.planId,
                                      JSON.stringify(err2));
                      }
                    }

                  }
                }
              }
            }
          }
        }
        catch (err) {
          // always return to previous database
          db._useDatabase(previousDatabase);
          throw err;
        }

      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle collection changes
////////////////////////////////////////////////////////////////////////////////

function handleCollectionChanges (plan) {
  var plannedCollections = getByPrefix3d(plan, "Plan/Collections/");

  var ok = true;

  try {
    createLocalCollections(plannedCollections, plan["Plan/Version"]);
    dropLocalCollections(plannedCollections);
    cleanupCurrentCollections(plannedCollections);
    synchronizeLocalFollowerCollections(plannedCollections);
  }
  catch (err) {
    console.error("Caught error in handleCollectionChanges: " + 
                  JSON.stringify(err));
    ok = false; 
  }
  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief make sure that replication is set up for all databases
////////////////////////////////////////////////////////////////////////////////

function setupReplication () {
  console.debug("Setting up replication...");

  var db = require("internal").db;
  var rep = require("@arangodb/replication");
  var dbs = db._listDatabases();
  var i;
  var ok = true;
  for (i = 0; i < dbs.length; i++) {
    try {
      var database = dbs[i];
      console.debug("Checking replication of database "+database);
      db._useDatabase(database);

      var state = rep.applier.state();
      if (state.state.running === false) {
        var endpoint = ArangoClusterInfo.getServerEndpoint(
                              ArangoServerState.idOfPrimary());
        var config = { "endpoint": endpoint, "includeSystem": false,
                       "incremental": false, "autoStart": true,
                       "requireFromPresent": true};
        console.info("Starting synchronization...");
        var res = rep.sync(config);
        console.info("Last log tick: "+res.lastLogTick+
                    ", starting replication...");
        rep.applier.properties(config);
        var res2 = rep.applier.start(res.lastLogTick);
        console.info("Result of replication start: "+res2);
      }
    }
    catch (err) {
      console.error("Could not set up replication for database ", database);
      ok = false;
    }
  }
  db._useDatabase("_system");
  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief role change from secondary to primary
////////////////////////////////////////////////////////////////////////////////

function secondaryToPrimary () {
  console.info("Switching role from secondary to primary...");
  var db = require("internal").db;
  var rep = require("@arangodb/replication");
  var dbs = db._listDatabases();
  var i;
  try {
    for (i = 0; i < dbs.length; i++) {
      var database = dbs[i];
      console.info("Stopping asynchronous replication for db " +
                   database + "...");
      db._useDatabase(database);
      var state = rep.applier.state();
      if (state.state.running === true) {
        try {
          rep.applier.stop();
        }
        catch (err) {
          console.info("Exception caught whilst stopping replication!");
        }
      }
      rep.applier.forget();
    }
  }
  finally {
    db._useDatabase("_system");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief role change from primary to secondary
////////////////////////////////////////////////////////////////////////////////

function primaryToSecondary () {
  console.info("Switching role from primary to secondary...");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief change handling trampoline function
////////////////////////////////////////////////////////////////////////////////

function handleChanges (plan, current) {
  var changed = false;
  var role = ArangoServerState.role();
  if (role === "PRIMARY" || role === "SECONDARY") {
    // Need to check role change for automatic failover:
    var myId = ArangoServerState.id();
    if (role === "PRIMARY") {
      if (! plan.hasOwnProperty("Plan/DBServers/"+myId)) {
        // Ooops! We do not seem to be a primary any more!
        changed = ArangoServerState.redetermineRole();
      }
    }
    else { // role === "SECONDARY"
      if (plan.hasOwnProperty("Plan/DBServers/"+myId)) {
        // Ooops! We are now a primary!
        changed = ArangoServerState.redetermineRole();
      }
      else {
        var found = null;
        var p;
        for (p in plan) {
          if (plan.hasOwnProperty(p) && plan[p] === myId) {
            found = p;
            break;
          }
        }
        if (found !== ArangoServerState.idOfPrimary()) {
          // Note this includes the case that we are not found at all!
          changed = ArangoServerState.redetermineRole();
        }
      }
    }
  }
  var oldRole = role;
  if (changed) {
    role = ArangoServerState.role();
    console.log("Our role has changed to " + role);
    if (oldRole === "SECONDARY" && role === "PRIMARY") {
      secondaryToPrimary();
    }
    else if (oldRole === "PRIMARY" && role === "SECONDARY") {
      primaryToSecondary();
    }
  }

  handleDatabaseChanges(plan, current);
  var success;
  if (role === "PRIMARY" || role === "COORDINATOR") {
    // Note: This is only ever called for DBservers (primary and secondary),
    // we keep the coordinator case here just in case...
    success = handleCollectionChanges(plan, current);
  }
  else {
    success = setupReplication();
  }

  return success;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief throw an ArangoError
////////////////////////////////////////////////////////////////////////////////

var raiseError = function (code, msg) {
  var err = new ArangoError();
  err.errorNum = code;
  err.errorMessage = msg;

  throw err;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief retrieve a list of shards for a collection
////////////////////////////////////////////////////////////////////////////////

var shardList = function (dbName, collectionName) {
  var ci = global.ArangoClusterInfo.getCollectionInfo(dbName, collectionName);

  if (ci === undefined || typeof ci !== 'object') {
    throw "unable to determine shard list for '" + dbName + "/" + collectionName + "'";
  }

  var shards = [ ], shard;
  for (shard in ci.shards) {
    if (ci.shards.hasOwnProperty(shard)) {
      shards.push(shard);
    }
  }

  if (shards.length === 0) {
    raiseError(arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code,
               arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.message);
  }

  return shards;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for a distributed response
////////////////////////////////////////////////////////////////////////////////

var waitForDistributedResponse = function (data, shards) {
  var received = [ ];

  while (received.length < shards.length) {
    var result = global.ArangoClusterComm.wait(data);
    var status = result.status;

    if (status === "ERROR") {
      raiseError(arangodb.errors.ERROR_INTERNAL.code,
                 "received an error from a DB server: " + JSON.stringify(result));
    }
    else if (status === "TIMEOUT") {
      raiseError(arangodb.errors.ERROR_CLUSTER_TIMEOUT.code,
                 arangodb.errors.ERROR_CLUSTER_TIMEOUT.message);
    }
    else if (status === "DROPPED") {
      raiseError(arangodb.errors.ERROR_INTERNAL.code,
                 "the operation was dropped");
    }
    else if (status === "RECEIVED") {
      received.push(result);

      if (result.headers && result.headers.hasOwnProperty('x-arango-response-code')) {
        var code = parseInt(result.headers['x-arango-response-code'].substr(0, 3), 10);

        if (code >= 400) {
          var body;

          try {
            body = JSON.parse(result.body);
          }
          catch (err) {
            raiseError(arangodb.errors.ERROR_INTERNAL.code,
                       "error parsing JSON received from a DB server: " + err.message);
          }

          raiseError(body.errorNum,
                     body.errorMessage);
        }
      }
    }
    else {
      // something else... wait without GC
      require("internal").wait(0.1, false);
    }
  }

  return received;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not clustering is enabled
////////////////////////////////////////////////////////////////////////////////

var isCluster = function () {
  var role = global.ArangoServerState.role();
  
  return (role !== undefined && role !== "SINGLE");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not we are a coordinator
////////////////////////////////////////////////////////////////////////////////

var isCoordinator = function () {
  return global.ArangoServerState.isCoordinator();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief role
////////////////////////////////////////////////////////////////////////////////

var role = function () {
  var role = global.ArangoServerState.role();

  if (role !== "SINGLE") {
    return role;
  }
  return undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief status
////////////////////////////////////////////////////////////////////////////////

var status = function () {
  if (! isCluster() || ! global.ArangoServerState.initialized()) {
    return undefined;
  }

  return global.ArangoServerState.status();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief isCoordinatorRequest
////////////////////////////////////////////////////////////////////////////////

var isCoordinatorRequest = function (req) {
  if (! req || ! req.hasOwnProperty("headers")) {
    return false;
  }

  return req.headers.hasOwnProperty("x-arango-coordinator");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief handlePlanChange
////////////////////////////////////////////////////////////////////////////////

var handlePlanChange = function () {
  if (! isCluster() || isCoordinator() || ! global.ArangoServerState.initialized()) {
    return;
  }

  try {
    var plan    = global.ArangoAgency.get("Plan", true);
    var current = global.ArangoAgency.get("Current", true);

    handleChanges(plan, current);
    console.info("plan change handling successful");
  }
  catch (err) {
    console.error("error details: %s", JSON.stringify(err));
    console.error("error stack: %s", err.stack);
    console.error("plan change handling failed");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcherFrontendDisabled
////////////////////////////////////////////////////////////////////////////////

var dispatcherFrontendDisabled = function () {
  return global.ArangoServerState.disableDispatcherFrontend();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcherKickstarterDisabled
////////////////////////////////////////////////////////////////////////////////

var dispatcherKickstarterDisabled = function () {
  return global.ArangoServerState.disableDispatcherKickstarter();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief coordinatorId
////////////////////////////////////////////////////////////////////////////////

var coordinatorId = function () {
  if (! isCoordinator()) {
    console.error("not a coordinator");
  }
  return global.ArangoServerState.id();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief bootstrap db servers
////////////////////////////////////////////////////////////////////////////////

var bootstrapDbServers = function (isRelaunch) {
  global.ArangoClusterInfo.reloadDBServers();

  var dbServers = global.ArangoClusterInfo.getDBServers();
  var ops = [];
  var i;

  var options = {
    coordTransactionID: global.ArangoClusterInfo.uniqid(),
    timeout: 90
  };

  for (i = 0;  i < dbServers.length;  ++i) {
    var server = dbServers[i];

    var op = global.ArangoClusterComm.asyncRequest(
      "POST",
      "server:" + server,
      "_system",
      "/_admin/cluster/bootstrapDbServer",
      '{"isRelaunch": ' + (isRelaunch ? "true" : "false") + '}',
      {},
      options);

    ops.push(op);
  }

  var result = true;

  for (i = 0;  i < ops.length;  ++i) {
    var r = global.ArangoClusterComm.wait(ops[i]);

    if (r.status === "RECEIVED") {
      console.info("bootstraped DB server %s", dbServers[i]);
    }
    else if (r.status === "TIMEOUT") {
      console.error("cannot bootstrap DB server %s: operation timed out", dbServers[i]);
      result = false;
    }
    else {
      console.error("cannot bootstrap DB server %s: %s", dbServers[i], JSON.stringify(r));
      result = false;
    }
  }

  return result;
};


exports.bootstrapDbServers            = bootstrapDbServers;
exports.coordinatorId                 = coordinatorId;
exports.dispatcherFrontendDisabled    = dispatcherFrontendDisabled;
exports.dispatcherKickstarterDisabled = dispatcherKickstarterDisabled;
exports.handlePlanChange              = handlePlanChange;
exports.isCluster                     = isCluster;
exports.isCoordinator                 = isCoordinator;
exports.isCoordinatorRequest          = isCoordinatorRequest;
exports.role                          = role;
exports.shardList                     = shardList;
exports.status                        = status;
exports.wait                          = waitForDistributedResponse;

exports.Kickstarter = Kickstarter;
exports.Planner = Planner;
exports.PortFinder = PortFinder;


