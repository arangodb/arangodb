/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global ArangoServerState, require, exports */

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
///
///   - "all": do all tests
///   - "make"
///   - "config"
///   - "boost"
///   - "shell_server"
///   - "shell_server_ahuacatl"
///   - "http_server"
///   - "ssl_server"
///   - "shell_client"
///   - "dump"
///   - "arangob"
///   - "import"
///   - "upgrade"
///   - "dfdb"
///   - "foxx-manager"
///   - "authentication"
///   - "authentication-parameters
///
/// The following properties of `options` are defined:
///
///   - `force`: if set to true the tests are continued even if one fails
///   - `skipBoost`: if set to true the boost unittests are skipped
///   - `skipGeo`: if set to true the geo index tests are skipped
///   - `skipAhuacatl`: if set to true the ahuacatl tests are skipped
///   - `skipRanges`: if set to true the ranges tests are skipped
///   - `valgrind`: if set to true the arangods are run with the valgrind
///     memory checker
///   - `cluster`: if set to true the tests are run with the coordinator
///     of a small local cluster
////////////////////////////////////////////////////////////////////////////////

var _ = require("underscore");

var testFuncs = {};
var print = require("internal").print;
var fs = require("fs");
var download = require("internal").download;
var wait = require("internal").wait;
var executeExternal = require("internal").executeExternal;
var killExternal = require("internal").killExternal;
var statusExternal = require("internal").statusExternal;
var base64Encode = require("internal").base64Encode;

var PortFinder = require("org/arangodb/cluster").PortFinder;
var Planner = require("org/arangodb/cluster").Planner;
var Kickstarter = require("org/arangodb/cluster").Kickstarter;

function findTopDir () {
  return fs.normalize(fs.join(ArangoServerState.executablePath(), "..",".."));
}
  
function makeTestingArgs () {
  var topDir = findTopDir();
  return [ "--configuration",                  "none",
           "--database.maximal-journal-size",  "1048576",
           "--database.force-sync-properties", "false",
           "--javascript.gc-interval",         "1",
           "--javascript.app-path",            fs.join(topDir,"js","apps"),
           "--javascript.startup-directory",   fs.join(topDir,"js"),
           "--ruby.action-directory",          fs.join(topDir,"mr","actions"),
           "--ruby.modules-path", 
             fs.join(topDir,"mr","server","modules")+":"+
             fs.join(topDir,"mr","common","modules"),
           "--server.threads",                 "4",
           "--server.disable-authentication",  "true" ];
}

function makeTestingArgsClient (options) {
  var topDir = findTopDir();
  return [ "--configuration",                  "none",
           "--javascript.startup-directory",   fs.join(topDir,"js"),
           "--no-colors",
           "--quiet",
           "--server.username",                options.username,
           "--server.password",                options.password ];
}

function makeAuthorisationHeaders (options) {
  return {"headers":
            {"Authorization": "Basic " + base64Encode(options.username+":"+
                                                      options.password)}};
}

