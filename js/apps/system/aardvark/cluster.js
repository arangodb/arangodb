/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, unparam: true */
/*global require, applicationContext*/

////////////////////////////////////////////////////////////////////////////////
/// @brief A Foxx.Controller to show all Foxx Applications
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


"use strict";

// Initialise a new FoxxController called controller under the urlPrefix: "cluster".
var FoxxController = require("org/arangodb/foxx").Controller,
  controller = new FoxxController(applicationContext),
  _ = require("underscore"),
  Communication = require("org/arangodb/sharding/agency-communication"),
  comm = new Communication.Communication();
  
/** Get all DBServers
 *
 * Get a list of all running and expected DBServers
 * within the cluster
 */
controller.get("/DBServers", function(req, res) {
  var list = {
      Pavel: {
        role: "primary",
        secondary: "Sally",
        address: "tcp://192.168.0.1:1337"
      },
      Pancho: {
        role: "primary",
        secondary: "none",
        address: "tcp://192.168.0.2:1337"
      },
      Pablo: {
        role: "primary",
        secondary: "Sandy",
        address: "tcp://192.168.0.5:1337"
      },
      Sally: {
        role: "secondary",
        address: "tcp://192.168.1.1:1337"
      },
      Sandy: {
        role: "secondary",
        address: "tcp://192.168.1.5:1337"
      }
    },
    noBeat = ["Sandy"],
    serving = ["Pancho", "Pavel"],

    beats = comm.sync.Heartbeats(),
    resList = [];

    list = comm.current.DBServers().getList();
    noBeat = beats.noBeat();
    serving = beats.getServing();
    

  _.each(list, function(v, k) {
    v.name = k;
    resList.push(v);
    if (_.contains(noBeat, k)) {
      v.status = "critical";
      return;
    }
    if (v.role === "primary" && !_.contains(serving, k)) {
      v.status = "warning";
      return;
    }
    v.status = "ok";
  });
  res.json(resList);
});
