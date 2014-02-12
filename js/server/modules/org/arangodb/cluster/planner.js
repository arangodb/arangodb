/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, stupid: true */
/*global module, require, exports, ArangoAgency, SYS_TEST_PORT */

////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster planning functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 triagens GmbH, Cologne, Germany
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

// -----------------------------------------------------------------------------
// --SECTION--                                             Planner functionality
// -----------------------------------------------------------------------------

var fs = require("fs");
var download = require("internal").download;

// Our default configurations:

var PlannerLocalDefaults = {
  "agencyPrefix"            : "meier",
  "numberOfAgents"          : 3,
  "numberOfDBservers"       : 2,
  "startSecondaries"        : false,
  "numberOfCoordinators"    : 1,
  "DBserverIDs"             : ["Pavel", "Perry", "Pancho", "Paul", "Pierre",
                               "Pit", "Pia", "Pablo" ],
  "coordinatorIDs"          : ["Claus", "Chantalle", "Claire", "Claudia",
                               "Claas", "Clemens", "Chris" ],
  "dataPath"                : "",   // means same dir as dispatcher data
  "logPath"                 : "",   // means same dir as dispatcher data
  "arangodPath"             : "",   // means same executable as dispatcher
  "agentPath"               : "",   // means `etcd` in same place as dispatcher
  "agentExtPorts"           : [4001],
  "agentIntPorts"           : [7001],
  "DBserverPorts"           : [8629],
  "coordinatorPorts"        : [8530],
  "dispatchers"             : {"me": {"endpoint": "tcp://localhost:"}}
                              // this means only we as a local instance
};
  
// The following is just a further example:

var PlannerDistributedDefaults = {
  "agencyPrefix"            : "mueller",
  "numberOfAgents"          : 3,
  "numberOfDBservers"       : 3,
  "startSecondaries"        : false,
  "numberOfCoordinators"    : 3,
  "DBserverIDs"             : ["Pavel", "Perry", "Pancho", "Paul", "Pierre",
                               "Pit", "Pia", "Pablo" ],
  "coordinatorIDs"          : ["Claus", "Chantalle", "Claire", "Claudia",
                               "Claas", "Clemens", "Chris" ],
  "dataPath"                : "",
  "logPath"                 : "",
  "arangodPath"             : "",
  "agentPath"               : "etcd",   // means `etcd` in same place as
                                        // dispatcher executable
  "agentExtPorts"           : [4001],
  "agentIntPorts"           : [7001],
  "DBserverPorts"           : [8629],
  "coordinatorPorts"        : [8530],
  "dispatchers"             : { "machine1": 
                                  { "endpoint": "tcp://machine1:8529",
                                    "avoidPorts": {},
                                    "arangodExtraArgs": []},
                                "machine2":
                                  { "id": "machine2", 
                                    "endpoint": "tcp://machine2:8529",
                                    "avoidPorts": {},
                                    "arangodExtraArgs": []},
                                "machine3":
                                  { "id": "machine3", 
                                    "endpoint": "tcp://machine3:8529",
                                    "avoidPorts": {},
                                    "arangodExtraArgs": []} }
};

// Some helpers using underscore:

var _ = require("underscore");

function objmap (o, f) {
  var r = {};
  var k = _.keys(o);
  var i;
  for (i = 0;i < k.length; i++) {
    r[k[i]] = f(o[k[i]]);
  }
  return r;
}

function copy (o) {
  if (_.isArray(o)) {
    return _.map(o,copy);
  }
  if (_.isObject(o)) {
    return objmap(o,copy);
  }
  return o;
}

// A class to find free ports:

function PortFinder (list, dispatcher) {
  if (!Array.isArray(list)) {
    throw "need a list as first argument";
  }
  if (typeof dispatcher !== "object" ||
      !dispatcher.hasOwnProperty("endpoint") ||
      !dispatcher.hasOwnProperty("id")) {
    throw 'need a dispatcher object as second argument';
  }
  if (!dispatcher.hasOwnProperty("avoidPorts")) {
    dispatcher.avoidPorts = {};
  }
  this.list = list;
  this.dispatcher = dispatcher;
  this.pos = 0;
  this.port = 0;
}