function startInstance (protocol, options) {
  // protocol must be one of ["tcp", "ssl", "unix"]
  var topDir = fs.normalize(fs.join(ArangoServerState.executablePath(),
                                    "..",".."));
  var instanceInfo = {};
  instanceInfo.topDir = topDir;
  var tmpDataDir = fs.getTempFile();
  fs.makeDirectoryRecursive(tmpDataDir);
  instanceInfo.tmpDataDir = tmpDataDir;

  var endpoint;
  var pos;
  if (options.cluster) {
    // FIXME: protocol and valgrind currently ignored!
    var p = new Planner({"numberOfDBservers":2, 
                         "numberOfCoordinators":1,
                         "dispatchers": 
                           {"me":{"endpoint":"tcp://localhost:",
                                  "arangodExtraArgs":makeTestingArgs(),
                                  "username": "root",
                                  "password": ""}}
                        });
    instanceInfo.kickstarter = new Kickstarter(p.getPlan());
    instanceInfo.kickstarter.launch();
    var runInfo = options.kickstarter.runInfo;
    var roles = runInfo[runInfo.length-1].roles;
    var endpoints = runInfo[runInfo.length-1].endpoints;
    pos = roles.indexOf("Coordinator");
    endpoint = endpoints[pos];
  }
  else {   // single instance mode
    // We use the PortFinder to find a free port for our subinstance,
    // to this end, we have to fake a dummy dispatcher:
    var dispatcher = {endpoint: "tcp://localhost:", avoidPorts: {}, id: "me"};
    var pf = new PortFinder([8529],dispatcher);
    var port = pf.next();
    instanceInfo.port = port;
    var args = makeTestingArgs();
    args.push("--server.endpoint");
    endpoint = protocol+"://127.0.0.1:"+port;
    args.push(endpoint);
    args.push("--database.directory");
    args.push(fs.join(tmpDataDir,"data"));
    fs.makeDirectoryRecursive(fs.join(tmpDataDir,"data"));
    args.push("--log.file");
    args.push(fs.join(tmpDataDir,"log"));
    instanceInfo.pid = executeExternal(ArangoServerState.executablePath(), 
                                       args);
  }

  // Wait until the server/coordinator is up:
  var up = false;
  var url;
  if (protocol === "ssl") {
    url = "https";
  }
  else {
    url = "http";
  }
  pos = endpoint.indexOf("://");
  url += endpoint.substr(pos);
  print(url);
  while (true) {
    wait(0.5);
    var r = download(url+"/_api/version","",makeAuthorisationHeaders(options));
    if (!r.error && r.code === 200) {
      break;
    }
  }

  instanceInfo.endpoint = endpoint;
  instanceInfo.url = url;

  return instanceInfo;
}

function shutdownInstance (instanceInfo, options) {
  if (options.cluster) {
    instanceInfo.kickstarter.shutdown();
    instanceInfo.kickstarter.cleanup();
  }
  else {
    download(instanceInfo.url+"/_admin/shutdown","",
             makeAuthorisationHeaders(options));
    wait(10);
    killExternal(instanceInfo.pid);
  }
  fs.removeDirectoryRecursive(instanceInfo.tmpDataDir);
}

function makePath (path) {
  return fs.join.apply(null,path.split("/"));
}

var tests_shell_common =
  [ makePath("js/common/tests/shell-require.js"),
    makePath("js/common/tests/shell-aqlfunctions.js"),
    makePath("js/common/tests/shell-attributes.js"),
    makePath("js/common/tests/shell-base64.js"),
    makePath("js/common/tests/shell-collection.js"),
    makePath("js/common/tests/shell-collection-volatile.js"),
    makePath("js/common/tests/shell-crypto.js"),
    makePath("js/common/tests/shell-database.js"),
    makePath("js/common/tests/shell-document.js"),
    makePath("js/common/tests/shell-download.js"),
    makePath("js/common/tests/shell-edge.js"),
    makePath("js/common/tests/shell-fs.js"),
    makePath("js/common/tests/shell-graph-traversal.js"),
    makePath("js/common/tests/shell-graph-algorithms.js"),
    makePath("js/common/tests/shell-graph-measurement.js"),
    makePath("js/common/tests/shell-keygen.js"),
    makePath("js/common/tests/shell-simple-query.js"),
    makePath("js/common/tests/shell-statement.js"),
    makePath("js/common/tests/shell-transactions.js"),
    makePath("js/common/tests/shell-unload.js"),
    makePath("js/common/tests/shell-users.js"),
    makePath("js/common/tests/shell-index.js"),
    makePath("js/common/tests/shell-index-geo.js"),
    makePath("js/common/tests/shell-cap-constraint.js"),
    makePath("js/common/tests/shell-unique-constraint.js"),
    makePath("js/common/tests/shell-hash-index.js"),
    makePath("js/common/tests/shell-fulltext.js"),
    makePath("js/common/tests/shell-graph.js")
  ];

var tests_shell_server_only =
  [
    makePath("js/server/tests/cluster.js"),
    makePath("js/server/tests/compaction.js"),
    makePath("js/server/tests/transactions.js"),
    makePath("js/server/tests/routing.js"),
    makePath("js/server/tests/shell-any.js"),
    makePath("js/server/tests/shell-bitarray-index.js"),
    makePath("js/server/tests/shell-database.js"),
    makePath("js/server/tests/shell-foxx.js"),
    makePath("js/server/tests/shell-foxx-repository.js"),
    makePath("js/server/tests/shell-foxx-model.js"),
    makePath("js/server/tests/shell-foxx-base-middleware.js"),
    makePath("js/server/tests/shell-foxx-template-middleware.js"),
    makePath("js/server/tests/shell-foxx-format-middleware.js"),
    makePath("js/server/tests/shell-foxx-preprocessor.js"),
    makePath("js/server/tests/shell-skiplist-index.js"),
    makePath("js/server/tests/shell-skiplist-rm-performance.js"),
    makePath("js/server/tests/shell-skiplist-correctness.js")
  ];

