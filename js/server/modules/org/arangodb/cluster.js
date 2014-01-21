/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global ArangoAgency, ArangoServerState, require, exports */

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
var arangodb = require("org/arangodb");
var db = arangodb.db;
var ArangoCollection = arangodb.ArangoCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief get values from Plan or Current by a prefix
////////////////////////////////////////////////////////////////////////////////
  
function getByPrefix (values, prefix, multiDimensional) {
  var result = { };
  var a;
  var n = prefix.length;

  for (a in values) {
    if (values.hasOwnProperty(a)) {
      if (a.substr(0, n) === prefix) {
        var key = a.substr(n);

        if (multiDimensional) {
          var parts = key.split('/');
          if (! result.hasOwnProperty(parts[0])) {
            result[parts[0]] = { };
          }
          result[parts[0]][parts[1]] = values[a];
        }
        else {
          result[key] = values[a];
        }
      }
    }
  }
  return result;
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
/// @brief execute an action under a write-lock
////////////////////////////////////////////////////////////////////////////////

function writeLocked (lockInfo, cb, args) {
  var timeout = lockInfo.timeout;
  if (timeout === undefined) {
    timeout = 5;
  }

  var ttl = lockInfo.ttl;
  if (ttl === undefined) {
    ttl = 10;
  }

  ArangoAgency.lockWrite(lockInfo.part, ttl, timeout);
  
  try {
    cb.apply(this, args);
    ArangoAgency.increaseVersion(lockInfo.part + "/Version");
    ArangoAgency.unlockWrite(lockInfo.part, timeout); 
  }
  catch (err) {
    ArangoAgency.unlockWrite(lockInfo.part, timeout); 
    throw err;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return a hash with the local databases
////////////////////////////////////////////////////////////////////////////////

function getLocalDatabases () {
  var result = { };

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

  db._collections().forEach(function (collection) {
    var name = collection.name();

    if (name.substr(0, 1) !== '_') {
      result[name] = { 
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
          result[name][p] = properties[p];
        }
      }
    }
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create databases if they exist in the plan but not locally
////////////////////////////////////////////////////////////////////////////////

function createLocalDatabases (plannedDatabases) {
  var ourselves = ArangoServerState.id();

  var createDatabaseAgency = function (payload) { 
    ArangoAgency.set("Current/Databases/" + payload.name + "/" + ourselves, 
                     payload);
  };
  
  var localDatabases = getLocalDatabases();
  var name;

  // check which databases need to be created locally
  for (name in plannedDatabases) {
    if (plannedDatabases.hasOwnProperty(name)) {
      if (! localDatabases.hasOwnProperty(name)) {
        // must create database

        var payload = plannedDatabases[name];
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
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief drop databases if they do exist locally but not in the plan
////////////////////////////////////////////////////////////////////////////////

function dropLocalDatabases (plannedDatabases) {
  var ourselves = ArangoServerState.id();

  var dropDatabaseAgency = function (payload) { 
    try {
      ArangoAgency.remove("Current/Databases/" + payload.name + "/" + ourselves);
    }
    catch (err) {
      // ignore errors
    }
  };
  
  var localDatabases = getLocalDatabases();
  var name;

  // check which databases need to be deleted locally
  for (name in localDatabases) {
    if (localDatabases.hasOwnProperty(name)) {
      if (! plannedDatabases.hasOwnProperty(name)) {
        // must drop database

        console.info("dropping local database '%s'", name);
        db._dropDatabase(name);
        
        writeLocked({ part: "Current" }, 
                    dropDatabaseAgency, 
                    [ { name: name } ]);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle database changes
////////////////////////////////////////////////////////////////////////////////

function handleDatabaseChanges (plan, current) {
  var plannedDatabases = getByPrefix(plan, "Plan/Databases/"); 

  db._useDatabase("_system");
  createLocalDatabases(plannedDatabases);
  dropLocalDatabases(plannedDatabases);  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create collections if they exist in the plan but not locally
////////////////////////////////////////////////////////////////////////////////

function createLocalCollections (plannedCollections) {
  var ourselves = ArangoServerState.id();

  var createCollectionAgency = function (database, shard, payload) { 
    ArangoAgency.set("Current/Collections/" + database + "/" + payload.id + "/" + shard, 
                     payload);
  };
  
  var localDatabases = getLocalDatabases();
  var database;

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
              var payload = collections[collection];
              var shards = payload.shards;
              var shard;
            
              for (shard in shards) {
                if (shards.hasOwnProperty(shard)) {
                  if (shards[shard] === ourselves) {
                    // found a shard we are responsible for

                    if (! localCollections.hasOwnProperty(shard)) {
                      // must create this shard
                      payload.planId = payload.id;

                      console.info("creating local shard '%s/%s' for central '%s/%s'", 
                                   database, 
                                   shard, 
                                   database,
                                   payload.id);
           
                      try {
                        if (payload.type === ArangoCollection.TYPE_EDGE) {
                          db._createEdgeCollection(shard, payload);
                        }
                        else {
                          db._create(shard, payload);
                        }
                        payload.error = false;
                        payload.errorNum = 0;
                        payload.errorMessage = "no error";
                      }
                      catch (err2) {
                        payload.error = true;
                        payload.errorNum = err2.errorNum;
                        payload.errorMessage = err2.errorMessage;
                      }

                      writeLocked({ part: "Current" }, 
                                  createCollectionAgency, 
                                  [ database, shard, payload ]);
                    }
                    else {
                      // collection exists, now compare collection properties
                      var properties = { };
                      var cmp = [ "journalSize", "waitForSync", "doCompact" ], i;
                      for (i = 0; i < cmp.length; ++i) {
                        var p = cmp[i];
                        if (localCollections[shard][p] !== payload[p]) {
                          // property change
                          properties[p] = payload[p];
                        }
                      }
                        
                      if (Object.keys(properties).length > 0) {
                        console.info("updating properties for local shard '%s/%s'", 
                                     database, 
                                     shard);
                        
                        try {
                          db._collection(shard).properties(properties);
                          payload.error = false;
                          payload.errorNum = 0;
                          payload.errorMessage = "no error";

                        }
                        catch (err3) {
                          payload.error = true;
                          payload.errorNum = err3.errorNum;
                          payload.errorMessage = err3.errorMessage;
                        }

                        writeLocked({ part: "Current" }, 
                                    createCollectionAgency, 
                                    [ database, shard, payload ]);
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
/// @brief drop collections if they exist locally but not in the plan
////////////////////////////////////////////////////////////////////////////////

function dropLocalCollections (plannedCollections) {
  var ourselves = ArangoServerState.id();

  var dropCollectionAgency = function (database, shardID, id) {
    try { 
      ArangoAgency.remove("Current/Collections/" + database + "/" + id + "/" + shardID);
    }
    catch (err) {
      // ignore errors
    }
  };

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
                         (shardMap[collection] !== ourselves);

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
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handle collection changes
////////////////////////////////////////////////////////////////////////////////

function handleCollectionChanges (plan, current) {
  var plannedCollections = getByPrefix(plan, "Plan/Collections/", true); 

  db._useDatabase("_system");
  createLocalCollections(plannedCollections);
  
  db._useDatabase("_system");
  dropLocalCollections(plannedCollections);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief change handling trampoline function
////////////////////////////////////////////////////////////////////////////////

function handleChanges (plan, current) {
  handleDatabaseChanges(plan, current);
  handleCollectionChanges(plan, current);
}


var isCluster = function () {
  return (typeof ArangoServerState !== "undefined" &&
          ArangoServerState.initialised());
};

var isCoordinator = function () {
  if (! isCluster()) {
    return false;
  }

  return ArangoServerState.isCoordinator();
};

var role = function () {
  if (! isCluster()) {
    return undefined;
  }

  return ArangoServerState.role();
};

var status = function () {
  if (! isCluster()) {
    return undefined;
  }

  return ArangoServerState.status();
};

var isCoordinatorRequest = function (req) {
  if (! req || ! req.hasOwnProperty("headers")) {
    return false;
  }

  return req.headers.hasOwnProperty("x-arango-coordinator");
};

var handlePlanChange = function () {
  if (! isCluster() || isCoordinator()) {
    return;
  }

  try {
    var plan    = ArangoAgency.get("Plan", true);
    var current = ArangoAgency.get("Current", true);
 
    handleChanges(plan, current);
    console.info("plan change handling successful");
  }
  catch (err) {
    require("internal").print(err);
    console.error("plan change handling failed");
  }
};

exports.isCluster            = isCluster;
exports.isCoordinator        = isCoordinator;
exports.role                 = role;
exports.status               = status;
exports.isCoordinatorRequest = isCoordinatorRequest;
exports.handlePlanChange     = handlePlanChange;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
