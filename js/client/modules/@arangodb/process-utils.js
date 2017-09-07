/* jshint strict: false, sub: true */
/* global print, arango */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2014 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const yaml = require('js-yaml');
const toArgv = require('internal').toArgv;
const crashUtils = require('@arangodb/crash-utils');
const crypto = require('@arangodb/crypto');

/* Functions: */
const executeExternal = require('internal').executeExternal;
const executeExternalAndWait = require('internal').executeExternalAndWait;
const killExternal = require('internal').killExternal;
const statusExternal = require('internal').statusExternal;
const base64Encode = require('internal').base64Encode;
const testPort = require('internal').testPort;
const download = require('internal').download;
const time = require('internal').time;
const wait = require('internal').wait;

/* Constants: */
// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const platform = require('internal').platform;

const abortSignal = 6;

let executableExt = '';
if (platform.substr(0, 3) === 'win') {
  executableExt = '.exe';
}

let serverCrashed = false;
let cleanupDirectories = [];

let BIN_DIR;
let ARANGOBENCH_BIN;
let ARANGODUMP_BIN;
let ARANGOD_BIN;
let ARANGOIMP_BIN;
let ARANGORESTORE_BIN;
let ARANGOEXPORT_BIN;
let ARANGOSH_BIN;
let CONFIG_ARANGODB_DIR;
let CONFIG_RELATIVE_DIR;
let CONFIG_DIR;
let JS_DIR;
let JS_ENTERPRISE_DIR;
let LOGS_DIR;
let UNITTESTS_DIR;

const TOP_DIR = (function findTopDir () {
  const topDir = fs.normalize(fs.makeAbsolute('.'));

  if (!fs.exists('3rdParty') && !fs.exists('arangod') &&
    !fs.exists('arangosh') && !fs.exists('UnitTests')) {
    throw new Error('Must be in ArangoDB topdir to execute unit tests.');
  }

  return topDir;
}());

// //////////////////////////////////////////////////////////////////////////////
// / @brief calculates all the path locations
// / required to be called first.
// //////////////////////////////////////////////////////////////////////////////