var tests_shell_server = tests_shell_common.concat(tests_shell_server_only);

var tests_shell_server_ahuacatl =
  [
    makePath("js/server/tests/ahuacatl-ranges.js"),
    makePath("js/server/tests/ahuacatl-queries-optimiser.js"),
    makePath("js/server/tests/ahuacatl-queries-optimiser-in.js"),
    makePath("js/server/tests/ahuacatl-queries-optimiser-limit.js"),
    makePath("js/server/tests/ahuacatl-queries-optimiser-sort.js"),
    makePath("js/server/tests/ahuacatl-queries-optimiser-ref.js"),
    makePath("js/server/tests/ahuacatl-escaping.js"),
    makePath("js/server/tests/ahuacatl-functions.js"),
    makePath("js/server/tests/ahuacatl-variables.js"),
    makePath("js/server/tests/ahuacatl-bind.js"),
    makePath("js/server/tests/ahuacatl-complex.js"),
    makePath("js/server/tests/ahuacatl-logical.js"),
    makePath("js/server/tests/ahuacatl-arithmetic.js"),
    makePath("js/server/tests/ahuacatl-relational.js"),
    makePath("js/server/tests/ahuacatl-ternary.js"),
    makePath("js/server/tests/ahuacatl-parse.js"),
    makePath("js/server/tests/ahuacatl-hash.js"),
    makePath("js/server/tests/ahuacatl-skiplist.js"),
    makePath("js/server/tests/ahuacatl-cross.js"),
    makePath("js/server/tests/ahuacatl-graph.js"),
    makePath("js/server/tests/ahuacatl-edges.js"),
    makePath("js/server/tests/ahuacatl-refaccess-variable.js"),
    makePath("js/server/tests/ahuacatl-refaccess-attribute.js"),
    makePath("js/server/tests/ahuacatl-queries-simple.js"),
    makePath("js/server/tests/ahuacatl-queries-variables.js"),
    makePath("js/server/tests/ahuacatl-queries-geo.js"),
    makePath("js/server/tests/ahuacatl-queries-fulltext.js"),
    makePath("js/server/tests/ahuacatl-queries-collection.js"),
    makePath("js/server/tests/ahuacatl-queries-noncollection.js"),
    makePath("js/server/tests/ahuacatl-subquery.js"),
    makePath("js/server/tests/ahuacatl-operators.js")
  ];

var tests_shell_server_ahuacatl_extended =
  [
    makePath("js/server/tests/ahuacatl-ranges-combined-1.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-2.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-3.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-4.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-5.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-6.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-7.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-8.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-9.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-10.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-11.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-12.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-13.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-14.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-15.js"),
    makePath("js/server/tests/ahuacatl-ranges-combined-16.js")
  ];

var tests_shell_client_only = 
  [
    makePath("js/client/tests/shell-endpoints.js"),
    makePath("js/client/tests/shell-fm.js"),
    makePath("js/client/tests/client.js")
  ];

var tests_shell_client = tests_shell_common.concat(tests_shell_client_only);

function performTests(options, testList) {
  var instanceInfo = startInstance("tcp",options);
  var results = [];
  var i;
  var te;
  for (i = 0; i < testList.length; i++) {
    te = testList[i];
    print("\nTrying",te,"...");
    var r;
    try {
      var t = 'var runTest = require("jsunity").runTest; '+
              'return runTest("'+te+'");';
      var o = makeAuthorisationHeaders(options);
      o.method = "POST";
      o.timeout = 24*3600;
      r = download(instanceInfo.url+"/_admin/execute?returnAsJSON=true",t,o);
      if (!r.error && r.code === 200) {
        r = JSON.parse(r.body);
      }
    }
    catch (err) {
      r = err;
    }
    results.push(r);
    if (r !== true && !options.force) {
      break;
    }
  }
  print("Shutting down...");
  shutdownInstance(instanceInfo,options);
  print("done.");
  return results;
}

