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
const internal = require('internal');
const toArgv = internal.toArgv;
const crashUtils = require('@arangodb/crash-utils');
const crypto = require('@arangodb/crypto');

/* Functions: */
const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const killExternal = internal.killExternal;
const statusExternal = internal.statusExternal;
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

const platform = internal.platform;

const abortSignal = 6;

class ConfigBuilder {
  constructor(type) {
    this.config = {
      'log.foreground-tty': 'true'
    };
    this.type = type;
    switch (type) {
      case 'restore':
        this.config.configuration = fs.join(CONFIG_DIR, 'arangorestore.conf'); 
        this.executable = ARANGORESTORE_BIN;
        break;
      case 'dump':
        this.config.configuration = fs.join(CONFIG_DIR, 'arangodump.conf'); 
        this.executable = ARANGODUMP_BIN;
        break;
      default:
        throw 'Sorry this type of Arango-Binary is not yet implemented: ' + type;
    }
  }

  setAuth(username, password) {
    this.config['server.username'] = username;
    this.config['server.password'] = password;
  }
  setEndpoint(endpoint) { this.config['server.endpoint'] = endpoint; }
  setDatabase(database) { this.config['server.database'] = database; }
  setIncludeSystem(active) {
    if (this.type !== 'restore' && this.type !== 'dump') {
      throw '"include-system-collections" is not supported for binary: ' + this.type;
    }
    this.config['include-system-collections'] = active ? 'true' : 'false';
  }
  setOutputDirectory(dir) {
    if (this.type !== 'dump') {
      throw '"output-directory" is not supported for binary: ' + this.type;
    }
    this.config['output-directory'] = fs.join(this.rootDir, dir);
  }
  setInputDirectory(dir, createDatabase) {
    if (this.type !== 'restore') {
      throw '"input-directory" is not supported for binary: ' + this.type;
    }
    this.config['input-directory'] = fs.join(this.rootDir, dir);
    if (createDatabase) {
      this.config['create-database'] = 'true';
    } else {
      this.config['create-database'] = 'false';
    }
  }
  activateEncryption() { this.config['encription.keyfile'] = fs.join(this.rootDir, 'secret-key'); }
  setRootDir(dir) { this.rootDir = dir; }
  restrictToCollection(collection) {
    if (this.type !== 'restore' && this.type !== 'dump') {
      throw '"collection" is not supported for binary: ' + this.type;
    }
    this.config['collection'] = collection;
  };

  toArgv() { return internal.toArgv(this.config); }

  getExe() { return this.executable; }

  print() {
    print(this.executable);
    print(this.config);
  }
}

const createBaseConfigBuilder = function (type, options, instanceInfo, database = '_system') {
  const cfg = new ConfigBuilder(type);
  cfg.setAuth(options.username, options.password);
  cfg.setDatabase(database);
  cfg.setEndpoint(instanceInfo.endpoint);
  cfg.setRootDir(instanceInfo.rootDir);
  return cfg;
};

let executableExt = '';
if (platform.substr(0, 3) === 'win') {
  executableExt = '.exe';
}

let serverCrashedLocal = false;
let serverFailMessagesLocal = "";
let cleanupDirectories = [];

