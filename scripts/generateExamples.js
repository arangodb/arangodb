/*jshint globalstrict:false, unused:false */
/*global start_pretty_print */

var fs = require("fs");
var internal = require("internal");
var executeExternal = require("internal").executeExternal;
var executeExternalAndWait = internal.executeExternalAndWait;
var download = require("internal").download;
var print = internal.print;
var wait = require("internal").wait;
var killExternal = require("internal").killExternal;
var toArgv = require("internal").toArgv;
var statusExternal = require("internal").statusExternal;

var yaml = require("js-yaml");
var endpointToURL = require("org/arangodb/cluster/planner").endpointToURL;
var PortFinder = require("org/arangodb/cluster").PortFinder;

var documentationSourceDirs = [
  fs.join(fs.makeAbsolute(''), "Documentation/Examples/setup-arangosh.js"),
  fs.join(fs.makeAbsolute(''), "js/actions"),
  fs.join(fs.makeAbsolute(''), "js/client"),
  fs.join(fs.makeAbsolute(''), "js/common"),
  fs.join(fs.makeAbsolute(''), "js/server"),
  fs.join(fs.makeAbsolute(''), "js/apps/system/_api/gharial/APP"),
  fs.join(fs.makeAbsolute(''), "Documentation/Books/Users"),
  fs.join(fs.makeAbsolute(''), "arangod/RestHandler"),
  fs.join(fs.makeAbsolute(''), "arangod/V8Server")];


	

var theScript = 'Documentation/Scripts/generateExamples.py';

var scriptArguments = {
  'outputDir': fs.join(fs.makeAbsolute(''), "Documentation/Examples"),
  'outputFile': '/tmp/arangosh.examples.js'
};

function main (argv) {
  "use strict";
  var thePython = 'python';
  var test = argv[1];
  var options = {};
  var serverEndpoint = '';
  var startServer = true;
  var instanceInfo = {};
  var serverCrashed = false;
  var protocol = 'tcp';
  var tmpDataDir = fs.getTempFile();
  var count = 0;

  try {
    options = internal.parseArgv(argv, 1);
  }
  catch (x) {
    print("failed to parse the options: " + x.message);
    return -1;
  }

  if (options.hasOwnProperty('withPython')) {
    thePython = options.withPython;
  }

  if (options.hasOwnProperty('onlyThisOne')) {
    scriptArguments.onlyThisOne = options.onlyThisOne;
  }

  if (options.hasOwnProperty('server.endpoint')) {
    startServer = false;
    serverEndpoint = options['server.endpoint'];
  }
  var args = [theScript].concat(internal.toArgv(scriptArguments));
  args = args.concat(['--arangoshSetup']);
  args = args.concat(documentationSourceDirs);

  internal.print(JSON.stringify(args));

  var res = executeExternalAndWait(thePython, args);

  if (startServer) {
    // We use the PortFinder to find a free port for our subinstance,
    // to this end, we have to fake a dummy dispatcher:
    var dispatcher = {endpoint: "tcp://127.0.0.1:", avoidPorts: {}, id: "me"};
    var pf = new PortFinder([8529],dispatcher);
    var port = pf.next();
    instanceInfo.port = port;
    serverEndpoint = protocol+"://127.0.0.1:"+port;
    
    instanceInfo.url = endpointToURL(serverEndpoint);

    var serverArgs = {};
    serverArgs["server.endpoint"] = serverEndpoint;
    serverArgs["database.directory"] = fs.join(tmpDataDir,"data");
    fs.makeDirectoryRecursive(fs.join(tmpDataDir,"data"));
    args["log.file"] = fs.join(tmpDataDir,"log");
    instanceInfo.pid = executeExternal(fs.join("bin","arangod"), toArgv(serverArgs));
    // Wait until the server is up:
    count = 0;
    instanceInfo.endpoint = serverEndpoint;

    while (true) {
      wait(0.5, false);
      var r = download(instanceInfo.url + "/_api/version", "");

      if (! r.error && r.code === 200) {
        break;
      }
      count ++;
      if (count % 60 === 0) {
        res = statusExternal(instanceInfo.pid, false);
        if (res.status !== "RUNNING") {
          print("start failed - process is gone: " + yaml.safeDump(res));
          return 1;
        }
      }
    }
  }
  var arangoshArgs = {
    'configuration': fs.join(fs.makeAbsolute(''), 'etc', 'relative', 'arangosh.conf'),
    'server.password': "",
    'server.endpoint': serverEndpoint,
    'javascript.execute': scriptArguments.outputFile
  };

  res = executeExternalAndWait('bin/arangosh', internal.toArgv(arangoshArgs));

  if (startServer) {
    if (typeof(instanceInfo.exitStatus) === 'undefined') {
      download(instanceInfo.url+"/_admin/shutdown","");

      print("Waiting for server shut down");
      count = 0;
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
            print("forcefully terminating " + yaml.safeDump(instanceInfo.pid) +
                  " after 600 s grace period; marking crashy.");
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
            print("Server shut down with : " +
                  yaml.safeDump(instanceInfo.exitStatus) +
                  " marking build as crashy.");
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
  }
  return 0;
}
