/*jshint strict: false, sub: true */

////////////////////////////////////////////////////////////////////////////////
/// @brief General unittest framework
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

////////////////////////////////////////////////////////////////////////////////
/// @fn JSF_UnitTest
/// @brief framework to perform unittests
///
/// This function gets one or two arguments, the first describes which tests
/// to perform and the second is an options object. For `which` the following
/// values are allowed:
///  Empty will give you a complete list.
////////////////////////////////////////////////////////////////////////////////

var functionsDocumentation = {
  'all'               : " do all tests (marked with [x])",
  "shell_server_perf" : "bulk tests intended to get an overview of executiontime needed.",
  "single_client"     : "run one test suite isolated via the arangosh; options required\n" +
    "            Run without to get more detail",
  "single_server"     : "run one test suite on the server; options required\n" + 
    "            Run without to get more detail",
  "single_localserver": "run on this very instance of arangod, don't fork a new one.\n"
};

var optionsDocumentation = [
  '',
  ' The following properties of `options` are defined:',
  '',
  '   - `jsonReply`: if set a json is returned which the caller has to ',
  '        present the user',
  '   - `force`: if set to true the tests are continued even if one fails',
  '   - `skipBoost`: if set to true the boost unittests are skipped',
  '   - `skipGeo`: if set to true the geo index tests are skipped',
  '   - `skipGraph`: if set to true the Graph tests are skipped',
  '   - `skipAql`: if set to true the AQL tests are skipped',
  '   - `skipRanges`: if set to true the ranges tests are skipped',
  '   - `skipTimeCritical`: if set to true, time critical tests will be skipped.',
  '   - `skipMemoryIntense`: tests using lots of resources will be skippet.',
  '   - `skipAuth : testing authentication will be skipped.',
  '   - `skipSsl`: ommit the ssl_server rspec tests.',
  '   - `skipLogAnalysis`: don\'t try to crawl the server logs',
  '   - `skipConfig`: ommit the noisy configuration tests',
  '   - `skipFoxQueues`: ommit the test for the fox queues',
  '',
  '   - `cluster`: if set to true the tests are run with the coordinator',
  '     of a small local cluster',
  '   - `clusterNodes`: number of DB-Servers to use',
  '   - valgrindHosts  - configure which clustercomponents to run using valgrintd',
  '        Coordinator - run Coordinator with valgrind',
  '        DBServer    - run DBServers with valgrind',
  '   - `test`: path to single test to execute for "single" test target',
  '   - `cleanup`: if set to true (the default), the cluster data files',
  '     and logs are removed after termination of the test.',
  '   - `jasmineReportFormat`: this option is passed on to the `format`',
  '     option of the Jasmine options object, only for Jasmine tests.',
  '',
  '   - benchargs : additional commandline arguments to arangob',
  '',
  '   - `valgrind`: if set to true the arangods are run with the valgrind',
  '     memory checker',
  '   - `valgrindXmlFileBase`: string to prepend to the xml report name',
  '   - `valgrindargs`: list of commandline parameters to add to valgrind',
  '',
  '   - `extraargs`: list of extra commandline arguments to add to arangod',
  '   - `portOffset`: move our base port by n ports up',
  ''
];

var _ = require("underscore");
var cleanupDirectories = [];
var testFuncs = {'all': function(){}};
var print = require("internal").print;
var time = require("internal").time;
var fs = require("fs");
var download = require("internal").download;
var wait = require("internal").wait;
var executeExternal = require("internal").executeExternal;
var killExternal = require("internal").killExternal;
var statusExternal = require("internal").statusExternal;
var executeExternalAndWait = require("internal").executeExternalAndWait;
var base64Encode = require("internal").base64Encode;
var yaml = require("js-yaml");

var PortFinder = require("org/arangodb/cluster").PortFinder;
var Planner = require("org/arangodb/cluster").Planner;
var Kickstarter = require("org/arangodb/cluster").Kickstarter;

var endpointToURL = require("org/arangodb/cluster/planner").endpointToURL;
var toArgv = require("internal").toArgv;

var serverCrashed = false;

var optionsDefaults = { "cluster": false,
                        "valgrind": false,
                        "force": true,
                        "skipBoost": false,
                        "skipGeo": false,
                        "skipTimeCritical": false,
                        "skipMemoryIntense": false,
                        "skipAql": false,
                        "skipRanges": false,
                        "skipLogAnalysis": false,
                        "username": "root",
                        "password": "",
                        "test": undefined,
                        "cleanup": true,
                        "jsonReply": false,
                        "portOffset": 0,
                        "valgrindargs": [],
                        "valgrindXmlFileBase" : "",
                        "extraargs": [],
                        "coreDirectory": "/var/tmp"

};
var allTests =
  [
    "config",
    "boost",
    "shell_server",
    "shell_server_aql",
    "http_server",
    "ssl_server",
    "shell_client",
    "dump",
    "arangob",
    "importing",
    "upgrade",
    "authentication",
    "authentication_parameters"
  ];


function printUsage () {
  print();
  print("Usage: UnitTest( which, options )");
  print();
  print('       where "which" is one of:\n');
  var i;
  var checkAll;
  var oneFunctionDocumentation;
  for (i in testFuncs) {
    if (testFuncs.hasOwnProperty(i)) {
      if (functionsDocumentation.hasOwnProperty(i)) {
        oneFunctionDocumentation = ' - ' + functionsDocumentation[i];
      }
      else {
        oneFunctionDocumentation = '';
      }
      if (allTests.indexOf(i) !== -1) { 
        checkAll = '[x]';
      }
      else {
        checkAll = '   ';
      }
      print('    ' + checkAll + ' '+i+' ' + oneFunctionDocumentation);
    }
  }
  for (i in optionsDocumentation) {
    if (optionsDocumentation.hasOwnProperty(i)) {
      print(optionsDocumentation[i]);
    }
  }
}

function filterTestcaseByOptions (testname, options, whichFilter) {
  if (options.hasOwnProperty('test') && (typeof(options.test) !== 'undefined')) {
    whichFilter.filter = "testcase";
    return testname === options.test;
  }
  if ((testname.indexOf("-cluster") !== -1) && (options.cluster === false)) {
    whichFilter.filter = 'noncluster';
    return false;
  }

  if (testname.indexOf("-noncluster") !== -1 && (options.cluster === true)) {
    whichFilter.filter = 'cluster';
    return false;
  }

  if (testname.indexOf("-timecritical") !== -1 && (options.skipTimeCritical === true)) {
    whichFilter.filter = 'timecritical';
    return false;
  }

  if (testname.indexOf("-geo") !== -1 && options.skipGeo) {
    whichFilter.filter = 'geo';
    return false;
  }

  if (testname.indexOf("-graph") !== -1 && options.skipGraph) {
    whichFilter.filter = 'graph';
    return false;
  }

  if (testname.indexOf("-disabled") !== -1) {
    whichFilter.filter = 'disabled';
    return false;
  }

  if (testname.indexOf("replication") !== -1) {
    whichFilter.filter = 'replication';
    return false;
  }

  if ((testname.indexOf("-memoryintense") !== -1) && (options.skipMemoryIntense === true)) {
    whichFilter.filter = 'memoryintense';
    return false;
  }

  return true;
}

function findTopDir () {
  var topDir = fs.normalize(fs.makeAbsolute("."));
  if (! fs.exists("3rdParty") && ! fs.exists("arangod") &&
      ! fs.exists("arangosh") && ! fs.exists("UnitTests")) {
    throw "Must be in ArangoDB topdir to execute unit tests.";
  }
  return topDir;
}

function makeTestingArgs (appDir) {
  var topDir = findTopDir();
  fs.makeDirectoryRecursive(appDir, true);
  return { "configuration":                  "none",
           "server.keyfile":                 fs.join(topDir, "UnitTests", "server.pem"),
           "database.maximal-journal-size":  "1048576",
           "database.force-sync-properties": "false",
           "javascript.app-path":            appDir,
           "javascript.startup-directory":   fs.join(topDir, "js"),
           "server.threads":                 "20",
           "javascript.v8-contexts":         "5",
           "server.disable-authentication":  "true",
           "server.allow-use-database":      "true" };
}

function makeTestingArgsClient (options) {
  var topDir = findTopDir();
  return { "configuration":                  "none",
           "javascript.startup-directory":   fs.join(topDir, "js"),
           "server.username":                options.username,
           "server.password":                options.password,
           "flatCommands": ["--no-colors", "--quiet"]
 };
}

function makeAuthorisationHeaders (options) {
  return {"headers":
            {"Authorization": "Basic " + base64Encode(options.username+":"+
                                                      options.password)}};
}