let BIN_DIR;
let ARANGOBENCH_BIN;
let ARANGODUMP_BIN;
let ARANGOD_BIN;
let ARANGOIMPORT_BIN;
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
    !fs.exists('arangosh') && !fs.exists('tests')) {
    throw new Error('Must be in ArangoDB topdir to execute tests.');
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

  UNITTESTS_DIR = fs.join(fs.join(builddir, 'tests'));
  if (!fs.exists(UNITTESTS_DIR)) {
    UNITTESTS_DIR = fs.join(TOP_DIR, UNITTESTS_DIR);
  }

  if (buildType !== '') {
    BIN_DIR = fs.join(BIN_DIR, buildType);
    UNITTESTS_DIR = fs.join(UNITTESTS_DIR, buildType);
  }

  ARANGOBENCH_BIN = fs.join(BIN_DIR, 'arangobench' + executableExt);
  ARANGODUMP_BIN = fs.join(BIN_DIR, 'arangodump' + executableExt);
  ARANGOD_BIN = fs.join(BIN_DIR, 'arangod' + executableExt);
  ARANGOIMPORT_BIN = fs.join(BIN_DIR, 'arangoimport' + executableExt);
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
    ARANGOIMPORT_BIN,
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

    internal.wait(0.1, false);
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
      if (options.extremeVerbosity === true) {
        print("Cleaning up: " + cleanupDirectory);
      }
      // Avoid attempting to remove the same directory multiple times
      if ((cleanupDirectories.indexOf(cleanupDirectory) === -1) &&
          (fs.exists(cleanupDirectory))) {
        let i = 0;
        while (i < 5) {
          try {
            fs.removeDirectoryRecursive(cleanupDirectory, true);
            return;
          } catch (x) {
            print('failed to delete directory "' + cleanupDirectory + '" - "' +
                  x + '" - Will retry in 5 seconds"');
            sleep(5);
          }
          i += 1;
        }
        print('failed to delete directory "' + cleanupDirectory + '" - "' +
              '" - Deferring cleanup for test run end."');
        cleanupDirectories.unshift(cleanupDirectory);
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
    'javascript.copy-installation': false,
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
// / @brief check whether process does bad on the wintendo
// //////////////////////////////////////////////////////////////////////////////

function runProcdump (options, instanceInfo, rootDir, pid) {
  let procdumpArgs = [ ];
  let dumpFile = fs.join(rootDir, 'core_' + pid + '.dmp');
  if (options.exceptionFilter != null) {
    procdumpArgs = [
      '-accepteula',
      '-64',
      '-e',
      options.exceptionCount
    ];
    let filters = options.exceptionFilter.split(',');
    for (let which in filters) {
      procdumpArgs.push('-f');
      procdumpArgs.push(filters[which]);
    }
    procdumpArgs.push('-ma');
    procdumpArgs.push(pid);
    procdumpArgs.push(dumpFile);
  } else {
    procdumpArgs = [
      '-accepteula',
      '-e',
      '-ma',
      pid,
      dumpFile
    ];
  }
  try {
    if (options.extremeVerbosity) {
      print("Starting procdump: " + JSON.stringify(procdumpArgs));
    }
    instanceInfo.monitor = executeExternal('procdump', procdumpArgs);
    instanceInfo.coreFilePattern = dumpFile;
  } catch (x) {
    print('failed to start procdump - is it installed?');
    // throw x;
  }
}

function stopProcdump (options, instanceInfo) {
  if (instanceInfo.hasOwnProperty('monitor') &&
      instanceInfo.monitor.pid !== null) {
    print("wating for procdump to exit");
    statusExternal(instanceInfo.monitor.pid, true);
    instanceInfo.monitor.pid = null;
  }
}
// //////////////////////////////////////////////////////////////////////////////
// / @brief executes a command and waits for result
// //////////////////////////////////////////////////////////////////////////////

function executeAndWait (cmd, args, options, valgrindTest, rootDir, circumventCores, coreCheck = false) {
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

  if (circumventCores) {
    if (platform.substr(0, 3) !== 'win') {
      // this shellscript will prevent cores from being writen on macos and linux.
      args.unshift(cmd);
      cmd = TOP_DIR + '/scripts/disable-cores.sh';
    }
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

  let instanceInfo = {
    rootDir: rootDir,
    pid: 0,
    exitStatus: {}
  };

  let res = {};
  if (platform.substr(0, 3) === 'win' && !options.disableMonitor) {
    res = executeExternal(cmd, args);
    instanceInfo.pid = res.pid;
    instanceInfo.exitStatus = res;
    runProcdump(options, instanceInfo, rootDir, res.pid);
    Object.assign(instanceInfo.exitStatus, 
                  statusExternal(res.pid, true));
    stopProcdump(options, instanceInfo);
  } else {
    res = executeExternalAndWait(cmd, args);
    instanceInfo.pid = res.pid;
    instanceInfo.exitStatus = res;
  }
  const deltaTime = time() - startTime;

  let errorMessage = ' - ';

  if (coreCheck &&
      instanceInfo.exitStatus.hasOwnProperty('signal') &&
      ((instanceInfo.exitStatus.signal === 11) ||
       (instanceInfo.exitStatus.signal === 6) ||
       // Windows sometimes has random numbers in signal...
       (platform.substr(0, 3) === 'win')
      )
     ) {
    print("executeAndWait: Marking crashy - " + JSON.stringify(instanceInfo));
    crashUtils.analyzeCrash(cmd,
                            instanceInfo,
                            options,
                            'execution of ' + cmd + ' - ' + instanceInfo.exitStatus.signal);
    if (options.coreCheck) {
      print(instanceInfo.exitStatus.gdbHint);
    }
    serverCrashedLocal = true;
  }

  if (instanceInfo.exitStatus.status === 'TERMINATED') {
    const color = (instanceInfo.exitStatus.exit === 0 ? GREEN : RED);

    print(color + 'Finished: ' + instanceInfo.exitStatus.status +
      ' exit code: ' + instanceInfo.exitStatus.exit +
      ' Time elapsed: ' + deltaTime + RESET);

    if (instanceInfo.exitStatus.exit === 0) {
      return {
        status: true,
        message: '',
        duration: deltaTime
      };
    } else {
      return {
        status: false,
        message: 'exit code was ' + instanceInfo.exitStatus.exit,
        duration: deltaTime
      };
    }
  } else if (instanceInfo.exitStatus.status === 'ABORTED') {
    if (typeof (instanceInfo.exitStatus.errorMessage) !== 'undefined') {
      errorMessage += instanceInfo.exitStatus.errorMessage;
    }

    print('Finished: ' + instanceInfo.exitStatus.status +
      ' Signal: ' + instanceInfo.exitStatus.signal +
      ' Time elapsed: ' + deltaTime + errorMessage);

    return {
      status: false,
      message: 'irregular termination: ' + instanceInfo.exitStatus.status +
        ' exit signal: ' + instanceInfo.exitStatus.signal + errorMessage,
      duration: deltaTime
    };
  } else {
    if (typeof (instanceInfo.exitStatus.errorMessage) !== 'undefined') {
      errorMessage += instanceInfo.exitStatus.errorMessage;
    }

    print('Finished: ' + instanceInfo.exitStatus.status +
      ' exit code: ' + instanceInfo.exitStatus.signal +
      ' Time elapsed: ' + deltaTime + errorMessage);

    return {
      status: false,
      message: 'irregular termination: ' + instanceInfo.exitStatus.status +
        ' exit code: ' + instanceInfo.exitStatus.exit + errorMessage,
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

function runArangoshCmd (options, instanceInfo, addArgs, cmds, coreCheck = false) {
  let args = makeArgsArangosh(options);
  args['server.endpoint'] = instanceInfo.endpoint;

  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }

  internal.env.INSTANCEINFO = JSON.stringify(instanceInfo);
  const argv = toArgv(args).concat(cmds);
  return executeAndWait(ARANGOSH_BIN, argv, options, 'arangoshcmd', instanceInfo.rootDir, false, coreCheck);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangoimport
// //////////////////////////////////////////////////////////////////////////////

function runArangoImport (options, instanceInfo, what, coreCheck = false) {
  let args = {
    'log.foreground-tty': 'true',
    'server.username': options.username,
    'server.password': options.password,
    'server.endpoint': instanceInfo.endpoint,
    'file': fs.join(TOP_DIR, what.data),
    'collection': what.coll,
    'type': what.type,
    'on-duplicate': what.onDuplicate || 'error',
    'ignore-missing': what.ignoreMissing || false
  };

  if (what.skipLines !== undefined) {
    args['skip-lines'] = what.skipLines;
  }

  if (what.create !== undefined) {
    args['create-collection'] = what.create;
  }

  if (what.createDatabase !== undefined) {
    args['create-database'] = what.createDatabase;
  }

  if (what.database !== undefined) {
    args['server.database'] = what.database;
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

  return executeAndWait(ARANGOIMPORT_BIN, toArgv(args), options, 'arangoimport', instanceInfo.rootDir, false, coreCheck);
}


// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangodump or arangorestore based on config object
// //////////////////////////////////////////////////////////////////////////////

function runArangoDumpRestoreCfg (config, options, rootDir, coreCheck) {
  if (options.extremeVerbosity === true) {
    config.print();
  }
  return executeAndWait(config.getExe(), config.toArgv(), options, 'arangorestore', rootDir, false, coreCheck);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangodump or arangorestore
// //////////////////////////////////////////////////////////////////////////////

function runArangoDumpRestore (options, instanceInfo, which, database, rootDir, dumpDir = 'dump', includeSystem = true, coreCheck = false) {
  const cfg = createBaseConfigBuilder(which, options, instanceInfo, database);
  cfg.setIncludeSystem(includeSystem);
  if (rootDir) { cfg.setRootDir(rootDir); } 

  if (which === 'dump') {
    cfg.setOutputDirectory(dumpDir);
  } else {
    cfg.setInputDirectory(dumpDir, true);
  }

  if (options.encrypted) {
    cfg.activateEncryption();
  }
  return runArangoDumpRestoreCfg(cfg, options, rootDir, coreCheck);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangobench
// //////////////////////////////////////////////////////////////////////////////

function runArangoBenchmark (options, instanceInfo, cmds, rootDir, coreCheck = false) {
  let args = {
    'configuration': fs.join(CONFIG_DIR, 'arangobench.conf'),
    'log.foreground-tty': 'true',
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

  return executeAndWait(ARANGOBENCH_BIN, toArgv(args), options, 'arangobench', instanceInfo.rootDir, false, coreCheck);
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
  const ret = res.status === 'RUNNING' && crashUtils.checkMonitorAlive(ARANGOD_BIN, arangod, options, res);

  if (!ret) {
    print('ArangoD with PID ' + arangod.pid + ' gone:');
    if (!arangod.hasOwnProperty('exitStatus')) {
      arangod.exitStatus = res;
    }
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
      serverCrashedLocal = true;
      print("checkArangoAlive: Marking crashy - " + JSON.stringify(arangod));
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
// / @brief on linux get a statistic about the sockets we used
// //////////////////////////////////////////////////////////////////////////////

function getSockStat(arangod, options, preamble) {
  if (options.getSockStat && (platform === 'linux')) {
    let sockStat = preamble + arangod.pid + "\n";
    try {
      sockStat += fs.read("/proc/" + arangod.pid + "/net/sockstat");
      return sockStat;
    }
    catch (e) {/* oops, process already gone? don't care. */ }
  }
  return "";
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
  if ((!arangod.hasOwnProperty('exitStatus')) ||
      (arangod.exitStatus.status === 'RUNNING')) {
    if (forceTerminate) {
      let sockStat = getSockStat(arangod, options, "Force killing - sockstat before: ");
      arangod.exitStatus = killExternal(arangod.pid, abortSignal);
      analyzeServerCrash(arangod, options, 'shutdown timeout; instance forcefully KILLED because of fatal timeout in testrun ' + sockStat);
    } else if (options.useKillExternal) {
      let sockStat = getSockStat(arangod, options, "Shutdown by kill - sockstat before: ");
      arangod.exitStatus = killExternal(arangod.pid);
      print(sockStat);
    } else {
      const requestOptions = makeAuthorizationHeaders(options);
      requestOptions.method = 'DELETE';
      print(Date() + ' ' + arangod.url + '/_admin/shutdown');
      let sockStat = getSockStat(arangod, options, "Sock stat for: ");
      const reply = download(arangod.url + '/_admin/shutdown', '', requestOptions);
      if ((reply.code !== 200) && // if the server should reply, we expect 200 - if not:
          !((reply.code === 500) &&
            (
              (reply.message === "Connection closed by remote") || // http connection
              reply.message.includes('failed with #111')           // https connection
            ))) {
        serverCrashedLocal = true;
        print(Date() + ' Wrong shutdown response: ' + JSON.stringify(reply) + "' " + sockStat + " continuing with hard kill!");
        shutdownArangod(arangod, options, true);
      }
      else {
        print(sockStat);
      }
      if (options.extremeVerbosity) {
        print(Date() + ' Shutdown response: ' + JSON.stringify(reply));
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

  if (!forceTerminate) {
    try {
      // send a maintenance request to any of the coordinators, so that
      // no failed server/failed follower jobs will be started on shutdown
      let coords = instanceInfo.arangods.filter(arangod =>
                                                arangod.role === 'coordinator' &&
                                                !arangod.hasOwnProperty('exitStatus'));
      if (coords.length > 0) {
        let requestOptions = makeAuthorizationHeaders(options);
        requestOptions.method = 'PUT';

        print(coords[0].url + "/_admin/cluster/maintenance");
        download(coords[0].url + "/_admin/cluster/maintenance", JSON.stringify("on"), requestOptions);
      }
    } catch (err) {
      print("error while setting cluster maintenance mode:", err);
    }
  }

  // Shut down all non-agency servers:
  const n = instanceInfo.arangods.length;

  let toShutdown = instanceInfo.arangods.slice();
  toShutdown.sort((a, b) => {
    if (a.role === b.role) return 0;
    if (a.role === 'coordinator' &&
        b.role === 'dbserver') return -1;
    if (b.role === 'coordinator' &&
        a.role === 'dbserver') return 1;
    if (a.role === 'agent') return 1;
    if (b.role === 'agent') return -1;
    return 0;
  });
  print('Shutdown order ' + JSON.stringify(toShutdown));

  let nonAgenciesCount = instanceInfo.arangods
      .filter(arangod => {
        if (arangod.hasOwnProperty('exitStatus') && 
            (arangod.exitStatus.status !== 'RUNNING')) {
          return false;
        }
        return (arangod.role !== 'agent');
      }).length;

  let timeout = 666;
  if (options.valgrind) {
    timeout *= 10;
  }
  if (options.sanitizer) {
    timeout *= 2;
  }

  var shutdownTime = internal.time();
  while (toShutdown.length > 0) {
    toShutdown = toShutdown.filter(arangod => {
      if (!arangod.hasOwnProperty('exitStatus')) {
        if ((nonAgenciesCount > 0) && (arangod.role === 'agent')) {
          return true;
        }
        shutdownArangod(arangod, options, forceTerminate);
        if (forceTerminate) {
          print("FORCED shut down: " + JSON.stringify(arangod));
        } else {
          arangod.exitStatus = {
            status: 'RUNNING'
          };
          print("Commanded shut down: " + JSON.stringify(arangod));
        }
        return true;
      }
      if (arangod.exitStatus.status === 'RUNNING') {
        arangod.exitStatus = statusExternal(arangod.pid, false);
        crashUtils.checkMonitorAlive(ARANGOD_BIN, arangod, options, arangod.exitStatus);
      }
      if (arangod.exitStatus.status === 'RUNNING') {
        let localTimeout = timeout;
        if (arangod.role === 'agent') {
          localTimeout = localTimeout + 60;
        }
        if ((internal.time() - shutdownTime) > localTimeout) {
          print('forcefully terminating ' + yaml.safeDump(arangod) +
                ' after ' + timeout + 's grace period; marking crashy.');
          serverCrashedLocal = true;
          arangod.exitStatus = killExternal(arangod.pid, abortSignal);
          analyzeServerCrash(arangod,
                             options,
                             'shutdown timeout; instance "' +
                             arangod.role +
                             '" forcefully KILLED after 60s - ' +
                             arangod.exitStatus.signal);
          if (arangod.role !== 'agent') {
            nonAgenciesCount --;
          }
          return false;
        } else {
          return true;
        }
      } else if (arangod.exitStatus.status !== 'TERMINATED') {
        if (arangod.role !== 'agent') {
          nonAgenciesCount --;
        }
        if (arangod.exitStatus.hasOwnProperty('signal') || arangod.exitStatus.hasOwnProperty('monitor')) {
          analyzeServerCrash(arangod, options, 'instance "' + arangod.role + '" Shutdown - ' + arangod.exitStatus.signal);
          print("shutdownInstance: Marking crashy - " + JSON.stringify(arangod));
          serverCrashedLocal = true;
        }
        stopProcdump(options, arangod);
      } else {
        if (arangod.role !== 'agent') {
          nonAgenciesCount --;
        }
        print('Server "' + arangod.role + '" shutdown: Success: pid', arangod.pid);
        stopProcdump(options, arangod);
        return false;
      }
    });
    if (toShutdown.length > 0) {
      let roles = {};
      toShutdown.forEach(arangod => { 
        if (!roles.hasOwnProperty(arangod.role)) {
          roles[arangod.role] = 0;
        } 
        ++roles[arangod.role]; 
      });
      let roleNames = [];
      for (let r in roles) {
        // e.g. 2 + coordinator + (s)
        roleNames.push(roles[r] + ' ' + r + '(s)');
      }
      print(roleNames.join(', ') + ' are still running...');
      require('internal').wait(1, false);
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
  if (options.cluster && options.activefailover ||
     !options.cluster && !options.activefailover) {
    throw "invalid call to startInstanceCluster";
  }

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
  let agencyUrl = instanceInfo.url;
  if (!checkInstanceAlive(instanceInfo, options)) {
    throw new Error('startup of agency failed! bailing out!');
  }

  let i;
  if (options.cluster) {
    for (i = 0; i < options.dbServers; i++) {
      let port = findFreePort(options.minPort, options.maxPort, usedPorts);
      usedPorts.push(port);
      let endpoint = protocol + '://127.0.0.1:' + port;
      let primaryArgs = _.clone(options.extraArgs);
      primaryArgs['server.endpoint'] = endpoint;
      primaryArgs['cluster.my-address'] = endpoint;
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
      coordinatorArgs['cluster.my-role'] = 'COORDINATOR';
      coordinatorArgs['cluster.agency-endpoint'] = agencyEndpoint;

      startInstanceSingleServer(instanceInfo, protocol, options, ...makeArgs('coordinator' + i, 'coordinator', coordinatorArgs), 'coordinator');
    }
  } else if (options.activefailover) {
    for (i = 0; i < options.singles; i++) {
      let port = findFreePort(options.minPort, options.maxPort, usedPorts);
      usedPorts.push(port);
      let endpoint = protocol + '://127.0.0.1:' + port;
      let singleArgs = _.clone(options.extraArgs);
      singleArgs['server.endpoint'] = endpoint;
      singleArgs['cluster.my-address'] = endpoint;
      singleArgs['cluster.my-role'] = 'SINGLE';
      singleArgs['cluster.agency-endpoint'] = agencyEndpoint;
      singleArgs['replication.active-failover'] = true;
      startInstanceSingleServer(instanceInfo, protocol, options, ...makeArgs('single' + i, 'single', singleArgs), 'single');
      sleep(1.0);
    }
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
          if (!arangod.hasOwnProperty('exitStatus') ||
              (arangod.exitStatus.status === 'RUNNING')) {
            arangod.exitStatus = killExternal(arangod.pid, abortSignal);
          }
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
        arangod.exitStatus = killExternal(arangod.pid, abortSignal);
        analyzeServerCrash(arangod, options, 'startup timeout; forcefully terminating ' + arangod.role + ' with pid: ' + arangod.pid);
      });
      throw new Error('cluster startup timed out after 10 minutes!');
    }
  }

  // we need to find the leading server
  if (options.activefailover) {
    internal.wait(5.0, false);
    let opts = {
      method: 'POST',
      jwt: crypto.jwtEncode(authOpts['server.jwt-secret'], {'server_id': 'none', 'iss': 'arangodb'}, 'HS256'),
      headers: {'content-type': 'application/json' }
    };
    let reply = download(agencyUrl + '/_api/agency/read', '[["/arango/Plan/AsyncReplication/Leader"]]', opts);

    if (!reply.error && reply.code === 200) {
      let res = JSON.parse(reply.body);
      //internal.print("Response ====> " + reply.body);
      let leader = res[0].arango.Plan.AsyncReplication.Leader;
      if (!leader) {
        throw "Leader is not selected";
      }
    }

    opts['method'] = 'GET';
    reply = download(instanceInfo.url + '/_api/cluster/endpoints', '', opts);
    let res = JSON.parse(reply.body);
    let leader = res.endpoints[0].endpoint;
    instanceInfo.arangods.forEach(d => {
      if (d.endpoint === leader) {
        instanceInfo.endpoint = d.endpoint;
        instanceInfo.url = d.url;
      }
    });
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

  if (platform.substr(0, 3) === 'win' && !options.disableMonitor) {
    runProcdump(options, instanceInfo, rootDir, instanceInfo.pid);
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
    arangods: [],
    protocol: protocol
  };

  const startTime = time();
  try {
    if (options.hasOwnProperty('server')) {
      let rc = { 
                 endpoint: options.server,
                 rootDir: options.serverRoot,
                 url: options.server.replace('tcp', 'http'),
                 arangods: []
               };
      arango.reconnect(rc.endpoint, '_system', 'root', '');
      return rc;
    } else if (options.cluster || options.activefailover) {
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
          wait(0.5, false);
          if (options.useReconnect) {
            try {
              arango.reconnect(instanceInfo.endpoint,
                               '_system',
                               options.username,
                               options.password,
                               count > 50
                              );
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
exports.stopProcdump = stopProcdump;

exports.createBaseConfig = createBaseConfigBuilder;
exports.run = {
  arangoshCmd: runArangoshCmd,
  arangoImport: runArangoImport,
  arangoDumpRestore: runArangoDumpRestore,
  arangoDumpRestoreWithConfig: runArangoDumpRestoreCfg,
  arangoBenchmark: runArangoBenchmark
};

exports.shutdownInstance = shutdownInstance;
exports.startArango = startArango;
exports.startInstance = startInstance;
exports.setupBinaries = setupBinaries;
exports.executableExt = executableExt;
exports.serverCrashed = serverCrashedLocal;
exports.serverFailMessages = serverFailMessagesLocal;

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
Object.defineProperty(exports, 'serverCrashed', {get: () => serverCrashedLocal, set: (value) => { serverCrashedLocal = value; } });
Object.defineProperty(exports, 'serverFailMessages', {get: () => serverFailMessagesLocal, set: (value) => { serverFailMessagesLocal = value; }});
