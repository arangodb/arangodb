/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, stupid: true */
/*global module, require, exports, ArangoAgency */

////////////////////////////////////////////////////////////////////////////////
/// @brief Agency Communication
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
/// @author Michael Hackstein
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                              Agency-Communication
// -----------------------------------------------------------------------------
exports.Communication = function() {
  "use strict"; 
  var agency,
    AgencyWrapper,
    splitServerName,
    storeServersInCache,
    Target,
    mapCollectionIDsToNames,
    updateCollectionRouteForName,
    updateDatabaseRoutes,
    difference,
    self = this,
    _ = require("underscore");

  splitServerName = function(route) {
    var splits = route.split("/");
    return splits[splits.length - 1];
  };


  mapCollectionIDsToNames = function(list) {
    var res = {};
    _.each(list, function(v, k) {
      var n = splitServerName(k);
      if (n === "Lock" || n === "Version") {
        return;
      }
      res[v.name] = n;
    });
    return res;
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                     AgencyWrapper
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for Agency
///
/// Creates access to routes via objects instead of route strings.
/// And defines specific operations allowed on these routes.
////////////////////////////////////////////////////////////////////////////////

  AgencyWrapper = function() {
    var _agency = exports._createAgency();
    var routes = { 
      vision: "Vision/",
      target: "Target/",
      plan: "Plan/",
      current: "Current/",
      fail: "Fail/"
    };
    var stubs = {
      get: function(route, recursive) {
        var res = _agency.get(route, recursive);
        return res;
      },
      set: function(route, name, value) {
        if (value !== undefined) {
          return _agency.set(route + "/" + name, value);
        }
        return _agency.set(route, name);
      },
      remove: function(route, name) {
        return _agency.remove(route + "/" + name);
      },
      checkVersion: function(route) {
        return false;
      },
      list: function(route) {
        return _.map(_agency.list(route, false, true), splitServerName).sort();
      }
    };
    var addLevel = function(base, name, route, functions) {
      var newRoute = base.route;
      if (newRoute) {
        newRoute += "/";
      } else {
        newRoute = "";
      }
      newRoute += route;
      var newLevel = {
        route: newRoute
      };
      _.each(functions, function(f) {
        newLevel[f] = stubs[f].bind(null, newLevel.route); 
      });
      base[name] = newLevel;
      return newLevel;
    };
    var target = addLevel(this, "target", "Target");
    addLevel(target, "dbServers", "DBServers", ["get", "set", "remove", "checkVersion"]);
    addLevel(target, "db", "Databases", ["list"]);
    addLevel(target, "coordinators", "Coordinators", ["list", "set", "remove", "checkVersion"]);
    var plan = addLevel(this, "plan", "Plan");
    addLevel(plan, "dbServers", "DBServers", ["get", "checkVersion"]);
    addLevel(plan, "db", "Databases", ["list"]);
    addLevel(plan, "coordinators", "Coordinators", ["list", "checkVersion"]);
    var current = addLevel(this, "current", "Current");
    addLevel(current, "dbServers", "DBServers", ["get", "checkVersion"]);
    addLevel(current, "db", "Databases", ["list"]);
    addLevel(current, "coordinators", "Coordinators", ["list", "checkVersion"]);
    addLevel(current, "registered", "ServersRegistered", ["get", "checkVersion"]);

    var sync = addLevel(this, "sync", "Sync");
    addLevel(sync, "beat", "ServerStates", ["get"]);
    addLevel(sync, "interval", "HeartbeatIntervalMs", ["get"]);

    this.addLevel = addLevel;
  };

  agency = new AgencyWrapper(); 


// -----------------------------------------------------------------------------
// --SECTION--                                                  Helper Functions
// -----------------------------------------------------------------------------

  updateDatabaseRoutes = function(base, writeAccess) {
    var list = self.plan.Databases().getList();
    _.each(_.keys(base), function(k) {
      if (k !== "route" && k !== "list") {
        delete base[k];
      }
    });
    var oldRoute = base.route;
    base.route = base.route.replace("Databases", "Collections")
    _.each(list, function(d) {
      agency.addLevel(base, d, d, ["get", "checkVersion"]);
    });
    base.route = oldRoute;
  };

  updateCollectionRouteForName = function(route, db, name, writeAccess) {
    var list = self.plan.Databases().select(db).getCollectionObjects();
    var cId = null;
    _.each(list, function(v, k) {
      if (v.name === name) {
        cId = splitServerName(k);
      }
    });
    var acts = ["get"];
    if (writeAccess) {
      acts.push("set");
    }
    agency.addLevel(route, name, cId, acts);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Stores database servers in cache
///
/// Stores a list of database servers into a cache object. 
/// It will convert their roles accordingly. 
////////////////////////////////////////////////////////////////////////////////

  storeServersInCache = function(place, servers) {
    _.each(servers, function(v, k) {
      var pName = splitServerName(k);
      place[pName] = place[pName] || {};
      place[pName].role = "primary";
      if (v !== "none") {
        place[v] = {
          role: "secondary"
        };
        place[pName] = place[pName] || {};
        place[pName].secondary = v;
      }
    });
  };


// -----------------------------------------------------------------------------
// --SECTION--                                               Object Constructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Object for DBServer configuration
///
/// Allows to list all database servers for the given hierarchy level.
/// If write access is granted also options to add
/// and remove servers are allowed.
////////////////////////////////////////////////////////////////////////////////

  var DBServersObject = function(route, writeAccess) {
    var cache = {};
    var servers;
    var getList = function() {
      if (!route.checkVersion()) {
        cache = {};
        servers = route.get(true);
        storeServersInCache(cache, servers);
      }
      return cache;
    };
    this.getList = function() {
      return getList();
    };
    if (writeAccess) {
      this.addPrimary = function(name) {
        return route.set(name, "none");
      };
      this.addSecondary = function(name, primaryName) {
        return route.set(primaryName, name);
      };
      this.addPair = function(primaryName, secondaryName) {
        return route.set(primaryName, secondaryName);
      };
      this.removeServer = function(name) {
        var res = -1;
        _.each(getList(), function(opts, n) {
          if (n === name) {
            // The removed server is a primary
            if (opts.role === "primary") {
              res = route.remove(name);
              if (!res) {
                res = -1;
                return;
              }
              if (opts.secondary !== "none") {
                res = route.set(opts.secondary, "none");
              }
              return;
            }
          }
          if (opts.role === "primary" && opts.secondary === name) {
            res = route.set(n, "none");
            return;
          }
        });
        return res;
      };
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Object for Coordinator configuration
///
/// Allows to list all coordinators for the given hierarchy level.
/// If write access is granted also options to add
/// and remove servers are allowed.
////////////////////////////////////////////////////////////////////////////////
  var CoordinatorsObject = function(route, writeAccess) {
    this.getList = function() {
      return route.list();
    };
    if (writeAccess) {
      this.add = function(name) {
        return route.set(name, true);
      };
      this.remove = function(name) {
        return route.remove(name);
      };
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Object for Collection configuration
///
/// Allows to collect the information stored about a collection. 
/// This includes indices, journalSize etc. and also shards. 
/// Also convenience functions for shards are added,
/// allowing to get a mapping server -> shards
/// and a mapping shard -> server.
/// If write access is granted also an option to move a shard is added.
////////////////////////////////////////////////////////////////////////////////
  var ColObject = function(route, writeAccess) {
    this.info = function() {
      var res = route.get();
      return _.values(res)[0];
    };
    this.getShards = function() {
      var info = this.info();
      if (!info) {
        return;
      }
      return info.shards;
    };
    this.getShardsByServers = function() {
      var list = this.getShards();
      var res = {};
      _.each(list, function(v, k) {
        require("internal").print(v);
        res[v] = res[v] || {
          shards: [],
          name: v
        };
        res[v].shards.push(k);
      });
      var resList = [];
      _.each(res, function(v) { resList.push(v);});
      return resList;
    };
    this.getShardsForServer = function(name) {
      var list = this.getShards();
      var res = [];
      _.each(list, function(v, k) {
        if (v === name) {
          res.push(k);
        }
      });
      return res;
    };
    this.getServerForShard = function(name) {
      var list = this.getShards();
      return list[name];
    };
    if (writeAccess) {
      this.moveShard = function(shard, target) {
        var toUpdate = this.info();
        toUpdate.shards[shard] = target;
        return route.set(toUpdate);
      };
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Object for content of a Database
///
/// Allows to collect the information stored about a database. 
/// It allos to get a list of collections and to select one of them for 
/// further information.
////////////////////////////////////////////////////////////////////////////////
  var DBObject = function(route, db, writeAccess) {
    var cache;
    var getRaw = function() {
      if (!cache || !route.checkVersion()) {
        cache = route.get(true);
      }
      return cache;
    };
    var getList = function() {
      return _.keys(mapCollectionIDsToNames(
        self.plan.Databases().select(db).getCollectionObjects()
      )).sort();
    };
    this.getCollectionObjects = function() {
      return getRaw();
    };
    this.getCollections = function() {
      return getList();
    };
    this.collection = function(name) {
      updateCollectionRouteForName(route, db, name, writeAccess);
      var colroute = route[name];
      if (!colroute) {
        return false;
      }
      return new ColObject(colroute, writeAccess);
    };
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Object for Database configuration
///
/// Allows to list all added databases.
/// Also allows to select one database for further information.
////////////////////////////////////////////////////////////////////////////////

  var DatabasesObject = function(route, writeAccess) {
    this.getList = function() {
      return route.list();            
    };
    this.select = function(name) {
      updateDatabaseRoutes(route, writeAccess);
      var subroute = route[name];
      if (!subroute) {
        return false;
      }
      return new DBObject(subroute, name, writeAccess);
    };
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                            Vision
// -----------------------------------------------------------------------------

  // Not yet defined

// -----------------------------------------------------------------------------
// --SECTION--                                                            Target
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Access point for "Target" level.
///
/// Gives access to all information stored in the target level.
/// This includes DBServers, Databases and Coordinators.
/// Also grants write access.
////////////////////////////////////////////////////////////////////////////////

  Target = function() {
    var DBServers;
    var Databases;
    var Coordinators;

    this.DBServers = function() {
      if (!DBServers) {
        DBServers = new DBServersObject(agency.target.dbServers, true);
      }
      return DBServers;
    };

    this.Databases = function() {
      if (!Databases) {
        Databases = new DatabasesObject(agency.target.db, true);
      }
      return Databases;
    };

    this.Coordinators = function() {
      if (!Coordinators) {
        Coordinators = new CoordinatorsObject(agency.target.coordinators, true);
      }
      return Coordinators;
    };
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                              Plan
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Access point for "Plan" level.
///
/// Gives access to all information stored in the plan level.
/// This includes DBServers, Databases and Coordinators.
/// Does not grant write access.
////////////////////////////////////////////////////////////////////////////////

  var Plan = function () {
    var DBServers;
    var Databases;
    var Coordinators;

    this.DBServers = function() {
      if (!DBServers) {
        DBServers = new DBServersObject(agency.plan.dbServers);
      }
      return DBServers;
    };

    this.Databases = function() {
      if (!Databases) {
        Databases = new DatabasesObject(agency.plan.db);
      }
      return Databases;
    };

    this.Coordinators = function() {
      if (!Coordinators) {
        Coordinators = new CoordinatorsObject(agency.plan.coordinators);
      }
      return Coordinators;
    };
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                           Current
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Access point for "Current" level.
///
/// Gives access to all information stored in the current level.
/// This includes DBServers, Databases and Coordinators.
/// Furthermore this consideres IP addresses of all servers.
/// Does not grant write access.
////////////////////////////////////////////////////////////////////////////////

  var Current = function () {
    var DBServers;
    var Databases;
    var Coordinators;

    var DBServersObject = function() {
      var cache = {};
      var servers;
      var getList = function() {
        if (
            !agency.current.dbServers.checkVersion()
            || !agency.current.registered.checkVersion()
          ) {
          cache = {};
          servers = agency.current.dbServers.get(true);
          storeServersInCache(cache, servers);
          var addresses = agency.current.registered.get(true);
          _.each(addresses, function(v, k) {
            var pName = splitServerName(k);
            if (cache[pName]) {
              cache[pName].address = v;
            }
          });
        }
        return cache;
      };
      this.getList = function() {
        return getList();
      };
    };
    var CoordinatorsObject = function() {
      var cache;
      var servers;
      this.getList = function() {
        if (
            !agency.current.coordinators.checkVersion()
            || !agency.current.registered.checkVersion()
           ) {
          cache = {};
          servers = agency.current.coordinators.list();
          var addresses = agency.current.registered.get(true);
          _.each(addresses, function(v, k) {
            var pName = splitServerName(k);
            if (_.contains(servers, pName)) {
              cache[pName] = v;
            }
          });
        }
        return cache;
      };
    };

    this.DBServers = function() {
      if (!DBServers) {
        DBServers = new DBServersObject();
      }
      return DBServers;
    };

    this.Databases = function() {
      if (!Databases) {
        Databases = new DatabasesObject(agency.current.db);
      }
      return Databases;
    };

    this.Coordinators = function() {
      if (!Coordinators) {
        Coordinators = new CoordinatorsObject();
      }
      return Coordinators;
    };

  };
  
// -----------------------------------------------------------------------------
// --SECTION--                                                              Sync
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Access point for "Sync" level.
///
/// Gives access to all information stored in the sync level.
/// This includes the heartbeat of the servers.
/// Offers convenience functions to get serving primaries,
/// list of primaries and secondaries inSync and out of sync
/// and a list of all inactive servers (servers that do not serve
/// or act as secondary).
/// Furthermore offers a function to check if a server has not send a heartbeat
/// in time.
/// Does not grant write access.
////////////////////////////////////////////////////////////////////////////////

  var Sync = function() {
    var Heartbeats;
    var interval = agency.sync.interval.get();
    interval = _.values(interval)[0];

    var didBeatInTime = function(time) {

    };

    var isInSync = function(status) {
      return (status === "SERVINGSYNC" || status === "INSYNC");
    };
    var isOutSync = function(status) {
      return (status === "SERVINGASYNC" || status === "SYNCING");
    };
    var isServing = function(status) {
      return (status === "SERVINGASYNC" || status === "SERVINGSYNC");
    };
    var isInactive = function(status) {
      return !isInSync(status) && !isOutSync(status);
    };

    var HeartbeatsObject = function() {
      this.list = function() {
        var res = agency.sync.beat.get(true);
        _.each(res, function(v, k) {
          delete res[k];
          res[splitServerName(k)] = v;  
        });
        return res;
      };
      this.getInactive = function() {
        var list = this.list();
        var res = [];
        _.each(list, function(v, k) {
          if (isInactive(v.status)) {
            res.push(k);
          }
        });
        return res.sort();
      };
      this.getServing = function() {
        var list = this.list();
        var res = [];
        _.each(list, function(v, k) {
          if (isServing(v.status)) {
            res.push(k);
          }
        });
        return res.sort();
      };
      this.getInSync = function() {
        var list = this.list();
        var res = [];
        _.each(list, function(v, k) {
          if (isInSync(v.status)) {
            res.push(k);
          }
        });
        return res.sort();
      };
      this.getOutSync = function() {
        var list = this.list();
        var res = [];
        _.each(list, function(v, k) {
          if (isOutSync(v.status)) {
            res.push(k);
          }
        });
        return res.sort();
      };
      this.noBeat = function() {
        var lastAccepted = new Date((new Date()).getTime() - (2 * interval));
        var res = [];
        var list = this.list();
        _.each(list, function(v, k) {
          if (new Date(v.time) < lastAccepted) {
            res.push(k);
          }
        });
        return res.sort();
      };

    };

    this.Heartbeats = function() {
      if (!Heartbeats) {
        Heartbeats = new HeartbeatsObject();
      }
      return Heartbeats;
    };

  };

// -----------------------------------------------------------------------------
// --SECTION--                                                              Diff
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Access point to visualize differences between levels.
///
/// Gives convenient access to compute discrepancies between levels.
/// It allows to get differences between target and plan.
/// (These require a user to step in, the managers do not have enough
/// resources to reach the target).
/// And the differences between plan and current.
/// (These do not require a user to step in, its the managers duty)
/// Supports differences for DBServers and Coordinators.
/// Databases not yet included.
////////////////////////////////////////////////////////////////////////////////

  var Diff = function(target, plan, current) {

    var DiffObject = function(supRoute, infRoute, supName) {
      var infName;
      switch (supName) {
        case "target":
          infName = "plan";
          break;
        case "plan":
          infName = "current";
          break;
        default:
          throw "Sorry please give a correct superior name";
      }
      var difference = function(superior, inferior) {
        var diff = {
          missing: [],
          difference: {}
        };
        var comp;
        if (_.isArray(superior)) {
          comp = inferior;
          if (!_.isArray(inferior)) {
            // Current stores ips no array
            comp = _.keys(inferior);
          }
          _.each(superior, function(v) {
            if (!_.contains(comp, v)) {
              diff.missing.push(v);
            }
          });
          return diff;
        }
        _.each(superior, function(v, k) {
          if (!inferior.hasOwnProperty(k)) {
            diff.missing.push(k);
            return;
          }
          var compTo = _.clone(inferior[k]);
          delete compTo.address;
          if (JSON.stringify(v) !== JSON.stringify(compTo)) {
            diff.difference[k] = {};
            diff.difference[k][supName] = v;
            diff.difference[k][infName] = inferior[k];
          }
        });
        return diff;
      };
      this.DBServers = function() {
        return difference(supRoute.DBServers().getList(), infRoute.DBServers().getList()); 
      };
      this.Coordinators = function() {
        return difference(supRoute.Coordinators().getList(), infRoute.Coordinators().getList()); 
      };
    };

    this.plan = new DiffObject(target, plan, "target");
    this.current = new DiffObject(plan, current, "plan");

  };

// -----------------------------------------------------------------------------
// --SECTION--                                             Global Object binding
// -----------------------------------------------------------------------------

  this.target = new Target();
  this.plan = new Plan();
  this.current = new Current();
  this.sync = new Sync();
  this.diff = new Diff(this.target, this.plan, this.current);

// -----------------------------------------------------------------------------
// --SECTION--                                      Global Convenience functions
// -----------------------------------------------------------------------------

  this.addPrimary = this.target.DBServers().addPrimary;
  this.addSecondary = this.target.DBServers().addSecondary;
  this.addCoordinator = this.target.Coordinators().add;
  this.addPair = this.target.DBServers().addPair;
  this.removeServer = function(name) {
    var res = this.target.DBServers().removeServer(name);
    if (res === -1) {
      return this.target.Coordinators().remove(name);
    }
    return res;
  };
};
////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_agency-communication_agency
/// @brief A wrapper around the Agency initialisation
///
/// @FUN{_createAgency()}
///
/// This returns a singleton instance for the agency or creates it.
///
/// @EXAMPLES
///
/// @code
///     agency  = communication._createAgency();
/// @endcode
////////////////////////////////////////////////////////////////////////////////
exports._createAgency = function() {
  "use strict";
  var agency;
  if (agency) {
    return agency;
  }
  agency = ArangoAgency;
  return agency;
};



// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
