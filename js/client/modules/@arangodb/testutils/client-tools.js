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

const internal = require('internal');
const pu = require('@arangodb/testutils/process-utils');
const fs = require('fs');


/* Functions: */
const toArgv = internal.toArgv;

/* Constants: */
// const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
// const YELLOW = internal.COLORS.COLOR_YELLOW;

let logCounter = 0;

class ConfigBuilder {
  constructor(type) {
    this.config = {
      'log.foreground-tty': 'true'
    };
    this.type = type;
    switch (type) {
    case 'restore':
      this.config.configuration = fs.join(pu.CONFIG_DIR, 'arangorestore.conf');
      this.executable = pu.ARANGORESTORE_BIN;
      this.logprefix = 'restore';
      break;
    case 'dump':
      this.config.configuration = fs.join(pu.CONFIG_DIR, 'arangodump.conf');
      this.executable = pu.ARANGODUMP_BIN;
      this.logprefix = 'dump';
      break;
    case 'import':
      this.config.configuration = fs.join(pu.CONFIG_DIR, 'arangoimport.conf');
      this.executable = pu.ARANGOIMPORT_BIN;
      this.logprefix = 'import';
      break;
    default:
      throw 'Sorry this type of Arango-Binary is not yet implemented: ' + type;
    }
  }

