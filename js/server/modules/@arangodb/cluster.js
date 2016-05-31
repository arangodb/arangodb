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
var request = require("@arangodb/request").request;
var wait = require("internal").wait;

var endpointToURL = function (endpoint) {
  if (endpoint.substr(0,6) === "ssl://") {
    return "https://" + endpoint.substr(6);
  }
  var pos = endpoint.indexOf("://");
  if (pos === -1) {
    return "http://" + endpoint;
  }
  return "http" + endpoint.substr(pos);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create and post an AQL query that will fetch a READ lock on a
/// collection and will time out after a number of seconds
////////////////////////////////////////////////////////////////////////////////

function startReadLockOnLeader (endpoint, database, collName, timeout) {
  var url = endpointToURL(endpoint) + "/_db/" + database;
  var r = request({ url: url + "/_api/replication/holdReadLockCollection", 
                    method: "GET" });
  if (r.status !== 200) {
    console.error("startReadLockOnLeader: Could not get ID for shard",
                  collName, r);
    return false;
  }
  try {
    r = JSON.parse(r.body);
  }
  catch (err) {
    console.error("startReadLockOnLeader: Bad response body from",
                  "/_api/replication/holdReadLockCollection", r, 
                  JSON.stringify(err));
    return false;
  }
  const id = r.id;

  var body = { "id": id, "collection": collName, "ttl": timeout };
  r = request({ url: url + "/_api/replication/holdReadLockCollection", 
                body: JSON.stringify(body),
                method: "POST", headers:  {"x-arango-async": "store"} });
  if (r.status !== 202) {
    console.error("startReadLockOnLeader: Could not start read lock for shard",
                  collName, r);
    return false;
  }
  var rr = r;  // keep a copy

  var count = 0;
  while (++count < 20) {   // wait for some time until read lock established:
    // Now check that we hold the read lock:
    r = request({ url: url + "/_api/replication/holdReadLockCollection", 
                  body: JSON.stringify(body),
                  method: "PUT" });
    if (r.status === 200) {
      return id;
    }
    console.debug("startReadLockOnLeader: Do not see read lock yet...");
    wait(0.5);
  }
  var asyncJobId = rr.headers["x-arango-async-id"];
  r = request({ url: url + "/_api/job/" + asyncJobId, body: "", method: "PUT"});
  console.error("startReadLockOnLeader: giving up, async result:", r);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel such a query, return whether or not the query was found
////////////////////////////////////////////////////////////////////////////////

function cancelReadLockOnLeader (endpoint, database, lockJobId) {
  var url = endpointToURL(endpoint) + "/_db/" + database + 
            "/_api/replication/holdReadLockCollection";
  var r;
  var body = {"id":lockJobId};
  try {
    r = request({url, body: JSON.stringify(body), method: "DELETE" });
  }
  catch (e) {
    console.error("cancelReadLockOnLeader: exception caught:", e);
    return false;
  }
  if (r.status !== 200) {
    console.error("cancelReadLockOnLeader: error", r);
    return false;
  }
  console.debug("cancelReadLockOnLeader: success");
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cancel barrier from sync
////////////////////////////////////////////////////////////////////////////////

function cancelBarrier (endpoint, database, barrierId) {
  var url = endpointToURL(endpoint) + "/_db/" + database +
            "/_api/replication/barrier/" + barrierId;
  var r = request({url, method: "DELETE" });
  if (r.status !== 200 && r.status !== 204) {
    console.error("CancelBarrier: error", r);
    return false;
  }
  console.debug("cancelBarrier: success");
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tell leader that we are in sync
////////////////////////////////////////////////////////////////////////////////

function addShardFollower(endpoint, database, shard) {
  console.debug("addShardFollower: tell the leader to put us into the follower list...");
  var url = endpointToURL(endpoint) + "/_db/" + database + 
            "/_api/replication/addFollower";
  var body = {followerId: ArangoServerState.id(), shard };
  var r = request({url, body: JSON.stringify(body), method: "PUT"});
  if (r.status !== 200) {
    console.error("addShardFollower: could not add us to the leader's follower list.", r);
    return false;
  }
  return true;
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
/// @brief return a hash with the local databases
////////////////////////////////////////////////////////////////////////////////

function getLocalDatabases () {
  var result = { };
  var db = require("internal").db;

  db._databases().forEach(function (database) {
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

function createLocalDatabases (plannedDatabases, currentDatabases, writeLocked) {

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
        writeLocked({ part: "Current" },
                    createDatabaseAgency,
                    [ payload ]);
      } else if (typeof currentDatabases[name] !== 'object' || !currentDatabases[name].hasOwnProperty(ourselves)) {
        // mop: ok during cluster startup we have this buggy situation where a dbserver
        // has a database but has not yet announced it to the agency :S
        writeLocked({ part: "Current" },
                    createDatabaseAgency,
                    [ payload ]);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop databases if they do exist locally but not in the plan
////////////////////////////////////////////////////////////////////////////////

function dropLocalDatabases (plannedDatabases, writeLocked) {
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
          }
          finally {
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

function cleanupCurrentDatabases (currentDatabases, writeLocked) {
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

function handleDatabaseChanges (plan, current, writeLocked) {

  var plannedDatabases = plan.Databases;
  var currentDatabases = current.Databases;

  createLocalDatabases(plannedDatabases, currentDatabases, writeLocked);
  dropLocalDatabases(plannedDatabases, writeLocked);
  cleanupCurrentDatabases(currentDatabases, writeLocked);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create collections if they exist in the plan but not locally
////////////////////////////////////////////////////////////////////////////////

function createLocalCollections (plannedCollections, planVersion,
                                 currentCollections,
                                 takeOverResponsibility, writeLocked) {
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
  
  var takeOver = createCollectionAgency;


  var db = require("internal").db;
  db._useDatabase("_system");

  var migrate = writeLocked => {
    var localDatabases = getLocalDatabases();
    var database;
    var i;

    // iterate over all matching databases
    for (database in plannedCollections) {
      if (plannedCollections.hasOwnProperty(database)) {
        if (localDatabases.hasOwnProperty(database)) {
          // switch into other database
          db._useDatabase(database);

          try {
            // iterate over collections of database
            var localCollections = getLocalCollections();

            var collections = plannedCollections[database];

            // diff the collections
            Object.keys(collections).forEach(function(collection) {
              var collInfo = collections[collection];
              var shards = collInfo.shards;
              var shard;

              collInfo.planId = collInfo.id;
              var save = [collInfo.id, collInfo.name];
              delete collInfo.id;  // must not actually set it here
              delete collInfo.name;  // name is now shard

              for (shard in shards) {
                if (shards.hasOwnProperty(shard)) {
                  var didWrite = false;
                  if (shards[shard].indexOf(ourselves) >= 0) {
                    var isLeader = shards[shard][0] === ourselves;
                    var wasLeader = isLeader;
                    try {
                      var currentServers = currentCollections[database][collection][shard].servers;
                      wasLeader = currentServers[0] === ourselves;
                    }
                    catch(err) {
                    }

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

                      if (isLeader) {
                        writeLocked({ part: "Current" },
                            createCollectionAgency,
                            [ database, shard, collInfo, error ]);
                        didWrite = true;
                      }
                    }
                    else {
                      if (!isLeader && wasLeader) {
                        db._collection(shard).leaderResign();
                      }

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
                        if (isLeader) {
                          writeLocked({ part: "Current" },
                              createCollectionAgency,
                              [ database, shard, collInfo, error ]);
                          didWrite = true;
                        }
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
                        if (isLeader) {
                          writeLocked({ part: "Current" },
                              createCollectionAgency,
                              [ database, shard, collInfo, error ]);
                          didWrite = true;
                        }
                      }
                    }

                    if (error.error) {
                      if (takeOverResponsibility && !didWrite) {
                        if (isLeader) {
                          writeLocked({ part: "Current" },
                              takeOver,
                              [ database, shard, collInfo, error ]);
                        }
                      }
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
                        if (changed && isLeader) {
                          writeLocked({ part: "Current" },
                              createCollectionAgency,
                              [ database, shard, collInfo, error ]);
                          didWrite = true;
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
                      if (changed2 && isLeader) {
                        writeLocked({ part: "Current" },
                            createCollectionAgency,
                            [ database, shard, collInfo, error ]);
                        didWrite = true;
                      }
                    }

                    if ((takeOverResponsibility && !didWrite && isLeader) ||
                        (!didWrite && isLeader && !wasLeader)) {
                      writeLocked({ part: "Current" },
                          takeOver,
                          [ database, shard, collInfo, error ]);
                    }
                  }
                }
              }
              collInfo.id = save[0];
              collInfo.name = save[1];

            });
          }
          catch (err) {
            // always return to previous database
            db._useDatabase("_system");
            throw err;
          }

          db._useDatabase("_system");
        }
      }
    }
  };

  if (takeOverResponsibility) {
    // mop: if this is a complete takeover we need a global lock because
    // otherwise the coordinator might fetch results which are only partly
    // migrated
    var fakeLock = (lockInfo, cb, args) => {
      if (!lockInfo || lockInfo.part !== 'Current') {
        throw new Error("Invalid lockInfo " + JSON.stringify(lockInfo));
      }
      return cb(...args);
    };
    writeLocked({ part: "Current" }, migrate, [fakeLock]);
  } else {
    migrate(writeLocked);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop collections if they exist locally but not in the plan
////////////////////////////////////////////////////////////////////////////////

function dropLocalCollections (plannedCollections, writeLocked) {
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
        db._useDatabase("_system");
        throw err;
      }
      db._useDatabase("_system");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clean up what's in Current/Collections for ourselves
////////////////////////////////////////////////////////////////////////////////

function cleanupCurrentCollections (plannedCollections, currentCollections,
                                    writeLocked) {
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

              if (! shardMap.hasOwnProperty(shard)) {
                // found an entry in current of a shard that is no longer
                // mentioned in the plan
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
/// @brief launch a scheduled job if needed
////////////////////////////////////////////////////////////////////////////////

function launchJob() {
  const registerTask = require("internal").registerTask;
  var jobs = global.KEYSPACE_GET("shardSynchronization");
  if (jobs.running === null) {
    var shards = Object.keys(jobs.scheduled);
    if (shards.length > 0) {
      var jobInfo = jobs.scheduled[shards[0]];
      registerTask({
        database: jobInfo.database,
        params: {database: jobInfo.database, shard: jobInfo.shard, 
                 planId: jobInfo.planId, leader: jobInfo.leader},
        command: function(params) {
          require("@arangodb/cluster").synchronizeOneShard(
            params.database, params.shard, params.planId, params.leader);
        }});
      global.KEY_SET("shardSynchronization", "running", jobInfo);
      console.debug("scheduleOneShardSynchronization: have launched job", jobInfo);
      delete jobs.scheduled[shards[0]];
      global.KEY_SET("shardSynchronization", "scheduled", jobs.scheduled);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief synchronize one shard, this is run as a V8 task
////////////////////////////////////////////////////////////////////////////////

function synchronizeOneShard(database, shard, planId, leader) {
  // synchronize this shard from the leader
  // this function will throw if anything goes wrong

  var ok = false;
  const rep = require("@arangodb/replication");

  console.info("synchronizeOneShard: trying to synchronize local shard '%s/%s' for central '%s/%s'",
               database, shard, database, planId);
  try {
    var ep = ArangoClusterInfo.getServerEndpoint(leader);
    // First once without a read transaction:
    var sy;
    var count = 3600;   // Try for a full hour, this is because there
                        // can only be one syncCollection in flight
                        // at a time
    while (true) {
      try {
        sy = rep.syncCollection(shard, 
            { endpoint: ep, incremental: true,
              keepBarrier: true });
        break;
      }
      catch (err) {
        console.info("synchronizeOneShard: syncCollection did not work,",
                     "trying again later for shard", shard);
      }
      if (--count <= 0) {
        console.error("synchronizeOneShard: syncCollection did not work",
                      "after many tries, giving up on shard", shard);
        throw "syncCollection did not work";
      }
      wait(1.0);
    }

    if (sy.error) {
      console.error("synchronizeOneShard: could not initially synchronize",
                    "shard ", shard, sy);
      throw "Initial sync for shard " + shard + " failed";
    } else {
      if (sy.collections.length === 0 ||
          sy.collections[0].name !== shard) {
        cancelBarrier(ep, database, sy.barrierId);
        throw "Shard " + shard + " seems to be gone from leader!";
      } else {
        // Now start a read transaction to stop writes:
        var lockJobId = false;
        try {
          lockJobId = startReadLockOnLeader(ep, database,
                                            shard, 300);
          console.debug("lockJobId:", lockJobId);
        }
        catch (err1) {
          console.error("synchronizeOneShard: exception in startReadLockOnLeader:", err1);
        }
        finally {
          cancelBarrier(ep, database, sy.barrierId);
        }
        if (lockJobId !== false) {
          try {
            var sy2 = rep.syncCollectionFinalize(
              database, shard, sy.collections[0].id, 
              sy.lastLogTick, { endpoint: ep });
            if (sy2.error) {
              console.error("synchronizeOneShard: Could not synchronize shard",
                            shard, sy2);
              ok = false;
            } else {
              ok = addShardFollower(ep, database, shard);
            }
          }
          catch (err3) {
            console.error("synchronizeOneshard: exception in",
                          "syncCollectionFinalize:", err3);
          }
          finally {
            if (!cancelReadLockOnLeader(ep, database, 
                                        lockJobId)) {
              console.error("synchronizeOneShard: read lock has timed out",
                            "for shard", shard);
              ok = false;
            }
          }
        } else {
          console.error("synchronizeOneShard: lockJobId was false for shard",
                        shard);
        }
        if (ok) {
          console.info("synchronizeOneShard: synchronization worked for shard",
                       shard);
        } else {
          throw "Did not work for shard " + shard + ".";  
          // just to log below in catch
        }
      }
    }
  }
  catch (err2) {
    console.error("synchronization of local shard '%s/%s' for central '%s/%s' failed: %s",
                  database, shard, database, planId, JSON.stringify(err2));
  }
  // Tell others that we are done:
  global.KEY_SET("shardSynchronization", "running", null);
  launchJob();  // start a new one if needed
}

////////////////////////////////////////////////////////////////////////////////
/// @brief schedule a shard synchronization
////////////////////////////////////////////////////////////////////////////////

function scheduleOneShardSynchronization(database, shard, planId, leader) {
  console.debug("scheduleOneShardSynchronization:", database, shard, planId,
                leader);
  var jobs;
  try {
    jobs = global.KEYSPACE_GET("shardSynchronization");
  }
  catch (e) {
    global.KEYSPACE_CREATE("shardSynchronization");
    global.KEY_SET("shardSynchronization", "scheduled", {});
    global.KEY_SET("shardSynchronization", "running", null);
    jobs = { scheduled: {}, running: null };
  }

  if ((jobs.running !== null && jobs.running.shard === shard) ||
      jobs.scheduled.hasOwnProperty(shard)) {
    console.debug("task is already running or scheduled,",
                  "ignoring scheduling request");
    return false;
  }

  // If we reach this, we actually have to schedule a new task:
  var jobInfo = { database, shard, planId, leader };
  jobs.scheduled[shard] = jobInfo;
  global.KEY_SET("shardSynchronization", "scheduled", jobs.scheduled);
  console.debug("scheduleOneShardSynchronization: have scheduled job", jobInfo);
  if (jobs.running === null) {  // no job scheduled, so start it:
    launchJob();
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief synchronize collections for which we are followers (synchronously
/// replicated shards)
////////////////////////////////////////////////////////////////////////////////

function synchronizeLocalFollowerCollections (plannedCollections,
                                              currentCollections) {
  var ok = true;
  var ourselves = global.ArangoServerState.id();

  var db = require("internal").db;
  db._useDatabase("_system");
  var localDatabases = getLocalDatabases();
  var database;

  // iterate over all matching databases
  for (database in plannedCollections) {
    if (plannedCollections.hasOwnProperty(database)) {
      if (localDatabases.hasOwnProperty(database)) {
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
                    if (inCurrent === undefined ||
                        ! inCurrent.hasOwnProperty("servers") ||
                        typeof inCurrent.servers !== "object" ||
                        typeof inCurrent.servers[0] !== "string" ||
                        inCurrent.servers[0] === "") {
                      console.debug("Leader has not yet created shard, let's",
                                    "come back later to this shard...");
                    } else {
                      if (inCurrent.servers.indexOf(ourselves) === -1) {
                        if (!scheduleOneShardSynchronization(
                                database, shard, collInfo.planId,
                                inCurrent.servers[0])) {
                          ok = false;
                        }
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
          db._useDatabase("_system");
          throw err;
        }

        db._useDatabase("_system");
      }
    }
  }
  return ok;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle collection changes
////////////////////////////////////////////////////////////////////////////////

function handleCollectionChanges (plan, current, takeOverResponsibility,
                                  writeLocked) {
  var plannedCollections = plan.Collections;
  var currentCollections = current.Collections;

  var ok = true;

  try {
    createLocalCollections(plannedCollections, plan.Version, currentCollections,
                           takeOverResponsibility, writeLocked);
    // Note that dropLocalCollections does not 
    // need the currentCollections, since they compare the plan with
    // the local situation.
    dropLocalCollections(plannedCollections, writeLocked);
    cleanupCurrentCollections(plannedCollections, currentCollections,
                              writeLocked);
    if (!synchronizeLocalFollowerCollections(plannedCollections,
                                             currentCollections)) {
      // If not all needed jobs have been scheduled, then work is still
      // ongoing, therefore we want to revisit this soon.
      ok = false;
    }
  }
  catch (err) {
    console.error("Caught error in handleCollectionChanges: " + 
                  JSON.stringify(err), JSON.stringify(err.stack));
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
  var dbs = db._databases();
  var i;
  var ok = true;
  for (i = 0; i < dbs.length; i++) {
    var database = dbs[i];
    try {
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
      console.error("Could not set up replication for database ", database, JSON.stringify(err));
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
  var dbs = db._databases();
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

function handleChanges (plan, current, writeLocked) {
  var changed = false;
  var role = ArangoServerState.role();
  if (role === "PRIMARY" || role === "SECONDARY") {
    // Need to check role change for automatic failover:
    var myId = ArangoServerState.id();
    if (role === "PRIMARY") {
      if (! plan.DBServers[myId]) {
        // Ooops! We do not seem to be a primary any more!
        changed = ArangoServerState.redetermineRole();
      }
    } else { // role === "SECONDARY"
      if (plan.DBServers[myId]) {
        changed = ArangoServerState.redetermineRole();
        if (!changed) {
          // mop: oops...changing role has failed. retry next time.
          return false;
        }
      } else {
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

  handleDatabaseChanges(plan, current, writeLocked);
  var success;
  if (role === "PRIMARY" || role === "COORDINATOR") {
    // Note: This is only ever called for DBservers (primary and secondary),
    // we keep the coordinator case here just in case...
    success = handleCollectionChanges(plan, current, changed, writeLocked);
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

var handlePlanChange = function (plan, current) {
  if (! isCluster() || isCoordinator() || ! global.ArangoServerState.initialized()) {
    return;
  }
  
  let versions = {
    plan: plan.Version,
    current: current.Version,
  };
    
  //////////////////////////////////////////////////////////////////////////////
  /// @brief execute an action under a write-lock
  //////////////////////////////////////////////////////////////////////////////

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
      
      let version = global.ArangoAgency.get(lockInfo.part + "/Version");
      versions[lockInfo.part.toLowerCase()] = version.arango[lockInfo.part].Version;
      
      global.ArangoAgency.unlockWrite(lockInfo.part, timeout);
    }
    catch (err) {
      global.ArangoAgency.unlockWrite(lockInfo.part, timeout);
      throw err;
    }
  }

  try {
    handleChanges(plan, current, writeLocked);

    console.info("plan change handling successful");
  } catch (err) {
    console.error("error details: %s", JSON.stringify(err));
    console.error("error stack: %s", err.stack);
    console.error("plan change handling failed");
  }
  return versions;
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
exports.handlePlanChange              = handlePlanChange;
exports.isCluster                     = isCluster;
exports.isCoordinator                 = isCoordinator;
exports.isCoordinatorRequest          = isCoordinatorRequest;
exports.role                          = role;
exports.shardList                     = shardList;
exports.status                        = status;
exports.wait                          = waitForDistributedResponse;
exports.endpointToURL                 = endpointToURL;
exports.synchronizeOneShard           = synchronizeOneShard;
