/* jshint strict: false, sub: true */
/* global print, arango */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const rp = require('@arangodb/testutils/result-processing');
const tu = require('@arangodb/testutils/test-utils');
const yaml = require('js-yaml');
const internal = require('internal');
const crashUtils = require('@arangodb/testutils/crash-utils');
const {sanHandler} = require('@arangodb/testutils/san-file-handler');
const crypto = require('@arangodb/crypto');
const ArangoError = require('@arangodb').ArangoError;

/* Functions: */
const toArgv = internal.toArgv;
const executeExternalAndWait = internal.executeExternalAndWait;
const killExternal = internal.killExternal;
const statusExternal = internal.statusExternal;
const statisticsExternal = internal.statisticsExternal;
const base64Encode = internal.base64Encode;
const testPort = internal.testPort;
const download = internal.download;
const time = internal.time;
const wait = internal.wait;
const sleep = internal.sleep;

/* Constants: */
// const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
// const YELLOW = internal.COLORS.COLOR_YELLOW;
const IS_A_TTY = internal.isATTy();

const platform = internal.platform;

const abortSignal = 6;
const termSignal = 15;

let tcpdump;
let assertLines = [];

let executableExt = '';
let serverCrashedLocal = false;
let serverFailMessagesLocal = "";
let cleanupDirectories = [];

let binaries = [];
let currentBinarySet = 0;