function startInstance (protocol, options, addArgs, testname, tmpDir) {
  // protocol must be one of ["tcp", "ssl", "unix"]
  var startTime = time();
  var topDir = findTopDir();
  var instanceInfo = {};
  instanceInfo.topDir = topDir;
  var tmpDataDir = tmpDir || fs.getTempFile();
  var appDir;

  instanceInfo.flatTmpDataDir = tmpDataDir;

  tmpDataDir = fs.join(tmpDataDir, testname);
  appDir =  fs.join(tmpDataDir, "apps");
  fs.makeDirectoryRecursive(tmpDataDir);
  instanceInfo.tmpDataDir = tmpDataDir;
  var optionsExtraArgs = {};
  if (typeof(options.extraargs) === 'object') {
    optionsExtraArgs = options.extraargs;
  }
  var endpoint;
  var pos;
  var valgrindopts = {};
  if (typeof(options.valgrindargs) === 'object') {
    valgrindopts = options.valgrindargs;
  }

  var valgrindHosts = '';
  if (typeof(options.valgrindHosts) === 'object') {
    if (options.valgrindHosts.Coordinator === true) {
      valgrindHosts += 'Coordinator';
    }

    if (options.valgrindHosts.DBServer === true) {
      valgrindHosts += 'DBServer';
    }
    
  }

  var dispatcher;
  if (options.cluster) {

    var clusterNodes = 2;
    if (options.clusterNodes) {
      clusterNodes = options.clusterNodes;
    }

    var extraargs = makeTestingArgs(appDir);
    extraargs = _.extend(extraargs, optionsExtraArgs);
    if (addArgs !== undefined) {
      extraargs = _.extend(extraargs, addArgs);
    }
    dispatcher = {"endpoint":"tcp://127.0.0.1:",
                  "arangodExtraArgs": toArgv(extraargs),
                  "username": "root",
                  "password": ""};
    print("Temporary cluster data and logs are in",tmpDataDir);

    var runInValgrind = "";
    var valgrindXmlFileBase = "";
    if (typeof(options.valgrind) === 'string') {
      runInValgrind = options.valgrind;
      valgrindXmlFileBase = options.valgrindXmlFileBase;
    }

    var p = new Planner({"numberOfDBservers"      : clusterNodes,
                         "numberOfCoordinators"   : 1,
                         "dispatchers"            : {"me": dispatcher},
                         "dataPath"               : tmpDataDir,
                         "logPath"                : tmpDataDir,
                         "useSSLonCoordinators"   : protocol === "ssl",
                         "valgrind"               : runInValgrind,
                         "valgrindopts"           : toArgv(valgrindopts, true),
                         "valgrindXmlFileBase"    : '_cluster' + valgrindXmlFileBase,
                         "valgrindTestname"       : testname,
                         "valgrindHosts"          : valgrindHosts
                        });
    instanceInfo.kickstarter = new Kickstarter(p.getPlan());
    var rc = instanceInfo.kickstarter.launch();
    if (rc.error) {
      print("Cluster startup failed: " + rc.errorMessage);
      return false;
    }
    var runInfo = instanceInfo.kickstarter.runInfo;
    var j = runInfo.length - 1;
    while (j > 0 && runInfo[j].isStartServers === undefined) {
      j--;
    }

    if ((runInfo.length === 0) ||
        (runInfo[0].error === true))
    {
      var error = new Error();
      error.errorMessage = yaml.safeDump(runInfo);
      throw error;
    }
    var roles = runInfo[j].roles;
    var endpoints = runInfo[j].endpoints;
    pos = roles.indexOf("Coordinator");
    endpoint = endpoints[pos];
  }
  else {   // single instance mode
    // We use the PortFinder to find a free port for our subinstance,
    // to this end, we have to fake a dummy dispatcher:
    dispatcher = {endpoint: "tcp://127.0.0.1:", avoidPorts: {}, id: "me"};
    var pf = new PortFinder([8529 + options.portOffset],dispatcher);
    var port = pf.next();
    instanceInfo.port = port;
    endpoint = protocol+"://127.0.0.1:"+port;

    var args = makeTestingArgs(appDir);
    args["server.endpoint"] = endpoint;
    args["database.directory"] = fs.join(tmpDataDir,"data");
    fs.makeDirectoryRecursive(fs.join(tmpDataDir,"data"));
    args["log.file"] = fs.join(tmpDataDir,"log");
    if (protocol === "ssl") {
      args["server.keyfile"] = fs.join("UnitTests","server.pem");
    }
    args = _.extend(args, optionsExtraArgs);

    if (addArgs !== undefined) {
      args = _.extend(args, addArgs);
    }
    if (typeof(options.valgrind) === 'string') {
      var run = fs.join("bin","arangod");
      var testfn = options.valgrindXmlFileBase;
      if (testfn.length > 0 ) {
        testfn += '_' ;
      }
      testfn += testname;

      valgrindopts["xml-file"] = testfn + '.%p.xml';
      valgrindopts["log-file"] = testfn + '.%p.valgrind.log';
      // Sequence: Valgrind arguments; binary to run; options to binary:
      var newargs=toArgv(valgrindopts, true).concat([run]).concat(toArgv(args));
      var cmdline = options.valgrind;
      instanceInfo.pid = executeExternal(cmdline, newargs);
    }
    else {
      instanceInfo.pid = executeExternal(fs.join("bin","arangod"), toArgv(args));
    }
  }

  // Wait until the server/coordinator is up:
  var count = 0;
  var url = endpointToURL(endpoint);
  instanceInfo.url = url;
  instanceInfo.endpoint = endpoint;

  while (true) {
    wait(0.5, false);
    var r = download(url+"/_api/version", "", makeAuthorisationHeaders(options));
    if (! r.error && r.code === 200) {
      break;
    }
    count ++;
    if (count % 60 === 0) {
      if (! checkInstanceAlive(instanceInfo, options)) {
        print("startup failed! bailing out!");
        return false;
      }
    }
  }
  print("up and running in " + (time () - startTime) + " seconds");
  if (!options.cluster && (require("internal").platform.substr(0,3) === 'win')) {
    var procdumpArgs = [
      '-accepteula',
      '-e',
      '-ma',
      instanceInfo.pid.pid,
      fs.join(tmpDataDir, 'core.dmp')
    ];
    instanceInfo.monitor = executeExternal('procdump', procdumpArgs);
  }
  return instanceInfo;
}

function readImportantLogLines(logPath) {
  var i, j;
  var importantLines = {};
  var list=fs.list(logPath);
  var jasmineTest = fs.join("jasmine", "core");
  for (i = 0; i < list.length; i++) {
    var fnLines = [];
    if (list[i].slice(0,3) === 'log') {
      var buf = fs.readBuffer(fs.join(logPath,list[i]));
      var lineStart = 0;
      var maxBuffer = buf.length;
      for (j = 0; j < maxBuffer; j++) {
        if (buf[j] === 10) { // \n
          var line = buf.asciiSlice(lineStart, j);
          // filter out regular INFO lines, and test related messages
          if ((line.search(" INFO ") < 0) &&
            (line.search("WARNING about to execute:") < 0) &&
            (line.search(jasmineTest) < 0)) {
            fnLines.push(line);
          }
          lineStart = j + 1;
        }
      }
    }
    if (fnLines.length > 0) {
      importantLines[list[i]] = fnLines;
    }
  }
  return importantLines;
}

function analyzeCoreDump(instanceInfo, options, storeArangodPath, pid) {
  var command;
  command  = '(';
  command += "printf 'bt full\\n thread apply all bt\\n';";
  command += "sleep 10;";
  command += "echo quit;";
  command += "sleep 2";
  command += ") | gdb " + storeArangodPath + " " + options.coreDirectory + "/*core*" + pid + '*';
  var args = ['-c', command];
  print(JSON.stringify(args));
  executeExternalAndWait("/bin/bash", args);

}


