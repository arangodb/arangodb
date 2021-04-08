/* jshint strict: false, sub: true */
/* global print, arango */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// DISCLAIMER
//
// Copyright 2016-2019 ArangoDB GmbH, Cologne, Germany
// Copyright 2014 triagens GmbH, Cologne, Germany
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright holder is ArangoDB GmbH, Cologne, Germany
//
// @author Max Neunhoeffer
// @author Wilfried Goesgens
// @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'dump': 'dump tests',
  'dump_mixed_cluster_single': 'dump tests - dump cluster restore single',
  'dump_mixed_single_cluster': 'dump tests - dump single restore cluster',
  'dump_authentication': 'dump tests with authentication',
  'dump_jwt': 'dump tests with JWT',
  'dump_encrypted': 'encrypted dump tests',
  'dump_maskings': 'masked dump tests',
  'dump_multiple': 'restore multiple DBs at once',
  'dump_no_envelope': 'dump without data envelopes',
  'dump_with_crashes': 'restore and crash the client multiple times',
  'hot_backup': 'hotbackup tests'
};

const optionsDocumentation = [
  '   - `skipEncrypted` : if set to true the encryption tests are skipped'
];

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const fs = require('fs');
const _ = require('lodash');
const hb = require("@arangodb/hotbackup");
const sleep = require("internal").sleep;
const db = require("internal").db;
const platform = require("internal").platform;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const encryptionKey = '01234567890123456789012345678901';
const encryptionKeySha256 = "861009ec4d599fab1f40abc76e6f89880cff5833c79c548c99f9045f191cd90b";

let timeoutFactor = 1;
if (global.ARANGODB_CLIENT_VERSION(true).asan === 'true' ||
    global.ARANGODB_CLIENT_VERSION(true).tsan === 'true' ||
    (platform.substr(0, 3) === 'win')) {
  timeoutFactor = 8;
} else if (process.env.hasOwnProperty('GCOV_PREFIX')) {
    timeoutFactor = 16;
}

const testPaths = {
  'dump': [tu.pathForTesting('server/dump')],
  'dump_mixed_cluster_single': [tu.pathForTesting('server/dump')],
  'dump_mixed_single_cluster': [tu.pathForTesting('server/dump')],
  'dump_authentication': [tu.pathForTesting('server/dump')],
  'dump_jwt': [tu.pathForTesting('server/dump')],
  'dump_encrypted': [tu.pathForTesting('server/dump')],
  'dump_maskings': [tu.pathForTesting('server/dump')],
  'dump_multiple': [tu.pathForTesting('server/dump')],
  'dump_no_envelope': [tu.pathForTesting('server/dump')],
  'dump_with_crashes': [tu.pathForTesting('server/dump')],
  'hot_backup': [tu.pathForTesting('server/dump')]
};

class DumpRestoreHelper {
  constructor(firstRunOptions, secondRunOptions, serverOptions, clientAuth, dumpOptions, restoreOptions, which, afterServerStart) {
    this.serverOptions = serverOptions;
    this.instanceInfo = {};
    this.firstRunOptions = firstRunOptions;
    this.secondRunOptions = secondRunOptions;
    this.restartServer = firstRunOptions.cluster !== secondRunOptions.cluster;
    this.clientAuth = clientAuth;
    this.dumpOptions = dumpOptions;
    this.restoreOptions = restoreOptions;
    this.which = which;
    this.results = {failed: 1};
    this.dumpConfig = false;
    this.restoreConfig = false;
    this.restoreOldConfig = false;
    this.afterServerStart = afterServerStart;
  }