PortFinder.prototype.next = function () {
  while (true) {   // will be left by return when port is found
    if (this.pos < this.list.length) {
      this.port = this.list[this.pos++];
    }
    else if (this.port === 0) {
      this.port = Math.floor(Math.random()*(65536-1024))+1024;
    }
    else {
      this.port++;
      if (this.port > 65535) {
        this.port = 1024;
      }
    }
    // Check that port is available:
    if (!this.dispatcher.avoidPorts.hasOwnProperty(this.port)) {
      var available = true;
      if (this.dispatcher.endpoint === "tcp://localhost:") {
        available = SYS_TEST_PORT("tcp://0.0.0.0:"+this.port);
      }
      else {
        var url = "http" + this.dispatcher.endpoint.substr(3) + 
                  "/_admin/clusterCheckPort?port="+this.port;
        var r = download(url, "", {"method": "GET"});
        if (r.code === 200) {
          available = JSON.parse(r.body);
        }
      }
      if (available) {
        this.dispatcher.avoidPorts[this.port] = true;  // do not use it again
        return this.port;
      }
    }
  }
};
 
function exchangePort (endpoint, newport) {
  var pos = endpoint.lastIndexOf(":");
  if (pos < 0) {
    return endpoint+":"+newport;
  }
  return endpoint.substr(0,pos+1)+newport;
}

// The following function merges default configurations and user configuration.