testFuncs.shell_server = function (options) {
  return performTests(options, tests_shell_server);
};

testFuncs.shell_server_only = function (options) {
  return performTests(options, tests_shell_server_only);
};

testFuncs.shell_server_ahuacatl = function(options) {
  if (!options.skipAhuacatl) {
    if (options.skipRanges) {
      return performTests(options, tests_shell_server_ahuacatl);
    }
    return performTests(options, tests_shell_server_ahuacatl.concat(
                                 tests_shell_server_ahuacatl_extended));
  }
  return "skipped";
};

testFuncs.shell_client = function(options) {
  var topDir = fs.normalize(fs.join(ArangoServerState.executablePath(),
                                    "..",".."));
  var instanceInfo = startInstance("tcp",options);
  print("Waiting...");
  wait(30);
  var args = makeTestingArgsClient(options);
  args.push("--server.endpoint");
  args.push(instanceInfo.endpoint);
  args.push("--javascript.unit-tests");
  var results = [];
  var i;
  var te;
  for (i = 0; i < tests_shell_client.length; i++) {
    te = tests_shell_client[i];
    print("\nTrying",te,"...");
    var r;
    args.push(fs.join(topDir,te));
    var arangosh = fs.normalize(fs.join(ArangoServerState.executablePath(),
                                        "..","arangosh"));
    print(arangosh);
    print(args);
    var pid = executeExternal(arangosh, args);
    var stat;
    while (true) {
      wait(0.1);
      stat = statusExternal(pid);
      print(stat);
      if (stat.status !== "RUNNING") { break; }
    }
    r = stat.exit;
    results.push(r);
    args.pop();
    if (r !== 0 && !options.force) {
      break;
    }
  }
  print("Shutting down...");
  shutdownInstance(instanceInfo,options);
  print("done.");
  return results;
};

testFuncs.dummy = function (options) {
  var instanceInfo = startInstance("tcp",options);
  print("Startup done.");
  wait(3600);
  print("Shutting down...");
  shutdownInstance(instanceInfo,options);
  print("done.");
  return undefined;
};

var optionsDefaults = { "cluster": false,
                        "valgrind": false,
                        "force": false,
                        "skipBoost": false,
                        "skipGeo": false,
                        "skipAhuacatl": false,
                        "skipRanges": false,
                        "username": "root",
                        "password": "" };

var allTests = 
  [
    "make",
    "codebase_static",
    "config",
    "boost",
    "shell_server",
    "shell_server_ahuacatl",
    "http_server",
    "ssl_server",
    "shell_client",
    "dump",
    "arangob",
    "import",
    "upgrade",
    "dfdb",
    "foxx_manager",
    "authentication",
    "authentication_parameters"
  ];

function printUsage () {
  print("Usage: UnitTest( which, options )");
  print('       where "which" is one of:');
  print('         "all": do all tests');
  var i;
  for (i in testFuncs) {
    if (testFuncs.hasOwnProperty(i)) {
      print('         "'+i+'"');
    }
  }
  print('       and options can contain the following boolean properties:');
  print('         "force": continue despite a failed test'); 
  print('         "skipBoost": skip the boost unittests');
  print('         "skipGeo": skip the geo index tests');
  print('         "skipAhuacatl": skip the ahuacatl tests');
  print('         "skipRanges": skip the ranges tests');
  print('         "valgrind": arangods are run with valgrind');
  print('         "cluster": tests are run on a small local cluster');
}

function UnitTest (which, options) {
  if (typeof options !== "object") {
    options = {};
  }
  _.defaults(options, optionsDefaults);
  if (which === undefined) {
    printUsage();
    return;
  }
  if (which === "all") {
    var n;
    var results = {};
    for (n = 0;n < allTests.length;n++) {
      results[allTests[n]] = testFuncs[allTests[n]](options);
    }
    return results;
  }
  if (!testFuncs.hasOwnProperty(which)) {
    printUsage();
    throw 'Unknown test "'+which+'"';
  }
  var r = {};
  r[which] = testFuncs[which](options);
  return r;
}

exports.UnitTest = UnitTest;

// PROBLEME:
//     context-Problem wg. HTTP REST execute statt command line
//        ==> testMode
//   shell_server:
//        "js/server/tests/transactions.js",
//   shell_server_ahuacatl:
//        "js/server/tests/ahuacatl-functions.js",
//        
// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