  bindInstanceInfo(options) {
    this.arangosh = tu.runInLocalArangosh.bind(this, options, this.instanceInfo);
    if (!this.dumpConfig) {
      // the dump config will only be configured for the first instance.
      this.dumpConfig = pu.createBaseConfig('dump', this.dumpOptions, this.instanceInfo);
      this.dumpConfig.setOutputDirectory('dump');
      this.dumpConfig.setIncludeSystem(true);
      if (this.dumpOptions.hasOwnProperty("maskings")) {
        this.dumpConfig.setMaskings(this.dumpOptions.maskings);
      }
      if (this.dumpOptions.allDatabases) {
        this.dumpConfig.setAllDatabases();
      }
    }
    if (this.dumpOptions.encrypted) {
      this.dumpConfig.activateEncryption();
    }
    if (this.dumpOptions.jwtSecret) {
      let keyDir = fs.join(fs.getTempPath(), 'jwtSecrets');
      if (!fs.exists(keyDir)) {  // needed on win32
        fs.makeDirectory(keyDir);
      }
      pu.cleanupDBDirectoriesAppend(keyDir);
      let keyFile = fs.join(keyDir, 'secret-for-dump');
      fs.write(keyFile, this.dumpOptions.jwtSecret);
      this.dumpConfig.setJwtFile(keyFile);
    }

    if (!this.restoreConfig) {
      this.restoreConfig = pu.createBaseConfig('restore', this.restoreOptions, this.instanceInfo);
      this.restoreConfig.setInputDirectory('dump', true);
      this.restoreConfig.setIncludeSystem(true);
    } else {
      this.restoreConfig.setEndpoint(this.instanceInfo.endpoint);
    }
    
    if (!this.restoreOldConfig) {
      this.restoreOldConfig = pu.createBaseConfig('restore', this.restoreOptions, this.instanceInfo);
      this.restoreOldConfig.setInputDirectory('dump', true);
      this.restoreOldConfig.setIncludeSystem(true);
      this.restoreOldConfig.setRootDir(pu.TOP_DIR);
    } else {
      this.restoreOldConfig.setEndpoint(this.instanceInfo.endpoint);
    }
    if (this.restoreOptions.jwtSecret) {
      let keyDir = fs.join(fs.getTempPath(), 'jwtSecrets');
      if (!fs.exists(keyDir)) {  // needed on win32
        fs.makeDirectory(keyDir);
      }
      pu.cleanupDBDirectoriesAppend(keyDir);
      let keyFile = fs.join(fs.getTempPath(), 'secret-for-restore');
      fs.write(keyFile, this.restoreOptions.jwtSecret);
      this.restoreConfig.setJwtFile(keyFile);
      this.restoreOldConfig.setJwtFile(keyFile);
    }
    this.setOptions();
    this.arangorestore = pu.run.arangoDumpRestoreWithConfig.bind(this, this.restoreConfig, this.restoreOptions, this.instanceInfo.rootDir, this.firstRunOptions.coreCheck);
    this.arangorestoreOld = pu.run.arangoDumpRestoreWithConfig.bind(this, this.restoreOldConfig, this.restoreOptions, this.instanceInfo.rootDir, this.firstRunOptions.coreCheck);
    this.arangodump = pu.run.arangoDumpRestoreWithConfig.bind(this, this.dumpConfig, this.dumpOptions, this.instanceInfo.rootDir, this.firstRunOptions.coreCheck);
    this.fn = this.afterServerStart(this.instanceInfo);
  }
  setOptions() {
    if (this.restoreOptions.encrypted) {
      this.restoreConfig.activateEncryption();
      this.restoreOldConfig.activateEncryption();
    }
    if (this.dumpOptions.compressed) {
      this.dumpConfig.activateCompression();
    }
    if (this.firstRunOptions.deactivateCompression) {
      this.dumpConfig.deactivateCompression();
    }
    if (this.firstRunOptions.deactivateEnvelopes) {
      this.dumpConfig.deactivateEnvelopes();
    }
    if (this.restoreOptions.allDatabases) {
      this.restoreConfig.setAllDatabases();
      this.restoreOldConfig.setAllDatabases();
    } else {
      this.restoreOldConfig.setDatabase('_system');
    }
    if (this.restoreOptions.activateFailurePoint) {
      print("Activating failure point");
      this.restoreConfig.activateFailurePoint();
      this.restoreOldConfig.deactivateFailurePoint();
    } else {
      this.restoreConfig.deactivateFailurePoint();
      this.restoreOldConfig.deactivateFailurePoint();
    }
  }
  print (s) {
    print(CYAN + Date() + ': ' + this.which + ' and Restore - ' + s + RESET);
  }

  startFirstInstance() {
    this.instanceInfo = pu.startInstance('tcp', this.firstRunOptions, this.serverOptions, this.which);

    if (this.instanceInfo === false) {
      let rc =  {
        failed: 1,
      };
      this.results = {
        failed: 1,
        [this.which]: {
          status: false,
          message: 'failed to start server!'
        }};
      return false;
    }
    this.bindInstanceInfo(this.firstRunOptions);
    return true;
  }

