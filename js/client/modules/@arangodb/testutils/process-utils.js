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
const rp = require('@arangodb/testutils/result-processing');
const yaml = require('js-yaml');
const internal = require('internal');
const crashUtils = require('@arangodb/testutils/crash-utils');
const crypto = require('@arangodb/crypto');
const ArangoError = require('@arangodb').ArangoError;
const debugGetFailurePoints = require('@arangodb/test-helper').debugGetFailurePoints;

/* Functions: */
const toArgv = internal.toArgv;
const executeExternal = internal.executeExternal;
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
    case 'import':
      this.config.configuration = fs.join(CONFIG_DIR, 'arangoimport.conf');
      this.executable = ARANGOIMPORT_BIN;
      break;
    default:
      throw 'Sorry this type of Arango-Binary is not yet implemented: ' + type;
    }
  }

  setWhatToImport(what) {
    this.config['file'] = fs.join(TOP_DIR, what.data);
    this.config['collection'] = what.coll;
    this.config['type'] = what.type;
    this.config['on-duplicate'] = what.onDuplicate || 'error';
    this.config['ignore-missing'] = what.ignoreMissing || false;
    if (what.headers !== undefined) {
      this.config['headers-file'] = fs.join(TOP_DIR, what.headers);
    }

    if (what.skipLines !== undefined) {
      this.config['skip-lines'] = what.skipLines;
    }

    if (what.create !== undefined) {
      this.config['create-collection'] = what.create;
    }

    if (what.createDatabase !== undefined) {
      this.config['create-database'] = what.createDatabase;
    }

    if (what.database !== undefined) {
      this.config['server.database'] = what.database;
    }

    if (what.backslash !== undefined) {
      this.config['backslash-escape'] = what.backslash;
    }

    if (what.separator !== undefined) {
      this.config['separator'] = what.separator;
    }

    if (what.convert !== undefined) {
      this.config['convert'] = what.convert ? 'true' : 'false';
    }

    if (what.removeAttribute !== undefined) {
      this.config['remove-attribute'] = what.removeAttribute;
    }

    if (what.datatype !== undefined) {
      this.config['datatype'] = what.datatype;
    }

    if (what.mergeAttributes !== undefined) {
      this.config['merge-attributes'] = what.mergeAttributes;
    }

    if (what.batchSize !== undefined) {
      this.config['batch-size'] = what.batchSize;
    }

    if (what["from-collection-prefix"] !== undefined) {
      this.config["from-collection-prefix"] = what["from-collection-prefix"];
    }
    if (what["to-collection-prefix"] !== undefined) {
      this.config["to-collection-prefix"] = what["to-collection-prefix"];
    }
    if (what["overwrite-collection-prefix"] !== undefined) {
      this.config["overwrite-collection-prefix"] = what["overwrite-collection-prefix"];
    }
  }

  setAuth(username, password) {
    if (username !== undefined) {
      this.config['server.username'] = username;
    }
    if (password !== undefined) {
      this.config['server.password'] = password;
    }
  }
  setEndpoint(endpoint) { this.config['server.endpoint'] = endpoint; }
  setDatabase(database) {
    if (this.haveSetAllDatabases()) {
      throw new Error("must not specify all-databases and database");
    }
    this.config['server.database'] = database;
  }
  setThreads(threads) {
    if (this.type !== 'restore' && this.type !== 'dump') {
      throw '"threads" is not supported for binary: ' + this.type;
    }
    this.config['threads'] = threads;
  }
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
  getOutputDirectory() {
    return this.config['output-directory'];
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
  setJwtFile(file) {
    delete this.config['server.password'];
    this.config['server.jwt-secret-keyfile'] = file;
  }
  setMaskings(dir) {
    if (this.type !== 'dump') {
      throw '"maskings" is not supported for binary: ' + this.type;
    }
    this.config['maskings'] = fs.join(TOP_DIR, "tests/js/common/test-data/maskings", dir);
  }
  activateEncryption() { this.config['encryption.keyfile'] = fs.join(this.rootDir, 'secret-key'); }
  activateCompression() {
    if (this.type === 'dump') {
      this.config['--compress-output'] = true;
    }
  }
  deactivateCompression() {
    if (this.type === 'dump') {
      this.config['--compress-output'] = false;
    }
  }
  activateEnvelopes() {
    if (this.type === 'dump') {
      this.config['--envelope'] = true;
    }
  }
  deactivateEnvelopes() {
    if (this.type === 'dump') {
      this.config['--envelope'] = false;
    }
  }
  setRootDir(dir) { this.rootDir = dir; }
  restrictToCollection(collection) {
    if (this.type !== 'restore' && this.type !== 'dump') {
      throw '"collection" is not supported for binary: ' + this.type;
    }
    this.config['collection'] = collection;
  };
  setAllDatabases() {
    this.config['all-databases'] = 'true';
    delete this.config['server.database'];
  }
  haveSetAllDatabases() {
    return this.config.hasOwnProperty('all-databases');
  }
  activateFailurePoint() {
    if (this.type !== "restore") {
      throw '"activateFailurePoint" is not supported for binary: ' + this.type;
    }
    this.config['fail-after-update-continue-file'] = 'true';
  }
  enableContinue() {
    if (this.type !== "restore") {
      throw '"enableContinue" is not supported for binary: ' + this.type;
    }
    this.config['continue'] = 'true';
  }
  deactivateFailurePoint() {
    delete this.config['fail-after-update-continue-file'];
  }
  disableContinue() {
    delete this.config['continue'];
  }
  toArgv() { return internal.toArgv(this.config); }

  getExe() { return this.executable; }

  print() {
    print(this.executable);
    print(this.config);
  }
}

