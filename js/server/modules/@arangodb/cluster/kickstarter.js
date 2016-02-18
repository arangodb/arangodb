/*jshint strict: false, unused: false */
/*global ArangoServerState */

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


var download = require("internal").download;
var executeExternal = require("internal").executeExternal;
var killExternal = require("internal").killExternal;
var statusExternal = require("internal").statusExternal;
var base64Encode = require("internal").base64Encode;

var fs = require("fs");
var wait = require("internal").wait;
var toArgv = require("internal").toArgv;

var exchangePort = require("@arangodb/cluster/planner").exchangePort;
var exchangeProtocol = require("@arangodb/cluster/planner").exchangeProtocol;

var console = require("console");

var launchActions = {};
var shutdownActions = {};
var cleanupActions = {};
var isHealthyActions = {};
var upgradeActions = {};

var getAddrPort = require("@arangodb/cluster/planner").getAddrPort;
var getPort = require("@arangodb/cluster/planner").getPort;
var endpointToURL = require("@arangodb/cluster/planner").endpointToURL;

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

function waitForServerUp (cmd, endpoint, timeout) {
  var url = endpointToURL(endpoint) + "/_api/version";
  var time = 0;
  while (true) {
    var r = download(url, "", {});
    if (r.code === 200 || r.code === 401) {
      if (cmd.extremeVerbosity) {
        console.info("Could talk to "+endpoint+" .");
      }
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

function waitForServerDown (cmd, endpoint, timeout) {
  var url = endpointToURL(endpoint) + "/_api/version";
  var time = 0;
  while (true) {
    var r = download(url, "", {});
    if (r.code === 500 || r.code === 401) {
      if (cmd.extremeVerbosity) {
        console.info("Server at " + endpoint + " does not answer any more.");
      }
      return true;
    }
    if (time >= timeout) {
      console.info("After timeout, server at " + endpoint +
                   "is still there, giving up.");
      return false;
    }
    wait(0.5);
    time += 0.5;
  }
}

function sendToAgency (cmd, agencyURL, path, obj) {
  var res;
  var body;

  if (cmd.extremeVerbosity) {
    console.info("Sending %s to agency...", path);
  }
  if (typeof obj === "string") {
    var count = 0;
    while (count++ <= 2) {
      body = "value=" + encodeURIComponent(obj);
      res = download(agencyURL + path,body,
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
      res = sendToAgency(cmd, agencyURL, path + "/" + encode(keys[i]), obj[keys[i]]);
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
  var yaml = require("js-yaml");
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

  var instanceName = "agent"+cmd.agencyPrefix+cmd.extPort;
  var extEndpoint = getAddrPort(exchangePort(dispatchers[cmd.dispatcher].endpoint, cmd.extPort));
  var extBind = cmd.onlyLocalhost ? ("127.0.0.1:"+cmd.extPort) : extEndpoint;
  var clusterEndPoint = getAddrPort(exchangePort(dispatchers[cmd.dispatcher].endpoint, cmd.intPort));
  var clusterBind = cmd.onlyLocalhost ? ("127.0.0.1:"+cmd.intPort) : clusterEndPoint;

  var clusterUrl = "http://" + clusterEndPoint;
  var agencyUrl =  "http://" + extBind;
  if (require("internal").platform.substr(0,3) === 'win') {
    agentDataDir = agentDataDir.split("\\").join("/");
  }

  var args = {
      "data-dir":               agentDataDir,
      "name":                   instanceName,
      "bind-addr":              extBind,
      "addr":                   extEndpoint,
      "peer-bind-addr":         clusterBind,
      "peer-addr":              clusterEndPoint,
      "initial-cluster-state":  "new",
      "initial-cluster":        instanceName + "=" + clusterUrl
      // the following might speed up etcd, but it might also
      // make it more unstable:
      // ,"peer-heartbeat-timeout": "10",
      // "peer-election-timeout": "20"
  };
  var i;
  if (cmd.peers.length > 0) {
    var st = getAddrPort(cmd.peers[0]);
    for (i = 1; i < cmd.peers.length; i++) {
      st = st + "," + getAddrPort(cmd.peers[i]);
    }
    args.peers = st;
  }
  var agentPath = cmd.agentPath;
  if (agentPath === "") {
    agentPath = ArangoServerState.agentPath();
  }
  if (! fs.exists(agentPath)) {
    return {"error":true, "isStartAgent": true,
            "errorMessage": "agency binary not found at '" + agentPath + "'"};
  }

  var pid = executeExternal(agentPath, toArgv(args));
  var res;
  var count = 0;
  while (++count < 20) {
    wait(0.5);   // Wait a bit to give it time to startup
    res = download(agencyUrl + "/v2/stats/self");
    if (res.code === 200) {
      return {"error":false, "isStartAgent": true, "pid": pid,
              "endpoint": "tcp://"+extEndpoint};
    }
    res = statusExternal(pid, false);
    if (res.status !== "RUNNING") {
      return {"error":true, "isStartAgent": true,
              "errorMessage": "agency failed to start:\n" + yaml.safeDump({"exit status": res})};
    }
  }
  killExternal(pid);
  return {"error":true, "isStartAgent": true,
          "errorMessage": "agency did not come alive on time, giving up."};
};

launchActions.sendConfiguration = function (dispatchers, cmd, isRelaunch) {
  if (isRelaunch) {
    // nothing to do here
    console.info("Waiting 1 second for agency to come alive...");
    wait(1);
    return {"error":false, "isSendConfiguration": true};
  }
  var url = endpointToURL(cmd.agency.endpoints[0]) + "/v2/keys";
  var res = sendToAgency(cmd, url, "", cmd.data);
  if (res === true) {
    return {"error":false, "isSendConfiguration": true};
  }
  var yaml = require("js-yaml");
  return {"error":true, "isSendConfiguration": true, "suberror": res, errorMessage : yaml.safeDump(res)};
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

  var url = endpointToURL(cmd.agency.endpoints[0]) + "/v2/keys/"+
            cmd.agency.agencyPrefix + "/";
  if (cmd.extremeVerbosity) {
    console.info("Downloading %sLaunchers/%s", url, encode(cmd.name));
  }
  var res = download(url + "Dispatcher/Launchers/" + encode(cmd.name), "", { method: "GET",
                                                       followRedirects: true });
  if (res.code !== 200) {
    return {"error": true, "isStartServers": true, "suberror": res};
  }
  var body = JSON.parse( res.body );
  var info = JSON.parse(body.node.value);
  var id, ep, args, pids, port, endpoints, endpointNames, roles;

  console.info("Starting servers...");
  var i;
  var servers = info.DBservers.concat(info.Coordinators);
  roles = [];
  for (i = 0; i < info.DBservers.length; i++) {
    roles.push("PRIMARY");
  }
  for (i = 0; i < info.Coordinators.length; i++) {
    roles.push("COORDINATOR");
  }
  pids = [];
  endpoints = [];
  endpointNames = [];
  for (i = 0; i < servers.length; i++) {
    id = servers[i];

    var serverUrl = url + "Dispatcher/Endpoints/" + encodeURIComponent(id);
    if (cmd.extremeVerbosity) {
      console.info("Downloading ", serverUrl);
    }
    res = download(serverUrl);
    if (res.code !== 200) {
      return {"error": true, "pids": pids,
              "isStartServers": true, "suberror": res};
    }
    console.info("Starting server %s",id);
    body = JSON.parse(res.body);
    ep = body.node.value;
    port = getPort(ep);
    var useSSL = false;
    if (roles[i] === "PRIMARY") {
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
            "--cluster.my-local-info", id,
            "--cluster.my-role", roles[i],
            "--cluster.my-address", ep,
            "--cluster.agency-prefix", cmd.agency.agencyPrefix,
            "--cluster.agency-endpoint", cmd.agency.endpoints[0],
            "--server.endpoint"]);
    if (cmd.onlyLocalhost) {
      args.push(exchangeProtocol("tcp://127.0.0.1:" + port, useSSL));
    }
    else {
      args.push(exchangeProtocol("tcp://0.0.0.0:" + port, useSSL));
    }
    args.push("--log.file");
    var logfile = fs.join(logPath, "log-" + cmd.agency.agencyPrefix + "-" + id);
    args.push(logfile);
    if (!isRelaunch) {
      if (!fs.exists(logPath)) {
        fs.makeDirectoryRecursive(logPath);
      }
      if (fs.exists(logfile)) {
        fs.remove(logfile);
      }
    }
    var datadir = fs.join(dataPath, "data-" + cmd.agency.agencyPrefix + "-" + id);
    if (!isRelaunch) {
      if (!fs.exists(dataPath)) {
        fs.makeDirectoryRecursive(dataPath);
      }
      if (fs.exists(datadir)) {
        fs.removeDirectoryRecursive(datadir,true);
      }
      fs.makeDirectory(datadir);
    }
    args.push("--database.directory");
    args.push(datadir);
    args = args.concat(dispatchers[cmd.dispatcher].arangodExtraArgs);
    var arangodPath = fs.makeAbsolute(cmd.arangodPath);
    if (arangodPath !== cmd.arangodPath) {
      arangodPath = ArangoServerState.arangodPath();
    }
    if ((cmd.valgrind !== '') &&
        (cmd.valgrindHosts !== undefined) &&
        (cmd.valgrindHosts.indexOf(roles[i]) > -1)) {
      var valgrindOpts = cmd.valgrindOpts.concat(
        ["--xml-file=" + cmd.valgrindXmlFileBase + '_' + cmd.valgrindTestname + '_' + id  + '.%p.xml',
         "--log-file=" + cmd.valgrindXmlFileBase + '_' + cmd.valgrindTestname + '_' + id  + '.%p.valgrind.log']);
      var newargs = valgrindOpts.concat([arangodPath]).concat(args);
      var cmdline = cmd.valgrind;
      pids.push(executeExternal(cmdline, newargs));
    }
    else {
      pids.push(executeExternal(arangodPath, args));
    }
    ep = exchangePort(dispatchers[cmd.dispatcher].endpoint,port);
    ep = exchangeProtocol(ep,useSSL);
    endpoints.push(ep);
    endpointNames.push(id);
  }

  var error = false;
  for (i = 0;i < endpoints.length;i++) {
    var timeout = 50;
    if (cmd.valgrind !== '') {
      timeout *= 10000;
    }
    if (! waitForServerUp(cmd, endpoints[i], timeout)) {
      error = true;
    }
  }

  return {"error": error,
          "isStartServers": true,
          "pids": pids,
          "endpoints": endpoints,
          "endpointNames": endpointNames,
          "roles": roles};
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
    return {"error": true, "bootstrapServers": true, "errorMessage": "No coordinators to start"};
  }

  // authorization header for coordinator
  var hdrs = {
    Authorization: getAuthorizationHeader(username, password)
  };

  // default options
  var timeout = 90;
  if (cmd.valgrind !== '') {
    timeout *= 10000;
  }
  var options = {
    method: "POST",
    timeout: timeout,
    headers: hdrs,
    returnBodyOnError: true
  };

  // execute bootstrap command on first server
  var retryCount = 0;
  var result;
  var url = coordinators[0] + "/_admin/cluster/bootstrapDbServers";
  var body = '{"isRelaunch": ' + (isRelaunch ? "true" : "false") + '}';
  while (retryCount < timeout) {

    result = download(url, body, options);

    if ((result.code === 503) && (retryCount < 3)) {
      wait(1);
      retryCount+=1;
      continue;
    }
    if (result.code !== 200) {
      var err1 = "bootstrapping DB servers failed: " + extractErrorMessage(result);
      console.error("%s", err1);
      return {"error": true, "bootstrapServers": true, "errorMessage": err1};
    }
    break;
  }
  // execute cluster database upgrade
  url = coordinators[0] + "/_admin/cluster/upgradeClusterDatabase";

  result = download(url, body, options);

  if (result.code !== 200) {
    var err2 = "upgrading cluster database failed: " + extractErrorMessage(result);
    console.error("%s", err2);
    return {"error": true, "bootstrapServers": true, "errorMessage": err2};
  }

  // bootstrap coordinators
  var i;

  for (i = 0;  i < coordinators.length;  ++i) {
    url = coordinators[i] + "/_admin/cluster/bootstrapCoordinator";

    result = download(url, body, options);

    if (result.code !== 200) {
      var err = "bootstrapping coordinator " +
        coordinators[i] + " failed: " + 
        extractErrorMessage(result);
      console.error("%s", err);
      return {"error": true, "bootstrapServers": true, "errorMessage": err};
    }
  }

  return {"error": false, "bootstrapServers": true, "errorMessage": ''};
};

shutdownActions.startAgent = function (dispatchers, cmd, run) {
  if (cmd.extremeVerbosity) {
    console.info("Shutting down agent %s", JSON.stringify(run.pid));
  }
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
    console.info("Using API to shutdown %s %s %s %s",
                 run.roles[i],
                 run.endpointNames[i],
                 run.endpoints[i],
                 JSON.stringify(run.pids[i]));
    url = endpointToURL(run.endpoints[i])+"/_admin/shutdown";
    // We use the cluster-internal authentication:
    var hdrs = { Authorization : ArangoServerState.getClusterAuthentication() };
    r = download(url,"",{method:"GET", headers: hdrs});
    if (r.code !== 200) {
      console.info("Shutdown API result: %s %s %s %s %s",
                   run.roles[i],
                   run.endpointNames[i],
                   run.endpoints[i],
                   JSON.stringify(run.pids[i]),
                   JSON.stringify(r));
    }
  }
  for (i = 0;i < run.endpoints.length;i++) {
    waitForServerDown(cmd, run.endpoints[i], 30);
    // we cannot do much with the result...
  }

  var shutdownWait = 20;
  if (cmd.valgrind !== '') {
    shutdownWait *= 600; // should be enough, even with Valgrind
  }
  console.info("Waiting up to " + shutdownWait + " seconds for servers to shutdown gracefully...");
  var j = 0; 
  var runpids = run.pids.length;
  while ((j < shutdownWait) && (runpids > 0)) {
    wait(1);
    j++;
    for (i = 0; i < run.pids.length; i++) {
      if (serverStates[JSON.stringify(run.pids[i])] === undefined) {
        var s = statusExternal(run.pids[i]);

        if ((s.status === "NOT-FOUND")  ||
            (s.status === "TERMINATED") ||
            s.hasOwnProperty('signal')) {
          runpids -=1;
          serverStates[JSON.stringify(run.pids[i])] = s;
          error = true;
        }
        else if (j > shutdownWait) {
          if (s.status !== "TERMINATED") {
            if (s.hasOwnProperty('signal')) {
              error = true;
              console.error("shuting down %s %s done - with problems: " + s,
                            run.roles[i],
                            run.endpointNames[i],
                            JSON.stringify(run.pids[i]));
            }
            else {
              console.info("Shutting down %s the hard way...",
                           JSON.stringify(run.pids[i]));
              s.killedState = killExternal(run.pids[i]);
              console.info("done.");
              runpids -=1;
            }
            serverStates[JSON.stringify(run.pids[i])] = s;
          }
        }
      }
    }
  }
  return {"error": error, "isStartServers": true, "serverStates" : serverStates};
};

cleanupActions.startAgent = function (dispatchers, cmd) {
  if (cmd.extremeVerbosity) {
    console.info("Cleaning up agent...");
  }
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
  if (cmd.extremeVerbosity) {
    console.info("Cleaning up DBservers...");
  }
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
  if (cmd.extremeVerbosity) {
    console.info("Checking health of agent %s", JSON.stringify(run.pid));
  }
  var r = statusExternal(run.pid);
  r.isStartAgent = true;
  r.error = false;
  return r;
};

isHealthyActions.startServers = function (dispatchers, cmd, run) {
  var i;
  var r = [];
  for (i = 0;i < run.pids.length;i++) {
    if (cmd.extremeVerbosity) {
      console.info("Checking health of %s %s %s %s",
                   run.roles[i],
                   run.endpointNames[i],
                   run.endpoints[i],
                   JSON.stringify(run.pids[i]));
    }
    r.push(statusExternal(run.pids[i]));
  }
  var s = [];
  var x;
  var error = false;
  for (i = 0;i < run.endpoints.length;i++) {
    x = waitForServerUp(cmd, run.endpoints[i], 0);
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
  if (cmd.extremeVerbosity) {
    console.info("Downloading %sLaunchers/%s", url, encode(cmd.name));
  }
  var res = download(url+"Launchers/" + encode(cmd.name), "", { method: "GET",
                                                   followRedirects: true });
  if (res.code !== 200) {
    return {"error": true, "isStartServers": true, "suberror": res};
  }
  var body = JSON.parse( res.body );
  var info = JSON.parse(body.node.value);
  var id,ep,args,pids,port,endpoints,endpointNames,roles;

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
    if (cmd.extremeVerbosity) {
      console.info("Downloading %sTarget/MapIDToEndpoint/%s", url, id);
    }
    res = download(url + "Target/MapIDToEndpoint/" + id);
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
  endpointNames = [];
  for (i = 0; i < servers.length; i++) {
    id = servers[i];
    if (cmd.extremeVerbosity) {
      console.info("Downloading %sTarget/MapIDToEndpoint/%s", url, id);
    }
    res = download(url + "Target/MapIDToEndpoint/" + id);
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
    endpointNames.push(id);
  }

  error = false;
  for (i = 0;i < endpoints.length;i++) {
    if (! waitForServerUp(cmd, endpoints[i], 30)) {
      error = true;
    }
  }

  return {"error": error,
          "isStartServers": true,
          "pids": pids,
          "endpoints": endpoints,
          "endpointNames": endpointNames,
          "roles": roles};
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

  // authorization header for coordinator
  var hdrs = {
    Authorization: getAuthorizationHeader(username, password)
  };

  // default options
  var timeout = 90;
  if (cmd.valgrind !== '') {
    timeout *= 10000;
  }
  var options = {
    method: "POST",
    timeout: timeout,
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
/// @brief was docuBlock JSF_Cluster_Kickstarter_Constructor
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
/// @brief was docuBlock JSF_Kickstarter_prototype_launch
////////////////////////////////////////////////////////////////////////////////

Kickstarter.prototype.launch = function () {
  var clusterPlan = this.clusterPlan;
  var myname = this.myname;
  var dispatchers = clusterPlan.dispatchers;
  var cmds = clusterPlan.commands;
  var results = [];
  var cmd;
  var errors = "";
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
          errors += "Launchjob " + cmd.action + " failed: " + res.errorMessage;
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
      var timeout = 90;
      if (cmd.valgrind !== '') {
        timeout *= 10000;
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: timeout});
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
    return {"error": true, "errorMessage": errors,
            "runInfo": results};
  }
  return {"error": false, "errorMessage": "none",
          "runInfo": results};
};

////////////////////////////////////////////////////////////////////////////////
/// @brief was docuBlock JSF_Kickstarter_prototype_relaunch
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
      var timeout = 90;
      if (cmd.valgrind !== '') {
        timeout *= 10000;
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: timeout});
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
/// @brief was docuBlock JSF_Kickstarter_prototype_shutdown
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
      var timeout = 90;
      if (cmd.valgrind !== '') {
        timeout *= 10000;
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: timeout});
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
/// @brief was docuBlock JSF_Kickstarter_prototype_cleanup
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
      var timeout = 90;
      if (cmd.valgrind !== '') {
        timeout *= 10000;
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: timeout});
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
/// @brief was docuBlock JSF_Kickstarter_prototype_isHealthy
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
    if (runInfo === undefined || ! Array.isArray(runInfo) ||
        i >= runInfo.length) {
      return {"error": true,
              "errorMessage": "runInfo object not found or broken",
              "results": results};
    }
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
      var timeout = 90;
      if (cmd.valgrind !== '') {
        timeout *= 10000;
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: timeout});
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
/// @brief was docuBlock JSF_Kickstarter_prototype_upgrade
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
      var timeout = 90;
      if (cmd.valgrind !== '') {
        timeout *= 10000;
      }
      var response = download(url, body, {method: "POST", headers: hdrs, timeout: timeout});
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