  restartInstance() {
    if (this.restartServer) {
      print(CYAN + 'Shutting down...' + RESET);
      this.results['shutdown'] = pu.shutdownInstance(this.instanceInfo, this.firstRunOptions);
      print(CYAN + 'done.' + RESET);
      this.which = this.which + "_2";
      this.instanceInfo = pu.startInstance('tcp', this.secondRunOptions, this.serverOptions, this.which);

      if (this.instanceInfo === false) {
        let rc =  {
          failed: 1,
        };
        this.results = {
          failed: 1,
          [this.which]: {
            status: false,
            message: 'failed to start the second server!'
          }};
        return false;
      }
      this.bindInstanceInfo(this.secondRunOptions);
      return true;

    }
    return true;
  }

  adjustRestoreToDump() {
    this.restoreOptions = this.dumpOptions;
    this.restoreConfig = pu.createBaseConfig('restore', this.dumpOptions, this.instanceInfo);
    this.arangorestore = pu.run.arangoDumpRestoreWithConfig.bind(this, this.restoreConfig, this.restoreOptions, this.instanceInfo.rootDir, this.firstRunOptions.coreCheck);
  }

  isAlive() {
    return pu.arangod.check.instanceAlive(this.instanceInfo, this.firstRunOptions);
  }

  getUptime() {
    try {
      return pu.arangod.check.uptime(this.instanceInfo, this.firstRunOptions);
    } catch (x) {
      print(x); // TODO
      print("uptime continuing anyways");
      return {};
    }
  }

  validate(phaseInfo) {
    phaseInfo.failed = (phaseInfo.status !== true || !this.isAlive() ? 1 : 0);
    return phaseInfo.failed === 0;
  }

  extractResults() {
    if (this.fn !== undefined) {
      fs.remove(this.fn);
    }
    if (this.instanceInfo === false) {
      print(RED + 'no instance running. Nothing to stop!' + RESET);
      return this.results;
    }
    print(CYAN + 'Shutting down...' + RESET);
    this.results['shutdown'] = pu.shutdownInstance(this.instanceInfo, this.firstRunOptions);
    print(CYAN + 'done.' + RESET);

    print();
    return this.results;
  }

  runSetupSuite(path) {
    this.print('Setting up - ' + path);
    this.results.setup = this.arangosh(path, this.clientAuth);
    return this.validate(this.results.setup);
  }

  runCheckDumpFilesSuite(path) {
    this.print('Inspecting dumped files - ' + path);
    process.env['dump-directory'] = this.dumpConfig.config['output-directory'];
    this.results.checkDumpFiles = this.arangosh(path, this.clientAuth);
    delete process.env['dump-directory'];
    return this.validate(this.results.checkDumpFiles);
  }

  runCleanupSuite(path) {
    this.print('Cleaning up - ' + path);
    this.results.cleanup = this.arangosh(path, this.clientAuth);
    return this.validate(this.results.cleanup);
  }

  dumpFrom(database, separateDir = false) {
    this.print('dump');
    if (separateDir) {
      if (!fs.exists(fs.join(this.instanceInfo.rootDir, 'dump'))) {
        fs.makeDirectory(fs.join(this.instanceInfo.rootDir, 'dump'));
      }
      this.dumpConfig.setOutputDirectory('dump' + fs.pathSeparator + database);
    }
    if (!this.dumpConfig.haveSetAllDatabases()) {
      this.dumpConfig.setDatabase(database);
    }
    this.results.dump = this.arangodump();
    return this.validate(this.results.dump);
  }

  restoreTo(database, options = { separate: false, fromDir: '' }) {
    this.print('restore');

    if (options.hasOwnProperty('separate') && options.separate === true) {
      if (!options.hasOwnProperty('fromDir') || typeof options.fromDir !== 'string') {
        options.fromDir = database;
      }
      if (!fs.exists(fs.join(this.instanceInfo.rootDir, 'dump'))) {
        fs.makeDirectory(fs.join(this.instanceInfo.rootDir, 'dump'));
      }
      this.restoreConfig.setInputDirectory('dump' + fs.pathSeparator + options.fromDir, true);
    }

    if (!this.restoreConfig.haveSetAllDatabases()) {
      this.restoreConfig.setDatabase(database);
    }
    this.restoreConfig.disableContinue();
    let retryCount = 0;
    do {
      this.results.restore = this.arangorestore();
      if (this.results.restore.exitCode === 38) {
        retryCount += 1;
        if (retryCount === 21) {
          this.restoreConfig.deactivateFailurePoint();
          this.restoreOldConfig.deactivateFailurePoint();
          print("Failure point has terminated the application, restarting, continuing without.");
        } else {
          print("Failure point has terminated the application, restarting");
        }
        sleep(2 * timeoutFactor);
        
        this.restoreConfig.enableContinue();
      }
    } while(this.results.restore.exitCode === 38);
    this.restoreConfig.disableContinue();
    return this.validate(this.results.restore);
  }

