/*jshint strict: false */

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


var download = require("internal").download;
var base64Encode = require("internal").base64Encode;
var testPort = require("internal").testPort;

// Our default configurations:

var PlannerLocalDefaults = {
  "agencyPrefix"            : "arango",
  "numberOfAgents"          : 1,
  "numberOfDBservers"       : 2,
  "startSecondaries"        : false,
  "numberOfCoordinators"    : 1,
  "DBserverIDs"             : ["Pavel", "Perry", "Pancho", "Paul", "Pierre",
                               "Pit", "Pia", "Pablo", "Peggy" ],
  "coordinatorIDs"          : ["Claus", "Chantalle", "Claire", "Claudia",
                               "Claas", "Clemens", "Chris" ],
  "dataPath"                : "",   // means configured in dispatcher
  "logPath"                 : "",   // means configured in dispatcher
  "arangodPath"             : "",   // means configured in dispatcher
  "agentPath"               : "",   // means configured in dispatcher
  "agentExtPorts"           : [4001],
  "agentIntPorts"           : [7001],
  "DBserverPorts"           : [8629],
  "coordinatorPorts"        : [8530],
  "dispatchers"             : {"me": {"endpoint": "tcp://127.0.0.1:"}},
                              // this means only we as a local instance
  "useSSLonDBservers"       : false,
  "useSSLonCoordinators"    : false,
  "valgrind"                : "",
  "valgrindOpts"            : [],
  "valgrindXmlFileBase"     : "",
  "valgrindTestname"        : "",
  "valgrindHosts"           : "",
  "extremeVerbosity"        : false
};

// Some helpers using underscore:

var _ = require("lodash");

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
    throw new Error("need a list as first argument");
  }
  if (typeof dispatcher !== "object" ||
      !dispatcher.hasOwnProperty("endpoint") ||
      !dispatcher.hasOwnProperty("id")) {
    throw new Error('need a dispatcher object as second argument');
  }
  if (!dispatcher.hasOwnProperty("avoidPorts")) {
    dispatcher.avoidPorts = {};
  }
  this.list = list;
  this.dispatcher = dispatcher;
  this.pos = 0;
  this.port = 0;
}

function getAuthorizationHeader (username, passwd) {
  return "Basic " + base64Encode(username + ":" + passwd);
}

function getAuthorization (dispatcher) {
  return getAuthorizationHeader(dispatcher.username, dispatcher.passwd);
}

function endpointToURL (endpoint) {
  if (endpoint.substr(0,6) === "ssl://") {
    return "https://" + endpoint.substr(6);
  }
  var pos = endpoint.indexOf("://");
  if (pos === -1) {
    return "http://" + endpoint;
  }
  return "http" + endpoint.substr(pos);
}

PortFinder.prototype.next = function () {
  while (true) {   // will be left by return when port is found
    if (this.pos < this.list.length) {
      this.port = this.list[this.pos++];
      if (this.dispatcher.hasOwnProperty("portOverlapIndex")) {
        // add some value to the initial port if we had multiple 
        // dispatchers on the same IP address. Otherwise, the
        // dispatchers will try to bind the exact same ports, and
        // this is prone to races
        this.port += this.dispatcher.portOverlapIndex * 20;
      }
    }
    else if (this.port === 0) {
      this.port = Math.floor(Math.random() * (65536 - 1024)) + 1024;
    }
    else {
      this.port++;
      if (this.port > 65535) {
        this.port = 1024;
      }
    }
    // Check that port is available:
    if (! this.dispatcher.avoidPorts.hasOwnProperty(this.port)) {
      var available = true;
      if ((this.dispatcher.endpoint === "tcp://localhost:") ||
          (this.dispatcher.endpoint === "tcp://127.0.0.1:")) {
        available = testPort("tcp://0.0.0.0:" + this.port);
      }
      else {
        var url = endpointToURL(this.dispatcher.endpoint) +
                  "/_admin/clusterCheckPort?port="+this.port;
        var hdrs = {};
        if (this.dispatcher.username !== undefined &&
            this.dispatcher.passwd !== undefined) {
          hdrs.Authorization = getAuthorization(this.dispatcher);
        }
        var r = download(url, "", {"method": "GET", "timeout": 5,
                                   "headers": hdrs });
        if (r.code === 200) {
          available = JSON.parse(r.body);
        }
        else {
          throw new Error("Cannot check port on dispatcher "+this.dispatcher.endpoint);
        }
      }
      if (available) {
        this.dispatcher.avoidPorts[this.port] = true;  // do not use it again
        return this.port;
      }
    }
  }
};

function exchangeProtocol (endpoint, useSSL) {
  var pos = endpoint.indexOf("://");
  if (pos !== -1) {
    return (useSSL ? "ssl" : "tcp") + endpoint.substr(3);
  }
  return (useSSL ? "ssl://" : "tcp://") + endpoint;
}

