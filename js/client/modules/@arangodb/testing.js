/*jshint strict: false, sub: true */
/*global print, arango */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  "all": "run all tests (marked with [x])",
  "arangob": "arangob tests",
  "arangosh": "arangosh exit codes tests",
  "authentication": "authentication tests",
  "authentication_parameters": "authentication parameters tests",
  "boost": "boost test suites",
  "config": "checks the config file parsing",
  "dump": "dump tests",
  "foxx_manager": "foxx manager tests",
  "http_server": "http server tests",
  "importing": "import tests",
  "shell_client": "shell client tests",
  "shell_server": "shell server tests",
  "shell_server_aql": "AQL tests in the server",
  "shell_server_only": "server specific tests",
  "shell_server_perf": "bulk tests intended to get an overview of executiontime needed",
  "single_client": "run one test suite isolated via the arangosh; options required\n" +
    "            run without arguments to get more detail",
  "single_server": "run one test suite on the server; options required\n" +
    "            run without arguments to get more detail",
  "ssl_server": "https server tests",
  "upgrade": "upgrade tests"
};

const optionsDocumentation = [
  '',
  ' The following properties of `options` are defined:',
  '',
  '   - `jsonReply`: if set a json is returned which the caller has to ',
  '        present the user',
  '   - `force`: if set to true the tests are continued even if one fails',
  '',
  '   - `skipAql`: if set to true the AQL tests are skipped',
  '   - `skipArangoBNonConnKeepAlive`: if set to true benchmark do not use keep-alive',
  '   - `skipArangoB`: if set to true benchmark tests are skipped',
  '   - `skipAuth : testing authentication will be skipped.',
  '   - `skipBoost`: if set to true the boost unittests are skipped',
  '   - `skipConfig`: omit the noisy configuration tests',
  '   - `skipFoxxQueues`: omit the test for the foxx queues',
  '   - `skipGeo`: if set to true the geo index tests are skipped',
  '   - `skipGraph`: if set to true the graph tests are skipped',
  '   - `skipLogAnalysis`: don\'t try to crawl the server logs',
  '   - `skipMemoryIntense`: tests using lots of resources will be skipped.',
  '   - `skipNightly`: omit the nightly tests',
  '   - `skipRanges`: if set to true the ranges tests are skipped',
  '   - `skipSsl`: omit the ssl_server rspec tests.',
  '   - `skipTimeCritical`: if set to true, time critical tests will be skipped.',
  '   - `skipNondeterministic`: if set, nondeterministic tests are skipped.',
  '',
  '   - `onlyNightly`: execute only the nightly tests',
  '   - `loopEternal`: to loop one test over and over.',
  '   - `loopSleepWhen`: sleep every nth iteration',
  '   - `loopSleepSec`: sleep seconds between iterations',
  '',
  '   - `cluster`: if set to true the tests are run with the coordinator',
  '     of a small local cluster',
  '   - `clusterNodes`: number of DB-Servers to use',
  '   - valgrindHosts  - configure which clustercomponents to run using valgrind',
  '        Coordinator - flag to run Coordinator with valgrind',
  '        DBServer    - flag to run DBServers with valgrind',
  '   - `test`: path to single test to execute for "single" test target',
  '   - `cleanup`: if set to true (the default), the cluster data files',
  '     and logs are removed after termination of the test.',
  '',
  '   - benchargs : additional commandline arguments to arangob',
  '',
  '   - `valgrind`: if set to true the arangods are run with the valgrind',
  '     memory checker',
  '   - `valgrindXmlFileBase`: string to prepend to the xml report name',
  '   - `valgrindargs`: list of commandline parameters to add to valgrind',
  '',
  '   - `extraargs`: list of extra commandline arguments to add to arangod',
  '   - `extremeVerbosity`: if set to true, then there will be more test run',
  '     output, especially for cluster tests.',
  ''
];

const optionsDefaults = {
  "cleanup": true,
  "cluster": false,
  "concurrency": 3,
  "coreDirectory": "/var/tmp",
  "duration": 10,
  "extraargs": [],
  "extremeVerbosity": false,
  "force": true,
  "jsonReply": false,
  "loopEternal": false,
  "loopSleepSec": 1,
  "loopSleepWhen": 1,
  "onlyNightly": false,
  "password": "",
  "skipAql": false,
  "skipArangoB": false,
  "skipArangoBNonConnKeepAlive": false,
  "skipBoost": false,
  "skipGeo": false,
  "skipLogAnalysis": false,
  "skipMemoryIntense": false,
  "skipNightly": true,
  "skipNondeterministic": false,
  "skipRanges": false,
  "skipTimeCritical": false,
  "test": undefined,
  "username": "root",
  "valgrind": false,
  "valgrindXmlFileBase": "",
  "valgrindargs": [],
  "writeXmlReport": true,
};

const _ = require("lodash");
const fs = require("fs");
const yaml = require("js-yaml");

const base64Encode = require("internal").base64Encode;
const download = require("internal").download;
const executeExternal = require("internal").executeExternal;
const executeExternalAndWait = require("internal").executeExternalAndWait;
const killExternal = require("internal").killExternal;
const sleep = require("internal").sleep;
const statusExternal = require("internal").statusExternal;
const testPort = require("internal").testPort;
const time = require("internal").time;
const toArgv = require("internal").toArgv;
const wait = require("internal").wait;

const Planner = require("@arangodb/cluster").Planner;
const Kickstarter = require("@arangodb/cluster").Kickstarter;

let cleanupDirectories = [];
let serverCrashed = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief top-level directory
////////////////////////////////////////////////////////////////////////////////

