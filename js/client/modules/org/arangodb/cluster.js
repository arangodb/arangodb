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

var internal = require("internal");
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
  arangosh.checkRequestResult(r);
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
  arangosh.checkRequestResult(r);
  return r;
};

Kickstarter.prototype.relaunch = function () {
  "use strict";
  var r = db._connection.POST("/_admin/clusterDispatch",
                              JSON.stringify({"action": "relaunch",
                                              "clusterPlan": this.clusterPlan,
                                              "myname": this.myname}));
  arangosh.checkRequestResult(r);
  this.runInfo = r.runInfo;
  return r;
};

Kickstarter.prototype.cleanup = function () {
  "use strict";
  var r = db._connection.POST("/_admin/clusterDispatch",
                              JSON.stringify({"action": "cleanup",
                                              "clusterPlan": this.clusterPlan,
                                              "myname": this.myname}));
  arangosh.checkRequestResult(r);
  return r;
};

Kickstarter.prototype.isHealthy = function () {
  "use strict";
  var r = db._connection.POST("/_admin/clusterDispatch",
                              JSON.stringify({"action": "isHealthy",
                                              "clusterPlan": this.clusterPlan,
                                              "myname": this.myname,
                                              "runInfo": this.runInfo}));
  arangosh.checkRequestResult(r);
  return r;
};

exports.Planner = Planner;
exports.Kickstarter = Kickstarter;

    

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
