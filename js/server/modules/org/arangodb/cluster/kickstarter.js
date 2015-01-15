/*jshint strict: false, unused: false */
/*global require, exports, ArangoServerState */

////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster kickstarting functionality using dispatchers
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
// --SECTION--                                         Kickstarter functionality
// -----------------------------------------------------------------------------

var download = require("internal").download;
var executeExternal = require("internal").executeExternal;
var killExternal = require("internal").killExternal;
var statusExternal = require("internal").statusExternal;
var base64Encode = require("internal").base64Encode;

var fs = require("fs");
var wait = require("internal").wait;

var exchangePort = require("org/arangodb/cluster/planner").exchangePort;
var exchangeProtocol = require("org/arangodb/cluster/planner").exchangeProtocol;

var console = require("console");

var launchActions = {};
var shutdownActions = {};
var cleanupActions = {};
var isHealthyActions = {};
var upgradeActions = {};

var getAddrPort = require("org/arangodb/cluster/planner").getAddrPort;
var getPort = require("org/arangodb/cluster/planner").getPort;
var endpointToURL = require("org/arangodb/cluster/planner").endpointToURL;

function extractErrorMessage (result) {
  if (result === undefined) {
    return "unknown error";
  }

  if (result.hasOwnProperty("body") && result.body !== undefined) {
    try {
      var e =  JSON.parse(result.body);

      if (e.hasOwnProperty("errorMessage")) {
        return e.errorMessage;
      }

      if (e.hasOwnProperty("errorNum")) {
        return e.errorNum;
      }

      if (result.body !== "") {
        return result.body;
      }
    }
    catch (err) {
      if (result.body !== "") {
        return result.body;
      }
    }
  }

  if (result.hasOwnProperty("message")) {
    return result.message;
  }

  if (result.hasOwnProperty("code")) {
    return String(result.code);
  }

  return "unknown error";
}

