/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, stupid: true */
/*global module, require, exports, ArangoAgency, SYS_TEST_PORT */

////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster kickstarting functionality using dispatchers
///
/// @file js/server/modules/org/arangodb/cluster/kickstarter.js
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
// --SECTION--                                         Kickstarter functionality
// -----------------------------------------------------------------------------

var download = require("internal").download;
var executeExternal = require("internal").executeExternal;
var killExternal = require("internal").killExternal;

var fs = require("fs");
var wait = require("internal").wait;

var exchangePort = require("org/arangodb/cluster/planner").exchangePort;

var print = require("internal").print;

var launchActions = {};
var relaunchActions = {};
var shutdownActions = {};

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

function encode (st) {
  var st2 = "";
  var i;
  for (i = 0; i < st.length; i++) {
    if (st[i] === "_") {
      st2 += "@U";
    }
    else if (st[i] === "@") {
      st2 += "@@";
    } 
    else {
      st2 += st[i];
    }
  }
  return encodeURIComponent(st2);
}

function sendToAgency (agencyURL, path, obj) {
  var res;
  var body;
  print("Sending ",path," to agency...");
  if (typeof obj === "string") {
    var count = 0;
    while (count++ <= 2) {
      body = "value="+encodeURIComponent(obj);
      //print("sending:",agencyURL+path,"\nwith body",body);
      res = download(agencyURL+path,body,
          {"method":"PUT", "followRedirects": true,
           "headers": { "Content-Type": "application/x-www-form-urlencoded"}});
      //print("Code ", res.code); 
      if (res.code === 201) {
        return true;
      }
    }
    return res;
  }
  if (typeof obj !== "object") {
    return "Strange object found: not a string or object";
  }
  var keys = Object.keys(obj);
  var i;
  if (keys.length !== 0) {
    for (i = 0; i < keys.length; i++) {
      res = sendToAgency (agencyURL, path+"/"+encode(keys[i]), obj[keys[i]]);
      if (res !== true) {
        return res;
      }
    }
    return true;
  }
  // Create a directory
  var count2 = 0;
  while (count2++ <= 2) {
    body = "dir=true";
    res = download(agencyURL+path,body,
          {"method": "PUT", "followRedirects": true,
           "headers": { "Content-Type": "application/x-www-form-urlencoded"}});
    if (res.code === 201) {
      return true;
    }
  }
  return res;
}

launchActions.startAgent = function (dispatchers, cmd, isRelaunch) {
  var agentDataDir = fs.join(cmd.dataPath, 
                             "agent"+cmd.agencyPrefix+cmd.extPort);
  if (!isRelaunch) {
    if (fs.exists(agentDataDir)) {
      fs.removeDirectoryRecursive(agentDataDir,true);
    }
  }
  var args = ["-data-dir", agentDataDir,
              "-name", "agent"+cmd.agencyPrefix+cmd.extPort,
              "-bind-addr", (cmd.onlyLocalhost ? "127.0.0.1:" 
                                               : "0.0.0.0:")+cmd.extPort,
              "-addr", getAddrPort(
                          exchangePort(dispatchers[cmd.dispatcher].endpoint,
                                       cmd.extPort)),
              "-peer-bind-addr", (cmd.onlyLocalhost ? "127.0.0.1:"
                                                    : "0.0.0.0:")+cmd.intPort,
              "-peer-addr", getAddrPort(
                          exchangePort(dispatchers[cmd.dispatcher].endpoint,
                                       cmd.intPort)) ];
  var i;
  if (cmd.peers.length > 0) {
    args.push("-peers");
    var st = getAddrPort(cmd.peers[0]);
    for (i = 1; i < cmd.peers.length; i++) {
      st = st + "," + getAddrPort(cmd.peers[i]);
    }
    args.push(getAddrPort(cmd.peers[0]));
  }
  var pid = executeExternal(cmd.agentPath, args);
  var res;
  while (true) {
    wait(0.5);   // Wait a bit to give it time to startup
    res = download("http://localhost:"+cmd.extPort+"/v2/keys/");
    if (res.code === 200) {
      return {"error":false, "isAgent": true, "pid": pid};
    }
  }
};