  runTests(file, database) {
    this.print('dump after restore - ' + file);
    if (this.restoreConfig.haveSetAllDatabases()) {
      // if we dump with multiple databases, it remains with the original name.
      database = 'UnitTestsDumpSrc';
    }
    db._useDatabase(database);
    this.results.test = this.arangosh(file, {'server.database': database});
    return this.validate(this.results.test);
  }

  runReTests(file, database) {
    this.print('revalidating modifications - ' + file);
    db._useDatabase(database);
    this.results.test = this.arangosh(file, {'server.database': database});
    return this.validate(this.results.test);
  }

  tearDown(file) {
    this.print('teardown - ' + file);
    db._useDatabase('_system');
    this.results.tearDown = this.arangosh(file);
    return this.validate(this.results.tearDown);
  }

  restoreOld(directory) {
    this.print('restoreOld');
    db._useDatabase('_system');
    this.restoreOldConfig.setInputDirectory(directory, true);
    this.results.restoreOld = this.arangorestoreOld();
    return this.validate(this.results.restoreOld);
  }

  testRestoreOld(file) {
    this.print('test restoreOld - ' + file);
    db._useDatabase('_system');
    this.results.testRestoreOld = this.arangosh(file);
    return this.validate(this.results.testRestoreOld);
  }

  restoreFoxxComplete(database) {
    this.print('Foxx Apps with full restore');
    this.restoreConfig.setDatabase(database);
    this.restoreConfig.setIncludeSystem(true);
    this.results.restoreFoxxComplete = this.arangorestore();
    return this.validate(this.results.restoreFoxxComplete);
  }

  testFoxxComplete(file, database) {
    this.print('Test Foxx Apps after full restore - ' + file);
    db._useDatabase(database);
    this.results.testFoxxComplete = this.arangosh(file, {'server.database': database});
    return this.validate(this.results.testFoxxComplete);
  }

  restoreFoxxAppsBundle(database) {
    this.print('Foxx Apps restore _apps then _appbundles');
    db._useDatabase('_system');
    this.restoreConfig.setDatabase(database);
    this.restoreConfig.restrictToCollection('_apps');
    this.results.restoreFoxxAppBundlesStep1 = this.arangorestore();
    if (!this.validate(this.results.restoreFoxxAppBundlesStep1)) {
      return false;
    }
    this.restoreConfig.restrictToCollection('_appbundles');
    // TODO if cluster, switch coordinator!
    this.results.restoreFoxxAppBundlesStep2 = this.arangorestore();
    return this.validate(this.results.restoreFoxxAppBundlesStep2);
  }

  testFoxxAppsBundle(file, database) {
    this.print('Test Foxx Apps after _apps then _appbundles restore - ' + file);
    db._useDatabase(database);
    this.results.testFoxxAppBundles = this.arangosh(file, {'server.database': database});
    return this.validate(this.results.testFoxxAppBundles);
  }

  restoreFoxxBundleApps(database) {
    this.print('Foxx Apps restore _appbundles then _apps');
    db._useDatabase(database);
    this.restoreConfig.setDatabase(database);
    this.restoreConfig.restrictToCollection('_appbundles');
    this.results.restoreFoxxAppBundlesStep1 = this.arangorestore();
    if (!this.validate(this.results.restoreFoxxAppBundlesStep1)) {
      return false;
    }
    this.restoreConfig.restrictToCollection('_apps');
    // TODO if cluster, switch coordinator!
    this.results.restoreFoxxAppBundlesStep2 = this.arangorestore();
    return this.validate(this.results.restoreFoxxAppBundlesStep2);
  }

  testFoxxBundleApps(file, database) {
    this.print('Test Foxx Apps after _appbundles then _apps restore - ' + file);
    db._useDatabase(database);
    this.results.testFoxxFoxxAppBundles = this.arangosh(file, {'server.database': database});
    return this.validate(this.results.testFoxxAppBundles);
  }

  createHotBackup() {
    this.print("creating backup");
    let cmds = {
      "label": "testHotBackup"
    };
    this.results.createHotBackup = pu.run.arangoBackup(this.firstRunOptions, this.instanceInfo, "create", cmds, this.instanceInfo.rootDir, true);
    this.print("done creating backup");
    return this.results.createHotBackup.status;
  }