  setWhatToImport(what) {
    this.config['file'] = fs.join(pu.TOP_DIR, what.data);
    this.config['collection'] = what.coll;
    this.config['type'] = what.type;
    this.config['on-duplicate'] = what.onDuplicate || 'error';
    this.config['ignore-missing'] = what.ignoreMissing || false;
    if (what.headers !== undefined) {
      this.config['headers-file'] = fs.join(pu.TOP_DIR, what.headers);
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

  calculateLogFile() {
    this.config['log.file'] = fs.join(fs.getTempPath(), `${this.logprefix}_${logCounter}.log`);
    logCounter += 1;
  }

  getLogFile() {
    return fs.read(this.config['log.file']);
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
  hasJwt() {
    return this.config.hasOwnProperty('server.jwt-secret-keyfile');
  }
  setMaskings(dir) {
    if (this.type !== 'dump') {
      throw '"maskings" is not supported for binary: ' + this.type;
    }
    this.config['maskings'] = fs.join(pu.TOP_DIR, "tests/js/common/test-data/maskings", dir);
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
  setUseParallelDump(value = true) {
    if (this.type === 'dump') {
      this.config['parallel-dump'] = value;
    }
  }
  setUseSplitFiles(value = false) {
    if (this.type === 'dump') {
      this.config['split-files'] = value;
    }
  }
  setRootDir(dir) { this.rootDir = dir; }
  restrictToCollection(collection) {
    if (this.type !== 'restore' && this.type !== 'dump') {
      throw '"collection" is not supported for binary: ' + this.type;
    }
    this.config['collection'] = collection;
  }
  resetAllDatabases() {
    delete this.config['all-databases'];
  }
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

// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////
// / operate the arango commandline utilities
// //////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////

function makeArgsArangosh (options) {
  return {
    'configuration': fs.join(pu.CONFIG_DIR, 'arangosh.conf'),
    'javascript.startup-directory': pu.JS_DIR,
    'javascript.module-directory': pu.JS_ENTERPRISE_DIR,
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
  return pu.executeAndWait(pu.ARANGOSH_BIN, argv, options, 'arangoshcmd', instanceInfo.rootDir, coreCheck);
}

function rtaMakedata(options, instanceManager, writeReadClean, msg, logFile, moreargv=[], addArgs=undefined) {
  let args = Object.assign(makeArgsArangosh(options), {
    'server.endpoint': instanceManager.findEndpoint(),
    'log.file': logFile,
    'log.level': ['warning', 'httpclient=debug', 'V8=debug'],
    'javascript.execute': [
        fs.join(options.rtasource, 'test_data', 'makedata.js'),
        fs.join(options.rtasource, 'test_data', 'checkdata.js'),
        fs.join(options.rtasource, 'test_data', 'cleardata.js')
    ][writeReadClean],
    'server.force-json': options.forceJson,
  });
  if (addArgs !== undefined) {
    args = Object.assign(args, addArgs);
  }
  let argv = toArgv(args);
  argv = argv.concat(['--'],
                     moreargv, [
                       '--minReplicationFactor', '2',
                       '--progress', 'true',
                       '--oldVersion', require('internal').db._version()
                     ]);
  if (options.rtaNegFilter !== '') {
    argv = argv.concat(['--skip', options.rtaNegFilter]);
  }
  if (options.forceOneShard) {
    argv = argv.concat(['--singleShard', 'true']);
  }
  if (options.hasOwnProperty('makedata_args')) {
    argv = argv.concat(toArgv(options['makedata_args']));
  }
  print('\n' + (new Date()).toISOString() + GREEN + " [============] Makedata : Trying " +
        args['javascript.execute'] + '\n ' + msg + ' ... ', RESET);
  if (options.extremeVerbosity !== 'silence') {
    print(argv);
  }
  return pu.executeAndWait(pu.ARANGOSH_BIN, argv, options, 'arangosh', instanceManager.rootDir, options.coreCheck, 60 * 15);
}
function rtaWaitShardsInSync(options, instanceManager) {
  let args = Object.assign(makeArgsArangosh(options), {
    'server.endpoint': instanceManager.findEndpoint(),
    'log.file': fs.join(fs.getTempPath(), `rta_wait_for_shards_in_sync.log`),
    'log.level': ['warning', 'httpclient=debug', 'V8=debug'],
    'server.force-json': options.forceJson,
    'javascript.execute': fs.join(options.rtasource, 'test_data','run_in_arangosh.js'),
  });
  let myargs = toArgv(args).concat([
    '--javascript.module-directory',
    fs.join(options.rtasource, 'test_data'),
    '--',
    fs.join(options.rtasource, 'test_data', 'tests', 'js', 'server', 'cluster', 'wait_for_shards_in_sync.js'),
    '--args',
    'true'
  ]);
  if (options.extremeVerbosity !== 'silence') {
    print(myargs);
  }
  let rc = pu.executeAndWait(pu.ARANGOSH_BIN, myargs, options, 'arangosh', instanceManager.rootDir, options.coreCheck);
}
// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangoimport
// //////////////////////////////////////////////////////////////////////////////

function runArangoImportCfg (config, options, rootDir, coreCheck = false) {
  if (options.extremeVerbosity === true) {
    config.print();
  }
  return pu.executeAndWait(config.getExe(), config.toArgv(), options, pu.ARANGO_IMPORT_BIN, rootDir, coreCheck);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangodump or arangorestore based on config object
// //////////////////////////////////////////////////////////////////////////////

function runArangoDumpRestoreCfg (config, options, rootDir, coreCheck) {
  config.calculateLogFile();
  if (options.extremeVerbosity === true) {
    config.print();
  }

  let ret = pu.executeAndWait(config.getExe(), config.toArgv(), options, pu.ARANGORESTORE_BIN, rootDir, coreCheck);
  ret.message += `\nContents of log file ${config['log.file']}:\n` + config.getLogFile();
  return ret;
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
    'configuration': fs.join(pu.CONFIG_DIR, 'arangobackup.conf'),
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

  return pu.executeAndWait(pu.ARANGOBACKUP_BIN, toArgv(args), options, 'arangobackup', instanceInfo.rootDir, coreCheck);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief runs arangobench
// //////////////////////////////////////////////////////////////////////////////

function runArangoBenchmark (options, instanceInfo, cmds, rootDir, coreCheck = false) {
  let args = {
    'configuration': fs.join(pu.CONFIG_DIR, 'arangobench.conf'),
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

  return pu.executeAndWait(pu.ARANGOBENCH_BIN, toArgv(args), options, 'arangobench', instanceInfo.rootDir, coreCheck);
}

exports.makeArgs = {
  arangosh: makeArgsArangosh
};

exports.createBaseConfig = createBaseConfigBuilder;
exports.run = {
  arangoshCmd: runArangoshCmd,
  arangoImport: runArangoImportCfg,
  arangoDumpRestore: runArangoDumpRestore,
  arangoDumpRestoreWithConfig: runArangoDumpRestoreCfg,
  arangoBenchmark: runArangoBenchmark,
  arangoBackup: runArangoBackup,
  rtaWaitShardsInSync: rtaWaitShardsInSync,
  rtaMakedata: rtaMakedata
};

