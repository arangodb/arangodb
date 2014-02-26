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
    cluster = require("org/arangodb/cluster"),
    _ = require("underscore");

  /** Plan and start a new cluster
   *
   * This will plan a new cluster with the information
   * given in the body
   */
  controller.post("/plan", function(req, res) {
    var config = {},
        input = req.body(),
        result = {},
        starter,
        i,
        tmp,
        planner;

    if (input.type === "testSetup") {
      config.dispatchers = {
        "d1": {
          "endpoint": "tcp://" + input.dispatcher
        }
      };
      config.numberOfDBservers = input.numberDBServers;
      config.numberOfCoordinators = input.numberCoordinators;
      result.config = config;
      planner = new cluster.Planner(config);
      result.plan = planner.getPlan();
      starter = new cluster.Kickstarter(planner.getPlan());
      tmp = starter.launch();
      result.runInfo = tmp.runInfo;
      res.json(result);
    } else {
      i = 0;
      config.dispatchers = {};
      config.numberOfDBservers = 0;
      config.numberOfCoordinators = 0;
      _.each(input.dispatcher, function(d) {
        i++;
        var inf = {};
        inf.endpoint = "tcp://" + d.host;
        if (d.isCoordinator) {
          config.numberOfCoordinators++;
        } else {
          inf.allowCoordinators = false;
        }
        if (d.isDBServer) {
          config.numberOfDBservers++;
        } else {
          inf.allowDBservers = false;
        }
        config.dispatchers["d" + i] = inf;
      });
      result.config = config;
      planner = new cluster.Planner(config);
      result.plan = planner.getPlan();
      starter = new cluster.Kickstarter(planner.getPlan());
      tmp = starter.launch();
      result.runInfo = tmp.runInfo;
      res.json(result);
    }
  });

  controller.post("/healthcheck", function(req, res) {
    var input = req.body();
    var k = new cluster.Kickstarter(input.plan);
    k.runInfo = input.runInfo;
    res.json(k.isHealthy());
  });

  controller.post("/shutdown", function(req, res) {
    var input = req.body();
    var k = new cluster.Kickstarter(input.plan);
    k.runInfo = input.runInfo;
    var shutdownInfo = k.shutdown();
    if (shutdownInfo.error) {
      res.json(shutdownInfo.results);
      res.status(409);
    }
  });

  controller.post("/cleanup", function(req, res) {
    var input = req.body();
    var k = new cluster.Kickstarter(input.plan);
    k.runInfo = input.runInfo;
    var shutdownInfo = k.shutdown();
    if (shutdownInfo.error) {
      res.json("Unable to shutdown cluster");
      res.status(409);
      return;
    }
    k.cleanup();
  });

  controller.post("/relaunch", function(req, res) {
    var input = req.body();
    var k = new cluster.Kickstarter(input.plan);
    var r = k.relaunch();
    if (r.error) {
      res.json("Unable to relaunch cluster");
      res.status(409);
      return;
    }
    res.json({
      config: input.config,
      plan: r.plan,
      runInfo: r.runInfo
    });
  });

  // FAKE TO BE REMOVED TODO 
  controller.get("/ClusterType", function(req, res) {
    res.json({
      type: "symmetricSetup"
    });
  });

  if (cluster.isCluster()) {
    // only make these functions available in cluster mode!

    var Communication = require("org/arangodb/cluster/agency-communication"),
    comm = new Communication.Communication(),
    beats = comm.sync.Heartbeats(),
    diff = comm.diff.current,
    servers = comm.current.DBServers(),
    dbs = comm.current.Databases(),
    coords = comm.current.Coordinators();


    /** Get the type of the cluster
     *
     * Returns a string containing the cluster type
     * Possible anwers:
     * - testSetup 
     * - symmetricalSetup
     * - asymmetricalSetup
     *
     */
    controller.get("/ClusterType", function(req, res) {
      // Not yet implemented
      res.json({
        type: "symmetricSetup"
      });
    });

    /** Get all DBServers
    *
    * Get a list of all running and expected DBServers
    * within the cluster
    */
    controller.get("/DBServers", function(req, res) {
      var resList = [],
        list = servers.getList(),
        diffList = diff.DBServers(),
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
      _.each(diffList.missing, function(v) {
        v.status = "missing";
        resList.push(v);
      });
      res.json(resList);
    });

    controller.get("/Coordinators", function(req, res) {
      var resList = [],
        list = coords.getList(),
        diffList = diff.Coordinators(),
        noBeat = beats.noBeat();
      
      _.each(list, function(url, k) {
        var v = {};
        v.name = k;
        v.address = url;
        resList.push(v);
        if (_.contains(noBeat, k)) {
          v.status = "critical";
          return;
        }
        v.status = "ok";
      });
      _.each(diffList.missing, function(v) {
        v.status = "missing";
        resList.push(v);
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
      try {
        res.json(_.map(selected.getCollections(),
          function(c) {
            return {name: c};
          })
        );
      } catch(e) {
        res.json([]);
      }
    });

    controller.get("/:dbname/:colname/Shards", function(req, res) {
      var dbname = req.params("dbname"),
        colname = req.params("colname"),
        selected = dbs.select(dbname).collection(colname);
      res.json(selected.getShardsByServers());
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

  } // end isCluster()

}());