launchActions.sendConfiguration = function (dispatchers, cmd, isRelaunch) {
  if (isRelaunch) {
    // nothing to do here
    return {"error":false, "isSendConfiguration": true};
  }
  var url = "http://"+getAddrPort(cmd.agency.endpoints[0])+"/v2/keys";
  var res = sendToAgency(url, "", cmd.data);
  if (res === true) {
    return {"error":false, "isSendConfiguration": true};
  }
  return {"error":true, "isSendConfiguration": true, "suberror": res};
};

launchActions.startServers = function (dispatchers, cmd, isRelaunch) {
  var url = "http://"+getAddrPort(cmd.agency.endpoints[0])+"/v2/keys/"+
            cmd.agency.agencyPrefix+"/";
  print("Downloading ",url+"Launchers/"+cmd.name);
  var res = download(url+"Launchers/"+cmd.name,"",{method:"GET",
                                                   followRedirects:true});
  if (res.code !== 200) {
    return {"error": true, "isStartServers": true, "suberror": res};
  }
  var body = JSON.parse( res.body );
  var info = JSON.parse(body.node.value);
  var id,ep,args,pids,port;

  print("Starting servers...");
  var i;
  print(info);
  var servers = info.DBservers.concat(info.Coordinators);
  pids = [];
  for (i = 0; i < servers.length; i++) {
    id = servers[i];
    print("Downloading ",url+"Target/MapIDToEndpoint/"+id);
    res = download(url+"Target/MapIDToEndpoint/"+id);
    if (res.code !== 200) {
      return {"error": true, "pids": pids,
              "isStartServers": true, "suberror": res};
    }
    print("Starting server ",id);
    body = JSON.parse(res.body);
    ep = JSON.parse(body.node.value);
    port = getPort(ep);
    args = ["--cluster.my-id", id, 
            "--cluster.agency-prefix", cmd.agency.agencyPrefix,
            "--cluster.agency-endpoint", cmd.agency.endpoints[0],
            "--server.endpoint"];
    if (cmd.onlyLocalhost) {
      args.push("tcp://127.0.0.1:"+port);
    }
    else {
      args.push("tcp://0.0.0.0:"+port);
    }
    args.push("--log.file");
    var logfile = fs.join(cmd.dataPath,"log-"+cmd.agency.agencyPrefix+"-"+id);
    args.push(logfile);
    if (!isRelaunch) {
      if (fs.exists(logfile)) {
        fs.remove(logfile);
      }
    }
    var datadir = fs.join(cmd.dataPath,"data-"+cmd.agency.agencyPrefix+"-"+id);
    args.push(datadir);
    if (!isRelaunch) {
      if (fs.exists(datadir)) {
        fs.removeDirectoryRecursive(datadir,true);
      }
      fs.makeDirectory(datadir);
    }
    pids.push(executeExternal(cmd.arangodPath, args));
  }
  return {"error": false, "pids": pids};
};

shutdownActions.startAgent = function (dispatchers, cmd, run) {
  print("Killing agent ", run.pid);
  killExternal(run.pid);
  return {"error": false};
};

shutdownActions.sendConfiguration = function (dispatchers, cmd, run) {
  print("Waiting for 10 seconds for servers before killing agency.");
  wait(10);
  return {"error": false};
};

shutdownActions.startServers = function (dispatchers, cmd, run) {
  var i;
  for (i = 0;i < run.pids.length;i++) {
    print("Killing ", run.pids[i]);
    killExternal(run.pids[i]);
  }
  return {"error": false};
};

function Kickstarter (clusterPlan, myname) {
  this.clusterPlan = clusterPlan;
  if (myname === undefined) {
    this.myname = "me";
  }
  else {
    this.myname = myname;
  }
}