function checkInstanceAlive(instanceInfo, options) {
  var storeArangodPath;
  if (options.cluster === false) {
    if (instanceInfo.hasOwnProperty('exitStatus')) {
      return false;
    }
    var res = statusExternal(instanceInfo.pid, false);
    var ret = res.status === "RUNNING";
    if (! ret) {
      print("ArangoD with PID " + instanceInfo.pid.pid + " gone:");
      print(instanceInfo);
      if (res.hasOwnProperty('signal') && 
          ((res.signal === 11) ||
           (res.signal === 6)  ||
           // Windows sometimes has random numbers in signal...
           (require("internal").platform.substr(0,3) === 'win')
          )
         )
      {
        storeArangodPath = "/var/tmp/arangod_" + instanceInfo.pid.pid;
        print("Core dump written; copying arangod to " + 
              instanceInfo.tmpDataDir + " for later analysis.");
        res.gdbHint = "Run debugger with 'gdb " + 
          storeArangodPath + " " + options.coreDirectory + 
          "/core*" + instanceInfo.pid.pid + "*'";
        if (require("internal").platform.substr(0,3) === 'win') {
          fs.copyFile("bin\\arangod.exe", instanceInfo.tmpDataDir + "\\arangod.exe");
          fs.copyFile("bin\\arangod.pdb", instanceInfo.tmpDataDir + "\\arangod.pdb");
          // Windows: wait for procdump to do its job...
          statusExternal(instanceInfo.monitor, true);
        }
        else {
          fs.copyFile("bin/arangod", storeArangodPath);
          analyzeCoreDump(instanceInfo, options, storeArangodPath, instanceInfo.pid.pid);
        }
      }
      instanceInfo.exitStatus = res;
    }
    if (!ret) {
      serverCrashed = true;
    }
    return ret;
  }
  var ClusterFit = true;
  for (var part in instanceInfo.kickstarter.runInfo) {
    if (instanceInfo.kickstarter.runInfo[part].hasOwnProperty("pids")) {
      for (var pid in instanceInfo.kickstarter.runInfo[part].pids) {
        if (instanceInfo.kickstarter.runInfo[part].pids.hasOwnProperty(pid)) {
          var checkpid = instanceInfo.kickstarter.runInfo[part].pids[pid];
          var ress = statusExternal(checkpid, false);
          if (ress.hasOwnProperty('signal') && 
              ((ress.signal === 11) || (ress.signal === 6))) {
            storeArangodPath = "/var/tmp/arangod_" + checkpid.pid;
            print("Core dump written; copying arangod to " + 
                  storeArangodPath + " for later analysis.");
            ress.gdbHint = "Run debugger with 'gdb " + 
              storeArangodPath + 
              " /var/tmp/core*" + checkpid.pid + "*'";

            if (require("internal").platform.substr(0,3) === 'win') {
              fs.copyFile("bin\\arangod.exe", instanceInfo.tmpDataDir + "\\arangod.exe");
              fs.copyFile("bin\\arangod.pdb", instanceInfo.tmpDataDir + "\\arangod.pdb");
              // Windows: wait for procdump to do its job...
              statusExternal(instanceInfo.monitor, true);
            }
            else {
              fs.copyFile("bin/arangod", storeArangodPath);
              analyzeCoreDump(instanceInfo, options, storeArangodPath, checkpid.pid);
            }
            
            instanceInfo.exitStatus = ress;
            ClusterFit = false;
          }
        }
      }
    }
  }
  if (ClusterFit && instanceInfo.kickstarter.isHealthy()) {
    return true;
  }
  else {
    serverCrashed = true;
    return false;
  }
}

function waitOnServerForGC(instanceInfo, options, waitTime) {
  try {
    print("waiting " + waitTime + " for server GC");
    var r;
    var t;
    t = 'require("internal").wait(' + waitTime + ', true);';
    var o = makeAuthorisationHeaders(options);
    o.method = "POST";
    o.timeout = waitTime * 10;
    o.returnBodyOnError = true;
    r = download(instanceInfo.url + "/_admin/execute?returnAsJSON=true",t,o);
    print("waiting " + waitTime + " for server GC - done.");
    if (! r.error && r.code === 200) {
      r = JSON.parse(r.body);
    } else {
      return {
        status: false,
        message: r.body
      };
    }
    return r;
  } catch (e) {
    return {
      status: false,
      message: e.message || String(e),
      stack: e.stack
    };
  }

}


function shutdownInstance (instanceInfo, options) {
  if (!checkInstanceAlive(instanceInfo, options)) {
      print("Server already dead, doing nothing. This shouldn't happen?");
  }
  if (options.valgrind) {
    waitOnServerForGC(instanceInfo, options, 60);
  }
  if (options.cluster) {
    var rc = instanceInfo.kickstarter.shutdown();
    if (!options.skipLogAnalysis) {
      instanceInfo.importantLogLines = readImportantLogLines(instanceInfo.tmpDataDir);
    }
    if (options.cleanup) {
      instanceInfo.kickstarter.cleanup();
    }
    if (rc.error) {
      for (var i = 0; i < rc.results.length; i++ ) {
        if (rc.results[i].hasOwnProperty('isStartServers') && 
            (rc.results[i].isStartServers === true)) {
          for (var serverState in rc.results[i].serverStates) {
            if (rc.results[i].serverStates.hasOwnProperty(serverState)){
              if ((rc.results[i].serverStates[serverState].status === "NOT-FOUND") || 
                  (rc.results[i].serverStates[serverState].hasOwnProperty('signal'))) {
                print("Server " + serverState + " shut down with:\n" +
                      yaml.safeDump(rc.results[i].serverStates[serverState]) +
                      " marking run as crashy.");
                serverCrashed = true;
              }
            }
          }
        }
      }
    }

  }
  else {
    if (typeof(instanceInfo.exitStatus) === 'undefined') {
      download(instanceInfo.url+"/_admin/shutdown","",
               makeAuthorisationHeaders(options));

      print("Waiting for server shut down");
      var count = 0;
      var bar = "[";
      while (1) {
        instanceInfo.exitStatus = statusExternal(instanceInfo.pid, false);
        if (instanceInfo.exitStatus.status === "RUNNING") {
          count ++;
          if (typeof(options.valgrind) === 'string') {
            wait(1);
            continue;
          }
          if (count % 10 ===0) {
            bar = bar + "#";
          }
          if (count > 600) {
            print("forcefully terminating " + yaml.safeDump(instanceInfo.pid) + " after 600 s grace period.");
            serverCrashed = true;
            killExternal(instanceInfo.pid);
            break;
          }
          else {
            wait(1);
          }
        }
        else if (instanceInfo.exitStatus.status !== "TERMINATED") {
          if (instanceInfo.exitStatus.hasOwnProperty('signal')) {
            print("Server shut down with : " + yaml.safeDump(instanceInfo.exitStatus) + " marking build as crashy.");
            serverCrashed = true;
            break;
          }
          if (require("internal").platform.substr(0,3) === 'win') {
            // Windows: wait for procdump to do its job...
            statusExternal(instanceInfo.monitor, true);
          }
        }
        else {
          print("Server shutdown: Success.");
          break; // Success.
        }
      }
      if (count > 10) {
        print("long Server shutdown: " + bar + ']');
      }
    }
    else {
      print("Server already dead, doing nothing.");
    }
    if (!options.skipLogAnalysis) {
      instanceInfo.importantLogLines = readImportantLogLines(instanceInfo.tmpDataDir);
    }
  }

  cleanupDirectories = cleanupDirectories.concat(instanceInfo.tmpDataDir, instanceInfo.flatTmpDataDir);
}

function checkBodyForJsonToParse(request) {
  if (request.hasOwnProperty('body')) {
    var parsedBody = JSON.parse(request.hasOwnProperty('body'));
    request.body = parsedBody;
  }
}

function cleanupDBDirectories(options) {
  if (options.cleanup) {
    while (cleanupDirectories.length) {
      var cleanupDirectory = cleanupDirectories.shift();
      // Avoid attempting to remove the same directory multiple times
      if (cleanupDirectories.indexOf(cleanupDirectory) === -1) {
        fs.removeDirectoryRecursive(cleanupDirectory, true);
      }
    }
  }
}

function makePathUnix (path) {
  return fs.join.apply(null,path.split("/"));
}

function makePathGeneric (path) {
  return fs.join.apply(null,path.split(fs.pathSeparator));
}

var foundTests = false;

var tests_shell_common;
var tests_shell_server_only;
var tests_shell_client_only;
var tests_shell_server;
var tests_shell_client;
var tests_shell_server_aql;
var tests_shell_server_aql_extended;
var tests_shell_server_aql_performance;