const createBaseConfigBuilder = function (type, options, instanceInfo, database = '_system') {
  const cfg = new ConfigBuilder(type);
  if (!options.jwtSecret) {
    cfg.setAuth(options.username, options.password);
  }
  if (options.hasOwnProperty('logForceDirect')) {
    cfg.config['log.force-direct'] = true;
  }
  if (options.hasOwnProperty('serverRequestTimeout')) {
    cfg.config['server.request-timeout'] = options.serverRequestTimeout;
  }
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
let isEnterpriseClient = false;

let BIN_DIR;
let ARANGOBACKUP_BIN;
let ARANGOBENCH_BIN;
let ARANGODUMP_BIN;
let ARANGOD_BIN;
let ARANGOIMPORT_BIN;
let ARANGORESTORE_BIN;
let ARANGOEXPORT_BIN;
let ARANGOSH_BIN;
let ARANGO_SECURE_INSTALLATION_BIN;
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
    !fs.exists('client-tools') && !fs.exists('tests')) {
    throw new Error('Must be in ArangoDB topdir to execute tests.');
  }

  return topDir;
}());

// create additional system environment variables for coverage
function coverageEnvironment () {
  let result = [];
  let name = 'GCOV_PREFIX';

  if (process.env.hasOwnProperty(name)) {
    result.push(
      name +
      "=" +
      process.env[name] +
      "/" +
      crypto.md5(String(internal.time() + Math.random())));
  }

  return result;
}

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

  if (!fs.exists(BIN_DIR)) {
    BIN_DIR = fs.join(TOP_DIR, 'bin');
  }

  UNITTESTS_DIR = fs.join(fs.join(builddir, 'tests'));
  if (!fs.exists(UNITTESTS_DIR)) {
    UNITTESTS_DIR = fs.join(TOP_DIR, UNITTESTS_DIR);
  }

  if (buildType !== '') {
    if (fs.exists(fs.join(BIN_DIR, buildType))) {
      BIN_DIR = fs.join(BIN_DIR, buildType);
    }
    if (fs.exists(fs.join(UNITTESTS_DIR, buildType))) {
      UNITTESTS_DIR = fs.join(UNITTESTS_DIR, buildType);
    }
  }

  ARANGOBACKUP_BIN = fs.join(BIN_DIR, 'arangobackup' + executableExt);
  ARANGOBENCH_BIN = fs.join(BIN_DIR, 'arangobench' + executableExt);
  ARANGODUMP_BIN = fs.join(BIN_DIR, 'arangodump' + executableExt);
  ARANGOD_BIN = fs.join(BIN_DIR, 'arangod' + executableExt);
  ARANGOIMPORT_BIN = fs.join(BIN_DIR, 'arangoimport' + executableExt);
  ARANGORESTORE_BIN = fs.join(BIN_DIR, 'arangorestore' + executableExt);
  ARANGOEXPORT_BIN = fs.join(BIN_DIR, 'arangoexport' + executableExt);
  ARANGOSH_BIN = fs.join(BIN_DIR, 'arangosh' + executableExt);
  ARANGO_SECURE_INSTALLATION_BIN = fs.join(BIN_DIR, 'arango-secure-installation' + executableExt);

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
    ARANGOSH_BIN
  ];

  if (global.ARANGODB_CLIENT_VERSION) {
    let version = global.ARANGODB_CLIENT_VERSION(true);
    if (version.hasOwnProperty('enterprise-version')) {
      isEnterpriseClient = true;
      checkFiles.push(ARANGOBACKUP_BIN);
    }
    ["asan", "ubsan", "lsan", "tsan"].forEach((san) => {
      let envName = san.toUpperCase() + "_OPTIONS";
      let fileName = san + "_arangodb_suppressions.txt";
      if (!process.env.hasOwnProperty(envName) &&
          fs.exists(fileName)) {
        // print('preparing ' + san + ' environment');
        process.env[envName] = `suppressions=${fs.join(fs.makeAbsolute(''), fileName)}`;
      }
    });
  }

  for (let b = 0; b < checkFiles.length; ++b) {
    if (!fs.isFile(checkFiles[b])) {
      throw new Error('unable to locate ' + checkFiles[b]);
    }
  }
  global.ARANGOSH_BIN = ARANGOSH_BIN;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief if we forgot about processes, this safe guard will clean up and mark failed
