/*jshint globalstrict:false, unused:false */
/*global start_pretty_print */
'use strict';

const fs = require("fs");
const internal = require("internal");
const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const download = internal.download;
const print = internal.print;
const wait = internal.wait;
const killExternal = internal.killExternal;
const toArgv = internal.toArgv;
const statusExternal = internal.statusExternal;
const testPort = internal.testPort;

const yaml = require("js-yaml");

const documentationSourceDirs = [
  fs.join(fs.makeAbsolute(''), "Documentation/Scripts/setup-arangosh.js"),
  fs.join(fs.makeAbsolute(''), "Documentation/DocuBlocks"),
  fs.join(fs.makeAbsolute(''), "Documentation/Books/Manual"),
  fs.join(fs.makeAbsolute(''), "Documentation/Books/AQL"),
  fs.join(fs.makeAbsolute(''), "Documentation/Books/HTTP")
];

const theScript = 'utils/generateExamples.py';

const scriptArguments = {
  'outputDir': fs.join(fs.makeAbsolute(''), "Documentation/Examples"),
  'outputFile': fs.join(fs.makeAbsolute(''), "arangosh.examples.js")
};

const programPaths = [
  "build/bin/",
  "build/bin/RelWithDebInfo/",
  "bin/"
];

let ARANGOD;
let ARANGOSH;

function locateProgram(programName, errMsg) {
  for (let programPath of programPaths) {
    let path = fs.join(fs.join(fs.makeAbsolute('')), programPath, programName);
    if (fs.isFile(path) || fs.isFile(path + ".exe")) {
      return path;
    }
  }
  throw errMsg;
}

function endpointToURL(endpoint) {
  if (endpoint.substr(0, 6) === "ssl://") {
    return "https://" + endpoint.substr(6);
  }

  const pos = endpoint.indexOf("://");

  if (pos === -1) {
    return "http://" + endpoint;
  }

  return "http" + endpoint.substr(pos);
}

function findFreePort() {
  while (true) {
    const port = Math.floor(Math.random() * (65536 - 1024)) + 1024;
    const free = testPort("tcp://0.0.0.0:" + port);

    if (free) {
      return port;
    }
  }

  return 8529;
}

function main(argv) {
  let thePython = 'python';
  if (fs.exists('build/CMakeCache.txt')) {
    let CMakeCache = fs.readFileSync('build/CMakeCache.txt');
    thePython = CMakeCache.toString().match(/^PYTHON_EXECUTABLE:FILEPATH=(.*)$/m)[1];
  }
  let options = {};
  let serverEndpoint = '';
  let startServer = true;
  let instanceInfo = {};
  let serverCrashed = false;
  let protocol = 'tcp';
  let tmpDataDir = fs.getTempFile();
  let count = 0;

  try {
    options = internal.parseArgv(argv, 0);
  } catch (x) {
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
    if (scriptArguments.hasOwnProperty('onlyThisOne')) {
      throw("don't run the full suite on pre-existing servers");
    }
    startServer = false;
    serverEndpoint = options['server.endpoint'];
    
  }

  let args = [theScript].concat(internal.toArgv(scriptArguments));
  args = args.concat(['--arangoshSetup']);
  args = args.concat(documentationSourceDirs);

  let res = executeExternalAndWait(thePython, args);

  if (res.exit !== 0) {
    print("parsing the examples failed - aborting!");
    print(res);
    return -1;
  }

  if (startServer) {
    let port = findFreePort();
    instanceInfo.port = port;
    serverEndpoint = protocol + "://127.0.0.1:" + port;

    instanceInfo.url = endpointToURL(serverEndpoint);

    fs.makeDirectoryRecursive(fs.join(tmpDataDir, "data"));

    let serverArgs = {};
    fs.makeDirectoryRecursive(fs.join(tmpDataDir, "apps"));

    serverArgs["configuration"] = "none";
    serverArgs["database.directory"] = fs.join(tmpDataDir, "data");
    serverArgs["javascript.app-path"] = fs.join(tmpDataDir, "apps");
    serverArgs["javascript.startup-directory"] = "js";
    serverArgs["javascript.module-directory"] = "enterprise/js";
    serverArgs["log.file"] = fs.join(tmpDataDir, "log");
    serverArgs["server.authentication"] = "false";
    serverArgs["server.endpoint"] = serverEndpoint;
    serverArgs["server.storage-engine"] = "rocksdb"; // examples depend on it
    serverArgs["backup.api-enabled"] = "true";

    print("================================================================================");
    ARANGOD = locateProgram("arangod", "Cannot find arangod to execute tests against");
    print(ARANGOD);
    print(toArgv(serverArgs));
    instanceInfo.pid = executeExternal(ARANGOD, toArgv(serverArgs)).pid;

    // Wait until the server is up:
    count = 0;
    instanceInfo.endpoint = serverEndpoint;

    while (true) {
      wait(0.5, false);
      let r = download(instanceInfo.url + "/_api/version", "");

      if (!r.error && r.code === 200) {
        break;
      }

      count++;

      if (count % 60 === 0) {
        res = statusExternal(instanceInfo.pid, false);

        if (res.status !== "RUNNING") {
          print("start failed - process is gone: " + yaml.safeDump(res));
          return 1;
        }
      }
    }
  }

  let arangoshArgs = {
    'configuration': fs.join(fs.makeAbsolute(''), 'etc', 'relative', 'arangosh.conf'),
    'server.password': "",
    'server.endpoint': serverEndpoint,
    'javascript.execute': scriptArguments.outputFile
  };

  print("--------------------------------------------------------------------------------");
  ARANGOSH = locateProgram("arangosh", "Cannot find arangosh to run tests with");
  print(ARANGOSH);
  print(internal.toArgv(arangoshArgs));
  res = executeExternalAndWait(ARANGOSH, internal.toArgv(arangoshArgs));

  if (startServer) {
    if (typeof(instanceInfo.exitStatus) === 'undefined') {
      download(instanceInfo.url + "/_admin/shutdown", "", {method: "DELETE"});

      print("Waiting for server shut down");
      count = 0;
      let bar = "[";

      while (1) {
        instanceInfo.exitStatus = statusExternal(instanceInfo.pid, false);

        if (instanceInfo.exitStatus.status === "RUNNING") {
          count++;
          if (typeof(options.valgrind) === 'string') {
            wait(1);
            continue;
          }
          if (count % 10 === 0) {
            bar = bar + "#";
          }
          if (count > 600) {
            print("forcefully terminating " + yaml.safeDump(instanceInfo.pid) +
              " after 600 s grace period; marking crashy.");
            serverCrashed = true;
            killExternal(instanceInfo.pid);
            break;
          } else {
            wait(1);
          }
        } else if (instanceInfo.exitStatus.status !== "TERMINATED") {
          if (instanceInfo.exitStatus.hasOwnProperty('signal')) {
            print("Server shut down with : " +
              yaml.safeDump(instanceInfo.exitStatus) +
              " marking build as crashy.");
            serverCrashed = true;
            break;
          }
          if (internal.platform.substr(0, 3) === 'win') {
            // Windows: wait for procdump to do its job...
            statusExternal(instanceInfo.monitor, true);
          }
        } else {
          print("Server shutdown: Success.");
          break; // Success.
        }
      }

      if (count > 10) {
        print("long Server shutdown: " + bar + ']');
      }

    }
  }

  if (res.exit != 0) {
    throw("generating examples failed!");
  }

  return 0;
}

main(ARGUMENTS);