  restoreHotBackup() {
    this.print("restoring backup - start");
    db._useDatabase('_system');
    let list = this.listHotBackup();
    let backupName;
    Object.keys(list).forEach(function (name, i) {
      if (name.search("testHotBackup") !== -1) {
        backupName = name;
      }
    });
    if (backupName === undefined) {
      this.print("didn't find a backup matching our pattern!");
      this.results.restoreHotBackup = { status: false };
      return false;
    }
    if (!list[backupName].hasOwnProperty("keys") ||
         list[backupName].keys[0].sha256 !== encryptionKeySha256) {
      this.print("didn't find a backup having correct encryption keys!");
      this.print(JSON.stringify(list));
      this.results.restoreHotBackup = { status: false };
      return false;
    }
    this.print("restoring backup");
    let cmds = {
      "identifier": backupName,
      "max-wait-for-restart": 100.0
    };
    this.results.restoreHotBackup = pu.run.arangoBackup(this.firstRunOptions, this.instanceInfo, "restore", cmds, this.instanceInfo.rootDir, true);
    this.print("done restoring backup");
    return true;
  }

  listHotBackup() {
    return hb.get();
  }
};

function getClusterStrings(options) {
  if (options.hasOwnProperty('allDatabases') && options.allDatabases) {
    return {
      cluster: '-multiple',
      notCluster: '-multiple'
    };
  }
  if (options.cluster) {
    return {
      cluster: '-cluster',
      notCluster: '-singleserver'
    };
  } else {
    return {
      cluster: '',
      notCluster: '-cluster'
    };
  }
}

function dump_backend_two_instances (firstRunOptions, secondRunOptions, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, tstFiles, afterServerStart) {
  print(CYAN + which + ' tests...' + RESET);

  const helper = new DumpRestoreHelper(firstRunOptions, secondRunOptions, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, afterServerStart);
  if (!helper.startFirstInstance()) {
    return helper.extractResults();
  }

  const setupFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpSetup));
  const cleanupFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCleanup));
  const checkDumpFiles = tstFiles.checkDumpFiles ? tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCheckDumpFiles)) : undefined;
  const testFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpAgain));
  const tearDownFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpTearDown));

  if (firstRunOptions.hasOwnProperty("multipleDumps") && firstRunOptions.multipleDumps) {
    if (!helper.runSetupSuite(setupFile) ||
        !helper.dumpFrom('_system', true) ||
        !helper.dumpFrom('UnitTestsDumpSrc', true) ||
        (checkDumpFiles && !helper.runCheckDumpFilesSuite(checkDumpFiles)) ||
        !helper.runCleanupSuite(cleanupFile) ||
        !helper.restartInstance() ||
        !helper.restoreTo('UnitTestsDumpDst', { separate: true, fromDir: 'UnitTestsDumpSrc'}) ||
        !helper.restoreTo('_system', { separate: true }) ||
        !helper.runTests(testFile,'UnitTestsDumpDst') ||
        !helper.tearDown(tearDownFile)) {
      return helper.extractResults();
    }
  }
  else {
    if (!helper.runSetupSuite(setupFile) ||
        !helper.dumpFrom('UnitTestsDumpSrc') ||
        (checkDumpFiles && !helper.runCheckDumpFilesSuite(checkDumpFiles)) ||
        !helper.runCleanupSuite(cleanupFile) ||
        !helper.restartInstance() ||
        !helper.restoreTo('UnitTestsDumpDst') ||
        !helper.runTests(testFile,'UnitTestsDumpDst') ||
        !helper.tearDown(tearDownFile)) {
      return helper.extractResults();
    }
  }

  if (tstFiles.hasOwnProperty("dumpCheckGraph")) {
    const notCluster = getClusterStrings(secondRunOptions).notCluster;
    const restoreDir = tu.makePathUnix(tu.pathForTesting('server/dump/dump' + notCluster));
    const oldTestFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCheckGraph));
    if (!helper.restoreOld(restoreDir) ||
        !helper.testRestoreOld(oldTestFile)) {
      return helper.extractResults();
    }
  }

  if (tstFiles.hasOwnProperty("foxxTest")) {
    const foxxTestFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.foxxTest));
    if (secondRunOptions.hasOwnProperty("multipleDumps") && secondRunOptions.multipleDumps) {
      helper.adjustRestoreToDump();
      helper.restoreConfig.setInputDirectory(fs.join('dump','UnitTestsDumpSrc'), true);
    }
    if (!helper.restoreFoxxComplete('UnitTestsDumpFoxxComplete') ||
        !helper.testFoxxComplete(foxxTestFile, 'UnitTestsDumpFoxxComplete') ||
        !helper.restoreFoxxAppsBundle('UnitTestsDumpFoxxAppsBundle') ||
        !helper.testFoxxAppsBundle(foxxTestFile, 'UnitTestsDumpFoxxAppsBundle') ||
        !helper.restoreFoxxAppsBundle('UnitTestsDumpFoxxBundleApps') ||
        !helper.testFoxxAppsBundle(foxxTestFile, 'UnitTestsDumpFoxxBundleApps')) {
      return helper.extractResults();
    }
  }

  return helper.extractResults();
}

