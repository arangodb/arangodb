/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, regexp: true plusplus: true */
/*global require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoShell client API for cluster operation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangosh = require("org/arangodb/arangosh");
var db = require("org/arangodb").db;

function Planner (userConfig) {
  "use strict";
  if (typeof userConfig !== "object") {
    throw "userConfig must be an object";
  }
  var r = db._connection.POST("/_admin/clusterPlanner",
                              JSON.stringify(userConfig));
  r = arangosh.checkRequestResult(r);
  this.clusterPlan = r.clusterPlan;
  this.config = r.config;
}

Planner.prototype.getPlan = function () {
  return this.clusterPlan;
};

function Kickstarter (clusterPlan, myname) {
  "use strict";
  if (typeof clusterPlan !== "object") {
    throw "clusterPlan must be an object";
  }
  this.clusterPlan = clusterPlan;
  if (myname === undefined) {
    this.myname = "me";
  }
  else {
    this.myname = myname;
  }
}

Kickstarter.prototype.launch = function () {
  "use strict";
  var r = db._connection.POST("/_admin/clusterDispatch",
                              JSON.stringify({"action": "launch",
                                              "clusterPlan": this.clusterPlan,
                                              "myname": this.myname}));
  this.runInfo = r.runInfo;
  return r;
};

Kickstarter.prototype.shutdown = function () {
  "use strict";
  var r = db._connection.POST("/_admin/clusterDispatch",
                              JSON.stringify({"action": "shutdown",
                                              "clusterPlan": this.clusterPlan,
                                              "myname": this.myname,
                                              "runInfo": this.runInfo}));
  return r;
};

Kickstarter.prototype.relaunch = function () {
  "use strict";
  var r = db._connection.POST("/_admin/clusterDispatch",
                              JSON.stringify({"action": "relaunch",
                                              "clusterPlan": this.clusterPlan,
                                              "myname": this.myname}));
  this.runInfo = r.runInfo;
  return r;
};

Kickstarter.prototype.cleanup = function () {
  "use strict";
  var r = db._connection.POST("/_admin/clusterDispatch",
                              JSON.stringify({"action": "cleanup",
                                              "clusterPlan": this.clusterPlan,
                                              "myname": this.myname}));
  return r;
};

Kickstarter.prototype.isHealthy = function () {
  "use strict";
  var r = db._connection.POST("/_admin/clusterDispatch",
                              JSON.stringify({"action": "isHealthy",
                                              "clusterPlan": this.clusterPlan,
                                              "myname": this.myname,
                                              "runInfo": this.runInfo}));
  return r;
};

Kickstarter.prototype.upgrade = function (username, password) {
  "use strict";
  if (username === undefined || password === undefined) {
    username = "root";
    password = "";
  }
  var r = db._connection.POST("/_admin/clusterDispatch",
                              JSON.stringify({"action": "upgrade",
                                              "clusterPlan": this.clusterPlan,
                                              "myname": this.myname,
                                              "username": username,
                                              "password": password}));
  this.runInfo = r.runInfo;
  return r;
};

function Upgrade () {
  var db = require("internal").db;
  var print = require("internal").print;
  var col = db._cluster_kickstarter_plans;
  print("\nHello, this is the ArangoDB cluster upgrade function...\n");
  if (col === undefined || col.count() === 0) {
    print("I did not find a cluster plan, therefore I give up.");
  }
  else {
    var x = col.any();
    if (db._version() >= "2.2" && x.hasOwnProperty("runInfo")) {
      print("Warning: It seems that your cluster has not been shutdown.");
      print("         Please shutdown your cluster via the web interface and");
      print("         then run this script again.");
    }
    else {
      var Kickstarter = require("org/arangodb/cluster").Kickstarter;
      var k = new Kickstarter(x.plan);
      var r = k.upgrade(x.user.name, x.user.passwd);
      if (r.error === true) {
        print("Error: Upgrade went wrong, here are more details:");
        print(JSON.stringify(r));
      }
      else {
        x.runInfo = r.runInfo;
        col.replace(x._key, x);
        print("Upgrade successful, your cluster is already running.");
      }
    }
  }
}

function Relaunch() {
  var db = require("internal").db;
  var print = require("internal").print;
  var col = db._cluster_kickstarter_plans;
  print("\nHello, this is the ArangoDB cluster relaunch function...\n");
  if (col === undefined || col.count() === 0) {
    print("I did not find a cluster plan, therefore I give up.");
  }
  else {
    var x = col.any();
    if (db._version() >= "2.2" && x.hasOwnProperty("runInfo")) {
      print("Warning: It seems your cluster has not been shutdown.");
    }
    else {
      var Kickstarter = require("org/arangodb/cluster").Kickstarter;
      var k = new Kickstarter(x.plan);
      var r = k.relaunch();
      if (r.error === true) {
        print("Error: Relaunch went wrong, here are more details:");
        print(JSON.stringify(r));
      }
      else {
        x.runInfo = r.runInfo;
        col.replace(x._key, x);
        print("Relaunch successful, your cluster is now running.");
      }
    }
  }
}

function Shutdown() {
  var db = require("internal").db;
  var print = require("internal").print;
  var col = db._cluster_kickstarter_plans;
  print("\nHello, this is the ArangoDB cluster shutdown function...\n");
  if (col === undefined || col.count() === 0) {
    print("I did not find a cluster plan, therefore I give up.");
  }
  else {
    var x = col.any();
    if (! x.hasOwnProperty("runInfo")) {
      print("Warning: It seems your cluster is already shutdown. I give up.");
    }
    else {
      var Kickstarter = require("org/arangodb/cluster").Kickstarter;
      var k = new Kickstarter(x.plan);
      k.runInfo = x.runInfo;
      var r = k.shutdown();
      if (r.error === true) {
        print("Error: Shutdown went wrong, here are more details:");
        print(JSON.stringify(r));
      }
      else {
        delete x.runInfo;
        db._cluster_kickstarter_plans.replace(x._key, x);
        print("Shutdown successful, your cluster is now stopped.");
      }
    }
  }
}

function IsHealthy() {
  var db = require("internal").db;
  var print = require("internal").print;
  var col = db._cluster_kickstarter_plans;
  print("\nHello, this is the ArangoDB cluster health check function...\n");
  if (col === undefined || col.count() === 0) {
    print("I did not find a cluster plan, therefore I give up.");
  }
  else {
    var x = col.any();
    if (! x.hasOwnProperty("runInfo")) {
      print("Warning: It seems your cluster is already shutdown. I give up.");
    }
    else {
      var Kickstarter = require("org/arangodb/cluster").Kickstarter;
      var k = new Kickstarter(x.plan);
      k.runInfo = x.runInfo;
      var r = k.isHealthy();
      if (r.error === true) {
        print("Error: Health check went wrong.");
        return r;
      }
      print("Health check successful.");
      return r;
    }
  }
}

exports.Planner = Planner;
exports.Kickstarter = Kickstarter;
exports.Upgrade = Upgrade;
exports.Relaunch = Relaunch;
exports.Shutdown = Shutdown;
exports.IsHealthy = IsHealthy;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