function makePath (path) {
  return fs.join.apply(null,path.split("/"));
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

function getAuthorizationHeader (username, passwd) {
  return "Basic " + base64Encode(username + ":" + passwd);
}

function getAuthorization (dispatcher) {
  return getAuthorizationHeader(dispatcher.username, dispatcher.passwd);
}

function waitForServerUp (endpoint, timeout) {
  var url = endpointToURL(endpoint) + "/_api/version";
  var time = 0;
  while (true) {
    var r = download(url, "", {});
    if (r.code === 200 || r.code === 401) {
      console.info("Could talk to "+endpoint+" .");
      return true;
    }
    if (time >= timeout) {
      console.info("Could not talk to endpoint "+endpoint+", giving up.");
      return false;
    }
    wait(0.5);
    time += 0.5;
  }
}

function waitForServerDown (endpoint, timeout) {
  var url = endpointToURL(endpoint) + "/_api/version";
  var time = 0;
  while (true) {
    var r = download(url, "", {});
    if (r.code === 500 || r.code === 401) {
      console.info("Server at "+endpoint+" does not answer any more.");
      return true;
    }
    if (time >= timeout) {
      console.info("After timeout, server at "+endpoint+
                   "is still there, giving up.");
      return false;
    }
    wait(0.5);
    time += 0.5;
  }
}

function sendToAgency (agencyURL, path, obj) {
  var res;
  var body;

  console.info("Sending %s to agency...", path);
  if (typeof obj === "string") {
    var count = 0;
    while (count++ <= 2) {
      body = "value="+encodeURIComponent(obj);
      res = download(agencyURL+path,body,
          {"method":"PUT", "followRedirects": true,
           "headers": { "Content-Type": "application/x-www-form-urlencoded"}});
      if (res.code === 201 || res.code === 200) {
        return true;
      }
      wait(3);  // wait 3 seconds before trying again
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
    if (res.code === 201 || res.code === 200) {
      return true;
    }
    wait(3);  // wait 3 seconds before trying again
  }
  return res;
}

launchActions.startAgent = function (dispatchers, cmd, isRelaunch) {
  console.info("Starting agent...");

  var dataPath = fs.makeAbsolute(cmd.dataPath);
  if (dataPath !== cmd.dataPath) {   // path was relative
    dataPath = fs.normalize(fs.join(ArangoServerState.dataPath(),cmd.dataPath));
  }

  var agentDataDir = fs.join(dataPath, "agent"+cmd.agencyPrefix+cmd.extPort);
  if (!isRelaunch) {
    if (fs.exists(agentDataDir)) {
      fs.removeDirectoryRecursive(agentDataDir,true);
    }
  }
  var extEndpoint = getAddrPort(
                          exchangePort(dispatchers[cmd.dispatcher].endpoint,
                                       cmd.extPort));
  var args = ["-data-dir", agentDataDir,
              "-name", "agent"+cmd.agencyPrefix+cmd.extPort,
              "-bind-addr", (cmd.onlyLocalhost ? "127.0.0.1:"
                                               : "0.0.0.0:")+cmd.extPort,
              "-addr", extEndpoint,
              "-peer-bind-addr", (cmd.onlyLocalhost ? "127.0.0.1:"
                                                    : "0.0.0.0:")+cmd.intPort,
              "-peer-addr", getAddrPort(
                          exchangePort(dispatchers[cmd.dispatcher].endpoint,
                                       cmd.intPort))
              // the following might speed up etcd, but it might also
              // make it more unstable:
              // ,"-peer-heartbeat-timeout=10",
              // "-peer-election-timeout=20"
             ];
  var i;
  if (cmd.peers.length > 0) {
    args.push("-peers");
    var st = getAddrPort(cmd.peers[0]);
    for (i = 1; i < cmd.peers.length; i++) {
      st = st + "," + getAddrPort(cmd.peers[i]);
    }
    args.push(st);
  }
  var agentPath = cmd.agentPath;
  if (agentPath === "") {
    agentPath = ArangoServerState.agentPath();
  }
  if (! fs.exists(agentPath)) {
    return {"error":true, "isStartAgent": true,
            "errorMessage": "agency binary not found at '" + agentPath + "'"};
  }

  var pid = executeExternal(agentPath, args);
  var res;
  var count = 0;
  while (++count < 20) {
    wait(0.5);   // Wait a bit to give it time to startup
    res = download("http://localhost:"+cmd.extPort+"/v2/keys/");
    if (res.code === 200) {
      return {"error":false, "isStartAgent": true, "pid": pid,
              "endpoint": "tcp://"+extEndpoint};
    }
  }
  return {"error":true, "isStartAgent": true,
          "errorMessage": "agency did not come alive"};
};

launchActions.sendConfiguration = function (dispatchers, cmd, isRelaunch) {
  if (isRelaunch) {
    // nothing to do here
    console.info("Waiting 1 second for agency to come alive...");
    wait(1);
    return {"error":false, "isSendConfiguration": true};
  }
  var url = endpointToURL(cmd.agency.endpoints[0])+"/v2/keys";
  var res = sendToAgency(url, "", cmd.data);
  if (res === true) {
    return {"error":false, "isSendConfiguration": true};
  }
  return {"error":true, "isSendConfiguration": true, "suberror": res};
};

launchActions.startServers = function (dispatchers, cmd, isRelaunch) {
  var dataPath = fs.makeAbsolute(cmd.dataPath);
  if (dataPath !== cmd.dataPath) {   // path was relative
    dataPath = fs.normalize(fs.join(ArangoServerState.dataPath(),cmd.dataPath));
  }
  var logPath = fs.makeAbsolute(cmd.logPath);
  if (logPath !== cmd.logPath) {    // path was relative
    logPath = fs.normalize(fs.join(ArangoServerState.logPath(), cmd.logPath));
  }

  var url = endpointToURL(cmd.agency.endpoints[0])+"/v2/keys/"+
            cmd.agency.agencyPrefix+"/";
  console.info("Downloading %sLaunchers/%s", url, encode(cmd.name));
  var res = download(url+"Launchers/"+encode(cmd.name),"",{method:"GET",
                                                       followRedirects:true});
  if (res.code !== 200) {
    return {"error": true, "isStartServers": true, "suberror": res};
  }
  var body = JSON.parse( res.body );
  var info = JSON.parse(body.node.value);
  var id,ep,args,pids,port,endpoints,roles;

  console.info("Starting servers...");
  var i;
  var servers = info.DBservers.concat(info.Coordinators);
  roles = [];
  for (i = 0; i < info.DBservers.length; i++) {
    roles.push("DBserver");
  }
  for (i = 0; i < info.Coordinators.length; i++) {
    roles.push("Coordinator");
  }
  pids = [];
  endpoints = [];
  for (i = 0; i < servers.length; i++) {
    id = servers[i];
    console.info("Downloading %sTarget/MapIDToEndpoint/%s", url, id);
    res = download(url+"Target/MapIDToEndpoint/"+id);
    if (res.code !== 200) {
      return {"error": true, "pids": pids,
              "isStartServers": true, "suberror": res};
    }
    console.info("Starting server %s",id);
    body = JSON.parse(res.body);
    ep = JSON.parse(body.node.value);
    port = getPort(ep);
    var useSSL = false;
    if (roles[i] === "DBserver") {
      args = ["--configuration", ArangoServerState.dbserverConfig()];
      useSSL = cmd.useSSLonDBservers;
    }
    else {
      args = ["--configuration", ArangoServerState.coordinatorConfig()];
      useSSL = cmd.useSSLonCoordinators;
    }
    args = args.concat([
            "--cluster.disable-dispatcher-kickstarter", "true",
            "--cluster.disable-dispatcher-frontend", "true",
            "--cluster.my-id", id,
            "--cluster.agency-prefix", cmd.agency.agencyPrefix,
            "--cluster.agency-endpoint", cmd.agency.endpoints[0],
            "--server.endpoint"]);
    if (cmd.onlyLocalhost) {
      args.push(exchangeProtocol("tcp://127.0.0.1:"+port,useSSL));
    }
    else {
      args.push(exchangeProtocol("tcp://0.0.0.0:"+port,useSSL));
    }
    args.push("--log.file");
    var logfile = fs.join(logPath,"log-"+cmd.agency.agencyPrefix+"-"+id);
    args.push(logfile);
    if (!isRelaunch) {
      if (!fs.exists(logPath)) {
        fs.makeDirectoryRecursive(logPath);
      }
      if (fs.exists(logfile)) {
        fs.remove(logfile);
      }
    }
    var datadir = fs.join(dataPath,"data-"+cmd.agency.agencyPrefix+"-"+id);
    args.push("--database.directory");
    args.push(datadir);
    if (!isRelaunch) {
      if (!fs.exists(dataPath)) {
        fs.makeDirectoryRecursive(dataPath);
      }
      if (fs.exists(datadir)) {
        fs.removeDirectoryRecursive(datadir,true);
      }
      fs.makeDirectory(datadir);
    }
    args = args.concat(dispatchers[cmd.dispatcher].arangodExtraArgs);
    var arangodPath = fs.makeAbsolute(cmd.arangodPath);
    if (arangodPath !== cmd.arangodPath) {
      arangodPath = ArangoServerState.arangodPath();
    }
    if (cmd.valgrind !== '') {
      var valgrindopts = cmd.valgrindopts.concat(
        ["--xml-file="+cmd.valgrindXmlFileBase + '_' + cmd.valgrindTestname + '_' + id  + '.%p.xml',
         "--log-file="+cmd.valgrindXmlFileBase + '_' + cmd.valgrindTestname + '_' + id  + '.%p.valgrind.log']);
      var newargs = valgrindopts.concat([arangodPath]).concat(args);
      var cmdline = cmd.valgrind;
      pids.push(executeExternal(cmdline, newargs));
    }
    else {
      pids.push(executeExternal(arangodPath, args));
    }
    ep = exchangePort(dispatchers[cmd.dispatcher].endpoint,port);
    ep = exchangeProtocol(ep,useSSL);
    endpoints.push(ep);
  }

  var error = false;
  for (i = 0;i < endpoints.length;i++) {
    if (! waitForServerUp(endpoints[i], 30)) {
      error = true;
    }
  }

  return {"error": error, "isStartServers": true,
          "pids": pids, "endpoints": endpoints, "roles": roles};
};

launchActions.bootstrapServers = function (dispatchers, cmd, isRelaunch,
                                           username, password) {
  var coordinators = cmd.coordinators;

  if (username === undefined && password === undefined) {
    username = "root";
    password = "";
  }

  // we need at least one coordinator
  if (0 === coordinators.length) {
    return {"error": true, "bootstrapServers": true};
  }

  // autorization header for coordinator
  var hdrs = {
    Authorization: getAuthorizationHeader(username, password)
  };

  // default options
  var options = {
    method: "POST",
    timeout: 90,
    headers: hdrs,
    returnBodyOnError: true
  };

  // execute bootstrap command on first server
  var url = coordinators[0] + "/_admin/cluster/bootstrapDbServers";
  var body = '{"isRelaunch": ' + (isRelaunch ? "true" : "false") + '}';

  var result = download(url, body, options);

  if (result.code !== 200) {
    console.error("bootstrapping DB servers failed: %s", extractErrorMessage(result));
    return {"error": true, "bootstrapServers": true};
  }

  // execute cluster database upgrade
  url = coordinators[0] + "/_admin/cluster/upgradeClusterDatabase";

  result = download(url, body, options);

  if (result.code !== 200) {
    console.error("upgrading cluster database failed: %s", extractErrorMessage(result));
    return {"error": true, "bootstrapServers": true};
  }

  // bootstrap coordinators
  var i;

  for (i = 0;  i < coordinators.length;  ++i) {
    url = coordinators[i] + "/_admin/cluster/bootstrapCoordinator";

    result = download(url, body, options);

    if (result.code !== 200) {
      console.error("bootstrapping coordinator %s failed: %s",
                    coordinators[i],
                    extractErrorMessage(result));
      return {"error": true, "bootstrapServers": true};
    }
  }

  return {"error": false, "bootstrapServers": true};
};

shutdownActions.startAgent = function (dispatchers, cmd, run) {
  console.info("Shutting down agent %s", JSON.stringify(run.pid));
  killExternal(run.pid);
  return {"error": false, "isStartAgent": true};
};

shutdownActions.sendConfiguration = function (dispatchers, cmd, run) {
  return {"error": false, "isSendConfiguration": true};
};

shutdownActions.startServers = function (dispatchers, cmd, run) {
  var i;
  var url;
  var r;
  var serverStates = {};
  var error = false;

  for (i = 0;i < run.endpoints.length;i++) {
    console.info("Using API to shutdown %s", JSON.stringify(run.pids[i]));
    url = endpointToURL(run.endpoints[i])+"/_admin/shutdown";
    // We use the cluster-internal authentication:
    var hdrs = { Authorization : ArangoServerState.getClusterAuthentication() };
    r = download(url,"",{method:"GET", headers: hdrs});
    if (r.code !== 200) {
      console.info("Shutdown API result:"+JSON.stringify(r));
    }
  }
  for (i = 0;i < run.endpoints.length;i++) {
    waitForServerDown(run.endpoints[i], 30);
    // we cannot do much with the result...
  }

  console.info("Waiting 8 seconds for servers to shutdown gracefully...");
  wait(8);

  for (i = 0;i < run.pids.length;i++) {
    var s = statusExternal(run.pids[i]);
    if (s.status !== "TERMINATED") {
      if (s.hasOwnProperty('signal')) {
        error = true;
        console.info("done - with problems: " + s);
      }
      else {
        console.info("Shutting down %s the hard way...",
                     JSON.stringify(run.pids[i]));
        s.killedState = killExternal(run.pids[i]);
        console.info("done.");
      }
      serverStates[run.pids[i]] = s;
    }
  }
  return {"error": error, "isStartServers": true, "serverStates" : serverStates};
};

cleanupActions.startAgent = function (dispatchers, cmd) {
  console.info("Cleaning up agent...");

  var dataPath = fs.makeAbsolute(cmd.dataPath);
  if (dataPath !== cmd.dataPath) {   // path was relative
    dataPath = fs.normalize(fs.join(ArangoServerState.dataPath(),cmd.dataPath));
  }

  var agentDataDir = fs.join(dataPath, "agent"+cmd.agencyPrefix+cmd.extPort);
  if (fs.exists(agentDataDir)) {
    fs.removeDirectoryRecursive(agentDataDir,true);
  }
  return {"error":false, "isStartAgent": true};
};

cleanupActions.startServers = function (dispatchers, cmd, isRelaunch) {
  console.info("Cleaning up DBservers...");

  var dataPath = fs.makeAbsolute(cmd.dataPath);
  if (dataPath !== cmd.dataPath) {   // path was relative
    dataPath = fs.normalize(fs.join(ArangoServerState.dataPath(),cmd.dataPath));
  }
  var logPath = fs.makeAbsolute(cmd.logPath);
  if (logPath !== cmd.logPath) {    // path was relative
    logPath = fs.normalize(fs.join(ArangoServerState.logPath(), cmd.logPath));
  }

  var servers = cmd.DBservers.concat(cmd.Coordinators);
  var i;
  var logfile, datadir;
  for (i = 0; i < servers.length; i++) {
    var id = servers[i];
    logfile = fs.join(logPath,"log-"+cmd.agency.agencyPrefix+"-"+id);
    if (fs.exists(logfile)) {
      fs.remove(logfile);
    }
    datadir = fs.join(dataPath,"data-"+cmd.agency.agencyPrefix+"-"+id);
    if (fs.exists(datadir)) {
      fs.removeDirectoryRecursive(datadir,true);
    }
  }
  return {"error": false, "isStartServers": true};
};

isHealthyActions.startAgent = function (dispatchers, cmd, run) {
  console.info("Checking health of agent %s", JSON.stringify(run.pid));
  var r = statusExternal(run.pid);
  r.isStartAgent = true;
  r.error = false;
  return r;
};

isHealthyActions.startServers = function (dispatchers, cmd, run) {
  var i;
  var r = [];
  for (i = 0;i < run.pids.length;i++) {
    console.info("Checking health of server %s", JSON.stringify(run.pids[i]));
    r.push(statusExternal(run.pids[i]));
  }
  var s = [];
  var x;
  var error = false;
  for (i = 0;i < run.endpoints.length;i++) {
    x = waitForServerUp(run.endpoints[i], 0);
    s.push(x);
    if (x === false) {
      error = true;
    }
  }
  return {"error": error, "isStartServers": true, "status": r,
          "answering": s};
};

// Upgrade for the agent is exactly as launch/relaunch:

upgradeActions.startAgent = launchActions.startAgent;


// Upgrade for the configuration

upgradeActions.sendConfiguration = launchActions.sendConfiguration;


// Upgrade for servers, copied from launchactions, but modified:

upgradeActions.startServers = function (dispatchers, cmd, isRelaunch) {
  var dataPath = fs.makeAbsolute(cmd.dataPath);
  if (dataPath !== cmd.dataPath) {   // path was relative
    dataPath = fs.normalize(fs.join(ArangoServerState.dataPath(),cmd.dataPath));
  }
  var logPath = fs.makeAbsolute(cmd.logPath);
  if (logPath !== cmd.logPath) {    // path was relative
    logPath = fs.normalize(fs.join(ArangoServerState.logPath(), cmd.logPath));
  }

  var url = endpointToURL(cmd.agency.endpoints[0])+"/v2/keys/"+
            cmd.agency.agencyPrefix+"/";
  console.info("Downloading %sLaunchers/%s", url, encode(cmd.name));
  var res = download(url+"Launchers/"+encode(cmd.name),"",{method:"GET",
                                                   followRedirects:true});
  if (res.code !== 200) {
    return {"error": true, "isStartServers": true, "suberror": res};
  }
  var body = JSON.parse( res.body );
  var info = JSON.parse(body.node.value);
  var id,ep,args,pids,port,endpoints,roles;

  console.info("Upgrading servers...");
  var i;
  var servers = info.DBservers.concat(info.Coordinators);
  roles = [];
  for (i = 0; i < info.DBservers.length; i++) {
    roles.push("DBserver");
  }
  for (i = 0; i < info.Coordinators.length; i++) {
    roles.push("Coordinator");
  }
  pids = [];
  endpoints = [];
  var useSSL;
  var arangodPath;
  var logfile, datadir;
  for (i = 0; i < servers.length; i++) {
    id = servers[i];
    console.info("Downloading %sTarget/MapIDToEndpoint/%s", url, id);
    res = download(url+"Target/MapIDToEndpoint/"+id);
    if (res.code !== 200) {
      return {"error": true, "pids": pids,
              "isStartServers": true, "suberror": res};
    }
    console.info("Upgrading server %s",id);
    body = JSON.parse(res.body);
    ep = JSON.parse(body.node.value);
    port = getPort(ep);
    useSSL = false;
    if (roles[i] === "DBserver") {
      args = ["--configuration", ArangoServerState.dbserverConfig()];
      useSSL = cmd.useSSLonDBservers;
    }
    else {
      args = ["--configuration", ArangoServerState.coordinatorConfig()];
      useSSL = cmd.useSSLonCoordinators;
    }
    args = args.concat([
            "--cluster.disable-dispatcher-kickstarter", "true",
            "--cluster.disable-dispatcher-frontend", "true",
            "--cluster.my-id", id,
            "--cluster.agency-prefix", cmd.agency.agencyPrefix,
            "--cluster.agency-endpoint", cmd.agency.endpoints[0],
            "--server.endpoint"]);
    if (cmd.onlyLocalhost) {
      args.push(exchangeProtocol("tcp://127.0.0.1:"+port,useSSL));
    }
    else {
      args.push(exchangeProtocol("tcp://0.0.0.0:"+port,useSSL));
    }
    args.push("--log.file");
    logfile = fs.join(logPath,"log-"+cmd.agency.agencyPrefix+"-"+id);
    args.push(logfile);
    datadir = fs.join(dataPath,"data-"+cmd.agency.agencyPrefix+"-"+id);
    args.push("--database.directory");
    args.push(datadir);
    args = args.concat(dispatchers[cmd.dispatcher].arangodExtraArgs);
    args.push("--upgrade");
    arangodPath = fs.makeAbsolute(cmd.arangodPath);
    if (arangodPath !== cmd.arangodPath) {
      arangodPath = ArangoServerState.arangodPath();
    }
    pids.push(executeExternal(arangodPath, args));
    ep = exchangePort(dispatchers[cmd.dispatcher].endpoint,port);
    ep = exchangeProtocol(ep,useSSL);
    endpoints.push(ep);
  }

  var error = false;
  for (i = 0;i < pids.length;i++) {
    var s = statusExternal(pids[i], true);
    if (s.status !== "TERMINATED" || s.exit !== 0) {
      error = true;
      console.info("Upgrade of server "+servers[i]+" went wrong.");
    }
  }

  if (error === true) {
    return {"error": error, "isStartServers": true,
            "errorMessage": "error on upgrade"};
  }

  console.info("Starting servers...");
  servers = info.DBservers.concat(info.Coordinators);
  roles = [];
  for (i = 0; i < info.DBservers.length; i++) {
    roles.push("DBserver");
  }
  for (i = 0; i < info.Coordinators.length; i++) {
    roles.push("Coordinator");
  }
  pids = [];
  endpoints = [];
  for (i = 0; i < servers.length; i++) {
    id = servers[i];
    console.info("Downloading %sTarget/MapIDToEndpoint/%s", url, id);
    res = download(url+"Target/MapIDToEndpoint/"+id);
    if (res.code !== 200) {
      return {"error": true, "pids": pids,
              "isStartServers": true, "suberror": res};
    }
    console.info("Starting server %s",id);
    body = JSON.parse(res.body);
    ep = JSON.parse(body.node.value);
    port = getPort(ep);
    useSSL = false;
    if (roles[i] === "DBserver") {
      args = ["--configuration", ArangoServerState.dbserverConfig()];
      useSSL = cmd.useSSLonDBservers;
    }
    else {
      args = ["--configuration", ArangoServerState.coordinatorConfig()];
      useSSL = cmd.useSSLonCoordinators;
    }
    args = args.concat([
            "--cluster.disable-dispatcher-kickstarter", "true",
            "--cluster.disable-dispatcher-frontend", "true",
            "--cluster.my-id", id,
            "--cluster.agency-prefix", cmd.agency.agencyPrefix,
            "--cluster.agency-endpoint", cmd.agency.endpoints[0],
            "--server.endpoint"]);
    if (cmd.onlyLocalhost) {
      args.push(exchangeProtocol("tcp://127.0.0.1:"+port,useSSL));
    }
    else {
      args.push(exchangeProtocol("tcp://0.0.0.0:"+port,useSSL));
    }
    args.push("--log.file");
    logfile = fs.join(logPath,"log-"+cmd.agency.agencyPrefix+"-"+id);
    args.push(logfile);
    if (!isRelaunch) {
      if (!fs.exists(logPath)) {
        fs.makeDirectoryRecursive(logPath);
      }
      if (fs.exists(logfile)) {
        fs.remove(logfile);
      }
    }
    datadir = fs.join(dataPath,"data-"+cmd.agency.agencyPrefix+"-"+id);
    args.push("--database.directory");
    args.push(datadir);
    if (!isRelaunch) {
      if (!fs.exists(dataPath)) {
        fs.makeDirectoryRecursive(dataPath);
      }
      if (fs.exists(datadir)) {
        fs.removeDirectoryRecursive(datadir,true);
      }
      fs.makeDirectory(datadir);
    }
    args = args.concat(dispatchers[cmd.dispatcher].arangodExtraArgs);
    arangodPath = fs.makeAbsolute(cmd.arangodPath);
    if (arangodPath !== cmd.arangodPath) {
      arangodPath = ArangoServerState.arangodPath();
    }
    pids.push(executeExternal(arangodPath, args));
    ep = exchangePort(dispatchers[cmd.dispatcher].endpoint,port);
    ep = exchangeProtocol(ep,useSSL);
    endpoints.push(ep);
  }

  error = false;
  for (i = 0;i < endpoints.length;i++) {
    if (! waitForServerUp(endpoints[i], 30)) {
      error = true;
    }
  }

  return {"error": error, "isStartServers": true,
          "pids": pids, "endpoints": endpoints, "roles": roles};
};


// Upgrade for the upgrade-database as in launch:

upgradeActions.bootstrapServers = function (dispatchers, cmd, isRelaunch,
                                             username, password) {
  var coordinators = cmd.coordinators;

  // we need at least one coordinator
  if (0 === coordinators.length) {
    return {"error": true, "bootstrapServers": true};
  }

  // We wait another half second (we have already waited for all servers
  // until they responded!), just to be on the safe side:
  wait(0.5);

  // autorization header for coordinator
  var hdrs = {
    Authorization: getAuthorizationHeader(username, password)
  };

  // default options
  var options = {
    method: "POST",
    timeout: 90,
    headers: hdrs,
    returnBodyOnError: true
  };

  // execute bootstrap command on first server
  var url = coordinators[0] + "/_admin/cluster/bootstrapDbServers";
  var body = '{"isRelaunch": false, "upgrade": true}';

  var result = download(url, body, options);

  if (result.code !== 200) {
    console.error("bootstrapping DB servers failed: %s", extractErrorMessage(result));
    return {"error": true, "bootstrapServers": true};
  }

  // execute cluster database upgrade
  console.info("Running upgrade script on whole cluster...");

  url = coordinators[0] + "/_admin/cluster/upgradeClusterDatabase";

  result = download(url, body, options);

  if (result.code !== 200) {
    console.error("upgrading cluster database failed: %s", extractErrorMessage(result));
    return {"error": true, "bootstrapServers": true};
  }

  // bootstrap coordinators
  var i;

  for (i = 0;  i < coordinators.length;  ++i) {
    url = coordinators[i] + "/_admin/cluster/bootstrapCoordinator";

    result = download(url, body, options);

    if (result.code !== 200) {
      console.error("bootstrapping coordinator %s failed: %s",
                    coordinators[i],
                    extractErrorMessage(result));
      return {"error": true, "bootstrapServers": true};
    }
  }

  return {"error": false, "bootstrapServers": true};
};



////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_Cluster_Kickstarter_Constructor
///
/// `new require("org/arangodb/cluster").Kickstarter(plan)`
///
/// This constructor constructs a kickstarter object. Its first
/// argument is a cluster plan as for example provided by the planner
/// (see Cluster Planner Constructor and the general
/// explanations before this reference). The second argument is
/// optional and is taken to be "me" if omitted, it is the ID of the
/// dispatcher this object should consider itself to be. If the plan
/// contains startup commands for the dispatcher with this ID, these
/// commands are executed immediately. Otherwise they are handed over
/// to another responsible dispatcher via a REST call.
///
/// The resulting object of this constructors allows to launch,
/// shutdown, relaunch the cluster described in the plan.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

function Kickstarter (clusterPlan, myname) {
  this.clusterPlan = clusterPlan;
  if (myname === undefined) {
    this.myname = "me";
  }
  else {
    this.myname = myname;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_Kickstarter_prototype_launch
///
/// `Kickstarter.launch()`
///
/// This starts up a cluster as described in the plan which was given to
/// the constructor. To this end, other dispatchers are contacted as
/// necessary. All startup commands for the local dispatcher are
/// executed immediately.
///
/// The result is an object that contains information about the started
/// processes, this object is also stored in the Kickstarter object
/// itself. We do not go into details here about the data structure,
/// but the most important information are the process IDs of the
/// started processes. The corresponding
/// [see shutdown method](../ModulePlanner/README.html#shutdown) needs this 
/// information to shut down all processes.
///
/// Note that all data in the DBservers and all log files and all agency
/// information in the cluster is deleted by this call. This is because
/// it is intended to set up a cluster for the first time. See
/// the [relaunch method](../ModulePlanner/README.html#relaunch)
/// for restarting a cluster without data loss.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Kickstarter.prototype.launch = function () {
  var clusterPlan = this.clusterPlan;
  var myname = this.myname;
  var dispatchers = clusterPlan.dispatchers;
  var cmds = clusterPlan.commands;
  var results = [];
  var cmd;

  var error = false;
  var i;
  var res;
  for (i = 0; i < cmds.length; i++) {
    cmd = cmds[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      if (launchActions.hasOwnProperty(cmd.action)) {
        res = launchActions[cmd.action](dispatchers, cmd, false);
        results.push(res);
        if (res.error === true) {
          error = true;
          break;
        }
      }
      else {
        results.push({ error: false, action: cmd.action });
      }
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "action": "launch",
                                  "clusterPlan": {
                                        "dispatchers": dispatchers,
                                        "commands": [cmd] },
                                  "myname": cmd.dispatcher });
      var url = endpointToURL(ep) + "/_admin/clusterDispatch";
      var hdrs = {};
      if (dispatchers[cmd.dispatcher].username !== undefined &&
          dispatchers[cmd.dispatcher].passwd !== undefined) {
        hdrs.Authorization = getAuthorization(dispatchers[cmd.dispatcher]);
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: 90});
      if (response.code !== 200) {
        error = true;
        results.push({"error":true, "errorMessage": "bad HTTP response code",
                      "response": response});
      }
      else {
        try {
          res = JSON.parse(response.body);
          results.push(res.runInfo[0]);
        }
        catch (err) {
          results.push({"error":true, "errorMessage": "exception in JSON.parse"});
          error = true;
        }
      }
      if (error) {
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

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_Kickstarter_prototype_relaunch
///
/// `Kickstarter.relaunch()`
///
/// This starts up a cluster as described in the plan which was given to
/// the constructor. To this end, other dispatchers are contacted as
/// necessary. All startup commands for the local dispatcher are
/// executed immediately.
///
/// The result is an object that contains information about the started
/// processes, this object is also stored in the Kickstarter object
/// itself. We do not go into details here about the data structure,
/// but the most important information are the process IDs of the
/// started processes. The corresponding
/// [shutdown method ](../ModulePlanner/README.html#shutdown) needs this information to
/// shut down all processes.
///
/// Note that this methods needs that all data in the DBservers and the
/// agency information in the cluster are already set up properly. See
/// the [launch method](../ModulePlanner/README.html#launch) for
/// starting a cluster for the first time.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Kickstarter.prototype.relaunch = function (username, password) {
  var clusterPlan = this.clusterPlan;
  var myname = this.myname;
  var dispatchers = clusterPlan.dispatchers;
  var cmds = clusterPlan.commands;
  var results = [];
  var cmd;

  if (username === undefined && password === undefined) {
    username = "root";
    password = "";
  }

  var error = false;
  var i;
  var res;
  for (i = 0; i < cmds.length; i++) {
    cmd = cmds[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      if (launchActions.hasOwnProperty(cmd.action)) {
        res = launchActions[cmd.action](dispatchers, cmd, true,
                                        username, password);
        results.push(res);
        if (res.error === true) {
          error = true;
          break;
        }
      }
      else {
        results.push({ error: false, action: cmd.action });
      }
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "action": "relaunch",
                                  "clusterPlan": {
                                        "dispatchers": dispatchers,
                                        "commands": [cmd] },
                                  "myname": cmd.dispatcher,
                                  "username": username,
                                  "password": password});
      var url = endpointToURL(ep) + "/_admin/clusterDispatch";
      var hdrs = {};
      if (dispatchers[cmd.dispatcher].username !== undefined &&
          dispatchers[cmd.dispatcher].passwd !== undefined) {
        hdrs.Authorization = getAuthorization(dispatchers[cmd.dispatcher]);
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: 90});
      if (response.code !== 200) {
        error = true;
        results.push({"error":true, "errorMessage": "bad HTTP response code",
                      "response": response});
      }
      else {
        try {
          res = JSON.parse(response.body);
          results.push(res.runInfo[0]);
        }
        catch (err) {
          results.push({"error":true, "errorMessage": "exception in JSON.parse"});
          error = true;
        }
      }
      if (error) {
        break;
      }
    }
  }
  this.runInfo = results;
  if (error) {
    return {"error": true, "errorMessage": "some error during relaunch",
            "runInfo": results};
  }
  return {"error": false, "errorMessage": "none",
          "runInfo": results};
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_Kickstarter_prototype_shutdown
///
/// `Kickstarter.shutdown()`
///
/// This shuts down a cluster as described in the plan which was given to
/// the constructor. To this end, other dispatchers are contacted as
/// necessary. All processes in the cluster are gracefully shut down
/// in the right order.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

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
  var res;
  for (i = cmds.length-1; i >= 0; i--) {
    cmd = cmds[i];
    var run = runInfo[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      if (shutdownActions.hasOwnProperty(cmd.action)) {
        res = shutdownActions[cmd.action](dispatchers, cmd, run);
        if (res.error === true) {
          error = true;
        }
        results.push(res);
      }
      else {
        results.push({ error: false, action: cmd.action });
      }
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "action": "shutdown",
                                  "clusterPlan": {
                                        "dispatchers": dispatchers,
                                        "commands": [cmd] },
                                  "runInfo": [run],
                                  "myname": cmd.dispatcher });
      var url = endpointToURL(ep) + "/_admin/clusterDispatch";
      var hdrs = {};
      if (dispatchers[cmd.dispatcher].username !== undefined &&
          dispatchers[cmd.dispatcher].passwd !== undefined) {
        hdrs.Authorization = getAuthorization(dispatchers[cmd.dispatcher]);
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: 90});
      if (response.code !== 200) {
        error = true;
        results.push({"error":true, "errorMessage": "bad HTTP response code",
                      "response": response});
      }
      else {
        try {
          res = JSON.parse(response.body);
          results.push(res.results[0]);
        }
        catch (err) {
          results.push({"error":true,
                        "errorMessage": "exception in JSON.parse"});
          error = true;
        }
      }
    }
  }
  results = results.reverse();
  if (error) {
    return {"error": true, "errorMessage": "some error during shutdown",
            "results": results};
  }
  return {"error": false, "errorMessage": "none", "results": results};
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_Kickstarter_prototype_cleanup
///
/// `Kickstarter.cleanup()`
///
/// This cleans up all the data and logs of a previously shut down cluster.
/// To this end, other dispatchers are contacted as necessary.
/// [Use shutdown](../ModulePlanner/README.html#shutdown) first and
/// use with caution, since potentially a lot of data is being erased with
/// this call!
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Kickstarter.prototype.cleanup = function() {
  var clusterPlan = this.clusterPlan;
  var myname = this.myname;
  var dispatchers = clusterPlan.dispatchers;
  var cmds = clusterPlan.commands;
  var results = [];
  var cmd;

  var error = false;
  var i;
  var res;
  for (i = 0; i < cmds.length; i++) {
    cmd = cmds[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      if (cleanupActions.hasOwnProperty(cmd.action)) {
        res = cleanupActions[cmd.action](dispatchers, cmd);
        results.push(res);
        if (res.error === true) {
          error = true;
        }
      }
      else {
        results.push({ error: false, action: cmd.action });
      }
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "action": "cleanup",
                                  "clusterPlan": {
                                        "dispatchers": dispatchers,
                                        "commands": [cmd] },
                                  "myname": cmd.dispatcher });
      var url = endpointToURL(ep) + "/_admin/clusterDispatch";
      var hdrs = {};
      if (dispatchers[cmd.dispatcher].username !== undefined &&
          dispatchers[cmd.dispatcher].passwd !== undefined) {
        hdrs.Authorization = getAuthorization(dispatchers[cmd.dispatcher]);
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: 90});
      if (response.code !== 200) {
        error = true;
        results.push({"error":true, "errorMessage": "bad HTTP response code",
                      "response": response});
      }
      else {
        try {
          res = JSON.parse(response.body);
          results.push(res.results[0]);
        }
        catch (err) {
          results.push({"error":true,
                        "errorMessage": "exception in JSON.parse"});
          error = true;
        }
      }
    }
  }
  if (error) {
    return {"error": true, "errorMessage": "some error during cleanup",
            "results": results};
  }
  return {"error": false, "errorMessage": "none", "results": results};
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_Kickstarter_prototype_isHealthy
///
/// `Kickstarter.isHealthy()`
///
/// This checks that all processes belonging to a running cluster are
/// healthy. To this end, other dispatchers are contacted as necessary.
/// At this stage it is only checked that the processes are still up and
/// running.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Kickstarter.prototype.isHealthy = function() {
  var clusterPlan = this.clusterPlan;
  var myname = this.myname;
  var dispatchers = clusterPlan.dispatchers;
  var cmds = clusterPlan.commands;
  var runInfo = this.runInfo;
  var results = [];
  var cmd;

  var error = false;
  var i;
  var res;
  for (i = cmds.length-1; i >= 0; i--) {
    cmd = cmds[i];
    var run = runInfo[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      if (isHealthyActions.hasOwnProperty(cmd.action)) {
        res = isHealthyActions[cmd.action](dispatchers, cmd, run);
        if (res.error === true) {
          error = true;
        }
        results.push(res);
      }
      else {
        results.push({ error: false, action: cmd.action });
      }
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "action": "isHealthy",
                                  "clusterPlan": {
                                        "dispatchers": dispatchers,
                                        "commands": [cmd] },
                                  "runInfo": [run],
                                  "myname": cmd.dispatcher });
      var url = "http" + ep.substr(3) + "/_admin/clusterDispatch";
      var hdrs = {};
      if (dispatchers[cmd.dispatcher].username !== undefined &&
          dispatchers[cmd.dispatcher].passwd !== undefined) {
        hdrs.Authorization = getAuthorization(dispatchers[cmd.dispatcher]);
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: 90});
      if (response.code !== 200) {
        error = true;
        results.push({"error":true, "errorMessage": "bad HTTP response code",
                      "response": response});
      }
      else {
        try {
          res = JSON.parse(response.body);
          results.push(res.results[0]);
          if (res.results[0].error === true) {
            error = true;
          }
        }
        catch (err) {
          results.push({"error":true,
                        "errorMessage": "exception in JSON.parse"});
          error = true;
        }
      }
    }
  }
  results = results.reverse();
  if (error) {
    return {"error": true, "errorMessage": "some error during isHealthy check",
            "results": results};
  }
  return {"error": false, "errorMessage": "none", "results": results};
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_Kickstarter_prototype_upgrade
///
/// `Kickstarter.upgrade(username, passwd)`
///
/// This performs an upgrade procedure on a cluster as described in
/// the plan which was given to the constructor. To this end, other
/// dispatchers are contacted as necessary. All commands for the local
/// dispatcher are executed immediately. The basic approach for the
/// upgrade is as follows: The agency is started first (exactly as
/// in relaunch), no configuration is sent there (exactly as in the
/// relaunch action), all servers are first started with the option
/// "--upgrade" and then normally. In the end, the upgrade-database.js
/// script is run on one of the coordinators, as in the launch action.
///
/// The result is an object that contains information about the started
/// processes, this object is also stored in the Kickstarter object
/// itself. We do not go into details here about the data structure,
/// but the most important information are the process IDs of the
/// started processes. The corresponding
/// [shutdown method](../ModulePlanner/README.html#shutdown) needs
/// this information to shut down all processes.
///
/// Note that this methods needs that all data in the DBservers and the
/// agency information in the cluster are already set up properly. See
/// the [launch method](../ModulePlanner/README.html#launch) for
/// starting a cluster for the first time.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Kickstarter.prototype.upgrade = function (username, password) {
  var clusterPlan = this.clusterPlan;
  var myname = this.myname;
  var dispatchers = clusterPlan.dispatchers;
  var cmds = clusterPlan.commands;
  var results = [];
  var cmd;

  if (username === undefined || password === undefined) {
    username = "root";
    password = "";
  }

  var error = false;
  var i;
  var res;
  for (i = 0; i < cmds.length; i++) {
    cmd = cmds[i];
    if (cmd.dispatcher === undefined || cmd.dispatcher === myname) {
      if (upgradeActions.hasOwnProperty(cmd.action)) {
        res = upgradeActions[cmd.action](dispatchers, cmd, true,
                                         username, password);
        results.push(res);
        if (res.error === true) {
          error = true;
          break;
        }
      }
      else {
        results.push({ error: false, action: cmd.action });
      }
    }
    else {
      var ep = dispatchers[cmd.dispatcher].endpoint;
      var body = JSON.stringify({ "action": "upgrade",
                                  "clusterPlan": {
                                        "dispatchers": dispatchers,
                                        "commands": [cmd] },
                                  "myname": cmd.dispatcher,
                                  "username": username,
                                  "password": password});
      var url = endpointToURL(ep) + "/_admin/clusterDispatch";
      var hdrs = {};
      if (dispatchers[cmd.dispatcher].username !== undefined &&
          dispatchers[cmd.dispatcher].passwd !== undefined) {
        hdrs.Authorization = getAuthorization(dispatchers[cmd.dispatcher]);
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: 90});
      if (response.code !== 200) {
        error = true;
        results.push({"error":true, "errorMessage": "bad HTTP response code",
                      "response": response});
      }
      else {
        try {
          res = JSON.parse(response.body);
          results.push(res.runInfo[0]);
        }
        catch (err) {
          results.push({"error":true, "errorMessage": "exception in JSON.parse"});
          error = true;
        }
      }
      if (error) {
        break;
      }
    }
  }
  this.runInfo = results;
  if (error) {
    return {"error": true, "errorMessage": "some error during upgrade",
            "runInfo": results};
  }
  return {"error": false, "errorMessage": "none",
          "runInfo": results};
};

exports.Kickstarter = Kickstarter;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