function fillConfigWithDefaults (config, defaultConfig) {
  var n;
  for (n in defaultConfig) {
    if (defaultConfig.hasOwnProperty(n)) {
      if (!config.hasOwnProperty(n)) {
        config[n] = copy(defaultConfig[n]);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_Cluster_Planner_Constructor
/// @brief the cluster planner constructor
///
/// @FUN{new require("org/arangodb/cluster/planner").Planner(@FA{userConfig})}
///
/// This constructor builds a cluster planner object. The one and only
/// argument is an object that can have the properties described below.
/// The planner can plan clusters on a single machine (basically for
/// testing purposes) and on multiple machines. The resulting "cluster plans"
/// can be used by the kickstarter (see @ref 
/// JSF_Cluster_Kickstarter_Constructor) to start up the processes comprising
/// the cluster, including the agency. To this end, there has to be one
/// dispatcher on every machine participating in the cluster. A dispatcher
/// is a simple instance of ArangoDB, compiled with the cluster extensions,
/// but not running in cluster mode. This is why the configuration option
/// `dispatchers` below is of central importance.
///
///   - `dispatchers`: an object with a property for each dispatcher,
///     the property name is the ID of the dispatcher and the value
///     should be an object with at least the property `endpoint`
///     containing the endpoint of the corresponding dispatcher. 
///     Further optional properties are:
///
///       - `avoidPorts` which is an object
///         in which all port numbers that should not be used are bound to
///         `true`, default is empty, that is, all ports can be used
///       - `arangodExtraArgs`, which is a list of additional
///         command line arguments that will be given to DBservers and
///         coordinators started by this dispatcher, the default is
///         an empty list
///       - `allowCoordinators`, which is a boolean value indicating
///         whether or not coordinators should be started on this
///         dispatcher, the default is `true`
///       - `allowDBservers`, which is a boolean value indicating
///         whether or not DBservers should be started on this dispatcher,
///         the default is `true`
///       - `username`, which is a string that contains the user name
///         for authentication with this dispatcher
///       - `passwd`, which is a string that contains the password
///         for authentication with this dispatcher, if not both
///         `username` and `passwd` are set, then no authentication
///         is used between dispatchers. Note that this will not work
///         if the dispatchers are configured with authentication.
///
///     If `.dispatchers` is empty (no property), then an entry for the
///     local arangod itself is automatically added. Note that if the
///     only configured dispatcher has endpoint `tcp://localhost:`,
///     all processes are started in a special "local" mode and are
///     configured to bind their endpoints only to the localhost device.
///     In all other cases both agents and `arangod` instances bind
///     their endpoints to all available network devices.
///   - `numberOfAgents`: the number of agents in the agency,
///     usually there is no reason to deviate from the default of 3. The 
///     planner distributes them amongst the dispatchers, if possible.
///   - `agencyPrefix`: a string that is used as prefix for all keys of
///     configuration data stored in the agency.
///   - `numberOfDBservers`: the number of DBservers in the
///     cluster. The planner distributes them evenly amongst the dispatchers.
///   - `startSecondaries`: a boolean flag indicating whether or not
///     secondary servers are started. In this version, this flag is
///     silently ignored, since we do not yet have secondary servers.
///   - `numberOfCoordinators`: the number of coordinators in the cluster,
///     the planner distributes them evenly amongst the dispatchers.
///   - `DBserverIDs`: a list of DBserver IDs (strings). If the planner 
///     runs out of IDs it creates its own ones using `DBserver`
///     concatenated with a unique number.
///   - `coordinatorIDs`: a list of coordinator IDs (strings). If the planner 
///     runs out of IDs it creates its own ones using `Coordinator`
///     concatenated with a unique number.
///   - `dataPath`: this is a string and describes the path under which
///     the agents, the DBservers and the coordinators store their
///     data directories. This can either be an absolute path (in which 
///     case all machines in the clusters must use the same path), or
///     it can be a relative path. In the latter case it describes the
///     data path relative to the path under which the dispatcher stores
///     its own data directory. For example, if the data directory
///     given on the command line of the dispatcher is
///     `/etc/arangod/data` and the value of the `dataPath` property is an
///     empty string , then the DBservers started by this dispatcher
///     will have their data directories under `/etc/arangod` as well.
///     These directories will be called `data-PREFIX-ID` where `PREFIX`
///     is replaced with the agency prefix (see above) and `ID` is the
///     ID of the DBserver or coordinator.
///   - `logPath`: this is a string and describes the path under which
///     the DBservers and the coordinators store their log file. The
///     same mechanism for absolute and relative paths apply as under
///     `dataPath`.
///   - `arangodPath`: this is a string and describes the path to the
///     actual executable `arangod` that will be started for the
///     DBservers and coordinators. If this is an absolute path, it
///     obviously has to be the same on all machines in the cluster
///     as described for `dataPath`. If it is an empty string, the
///     dispatcher uses the same executable that was used to start
///     itself. If it is a non-empty relative path, then this path is
///     meant relative to the directory in which the actual executable
///     of the dispatcher resides.
///   - `agentPath`: this is a string and describes the path to the
///     actual executable that will be started for the agents in the
///     agency. If this is an absolute path, it obviously has to be
///     the same on all machines in the cluster, as described for
///     `dataPath`. If it is an empty string, the dispatcher uses `etcd`
///     in the same directory as the executable that was used to start
///     itself. If it is a non-empty relative path, then this path is
///     meant relative to the directory in which the executable of the
///     dispatcher resides.
///   - `agentExtPorts`: a list of port numbers to use for the external
///     ports of the agents. When running out of numbers in this list,
///     the planner increments the last one used by one for every port
///     needed. Note that the planner checks availability of the ports
///     during the planning phase by contacting the dispatchers on the
///     different machines, and uses only ports that are free during
///     the planning phase. Obviously, if those ports are connected
///     before the actual startup, things can go wrong.
///   - `agentIntPorts`: a list of port numbers to use for the internal
///     ports of the agents. The same comments as for `agentExtPorts`
///     apply.
///   - `DBserverPorts`: a list of port numbers to use for the
///     DBservers. The same comments as for `agentExtPorts` apply.
///   - `coordinatorPorts`: a list of port numbers to use for the
///     coordinators. The same comments as for `agentExtPorts` apply.
///
/// All these values have default values. Here is the current set of
/// default values:
/// 
///     {
///       "agencyPrefix"            : "meier",
///       "numberOfAgents"          : 3,
///       "numberOfDBservers"       : 2,
///       "startSecondaries"        : false,
///       "numberOfCoordinators"    : 1,
///       "DBserverIDs"             : ["Pavel", "Perry", "Pancho", "Paul", "Pierre",
///                                    "Pit", "Pia", "Pablo" ],
///       "coordinatorIDs"          : ["Claus", "Chantalle", "Claire", "Claudia",
///                                    "Claas", "Clemens", "Chris" ],
///       "dataPath"                : "",   // means same dir as dispatcher data
///       "logPath"                 : "",   // means same dir as dispatcher data
///       "arangodPath"             : "",   // means same executable as dispatcher
///       "agentPath"               : "",   // means `etcd` in same place as dispatcher
///       "agentExtPorts"           : [4001],
///       "agentIntPorts"           : [7001],
///       "DBserverPorts"           : [8629],
///       "coordinatorPorts"        : [8530],
///       "dispatchers"             : {"me": {"endpoint": "tcp://localhost:"}}
///                                   // this means only we as a local instance
///     };
////////////////////////////////////////////////////////////////////////////////

function Planner (userConfig) {
  "use strict";
  if (typeof userConfig !== "object") {
    throw "userConfig must be an object";
  }
  this.config = copy(userConfig);
  fillConfigWithDefaults(this.config, PlannerLocalDefaults);
  this.commands = [];
  this.makePlan();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual planning method
////////////////////////////////////////////////////////////////////////////////

Planner.prototype.makePlan = function() {
  // This sets up the plan for the cluster according to the options

  var config = this.config;
  var dispatchers = this.dispatchers = copy(config.dispatchers);

  var id;
  for (id in dispatchers) {
    if (dispatchers.hasOwnProperty(id)) {
      dispatchers[id].id = id;
      if (!dispatchers[id].hasOwnProperty("avoidPorts")) {
        dispatchers[id].avoidPorts = {};
      }
      if (!dispatchers[id].hasOwnProperty("arangodExtraArgs")) {
        dispatchers[id].arangodExtraArgs = [];
      }
      if (!dispatchers[id].hasOwnProperty("allowCoordinators")) {
        dispatchers[id].allowCoordinators = true;
      }
      if (!dispatchers[id].hasOwnProperty("allowDBservers")) {
        dispatchers[id].allowDBservers = true;
      }
    }
  }
  // If no dispatcher is there, configure a local one (ourselves):
  if (Object.keys(dispatchers).length === 0) {
    dispatchers.me = { "id": "me", "endpoint": "tcp://localhost:", 
                       "avoidPorts": {}, "allowCoordinators": true,
                       "allowDBservers": true };
    config.onlyLocalhost = true;
  }
  else {
    config.onlyLocalhost = false;
    var k = Object.keys(dispatchers);
    if (k.length === 1 && 
        dispatchers[k[0]].endpoint.substr(0,16) === "tcp://localhost:") {
      config.onlyLocalhost = true;
    }
  }
  var dispList = Object.keys(dispatchers);

  var pf,pf2;   // lists of port finder objects
  var i;

  // Distribute agents to dispatchers (round robin, choosing ports)
  var d = 0;
  var agents = [];
  pf = [];   // will be filled lazily
  pf2 = [];  // will be filled lazily
  for (i = 0; i < config.numberOfAgents; i++) {
    // Find two ports:
    if (!pf.hasOwnProperty(d)) {
      pf[d] = new PortFinder(config.agentExtPorts, dispatchers[dispList[d]]);
      pf2[d] = new PortFinder(config.agentIntPorts, dispatchers[dispList[d]]);
    }
    agents.push({"dispatcher":dispList[d],
                 "extPort":pf[d].next(),
                 "intPort":pf2[d].next()});
    if (++d >= dispList.length) {
      d = 0;
    }
  }

  // Distribute coordinators to dispatchers
  var coordinators = [];
  var count;
  pf = [];
  d = 0;
  i = 0;
  var wrap = d;
  var oldi = i;
  while (i < config.numberOfCoordinators) {
    if (dispatchers[dispList[d]].allowCoordinators) {
      if (!pf.hasOwnProperty(d)) {
        pf[d] = new PortFinder(config.coordinatorPorts, 
                               dispatchers[dispList[d]]);
      }
      if (!config.coordinatorIDs.hasOwnProperty(i)) {
        config.coordinatorIDs[i] = "Coordinator"+i;
      }
      coordinators.push({"id":config.coordinatorIDs[i],
                         "dispatcher":dispList[d],
                         "port":pf[d].next()});
      i++;
    }
    if (++d >= dispList.length) {
      d = 0;
    }
    if (d === wrap && oldi === i) {
      break;
    }
  }

  // Distribute DBservers to dispatchers (secondaries if wanted)
  var DBservers = [];
  pf = [];
  i = 0;
  wrap = d;
  oldi = i;
  while (i < config.numberOfDBservers) {
    if (dispatchers[dispList[d]].allowDBservers) {
      if (!pf.hasOwnProperty(d)) {
        pf[d] = new PortFinder(config.DBserverPorts, 
                               dispatchers[dispList[d]]);
      }
      if (!config.DBserverIDs.hasOwnProperty(i)) {
        config.DBserverIDs[i] = "Primary"+i;
      }
      DBservers.push({"id":config.DBserverIDs[i],
                      "dispatcher":dispList[d],
                      "port":pf[d].next()});
      i++;
    }
    if (++d >= dispList.length) {
      d = 0;
    }
    if (d === wrap && oldi === i) {
      break;
    }
  }

  // Store this plan in object:
  this.coordinators = coordinators;
  this.DBservers = DBservers;
  this.agents = agents;
  var launchers = {};
  for (i = 0; i < dispList.length; i++) {
    launchers[dispList[i]] = { "DBservers": [], 
                               "Coordinators": [] };
  }

  // Set up agency data:
  var agencyData = this.agencyData = {};
  var prefix = agencyData[config.agencyPrefix] = {};
  var tmp;

  // First the Target, we collect Launchers information at the same time:
  tmp = prefix.Target = {};
  tmp.Lock = '"UNLOCKED"';
  tmp.Version = '"1"';
  var dbs = tmp.DBServers = {};
  var map = tmp.MapIDToEndpoint = {};
  var s;
  for (i = 0; i < DBservers.length; i++) {
    s = DBservers[i];
    dbs[s.id] = map[s.id] 
           = '"'+exchangePort(dispatchers[s.dispatcher].endpoint,s.port)+'"';
    launchers[s.dispatcher].DBservers.push(s.id);
  }
  var coo = tmp.Coordinators = {};
  for (i = 0; i < coordinators.length; i++) {
    s = coordinators[i];
    coo[s.id] = map[s.id] 
           = '"'+exchangePort(dispatchers[s.dispatcher].endpoint,s.port)+'"';
    launchers[s.dispatcher].Coordinators.push(s.id);
  }
  tmp.Databases = { "_system" : '{"name":"_system"}' };
  tmp.Collections = { "_system" : {} };

  // Now Plan:
  prefix.Plan = copy(tmp);
  delete prefix.Plan.MapIDToEndpoint;

  // Now Current:
  prefix.Current = { "Lock"             : '"UNLOCKED"',
                     "Version"          : '"1"',
                     "DBservers"        : {},
                     "Coordinators"     : {},
                     "Databases"        : {"_system":{}},
                     "Collections"      : {"_system":{}},
                     "ServersRegistered": {"Version":'"1"'},
                     "ShardsCopied"     : {} };

  // Now Sync:
  prefix.Sync = { "ServerStates"       : {},
                  "Problems"           : {},
                  "LatestID"           : '"0"',
                  "Commands"           : {},
                  "HeartbeatIntervalMs": '1000' };
  tmp = prefix.Sync.Commands;
  for (i = 0; i < DBservers; i++) {
    tmp[DBservers[i].id] = '"SERVE"';
  }

  // Finally Launchers:
  prefix.Launchers = objmap(launchers, JSON.stringify);

  // make commands
  tmp = this.commands = [];
  var tmp2,j;
  for (i = 0; i < agents.length; i++) {
    tmp2 = { "action" : "startAgent", "dispatcher": agents[i].dispatcher,
             "extPort": agents[i].extPort,
             "intPort": agents[i].intPort,
             "peers": [],
             "agencyPrefix": config.agencyPrefix,
             "dataPath": config.dataPath,
             "logPath": config.logPath,
             "agentPath": config.agentPath,
             "onlyLocalhost": config.onlyLocalhost };
    for (j = 0; j < i; j++) {
      var ep = dispatchers[agents[j].dispatcher].endpoint;
      tmp2.peers.push( exchangePort( ep, agents[j].intPort ) );
    }
    tmp.push(tmp2);
  }
  var agencyPos = { "agencyPrefix": config.agencyPrefix,
                    "endpoints": agents.map(function(a) {
                        return exchangePort(dispatchers[a.dispatcher].endpoint,
                                            a.extPort);}) };
  tmp.push( { "action": "sendConfiguration", 
              "agency": agencyPos,
              "data": agencyData } );
  for (i = 0; i < dispList.length; i++) {
    tmp.push( { "action": "startServers", "dispatcher": dispList[i],
                "DBservers": copy(launchers[dispList[i]].DBservers),
                "Coordinators": copy(launchers[dispList[i]].Coordinators),
                "name": dispList[i], 
                "dataPath": config.dataPath,
                "logPath": config.logPath,
                "arangodPath": config.arangodPath,
                "onlyLocalhost": config.onlyLocalhost,
                "agency": copy(agencyPos) } );
  }
  this.myname = "me";
};

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_Planner_prototype_getPlan
/// @brief returns the cluster plan as a JavaScript object.
///
/// @FUN{@FA{Planner}.getPlan()}
///
/// returns the cluster plan as a JavaScript object. The result of this
/// method can be given to the constructor of a Kickstarter (see 
/// @ref JSF_Cluster_Kickstarter_Constructor).
////////////////////////////////////////////////////////////////////////////////

Planner.prototype.getPlan = function() {
  // Computes the dispatcher commands and returns them as JSON
  return { "dispatchers": this.dispatchers,
           "commands": this.commands };
};

exports.PortFinder = PortFinder;
exports.Planner = Planner;
exports.exchangePort = exchangePort;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