Kickstarter.prototype.launch = function () {
  var clusterPlan = this.clusterPlan;
  var myname = this.myname;
  var dispatchers = clusterPlan.dispatchers;
  var cmds = clusterPlan.commands;
  var results = [];
  var cmd;

  var error = false;
  var i;
  for (i = 0; i < cmds.length; i++) {
    cmd = cmds[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      var res = launchActions[cmd.action](dispatchers, cmd, false);
      results.push(res);
      if (res.error === true) {
        error = true;
        break;
      }
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "action": "launch",
                                  "clusterPlan": {
                                        "dispatchers": dispatchers,
                                        "commands": [cmd] },
                                  "myname": cmd.dispatcher });
      var url = "http" + ep.substr(3) + "/_admin/clusterDispatch";
      var response = download(url, body, {"method": "post"});
      try {
        if (response.code !== 200) {
          error = true;
        }
        results.push(JSON.parse(response.body));
      }
      catch (err) {
        results.push({"error":true, "errorMessage": "exception in JSON.parse"});
        error = true;
        break;
      }
    }
  }
  this.runInfo = results;
  if (error) {
    return {"error": true, "errorMessage": "some error during launch",
            "runInfo": results};
  }
  return {"error": false, "errorMessage": "none",
          "runInfo": results};
};

Kickstarter.prototype.relaunch = function () {
  var clusterPlan = this.clusterPlan;
  var myname = this.myname;
  var dispatchers = clusterPlan.dispatchers;
  var cmds = clusterPlan.commands;
  var results = [];
  var cmd;

  var error = false;
  var i;
  for (i = 0; i < cmds.length; i++) {
    cmd = cmds[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      var res = launchActions[cmd.action](dispatchers, cmd, true);
      results.push(res);
      if (res.error === true) {
        error = true;
        break;
      }
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "action": "relaunch",
                                  "clusterPlan": {
                                        "dispatchers": dispatchers,
                                        "commands": [cmd] },
                                  "myname": cmd.dispatcher });
      var url = "http" + ep.substr(3) + "/_admin/clusterDispatch";
      var response = download(url, body, {"method": "post"});
      try {
        if (response.code !== 200) {
          error = true;
        }
        results.push(JSON.parse(response.body));
      }
      catch (err) {
        results.push({"error":true, "errorMessage": "exception in JSON.parse"});
        error = true;
        break;
      }
    }
  }
  this.runInfo = results;
  if (error) {
    return {"error": true, "errorMessage": "some error during launch",
            "runInfo": results};
  }
  return {"error": false, "errorMessage": "none",
          "runInfo": results};
};


Kickstarter.prototype.shutdown = function() {
  var clusterPlan = this.clusterPlan;
  var myname = this.myname;
  var dispatchers = clusterPlan.dispatchers;
  var cmds = clusterPlan.commands;
  var runInfo = this.runInfo;
  var results = [];
  var cmd;

  var error = false;
  var i;
  for (i = cmds.length-1; i >= 0; i--) {
    cmd = cmds[i];
    var run = runInfo[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      var res = shutdownActions[cmd.action](dispatchers, cmd, run);
      if (res.error === true) {
        error = true;
      }
      results.push(res);
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "action": "shutdown",
                                  "clusterPlan": {
                                        "dispatchers": dispatchers,
                                        "commands": [cmd],
                                        "runInfo": [run]},
                                  "myname": cmd.dispatcher });
      var url = "http" + ep.substr(3) + "/_admin/clusterDispatch";
      var response = download(url, body, {"method": "post"});
      try {
        if (response.code !== 200) {
          error = true;
          results.push(JSON.parse(response.body));
        }
      }
      catch (err) {
        results.push({"error":true, "errorMessage": "exception in JSON.parse"});
        error = true;
        break;
      }
    }
  }
  results = results.reverse();
  if (error) {
    return {"error": true, "errorMessage": "some error during launch",
            "results": results};
  }
  return {"error": false, "errorMessage": "none", "results": results};
};

Kickstarter.prototype.isHealthy = function() {
  throw "not yet implemented";
};

Kickstarter.prototype.checkDispatchers = function() {
  // Checks that all dispatchers are active, if none is configured,
  // a local one is started.
  throw "not yet implemented";
};

exports.Kickstarter = Kickstarter;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