function findTests () {
  if (foundTests) {
    return;
  }
  tests_shell_common = _.filter(fs.list(makePathUnix("js/common/tests")),
            function (p) {
              return p.substr(0,6) === "shell-" &&
                     p.substr(-3) === ".js";
            }).map(
            function(x) {
              return fs.join(makePathUnix("js/common/tests"),x);
            }).sort();
  tests_shell_server_only = _.filter(fs.list(makePathUnix("js/server/tests")),
            function (p) {
              return p.substr(0,6) === "shell-" &&
                     p.substr(-3) === ".js";
            }).map(
            function(x) {
              return fs.join(makePathUnix("js/server/tests"),x);
            }).sort();
  tests_shell_client_only = _.filter(fs.list(makePathUnix("js/client/tests")),
            function (p) {
              return p.substr(0,6) === "shell-" &&
                     p.substr(-3) === ".js";
            }).map(
            function(x) {
              return fs.join(makePathUnix("js/client/tests"),x);
            }).sort();
  tests_shell_server_aql = _.filter(fs.list(makePathUnix("js/server/tests")),
            function (p) {
              return p.substr(0,4) === "aql-" &&
                     p.substr(-3) === ".js" &&
                     p.indexOf("ranges-combined") === -1;
            }).map(
            function(x) {
              return fs.join(makePathUnix("js/server/tests"),x);
            }).sort();
  tests_shell_server_aql_extended = 
            _.filter(fs.list(makePathUnix("js/server/tests")),
            function (p) {
              return p.substr(0,4) === "aql-" &&
                     p.substr(-3) === ".js" &&
                     p.indexOf("ranges-combined") !== -1;
            }).map(
            function(x) {
              return fs.join(makePathUnix("js/server/tests"),x);
            }).sort();
  tests_shell_server_aql_performance = 
            _.filter(fs.list(makePathUnix("js/server/perftests")),
            function (p) {
              return p.substr(-3) === ".js";
            }).map(
            function(x) {
              return fs.join(makePathUnix("js/server/perftests"),x);
            }).sort();

  tests_shell_server = tests_shell_common.concat(tests_shell_server_only);
  tests_shell_client = tests_shell_common.concat(tests_shell_client_only);
  foundTests = true;
}

function runThere (options, instanceInfo, file) {
  try {
    var r;
    var t;
    if (file.indexOf("-spec") === -1) {
      t = 'var runTest = require("jsunity").runTest; '+
          'return runTest(' + JSON.stringify(file) + ');';
    }
    else {
      var jasmineReportFormat = options.jasmineReportFormat || 'progress';
      t = 'var executeTestSuite = require("jasmine").executeTestSuite; '+
          'try {' +
          'return { status: executeTestSuite([' + JSON.stringify(file) + '], {"format": '+
          JSON.stringify(jasmineReportFormat) + '}), message: "Success"};'+
          ' } catch (e) {' +
          'return {' +
          '  status: false,' + 
          '  message: e.message || String(e) || "unknown",' +
          '  stack: e.stack' + 
          '};' +
          '}';
    }
    var o = makeAuthorisationHeaders(options);
    o.method = "POST";
    o.timeout = 3600;
    o.returnBodyOnError = true;
    r = download(instanceInfo.url + "/_admin/execute?returnAsJSON=true",t,o);
    if (! r.error && r.code === 200) {
      r = JSON.parse(r.body);
    } else {
      return {
        status: false,
        message: r.body
      };
    }
    return r;
  } catch (e) {
    return {
      status: false,
      message: e.message || String(e),
      stack: e.stack
    };
  }
}


function runHere (options, instanceInfo, file) {
  var result;
  try {
    if (file.indexOf("-spec") === -1) {
      var runTest = require("jsunity").runTest; 
      result = runTest(file);
    }
    else {
      var jasmineReportFormat = options.jasmineReportFormat || 'progress';
      var executeTestSuite = require("jasmine").executeTestSuite; 
      try {
        result = executeTestSuite([ file ], { format: jasmineReportFormat });
      } catch (e) {
        result = {
          status: false,
          message: e.message || String(e) || "unknown",
          stack: e.stack
        };
        return result;
      }
    }
    if (file.indexOf("-spec") !== -1) {
      result = {
        status: result, 
        message: ''
      };
    }
 }
  catch (err) {
    result = err;
  }
  return result;
}

function executeAndWait (cmd, args) {
  var startTime = time();
  var res = executeExternalAndWait(cmd, args);
  var deltaTime = time() - startTime;
  var errorMessage = ' - ';

  if (res.status === "TERMINATED") {
    print("Finished: " + res.status + " exit code: " + res.exit + " Time elapsed: " + deltaTime);
    if (res.exit === 0) {
      return { status: true, message: "", duration: deltaTime};
    }
    else {
      return { status: false, message: "exit code was " + res.exit, duration: deltaTime};
    }
  }
  else if (res.status === "ABORTED") {
    if (typeof(res.errorMessage) !== 'undefined') {
      errorMessage += res.errorMessage;
    }
    print("Finished: " + res.status + " Signal: " + res.signal + " Time elapsed: " + deltaTime + errorMessage);
    return {
      status: false,
      message: "irregular termination: " + res.status + " exit signal: " + res.signal + errorMessage,
      duration: deltaTime
    };
  }
  else {
    if (typeof(res.errorMessage) !== 'undefined') {
      errorMessage += res.errorMessage;
    }
    print("Finished: " + res.status + " exit code: " + res.signal + " Time elapsed: " + deltaTime + errorMessage);
    return {
      status: false,
      message: "irregular termination: " + res.status + " exit code: " + res.exit + errorMessage,
      duration: deltaTime
    };
  }
}

function runInArangosh (options, instanceInfo, file, addArgs) {
  var args = makeTestingArgsClient(options);
  var topDir = findTopDir();
  args["server.endpoint"] = instanceInfo.endpoint;
  args["javascript.unit-tests"] = fs.join(topDir, file);
  if (addArgs !== undefined) {
    args = _.extend(args, addArgs);
  }
  var arangosh = fs.join("bin","arangosh");
  var result;
  var rc = executeAndWait(arangosh, toArgv(args));
  try {
    result = JSON.parse(fs.read("testresult.json"));
  }
  catch(x) {
    return rc;
  }
  if ((typeof(result[0]) === 'object') && 
      result[0].hasOwnProperty('status')) {
    return result[0];
  }
  else {
    // Jasmine tests...
    return rc;
  }
}

function runArangoshCmd (options, instanceInfo, addArgs, cmds) {
  var args = makeTestingArgsClient(options);
  args["server.endpoint"] = instanceInfo.endpoint;
  if (addArgs !== undefined) {
    args = _.extend(args, addArgs);
  }
  var argv = toArgv(args).concat(cmds);
  var arangosh = fs.join("bin", "arangosh");
  return executeAndWait(arangosh, argv);
}

function performTests(options, testList, testname, remote) {
  var instanceInfo;
  if (remote) {
    instanceInfo = startInstance("tcp", options, [], testname);
    if (instanceInfo === false) {
      return {status: false, message: "failed to start server!"};
    }
  }
  var results = {};
  var i;
  var te;
  var continueTesting = true;
  var filtered = {};

  if (testList.length === 0) {
    print("Testsuite is empty!");
    return {};
  }

  for (i = 0; i < testList.length; i++) {
    te = testList[i];
    if (filterTestcaseByOptions(te, options, filtered)) {
      if (! continueTesting) {
        print('oops!');
        print("Skipping, " + te + " server is gone.");
        results[te] = {status: false, message: instanceInfo.exitStatus};
        instanceInfo.exitStatus = "server is gone.";
        continue;
      }

      print("\n" + Date() + " arangod: Trying",te,"...");
      var r;
      if (remote) {
        r = runThere(options, instanceInfo, te);
      }
      else {
        r = runHere(options, instanceInfo, te);
      }
      if (r.hasOwnProperty('status')) {
        results[te] = r;
        if (results[te].status === false) {
          options.cleanup = false;
        }
        if (! r.status && ! options.force) {
          break;
        }
      }
      else {
        results[te] = {status: false, message: r};
        if (! options.force) {
          break;
        }
      }
      if (remote) {
        continueTesting = checkInstanceAlive(instanceInfo, options);
      }
    }
    else {
      print("Skipped " + te + " because of " + filtered.filter);
    }
  }
  if (remote) {
    print("Shutting down...");
    shutdownInstance(instanceInfo,options);
  }
  print("done.");
  if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
  }
  return results;
}

function single_usage (testsuite, list) {
  print("single_" + testsuite + ": No test specified!\n Available tests:");
  var filelist = "";

  for (var fileNo in list) {
    if (/\.js$/.test(list[fileNo])) {
      filelist += " " + list[fileNo];
    }
  }
  print(filelist);
  print("usage: single_" + testsuite + " '{\"test\":\"<testfilename>\"}'");
  print(" where <testfilename> is one from the list above.");
  return { status: false, message: "No test specified!"};
}


testFuncs.single_server = function (options) {
  var result = { };
  if (options.test !== undefined) {
    var instanceInfo = startInstance("tcp", options, [], "single_server");
    if (instanceInfo === false) {
      return {status: false, message: "failed to start server!"};
    }
    var te = options.test;
    print("\n" + Date() + " arangod: Trying",te,"...");
    result = {};
    var r = runThere(options, instanceInfo, makePathGeneric(te));

    if (r.hasOwnProperty('status')) {
      result[te] = r;
      if (result[te].status === false) {
        options.cleanup = false;
      }
    }
    print("Shutting down...");
    if (result[te].status === false) {
      options.cleanup = false;
    }
    shutdownInstance(instanceInfo,options);
    print("done.");
    if ((!options.skipLogAnalysis) &&
        instanceInfo.hasOwnProperty('importantLogLines') &&
        Object.keys(instanceInfo.importantLogLines).length > 0) {
      print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
    }
    return result;
  }
  else {
    findTests();
    return single_usage("server", tests_shell_server);
  }
};