function setupBinaries (builddir, buildType, configDir) {
  if (builddir === '') {
    if (fs.exists('build') && fs.exists(fs.join('build', 'bin'))) {
      builddir = 'build';
    } else if (fs.exists('bin')) {
      builddir = '.';
    } else {
      print('FATAL: cannot find binaries, use "--build"\n');

      return {
        status: false
      };
    }
  }

  BIN_DIR = fs.join(builddir, 'bin');
  if (!fs.exists(BIN_DIR)) {
    BIN_DIR = fs.join(TOP_DIR, BIN_DIR);
  }

  UNITTESTS_DIR = fs.join(TOP_DIR, fs.join(builddir, 'tests'));

  if (buildType !== '') {
    BIN_DIR = fs.join(BIN_DIR, buildType);
    UNITTESTS_DIR = fs.join(UNITTESTS_DIR, buildType);
  }

  ARANGOBENCH_BIN = fs.join(BIN_DIR, 'arangobench' + executableExt);
  ARANGODUMP_BIN = fs.join(BIN_DIR, 'arangodump' + executableExt);
  ARANGOD_BIN = fs.join(BIN_DIR, 'arangod' + executableExt);
  ARANGOIMP_BIN = fs.join(BIN_DIR, 'arangoimp' + executableExt);
  ARANGORESTORE_BIN = fs.join(BIN_DIR, 'arangorestore' + executableExt);
  ARANGOEXPORT_BIN = fs.join(BIN_DIR, 'arangoexport' + executableExt);
  ARANGOSH_BIN = fs.join(BIN_DIR, 'arangosh' + executableExt);

  CONFIG_ARANGODB_DIR = fs.join(builddir, 'etc', 'arangodb3');
  if (!fs.exists(CONFIG_ARANGODB_DIR)) {
    CONFIG_ARANGODB_DIR = fs.join(TOP_DIR, CONFIG_ARANGODB_DIR);
  }

  CONFIG_RELATIVE_DIR = fs.join(TOP_DIR, 'etc', 'relative');
  CONFIG_DIR = fs.join(TOP_DIR, configDir);

  JS_DIR = fs.join(TOP_DIR, 'js');
  JS_ENTERPRISE_DIR = fs.join(TOP_DIR, 'enterprise/js');

  LOGS_DIR = fs.join(TOP_DIR, 'logs');

  let checkFiles = [
    ARANGOBENCH_BIN,
    ARANGODUMP_BIN,
    ARANGOD_BIN,
    ARANGOIMP_BIN,
    ARANGORESTORE_BIN,
    ARANGOEXPORT_BIN,
    ARANGOSH_BIN];
  for (let b = 0; b < checkFiles.length; ++b) {
    if (!fs.isFile(checkFiles[b])) {
      throw new Error('unable to locate ' + checkFiles[b]);
    }
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief finds a free port
// //////////////////////////////////////////////////////////////////////////////

function findFreePort (minPort, maxPort, usedPorts) {
  if (typeof maxPort !== 'number') {
    maxPort = 32768;
  }

  if (maxPort - minPort < 0) {
    throw new Error('minPort ' + minPort + ' is smaller than maxPort ' + maxPort);
  }

  let tries = 0;
  while (true) {
    const port = Math.floor(Math.random() * (maxPort - minPort)) + minPort;
    tries++;
    if (tries > 20) {
      throw new Error('Couldn\'t find a port after ' + tries + ' tries. portrange of ' + minPort + ', ' + maxPort + ' too narrow?');
    }
    if (Array.isArray(usedPorts) && usedPorts.indexOf(port) >= 0) {
      continue;
    }
    const free = testPort('tcp://0.0.0.0:' + port);

    if (free) {
      return port;
    }

    require('internal').wait(0.1);
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief scans the log files for important infos
// //////////////////////////////////////////////////////////////////////////////

function readImportantLogLines (logPath) {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief cleans up the database direcory
// //////////////////////////////////////////////////////////////////////////////

function cleanupLastDirectory (options) {
  if (options.cleanup) {
    while (cleanupDirectories.length) {
      const cleanupDirectory = cleanupDirectories.shift();
      // Avoid attempting to remove the same directory multiple times
      if ((cleanupDirectories.indexOf(cleanupDirectory) === -1) &&
          (fs.exists(cleanupDirectory))) {
        fs.removeDirectoryRecursive(cleanupDirectory, true);
      }
      break;
    }
  }
}

function cleanupDBDirectories (options) {
  if (options.cleanup) {
    while (cleanupDirectories.length) {
      const cleanupDirectory = cleanupDirectories.shift();

      // Avoid attempting to remove the same directory multiple times
      if ((cleanupDirectories.indexOf(cleanupDirectory) === -1) &&
          (fs.exists(cleanupDirectory))) {
        fs.removeDirectoryRecursive(cleanupDirectory, true);
      }
    }
  }
}

function cleanupDBDirectoriesAppend (appendThis) {
  cleanupDirectories.unshift(appendThis);
}

function getCleanupDBDirectories () {
  return JSON.stringify(cleanupDirectories);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief adds authorization headers
// //////////////////////////////////////////////////////////////////////////////

function makeAuthorizationHeaders (options) {
  if (options['server.jwt-secret']) {
    var jwt = crypto.jwtEncode(options['server.jwt-secret'],
                             {'server_id': 'none',
                              'iss': 'arangodb'}, 'HS256');
    if (options.extremeVerbosity) {
      print('Using jwt token:     ' + jwt);
    }
    return {
      'headers': {
        'Authorization': 'bearer ' + jwt
      }
    };
  } else {
    return {
      'headers': {
        'Authorization': 'Basic ' + base64Encode(options.username + ':' +
            options.password)
      }
    };
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief converts endpoints to URL
// //////////////////////////////////////////////////////////////////////////////

function endpointToURL (endpoint) {
  if (endpoint.substr(0, 6) === 'ssl://') {
    return 'https://' + endpoint.substr(6);
  }

  const pos = endpoint.indexOf('://');

  if (pos === -1) {
    return 'http://' + endpoint;
  }

  return 'http' + endpoint.substr(pos);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief arguments for testing (server)
// //////////////////////////////////////////////////////////////////////////////

function makeArgsArangod (options, appDir, role, tmpDir) {
  console.assert(tmpDir !== undefined);
  if (appDir === undefined) {
    appDir = fs.getTempPath();
  }

  fs.makeDirectoryRecursive(appDir, true);

  fs.makeDirectoryRecursive(tmpDir, true);

  let config = 'arangod.conf';

  if (role !== undefined && role !== null && role !== '') {
    config = 'arangod-' + role + '.conf';
  }

  let args = {
    'configuration': fs.join(CONFIG_DIR, config),
    'define': 'TOP_DIR=' + TOP_DIR,
    'wal.flush-timeout': options.walFlushTimeout,
    'javascript.app-path': appDir,
    'http.trusted-origin': options.httpTrustedOrigin || 'all',
    'cluster.create-waits-for-sync-replication': false,
    'temp.path': tmpDir
  };
  if (options.storageEngine !== undefined) {
    args['server.storage-engine'] = options.storageEngine;
  }
  return args;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a command and waits for result
// //////////////////////////////////////////////////////////////////////////////

function executeAndWait (cmd, args, options, valgrindTest, rootDir, disableCoreCheck = false) {
  if (valgrindTest && options.valgrind) {
    let valgrindOpts = {};

    if (options.valgrindArgs) {
      valgrindOpts = options.valgrindArgs;
    }

    let testfn = options.valgrindFileBase;

    if (testfn.length > 0) {
      testfn += '_';
    }

    testfn += valgrindTest;

    if (valgrindOpts.xml === 'yes') {
      valgrindOpts['xml-file'] = testfn + '.%p.xml';
    }

    valgrindOpts['log-file'] = testfn + '.%p.valgrind.log';

    args = toArgv(valgrindOpts, true).concat([cmd]).concat(args);
    cmd = options.valgrind;
  }

  if (options.extremeVerbosity) {
    print('executeAndWait: cmd =', cmd, 'args =', args);
  }

  const startTime = time();
  if ((typeof (cmd) !== 'string') || (cmd === 'true') || (cmd === 'false')) {
    return {
      status: false,
      message: 'true or false as binary name for test cmd =' + cmd + 'args =' + args
    };
  }

  const res = executeExternalAndWait(cmd, args);
  const deltaTime = time() - startTime;

  let errorMessage = ' - ';

  if (!disableCoreCheck &&
      res.hasOwnProperty('signal') &&
      ((res.signal === 11) ||
       (res.signal === 6) ||
       // Windows sometimes has random numbers in signal...
       (platform.substr(0, 3) === 'win')
      )
     ) {
    print(res);
    let instanceInfo = {
      rootDir: rootDir,
      pid: res.pid,
      exitStatus: res
    };
    crashUtils.analyzeCrash(cmd, instanceInfo, {exitStatus: {}}, 'execution of ' + cmd + ' - ' + res.signal);
    serverCrashed = true;
  }

  if (res.status === 'TERMINATED') {
    const color = (res.exit === 0 ? GREEN : RED);

    print(color + 'Finished: ' + res.status +
      ' exit code: ' + res.exit +
      ' Time elapsed: ' + deltaTime + RESET);

    if (res.exit === 0) {
      return {
        status: true,
        message: '',
        duration: deltaTime
      };
    } else {
      return {
        status: false,
        message: 'exit code was ' + res.exit,
        duration: deltaTime
      };
    }
  } else if (res.status === 'ABORTED') {
    if (typeof (res.errorMessage) !== 'undefined') {
      errorMessage += res.errorMessage;
    }

    print('Finished: ' + res.status +
      ' Signal: ' + res.signal +
      ' Time elapsed: ' + deltaTime + errorMessage);

    return {
      status: false,
      message: 'irregular termination: ' + res.status +
        ' exit signal: ' + res.signal + errorMessage,
      duration: deltaTime
    };
  } else {
    if (typeof (res.errorMessage) !== 'undefined') {
      errorMessage += res.errorMessage;
    }

    print('Finished: ' + res.status +
      ' exit code: ' + res.signal +
      ' Time elapsed: ' + deltaTime + errorMessage);

    return {
      status: false,
      message: 'irregular termination: ' + res.status +
        ' exit code: ' + res.exit + errorMessage,
      duration: deltaTime
    };
  }
}

// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / operate the arango commandline utilities
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief arguments for testing (client)
// //////////////////////////////////////////////////////////////////////////////

function makeArgsArangosh (options) {
  return {
    'configuration': fs.join(CONFIG_DIR, 'arangosh.conf'),
    'javascript.startup-directory': JS_DIR,
    'javascript.module-directory': JS_ENTERPRISE_DIR,
    'server.username': options.username,
    'server.password': options.password,
    'flatCommands': ['--console.colors', 'false', '--quiet']
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangosh
// //////////////////////////////////////////////////////////////////////////////

function runArangoshCmd (options, instanceInfo, addArgs, cmds) {
  let args = makeArgsArangosh(options);
  args['server.endpoint'] = instanceInfo.endpoint;

  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }

  require('internal').env.INSTANCEINFO = JSON.stringify(instanceInfo);
  const argv = toArgv(args).concat(cmds);
  return executeAndWait(ARANGOSH_BIN, argv, options, 'arangoshcmd', instanceInfo.rootDir);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangoimp
// //////////////////////////////////////////////////////////////////////////////

function runArangoImp (options, instanceInfo, what) {
  let args = {
    'server.username': options.username,
    'server.password': options.password,
    'server.endpoint': instanceInfo.endpoint,
    'file': fs.join(TOP_DIR, what.data),
    'collection': what.coll,
    'type': what.type,
    'on-duplicate': what.onDuplicate || 'error'
  };

  if (what.skipLines !== undefined) {
    args['skip-lines'] = what.skipLines;
  }

  if (what.create !== undefined) {
    args['create-collection'] = what.create;
  }

  if (what.backslash !== undefined) {
    args['backslash-escape'] = what.backslash;
  }

  if (what.separator !== undefined) {
    args['separator'] = what.separator;
  }

  if (what.convert !== undefined) {
    args['convert'] = what.convert ? 'true' : 'false';
  }
  if (what.removeAttribute !== undefined) {
    args['remove-attribute'] = what.removeAttribute;
  }

  return executeAndWait(ARANGOIMP_BIN, toArgv(args), options, 'arangoimp', instanceInfo.rootDir);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangodump or arangorestore
// //////////////////////////////////////////////////////////////////////////////

function runArangoDumpRestore (options, instanceInfo, which, database, rootDir, dumpDir = 'dump', includeSystem = true) {
  let args = {
    'configuration': fs.join(CONFIG_DIR, (which === 'dump' ? 'arangodump.conf' : 'arangorestore.conf')),
    'server.username': options.username,
    'server.password': options.password,
    'server.endpoint': instanceInfo.endpoint,
    'server.database': database,
    'include-system-collections': includeSystem ? 'true' : 'false'
  };

  let exe;
  rootDir = rootDir || instanceInfo.rootDir;

  if (which === 'dump') {
    args['output-directory'] = fs.join(rootDir, dumpDir);
    exe = ARANGODUMP_BIN;
  } else {
    args['create-database'] = 'true';
    args['input-directory'] = fs.join(rootDir, dumpDir);
    exe = ARANGORESTORE_BIN;
  }

  if (options.extremeVerbosity === true) {
    print(exe);
    print(args);
  }

  return executeAndWait(exe, toArgv(args), options, 'arangorestore', rootDir);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangobench
// //////////////////////////////////////////////////////////////////////////////

function runArangoBenchmark (options, instanceInfo, cmds, rootDir) {
  let args = {
    'configuration': fs.join(CONFIG_DIR, 'arangobench.conf'),
    'server.username': options.username,
    'server.password': options.password,
    'server.endpoint': instanceInfo.endpoint,
    // 'server.request-timeout': 1200 // default now.
    'server.connection-timeout': 10 // 5s default
  };

  args = Object.assign(args, cmds);

  if (!args.hasOwnProperty('verbose')) {
    args['log.level'] = 'warning';
    args['flatCommands'] = ['--quiet'];
  }

  return executeAndWait(ARANGOBENCH_BIN, toArgv(args), options, 'arangobench', instanceInfo.rootDir);
}

// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / Server up/down utilities
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief the bad has happened, tell it the user and try to gather more
// /        information about the incident. (arangod wrapper for the crash-utils)
// //////////////////////////////////////////////////////////////////////////////
function analyzeServerCrash (arangod, options, checkStr) {
  return crashUtils.analyzeCrash(ARANGOD_BIN, arangod, options, checkStr);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief periodic checks whether spawned arangod processes are still alive
// //////////////////////////////////////////////////////////////////////////////
function checkArangoAlive (arangod, options) {
  const res = statusExternal(arangod.pid, false);
  const ret = res.status === 'RUNNING';

  if (!ret) {
    print('ArangoD with PID ' + arangod.pid + ' gone:');
    print(arangod);

    if (res.hasOwnProperty('signal') &&
      ((res.signal === 11) ||
      (res.signal === 6) ||
      // Windows sometimes has random numbers in signal...
      (platform.substr(0, 3) === 'win')
      )
    ) {
      arangod.exitStatus = res;
      analyzeServerCrash(arangod, options, 'health Check  - ' + res.signal);
      serverCrashed = true;
    }
  }

  return ret;
}

function checkInstanceAlive (instanceInfo, options) {
  return instanceInfo.arangods.reduce((previous, arangod) => {
    return previous && checkArangoAlive(arangod, options);
  }, true);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief waits for garbage collection using /_admin/execute
// //////////////////////////////////////////////////////////////////////////////

function waitOnServerForGC (instanceInfo, options, waitTime) {
  try {
    print('waiting ' + waitTime + ' for server GC');
    const remoteCommand = 'require("internal").wait(' + waitTime + ', true);';

    const requestOptions = makeAuthorizationHeaders(options);
    requestOptions.method = 'POST';
    requestOptions.timeout = waitTime * 10;
    requestOptions.returnBodyOnError = true;

    const reply = download(
      instanceInfo.url + '/_admin/execute?returnAsJSON=true',
      remoteCommand,
      requestOptions);

    print('waiting ' + waitTime + ' for server GC - done.');

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

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a command, possible with valgrind
// //////////////////////////////////////////////////////////////////////////////

function executeArangod (cmd, args, options) {
  if (options.valgrind) {
    let valgrindOpts = {};

    if (options.valgrindArgs) {
      valgrindOpts = options.valgrindArgs;
    }

    let testfn = options.valgrindFileBase;

    if (testfn.length > 0) {
      testfn += '_';
    }

    if (valgrindOpts.xml === 'yes') {
      valgrindOpts['xml-file'] = testfn + '.%p.xml';
    }

    valgrindOpts['log-file'] = testfn + '.%p.valgrind.log';

    args = toArgv(valgrindOpts, true).concat([cmd]).concat(args);
    cmd = options.valgrind;
  } else if (options.rr) {
    args = [cmd].concat(args);
    cmd = 'rr';
  }

  if (options.extremeVerbosity) {
    print('starting process ' + cmd + ' with arguments: ' + JSON.stringify(args));
  }
  return executeExternal(cmd, args);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief commands a server to shut down via webcall
// //////////////////////////////////////////////////////////////////////////////

function shutdownArangod (arangod, options, forceTerminate) {
  if (forceTerminate === undefined) {
    forceTerminate = false;
  }
  if (options.hasOwnProperty('server')) {
    print('running with external server');
    return;
  }

  if (options.valgrind) {
    waitOnServerForGC(arangod, options, 60);
  }
  if ((arangod.exitStatus === undefined) ||
      (arangod.exitStatus.status === 'RUNNING')) {
    if (forceTerminate) {
      killExternal(arangod.pid, abortSignal);
      arangod.exitStatus = {
        SIGNAL: String(abortSignal)
      };
      analyzeServerCrash(arangod, options, 'shutdown timeout; instance forcefully KILLED because of fatal timeout in testrun');
    } else if (options.useKillExternal) {
      killExternal(arangod.pid);
    } else {
      const requestOptions = makeAuthorizationHeaders(options);
      requestOptions.method = 'DELETE';
      print(arangod.url + '/_admin/shutdown');
      const reply = download(arangod.url + '/_admin/shutdown', '', requestOptions);
      if (options.extremeVerbosity) {
        print('Shutdown response: ' + JSON.stringify(reply));
      }
    }
  } else {
    print('Server already dead, doing nothing.');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief shuts down an instance
// //////////////////////////////////////////////////////////////////////////////

function shutdownInstance (instanceInfo, options, forceTerminate) {
  if (forceTerminate === undefined) {
    forceTerminate = false;
  }

  if (!checkInstanceAlive(instanceInfo, options)) {
    print('Server already dead, doing nothing. This shouldn\'t happen?');
  }

  // Shut down all non-agency servers:
  const n = instanceInfo.arangods.length;

  let nonagencies = instanceInfo.arangods
    .filter(arangod => arangod.role !== 'agent');
  nonagencies.sort((a, b) => {
    if (a.role === b.role) return 0;
    if (a.role === 'coordinator' &&
        b.role === 'dbserver') return -1;
    if (b.role === 'coordinator' &&
        a.role === 'dbserver') return 1;
    return 0;
  });
  print('Shutdown order ' + JSON.stringify(nonagencies));
  nonagencies.forEach(arangod => {
    wait(0.025);
    shutdownArangod(arangod, options, forceTerminate);
  });

  let agentsKilled = false;
  let nrAgents = n - nonagencies.length;

  let timeout = 666;
  if (options.valgrind) {
    timeout *= 10;
  }
  if (options.sanitizer) {
    timeout *= 2;
  }

  var shutdownTime = require('internal').time();

  let toShutdown = instanceInfo.arangods.slice();
  while (toShutdown.length > 0) {
    // Once all other servers are shut down, we take care of the agents,
    // we do this exactly once (agentsKilled flag) and only if there
    // are agents:
    if (!agentsKilled && nrAgents > 0 && toShutdown.length === nrAgents) {
      instanceInfo.arangods
        .filter(arangod => arangod.role === 'agent')
        .forEach(arangod => shutdownArangod(arangod, options));
      agentsKilled = true;
    }
    toShutdown = toShutdown.filter(arangod => {
      arangod.exitStatus = statusExternal(arangod.pid, false);

      if (arangod.exitStatus.status === 'RUNNING') {
        let localTimeout = timeout;
        if (arangod.role === 'agent') {
          localTimeout = localTimeout + 60;
        }
        if ((require('internal').time() - shutdownTime) > localTimeout) {
          print('forcefully terminating ' + yaml.safeDump(arangod.pid) +
            ' after ' + timeout + 's grace period; marking crashy.');
          serverCrashed = true;
          /* TODO!
          if (platform.substr(0, 3) === 'win') {
            const procdumpArgs = [
              '-accepteula',
              '-ma',
              arangod.pid,
              fs.join(instanceInfo.rootDir, 'core.dmp')
            ];
          }
          */
          killExternal(arangod.pid, abortSignal);
          analyzeServerCrash(arangod, options, 'shutdown timeout; instance forcefully KILLED after 60s - ' + arangod.exitStatus.signal);
          return false;
        } else {
          return true;
        }
      } else if (arangod.exitStatus.status !== 'TERMINATED') {
        if (arangod.exitStatus.hasOwnProperty('signal')) {
          analyzeServerCrash(arangod, options, 'instance Shutdown - ' + arangod.exitStatus.signal);
          serverCrashed = true;
        }
      } else {
        print('Server shutdown: Success: pid', arangod.pid);
        return false;
      }
    });
    if (toShutdown.length > 0) {
      print(toShutdown.length + ' arangods are still running...');
      wait(1);
    }
  }

  if (!options.skipLogAnalysis) {
    instanceInfo.arangods.forEach(arangod => {
      let errorEntries = readImportantLogLines(arangod.rootDir);
      if (Object.keys(errorEntries).length > 0) {
        print('Found messages in the server logs: \n' +
          yaml.safeDump(errorEntries));
      }
    });
  }

  cleanupDirectories.unshift(instanceInfo.rootDir);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief starts an instance
// /
// / protocol must be one of ["tcp", "ssl", "unix"]
// //////////////////////////////////////////////////////////////////////////////

function startInstanceCluster (instanceInfo, protocol, options,
  addArgs, rootDir) {
  let makeArgs = function (name, role, args) {
    args = args || {};

    let subDir = fs.join(rootDir, name);
    fs.makeDirectoryRecursive(subDir);

    let subArgs = makeArgsArangod(options, fs.join(subDir, 'apps'), role, fs.join(subDir, 'tmp'));
    // FIXME: someone should decide on the order of preferences
    subArgs = Object.assign(subArgs, addArgs);
    subArgs = Object.assign(subArgs, args);

    return [subArgs, subDir];
  };

  options.agencyWaitForSync = false;
  let usedPorts = [];
  options.usedPorts = usedPorts;
  startInstanceAgency(instanceInfo, protocol, options, ...makeArgs('agency', 'agency', {}));

  let agencyEndpoint = instanceInfo.endpoint;

  if (!checkInstanceAlive(instanceInfo, options)) {
    throw new Error('startup of agency failed! bailing out!');
  }

  let i;
  for (i = 0; i < options.dbServers; i++) {
    let port = findFreePort(options.minPort, options.maxPort, usedPorts);
    usedPorts.push(port);
    let endpoint = protocol + '://127.0.0.1:' + port;
    let primaryArgs = _.clone(options.extraArgs);
    primaryArgs['server.endpoint'] = endpoint;
    primaryArgs['cluster.my-address'] = endpoint;
    primaryArgs['cluster.my-local-info'] = endpoint;
    primaryArgs['cluster.my-role'] = 'PRIMARY';
    primaryArgs['cluster.agency-endpoint'] = agencyEndpoint;

    startInstanceSingleServer(instanceInfo, protocol, options, ...makeArgs('dbserver' + i, 'dbserver', primaryArgs), 'dbserver');
  }

  for (i = 0; i < options.coordinators; i++) {
    let port = findFreePort(options.minPort, options.maxPort, usedPorts);
    usedPorts.push(port);
    let endpoint = protocol + '://127.0.0.1:' + port;
    let coordinatorArgs = _.clone(options.extraArgs);
    coordinatorArgs['server.endpoint'] = endpoint;
    coordinatorArgs['cluster.my-address'] = endpoint;
    coordinatorArgs['cluster.my-local-info'] = endpoint;
    coordinatorArgs['cluster.my-role'] = 'COORDINATOR';
    coordinatorArgs['cluster.agency-endpoint'] = agencyEndpoint;

    startInstanceSingleServer(instanceInfo, protocol, options, ...makeArgs('coordinator' + i, 'coordinator', coordinatorArgs), 'coordinator');
  }

  // disabled because not in use (jslint)
  // let coordinatorUrl = instanceInfo.url
  // let response
  let httpOptions = makeAuthorizationHeaders(options);
  httpOptions.method = 'POST';
  httpOptions.returnBodyOnError = true;

  // scrape the jwt token
  let authOpts = _.clone(options);
  if (addArgs['server.jwt-secret'] && !authOpts['server.jwt-secret']) {
    authOpts['server.jwt-secret'] = addArgs['server.jwt-secret'];
  }

  let count = 0;
  while (true) {
    ++count;

    instanceInfo.arangods.forEach(arangod => {
      const reply = download(arangod.url + '/_api/version', '', makeAuthorizationHeaders(authOpts));
      if (!reply.error && reply.code === 200) {
        arangod.upAndRunning = true;
        return true;
      }

      if (!checkArangoAlive(arangod, options)) {
        instanceInfo.arangods.forEach(arangod => {
          killExternal(arangod.pid, abortSignal);
          analyzeServerCrash(arangod, options, 'startup timeout; forcefully terminating ' + arangod.role + ' with pid: ' + arangod.pid);
        });

        throw new Error(`cluster startup: pid ${arangod.pid} no longer alive! bailing out!`);
      }
      wait(0.5, false);
      return true;
    });

    let upAndRunning = 0;
    instanceInfo.arangods.forEach(arangod => {
      if (arangod.upAndRunning) {
        upAndRunning += 1;
      }
    });
    if (upAndRunning === instanceInfo.arangods.length) {
      break;
    }

    // Didn't startup in 10 minutes? kill it, give up.
    if (count > 1200) {
      instanceInfo.arangods.forEach(arangod => {
        killExternal(arangod.pid, abortSignal);
        analyzeServerCrash(arangod, options, 'startup timeout; forcefully terminating ' + arangod.role + ' with pid: ' + arangod.pid);
      });
      throw new Error('cluster startup timed out after 10 minutes!');
    }
  }

  arango.reconnect(instanceInfo.endpoint, '_system', 'root', '');
  return true;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief starts an instance
// /
// / protocol must be one of ["tcp", "ssl", "unix"]
// //////////////////////////////////////////////////////////////////////////////

function startArango (protocol, options, addArgs, rootDir, role) {
  const dataDir = fs.join(rootDir, 'data');
  const appDir = fs.join(rootDir, 'apps');
  const tmpDir = fs.join(rootDir, 'tmp');

  fs.makeDirectoryRecursive(dataDir);
  fs.makeDirectoryRecursive(appDir);
  fs.makeDirectoryRecursive(tmpDir);

  let args = makeArgsArangod(options, appDir, role, tmpDir);
  let endpoint;
  let port;

  if (!addArgs['server.endpoint']) {
    port = findFreePort(options.minPort, options.maxPort);
    endpoint = protocol + '://127.0.0.1:' + port;
  } else {
    endpoint = addArgs['server.endpoint'];
    port = endpoint.split(':').pop();
  }

  let instanceInfo = {
    role,
    port,
    endpoint,
    rootDir
  };

  args['server.endpoint'] = endpoint;
  args['database.directory'] = dataDir;
  args['log.file'] = fs.join(rootDir, 'log');

  if (protocol === 'ssl') {
    args['ssl.keyfile'] = fs.join('UnitTests', 'server.pem');
  }

  args = Object.assign(args, options.extraArgs);

  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }

  if (options.verbose) {
    args['log.level'] = 'debug';
  }

  instanceInfo.url = endpointToURL(instanceInfo.endpoint);
  try {
    instanceInfo.pid = executeArangod(ARANGOD_BIN, toArgv(args), options).pid;
  } catch (x) {
    print('failed to run arangod - ' + JSON.stringify(x));

    throw x;
  }
  instanceInfo.role = role;

  if (platform.substr(0, 3) === 'win') {
    const procdumpArgs = [
      '-accepteula',
      '-e',
      '-ma',
      instanceInfo.pid,
      fs.join(rootDir, 'core.dmp')
    ];

    try {
      instanceInfo.monitor = executeExternal('procdump', procdumpArgs);
    } catch (x) {
      print('failed to start procdump - is it installed?');
      // throw x;
    }
  }
  return instanceInfo;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief starts an agency instance
// /
// / protocol must be one of ["tcp", "ssl", "unix"]
// //////////////////////////////////////////////////////////////////////////////

function startInstanceAgency (instanceInfo, protocol, options, addArgs, rootDir) {
  const dataDir = fs.join(rootDir, 'data');

  const N = options.agencySize;
  const S = options.agencySupervision;
  if (options.agencyWaitForSync === undefined) {
    options.agencyWaitForSync = false;
  }
  const wfs = options.agencyWaitForSync;

  let usedPorts = options.usedPorts || [];
  for (let i = 0; i < N; i++) {
    let instanceArgs = _.clone(addArgs);
    instanceArgs['log.file'] = fs.join(rootDir, 'log' + String(i));
    instanceArgs['agency.activate'] = 'true';
    instanceArgs['agency.size'] = String(N);
    instanceArgs['agency.pool-size'] = String(N);
    instanceArgs['agency.wait-for-sync'] = String(wfs);
    instanceArgs['agency.supervision'] = String(S);
    instanceArgs['database.directory'] = dataDir + String(i);
    const port = findFreePort(options.minPort, options.maxPort, usedPorts);
    usedPorts.push(port);
    instanceArgs['server.endpoint'] = protocol + '://127.0.0.1:' + port;
    instanceArgs['agency.my-address'] = protocol + '://127.0.0.1:' + port;
    instanceArgs['agency.supervision-grace-period'] = '10.0';
    instanceArgs['agency.supervision-frequency'] = '1.0';

    if (i === N - 1) {
      let l = [];
      instanceInfo.arangods.forEach(arangod => {
        l.push('--agency.endpoint');
        l.push(arangod.endpoint);
      });
      l.push('--agency.endpoint');
      l.push(protocol + '://127.0.0.1:' + port);

      instanceArgs['flatCommands'] = l;
    }
    let dir = fs.join(rootDir, 'agency-' + i);
    fs.makeDirectoryRecursive(dir);
    fs.makeDirectoryRecursive(instanceArgs['database.directory']);
    instanceInfo.arangods.push(startArango(protocol, options, instanceArgs, rootDir, 'agent'));
    instanceInfo.endpoint = instanceInfo.arangods[instanceInfo.arangods.length - 1].endpoint;
    instanceInfo.url = instanceInfo.arangods[instanceInfo.arangods.length - 1].url;
    instanceInfo.role = 'agent';
    print('Agency Endpoint: ' + instanceInfo.endpoint);
  }
  return instanceInfo;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief starts a single server instance
// /
// / protocol must be one of ["tcp", "ssl", "unix"]
// //////////////////////////////////////////////////////////////////////////////

function startInstanceSingleServer (instanceInfo, protocol, options,
  addArgs, rootDir, role) {
  instanceInfo.arangods.push(startArango(protocol, options, addArgs, rootDir, role));

  instanceInfo.endpoint = instanceInfo.arangods[instanceInfo.arangods.length - 1].endpoint;
  instanceInfo.url = instanceInfo.arangods[instanceInfo.arangods.length - 1].url;

  return instanceInfo;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief starts any sort server instance
// /
// / protocol must be one of ["tcp", "ssl", "unix"]
// //////////////////////////////////////////////////////////////////////////////

function startInstance (protocol, options, addArgs, testname, tmpDir) {
  let rootDir = fs.join(tmpDir || fs.getTempPath(), testname);
  let instanceInfo = {
    rootDir,
    arangods: []
  };

  const startTime = time();
  try {
    if (options.hasOwnProperty('server')) {
      return { endpoint: options.server,
               rootDir: options.serverRoot,
               url: options.server.replace('tcp', 'http'),
               arangods: []
             };
    } else if (options.cluster) {
      startInstanceCluster(instanceInfo, protocol, options,
                           addArgs, rootDir);
    } else if (options.agency) {
      startInstanceAgency(instanceInfo, protocol, options,
                          addArgs, rootDir);
    } else {
      startInstanceSingleServer(instanceInfo, protocol, options,
                                addArgs, rootDir, 'single');
    }

    if (!options.cluster) {
      let count = 0;
      instanceInfo.arangods.forEach(arangod => {
        while (true) {
          if (options.useReconnect) {
            try {
              arango.reconnect(instanceInfo.endpoint, '_system', options.username, options.password);
              break;
            } catch (e) {
            }
          } else {
            const reply = download(arangod.url + '/_api/version', '', makeAuthorizationHeaders(options));

            if (!reply.error && reply.code === 200) {
              break;
            }
          }
          ++count;

          if (count % 60 === 0) {
            if (!checkArangoAlive(arangod, options)) {
              throw new Error('startup failed! bailing out!');
            }
          }
          wait(0.5, false);
        }
      });
    }
    print(CYAN + 'up and running in ' + (time() - startTime) + ' seconds' + RESET);
    var matchPort = /.*:.*:([0-9]*)/;
    var ports = [];
    var processInfo = [];
    instanceInfo.arangods.forEach(arangod => {
      let res = matchPort.exec(arangod.endpoint);
      if (!res) {
        return;
      }
      var port = res[1];
      ports.push('port ' + port);
      processInfo.push('  [' + arangod.role + '] up with pid ' + arangod.pid + ' on port ' + port);
    });

    print('sniffing template:\n  tcpdump -ni lo -s0 -w /tmp/out.pcap ' + ports.join(' or ') + '\n');
    print(processInfo.join('\n') + '\n');
  } catch (e) {
    print(e, e.stack);
    return false;
  }

  return instanceInfo;
}

// exports.analyzeServerCrash = analyzeServerCrash;
exports.makeArgs = {
  arangod: makeArgsArangod,
  arangosh: makeArgsArangosh
};

exports.arangod = {
  check: {
    alive: checkArangoAlive,
    instanceAlive: checkInstanceAlive
  },
  shutdown: shutdownArangod
};

exports.findFreePort = findFreePort;

exports.executeArangod = executeArangod;
exports.executeAndWait = executeAndWait;

exports.run = {
  arangoshCmd: runArangoshCmd,
  arangoImp: runArangoImp,
  arangoDumpRestore: runArangoDumpRestore,
  arangoBenchmark: runArangoBenchmark
};

exports.shutdownInstance = shutdownInstance;
// exports.startInstanceCluster = startInstanceCluster;
exports.startArango = startArango;
// exports.startInstanceAgency = startInstanceAgency;
// exports.startInstanceSingleServer = startInstanceSingleServer;
exports.startInstance = startInstance;
exports.setupBinaries = setupBinaries;

exports.executableExt = executableExt;
exports.serverCrashed = serverCrashed;

exports.cleanupDBDirectoriesAppend = cleanupDBDirectoriesAppend;
exports.cleanupDBDirectories = cleanupDBDirectories;
exports.cleanupLastDirectory = cleanupLastDirectory;
exports.getCleanupDBDirectories = getCleanupDBDirectories;

exports.makeAuthorizationHeaders = makeAuthorizationHeaders;

Object.defineProperty(exports, 'ARANGOEXPORT_BIN', {get: () => ARANGOEXPORT_BIN});
Object.defineProperty(exports, 'ARANGOD_BIN', {get: () => ARANGOD_BIN});
Object.defineProperty(exports, 'ARANGOSH_BIN', {get: () => ARANGOSH_BIN});
Object.defineProperty(exports, 'CONFIG_DIR', {get: () => CONFIG_DIR});
Object.defineProperty(exports, 'TOP_DIR', {get: () => TOP_DIR});
Object.defineProperty(exports, 'LOGS_DIR', {get: () => LOGS_DIR});
Object.defineProperty(exports, 'UNITTESTS_DIR', {get: () => UNITTESTS_DIR});
Object.defineProperty(exports, 'BIN_DIR', {get: () => BIN_DIR});
Object.defineProperty(exports, 'CONFIG_ARANGODB_DIR', {get: () => CONFIG_ARANGODB_DIR});
Object.defineProperty(exports, 'CONFIG_RELATIVE_DIR', {get: () => CONFIG_RELATIVE_DIR});
Object.defineProperty(exports, 'serverCrashed', {get: () => serverCrashed});