function exchangePort (endpoint, newport) {
  var pos = endpoint.lastIndexOf(":");
  if (pos < 0) {
    return endpoint+":"+newport;
  }
  return endpoint.substr(0,pos+1)+newport;
}

function getAddrPort (endpoint) {
  var pos = endpoint.indexOf("://");
  if (pos !== -1) {
    return endpoint.substr(pos+3);
  }
  return endpoint;
}

function getAddr (endpoint) {
  var addrPort = getAddrPort(endpoint);
  var pos = addrPort.indexOf(":");
  if (pos !== -1) {
    return addrPort.substr(0,pos);
  }
  return addrPort;
}

function getPort (endpoint) {
  var pos = endpoint.lastIndexOf(":");
  if (pos !== -1) {
    return parseInt(endpoint.substr(pos+1),10);
  }
  return 8529;
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
/// @brief check if IP addresses of dispatchers are non-unique, and if yes,
/// then set a flag for each dispatcher with non-unique address
/// we need this to prevent race conditions when multiple dispatchers on the
/// same physical server start coordinators or dbservers and compete for the 
/// same ports
////////////////////////////////////////////////////////////////////////////////

function checkDispatcherIps (config) {
  if (typeof config.dispatchers !== "object") {
    return;
  }

  var d, dispatcherIps = { };
  for (d in config.dispatchers) {
    if (config.dispatchers.hasOwnProperty(d)) {
      var ip = config.dispatchers[d].endpoint.replace(/:\d+$/, '').replace(/^(ssl|tcp):\/\//, '');
      if (! dispatcherIps.hasOwnProperty(ip)) {
        dispatcherIps[ip] = [ ];
      }
      dispatcherIps[ip].push(d);
    }
  }

  for (d in dispatcherIps) {
    if (dispatcherIps.hasOwnProperty(d)) {
      if (dispatcherIps[d].length > 1) {
        // more than one dispatcher for an IP address
        // need to ensure that the port ranges do not overlap
        for (var i = 0; i < dispatcherIps[d].length; ++i) {
          config.dispatchers[dispatcherIps[d][i]].portOverlapIndex = i;
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_Cluster_Planner_Constructor
////////////////////////////////////////////////////////////////////////////////

function Planner (userConfig) {
  'use strict';
  if (typeof userConfig !== "object") {
    throw new Error("userConfig must be an object");
  }
  this.config = copy(userConfig);
  checkDispatcherIps(this.config);

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
      if (!dispatchers[id].hasOwnProperty("allowAgents")) {
        dispatchers[id].allowAgents = true;
      }
    }
  }
  // If no dispatcher is there, configure a local one (ourselves):
  if (Object.keys(dispatchers).length === 0) {
    dispatchers.me = { "id": "me", "endpoint": "tcp://localhost:",
                       "avoidPorts": {}, "allowCoordinators": true,
                       "allowDBservers": true, "allowAgents": true };
    config.onlyLocalhost = true;
  }
  else {
    config.onlyLocalhost = false;
    var k = Object.keys(dispatchers);
    if (k.length === 1 &&
        ((dispatchers[k[0]].endpoint.substr(0,16) === "tcp://localhost:") ||
	 (dispatchers[k[0]].endpoint.substr(0,16) === "tcp://127.0.0.1:") )){
      config.onlyLocalhost = true;
    }
  }
  var dispList = Object.keys(dispatchers);

  var pf,pf2;   // lists of port finder objects
  var i;

  // Distribute agents to dispatchers (round robin, choosing ports)
  var d = 0;
  var wrap = d;
  var agents = [];
  pf = [];   // will be filled lazily
  pf2 = [];  // will be filled lazily
  i = 0;
  var oldi = i;
  while (i < config.numberOfAgents) {
    if (dispatchers[dispList[d]].allowAgents) {
      // Find two ports:
      if (!pf.hasOwnProperty(d)) {
        pf[d] = new PortFinder(config.agentExtPorts, dispatchers[dispList[d]]);
        pf2[d] = new PortFinder(config.agentIntPorts, dispatchers[dispList[d]]);
      }
      agents.push({"dispatcher":dispList[d],
                   "extPort":pf[d].next(),
                   "intPort":pf2[d].next()});
      i++;
    }
    if (++d >= dispList.length) {
      d = 0;
    }
    if (d === wrap && oldi === i) {
      break;
    }
  }

  // Distribute coordinators to dispatchers
  var coordinators = [];
  pf = [];
  d = 0;
  i = 0;
  wrap = d;
  oldi = i;
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
  var prefix = agencyData[config.agencyPrefix] = {Dispatcher: {}};
  var endpoints = {};
  var s, ep, tmp;
  for (i = 0; i < DBservers.length; i++) {
    s = DBservers[i];
    ep = exchangePort(dispatchers[s.dispatcher].endpoint,s.port);
    ep = exchangeProtocol(ep, config.useSSLonCoordinators);
    endpoints[s.id] = ep;
    launchers[s.dispatcher].DBservers.push(s.id);
  }
  for (i = 0; i < coordinators.length; i++) {
    s = coordinators[i];
    ep = exchangePort(dispatchers[s.dispatcher].endpoint,s.port);
    ep = exchangeProtocol(ep, config.useSSLonCoordinators);
    endpoints[s.id] = ep;
    launchers[s.dispatcher].Coordinators.push(s.id);
  }
  // Finally Launchers:
  prefix.Dispatcher.Launchers = objmap(launchers, JSON.stringify);
  prefix.Dispatcher.Endpoints = endpoints;
  // make commands
  tmp = this.commands = [];
  var tmp2,j;
  for (i = 0; i < agents.length; i++) {
    tmp2 = { "action"              : "startAgent",
             "dispatcher"          : agents[i].dispatcher,
             "extPort"             : agents[i].extPort,
             "intPort"             : agents[i].intPort,
             "peers"               : [],
             "agencyPrefix"        : config.agencyPrefix,
             "dataPath"            : config.dataPath,
             "logPath"             : config.logPath,
             "agentPath"           : config.agentPath,
             "onlyLocalhost"       : config.onlyLocalhost,
             "valgrind"            : config.valgrind,
             "valgrindOpts"        : config.valgrindOpts,
             "valgrindXmlFileBase" : config.valgrindXmlFileBase,
             "valgrindTestname"    : config.valgrindXmlFileBase,
             "valgrindHosts"       : config.valgrindHosts,
             "extremeVerbosity"    : config.extremeVerbosity
           };
    for (j = 0; j < i; j++) {
      ep = dispatchers[agents[j].dispatcher].endpoint;
      tmp2.peers.push( exchangePort( ep, agents[j].intPort ) );
    }
    tmp.push(tmp2);
  }
  var agencyPos = { "agencyPrefix": config.agencyPrefix,
                    "endpoints": agents.map(function(a) {
                        return exchangePort(dispatchers[a.dispatcher].endpoint,
                                            a.extPort);}) };
  tmp.push({ 
    "action": "sendConfiguration",
    "agency": agencyPos,
    "data": agencyData,
    "extremeVerbosity": config.extremeVerbosity
  });

  for (i = 0; i < dispList.length; i++) {
    tmp.push( { "action"                 : "startServers",
                "dispatcher"             : dispList[i],
                "DBservers"              : copy(launchers[dispList[i]].DBservers),
                "Coordinators"           : copy(launchers[dispList[i]].Coordinators),
                "name"                   : dispList[i],
                "dataPath"               : config.dataPath,
                "logPath"                : config.logPath,
                "arangodPath"            : config.arangodPath,
                "onlyLocalhost"          : config.onlyLocalhost,
                "agency"                 : copy(agencyPos),
                "useSSLonDBservers"      : config.useSSLonDBservers,
                "useSSLonCoordinators"   : config.useSSLonCoordinators,
                "valgrind"               : config.valgrind,
                "valgrindOpts"           : config.valgrindOpts,
                "valgrindXmlFileBase"    : config.valgrindXmlFileBase,
                "valgrindTestname"       : config.valgrindTestname,
                "valgrindHosts"          : config.valgrindHosts,
                "extremeVerbosity"       : config.extremeVerbosity
              } );
  }

  var cc = [];
  var c;
  var e;

  for (i = 0; i < coordinators.length; i++) {
    c = coordinators[i];
    e = exchangePort(dispatchers[c.dispatcher].endpoint, c.port);
    e = exchangeProtocol(e, config.useSSLonCoordinators);

    cc.push(endpointToURL(e));
  }

  var dd = [];

  for (i = 0; i < DBservers.length; i++) {
    c = DBservers[i];
    e = exchangePort(dispatchers[c.dispatcher].endpoint, c.port);
    e = exchangeProtocol(e, config.useSSLonDBservers);

    dd.push(endpointToURL(e));
  }

  tmp.push({ 
    "action": "bootstrapServers",
    "dbServers": dd,
    "coordinators": cc, 
    "extremeVerbosity": config.extremeVerbosity
  });

  this.myname = "me";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_Planner_prototype_getPlan
////////////////////////////////////////////////////////////////////////////////

Planner.prototype.getPlan = function() {
  // Computes the dispatcher commands and returns them as JSON
  return { "dispatchers": this.dispatchers,
           "commands": this.commands };
};

exports.PortFinder = PortFinder;
exports.Planner = Planner;
exports.exchangePort = exchangePort;
exports.exchangeProtocol = exchangeProtocol;
exports.getAddrPort = getAddrPort;
exports.getPort = getPort;
exports.getAddr = getAddr;
exports.endpointToURL = endpointToURL;