testFuncs.single_localserver = function (options) {
  var result = { };
  if (options.test !== undefined) {
    var instanceInfo;
    var te = options.test;
    print("\nArangod: Trying",te,"...");
    result = {};
    result[te] = runHere(options, instanceInfo, makePathGeneric(te));
    if (result[te].status === false) {
      options.cleanup = false;
    }
    return result;
  }
  else {
    findTests();
    return single_usage("localserver", tests_shell_server);
  }
};

testFuncs.single_client = function (options) {
  var result = { };
  if (options.test !== undefined) {
    var instanceInfo = startInstance("tcp", options, [], "single_client");
    if (instanceInfo === false) {
      return {status: false, message: "failed to start server!"};
    }
    var te = options.test;
    print("\n" + Date() + " arangosh: Trying ",te,"...");
    result[te] = runInArangosh(options, instanceInfo, te);

    print("Shutting down...");
    if (result[te].status === false) {
      options.cleanup = false;
    }
    shutdownInstance(instanceInfo,options);
    print("done.");
    if ((!options.skipLogAnalysis) &&
        instanceInfo.hasOwnProperty('importantLogLines') &&
        Object.keys(instanceInfo.importantLogLines).length > 0) {
      print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
    }
    return result;
  }
  else {
    findTests();
    return single_usage("client", tests_shell_client);
  }
};

testFuncs.shell_server_perf = function(options) {
  findTests();
  return performTests(options,
                      tests_shell_server_aql_performance,
                      'shell_server_perf',
                      true);
};

testFuncs.shell_server = function (options) {
  findTests();
  return performTests(options, tests_shell_server, 'shell_server', true);
};

testFuncs.shell_server_only = function (options) {
  findTests();
  return performTests(options,
                      tests_shell_server_only,
                      'shell_server_only',
                      true);
};

testFuncs.shell_server_aql = function(options) {
  findTests();
  if (! options.skipAql) {
    if (options.skipRanges) {
      return performTests(options,
                          tests_shell_server_aql,
                          'shell_server_aql_skipranges',
                          true);
    }
    else {
      return performTests(options,
                          tests_shell_server_aql.concat(
                            tests_shell_server_aql_extended),
                          'shell_server_aql',
                          true);
    }
  }
  return "skipped";
};

testFuncs.shell_server_aql_local = function(options) {
  if (!options.hasOwnProperty('cluster')) {
    print('need to specify whether this is a coordinator or not! Add "Cluster":true/false to the options.');
    return;
  }
  findTests();
  if (! options.skipAql) {
    if (options.skipRanges) {
      unitTestPrettyPrintResults(
        performTests(options,
                     tests_shell_server_aql,
                     'shell_server_aql_skipranges',
                     false));
    }
    else {
      unitTestPrettyPrintResults(
        performTests(options,
                     tests_shell_server_aql.concat(
                       tests_shell_server_aql_extended),
                     'shell_server_aql',
                     false));
    }
  }
};

testFuncs.shell_client = function(options) {
  findTests();
  var instanceInfo = startInstance("tcp", options, [], "shell_client");
  var results = {};
  var i;
  var te;
  var continueTesting = true;
  var filtered = {};
  if (instanceInfo === false) {
    return {status: false, message: "failed to start server!"};
  }

  for (i = 0; i < tests_shell_client.length; i++) {
    te = tests_shell_client[i];
    if (filterTestcaseByOptions(te, options, filtered)) {

      if (!continueTesting) {
        print("Skipping, " + te + " server is gone.");
        results[te] = {status: false, message: instanceInfo.exitStatus};
        instanceInfo.exitStatus = "server is gone.";
        continue;
      }

      print("\narangosh: Trying",te,"...");

      var r = runInArangosh(options, instanceInfo, te);
      results[te] = r;
      if (r.status !== true) {
        options.cleanup = false;
        if (! options.force) {
          break;
        }
      }

      continueTesting = checkInstanceAlive(instanceInfo, options);
    }
    else {
      print("Skipped " + te + " because of " + filtered.filter);
    }
  }
  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");
  if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
  }
  return results;
};

testFuncs.config = function (options) {
  if (options.skipConfig) {
    return {};
  }
  var topDir = findTopDir();
  var results = {};
  var ts = ["arangod",
            "arangob",
            "arangodump",
            "arangoimp",
            "arangorestore",
            "arangosh"];
  var args;
  var t;
  var i;
  print("--------------------------------------------------------------------------------");
  print("Absolut config tests");
  print("--------------------------------------------------------------------------------");
  for (i = 0; i < ts.length; i++) {
    t = ts[i];
    args = {
      "configuration" : fs.join(topDir,"etc","arangodb",t+".conf"),
      "flatCommands"  : ["--help"]
    };
    results[t] = executeAndWait(fs.join(topDir,"bin",t),
                                toArgv(args));
    print("Args for [" + t + "]:");
    print(yaml.safeDump(args));
    print("Result: " + results[t].status);
  }
  print("--------------------------------------------------------------------------------");
  print("relative config tests");
  print("--------------------------------------------------------------------------------");
  for (i = 0; i < ts.length; i++) {
    t = ts[i];

    args = {
      "configuration" : fs.join(topDir,"etc","relative",t+".conf"),
      "flatCommands"  : ["--help"]
    };

    results[t+"_rel"] = executeAndWait(fs.join(topDir,"bin",t),
                                       toArgv(args));
    print("Args for (relative) [" + t + "]:");
    print(yaml.safeDump(args));
    print("Result: " + results[t + "_rel"].status);
  }

  return results;
};

testFuncs.boost = function (options) {
  var topDir = findTopDir();
  var results = {};
  if (! options.skipBoost) {
    results.basics = executeAndWait(fs.join(topDir,"UnitTests","basics_suite"),
                                    ["--show_progress"]);
  }
  if (! options.skipGeo) {
    results.geo_suite = executeAndWait(
                          fs.join(topDir,"UnitTests","geo_suite"),
                          ["--show_progress"]);
  }
  return results;
};

function rubyTests (options, ssl) {
  var instanceInfo;
  if (ssl) {
    instanceInfo = startInstance("ssl", options, [], "ssl_server");
  }
  else {
    instanceInfo = startInstance("tcp", options, [], "http_server");
  }
  if (instanceInfo === false) {
    return {status: false, message: "failed to start server!"};
  }

  var tmpname = fs.getTempFile()+".rb";
  fs.write(tmpname,'RSpec.configure do |c|\n'+
                   '  c.add_setting :ARANGO_SERVER\n'+
                   '  c.ARANGO_SERVER = "' +
                          instanceInfo.endpoint.substr(6) + '"\n'+
                   '  c.add_setting :ARANGO_SSL\n'+
                   '  c.ARANGO_SSL = "' + (ssl ? '1' : '0') + '"\n'+
                   '  c.add_setting :ARANGO_USER\n'+
                   '  c.ARANGO_USER = "' + options.username + '"\n'+
                   '  c.add_setting :ARANGO_PASSWORD\n'+
                   '  c.ARANGO_PASSWORD = "' + options.password + '"\n'+
                   'end\n');
  var logsdir = fs.join(findTopDir(),"logs");
  try {
    fs.makeDirectory(logsdir);
  }
  catch (err) {
  }
  var files = fs.list(fs.join("UnitTests","HttpInterface"));
  var filtered = {};
  var result = {};
  var args;
  var i;
  var continueTesting = true;
  var command;
  if (require("internal").platform.substr(0,3) === 'win') {
    command = "rspec.bat";
  }
  else {
    command = "rspec";
  }

  for (i = 0; i < files.length; i++) {
    var te = files[i];
    if (te.substr(0,4) === "api-" && te.substr(-3) === ".rb") {
      if (filterTestcaseByOptions(te, options, filtered)) {

        args = ["--color", "-I", fs.join("UnitTests","HttpInterface"),
                "--format", "d", "--require", tmpname,
                fs.join("UnitTests","HttpInterface", te)];

        if (! continueTesting) {
          print("Skipping " + te + " server is gone.");
          result[te] = {status: false, message: instanceInfo.exitStatus};
          instanceInfo.exitStatus = "server is gone.";
          continue;
        }
        print("\nTrying ",te,"...");

        result[te] = executeAndWait(command, args);
        if (result[te].status === false) {
          options.cleanup = false;
          if (!options.force) {
            break;
          }
        }

        continueTesting = checkInstanceAlive(instanceInfo, options);

      }
      else {
        print("Skipped " + te + " because of " + filtered.filter);
      }
    }
  }

  print("Shutting down...");
  fs.remove(tmpname);
  shutdownInstance(instanceInfo,options);
  print("done.");
  if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
  }
  return result;
}

