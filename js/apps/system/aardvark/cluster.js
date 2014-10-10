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
    load = require("internal").download,
    _ = require("underscore");

  /** Plan and start a new cluster
   *
   * This will plan a new cluster with the information
   * given in the body
   */
  controller.get("/amICoordinator", function(req, res) {
    res.json(cluster.isCoordinator());
  });

  controller.get("/amIDispatcher", function(req, res) {
    res.json(!cluster.dispatcherDisabled());
  });

  if (!cluster.dispatcherDisabled()) {
    var Plans = require("./repositories/plans.js"),
      plans = new Plans.Repository(
        require("internal").db._collection(
          "_cluster_kickstarter_plans"
        )),
      getStarter = function() {
        var config = plans.loadConfig(),
          k;
        if (!config) {
          return;
        }
        k = new cluster.Kickstarter(config.plan);
        k.runInfo = config.runInfo;
        return k;
      },
      parseUser = function(header) {
        if (header && header.authorization) {
          var auth = require("internal").base64Decode(
            header.authorization.substr(6)
          );
          return {
            username: auth.substr(0, auth.indexOf(":")),
            passwd: auth.substr(auth.indexOf(":")+1) || ""
          };
        }
        return {
          username: "root",
          passwd: ""
        };
      },
      startUp = function(req, res) {
        cleanUp();
        var config = {},
            input = req.body(),
            result = {},
            starter,
            i,
            tmp,
            planner,
            auth,
            uname,
            pwd;
        auth = parseUser(req.headers);
        uname = auth.username;
        pwd = auth.passwd;
        if (input.type === "testSetup") {
          config.dispatchers = {
            "d1": {
              "username": uname,
              "passwd": pwd,
              "endpoint": "tcp://" + input.dispatchers
            }
          };
          config.numberOfDBservers = input.numberDBServers;
          config.numberOfCoordinators = input.numberCoordinators;
        } else {
          i = 0;
          config.dispatchers = {};
          config.numberOfDBservers = 0;
          config.numberOfCoordinators = 0;
          _.each(input.dispatchers, function(d) {
            i++;
            var inf = {};
            inf.username = d.username;
            inf.passwd = d.passwd;
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
          config.useSSLonDBservers = input.useSSLonDBservers;
          config.useSSLonCoordinators = input.useSSLonCoordinators;
        }
        result.config = config;
        planner = new cluster.Planner(config);
        result.plan = planner.getPlan();
        starter = new cluster.Kickstarter(planner.getPlan());
        tmp = starter.launch();
        result.runInfo = tmp.runInfo;
        result.user = {
          name: "root",
          passwd: ""
        };
        plans.storeConfig(result);
        res.json(result);
      },
      cleanUp = function() {
        var k = getStarter();
        if (k) {
          k.cleanup();
        }
      };
    // only make these functions available in dispatcher mode!
    controller.post("/plan", startUp);
    controller.put("/plan", startUp);

    controller.put("/plan/credentials", function(req, res) {
      var body = req.body(),
        u = body.user,
        p = body.passwd;
      plans.saveCredentials(u, p);
    });

    controller.get("/plan", function(req, res) {
      res.json(plans.loadConfig());
    });

    controller.del("/plan/cleanUp", function(req, res) {
      cleanUp();
      res.json("ok");
    });

    controller.post("/communicationCheck", function(req, res) {
      var list = req.body();
      var options = {};
      var result = [];
      var base64Encode = require("internal").base64Encode;
      _.each(list, function(info) {
        var host = info.host;
        var port = info.port;
        if (info.user) {
          options.headers = {
            "Authorization": "Basic " + base64Encode(info.user + ":" +
              info.passwd)
          };
        }
        var url = "http://" + host + ":" + port + "/_api/version";
        var resi = load(url, "", options);
        if (resi.code !== 200) {
          result.push(false);
        } else {
          result.push(true);
        }
        delete options.headers;
      });
      res.json(result);
    });

    controller.del("/plan", function(req, res) {
      plans.clear();
      res.json("ok");
    });

    controller.get("/healthcheck", function(req, res) {
      var out = getStarter().isHealthy();
      var answer = true;
      if (out.error) {
        _.each(out.results, function(res) {
          if (res.answering) {
            answer = (_.any(res.answering, function(i) {
              return i === true;
            }));
          }
        });
      }
      res.json(answer);
    });

    controller.get("/shutdown", function(req, res) {
      var k = getStarter();
      var shutdownInfo = k.shutdown();
      if (shutdownInfo.error) {
        require("console").log("%s", JSON.stringify(shutdownInfo.results));
      }
      plans.removeRunInfo();
    });

    controller.get("/upgrade", function(req, res) {
      //uname pswd

      var k = getStarter();
      var u = plans.getCredentials();
      var r = k.upgrade(u.name, u.passwd);
      if (r.error === true) {
        res.json(r);
        res.status = 500;
      }
      else {
        plans.replaceRunInfo(r.runInfo);
        res.json("ok");
      }

    });

    controller.get("/cleanup", function(req, res) {
      var k = getStarter();
      var shutdownInfo = k.shutdown();
      cleanUp();
      if (shutdownInfo.error) {
        require("console").log("%s", JSON.stringify(shutdownInfo.results));
        return;
      }
    });

    controller.get("/relaunch", function(req, res) {
      var k = getStarter();
      var u = plans.getCredentials();
      var r = k.relaunch(u.name, u.passwd);
      if (r.error) {
        res.json("Unable to relaunch cluster");
        res.status(409);
        return;
      }
      var result = {
        plan: r.plan,
        runInfo: r.runInfo
      };
      result = plans.updateConfig(result);
      res.json(result);
    });
  }
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
        didBeat = beats.didBeat(),
        serving = beats.getServing();

      _.each(list, function(v, k) {
        v.name = k;
        resList.push(v);
        if (!_.contains(didBeat, k)) {
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
        didBeat = beats.didBeat();

      _.each(list, function(v, k) {
        v.name = k;
        resList.push(v);
        if (!_.contains(didBeat, k)) {
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