function dump_backend (options, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, tstFiles, afterServerStart) {
  return dump_backend_two_instances(options, options, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, tstFiles, afterServerStart);
}

function dump (options) {
  let c = getClusterStrings(options);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-compressed.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph.js',
    foxxTest: 'check-foxx.js'
  };

  return dump_backend(options, {}, {}, options, options, 'dump', tstFiles, function(){});
}

function dumpMixedClusterSingle (options) {
  let clusterOptions = _.clone(options);
  clusterOptions.cluster = true;
  let singleOptions = _.clone(options);
  singleOptions.cluster = false;
  let clusterStrings = getClusterStrings(clusterOptions);
  let singleStrings = getClusterStrings(singleOptions);
  let tstFiles = {
    dumpSetup: 'dump-setup' + clusterStrings.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-compressed.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump-mixed' + singleStrings.cluster + '.js',
    dumpTearDown: 'dump-teardown-mixed' + singleStrings.cluster + '.js',
    dumpCheckGraph: 'check-graph.js',
    foxxTest: 'check-foxx.js'
  };

  return dump_backend_two_instances(clusterOptions, singleOptions, {}, {}, options, options, 'dump', tstFiles, function(){});
}

function dumpMixedSingleCluster (options) {
  let clusterOptions = _.clone(options);
  clusterOptions.cluster = true;
  let singleOptions = _.clone(options);
  singleOptions.cluster = false;
  let clusterStrings = getClusterStrings(clusterOptions);
  let singleStrings = getClusterStrings(singleOptions);
  let tstFiles = {
    dumpSetup: 'dump-setup-mixed' + singleStrings.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-compressed.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump-mixed' + clusterStrings.cluster + '.js',
    dumpTearDown: 'dump-teardown-mixed' + singleStrings.cluster + '.js',
    dumpCheckGraph: 'check-graph.js',
    foxxTest: 'check-foxx.js'
  };

  return dump_backend_two_instances(singleOptions, clusterOptions, {}, {}, options, options, 'dump', tstFiles, function(){});
}

function dumpMultiple (options) {
  let c = getClusterStrings(options);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-uncompressed.js',
    dumpCleanup: 'cleanup-multiple.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph-multiple.js'
  };

  let dumpOptions = {
    allDatabases: true,
    deactivateCompression: true
  };
  _.defaults(dumpOptions, options);
  return dump_backend(dumpOptions, {}, {}, dumpOptions, dumpOptions, 'dump_multiple', tstFiles, function(){});
}

function dumpNoEnvelope (options) {
  let c = getClusterStrings(options);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-uncompressed-no-envelopes.js',
    dumpCleanup: 'cleanup-multiple.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph-multiple.js'
  };

  let dumpOptions = {
    allDatabases: true,
    deactivateCompression: true,
    deactivateEnvelopes: true
  };
  _.defaults(dumpOptions, options);
  return dump_backend(dumpOptions, {}, {}, dumpOptions, dumpOptions, 'dump_no_envelope', tstFiles, function(){});
}

function dumpWithCrashes (options) {
  let c = getClusterStrings(options);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-uncompressed.js',
    dumpCleanup: 'cleanup-multiple.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph-multiple.js'
  };

  let dumpOptions = {
    allDatabases: true,
    deactivateCompression: true,
    activateFailurePoint: true
  };
  _.defaults(dumpOptions, options);
  return dump_backend(dumpOptions, {}, {}, dumpOptions, dumpOptions, 'dump_with_crashes', tstFiles, function(){});
}

