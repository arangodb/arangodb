/*jslint indent: 2, nomen: true, maxlen: 120, regexp: true */
/*global module, require, exports */

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
  
  var agency,
    _AgencyWrapper,
    splitServerName,
    storeServersInCache,
    Target,
    mapCollectionIDsToNames,
    _ = require("underscore");

  splitServerName = function(route) {
    var splits = route.split("/");
    return splits[splits.length - 1];
  };


  mapCollectionIDsToNames = function(list) {
    var res = {};
    _.each(list, function(v, k) {
      res[JSON.parse(v).name] = splitServerName(k);
    });
    return res;
  };

  _AgencyWrapper = function() {
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
        return _agency.get(route, recursive);
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
        return _agency.list(route).sort();
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
    var addLevelsForDBs = function(base, writeAccess) {
      var list = base.list();
      _.each(list, function(d) {
        addLevel(base, d, d, ["get", "checkVersion"]);
        var colList = mapCollectionIDsToNames(base[d].get(true));
        var acts = ["get"];
        if (writeAccess) {
          acts.push("set");
        }
        _.each(colList, function(id, name) {
          addLevel(base[d], name, id, acts);
        });
      });
    };
    var target = addLevel(this, "target", "Target");
    addLevel(target, "dbServers", "DBServers", ["get", "set", "remove", "checkVersion"]);
    addLevel(target, "db", "Collections", ["list"]);
    addLevelsForDBs(target.db, true);
    addLevel(target, "coordinators", "Coordinators", ["list", "set", "remove", "checkVersion"]);
    var plan = addLevel(this, "plan", "Plan");
    addLevel(plan, "dbServers", "DBServers", ["get", "checkVersion"]);
    addLevel(plan, "db", "Collections", ["list"]);
    addLevelsForDBs(plan.db);
    addLevel(plan, "coordinators", "Coordinators", ["list", "checkVersion"]);
    var current = addLevel(this, "current", "Current");
    addLevel(current, "dbServers", "DBServers", ["get", "checkVersion"]);
    addLevel(current, "db", "Collections", ["list"]);
    addLevelsForDBs(current.db);
    addLevel(current, "coordinators", "Coordinators", ["list", "checkVersion"]);
    addLevel(current, "registered", "ServersRegistered", ["get", "checkVersion"]);

    var sync = addLevel(this, "sync", "Sync");
    addLevel(sync, "beat", "ServerStates", ["get"]);
    addLevel(sync, "interval", "HeartbeatIntervalMs", ["get"]);

  }

  agency = new _AgencyWrapper(); 


// -----------------------------------------------------------------------------
// --SECTION--                                       Update Wanted Configuration
// -----------------------------------------------------------------------------

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
      }
    }
  };

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

  var ColObject = function(route, writeAccess) {
    this.info = function() {
      return JSON.parse(route.get());
    };
    this.getShards = function() {
      var info = this.info();
      return info.shards;
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
        return route.set(JSON.stringify(toUpdate));
      };
    }
  };

  var DBObject = function(route, writeAccess) {
    var cache;
    var getList = function() {
      if (!cache || !route.checkVersion()) {
        cache = _.keys(mapCollectionIDsToNames(route.get(true))).sort();
      }
      return cache;
    };
    this.getCollections = function() {
      return getList();
    };
    this.collection = function(name) {
      var colroute = route[name];
      if (!colroute) {
        return false;
      }
      return new ColObject(colroute, writeAccess);
    };
  };

  var DatabasesObject = function(route, writeAccess) {
    this.getList = function() {
      return route.list();            
    };
    this.select = function(name) {
      var subroute = route[name];
      if (!subroute) {
        return false;
      }
      return new DBObject(subroute, writeAccess);
    };
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                            Vision
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                            Target
// -----------------------------------------------------------------------------

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

  var Sync = function() {
    var Heartbeats;
    var interval = agency.sync.interval.get();

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
        return agency.sync.beat.get(true);
      };
      this.getInactive = function() {
        var list = this.list();
        var res = [];
        _.each(list, function(v, k) {
          if (isInactive(v.status)) {
            require("internal").print(k, isInSync(v.status), isOutSync(v.status));
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
// --SECTION--                                             Global Object binding
// -----------------------------------------------------------------------------

  this.target = new Target();
  this.plan = new Plan();
  this.current = new Current();
  this.sync = new Sync();

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
}
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
  var agency;
  if (agency) {
    return agency;
  }
  agency = new ArangoAgency();
  return agency;
};



// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
