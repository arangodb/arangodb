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
var db = require("org/arangodb").db;
  
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

function handleDatabaseChanges (plan, current) {
  var plannedDatabases = getByPrefix(plan, "Plan/Databases/"); 
 // var currentDatabases = getByPrefix(current, "Current/Databases/"); 
  var localDatabases = db._listDatabases();
  
  var createDatabase = function (payload) { 
    ArangoAgency.set("Current/Databases/" + payload.name + "/" + ArangoServerState.id(), payload);
  };
  
  var dropDatabase = function (payload) { 
    try {
      ArangoAgency.remove("Current/Databases/" + payload.name + "/" + ArangoServerState.id());
    }
    catch (err) {
    }
  };

  var name;

  // check which databases need to be created locally
  for (name in plannedDatabases) {
    if (plannedDatabases.hasOwnProperty(name)) {
      if (localDatabases.indexOf(name) === -1) {
        // must create database

        var payload = plannedDatabases[name];
        // TODO: handle options and user information


        console.info("creating local database '%s'", payload.name);
        db._createDatabase(payload.name);

        writeLocked({ part: "Current" }, createDatabase, [ payload ]);
      }
    }
  }

  // check which databases need to be deleted locally
  localDatabases.forEach (function (name) {
    if (! plannedDatabases.hasOwnProperty(name)) {
      // must drop database

      console.info("dropping local database '%s'", name);
      db._dropDatabase(name);
        
      writeLocked({ part: "Current" }, dropDatabase, [ { name: name } ]);
    }
  });
}

function handleChanges (plan, current) {
  handleDatabaseChanges(plan, current);
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