function dumpAuthentication (options) {
  const clientAuth = {
    'server.authentication': 'true'
  };

  const serverAuthInfo = {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann'
  };

  let dumpAuthOpts = {
    username: 'foobaruser',
    password: 'foobarpasswd'
  };

  let restoreAuthOpts = {
    username: 'foobaruser',
    password: 'pinus'
  };

  _.defaults(dumpAuthOpts, options);
  _.defaults(restoreAuthOpts, options);

  let tstFiles = {
    dumpSetup: 'dump-authentication-setup.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-nothing.js',
    dumpCleanup: 'cleanup-alter-user.js',
    dumpAgain: 'dump-authentication.js',
    dumpTearDown: 'dump-teardown.js',
    foxxTest: 'check-foxx.js'
  };

  options.multipleDumps = true;
  options['server.jwt-secret'] = 'haxxmann';

  return dump_backend(options, serverAuthInfo, clientAuth, dumpAuthOpts, restoreAuthOpts, 'dump_authentication', tstFiles, function(){});
}

function dumpJwt (options) {
  const clientAuth = {
    'server.authentication': 'true'
  };

  const serverAuthInfo = {
    'server.authentication': 'true',
    'server.jwt-secret': 'haxxmann'
  };

  let dumpAuthOpts = {
    jwtSecret: 'haxxmann',
    // intentionally no username/password
  };

  let restoreAuthOpts = {
    jwtSecret: 'haxxmann',
    // intentionally no username/password
  };

  _.defaults(dumpAuthOpts, options);
  _.defaults(restoreAuthOpts, options);

  let tstFiles = {
    dumpSetup: 'dump-authentication-setup.js',
    dumpCleanup: 'cleanup-alter-user.js',
    dumpAgain: 'dump-authentication.js',
    dumpTearDown: 'dump-teardown.js',
  };

  options.multipleDumps = true;
  options['server.jwt-secret'] = 'haxxmann';

  return dump_backend(options, serverAuthInfo, clientAuth, dumpAuthOpts, restoreAuthOpts, 'dump_authentication', tstFiles, function(){});
}

function dumpEncrypted (options) {
  // test is only meaningful in the Enterprise Edition
  let skip = true;
  if (global.ARANGODB_CLIENT_VERSION) {
    let version = global.ARANGODB_CLIENT_VERSION(true);
    if (version.hasOwnProperty('enterprise-version')) {
      skip = false;
    }
  }

  if (skip) {
    print('skipping dump_encrypted test');
    return {
      dump_encrypted: {
        status: true,
        skipped: true
      }
    };
  }

  let c = getClusterStrings(options);

  let afterServerStart = function(instanceInfo) {
    let keyFile = fs.join(instanceInfo.rootDir, 'secret-key');
    fs.write(keyFile, 'DER-HUND-der-hund-der-hund-der-h'); // must be exactly 32 chars long
    return keyFile;
  };

  let dumpOptions = _.clone(options);
  dumpOptions.encrypted = true;
  dumpOptions.compressed = true; // Should be overruled by 'encrypted'

  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-encrypted.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    foxxTest: 'check-foxx.js'
  };

  return dump_backend(options, {}, {}, dumpOptions, dumpOptions, 'dump_encrypted', tstFiles, afterServerStart);
}

function dumpMaskings (options) {
  // test is only meaningful in the Enterprise Edition
  let skip = true;
  if (global.ARANGODB_CLIENT_VERSION) {
    let version = global.ARANGODB_CLIENT_VERSION(true);
    if (version.hasOwnProperty('enterprise-version')) {
      skip = false;
    }
  }

  if (skip) {
    print('skipping dump_maskings test');
    return {
      dump_maskings: {
        status: true,
        skipped: true
      }
    };
  }

  let tstFiles = {
    dumpSetup: 'dump-maskings-setup.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-nothing.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump-maskings.js',
    dumpTearDown: 'dump-teardown.js'
  };

  let dumpMaskingsOpts = {
    maskings: 'maskings1.json'
  };

  _.defaults(dumpMaskingsOpts, options);

  return dump_backend(options, {}, {}, dumpMaskingsOpts, options, 'dump_maskings', tstFiles, function(){});
}