function switchBinarySet(which) {
  currentBinarySet = which;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief calculates all the path locations
// / required to be called first.
// //////////////////////////////////////////////////////////////////////////////

function setupBinaries (options) {
  function setupOneSet(directory, bin_dir) {
    let oneSet = {};

    oneSet.BIN_DIR = bin_dir;
    print(oneSet.BIN_DIR);
    print('1');
    if (fs.exists(fs.join(directory, oneSet.BIN_DIR))) {
      print(oneSet.BIN_DIR);
      print('2');
      oneSet.BIN_DIR = fs.join(directory, oneSet.BIN_DIR);
      if (fs.exists(fs.join(oneSet.BIN_DIR, "bin"))) {
        print(oneSet.BIN_DIR);
        print('3');
        oneSet.BIN_DIR = fs.join(oneSet.BIN_DIR, "bin");
      }
    } else if (fs.exists(fs.join(directory, "bin"))) {
      print('4');
      oneSet.BIN_DIR = fs.join(directory, 'bin');
    } else if (fs.exists(fs.join(oneSet.BIN_DIR, 'bin'))) {
      oneSet.BIN_DIR = fs.join(oneSet.BIN_DIR, 'bin');
      print('5');
    } else {
      print('6');
      oneSet.BIN_DIR = fs.makeAbsolute(oneSet.BIN_DIR);
    }
    print(oneSet.BIN_DIR);
    oneSet.TOP_DIR = directory;
    oneSet.UNITTESTS_DIR = fs.join(fs.join(options.build, 'tests'));
    if (!fs.exists(oneSet.UNITTESTS_DIR)) {
      oneSet.UNITTESTS_DIR = fs.join(directory, oneSet.UNITTESTS_DIR);
    }

    oneSet.ARANGOBACKUP_BIN = fs.join(oneSet.BIN_DIR, 'arangobackup' + executableExt);
    oneSet.ARANGOBENCH_BIN = fs.join(oneSet.BIN_DIR, 'arangobench' + executableExt);
    oneSet.ARANGODUMP_BIN = fs.join(oneSet.BIN_DIR, 'arangodump' + executableExt);
    oneSet.ARANGOD_BIN = fs.join(oneSet.BIN_DIR, 'arangod' + executableExt);
    oneSet.ARANGOIMPORT_BIN = fs.join(oneSet.BIN_DIR, 'arangoimport' + executableExt);
    oneSet.ARANGORESTORE_BIN = fs.join(oneSet.BIN_DIR, 'arangorestore' + executableExt);
    oneSet.ARANGOEXPORT_BIN = fs.join(oneSet.BIN_DIR, 'arangoexport' + executableExt);
    oneSet.ARANGOSH_BIN = fs.join(oneSet.BIN_DIR, 'arangosh' + executableExt);
    oneSet.ARANGO_SECURE_INSTALLATION_BIN = fs.join(oneSet.BIN_DIR, 'arango-secure-installation' + executableExt);

    oneSet.CONFIG_ARANGODB_DIR = fs.join(options.build, 'etc', 'arangodb3');
    if (!fs.exists(oneSet.CONFIG_ARANGODB_DIR)) {
      oneSet.CONFIG_ARANGODB_DIR = fs.join(directory, oneSet.CONFIG_ARANGODB_DIR);
    }

    oneSet.CONFIG_RELATIVE_DIR = fs.join(directory, 'etc', 'relative');
    oneSet.CONFIG_DIR = fs.join(directory, options.configDir);

    oneSet.JS_DIR = fs.join(directory, 'js');
    oneSet.JS_ENTERPRISE_DIR = fs.join(directory, 'enterprise/js');

    oneSet.LOGS_DIR = fs.join(directory, 'logs');

    let checkFiles = [
      oneSet.ARANGOBENCH_BIN,
      oneSet.ARANGODUMP_BIN,
      oneSet.ARANGOD_BIN,
      oneSet.ARANGOIMPORT_BIN,
      oneSet.ARANGORESTORE_BIN,
      oneSet.ARANGOBACKUP_BIN,
      oneSet.ARANGOEXPORT_BIN,
      oneSet.ARANGOSH_BIN
    ];

    checkFiles.forEach((file) => {
      if (!fs.isFile(file)) {
        throw new Error('unable to locate ' + file);
      }
    });

    global.ARANGOSH_BIN = oneSet.ARANGOSH_BIN;
    return oneSet;
  }
  if (options.oldSource !== undefined) {
    let oldSource = fs.makeAbsolute(options.oldSource);
    let oldBin = '';
    if (fs.exists(fs.join(oldSource, 'build')) && fs.exists(fs.join(oldSource, 'build', 'bin'))) {
      oldBin = 'build';
    }
    binaries.push(setupOneSet(oldSource, oldBin));
  }
  if (options.build === '') {
    print('FATAL: cannot find binaries, use "--build"\n');
    return {
      status: false
    };
  }
  const topDir = fs.normalize(fs.makeAbsolute('.'));
  if (!fs.exists('3rdParty') && !fs.exists('arangod') &&
    !fs.exists('client-tools') && !fs.exists('tests')) {
    throw new Error('Must be in ArangoDB topdir to execute tests.');
  }

  binaries.push(setupOneSet(topDir, options.build));
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief if we forgot about processes, this safe guard will clean up and mark failed
// //////////////////////////////////////////////////////////////////////////////

function killRemainingProcesses(results) {
  let running = internal.getExternalSpawned();
  results.status = results.status && (running.length === 0);
  for (let i = 0; i < running.length; i++) {
    let timeoutReached = internal.SetGlobalExecutionDeadlineTo(0.0);
    print('VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV');
    if (timeoutReached) {
      print(RED + Date() + ' external deadline reached!' + RESET);
    }
    internal.removePidFromMonitor(running[i].pid);
  }
  sleep(1);
  for (let i = 0; i < running.length; i++) {
    let timeoutReached = internal.SetGlobalExecutionDeadlineTo(0.0);
    print('VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV');
    if (timeoutReached) {
      print(RED + Date() + ' external deadline reached!' + RESET);
    }
    let status = internal.statusExternal(running[i].pid, false);
    if (status.status === "TERMINATED") {
      print(RED + Date() + " process exited without us joining it (marking crashy): " +
            JSON.stringify(running[i]) + JSON.stringify(status) + RESET);
    }
    else {
      print(RED + Date() + " Killing remaining process & marking crashy: " + JSON.stringify(running[i]) + RESET);
      print(killExternal(running[i].pid, abortSignal));
    }
    print('^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^');
    results.crashed = true;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief arguments for testing (client)
// //////////////////////////////////////////////////////////////////////////////


// //////////////////////////////////////////////////////////////////////////////
// / @brief loads the JWT secret from the various ways possible
// //////////////////////////////////////////////////////////////////////////////

function getJwtSecret(args) {
  let jwtSecret;
  if (args.hasOwnProperty('server.jwt-secret-folder')) {
    let files = fs.list(args['server.jwt-secret-folder']);
    files.sort();
    if (files.length > 0) {
      jwtSecret = fs.read(fs.join(args['server.jwt-secret-folder'], files[0]));
    }
  } else {
    jwtSecret = args['server.jwt-secret'];
  }
  return jwtSecret;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief adds authorization headers
// //////////////////////////////////////////////////////////////////////////////

function makeAuthorizationHeaders (options, args, JWT) {
  let jwtSecret = getJwtSecret(args);
  if (JWT) {
    jwtSecret = JWT;
  }

  if (jwtSecret) {
    let jwt;
    if (jwtSecret.startsWith("-----BEGIN PRIVATE KEY-----")) {
      jwt = crypto.jwtEncode(jwtSecret,
                             {'server_id': 'none',
                              'iss': 'arangodb'}, 'ES256');
    } else {
      jwt = crypto.jwtEncode(jwtSecret,
                             {'server_id': 'none',
                              'iss': 'arangodb'}, 'HS256');
    }
    if (options.extremeVerbosity) {
      print(Date() + ' Using jw token:     ' + jwt);
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
// / @brief executes a command and waits for result
// //////////////////////////////////////////////////////////////////////////////

function executeAndWait (cmd, args, options, valgrindTest, rootDir, coreCheck = false, timeout = 0, instanceInfo = {}) {
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

  const launchCmd = `${Date()} executeAndWait: cmd =${cmd} args =${JSON.stringify(args)}`;
  if (options.extremeVerbosity) {
    print(launchCmd);
  }

  const startTime = time();
  if ((typeof (cmd) !== 'string') || (cmd === 'true') || (cmd === 'false')) {
    return {
      timeout: false,
      status: false,
      message: 'true or false as binary name for test cmd =' + cmd + 'args =' + args
    };
  }

  if (!instanceInfo.hasOwnProperty('rootDir')) {
    instanceInfo = {
      rootDir: rootDir,
      pid: 0,
      exitStatus: {},
      getStructure: function() { return {}; }
    };
  }

  // V8 executeExternalAndWait thinks that timeout is in ms, so *1000

  let sh = new sanHandler(cmd.replace(/.*\//, ''), options);
  sh.detectLogfiles(instanceInfo.rootDir, instanceInfo.rootDir);
  let res = executeExternalAndWait(cmd, args, false, timeout * 1000,  sh.getSanOptions());

  instanceInfo.pid = res.pid;
  instanceInfo.exitStatus = res;
  const deltaTime = (time() - startTime) * 1000;
  let errorMessage = ' - ';
  if (sh.fetchSanFileAfterExit(res.pid)) {
    serverCrashedLocal = true;
    res.status = false;
    errorMessage += " Sanitizer indicated issues  - ";
  }

  if (coreCheck &&
      instanceInfo.exitStatus.hasOwnProperty('signal') &&
      ((instanceInfo.exitStatus.signal === 11) ||
       (instanceInfo.exitStatus.signal === 6) ||
        // mac sometimes SIG_ILLs...
       (instanceInfo.exitStatus.signal === 4))) {
    print(`${Date()} executeAndWait: Marking '${launchCmd}' crashy - ${JSON.stringify(instanceInfo)}`);
    crashUtils.analyzeCrash(cmd,
                            instanceInfo,
                            options,
                            `execution of '${launchCmd}' - ${instanceInfo.exitStatus.signal}`);
    if (options.coreCheck) {
      print(instanceInfo.exitStatus.gdbHint);
    }
    serverCrashedLocal = true;
  }

  if (instanceInfo.exitStatus.status === 'TERMINATED') {
    const color = (instanceInfo.exitStatus.exit === 0 ? GREEN : RED);

    print(color + Date() + ' Finished: ' + instanceInfo.exitStatus.status +
      ' exit code: ' + instanceInfo.exitStatus.exit +
      ' Time elapsed: ' + deltaTime + RESET);

    if (instanceInfo.exitStatus.exit === 0) {
      return {
        timeout: false,
        status: true,
        message: '',
        duration: deltaTime
      };
    } else {
      return {
        timeout: false,
        status: false,
        message: `exit code of '${launchCmd}' was ${instanceInfo.exitStatus.exit}`,
        duration: deltaTime,
        exitCode: instanceInfo.exitStatus.exit,
      };
    }
  } else if (instanceInfo.exitStatus.status === 'ABORTED') {
    if (typeof (instanceInfo.exitStatus.errorMessage) !== 'undefined') {
      errorMessage += instanceInfo.exitStatus.errorMessage;
    }

    print(Date() + ' Finished: ' + instanceInfo.exitStatus.status +
      ' Signal: ' + instanceInfo.exitStatus.signal +
      ' Time elapsed: ' + deltaTime + errorMessage);

    return {
      timeout: false,
      status: false,
      message: `irregular termination of '${launchCmd}': ${instanceInfo.exitStatus.status}` +
        ` exit signal: ${instanceInfo.exitStatus.signal} ${errorMessage}`,
      duration: deltaTime
    };
  } else if (res.status === 'TIMEOUT') {
    print(`${RED}${Date()} Killing ${cmd} - ${JSON.stringify(args)} after ${timeout}s${RESET}`);
    let resKill = killExternal(res.pid, abortSignal);
    if (coreCheck) {
      print(`${Date()} executeAndWait: Marking crashy because of timeout - ${JSON.stringify(instanceInfo)}`);
      crashUtils.analyzeCrash(cmd,
                              instanceInfo,
                              options,
                              `execution of '${launchCmd}' - kill because of timeout`);
      if (options.coreCheck) {
        print(instanceInfo.exitStatus.gdbHint);
      }
      serverCrashedLocal = true;
    }
    instanceInfo.pid = res.pid;
    instanceInfo.exitStatus = res;
    return {
      timeout: true,
      status: false,
      message: `termination of '${launchCmd}' by timeout: ${instanceInfo.exitStatus.status}` +
        ` killed by : ${abortSignal} ${errorMessage}`,
      duration: deltaTime
    };
  } else {
    if (typeof (instanceInfo.exitStatus.errorMessage) !== 'undefined') {
      errorMessage += instanceInfo.exitStatus.errorMessage;
    }

    print(Date() + ' Finished: ' + instanceInfo.exitStatus.status +
      ' exit code: ' + instanceInfo.exitStatus.signal +
      ' Time elapsed: ' + deltaTime + errorMessage);

    return {
      timeout: false,
      status: false,
      message: `irregular termination of  '${launchCmd}' : ${instanceInfo.exitStatus.status}` +
        ` exit code: ${instanceInfo.exitStatus.exit} ${errorMessage}`,
      duration: deltaTime
    };
  }
}

exports.setupBinaries = setupBinaries;
exports.endpointToURL = endpointToURL;

exports.executeAndWait = executeAndWait;
exports.killRemainingProcesses = killRemainingProcesses;

exports.executableExt = executableExt;

exports.switchBinarySet = switchBinarySet;
exports.makeAuthorizationHeaders = makeAuthorizationHeaders;
Object.defineProperty(exports, 'JS_DIR', {get: () => binaries[currentBinarySet].JS_DIR});
Object.defineProperty(exports, 'JS_ENTERPRISE_DIR', {get: () => binaries[currentBinarySet].JS_ENTERPRISE_DIR});
Object.defineProperty(exports, 'ARANGOBACKUP_BIN', {get: () => binaries[currentBinarySet].ARANGOBACKUP_BIN});
Object.defineProperty(exports, 'ARANGOBENCH_BIN', {get: () => binaries[currentBinarySet].ARANGOBENCH_BIN});
Object.defineProperty(exports, 'ARANGODUMP_BIN', {get: () => binaries[currentBinarySet].ARANGODUMP_BIN});
Object.defineProperty(exports, 'ARANGOD_BIN', {get: () => binaries[currentBinarySet].ARANGOD_BIN});
Object.defineProperty(exports, 'ARANGOEXPORT_BIN', {get: () => binaries[currentBinarySet].ARANGOEXPORT_BIN});
Object.defineProperty(exports, 'ARANGOIMPORT_BIN', {get: () => binaries[currentBinarySet].ARANGOIMPORT_BIN});
Object.defineProperty(exports, 'ARANGORESTORE_BIN', {get: () => binaries[currentBinarySet].ARANGORESTORE_BIN});
Object.defineProperty(exports, 'ARANGOSH_BIN', {get: () => binaries[currentBinarySet].ARANGOSH_BIN});
Object.defineProperty(exports, 'ARANGO_SECURE_INSTALLATION_BIN', {get: () => binaries[currentBinarySet].ARANGO_SECURE_INSTALLATION_BIN});
Object.defineProperty(exports, 'CONFIG_DIR', {get: () => binaries[currentBinarySet].CONFIG_DIR});
Object.defineProperty(exports, 'TOP_DIR', {get: () => binaries[currentBinarySet].TOP_DIR});
Object.defineProperty(exports, 'LOGS_DIR', {get: () => binaries[currentBinarySet].LOGS_DIR});
Object.defineProperty(exports, 'UNITTESTS_DIR', {get: () => binaries[currentBinarySet].UNITTESTS_DIR});
Object.defineProperty(exports, 'BIN_DIR', {get: () => binaries[currentBinarySet].BIN_DIR});
Object.defineProperty(exports, 'CONFIG_ARANGODB_DIR', {get: () => binaries[currentBinarySet].CONFIG_ARANGODB_DIR});
Object.defineProperty(exports, 'CONFIG_RELATIVE_DIR', {get: () => binaries[currentBinarySet].CONFIG_RELATIVE_DIR});
Object.defineProperty(exports, 'serverCrashed', {get: () => serverCrashedLocal, set: (value) => { serverCrashedLocal = value; } });
Object.defineProperty(exports, 'serverFailMessages', {get: () => serverFailMessagesLocal, set: (value) => { serverFailMessagesLocal = value; }});
exports.registerOptions = function(optionsDefaults, optionsDocumentation) {
  tu.CopyIntoObject(optionsDefaults, {
    'build': '',
    'oldSource': undefined,
    'configDir': 'etc/testing',
  });

  if (fs.exists('build') && fs.exists('build/bin')) {
    optionsDefaults.build = 'build';
  } else if (fs.exists('bin')) {
    optionsDefaults.build = 'bin';
  }

  tu.CopyIntoList(optionsDocumentation, [
    ' Environment options:',
    '   - `oldSource`: if we launch two set of binaries, the other source directory',
    '   - `build`: the directory containing the binaries',
    '   - `configDir`: the directory containing the config files, defaults to',
    '                  etc/testing',
    ''
  ]);
};