function findTopDir() {
  const topDir = fs.normalize(fs.makeAbsolute("."));

  if (!fs.exists("3rdParty") && !fs.exists("arangod") &&
    !fs.exists("arangosh") && !fs.exists("UnitTests")) {
    throw "Must be in ArangoDB topdir to execute unit tests.";
  }

  return topDir;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief arguments for testing (server)
////////////////////////////////////////////////////////////////////////////////

function makeArgsArangod(options, appDir) {
  const topDir = findTopDir();

  if (appDir === undefined) {
    appDir = fs.getTempPath();
  }

  fs.makeDirectoryRecursive(appDir, true);

  return {
    "configuration": "none",
    "server.keyfile": fs.join(topDir, "UnitTests", "server.pem"),
    "database.maximal-journal-size": "1048576",
    "database.force-sync-properties": "false",
    "javascript.app-path": appDir,
    "javascript.startup-directory": fs.join(topDir, "js"),
    "server.threads": "20",
    "javascript.v8-contexts": "5",
    "server.disable-authentication": "true",
    "server.allow-use-database": "true"
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief arguments for testing (client)
////////////////////////////////////////////////////////////////////////////////

function makeArgsArangosh(options) {
  const topDir = findTopDir();

  return {
    "configuration": "none",
    "javascript.startup-directory": fs.join(topDir, "js"),
    "server.username": options.username,
    "server.password": options.password,
    "flatCommands": ["--no-colors", "--quiet"]
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds authorization headers
////////////////////////////////////////////////////////////////////////////////

function makeAuthorizationHeaders(options) {
  return {
    "headers": {
      "Authorization": "Basic " + base64Encode(options.username + ":" +
        options.password)
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts endpoints to URL
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief scans the log files for important infos
////////////////////////////////////////////////////////////////////////////////

function readImportantLogLines(logPath) {
  const list = fs.list(logPath);
  let importantLines = {};

  for (let i = 0; i < list.length; i++) {
    let fnLines = [];

    if (list[i].slice(0, 3) === 'log') {
      const buf = fs.readBuffer(fs.join(logPath, list[i]));
      let lineStart = 0;
      let maxBuffer = buf.length;

      for (let j = 0; j < maxBuffer; j++) {
        if (buf[j] === 10) { // \n
          const line = buf.asciiSlice(lineStart, j);
          lineStart = j + 1;

          // filter out regular INFO lines, and test related messages
          let warn = line.search('WARNING about to execute:') !== -1;
          let info = line.search(' INFO ') !== -1;

          if (warn || info) {
            continue;
          }

          fnLines.push(line);
        }
      }
    }

    if (fnLines.length > 0) {
      importantLines[list[i]] = fnLines;
    }
  }

  return importantLines;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief analyzes a core dump using gdb (Unix)
///
/// We assume the system has core files in /var/tmp/, and we have a gdb.
/// you can do this at runtime doing:
///
/// echo 1 > /proc/sys/kernel/core_uses_pid
/// echo /var/tmp/core-%e-%p-%t > /proc/sys/kernel/core_pattern
///
/// or at system startup by altering /etc/sysctl.d/corepattern.conf : 
/// # We want core files to be located in a central location
/// # and know the PID plus the process name for later use.
/// kernel.core_uses_pid = 1
/// kernel.core_pattern =  /var/tmp/core-%e-%p-%t
////////////////////////////////////////////////////////////////////////////////

function analyzeCoreDump(instanceInfo, options, storeArangodPath, pid) {
  let command;

  command = '(';
  command += "printf 'bt full\\n thread apply all bt\\n';";
  command += "sleep 10;";
  command += "echo quit;";
  command += "sleep 2";
  command += ") | gdb " + storeArangodPath + " ";
  command += options.coreDirectory + "/*core*" + pid + '*';

  const args = ['-c', command];
  print(JSON.stringify(args));

  executeExternalAndWait("/bin/bash", args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief analyzes a core dump using cdb (Windows)
///  cdb is part of the WinDBG package.
////////////////////////////////////////////////////////////////////////////////

function analyzeCoreDumpWindows(instanceInfo) {
  const coreFN = instanceInfo.tmpDataDir + "\\" + "core.dmp";

  if (!fs.exists(coreFN)) {
    print("core file " + coreFN + " not found?");
    return;
  }

  const dbgCmds = [
    "kp", // print curren threads backtrace with arguments
    "~*kb", // print all threads stack traces
    "dv", // analyze local variables (if)
    "!analyze -v", // print verbose analysis
    "q" //quit the debugger
  ];

  const args = [
    "-z",
    coreFN,
    "-c",
    dbgCmds.join("; ")
  ];

  print("running cdb " + JSON.stringify(args));
  executeExternalAndWait("cdb", args);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks of an instance is still alive
////////////////////////////////////////////////////////////////////////////////

function checkInstanceAlive(instanceInfo, options) {
  if (options.cluster === false) {
    if (instanceInfo.hasOwnProperty('exitStatus')) {
      return false;
    }

    const res = statusExternal(instanceInfo.pid, false);
    const ret = res.status === "RUNNING";

    if (!ret) {
      print("ArangoD with PID " + instanceInfo.pid.pid + " gone:");
      print(instanceInfo);

      if (res.hasOwnProperty('signal') &&
        ((res.signal === 11) ||
          (res.signal === 6) ||
          // Windows sometimes has random numbers in signal...
          (require("internal").platform.substr(0, 3) === 'win')
        )
      ) {
        const storeArangodPath = "/var/tmp/arangod_" + instanceInfo.pid.pid;

        print("Core dump written; copying arangod to " +
          instanceInfo.tmpDataDir + " for later analysis.");

        res.gdbHint = "Run debugger with 'gdb " +
          storeArangodPath + " " + options.coreDirectory +
          "/core*" + instanceInfo.pid.pid + "*'";

        if (require("internal").platform.substr(0, 3) === 'win') {
          // Windows: wait for procdump to do its job...
          statusExternal(instanceInfo.monitor, true);
          analyzeCoreDumpWindows(instanceInfo);
        } else {
          fs.copyFile("bin/arangod", storeArangodPath);
          analyzeCoreDump(instanceInfo, options, storeArangodPath, instanceInfo.pid.pid);
        }
      }

      instanceInfo.exitStatus = res;
    }

    if (!ret) {
      print("marking crashy");
      serverCrashed = true;
    }

    return ret;
  }

  // cluster tests
  let clusterFit = true;

  for (let part in instanceInfo.kickstarter.runInfo) {
    if (instanceInfo.kickstarter.runInfo[part].hasOwnProperty("pids")) {
      for (let pid in instanceInfo.kickstarter.runInfo[part].pids) {
        if (instanceInfo.kickstarter.runInfo[part].pids.hasOwnProperty(pid)) {
          const checkpid = instanceInfo.kickstarter.runInfo[part].pids[pid];
          const ress = statusExternal(checkpid, false);

          if (ress.hasOwnProperty('signal') &&
            ((ress.signal === 11) || (ress.signal === 6))) {
            const storeArangodPath = "/var/tmp/arangod_" + checkpid.pid;

            print("Core dump written; copying arangod to " +
              storeArangodPath + " for later analysis.");

            ress.gdbHint = "Run debugger with 'gdb " +
              storeArangodPath +
              " /var/tmp/core*" + checkpid.pid + "*'";

            if (require("internal").platform.substr(0, 3) === 'win') {
              // Windows: wait for procdump to do its job...
              statusExternal(instanceInfo.monitor, true);
              analyzeCoreDumpWindows(instanceInfo);
            } else {
              fs.copyFile("bin/arangod", storeArangodPath);
              analyzeCoreDump(instanceInfo, options, storeArangodPath, checkpid.pid);
            }

            instanceInfo.exitStatus = ress;
            clusterFit = false;
          }
        }
      }
    }
  }

  if (clusterFit && instanceInfo.kickstarter.isHealthy()) {
    return true;
  } else {
    print("marking crashy");
    serverCrashed = true;
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief waits for garbage collection using /_admin/execute
////////////////////////////////////////////////////////////////////////////////

function waitOnServerForGC(instanceInfo, options, waitTime) {
  try {
    print("waiting " + waitTime + " for server GC");
    const remoteCommand = 'require("internal").wait(' + waitTime + ', true);';

    const requestOptions = makeAuthorizationHeaders(options);
    requestOptions.method = "POST";
    requestOptions.timeout = waitTime * 10;
    requestOptions.returnBodyOnError = true;

    const reply = download(
      instanceInfo.url + "/_admin/execute?returnAsJSON=true",
      remoteCommand,
      requestOptions);

    print("waiting " + waitTime + " for server GC - done.");

    if (!reply.error && reply.code === 200) {
      return JSON.parse(reply.body);
    } else {
      return {
        status: false,
        message: yaml.safedump(reply.body)
      };
    }
  } catch (ex) {
    return {
      status: false,
      message: ex.message || String(ex),
      stack: ex.stack
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up the database direcory
////////////////////////////////////////////////////////////////////////////////

function cleanupDBDirectories(options) {
  if (options.cleanup) {
    while (cleanupDirectories.length) {
      const cleanupDirectory = cleanupDirectories.shift();

      // Avoid attempting to remove the same directory multiple times
      if (cleanupDirectories.indexOf(cleanupDirectory) === -1) {
        fs.removeDirectoryRecursive(cleanupDirectory, true);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds a free port
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief build a unix path
////////////////////////////////////////////////////////////////////////////////

function makePathUnix(path) {
  return fs.join.apply(null, path.split("/"));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief build a generic path
////////////////////////////////////////////////////////////////////////////////

function makePathGeneric(path) {
  return fs.join.apply(null, path.split(fs.pathSeparator));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a remote command using /_admin/execute
////////////////////////////////////////////////////////////////////////////////

function runThere(options, instanceInfo, file) {
  try {
    let testCode;

    if (file.indexOf("-spec") === -1) {
      testCode = 'const runTest = require("jsunity").runTest; ' +
        'return runTest(' + JSON.stringify(file) + ', true);';
    } else {
      testCode = 'const runTest = require("@arangodb/mocha-runner"); ' +
        'return runTest(' + JSON.stringify(file) + ', true);';
    }

    let httpOptions = makeAuthorizationHeaders(options);
    httpOptions.method = "POST";
    httpOptions.timeout = 3600;

    if (typeof(options.valgrind) === 'string') {
      httpOptions.timeout *= 2;
    }

    httpOptions.returnBodyOnError = true;

    const reply = download(instanceInfo.url + "/_admin/execute?returnAsJSON=true",
      testCode,
      httpOptions);

    if (!reply.error && reply.code === 200) {
      return JSON.parse(reply.body);
    } else {
      if (reply.hasOwnProperty('body')) {
        return {
          status: false,
          message: reply.body
        };
      } else {
        return {
          status: false,
          message: yaml.safeDump(reply)
        };
      }
    }
  } catch (ex) {
    return {
      status: false,
      message: ex.message || String(ex),
      stack: ex.stack
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a list of tests
////////////////////////////////////////////////////////////////////////////////

function performTests(options, testList, testname) {
  let instanceInfo = startInstance("tcp", options, [], testname);

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  if (testList.length === 0) {
    print("Testsuite is empty!");

    return {
      "EMPTY TESTSUITE": {
        status: false,
        message: "no testsuites found!"
      }
    };
  }

  let results = {};
  let continueTesting = true;

  for (let i = 0; i < testList.length; i++) {
    let te = testList[i];
    let filtered = {};

    if (filterTestcaseByOptions(te, options, filtered)) {
      let first = true;
      let loopCount = 0;

      while (first || options.loopEternal) {
        if (!continueTesting) {
          print('oops!');
          print("Skipping, " + te + " server is gone.");

          results[te] = {
            status: false,
            message: instanceInfo.exitStatus
          };

          instanceInfo.exitStatus = "server is gone.";

          break;
        }

        print("\n" + Date() + " arangod: Trying", te, "...");

        let reply = runThere(options, instanceInfo, te);

        if (reply.hasOwnProperty('status')) {
          results[te] = reply;

          if (results[te].status === false) {
            options.cleanup = false;
          }

          if (!reply.status && !options.force) {
            break;
          }
        } else {
          results[te] = {
            status: false,
            message: reply
          };

          if (!options.force) {
            break;
          }
        }

        continueTesting = checkInstanceAlive(instanceInfo, options);

        first = false;

        if (options.loopEternal) {
          if (loopCount % options.loopSleepWhen === 0) {
            print("sleeping...");
            sleep(options.loopSleepSec);
            print("continuing.");
          }

          ++loopCount;
        }
      }
    } else {
      if (options.extremeVerbosity) {
        print("Skipped " + te + " because of " + filtered.filter);
      }
    }
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  if ((!options.skipLogAnalysis) &&
    instanceInfo.hasOwnProperty('importantLogLines') &&
    Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" +
      yaml.safeDump(instanceInfo.importantLogLines));
  }

  return results;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs a stress test on arangod
////////////////////////////////////////////////////////////////////////////////

function runStressTest(options, command, testname) {
  const concurrency = options.concurrency;

  let extra = {
    "javascript.v8-contexts": concurrency + 2,
    "server.threads": concurrency + 2
  };

  let instanceInfo = startInstance("tcp", options, extra, testname);

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  const requestOptions = makeAuthorizationHeaders(options);
  requestOptions.method = "POST";
  requestOptions.returnBodyOnError = true;
  requestOptions.headers = {
    "x-arango-async": "store"
  };

  const reply = download(
    instanceInfo.url + "/_admin/execute?returnAsJSON=true",
    command,
    requestOptions);

  if (reply.error || reply.code !== 202) {
    print("cannot execute command: (" +
      reply.code + ") " + reply.message);

    shutdownInstance(instanceInfo, options);

    return {
      status: false,
      message: reply.hasOwnProperty('body') ? reply.body : yaml.safeDump(reply)
    };
  }

  const id = reply.headers["x-arango-async-id"];

  const checkOpts = makeAuthorizationHeaders(options);
  checkOpts.method = "PUT";
  checkOpts.returnBodyOnError = true;

  while (true) {
    const check = download(
      instanceInfo.url + "/_api/job/" + id,
      "", checkOpts);

    if (!check.error) {
      if (check.code === 204) {
        print("still running (" + (new Date()) + ")");
        wait(60);
        continue;
      }

      if (check.code === 200) {
        print("stress test finished");
        break;
      }
    }

    print(yaml.safeDump(check));

    shutdownInstance(instanceInfo, options);

    return {
      status: false,
      message: check.hasOwnProperty('body') ? check.body : yaml.safeDump(check)
    };
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  return {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a command and wait for result
////////////////////////////////////////////////////////////////////////////////

function executeAndWait(cmd, args) {
  const startTime = time();
  const res = executeExternalAndWait(cmd, args);
  const deltaTime = time() - startTime;

  let errorMessage = ' - ';

  if (res.status === "TERMINATED") {
    print("Finished: " + res.status +
      " exit code: " + res.exit +
      " Time elapsed: " + deltaTime);

    if (res.exit === 0) {
      return {
        status: true,
        message: "",
        duration: deltaTime
      };
    } else {
      return {
        status: false,
        message: "exit code was " + res.exit,
        duration: deltaTime
      };
    }
  } else if (res.status === "ABORTED") {
    if (typeof(res.errorMessage) !== 'undefined') {
      errorMessage += res.errorMessage;
    }

    print("Finished: " + res.status +
      " Signal: " + res.signal +
      " Time elapsed: " + deltaTime + errorMessage);

    return {
      status: false,
      message: "irregular termination: " + res.status +
        " exit signal: " + res.signal + errorMessage,
      duration: deltaTime
    };
  } else {
    if (typeof(res.errorMessage) !== 'undefined') {
      errorMessage += res.errorMessage;
    }

    print("Finished: " + res.status +
      " exit code: " + res.signal +
      " Time elapsed: " + deltaTime + errorMessage);

    return {
      status: false,
      message: "irregular termination: " + res.status +
        " exit code: " + res.exit + errorMessage,
      duration: deltaTime
    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs file in arangosh
////////////////////////////////////////////////////////////////////////////////

function runInArangosh(options, instanceInfo, file, addArgs) {
  const topDir = findTopDir();

  let args = makeArgsArangosh(options);
  args["server.endpoint"] = instanceInfo.endpoint;
  args["javascript.unit-tests"] = fs.join(topDir, file);

  if (addArgs !== undefined) {
    args = _.extend(args, addArgs);
  }

  const arangosh = fs.join("bin", "arangosh");
  let rc = executeAndWait(arangosh, toArgv(args));

  let result;
  try {
    result = JSON.parse(fs.read("testresult.json"));
  } catch (x) {
    return rc;
  }

  if ((typeof result[0] === 'object') &&
    result[0].hasOwnProperty('status')) {
    return result[0];
  } else {
    return rc;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs arangosh
////////////////////////////////////////////////////////////////////////////////

function runArangoshCmd(options, instanceInfo, addArgs, cmds) {
  let args = makeArgsArangosh(options);
  args["server.endpoint"] = instanceInfo.endpoint;

  if (addArgs !== undefined) {
    args = _.extend(args, addArgs);
  }

  const argv = toArgv(args).concat(cmds);
  const arangosh = fs.join("bin", "arangosh");
  return executeAndWait(arangosh, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs arangodump or arangoimp
////////////////////////////////////////////////////////////////////////////////

function runArangoImp(options, instanceInfo, what) {
  const topDir = findTopDir();

  let args = {
    "server.username": options.username,
    "server.password": options.password,
    "server.endpoint": instanceInfo.endpoint,
    "file": fs.join(topDir, what.data),
    "collection": what.coll,
    "type": what.type
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

  const arangoimp = fs.join("bin", "arangoimp");
  return executeAndWait(arangoimp, toArgv(args));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs arangodump or arangorestore
////////////////////////////////////////////////////////////////////////////////

function runArangoDumpRestore(options, instanceInfo, which, database) {
  let args = {
    "configuration": "none",
    "server.username": options.username,
    "server.password": options.password,
    "server.endpoint": instanceInfo.endpoint,
    "server.database": database
  };

  let exe;

  if (which === "dump") {
    args["output-directory"] = fs.join(instanceInfo.tmpDataDir, "dump");
    exe = fs.join("bin", "arangodump");
  } else {
    args["create-database"] = "true";
    args["input-directory"] = fs.join(instanceInfo.tmpDataDir, "dump");
    exe = fs.join("bin", "arangorestore");
  }

  return executeAndWait(exe, toArgv(args));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs arangob
////////////////////////////////////////////////////////////////////////////////

function runArangoBenchmark(options, instanceInfo, cmds) {
  let args = {
    "configuration": "none",
    "server.username": options.username,
    "server.password": options.password,
    "server.endpoint": instanceInfo.endpoint,
    // "server.request-timeout": 1200 // default now. 
    "server.connect-timeout": 10 // 5s default
  };

  args = _.extend(args, cmds);

  if (!args.hasOwnProperty('verbose')) {
    args.quiet = true;
  }

  const exe = fs.join("bin", "arangob");
  return executeAndWait(exe, toArgv(args));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down an instance
////////////////////////////////////////////////////////////////////////////////

function shutdownInstance(instanceInfo, options) {
  if (!checkInstanceAlive(instanceInfo, options)) {
    print("Server already dead, doing nothing. This shouldn't happen?");
  }

  if (options.valgrind) {
    waitOnServerForGC(instanceInfo, options, 60);
  }

  if (options.cluster) {
    let rc = instanceInfo.kickstarter.shutdown();

    if (!options.skipLogAnalysis) {
      instanceInfo.importantLogLines = readImportantLogLines(instanceInfo.tmpDataDir);
    }

    if (options.cleanup) {
      instanceInfo.kickstarter.cleanup();
    }

    if (rc.error && rc.results !== undefined) {
      for (let i = 0; i < rc.results.length; ++i) {
        if (rc.results[i].hasOwnProperty('isStartServers') &&
          (rc.results[i].isStartServers === true)) {
          for (let serverState in rc.results[i].serverStates) {
            if (rc.results[i].serverStates.hasOwnProperty(serverState)) {
              if ((rc.results[i].serverStates[serverState].status === "NOT-FOUND") ||
                (rc.results[i].serverStates[serverState].hasOwnProperty('signal'))) {
                print("Server " + serverState + " shut down with:\n" +
                      yaml.safeDump(rc.results[i].serverStates[serverState]));

                if (!serverCrashed) {
                  print("marking run as crashy");
                  serverCrashed = true;
                }
              }
            }
          }
        }
      }
    }

    killExternal(instanceInfo.dispatcherPid);
  }

  // single server
  else {
    if (typeof(instanceInfo.exitStatus) === 'undefined') {
      download(instanceInfo.url + "/_admin/shutdown", "",
        makeAuthorizationHeaders(options));

      print("Waiting for server shut down");

      let count = 0;
      let bar = "[";

      while (true) {
        instanceInfo.exitStatus = statusExternal(instanceInfo.pid, false);
        if (instanceInfo.exitStatus.status === "RUNNING") {
          ++count;

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
          if (require("internal").platform.substr(0, 3) === 'win') {
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
    } else {
      print("Server already dead, doing nothing.");
    }

    if (!options.skipLogAnalysis) {
      instanceInfo.importantLogLines =
        readImportantLogLines(instanceInfo.tmpDataDir);
    }
  }

  cleanupDirectories = cleanupDirectories.concat(
    instanceInfo.tmpDataDir, instanceInfo.flatTmpDataDir);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts a dispatcher
////////////////////////////////////////////////////////////////////////////////

function startDispatcher(instanceInfo) {
  const cmd = fs.join("bin", "arangod");

  let args = {};

  args["configuration"] = "none";

  args["cluster.agent-path"] = fs.join("bin", "etcd-arango");

  args["cluster.arangod-path"] = fs.join("bin", "arangod");

  args["cluster.coordinator-config"] =
    fs.join("etc", "relative", "arangod-coordinator.conf");

  args["cluster.dbserver-config"] =
    fs.join("etc", "relative", "arangod-dbserver.conf");

  args["cluster.disable-dispatcher-frontend"] = "false";

  args["cluster.disable-dispatcher-kickstarter"] = "false";

  args["cluster.data-path"] = "cluster";

  args["cluster.log-path"] = "cluster";

  args["database.directory"] =
    fs.join(instanceInfo.flatTmpDataDir, "dispatcher");

  args["log.file"] =
    fs.join(instanceInfo.flatTmpDataDir, "dispatcher.log");

  args["server.endpoint"] = arango.getEndpoint();

  args["javascript.startup-directory"] = "js";

  args["javascript.app-path"] = fs.join("js", "apps");

  instanceInfo.dispatcherPid = executeExternal(cmd, toArgv(args));

  while (arango.GET("/_admin/version").error === true) {
    print("Waiting for dispatcher to appear");
    sleep(1);
  }

  print("Dispatcher is ready");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts an instance
///
/// protocol must be one of ["tcp", "ssl", "unix"]
////////////////////////////////////////////////////////////////////////////////

function startInstance(protocol, options, addArgs, testname, tmpDir) {
  const startTime = time();
  const topDir = findTopDir();

  let instanceInfo = {};
  instanceInfo.topDir = topDir;
  instanceInfo.flatTmpDataDir = tmpDir || fs.getTempFile();

  const tmpDataDir = fs.join(instanceInfo.flatTmpDataDir, testname);
  const appDir = fs.join(tmpDataDir, "apps");

  fs.makeDirectoryRecursive(tmpDataDir);
  instanceInfo.tmpDataDir = tmpDataDir;

  let optionsExtraArgs = {};

  if (typeof(options.extraargs) === 'object') {
    optionsExtraArgs = options.extraargs;
  }

  let valgrindopts = {};

  if (typeof(options.valgrindargs) === 'object') {
    valgrindopts = options.valgrindargs;
  }

  let valgrindHosts = '';

  if (typeof(options.valgrindHosts) === 'object') {
    if (options.valgrindHosts.Coordinator === true) {
      valgrindHosts += 'Coordinator';
    }

    if (options.valgrindHosts.DBServer === true) {
      valgrindHosts += 'DBserver';
    }
  }

  let dispatcher;
  let endpoint;

  // cluster mode
  if (options.cluster) {
    startDispatcher(instanceInfo, options);

    let clusterNodes = 2;

    if (options.clusterNodes) {
      clusterNodes = options.clusterNodes;
    }

    let extraargs = makeArgsArangod(options, appDir);
    extraargs = _.extend(extraargs, optionsExtraArgs);

    if (addArgs !== undefined) {
      extraargs = _.extend(extraargs, addArgs);
    }

    dispatcher = {
      "endpoint": "tcp://127.0.0.1:",
      "arangodExtraArgs": toArgv(extraargs),
      "username": "root",
      "password": ""
    };

    print("Temporary cluster data and logs are in", tmpDataDir);

    let runInValgrind = "";
    let valgrindXmlFileBase = "";

    if (typeof(options.valgrind) === 'string') {
      runInValgrind = options.valgrind;
      valgrindXmlFileBase = options.valgrindXmlFileBase;
    }

    let plan = new Planner({
      "numberOfDBservers": clusterNodes,
      "numberOfCoordinators": 1,
      "dispatchers": {
        "me": dispatcher
      },
      "dataPath": tmpDataDir,
      "logPath": tmpDataDir,
      "useSSLonCoordinators": protocol === "ssl",
      "valgrind": runInValgrind,
      "valgrindopts": toArgv(valgrindopts, true),
      "valgrindXmlFileBase": valgrindXmlFileBase + '_cluster',
      "valgrindTestname": testname,
      "valgrindHosts": valgrindHosts,
      "extremeVerbosity": options.extremeVerbosity
    });

    instanceInfo.kickstarter = new Kickstarter(plan.getPlan());

    let rc = instanceInfo.kickstarter.launch();

    if (rc.error) {
      print("Cluster startup failed: " + rc.errorMessage);
      return false;
    }

    let runInfo = instanceInfo.kickstarter.runInfo;
    let j = runInfo.length - 1;

    while (j > 0 && runInfo[j].isStartServers === undefined) {
      --j;
    }

    if ((runInfo.length === 0) || (runInfo[0].error === true)) {
      let error = new Error();
      error.errorMessage = yaml.safeDump(runInfo);
      throw error;
    }

    let roles = runInfo[j].roles;
    let endpoints = runInfo[j].endpoints;
    let pos = roles.indexOf("Coordinator");

    endpoint = endpoints[pos];
  }

  // single instance mode
  else {
    const port = findFreePort();
    instanceInfo.port = port;
    endpoint = protocol + "://127.0.0.1:" + port;

    let td = fs.join(tmpDataDir, "data");
    fs.makeDirectoryRecursive(td);

    let args = makeArgsArangod(options, appDir);
    args["server.endpoint"] = endpoint;
    args["database.directory"] = td;
    args["log.file"] = fs.join(tmpDataDir, "log");

    if (protocol === "ssl") {
      args["server.keyfile"] = fs.join("UnitTests", "server.pem");
    }

    args = _.extend(args, optionsExtraArgs);

    if (addArgs !== undefined) {
      args = _.extend(args, addArgs);
    }

    if (typeof(options.valgrind) === 'string') {
      const run = fs.join("bin", "arangod");
      let testfn = options.valgrindXmlFileBase;

      if (testfn.length > 0) {
        testfn += '_';
      }

      testfn += testname;

      valgrindopts["xml-file"] = testfn + '.%p.xml';
      valgrindopts["log-file"] = testfn + '.%p.valgrind.log';

      // Sequence: Valgrind arguments; binary to run; options to binary:
      const newargs = toArgv(valgrindopts, true).concat([run]).concat(toArgv(args));
      const cmdline = options.valgrind;

      instanceInfo.pid = executeExternal(cmdline, newargs);
    } else {
      instanceInfo.pid = executeExternal(fs.join("bin", "arangod"), toArgv(args));
    }
  }

  // wait until the server/coordinator is up:
  let count = 0;
  const url = endpointToURL(endpoint);

  instanceInfo.url = url;
  instanceInfo.endpoint = endpoint;

  while (true) {
    wait(0.5, false);

    const reply = download(url + "/_api/version", "", makeAuthorizationHeaders(options));

    if (!reply.error && reply.code === 200) {
      break;
    }

    ++count;

    if (count % 60 === 0) {
      if (!checkInstanceAlive(instanceInfo, options)) {
        print("startup failed! bailing out!");
        return false;
      }
    }
  }

  print("up and running in " + (time() - startTime) + " seconds");

  if (!options.cluster && (require("internal").platform.substr(0, 3) === 'win')) {
    const procdumpArgs = [
      '-accepteula',
      '-e',
      '-ma',
      instanceInfo.pid.pid,
      fs.join(tmpDataDir, 'core.dmp')
    ];

    try {
      instanceInfo.monitor = executeExternal('procdump', procdumpArgs);
    } catch (x) {
      print("failed to start procdump - is it installed?");
      throw x;
    }
  }

  return instanceInfo;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief runs ruby tests using RSPEC
////////////////////////////////////////////////////////////////////////////////

function camelize(str) {
  return str.replace(/(?:^\w|[A-Z]|\b\w)/g, function(letter, index) {
    return index === 0 ? letter.toLowerCase() : letter.toUpperCase();
  }).replace(/\s+/g, '');
}

function rubyTests(options, ssl) {
  let instanceInfo;

  if (ssl) {
    instanceInfo = startInstance("ssl", options, [], "ssl_server");
  } else {
    instanceInfo = startInstance("tcp", options, [], "http_server");
  }

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  const tmpname = fs.getTempFile() + ".rb";

  fs.write(tmpname, 'RSpec.configure do |c|\n' +
    '  c.add_setting :ARANGO_SERVER\n' +
    '  c.ARANGO_SERVER = "' +
    instanceInfo.endpoint.substr(6) + '"\n' +
    '  c.add_setting :ARANGO_SSL\n' +
    '  c.ARANGO_SSL = "' + (ssl ? '1' : '0') + '"\n' +
    '  c.add_setting :ARANGO_USER\n' +
    '  c.ARANGO_USER = "' + options.username + '"\n' +
    '  c.add_setting :ARANGO_PASSWORD\n' +
    '  c.ARANGO_PASSWORD = "' + options.password + '"\n' +
    'end\n');

  const logsdir = fs.join(findTopDir(), "logs");

  try {
    fs.makeDirectory(logsdir);
  } catch (err) {}

  const files = fs.list(fs.join("UnitTests", "HttpInterface"));

  let continueTesting = true;
  let filtered = {};
  let result = {};

  let args;
  let command;

  if (require("internal").platform.substr(0, 3) === 'win') {
    command = "rspec.bat";
  } else {
    command = "rspec";
  }

  const parseRspecJson = function(testCase, res, totalDuration) {
    let tName = camelize(testCase.description);
    let status = (testCase.status === "passed");

    res[tName] = {
      status: status,
      message: testCase.full_description,
      duration: totalDuration // RSpec doesn't offer per testcase time...
    };

    res.total++;

    if (!status) {
      const msg = yaml.safeDump(testCase)
        .replace(/.*rspec\/core.*\n/gm, "")
        .replace(/.*rspec\\core.*\n/gm, "");
      print("RSpec test case falied: \n" + msg);
      res[tName].message += "\n" + msg;
    }
  };

  for (let i = 0; i < files.length; i++) {
    const te = files[i];

    if (te.substr(0, 4) === "api-" && te.substr(-3) === ".rb") {
      if (filterTestcaseByOptions(te, options, filtered)) {
        if (!continueTesting) {
          print("Skipping " + te + " server is gone.");

          result[te] = {
            status: false,
            message: instanceInfo.exitStatus
          };

          instanceInfo.exitStatus = "server is gone.";
          break;
        }

        args = ["--color",
          "-I", fs.join("UnitTests", "HttpInterface"),
          "--format", "d",
          "--format", "j",
          "--out", fs.join("out", "UnitTests", te + ".json"),
          "--require", tmpname,
          fs.join("UnitTests", "HttpInterface", te)
        ];

        print("\n" + Date() + " rspec trying", te, "...");

        const res = executeAndWait(command, args);

        result[te] = {
          total: 0,
          status: res.status
        };

        const resultfn = fs.join("out", "UnitTests", te + ".json");

        try {
          const jsonResult = JSON.parse(fs.read(resultfn));

          if (options.extremeVerbosity) {
            print(yaml.safeDump(jsonResult));
          }

          for (let j = 0; j < jsonResult.examples.length; ++j) {
            parseRspecJson(jsonResult.examples[j], result[te],
              jsonResult.summary.duration);
          }

          result[te].duration = jsonResult.summary.duration;
        } catch (x) {
          print("Failed to parse rspec result: " + x);
          result[te]["complete_" + te] = res;

          if (res.status === false) {
            options.cleanup = false;

            if (!options.force) {
              break;
            }
          }
        }

        continueTesting = checkInstanceAlive(instanceInfo, options);
      } else {
        if (options.extremeVerbosity) {
          print("Skipped " + te + " because of " + filtered.filter);
        }
      }
    }
  }

  print("Shutting down...");

  fs.remove(tmpname);
  shutdownInstance(instanceInfo, options);

  print("done.");

  if ((!options.skipLogAnalysis) &&
    instanceInfo.hasOwnProperty('importantLogLines') &&
    Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" +
      yaml.safeDump(instanceInfo.importantLogLines));
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sort test-cases according to pathname
////////////////////////////////////////////////////////////////////////////////

let testsCases = {
  setup: false
};

function findTests() {
  if (testsCases.setup) {
    return;
  }

  testsCases.common = _.filter(fs.list(makePathUnix("js/common/tests/shell")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/common/tests/shell"), x);
    }).sort();

  testsCases.server_only = _.filter(fs.list(makePathUnix("js/server/tests/shell")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/server/tests/shell"), x);
    }).sort();

  testsCases.client_only = _.filter(fs.list(makePathUnix("js/client/tests/shell")),
    function(p) {
      return p.substr(-3) === ".js";
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/client/tests/shell"), x);
    }).sort();

  testsCases.server_aql = _.filter(fs.list(makePathUnix("js/server/tests/aql")),
    function(p) {
      return p.substr(-3) === ".js" && p.indexOf("ranges-combined") === -1;
    }).map(
    function(x) {
      return fs.join(makePathUnix("js/server/tests/aql"), x);
    }).sort();

  testsCases.server_aql_extended =
    _.filter(fs.list(makePathUnix("js/server/tests/aql")),
      function(p) {
        return p.substr(-3) === ".js" && p.indexOf("ranges-combined") !== -1;
      }).map(
      function(x) {
        return fs.join(makePathUnix("js/server/tests/aql"), x);
      }).sort();

  testsCases.server_aql_performance =
    _.filter(fs.list(makePathUnix("js/server/perftests")),
      function(p) {
        return p.substr(-3) === ".js";
      }).map(
      function(x) {
        return fs.join(makePathUnix("js/server/perftests"), x);
      }).sort();

  testsCases.server = testsCases.common.concat(testsCases.server_only);
  testsCases.client = testsCases.common.concat(testsCases.client_only);

  testsCases.setup = true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief filter test-cases according to options
////////////////////////////////////////////////////////////////////////////////

function filterTestcaseByOptions(testname, options, whichFilter) {
  if (options.hasOwnProperty('test') && (typeof(options.test) !== 'undefined')) {
    whichFilter.filter = "testcase";
    return testname === options.test;
  }

  if ((testname.indexOf("-cluster") !== -1) && !options.cluster) {
    whichFilter.filter = 'noncluster';
    return false;
  }

  if (testname.indexOf("-noncluster") !== -1 && options.cluster) {
    whichFilter.filter = 'cluster';
    return false;
  }

  if (testname.indexOf("-timecritical") !== -1 && options.skipTimeCritical) {
    whichFilter.filter = 'timecritical';
    return false;
  }

  if (testname.indexOf("-nightly") !== -1 && options.skipNightly && !options.onlyNightly) {
    whichFilter.filter = 'skip nightly';
    return false;
  }

  if (testname.indexOf("-geo") !== -1 && options.skipGeo) {
    whichFilter.filter = 'geo';
    return false;
  }

  if (testname.indexOf("-nondeterministic") !== -1 && options.skipNondeterministic) {
    whichFilter.filter = 'nondeterministic';
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

  if ((testname.indexOf("-memoryintense") !== -1) && options.skipMemoryIntense) {
    whichFilter.filter = 'memoryintense';
    return false;
  }

  if (testname.indexOf("-nightly") === -1 && options.onlyNightly) {
    whichFilter.filter = 'only nightly';
    return false;
  }

  if ((testname.indexOf("-novalgrind") !== -1) &&
    (typeof(options.valgrind) === 'string')) {
    whichFilter.filter = "skip in valgrind";
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test functions for all
////////////////////////////////////////////////////////////////////////////////

let allTests = [
  "arangob",
  "arangosh",
  "authentication",
  "authentication_parameters",
  "boost",
  "config",
  "dump",
  "http_server",
  "importing",
  "shell_client",
  "shell_server",
  "shell_server_aql",
  "ssl_server",
  "upgrade"
];

////////////////////////////////////////////////////////////////////////////////
/// @brief internal members of the results
////////////////////////////////////////////////////////////////////////////////

const internalMembers = [
  "code",
  "error",
  "status",
  "duration",
  "failed",
  "total",
  "crashed",
  "all_ok",
  "ok",
  "message",
  "suiteName"
];

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: all
////////////////////////////////////////////////////////////////////////////////

let testFuncs = {
  'all': function() {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: arangosh
////////////////////////////////////////////////////////////////////////////////

testFuncs.arangosh = function(options) {
  const arangosh = fs.join("bin", "arangosh");

  let failed = 0;
  let args = makeArgsArangosh(options);

  let ret = {
    "ArangoshExitCodeTest": {
      "testArangoshExitCodeFail": {},
      "testArangoshExitCodeSuccess": {},
      "total": 2,
      "duration": 0.0
    }
  };

  // termination by throw
  print("Starting arangosh with exception throwing script:");
  args["javascript.execute-string"] = "throw('foo')";

  const startTime = time();
  let rc = executeExternalAndWait(arangosh, toArgv(args));
  const deltaTime = time() - startTime;
  const failSuccess = (rc.hasOwnProperty('exit') && rc.exit === 1);

  if (!failSuccess) {
    ret.ArangoshExitCodeTest.testArangoshExitCodeFail['message'] = "didn't get expected return code (1): \n" +
      yaml.safeDump(rc);
    ++failed;
  }

  ret.ArangoshExitCodeTest.testArangoshExitCodeFail['status'] = failSuccess;
  ret.ArangoshExitCodeTest.testArangoshExitCodeFail['duration'] = deltaTime;
  print("Status: " + ((failSuccess) ? "SUCCESS" : "FAIL") + "\n");

  // regular termination
  print("Starting arangosh with regular terminating script:");
  args["javascript.execute-string"] = ";";

  const startTime2 = time();
  rc = executeExternalAndWait(arangosh, toArgv(args));
  const deltaTime2 = time() - startTime2;

  const successSuccess = (rc.hasOwnProperty('exit') && rc.exit === 0);

  if (!successSuccess) {
    ret.ArangoshExitCodeTest.testArangoshExitCodeFail['message'] = "didn't get expected return code (0): \n" +
      yaml.safeDump(rc);

    ++failed;
  }

  ret.ArangoshExitCodeTest.testArangoshExitCodeSuccess['status'] = failSuccess;
  ret.ArangoshExitCodeTest.testArangoshExitCodeSuccess['duration'] = deltaTime2;
  print("Status: " + ((successSuccess) ? "SUCCESS" : "FAIL") + "\n");

  // return result
  ret.ArangoshExitCodeTest.status = failSuccess && successSuccess;
  ret.ArangoshExitCodeTest.duration = deltaTime + deltaTime2;
  return ret;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: arangob
////////////////////////////////////////////////////////////////////////////////

const benchTodos = [{
  "requests": "10000",
  "concurrency": "2",
  "test": "version",
  "keep-alive": "false"
}, {
  "requests": "10000",
  "concurrency": "2",
  "test": "version",
  "async": "true"
}, {
  "requests": "20000",
  "concurrency": "1",
  "test": "version",
  "async": "true"
}, {
  "requests": "100000",
  "concurrency": "2",
  "test": "shapes",
  "batch-size": "16",
  "complexity": "2"
}, {
  "requests": "100000",
  "concurrency": "2",
  "test": "shapes-append",
  "batch-size": "16",
  "complexity": "4"
}, {
  "requests": "100000",
  "concurrency": "2",
  "test": "random-shapes",
  "batch-size": "16",
  "complexity": "2"
}, {
  "requests": "1000",
  "concurrency": "2",
  "test": "version",
  "batch-size": "16"
}, {
  "requests": "100",
  "concurrency": "1",
  "test": "version",
  "batch-size": "0"
}, {
  "requests": "100",
  "concurrency": "2",
  "test": "document",
  "batch-size": "10",
  "complexity": "1"
}, {
  "requests": "2000",
  "concurrency": "2",
  "test": "crud",
  "complexity": "1"
}, {
  "requests": "4000",
  "concurrency": "2",
  "test": "crud-append",
  "complexity": "4"
}, {
  "requests": "4000",
  "concurrency": "2",
  "test": "edge",
  "complexity": "4"
}, {
  "requests": "5000",
  "concurrency": "2",
  "test": "hash",
  "complexity": "1"
}, {
  "requests": "5000",
  "concurrency": "2",
  "test": "skiplist",
  "complexity": "1"
}, {
  "requests": "500",
  "concurrency": "3",
  "test": "aqltrx",
  "complexity": "1"
}, {
  "requests": "100",
  "concurrency": "3",
  "test": "counttrx"
}, {
  "requests": "500",
  "concurrency": "3",
  "test": "multitrx"
}];

testFuncs.arangob = function(options) {
  if (options.skipArangoB === true) {
    print("skipping Benchmark tests!");
    return {};
  }

  print("arangob tests...");

  let instanceInfo = startInstance("tcp", options, [], "arangob");

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  let results = {
    arangob: {
      status: true,
      total: 0
    }
  };

  let continueTesting = true;

  for (let i = 0; i < benchTodos.length; i++) {
    const benchTodo = benchTodos[i];

    if ((options.skipArangoBNonConnKeepAlive) &&
      benchTodo.hasOwnProperty('keep-alive') &&
      (benchTodo['keep-alive'] === "false")) {
      benchTodo['keep-alive'] = true;
    }

    // On the cluster we do not yet have working transaction functionality:
    if (!options.cluster ||
      (benchTodo.test !== "counttrx" &&
        benchTodo.test !== "multitrx")) {

      if (!continueTesting) {
        print("Skipping " + benchTodo + ", server is gone.");

        results.arangob[i] = {
          status: false,
          message: instanceInfo.exitStatus
        };

        instanceInfo.exitStatus = "server is gone.";

        break;
      }

      let args = benchTodo;

      if (options.hasOwnProperty('benchargs')) {
        args = _.extend(args, options.benchargs);
      }

      let oneResult = runArangoBenchmark(options, instanceInfo, args);

      results.arangob[i] = oneResult;
      results.arangob.total++;

      if (!results.arangob[i].status) {
        results.arangob.status = false;
      }

      continueTesting = checkInstanceAlive(instanceInfo, options);

      if (oneResult.status !== true && !options.force) {
        break;
      }
    }
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  if ((!options.skipLogAnalysis) &&
    instanceInfo.hasOwnProperty('importantLogLines') &&
    Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" +
      yaml.safeDump(instanceInfo.importantLogLines));
  }

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: authentication
////////////////////////////////////////////////////////////////////////////////

testFuncs.authentication = function(options) {
  if (options.skipAuth === true) {
    print("skipping Authentication tests!");

    return {};
  }

  print("Authentication tests...");

  let instanceInfo = startInstance("tcp", options, {
    "server.disable-authentication": "false"
  }, "authentication");

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  let results = {};

  results.auth = runInArangosh(options, instanceInfo,
    fs.join("js", "client", "tests", "auth.js"));

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  if ((!options.skipLogAnalysis) &&
    instanceInfo.hasOwnProperty('importantLogLines') &&
    Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" +
      yaml.safeDump(instanceInfo.importantLogLines));
  }

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: authentication parameters
////////////////////////////////////////////////////////////////////////////////

const authTestExpectRC = [
  [401, 401, 401, 401, 401, 401, 401],
  [401, 401, 401, 401, 401, 404, 404],
  [404, 404, 200, 301, 301, 404, 404]
];

const authTestUrls = [
  "/_api/",
  "/_api",
  "/_api/version",
  "/_admin/html",
  "/_admin/html/",
  "/test",
  "/the-big-fat-fox"
];

const authTestNames = [
  "Full",
  "SystemAuth",
  "None"
];

const authTestServerParams = [{
  "server.disable-authentication": "false",
  "server.authenticate-system-only": "false"
}, {
  "server.disable-authentication": "false",
  "server.authenticate-system-only": "true"
}, {
  "server.disable-authentication": "true",
  "server.authenticate-system-only": "true"
}];

function checkBodyForJsonToParse(request) {
  if (request.hasOwnProperty('body')) {
    request.body = JSON.parse(request.hasOwnProperty('body'));
  }
}

testFuncs.authentication_parameters = function(options) {
  if (options.skipAuth === true) {
    print("skipping Authentication with parameters tests!");
    return {};
  }

  print("Authentication with parameters tests...");

  let downloadOptions = {
    followRedirects: false,
    returnBodyOnError: true
  };

  if (typeof(options.valgrind) === 'string') {
    downloadOptions.timeout = 300;
  }

  let continueTesting = true;
  let results = {};

  for (let test = 0; test < 3; test++) {
    let instanceInfo = startInstance("tcp", options,
      authTestServerParams[test],
      "authentication_parameters_" + authTestNames[test]);

    if (instanceInfo === false) {
      return {
        authentication: {
          status: false,
          total: 1,
          failed: 1,
          message: authTestNames[test] + ": failed to start server!"
        }
      };
    }

    print(Date() + " Starting " + authTestNames[test] + " test");

    const testName = 'auth_' + authTestNames[test];
    results[testName] = {
      failed: 0,
      total: 0
    };

    for (let i = 0; i < authTestUrls.length; i++) {
      const authTestUrl = authTestUrls[i];

      ++results[testName].total;

      print("  URL: " + instanceInfo.url + authTestUrl);

      if (!continueTesting) {
        print("Skipping " + authTestUrl + ", server is gone.");

        results[testName][authTestUrl] = {
          status: false,
          message: instanceInfo.exitStatus
        };

        results[testName].failed++;
        instanceInfo.exitStatus = "server is gone.";

        break;
      }

      let reply = download(instanceInfo.url + authTestUrl, "", downloadOptions);

      if (reply.code === authTestExpectRC[test][i]) {
        results[testName][authTestUrl] = {
          status: true
        };
      } else {
        checkBodyForJsonToParse(reply);

        ++results[testName].failed;

        results[testName][authTestUrl] = {
          status: false,
          message: "we expected " +
            authTestExpectRC[test][i] +
            " and we got " + reply.code +
            " Full Status: " + yaml.safeDump(reply)
        };
      }

      continueTesting = checkInstanceAlive(instanceInfo, options);
    }

    results[testName].status = results[testName].failed === 0;

    print("Shutting down " + authTestNames[test] + " test...");
    shutdownInstance(instanceInfo, options);

    if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      (instanceInfo.importantLogLines.length > 0)) {
      print("Found messages in the server logs: \n" +
        yaml.safeDump(instanceInfo.importantLogLines));
    }

    print("done with " + authTestNames[test] + " test.");
  }

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: boost
////////////////////////////////////////////////////////////////////////////////

testFuncs.boost = function(options) {
  const topDir = findTopDir();
  let results = {};

  if (!options.skipBoost) {
    results.basics = executeAndWait(fs.join(topDir,
      "UnitTests", "basics_suite"), ["--show_progress"]);
  }

  if (!options.skipGeo) {
    results.geo_suite = executeAndWait(
      fs.join(topDir, "UnitTests", "geo_suite"), ["--show_progress"]);
  }

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: config
////////////////////////////////////////////////////////////////////////////////

testFuncs.config = function(options) {
  if (options.skipConfig) {
    return {};
  }

  let results = {
    absolut: {
      status: true,
      total: 0
    },
    relative: {
      status: true,
      total: 0
    }
  };

  const topDir = findTopDir();

  const ts = ["arangod",
    "arangob",
    "arangodump",
    "arangoimp",
    "arangorestore",
    "arangosh"
  ];

  print("--------------------------------------------------------------------------------");
  print("absolute config tests");
  print("--------------------------------------------------------------------------------");

  for (let i = 0; i < ts.length; i++) {
    const test = ts[i];

    const args = {
      "configuration": fs.join(topDir, "etc", "arangodb", test + ".conf"),
      "flatCommands": ["--help"]
    };

    results.absolut[test] = executeAndWait(fs.join(topDir, "bin", test), toArgv(args));

    if (!results.absolut[test].status) {
      results.absolut.status = false;
    }

    results.absolut.total++;

    print("Args for [" + test + "]:");
    print(yaml.safeDump(args));
    print("Result: " + results.absolut[test].status);
  }

  print("--------------------------------------------------------------------------------");
  print("relative config tests");
  print("--------------------------------------------------------------------------------");

  for (let i = 0; i < ts.length; i++) {
    const test = ts[i];

    const args = {
      "configuration": fs.join(topDir, "etc", "relative", test + ".conf"),
      "flatCommands": ["--help"]
    };

    results.relative[test] = executeAndWait(fs.join(topDir, "bin", test),
      toArgv(args));

    if (!results.relative[test].status) {
      results.relative.status = false;
    }

    results.relative.total++;

    print("Args for (relative) [" + test + "]:");
    print(yaml.safeDump(args));
    print("Result: " + results.relative[test].status);
  }

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: dump
////////////////////////////////////////////////////////////////////////////////

testFuncs.dump = function(options) {
  let cluster;

  if (options.cluster) {
    cluster = "-cluster";
  } else {
    cluster = "";
  }

  print("dump tests...");

  let instanceInfo = startInstance("tcp", options, [], "dump");

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  print(Date() + ": Setting up");

  let results = {};
  results.setup = runInArangosh(options, instanceInfo,
    makePathUnix("js/server/tests/dump/dump-setup" + cluster + ".js"));

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
        print(Date() + ": Dump and Restore - dump after restore");

        results.test = runInArangosh(options, instanceInfo,
          makePathUnix("js/server/tests/dump/dump" + cluster + ".js"), {
            "server.database": "UnitTestsDumpDst"
          });

        if (checkInstanceAlive(instanceInfo, options)) {
          print(Date() + ": Dump and Restore - teardown");

          results.tearDown = runInArangosh(options, instanceInfo,
            makePathUnix("js/server/tests/dump/dump-teardown" + cluster + ".js"));
        }
      }
    }
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  if ((!options.skipLogAnalysis) &&
    instanceInfo.hasOwnProperty('importantLogLines') &&
    Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" +
      yaml.safeDump(instanceInfo.importantLogLines));
  }

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: foxx manager
////////////////////////////////////////////////////////////////////////////////

testFuncs.foxx_manager = function(options) {
  print("foxx_manager tests...");

  let instanceInfo = startInstance("tcp", options, [], "foxx_manager");

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  let results = {};

  results.update = runArangoshCmd(options, instanceInfo, {
    "configuration": "etc/relative/foxx-manager.conf"
  }, ["update"]);

  if (results.update.status === true || options.force) {
    results.search = runArangoshCmd(options, instanceInfo, {
      "configuration": "etc/relative/foxx-manager.conf"
    }, ["search", "itzpapalotl"]);
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  if ((!options.skipLogAnalysis) &&
    instanceInfo.hasOwnProperty('importantLogLines') &&
    Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" +
      yaml.safeDump(instanceInfo.importantLogLines));
  }

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: http server
////////////////////////////////////////////////////////////////////////////////

testFuncs.http_server = function(options) {
  return rubyTests(options, false);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: importing
////////////////////////////////////////////////////////////////////////////////

const impTodos = [{
  id: "json1",
  data: makePathUnix("js/common/test-data/import/import-1.json"),
  coll: "UnitTestsImportJson1",
  type: "json",
  create: undefined
}, {
  id: "json2",
  data: makePathUnix("js/common/test-data/import/import-2.json"),
  coll: "UnitTestsImportJson2",
  type: "json",
  create: undefined
}, {
  id: "json3",
  data: makePathUnix("js/common/test-data/import/import-3.json"),
  coll: "UnitTestsImportJson3",
  type: "json",
  create: undefined
}, {
  id: "json4",
  data: makePathUnix("js/common/test-data/import/import-4.json"),
  coll: "UnitTestsImportJson4",
  type: "json",
  create: undefined
}, {
  id: "json5",
  data: makePathUnix("js/common/test-data/import/import-5.json"),
  coll: "UnitTestsImportJson5",
  type: "json",
  create: undefined
}, {
  id: "csv1",
  data: makePathUnix("js/common/test-data/import/import-1.csv"),
  coll: "UnitTestsImportCsv1",
  type: "csv",
  create: "true"
}, {
  id: "csv2",
  data: makePathUnix("js/common/test-data/import/import-2.csv"),
  coll: "UnitTestsImportCsv2",
  type: "csv",
  create: "true"
}, {
  id: "csv3",
  data: makePathUnix("js/common/test-data/import/import-3.csv"),
  coll: "UnitTestsImportCsv3",
  type: "csv",
  create: "true"
}, {
  id: "csv4",
  data: makePathUnix("js/common/test-data/import/import-4.csv"),
  coll: "UnitTestsImportCsv4",
  type: "csv",
  create: "true",
  separator: ";",
  backslash: true
}, {
  id: "csv5",
  data: makePathUnix("js/common/test-data/import/import-5.csv"),
  coll: "UnitTestsImportCsv5",
  type: "csv",
  create: "true",
  separator: ";",
  backslash: true
}, {
  id: "tsv1",
  data: makePathUnix("js/common/test-data/import/import-1.tsv"),
  coll: "UnitTestsImportTsv1",
  type: "tsv",
  create: "true"
}, {
  id: "tsv2",
  data: makePathUnix("js/common/test-data/import/import-2.tsv"),
  coll: "UnitTestsImportTsv2",
  type: "tsv",
  create: "true"
}, {
  id: "edge",
  data: makePathUnix("js/common/test-data/import/import-edges.json"),
  coll: "UnitTestsImportEdge",
  type: "json",
  create: "false"
}];

testFuncs.importing = function(options) {
  if (options.cluster) {
    if (options.extremeVerbosity) {
      print("Skipped because of cluster.");
    }

    return {
      "importing": {
        "status": true,
        "message": "skipped because of cluster",
        "skipped": true
      }
    };
  }

  let instanceInfo = startInstance("tcp", options, [], "importing");

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  let result = {};

  try {
    result.setup = runInArangosh(options, instanceInfo,
      makePathUnix("js/server/tests/import/import-setup.js"));

    if (result.setup.status !== true) {
      throw new Error("cannot start import setup");
    }

    for (let i = 0; i < impTodos.length; i++) {
      const impTodo = impTodos[i];

      result[impTodo.id] = runArangoImp(options, instanceInfo, impTodo);

      if (result[impTodo.id].status !== true && !options.force) {
        throw new Error("cannot run import");
      }
    }

    result.check = runInArangosh(options, instanceInfo,
      makePathUnix("js/server/tests/import/import.js"));

    result.teardown = runInArangosh(options, instanceInfo,
      makePathUnix("js/server/tests/import/import-teardown.js"));

  } catch (banana) {
    print("An exceptions of the following form was caught:",
      yaml.safeDump(banana));
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  if ((!options.skipLogAnalysis) &&
    instanceInfo.hasOwnProperty('importantLogLines') &&
    Object.keys(instanceInfo.importantLogLines).length > 0) {

    print("Found messages in the server logs: \n" +
      yaml.safeDump(instanceInfo.importantLogLines));
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell client
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_client = function(options) {
  findTests();

  let instanceInfo = startInstance("tcp", options, [], "shell_client");

  if (instanceInfo === false) {
    return {
      status: false,
      message: "failed to start server!"
    };
  }

  let results = {};
  let filtered = {};
  let continueTesting = true;

  for (let i = 0; i < testsCases.client.length; i++) {
    const te = testsCases.client[i];

    if (filterTestcaseByOptions(te, options, filtered)) {
      if (!continueTesting) {
        print("Skipping, " + te + " server is gone.");

        results[te] = {
          status: false,
          message: instanceInfo.exitStatus
        };

        instanceInfo.exitStatus = "server is gone.";

        break;
      }

      print("\narangosh: Trying", te, "...");

      const reply = runInArangosh(options, instanceInfo, te);
      results[te] = reply;

      if (reply.status !== true) {
        options.cleanup = false;

        if (!options.force) {
          break;
        }
      }

      continueTesting = checkInstanceAlive(instanceInfo, options);
    } else {
      if (options.extremeVerbosity) {
        print("Skipped " + te + " because of " + filtered.filter);
      }
    }
  }

  print("Shutting down...");
  shutdownInstance(instanceInfo, options);
  print("done.");

  if ((!options.skipLogAnalysis) &&
    instanceInfo.hasOwnProperty('importantLogLines') &&
    Object.keys(instanceInfo.importantLogLines).length > 0) {
    print("Found messages in the server logs: \n" +
      yaml.safeDump(instanceInfo.importantLogLines));
  }

  return results;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell server
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server = function(options) {
  findTests();

  return performTests(options, testsCases.server, 'shell_server');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell server aql
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server_aql = function(options) {
  findTests();

  if (!options.skipAql) {
    if (options.skipRanges) {
      return performTests(options, testsCases.server_aql,
        'shell_server_aql_skipranges');
    } else {
      return performTests(options,
        testsCases.server_aql.concat(testsCases.server_aql_extended),
        'shell_server_aql');
    }
  }

  return "skipped";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell server only
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server_only = function(options) {
  findTests();

  return performTests(options, testsCases.server_only, 'shell_server_only');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: shell server performance
////////////////////////////////////////////////////////////////////////////////

testFuncs.shell_server_perf = function(options) {
  findTests();

  return performTests(options, testsCases.server_aql_performance,
    'shell_server_perf');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: single client
////////////////////////////////////////////////////////////////////////////////

function single_usage(testsuite, list) {
  print("single_" + testsuite + ": No test specified!\n Available tests:");
  let filelist = "";

  for (let fileNo in list) {
    if (/\.js$/.test(list[fileNo])) {
      filelist += " " + list[fileNo];
    }
  }

  print(filelist);
  print("usage: single_" + testsuite + " '{\"test\":\"<testfilename>\"}'");
  print(" where <testfilename> is one from the list above.");

  return {
    status: false,
    message: "No test specified!"
  };
}

testFuncs.single_client = function(options) {
  options.writeXmlReport = false;

  if (options.test !== undefined) {
    let instanceInfo = startInstance("tcp", options, [], "single_client");

    if (instanceInfo === false) {
      return {
        status: false,
        message: "failed to start server!"
      };
    }

    const te = options.test;

    print("\n" + Date() + " arangosh: Trying ", te, "...");

    let result = {};
    result[te] = runInArangosh(options, instanceInfo, te);

    print("Shutting down...");

    if (result[te].status === false) {
      options.cleanup = false;
    }

    shutdownInstance(instanceInfo, options);

    print("done.");

    if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
      print("Found messages in the server logs: \n" +
        yaml.safeDump(instanceInfo.importantLogLines));
    }

    return result;
  } else {
    findTests();
    return single_usage("client", testsCases.client);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: single server
////////////////////////////////////////////////////////////////////////////////

testFuncs.single_server = function(options) {
  options.writeXmlReport = false;

  if (options.test !== undefined) {
    let instanceInfo = startInstance("tcp", options, [], "single_server");

    if (instanceInfo === false) {
      return {
        status: false,
        message: "failed to start server!"
      };
    }

    const te = options.test;

    print("\n" + Date() + " arangod: Trying", te, "...");

    let result = {};
    let reply = runThere(options, instanceInfo, makePathGeneric(te));

    if (reply.hasOwnProperty('status')) {
      result[te] = reply;

      if (result[te].status === false) {
        options.cleanup = false;
      }
    }

    print("Shutting down...");

    if (result[te].status === false) {
      options.cleanup = false;
    }

    shutdownInstance(instanceInfo, options);

    print("done.");

    if ((!options.skipLogAnalysis) &&
      instanceInfo.hasOwnProperty('importantLogLines') &&
      Object.keys(instanceInfo.importantLogLines).length > 0) {
      print("Found messages in the server logs: \n" +
        yaml.safeDump(instanceInfo.importantLogLines));
    }

    return result;
  } else {
    findTests();
    return single_usage("server", testsCases.server);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: https server
////////////////////////////////////////////////////////////////////////////////

testFuncs.ssl_server = function(options) {
  if (options.hasOwnProperty('skipSsl')) {
    return {};
  }

  return rubyTests(options, true);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: upgrade
////////////////////////////////////////////////////////////////////////////////

testFuncs.upgrade = function(options) {
  if (options.cluster) {
    return {
      "upgrade": {
        "status": true,
        "message": "skipped because of cluster",
        "skipped": true
      }
    };
  }

  let result = {
    upgrade: {
      status: true,
      total: 1
    }
  };

  const tmpDataDir = fs.getTempFile();
  fs.makeDirectoryRecursive(tmpDataDir);

  const appDir = fs.join(tmpDataDir, "app");
  const port = findFreePort();

  let args = makeArgsArangod(options, appDir);
  args["server.endpoint"] = "tcp://127.0.0.1:" + port;
  args["database.directory"] = fs.join(tmpDataDir, "data");

  fs.makeDirectoryRecursive(fs.join(tmpDataDir, "data"));

  const argv = toArgv(args).concat(["--upgrade"]);

  result.upgrade.first = executeAndWait(fs.join("bin", "arangod"), argv);

  if (result.upgrade.first !== 0 && !options.force) {
    print("not removing " + tmpDataDir);
    return result;
  }

  ++result.upgrade.total;
  result.upgrade.second = executeAndWait(fs.join("bin", "arangod"), argv);

  cleanupDirectories.push(tmpDataDir);

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief STRESS TEST: stress_crud
////////////////////////////////////////////////////////////////////////////////

testFuncs.stress_crud = function(options) {
  const duration = options.duration;
  const concurrency = options.concurrency;

  const command = `
    const stressCrud = require("./js/server/tests/stress/crud");

    stressCrud.createDeleteUpdateParallel({
      concurrency: ${concurrency},
      duration: ${duration},
      gnuplot: true,
      pauseFor: 60
    });
`;

  return runStressTest(options, command, "stress_crud");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief STRESS TEST: stress_locks
////////////////////////////////////////////////////////////////////////////////


testFuncs.stress_locks = function(options) {
  const duration = options.duration;
  const concurrency = options.concurrency;

  const command = `
    const deadlock = require("./js/server/tests/stress/deadlock");

    deadlock.lockCycleParallel({
      concurrency: ${concurrency},
      duration: ${duration},
      gnuplot: true
    });
`;

  return runStressTest(options, command, "stress_lock");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief pretty prints the result
////////////////////////////////////////////////////////////////////////////////

function unitTestPrettyPrintResults(r) {
  let testFail = 0;
  let testSuiteFail = 0;
  let success = "";
  let fail = "";

  try {
    for (let testrun in r) {
      if (r.hasOwnProperty(testrun) && (internalMembers.indexOf(testrun) === -1)) {
        let isSuccess = true;
        let successTests = {};
        let oneOutput = "* Testrun: " + testrun + "\n";

        for (let test in r[testrun]) {
          if (r[testrun].hasOwnProperty(test) &&
            (internalMembers.indexOf(test) === -1)) {
            if (r[testrun][test].status) {
              const where = test.lastIndexOf(fs.pathSeparator);
              let which;

              if (where < 0) {
                which = 'Unittests';
              } else {
                which = test.substring(0, where);
                test = test.substring(where + 1, test.length);
              }

              if (!successTests.hasOwnProperty(which)) {
                successTests[which] = [];
              }

              successTests[which].push(test);
            } else {
              testSuiteFail++;

              if (r[testrun][test].hasOwnProperty('message')) {
                isSuccess = false;
                oneOutput += "     [  Fail ] " + test +
                  ": Whole testsuite failed!\n";

                if (typeof r[testrun][test].message === "object" &&
                  r[testrun][test].message.hasOwnProperty('body')) {
                  oneOutput += r[testrun][test].message.body + "\n";
                } else {
                  oneOutput += r[testrun][test].message + "\n";
                }
              } else {
                isSuccess = false;
                oneOutput += "    [  Fail ] " + test + "\n";

                for (let oneTest in r[testrun][test]) {
                  if (r[testrun][test].hasOwnProperty(oneTest) &&
                    (internalMembers.indexOf(oneTest) === -1) &&
                    (!r[testrun][test][oneTest].status)) {
                    ++testFail;
                    oneOutput += "          -> " + oneTest +
                      " Failed; Verbose message:\n" +
                      r[testrun][test][oneTest].message + "\n";
                  }
                }
              }
            }
          }
        }

        if (successTests !== "") {
          for (let key in successTests) {
            if (successTests.hasOwnProperty(key)) {
              oneOutput = "     [Success] " + key +
                " / [" + successTests[key].join(', ') + ']\n' + oneOutput;
            }
          }
        }

        if (isSuccess) {
          success += oneOutput;
        } else {
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
  } catch (x) {
    print("exception caught while pretty printing result: ");
    print(x.message);
    print(JSON.stringify(r));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print usage information
////////////////////////////////////////////////////////////////////////////////

function printUsage() {
  print();
  print("Usage: UnitTest( which, options )");
  print();
  print('       where "which" is one of:\n');

  for (let i in testFuncs) {
    if (testFuncs.hasOwnProperty(i)) {
      let oneFunctionDocumentation;

      if (functionsDocumentation.hasOwnProperty(i)) {
        oneFunctionDocumentation = ' - ' + functionsDocumentation[i];
      } else {
        oneFunctionDocumentation = '';
      }

      let checkAll;

      if (allTests.indexOf(i) !== -1) {
        checkAll = '[x]';
      } else {
        checkAll = '   ';
      }

      print('    ' + checkAll + ' ' + i + ' ' + oneFunctionDocumentation);
    }
  }

  for (let i in optionsDocumentation) {
    if (optionsDocumentation.hasOwnProperty(i)) {
      print(optionsDocumentation[i]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief framework to perform unittests
///
/// This function gets one or two arguments, the first describes which tests
/// to perform and the second is an options object. For `which` the following
/// values are allowed:
///  Empty will give you a complete list.
////////////////////////////////////////////////////////////////////////////////

function unitTest(which, options) {
  if (typeof options !== "object") {
    options = {};
  }

  _.defaults(options, optionsDefaults);

  if (which === undefined) {
    printUsage();

    print('FATAL: "which" is undefined\n');

    return {
      ok: false,
      all_ok: false
    };
  }

  const jsonReply = options.jsonReply;
  delete options.jsonReply;

  let allok = true;
  let ok = false;

  let results = {};
  let thisReply;

  // running all tests
  if (which === "all") {
    for (let n = 0; n < allTests.length; n++) {
      print("Executing test", allTests[n], "with options", options);

      results[allTests[n]] = thisReply = testFuncs[allTests[n]](options);
      ok = true;

      for (let i in thisReply) {
        if (thisReply.hasOwnProperty(i)) {
          if (thisReply[i].status !== true) {
            ok = false;
          }
        }
      }

      thisReply.ok = ok;

      if (!ok) {
        allok = false;
      }

      results.all_ok = allok;
    }

    results.all_ok = allok;
    results.crashed = serverCrashed;

    if (allok) {
      cleanupDBDirectories(options);
    } else {
      print("since some tests weren't successfully, not cleaning up: \n" +
        yaml.safeDump(cleanupDirectories));
    }

    if (jsonReply === true) {
      return results;
    } else {
      unitTestPrettyPrintResults(results);
      return allok;
    }
  }

  // unknown test
  else if (!testFuncs.hasOwnProperty(which)) {
    let line = "Unknown test '" + which + "'\nKnown tests are: ";
    let sep = "";

    Object.keys(testFuncs).map(function(key) {
      line += sep + key;
      sep = ", ";
    });

    print(line);
    return {
      ok: false,
      all_ok: false
    };
  }

  // test found in testFuncs
  else {
    results[which] = thisReply = testFuncs[which](options);

    if (options.extremeVerbosity) {
      print("Test result:", yaml.safeDump(results));
    }

    ok = true;

    for (let i in thisReply) {
      if (thisReply.hasOwnProperty(i) &&
        (which !== "single" || i !== "test")) {

        if (thisReply[i].status !== true) {
          ok = false;
          allok = false;
        }
      }
    }

    thisReply.ok = ok;
    results.all_ok = ok;
    results.crashed = serverCrashed;

    if (allok) {
      cleanupDBDirectories(options);
    } else {
      print("since some tests weren't successfully, not cleaning up: \n" +
        yaml.safeDump(cleanupDirectories));
    }

    if (jsonReply === true) {
      return results;
    } else {
      unitTestPrettyPrintResults(results);
      return allok;
    }
  }

  return {
    ok: false,
    all_ok: false
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exports
////////////////////////////////////////////////////////////////////////////////

exports.UnitTest = unitTest;

exports.internalMembers = internalMembers;
exports.testFuncs = testFuncs;
exports.unitTestPrettyPrintResults = unitTestPrettyPrintResults;

// TODO write test for 2.6-style queues
// testFuncs.queue_legacy = function (options) {
//   if (options.skipFoxxQueues) {
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