testFuncs.http_server = function (options) {
  return rubyTests(options, false);
};

testFuncs.ssl_server = function (options) {
  if (options.hasOwnProperty('skipSsl')) {
    return {};
  }
  return rubyTests(options, true);
};

function runArangoImp (options, instanceInfo, what) {
  var topDir = findTopDir();
  var args = {
    "server.username": options.username,
    "server.password": options.password,
    "server.endpoint": instanceInfo.endpoint,
    "file":            fs.join(topDir,what.data),
    "collection":      what.coll,
    "type":            what.type
  };
  if (what.create !== undefined) {
    args["create-collection"] = what.create;
  }
  if (what.backslash !== undefined) {
    args["backslash-escape"] = what.backslash;
  }
  if (what.separator !== undefined) {
    args["separator"] = what.separator;
  }
  var arangoimp = fs.join("bin","arangoimp");
  return executeAndWait(arangoimp, toArgv(args));
}

function runArangoDumpRestore (options, instanceInfo, which, database) {
  var args = {
    "configuration":   "none",
    "server.username": options.username,
    "server.password": options.password,
    "server.endpoint": instanceInfo.endpoint,
    "server.database": database
  };
  var exe;
  if (which === "dump") {
    args["output-directory"] = fs.join(instanceInfo.tmpDataDir,"dump");
    exe = fs.join("bin","arangodump");
  }
  else {
    args["create-database"] = "true";
    args["input-directory"] = fs.join(instanceInfo.tmpDataDir,"dump");
    exe = fs.join("bin","arangorestore");
  }
  return executeAndWait(exe, toArgv(args));
}

function runArangoBenchmark (options, instanceInfo, cmds) {
  var args = {
    "configuration":   "none",
    "server.username": options.username,
    "server.password": options.password,
    "server.endpoint": instanceInfo.endpoint,
  };

  args = _.extend(args, cmds);

  if (!args.hasOwnProperty('verbose')) {
    args.quiet = true;
  }
  var exe = fs.join("bin","arangob");
  return executeAndWait(exe, toArgv(args));
}

var impTodo = [
  {id: "json1", data: makePathUnix("UnitTests/import-1.json"),
   coll: "UnitTestsImportJson1", type: "json", create: undefined},
  {id: "json2", data: makePathUnix("UnitTests/import-2.json"),
   coll: "UnitTestsImportJson2", type: "json", create: undefined},
  {id: "json3", data: makePathUnix("UnitTests/import-3.json"),
   coll: "UnitTestsImportJson3", type: "json", create: undefined},
  {id: "json4", data: makePathUnix("UnitTests/import-4.json"),
   coll: "UnitTestsImportJson4", type: "json", create: undefined},
  {id: "json5", data: makePathUnix("UnitTests/import-5.json"),
   coll: "UnitTestsImportJson5", type: "json", create: undefined},
  {id: "csv1", data: makePathUnix("UnitTests/import-1.csv"),
   coll: "UnitTestsImportCsv1", type: "csv", create: "true"},
  {id: "csv2", data: makePathUnix("UnitTests/import-2.csv"),
   coll: "UnitTestsImportCsv2", type: "csv", create: "true"},
  {id: "csv3", data: makePathUnix("UnitTests/import-3.csv"),
   coll: "UnitTestsImportCsv3", type: "csv", create: "true"},
  {id: "csv4", data: makePathUnix("UnitTests/import-4.csv"),
   coll: "UnitTestsImportCsv4", type: "csv", create: "true", separator: ";", backslash: true},
  {id: "csv5", data: makePathUnix("UnitTests/import-5.csv"),
   coll: "UnitTestsImportCsv5", type: "csv", create: "true", separator: ";", backslash: true},
  {id: "tsv1", data: makePathUnix("UnitTests/import-1.tsv"),
   coll: "UnitTestsImportTsv1", type: "tsv", create: "true"},
  {id: "tsv2", data: makePathUnix("UnitTests/import-2.tsv"),
   coll: "UnitTestsImportTsv2", type: "tsv", create: "true"},
  {id: "edge", data: makePathUnix("UnitTests/import-edges.json"),
   coll: "UnitTestsImportEdge", type: "json", create: "false"}
];

testFuncs.importing = function (options) {
  if (options.cluster) {
    print("Skipped because of cluster.");
    return {"importing": 
            {
              "status" : true,
              "message": "skipped because of cluster",
              "skipped": true
            }
           };
  }

  var instanceInfo = startInstance("tcp", options, [ ], "importing");
  if (instanceInfo === false) {
    return {status: false, message: "failed to start server!"};
  }

  var result = {};
  try {
    var r = runInArangosh(options, instanceInfo,
                          makePathUnix("js/server/tests/import-setup.js"));
    result.setup = r;
    if (r.status !== true) {
      throw "banana";
    }
    var i;
    for (i = 0; i < impTodo.length; i++) {
      r = runArangoImp(options, instanceInfo, impTodo[i]);
      result[impTodo[i].id] = r;
      if (r.status !== true && !options.force) {
        throw "banana";
      }
    }
    r = runInArangosh(options, instanceInfo,
                      makePathUnix("js/server/tests/import.js"));
    result.check = r;
    r = runInArangosh(options, instanceInfo,
                      makePathUnix("js/server/tests/import-teardown.js"));
    result.teardown = r;
  }
  catch (banana) {
    print("A banana of the following form was caught:",yaml.safeDump(banana));
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo,options);
  print("done.");
  if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
  }
  return result;
};

testFuncs.upgrade = function (options) {
  if (options.cluster) {
    return true;
  }

  var result = {};

  var tmpDataDir = fs.getTempFile();
  fs.makeDirectoryRecursive(tmpDataDir);
  var appDir = fs.join(tmpDataDir, "app");
 
  // We use the PortFinder to find a free port for our subinstance,
  // to this end, we have to fake a dummy dispatcher:
  var dispatcher = {endpoint: "tcp://127.0.0.1:", avoidPorts: [], id: "me"};
  var pf = new PortFinder([8529],dispatcher);
  var port = pf.next();
  var args = makeTestingArgs(appDir);
  var endpoint = "tcp://127.0.0.1:"+port;
  args["server.endpoint"]    = endpoint;
  args["database.directory"] = fs.join(tmpDataDir,"data");
  fs.makeDirectoryRecursive(fs.join(tmpDataDir,"data"));
  var argv = toArgv(args).concat(["--upgrade"]);
  result.first = executeAndWait(fs.join("bin","arangod"), argv);

  if (result.first !== 0 && !options.force) {
    print("not removing " + tmpDataDir);
    return result;
  }
  result.second = executeAndWait(fs.join("bin","arangod"), argv);

  cleanupDirectories.push(tmpDataDir);

  return result;
};

testFuncs.foxx_manager = function (options) {
  print("foxx_manager tests...");
  var instanceInfo = startInstance("tcp", options, [], "foxx_manager");
  var results = {};
  if (instanceInfo === false) {
    return {status: false, message: "failed to start server!"};
  }

  results.update = runArangoshCmd(options, instanceInfo,
                                  {"configuration": "etc/relative/foxx-manager.conf"},
                                  ["update"]);
  if (results.update.status === true || options.force) {
    results.search = runArangoshCmd(options, instanceInfo,
                                    {"configuration": "etc/relative/foxx-manager.conf"},
                                    ["search","itzpapalotl"]);
  }
  print("Shutting down...");
  shutdownInstance(instanceInfo,options);
  print("done.");
  if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
  }
  return results;
};

// TODO write test for 2.6-style queues
// testFuncs.queue_legacy = function (options) {
//   if (options.skipFoxQueues) {
//     print("skipping test of legacy queue job types");
//     return {};
//   }
//   var startTime;
//   var queueAppMountPath = '/test-queue-legacy';
//   print("Testing legacy queue job types");
//   var instanceInfo = startInstance("tcp", options, [], "queue_legacy");
//   if (instanceInfo === false) {
//     return {status: false, message: "failed to start server!"};
//   }
//   var data = {
//     naive: {_key: 'potato', hello: 'world'},
//     forced: {_key: 'tomato', hello: 'world'},
//     plain: {_key: 'banana', hello: 'world'}
//   };
//   var results = {};
//   results.install = runArangoshCmd(options, instanceInfo, {
//     "configuration": "etc/relative/foxx-manager.conf"
//   }, [
//     "install",
//     "js/common/test-data/apps/queue-legacy-test",
//     queueAppMountPath
//   ]);