function hotBackup (options) {
  let c = getClusterStrings(options);
  if (!require("internal").isEnterprise()) {
    return {
      'hotbackup is only enterprise': {
        status: true,
      }
    };
  }
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-nothing.js',
    dumpCheck: 'dump' + c.cluster + '.js',
    dumpModify: 'dump-modify.js',
    dumpRecheck: 'dump-modified.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    // do we need this? dumpCheckGraph: 'check-graph.js',
    // todo foxxTest: 'check-foxx.js'
  };

  let which = "dump";
  // /return dump_backend(options, {}, {}, dumpMaskingsOpts, options, 'dump_maskings', tstFiles, function(){});
  print(CYAN + which + ' tests...' + RESET);

  let addArgs = {};
  const useEncryption = true;
  if (useEncryption) {
    let keyDir = fs.join(fs.getTempPath(), 'arango_encryption');
    if (!fs.exists(keyDir)) {  // needed on win32
      fs.makeDirectory(keyDir);
    }
    pu.cleanupDBDirectoriesAppend(keyDir);

    let keyfile = fs.join(keyDir, 'secret');
    fs.write(keyfile, encryptionKey);

    addArgs['rocksdb.encryption-keyfolder'] = keyDir;
  }

  const helper = new DumpRestoreHelper(options, options, addArgs, {}, options, options, which, function(){});
  if (!helper.startFirstInstance()) {
    return helper.extractResults();
  }

  const setupFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpSetup));
  const dumpCheck = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCheck));
  const dumpModify = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpModify));
  const dumpRecheck  = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpRecheck));
  const tearDownFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpTearDown));
  if (!helper.runSetupSuite(setupFile) ||
      !helper.dumpFrom('UnitTestsDumpSrc') ||
      !helper.restartInstance() ||
      !helper.restoreTo('UnitTestsDumpDst') ||
      !helper.isAlive() ||
      !helper.createHotBackup() ||
      !helper.isAlive() ||
      !helper.runTests(dumpModify,'UnitTestsDumpDst') ||
      !helper.isAlive() ||
      !helper.runReTests(dumpRecheck,'UnitTestsDumpDst') ||
      !helper.isAlive() ||
      !helper.restoreHotBackup() ||
      !helper.runTests(dumpCheck, 'UnitTestsDumpDst')||
      !helper.tearDown(tearDownFile)) {
    return helper.extractResults();
  }

  if (tstFiles.hasOwnProperty("dumpCheckGraph")) {
    const notCluster = getClusterStrings(options).notCluster;
    const restoreDir = tu.makePathUnix(tu.pathForTesting('server/dump/dump' + notCluster));
    const oldTestFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCheckGraph));
    if (!helper.restoreOld(restoreDir) ||
        !helper.testRestoreOld(oldTestFile)) {
      return helper.extractResults();
    }
  }

  if (tstFiles.hasOwnProperty("foxxTest")) {
    const foxxTestFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.foxxTest));
    if (!helper.restoreFoxxComplete('UnitTestsDumpFoxxComplete') ||
        !helper.testFoxxComplete(foxxTestFile, 'UnitTestsDumpFoxxComplete') ||
        !helper.restoreFoxxAppsBundle('UnitTestsDumpFoxxAppsBundle') ||
        !helper.testFoxxAppsBundle(foxxTestFile, 'UnitTestsDumpFoxxAppsBundle') ||
        !helper.restoreFoxxAppsBundle('UnitTestsDumpFoxxBundleApps') ||
        !helper.testFoxxAppsBundle(foxxTestFile, 'UnitTestsDumpFoxxBundleApps')) {
      return helper.extractResults();
    }
  }

  return helper.extractResults();
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);

  testFns['dump'] = dump;
  defaultFns.push('dump');

  testFns['dump_mixed_cluster_single'] = dumpMixedClusterSingle;
  defaultFns.push('dump_mixed_cluster_single');

  testFns['dump_mixed_single_cluster'] = dumpMixedSingleCluster;
  defaultFns.push('dump_mixed_single_cluster');

  testFns['dump_authentication'] = dumpAuthentication;
  defaultFns.push('dump_authentication');
  
  testFns['dump_jwt'] = dumpJwt;
  defaultFns.push('dump_jwt');

  testFns['dump_encrypted'] = dumpEncrypted;
  defaultFns.push('dump_encrypted');

  testFns['dump_maskings'] = dumpMaskings;
  defaultFns.push('dump_maskings');

  testFns['dump_multiple'] = dumpMultiple;
  defaultFns.push('dump_multiple');

  testFns['dump_no_envelope'] = dumpNoEnvelope;
  defaultFns.push('dump_no_envelope');

  testFns['dump_with_crashes'] = dumpWithCrashes;
  defaultFns.push('dump_with_crashes');

  testFns['hot_backup'] = hotBackup;
  defaultFns.push('hot_backup');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
