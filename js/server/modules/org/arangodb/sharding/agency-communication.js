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
    cache,
    splitServerName,
    storeServersInCache,
    storeServerAddressesInCache,
    updateAddresses,
    updatePlan,
    updateTarget,
    DBServers,
    agencyRoutes,
    Target,
    _ = require("underscore");


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
        return _agency.set(route + "/" + name, value);
      },
      remove: function(route, name) {
        return _agency.remove(route + "/" + name);
      },
      checkVersion: function(route) {
        return false;
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
    var plan = addLevel(this, "plan", "Plan");
    addLevel(plan, "dbServers", "DBServers", ["get"]);
    
    //this.get = _agency.get;
    //this.set = _agency.set;
  }

  agency = new _AgencyWrapper(); 
  agencyRoutes = {
    vision: "Vision/",
    target: "Target/",
    plan: "Plan/",
    current: "Current/",
    fail: "Fail/"
  };


// -----------------------------------------------------------------------------
// --SECTION--                                       Update Wanted Configuration
// -----------------------------------------------------------------------------

  splitServerName = function(route) {
    var splits = route.split("/");
    return splits[splits.length - 1];
  };

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

  storeServerAddressesInCache = function(servers) {
    _.each(servers, function(v, k) {
      var pName = splitServerName(k);
      // Data Servers
      if (cache.wanted[pName]) {
        cache.wanted[pName].address = v;
      }
      if (cache.current[pName]) {
        cache.current[pName].address = v;
      }
      // Coordinators
      /*
      if (cache.wanted[pName]) {
        cache.wanted[pName].address = v;
      }
      */
    });
  };

  updateAddresses = function() {
    if (cache.wanted && cache.current) {
      var addresses = agency.get(agencyRoutes.current + "ServersRegistered", true);
      storeServerAddressesInCache(addresses);
    }
  };

  updateTarget = function() {
    cache.target = {};
    var servers = agency.target.dbServers.get(true);
    storeServersInCache(cache.target, servers);
    updateAddresses();
  };

  updatePlan = function(force) {
    cache.plan = {};
    var servers = agency.plan.dbServers.get(true);
    storeServersInCache(cache.plan, servers);
    updateAddresses();
  };

// -----------------------------------------------------------------------------
// --SECTION--                                               Configuration Cache
// -----------------------------------------------------------------------------
  cache = {
    getTarget: function() {
      if (!agency.target.dbServers.checkVersion()) {
        updateTarget();
      }
      // TODO Add Update on version mismatch
      return this.target;
    },

    getPlan: function() {
      return this.plan;
    }
  };

  //Fill Cache
  updateTarget();
  updatePlan();

// -----------------------------------------------------------------------------
// --SECTION--                                                            Vision
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                            Target
// -----------------------------------------------------------------------------

  Target = function() {
    var DBServers;

    this.DBServers = function() {
      if (!DBServers) {
        //Add DBServer specific functions
        DBServers = {
          getList: function() {
            return cache.getTarget();
          },
          addPrimary: function(name) {
            return agency.target.dbServers.set(name, "none");
          },
          addSecondary: function(name, primaryName) {
            return agency.target.dbServers.set(primaryName, name);
          },
          addPair: function(primaryName, secondaryName) {
            return agency.target.dbServers.set(primaryName, secondaryName);
          },
          removeServer: function(name) {
            var res = -1;
            _.each(cache.getTarget(), function(opts, n) {
              if (n === name) {
                // The removed server is a primary
                if (opts.role === "primary") {
                  res = agency.target.dbServers.remove(name);
                  if (!res) {
                    res = -1;
                    return;
                  }
                  if (opts.secondary !== "none") {
                    res = agency.target.dbServers.set(opts.secondary, "none");
                  }
                  return;
                }
              }
              if (opts.role === "primary" && opts.secondary === name) {
                res = agency.target.dbServers.set(n, "none");
                return;
              }
            });
            if (res === -1) {
              //TODO Debug info
              require("internal").print("Trying to remove a server that is not known");
            }
            return res;
          }
        };
      }
      return DBServers;
    };

  };

// -----------------------------------------------------------------------------
// --SECTION--                                                           Servers
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_agency-communication_servers_general
/// @brief A generic communication interface for lists of servers
///
/// @FUN{new Servers(@FA{route})}
///
/// This creates a new Interface to the agency to handle a set of servers.
/// The first argument is the subroute where the configuration of this type of
/// servers is stored.
///
/// @EXAMPLES
///
/// @code
///     list = new Servers("DBServers");
/// @endcode
////////////////////////////////////////////////////////////////////////////////
  this.DBServers = function() {
    if (!DBServers) {
      //Add DBServer specific functions
      DBServers = {
        list: function() {
          return cache.getPlan();
        },
        add: function() {},
        set: function() {},
        get: function() {},
        remove: function() {}
      };
    }
    return DBServers;
  };

  
// -----------------------------------------------------------------------------
// --SECTION--                                             Global Object binding
// -----------------------------------------------------------------------------

  this.target = new Target();

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