//   print("Restarting without foxx-queues-warmup-exports...");
//   shutdownInstance(instanceInfo, options);
//   instanceInfo = startInstance("tcp", options, {
//     "server.foxx-queues-warmup-exports": "false"
//   }, "queue_legacy", instanceInfo.flatTmpDataDir);
//   if (instanceInfo === false) {
//     return {status: false, message: "failed to restart server!"};
//   }
//   print("done.");

//   var res, body;
//   startTime = time();
//   try {
//     res = download(
//       instanceInfo.url + queueAppMountPath + '/',
//       JSON.stringify(data.naive),
//       {method: 'POST'}
//     );
//     body = JSON.parse(res.body);
//     results.naive = {status: body.success === false, message: JSON.stringify({body: res.body, code: res.code})};
//   } catch (e) {
//     results.naive = {status: true, message: JSON.stringify({body: res.body, code: res.code})};
//   }
//   results.naive.duration = time() - startTime;

//   startTime = time();
//   try {
//     res = download(
//       instanceInfo.url + queueAppMountPath + '/?allowUnknown=true',
//       JSON.stringify(data.forced),
//       {method: 'POST'}
//     );
//     body = JSON.parse(res.body);
//     results.forced = (
//       body.success
//       ? {status: true}
//       : {status: false, message: body.error, stacktrace: body.stacktrace}
//      );
//   } catch (e) {
//     results.forced = {status: false, message: JSON.stringify({body: res.body, code: res.code})};
//   }
//   results.forced.duration = time() - startTime;

//   print("Restarting with foxx-queues-warmup-exports...");
//   shutdownInstance(instanceInfo, options);
//   instanceInfo = startInstance("tcp", options, {
//     "server.foxx-queues-warmup-exports": "true"
//   }, "queue_legacy", instanceInfo.flatTmpDataDir);
//   if (instanceInfo === false) {
//     return {status: false, message: "failed to restart server!"};
//   }
//   print("done.");

//   startTime = time();
//   try {
//     res = download(instanceInfo.url + queueAppMountPath + '/', JSON.stringify(data.plain), {method: 'POST'});
//     body = JSON.parse(res.body);
//     results.plain = (
//       body.success
//       ? {status: true}
//       : {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//     );
//   } catch (e) {
//     results.plain = {status: false, message: JSON.stringify({body: res.body, code: res.code})};
//   }
//   results.plain.duration = time() - startTime;

//   startTime = time();
//   try {
//     for (var i = 0; i < 60; i++) {
//       wait(1);
//       res = download(instanceInfo.url + queueAppMountPath + '/');
//       body = JSON.parse(res.body);
//       if (body.length === 2) {
//         break;
//       }
//     }
//     results.final = (
//       body.length === 2 && body[0]._key === data.forced._key && body[1]._key === data.plain._key
//       ? {status: true}
//       : {status: false, message: JSON.stringify({body: res.body, code: res.code})}
//     );
//   } catch (e) {
//     results.final = {status: false, message: JSON.stringify({body: res.body, code: res.code})};
//   }
//   results.final.duration = time() - startTime;

//   results.uninstall = runArangoshCmd(options, instanceInfo, {
//     "configuration": "etc/relative/foxx-manager.conf"
//   }, [
//     "uninstall",
//     queueAppMountPath
//   ]);
//   print("Shutting down...");
//   shutdownInstance(instanceInfo, options);
//   print("done.");
//   if ((!options.skipLogAnalysis) &&
//       instanceInfo.hasOwnProperty('importantLogLines') &&
//       Object.keys(instanceInfo.importantLogLines).length > 0) {
//     print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
//   }
//   return results;
// };

testFuncs.dump = function (options) {
  var cluster;

  if (options.cluster) {
    cluster = "-cluster";
  }
  else {
    cluster = "";
  }
  print("dump tests...");
  var instanceInfo = startInstance("tcp",options, [], "dump");
  if (instanceInfo === false) {
    return {status: false, message: "failed to start server!"};
  }
  var results = {};
  print(Date() + ": Setting up");
  results.setup = runInArangosh(options, instanceInfo,
       makePathUnix("js/server/tests/dump-setup"+cluster+".js"));
  if (checkInstanceAlive(instanceInfo, options) &&
      (results.setup.status === true)) {
    print(Date() + ": Dump and Restore - dump");
    results.dump = runArangoDumpRestore(options, instanceInfo, "dump",
                                        "UnitTestsDumpSrc");
    if (checkInstanceAlive(instanceInfo, options)) {
      print(Date() + ": Dump and Restore - restore");
      results.restore = runArangoDumpRestore(options, instanceInfo, "restore",
                                             "UnitTestsDumpDst");
    
      if (checkInstanceAlive(instanceInfo, options)) {
        print(Date() + ": Dump and Restore - dump 2");
        results.test = runInArangosh(options, instanceInfo,
                                     makePathUnix("js/server/tests/dump"+cluster+".js"),
                                     { "server.database": "UnitTestsDumpDst"});
        if (checkInstanceAlive(instanceInfo, options)) {
          print(Date() + ": Dump and Restore - teardown");
          results.tearDown = runInArangosh(options, instanceInfo,
                                           makePathUnix("js/server/tests/dump-teardown"+cluster+".js"));
        }
      }
    }
  }
  print("Shutting down...");
  shutdownInstance(instanceInfo,options);
  print("done.");
  if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
  }
  return results;
};

var benchTodo = [
  {"requests":"10000", "concurrency":"2", "test":"version",       "keep-alive":"false"},
  {"requests":"10000", "concurrency":"2", "test":"version",       "async":"true"},
  {"requests":"20000", "concurrency":"1", "test":"version",       "async":"true"},
  {"requests":"100000","concurrency":"2", "test":"shapes",        "batch-size":"16",  "complexity":"2"},
  {"requests":"100000","concurrency":"2", "test":"shapes-append", "batch-size":"16",  "complexity":"4"},
  {"requests":"100000","concurrency":"2", "test":"random-shapes", "batch-size":"16",  "complexity":"2"},
  {"requests":"1000",  "concurrency":"2", "test":"version",       "batch-size":"16"},
  {"requests":"100",   "concurrency":"1", "test":"version",       "batch-size": "0"},
  {"requests":"100",   "concurrency":"2", "test":"document",      "batch-size":"10",  "complexity":"1"},
  {"requests":"2000",  "concurrency":"2", "test":"crud",                              "complexity":"1"},
  {"requests":"4000",  "concurrency":"2", "test":"crud-append",                       "complexity":"4"},
  {"requests":"4000",  "concurrency":"2", "test":"edge",                              "complexity":"4"},
  {"requests":"5000",  "concurrency":"2", "test":"hash",                              "complexity":"1"},
  {"requests":"5000",  "concurrency":"2", "test":"skiplist",                          "complexity":"1"},
  {"requests":"500",   "concurrency":"3", "test":"aqltrx",                            "complexity":"1"},
  {"requests":"100",   "concurrency":"3", "test":"counttrx"},
  {"requests":"500",   "concurrency":"3", "test":"multitrx"}
];

testFuncs.arangob = function (options) {
  print("arangob tests...");
  var instanceInfo = startInstance("tcp",options, [], "arangob");
  if (instanceInfo === false) {
    return {status: false, message: "failed to start server!"};
  }
  var results = {};
  var i,r;
  var continueTesting = true;

  for (i = 0; i < benchTodo.length; i++) {
    // On the cluster we do not yet have working transaction functionality:
    if (! options.cluster ||
        (benchTodo[i].test !== "counttrx" &&
         benchTodo[i].test !== "multitrx")) {

      if (!continueTesting) {
        print("Skipping " + benchTodo[i] + ", server is gone.");
        results[i] = {status: false, message: instanceInfo.exitStatus};
        instanceInfo.exitStatus = "server is gone.";
        continue;
      }
      var args = benchTodo[i];
      if (options.hasOwnProperty('benchargs')) {
        args = _.extend(args, options.benchargs);
      }
      r = runArangoBenchmark(options, instanceInfo, args);
      results[i] = r;

      continueTesting = checkInstanceAlive(instanceInfo, options);

      if (r.status !== true && !options.force) {
        break;
      }
    }
  }
  print("Shutting down...");
  shutdownInstance(instanceInfo,options);
  print("done.");
  if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
  }
  return results;
};

testFuncs.authentication = function (options) {
  if (options.skipAuth === true) {
    print("skipping Authentication tests!");
    return {};
  }

  print("Authentication tests...");
  var instanceInfo = startInstance("tcp", options,
                                   {"server.disable-authentication": "false"},
                                   "authentication");
  if (instanceInfo === false) {
    return {status: false, message: "failed to start server!"};
  }
  var results = {};
  results.auth = runInArangosh(options, instanceInfo,
                               fs.join("js","client","tests","auth.js"));
  print("Shutting down...");
  shutdownInstance(instanceInfo,options);
  print("done.");
  if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
  }
  return results;
};