// //////////////////////////////////////////////////////////////////////////////

function killRemainingProcesses(results) {
  let running = internal.getExternalSpawned();
  results.status = results.status && (running.length === 0);
  let i = 0;
  for (i = 0; i < running.length; i++) {
    let timeoutReached = internal.SetGlobalExecutionDeadlineTo(0.0);
    if (timeoutReached) {
      print(RED + Date() + ' external deadline reached!' + RESET);
    }
    let status = internal.statusExternal(running[i].pid, false);
    if (status.status === "TERMINATED") {
      print("process exited without us joining it (marking crashy): " + JSON.stringify(running[i]) + JSON.stringify(status));
    }
    else {
      print("Killing remaining process & marking crashy: " + JSON.stringify(running[i]));
      print(killExternal(running[i].pid, abortSignal));
    }
    results.crashed = true;
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

  internal.env.INSTANCEINFO = JSON.stringify(instanceInfo.getStructure());
  const argv = toArgv(args).concat(cmds);
  return executeAndWait(ARANGOSH_BIN, argv, options, 'arangoshcmd', instanceInfo.rootDir, coreCheck);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangoimport
// //////////////////////////////////////////////////////////////////////////////

function runArangoImportCfg (config, options, rootDir, coreCheck = false) {
  if (options.extremeVerbosity === true) {
    config.print();
  }
  return executeAndWait(config.getExe(), config.toArgv(), options, 'arangoimport', rootDir, coreCheck);
}


// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangodump or arangorestore based on config object
// //////////////////////////////////////////////////////////////////////////////

function runArangoDumpRestoreCfg (config, options, rootDir, coreCheck) {
  if (options.extremeVerbosity === true) {
    config.print();
  }
  return executeAndWait(config.getExe(), config.toArgv(), options, 'arangorestore', rootDir, coreCheck);
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
  if (options.allDatabases) {
    cfg.setAllDatabases();
  }
  return runArangoDumpRestoreCfg(cfg, options, rootDir, coreCheck);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangobench
// //////////////////////////////////////////////////////////////////////////////

function runArangoBackup (options, instanceInfo, which, cmds, rootDir, coreCheck = false) {
  let args = {
    'configuration': fs.join(CONFIG_DIR, 'arangobackup.conf'),
    'log.foreground-tty': 'true',
    'server.endpoint': instanceInfo.endpoint,
    'server.connection-timeout': 10 // 5s default
  };
  if (options.username) {
    args['server.username'] = options.username;
    args['server.password'] = "";
  }
  if (options.password) {
    args['server.password'] = options.password;
  }

  args = Object.assign(args, cmds);

  args['log.level'] = 'info';
  if (!args.hasOwnProperty('verbose')) {
    args['log.level'] = 'warning';
  }
  if (options.extremeVerbosity) {
    args['log.level'] = 'trace';
  }

  args['flatCommands'] = [which];

  return executeAndWait(ARANGOBACKUP_BIN, toArgv(args), options, 'arangobackup', instanceInfo.rootDir, coreCheck);
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

  return executeAndWait(ARANGOBENCH_BIN, toArgv(args), options, 'arangobench', instanceInfo.rootDir, coreCheck);
}

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
    let jwt = crypto.jwtEncode(jwtSecret,
                             {'server_id': 'none',
                              'iss': 'arangodb'}, 'HS256');
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

function executeAndWait (cmd, args, options, valgrindTest, rootDir, coreCheck = false, timeout = 0) {
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
    print(Date() + ' executeAndWait: cmd =', cmd, 'args =', args);
  }

  const startTime = time();
  if ((typeof (cmd) !== 'string') || (cmd === 'true') || (cmd === 'false')) {
    return {
      timeout: false,
      status: false,
      message: 'true or false as binary name for test cmd =' + cmd + 'args =' + args
    };
  }

  let instanceInfo = {
    rootDir: rootDir,
    pid: 0,
    exitStatus: {},
    getStructure: function() { return {}; }
  };

  let res = {};
  if (platform.substr(0, 3) === 'win' && !options.disableMonitor) {
    res = executeExternal(cmd, args, false, coverageEnvironment());
    instanceInfo.pid = res.pid;
    instanceInfo.exitStatus = res;
    if (crashUtils.runProcdump(options, instanceInfo, rootDir, res.pid)) {
      Object.assign(instanceInfo.exitStatus,
                    statusExternal(res.pid, true, timeout * 1000));
      if (instanceInfo.exitStatus.status === 'TIMEOUT') {
        print('Timeout while running ' + cmd + ' - will kill it now! ' + JSON.stringify(args));
        executeExternalAndWait('netstat', ['-aonb']);
        killExternal(res.pid);
        crashUtils.stopProcdump(options, instanceInfo);
        instanceInfo.exitStatus.status = 'ABORTED';
        const deltaTime = time() - startTime;
        return {
          timeout: true,
          status: false,
          message: 'irregular termination by TIMEOUT',
          duration: deltaTime
        };
      }
      crashUtils.stopProcdump(options, instanceInfo);
    } else {
      print('Killing ' + cmd + ' - ' + JSON.stringify(args));
      res = killExternal(res.pid);
      instanceInfo.pid = res.pid;
      instanceInfo.exitStatus = res;
    }
  } else {
    // V8 executeExternalAndWait thinks that timeout is in ms, so *1000
    res = executeExternalAndWait(cmd, args, false, timeout*1000, coverageEnvironment());
    instanceInfo.pid = res.pid;
    instanceInfo.exitStatus = res;
    crashUtils.calculateMonitorValues(options, instanceInfo, res.pid, cmd);
  }
  const deltaTime = time() - startTime;

  let errorMessage = ' - ';

  if (coreCheck &&
      instanceInfo.exitStatus.hasOwnProperty('signal') &&
      ((instanceInfo.exitStatus.signal === 11) ||
       (instanceInfo.exitStatus.signal === 6) ||
       (instanceInfo.exitStatus.signal === 4) || // mac sometimes SIG_ILLs...
       // Windows sometimes has random numbers in signal...
       (platform.substr(0, 3) === 'win')
      )
     ) {
    print(Date() + " executeAndWait: Marking crashy - " + JSON.stringify(instanceInfo));
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
        message: 'exit code was ' + instanceInfo.exitStatus.exit,
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
      message: 'irregular termination: ' + instanceInfo.exitStatus.status +
        ' exit signal: ' + instanceInfo.exitStatus.signal + errorMessage,
      duration: deltaTime
    };
  } else if (res.status === 'TIMEOUT') {
    print('Killing ' + cmd + ' - ' + JSON.stringify(args));
    let resKill = killExternal(res.pid, abortSignal);
    if (coreCheck) {
      print(Date() + " executeAndWait: Marking crashy because of timeout - " + JSON.stringify(instanceInfo));
      crashUtils.analyzeCrash(cmd,
                              instanceInfo,
                              options,
                              'execution of ' + cmd + ' - kill because of timeout');
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
      message: 'termination by timeout: ' + instanceInfo.exitStatus.status +
        ' killed by : ' + abortSignal + errorMessage,
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
      message: 'irregular termination: ' + instanceInfo.exitStatus.status +
        ' exit code: ' + instanceInfo.exitStatus.exit + errorMessage,
      duration: deltaTime
    };
  }
}

exports.setupBinaries = setupBinaries;
exports.endpointToURL = endpointToURL;
exports.coverageEnvironment = coverageEnvironment;

exports.makeArgs = {
  arangosh: makeArgsArangosh
};

exports.executeAndWait = executeAndWait;
exports.killRemainingProcesses = killRemainingProcesses;
exports.isEnterpriseClient = isEnterpriseClient;

exports.createBaseConfig = createBaseConfigBuilder;
exports.run = {
  arangoshCmd: runArangoshCmd,
  arangoImport: runArangoImportCfg,
  arangoDumpRestore: runArangoDumpRestore,
  arangoDumpRestoreWithConfig: runArangoDumpRestoreCfg,
  arangoBenchmark: runArangoBenchmark,
  arangoBackup: runArangoBackup
};

exports.executableExt = executableExt;

exports.makeAuthorizationHeaders = makeAuthorizationHeaders;
Object.defineProperty(exports, 'ARANGOBACKUP_BIN', {get: () => ARANGOBACKUP_BIN});
Object.defineProperty(exports, 'ARANGOBENCH_BIN', {get: () => ARANGOBENCH_BIN});
Object.defineProperty(exports, 'ARANGODUMP_BIN', {get: () => ARANGODUMP_BIN});
Object.defineProperty(exports, 'ARANGOD_BIN', {get: () => ARANGOD_BIN});
Object.defineProperty(exports, 'ARANGOEXPORT_BIN', {get: () => ARANGOEXPORT_BIN});
Object.defineProperty(exports, 'ARANGOIMPORT_BIN', {get: () => ARANGOIMPORT_BIN});
Object.defineProperty(exports, 'ARANGORESTORE_BIN', {get: () => ARANGORESTORE_BIN});
Object.defineProperty(exports, 'ARANGOSH_BIN', {get: () => ARANGOSH_BIN});
Object.defineProperty(exports, 'ARANGO_SECURE_INSTALLATION_BIN', {get: () => ARANGO_SECURE_INSTALLATION_BIN});
Object.defineProperty(exports, 'CONFIG_DIR', {get: () => CONFIG_DIR});
Object.defineProperty(exports, 'TOP_DIR', {get: () => TOP_DIR});
Object.defineProperty(exports, 'LOGS_DIR', {get: () => LOGS_DIR});
Object.defineProperty(exports, 'UNITTESTS_DIR', {get: () => UNITTESTS_DIR});
Object.defineProperty(exports, 'BIN_DIR', {get: () => BIN_DIR});
Object.defineProperty(exports, 'CONFIG_ARANGODB_DIR', {get: () => CONFIG_ARANGODB_DIR});
Object.defineProperty(exports, 'CONFIG_RELATIVE_DIR', {get: () => CONFIG_RELATIVE_DIR});
Object.defineProperty(exports, 'serverCrashed', {get: () => serverCrashedLocal, set: (value) => { serverCrashedLocal = value; } });
Object.defineProperty(exports, 'serverFailMessages', {get: () => serverFailMessagesLocal, set: (value) => { serverFailMessagesLocal = value; }});
