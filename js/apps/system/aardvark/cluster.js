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

(function() {

  "use strict";

  // Initialise a new FoxxController called controller under the urlPrefix: "cluster".
  var FoxxController = require("org/arangodb/foxx").Controller,
    controller = new FoxxController(applicationContext),
    _ = require("underscore"),
    Communication = require("org/arangodb/sharding/agency-communication"),
    comm = new Communication.Communication(),
    beats = comm.sync.Heartbeats(),
    servers = comm.current.DBServers(),
    dbs = comm.current.Databases(),
    coords = comm.current.Coordinators();
    
  /** Get all DBServers
   *
   * Get a list of all running and expected DBServers
   * within the cluster
   */
  controller.get("/DBServers", function(req, res) {
    var resList = [],
      list = servers.getList(),
      noBeat = beats.noBeat(),
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

  controller.get("/Coordinators", function(req, res) {
    var resList = [],
      list = coords.getList(),
      noBeat = beats.noBeat();
    
    _.each(list, function(url, k) {
      var v = {};
      v.name = k;
      v.url = url;
      resList.push(v);
      if (_.contains(noBeat, k)) {
        v.status = "critical";
        return;
      }
      v.status = "ok";
    });
    res.json(resList);
  });

  controller.get("/Databases", function(req, res) {
    var list = dbs.getList();
    res.json(_.map(list, function(d) {
      return {name: d};
    }));
  });

  controller.get("/:dbname/Collections", function(req, res) {
    var dbname = req.params("dbname"),
      selected = dbs.select(dbname);
    res.json(_.map(selected.getCollections(),
      function(c) {
        return {name: c};
      })
    );
  });

  controller.get("/:dbname/:colname/Shards/:servername", function(req, res) {
    var dbname = req.params("dbname"),
      colname = req.params("colname"),
      servername = req.params("servername"),
      selected = dbs.select(dbname).collection(colname);
    res.json(_.map(selected.getShardsForServer(servername),
      function(c) {
        return {id: c};
      })
    );
  });

}());