var urlsTodo = [
  "/_api/",
  "/_api",
  "/_api/version",
  "/_admin/html",
  "/_admin/html/",
  "/test",
  "/the-big-fat-fox"
];

var authTestServerParams = [
  {"server.disable-authentication":   "false",
   "server.authenticate-system-only": "false"},
  {"server.disable-authentication":   "false",
   "server.authenticate-system-only": "true"},
  {"server.disable-authentication":   "true",
   "server.authenticate-system-only": "true"}
];
var authTestNames = ["Full",
                     "SystemAuth",
                     "None"];
var authTestExpectRC = [
  [401, 401, 401, 401, 401, 401, 401],
  [401, 401, 401, 401, 401, 404, 404],
  [404, 404, 200, 301, 301, 404, 404]
];

testFuncs.authentication_parameters = function (options) {
  if (options.skipAuth === true) {
    print("skipping Authentication with parameters tests!");
    return {};
  }
  print("Authentication with parameters tests...");
  var results = {};

  var continueTesting = true;
  var downloadOptions = {
    followRedirects:false,
    returnBodyOnError:true
  };

  if (typeof(options.valgrind) === 'string') {
    downloadOptions.timeout = 300;
  }

  var test;
  for (test = 0; test < 3; test++) {
    var all_ok = true;
    var instanceInfo = startInstance("tcp", options,
                                     authTestServerParams[test],
                                     "authentication_parameters_" + authTestNames[test]);
    if (instanceInfo === false) {
      return {status: false, message: authTestNames[test] + ": failed to start server!"};
    }
    var r;
    var i;
    var testName = 'auth_' + authTestNames[test];

    print(Date() + " Starting " + authTestNames[test] + " test");
    results[testName] = {};
    for (i = 0; i < urlsTodo.length; i++) {
      print("  URL: " + instanceInfo.url + urlsTodo[i]);
      if (!continueTesting) {
        print("Skipping " + urlsTodo[i] + ", server is gone.");
        results[testName][urlsTodo[i]] = {
          status: false,
          message: instanceInfo.exitStatus
        };
        instanceInfo.exitStatus = "server is gone.";
        all_ok = false;
        continue;
      }

      r = download(instanceInfo.url + urlsTodo[i],"", downloadOptions);
      if (r.code === authTestExpectRC[test][i]) {
        results[testName][urlsTodo[i]] = { status: true, message: ""};
      }
      else {
        checkBodyForJsonToParse(r);
        results[testName][urlsTodo[i]] = { 
          status: false,
          message: "we expected " + 
            authTestExpectRC[test][i] +
            " and we got "
            + r.code +
            " Full Status: "
            + yaml.safeDump(r)
        };
        all_ok = false;
      }
      continueTesting = checkInstanceAlive(instanceInfo, options);
    }
    results[testName].status = all_ok;

    print("Shutting down " + authTestNames[test] + " test...");
    shutdownInstance(instanceInfo, options);
    if ((!options.skipLogAnalysis) &&
        instanceInfo.hasOwnProperty('importantLogLines') &&
        (instanceInfo.importantLogLines.length > 0)) {
      print("Found messages in the server logs: \n" + yaml.safeDump(instanceInfo.importantLogLines));
    }
    print("done with " + authTestNames[test] + " test.");
  }
  return results;
};

var internalMembers = ["code", "error", "status", "duration", "failed", "total", "crashed", "all_ok", "ok", "message"];

function unitTestPrettyPrintResults(r) {
  var testrun;
  var test;
  var key;
  var oneTest;
  var testFail = 0;
  var testSuiteFail = 0;
  var success = "";
  var fail = "";

  try {
    for (testrun in r) {
      if (r.hasOwnProperty(testrun) && (internalMembers.indexOf(testrun) === -1)) {
        var isSuccess = true;
        var oneOutput = "";

        oneOutput = "* Testrun: " + testrun + "\n";
        var successTests = {};
        for (test in  r[testrun]) {
          if (r[testrun].hasOwnProperty(test) && (internalMembers.indexOf(test) === -1)) {
            if (r[testrun][test].status) {
              var where = test.lastIndexOf(fs.pathSeparator);
              var which;
              if (where < 0 ) {
                which = 'Unittests';
              }
              else {
                which = test.substring(0, where);
                test = test.substring(where + 1, test.length);
              }
              if (!successTests.hasOwnProperty(which)) {
                successTests[which]=[];
              }
              successTests[which].push(test);
            }
            else {
              testSuiteFail++;
              if (r[testrun][test].hasOwnProperty('message')) {
                isSuccess = false;
                oneOutput += "     [  Fail ] " + test + ": Whole testsuite failed!\n";
                if (typeof r[testrun][test].message === "object" &&
                    r[testrun][test].message.hasOwnProperty('body')) {
                  oneOutput += r[testrun][test].message.body + "\n";
                }
                else {
                  oneOutput += r[testrun][test].message + "\n";
                }
              }
              else {
                isSuccess = false;
                oneOutput += "    [  Fail ] " + test + "\n";
                for (oneTest in r[testrun][test]) {
                  if (r[testrun][test].hasOwnProperty(oneTest) && 
                      (internalMembers.indexOf(oneTest) === -1) &&
                      (! r[testrun][test][oneTest].status)) {
                    testFail++;
                    oneOutput += "          -> " + oneTest + " Failed; Verbose message:\n";
                    oneOutput += r[testrun][test][oneTest].message + "\n";
                  }
                }
              }
            }
          }
        }
        if (successTests !== "") {
          for (key in successTests) {
            if (successTests.hasOwnProperty(key)) {
              oneOutput = "     [Success] " + key + " / [" + successTests[key].join(', ') + ']\n' + oneOutput;
            }
          }
        }
        if (isSuccess) {
          success += oneOutput;
        }
        else {
          fail += oneOutput;
        }
          
      }
    }
    if (success !== "") {
      print(success);
    }
    if (fail !== "") {
      print(fail);
    }
    print("Overall state: " + ((r.all_ok === true) ? "Success" : "Fail"));
    if (r.all_ok !== true) {
      print("   Suites failed: " + testSuiteFail + " Tests Failed: " + testFail);
    }
    if (r.crashed === true) {
      print("   We had at least one unclean shutdown of an arangod during the testrun.");
    }
  }
  catch (x) {
    print("exception caught while pretty printing result: ");
    print(x.message);
    print(JSON.stringify(r));
  }
}

function UnitTest (which, options) {
  var allok = true;
  var results = {};
  if (typeof options !== "object") {
    options = {};
  }
  _.defaults(options, optionsDefaults);
  if (which === undefined) {
    printUsage();
    return;
  }
  var jsonReply = options.jsonReply;
  delete options.jsonReply;
  var i;
  var ok;
  if (which === "all") {
    var n;
    for (n = 0; n < allTests.length; n++) {
      print("Doing test",allTests[n],"with options",options);
      results[allTests[n]] = r = testFuncs[allTests[n]](options);
      ok = true;
      for (i in r) {
        if (r.hasOwnProperty(i)) {
          if (r[i].status !== true) {
            ok = false;
          }
        }
      }
      r.ok = ok;
      if (!ok) {
        allok = false;
      }
      results.all_ok = allok;
    }
    results.all_ok = allok;
    results.crashed = serverCrashed;
    if (allok) {
      cleanupDBDirectories(options);
    }
    else {
      print("since some tests weren't successfully, not cleaning up: \n" +
            yaml.safeDump(cleanupDirectories));
    }
    if (jsonReply === true ) {
      return results;
    }
    else {
      unitTestPrettyPrintResults(results);
      return allok;
    }
  }
  else if (!testFuncs.hasOwnProperty(which)) {
    printUsage();
    throw 'Unknown test "' + which + '"';
  }
  else {
    var r;
    results[which] = r = testFuncs[which](options);
    // print("Testresult:", yaml.safeDump(r));
    ok = true;
    for (i in r) {
      if (r.hasOwnProperty(i) &&
          (which !== "single" || i !== "test")) {
        if (r[i].status !== true) {
          ok = false;
          allok = false;
        }
      }
    }
    r.ok = ok;
    results.all_ok = ok;
    results.crashed = serverCrashed;
    if (allok) {
      cleanupDBDirectories(options);
    }
    else {
      print("since some tests weren't successfully, not cleaning up: \n" +
            yaml.safeDump(cleanupDirectories));
    }
    if (jsonReply === true ) {
      return results;
    }
    else {
      unitTestPrettyPrintResults(results);
      return allok;
    }
  }
}

exports.internalMembers = internalMembers;
exports.testFuncs = testFuncs;
exports.UnitTest = UnitTest;
exports.unitTestPrettyPrintResults = unitTestPrettyPrintResults;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
