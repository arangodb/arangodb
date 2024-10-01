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
// / @author Copyright 2021, ArangoDB GmbH, Cologne, Germany
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
  'dump_with_crashes': 'restore and crash the client multiple times',
  'dump_with_crashes_parallel': 'restore and crash the client multiple times - parallel version',
  'dump_parallel': 'use experimental parallel dump',
  'hot_backup': 'hotbackup tests'
};

const optionsDocumentation = [
  '   - `dumpVPack`: if set to true to enable vpack dumping',
];

const pu = require('@arangodb/testutils/process-utils');
const ct = require('@arangodb/testutils/client-tools');
const tu = require('@arangodb/testutils/test-utils');
const tr = require('@arangodb/testutils/testrunner');
const trs = require('@arangodb/testutils/testrunners');
const im = require('@arangodb/testutils/instance-manager');
const fs = require('fs');
const _ = require('lodash');
const hb = require("@arangodb/hotbackup");
const sleep = require("internal").sleep;
const db = require("internal").db;
const platform = require("internal").platform;
const { isEnterprise, versionHas } = require("@arangodb/test-helper");

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const encryptionKey = '01234567890123456789012345678901';
const encryptionKeySha256 = "861009ec4d599fab1f40abc76e6f89880cff5833c79c548c99f9045f191cd90b";

let timeoutFactor = 1;
if (versionHas('asan') || versionHas('tsan')) {
  timeoutFactor = 8;
} else if (versionHas('coverage')) {
    timeoutFactor = 16;
}

const testPaths = {
  'dump': [tu.pathForTesting('client/dump')],
  'dump_mixed_cluster_single': [tu.pathForTesting('client/dump')],
  'dump_mixed_single_cluster': [tu.pathForTesting('client/dump')],
  'dump_authentication': [tu.pathForTesting('client/dump')],
  'dump_jwt': [tu.pathForTesting('client/dump')],
  'dump_encrypted': [tu.pathForTesting('client/dump')],
  'dump_maskings': [tu.pathForTesting('client/dump')],
  'dump_multiple': [tu.pathForTesting('client/dump')],
  'dump_with_crashes': [tu.pathForTesting('client/dump')],
  'dump_with_crashes_parallel': [tu.pathForTesting('client/dump')],
  'dump_parallel': [tu.pathForTesting('client/dump')],
  'hot_backup': [tu.pathForTesting('client/dump')]
};

class DumpRestoreHelper extends trs.runInArangoshRunner {
  constructor(firstRunOptions, secondRunOptions, serverOptions, clientAuth, dumpOptions, restoreOptions, which, afterServerStart, rtaArgs) {
    super(firstRunOptions, which, serverOptions, tr.sutFilters.checkUsers);
    this.serverOptions = serverOptions;
    this.firstRunOptions = firstRunOptions;
    this.secondRunOptions = secondRunOptions;
    this.restartServer = firstRunOptions.cluster !== secondRunOptions.cluster;
    this.clientAuth = clientAuth;
    this.dumpOptions = dumpOptions;
    this.restoreOptions = restoreOptions;
    this.allDatabases = [];
    this.allDumps = [];
    this.rtaArgs = [ 'DUMPDB', '--numberOfDBs', '1'].concat(rtaArgs);
    this.which = which;
    this.results = {failed: 0};
    this.dumpConfig = false;
    this.restoreConfig = false;
    this.restoreOldConfig = false;
    this.afterServerStart = afterServerStart;
    this.instanceManager = null;
    this.im1 = null;
    this.im2 = null;
    this.keyDir = null;
    this.otherKeyDir = null;
    this.doCleanup = true;
  }

  destructor(cleanup) {
    if (this.fn !== undefined && fs.exists(this.fn)) {
      fs.remove(this.fn);
    }
    if (this.instanceManager === null) {
      print(RED + 'no instance running. Nothing to stop!' + RESET);
      return this.results;
    }
    print(CYAN + 'Shutting down...' + RESET);
    if (this.im1.detectShouldBeRunning()) {
      try {
        this.im1.reconnect();
      } catch (ex) {
        print("forcefully shutting down because of: " + ex);
        this.results['shutdown'] = this.im1.shutdownInstance(true, "Execption caught: " + ex);
      }
    }
    if (this.im1.detectShouldBeRunning()) {
      this.results['shutdown'] = this.im1.shutdownInstance();
    }

    if (this.im2 !== null) {
      if (this.im2.detectShouldBeRunning()) {
        try {
          this.im2.reconnect();
        } catch (ex) {
          print("forcefully shutting down because of: " + ex);
          this.results['shutdown'] &= this.im1.shutdownInstance(true, "Execption caught: " + ex);
        }
      }
      if (this.im2.detectShouldBeRunning()) {
        this.results['shutdown'] &= this.im2.shutdownInstance();
      }
    }
    print(CYAN + 'done.' + RESET);

    Object.keys(this.results).forEach(key => {
      if (this.results[key].hasOwnProperty('failed')) {
        this.results.failed += this.results[key].failed;
      }
    });
    this.doCleanup = this.options.cleanup && (this.results.failed === 0) && cleanup;
    if (this.doCleanup) {
      if (this.im1 !== null) {
        this.im1.destructor(this.results.failed === 0);
      }
      if (this.im2 !== null) {
        this.im2.destructor(this.results.failed === 0);
      }

      [this.keyDir,
       this.otherKeyDir,
       this.dumpConfig.getOutputDirectory()].forEach(dir => {
         if ((dir !== null) && fs.exists(dir)) {
           print(CYAN + "Cleaning up " + dir + RESET);
           fs.removeDirectoryRecursive(dir);
         }
       });
    }
  }
  bindInstanceInfo(options) {
    if (!this.dumpConfig) {
      // the dump config will only be configured for the first instance.
      this.dumpConfig = ct.createBaseConfig('dump', this.dumpOptions, this.instanceManager);
      this.dumpConfig.setOutputDirectory('dump');
      this.dumpConfig.setIncludeSystem(true);
      if (this.dumpOptions.hasOwnProperty("maskings")) {
        this.dumpConfig.setMaskings(this.dumpOptions.maskings);
      }
      if (this.dumpOptions.allDatabases) {
        this.dumpConfig.setAllDatabases();
      }
    }
    if (this.dumpOptions.dumpVPack) {
      this.dumpConfig.activateVPack();
    }
    if (this.dumpOptions.encrypted) {
      this.dumpConfig.activateEncryption();
    }
    if (this.dumpOptions.hasOwnProperty("threads")) {
      this.dumpConfig.setThreads(this.dumpOptions.threads);
    }
    this.dumpConfig.setUseParallelDump(this.dumpOptions.useParallelDump);
    this.dumpConfig.setUseSplitFiles(this.dumpOptions.splitFiles);
    if (this.dumpOptions.jwtSecret) {
      this.keyDir = fs.join(fs.getTempPath(), 'jwtSecrets');
      if (!fs.exists(this.keyDir)) {  // needed on win32
        fs.makeDirectory(this.keyDir);
      }
      let keyFile = fs.join(this.keyDir, 'secret-for-dump');
      fs.write(keyFile, this.dumpOptions.jwtSecret);
      this.dumpConfig.setJwtFile(keyFile);
    }

    if (!this.restoreConfig) {
      this.restoreConfig = ct.createBaseConfig('restore', this.restoreOptions, this.instanceManager);
      this.restoreConfig.setInputDirectory('dump', true);
      this.restoreConfig.setIncludeSystem(true);
    } else {
      this.restoreConfig.setEndpoint(this.instanceManager.endpoint);
    }
    
    if (!this.restoreOldConfig) {
      this.restoreOldConfig = ct.createBaseConfig('restore', this.restoreOptions, this.instanceManager);
      this.restoreOldConfig.setInputDirectory('dump', true);
      this.restoreOldConfig.setIncludeSystem(true);
      this.restoreOldConfig.setRootDir(pu.TOP_DIR);
    } else {
      this.restoreOldConfig.setEndpoint(this.instanceManager.endpoint);
    }
    if (this.restoreOptions.hasOwnProperty("threads")) {
      this.restoreConfig.setThreads(this.restoreOptions.threads);
    }
    if (this.restoreOptions.jwtSecret) {
      this.otherKeyDir = fs.join(fs.getTempPath(), 'other_jwtSecrets');
      if (!fs.exists(this.otherKeyDir)) {  // needed on win32
        fs.makeDirectory(this.otherKeyDir);
      }
      let keyFile = fs.join(this.otherKeyDir, 'secret-for-restore');
      fs.write(keyFile, this.restoreOptions.jwtSecret);
      this.restoreConfig.setJwtFile(keyFile);
      this.restoreOldConfig.setJwtFile(keyFile);
    }
    this.setOptions();
    this.arangorestore = ct.run.arangoDumpRestoreWithConfig.bind(this, this.restoreConfig, this.restoreOptions, this.instanceManager.rootDir, this.firstRunOptions.coreCheck);
    this.arangorestoreOld = ct.run.arangoDumpRestoreWithConfig.bind(this, this.restoreOldConfig, this.restoreOptions, this.instanceManager.rootDir, this.firstRunOptions.coreCheck);
    this.arangodump = ct.run.arangoDumpRestoreWithConfig.bind(this, this.dumpConfig, this.dumpOptions, this.instanceManager.rootDir, this.firstRunOptions.coreCheck);
    this.fn = this.afterServerStart(this.instanceManager);
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
    let rootDir = fs.join(fs.getTempPath(), '1');
    this.instanceManager = this.im1 = new im.instanceManager('tcp', this.firstRunOptions, this.serverOptions, this.which, rootDir);
    this.instanceManager.prepareInstance();
    this.instanceManager.launchTcpDump("");
    if (!this.instanceManager.launchInstance()) {
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
    this.instanceManager.reconnect();

    this.bindInstanceInfo(this.firstRunOptions);
    return true;
  }

  restartInstance() {
    if (this.restartServer) {
      print(CYAN + 'Shutting down...' + RESET);
      let rootDir = fs.join(fs.getTempPath(), '2');
      print(CYAN + 'done.' + RESET);
      this.which = this.which + "_2";
      this.instanceManager = this.im2 = new im.instanceManager('tcp', this.secondRunOptions, this.serverOptions, this.which, rootDir);
      this.instanceManager.prepareInstance();
      this.instanceManager.launchTcpDump("");
      if (!this.instanceManager.launchInstance()) {
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
      this.instanceManager.reconnect();
      this.bindInstanceInfo(this.secondRunOptions);
      return true;

    }
    return true;
  }

  adjustRestoreToDump() {
    this.restoreOptions = this.dumpOptions;
    this.restoreConfig = ct.createBaseConfig('restore', this.dumpOptions, this.instanceManager);
    this.arangorestore = ct.run.arangoDumpRestoreWithConfig.bind(this, this.restoreConfig, this.restoreOptions, this.instanceManager.rootDir, this.firstRunOptions.coreCheck);
  }

  isAlive() {
    return (this.instanceManager !== null) && this.instanceManager.checkInstanceAlive();
  }

  getUptime() {
    try {
      return this.instanceManager.checkUptime();
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
    print();
    return this.results;
  }

  runSetupSuite(path) {
    this.print('Setting up - ' + path);
    this.results.setup = this.runOneTest(path);
    return this.validate(this.results.setup);
  }

  runCheckDumpFilesSuite(path) {
    this.print(`Inspecting dumped files - ${path} - ${this.allDumps.length}`);
    if (this.allDumps.length > 0) {
      process.env['dump-directory'] = this.allDumps[0];
    } else {
      process.env['dump-directory'] = this.dumpConfig.config['output-directory'];
    }
    this.results.checkDumpFiles = this.runOneTest(path);
    delete process.env['dump-directory'];
    return this.validate(this.results.checkDumpFiles);
  }

  runCleanupSuite(path) {
    this.print('Cleaning up - ' + path);
    this.results.cleanup = this.runOneTest(path);
    arango.reconnect(arango.getEndpoint(), '_system', "root", "");
    return this.validate(this.results.cleanup);
  }

  dumpFrom(database, separateDir = false) {
    this.print(`dumping ${database}`);
    let baseDir = fs.join(this.instanceManager.rootDir, 'dump');
    if (!fs.exists(baseDir)) {
      fs.makeDirectory(baseDir);
    }
    if (separateDir) {
      this.dumpConfig.setOutputDirectory(database);
    } else {
      this.dumpConfig.setOutputDirectory('dump');
    }
    this.allDumps.push(this.dumpConfig.config['output-directory']);
    if (database !== '_system' || !this.dumpConfig.haveSetAllDatabases()) {
      this.dumpConfig.resetAllDatabases();
      this.allDumps.push(this.dumpConfig.config['output-directory']);
      this.dumpConfig.setDatabase(database);
    }
    this.results.dump = this.arangodump();
    return this.validate(this.results.dump);
  }

  restoreTo(database, options = { separate: false, fromDir: '' }) {
    this.print(`restore ${database} ${JSON.stringify(options)}`);

    if (options.hasOwnProperty('separate') && options.separate === true) {
      if (!options.hasOwnProperty('fromDir') || typeof options.fromDir !== 'string') {
        options.fromDir = database;
      }
      if (!fs.exists(fs.join(this.instanceManager.rootDir, 'dump'))) {
        fs.makeDirectory(fs.join(this.instanceManager.rootDir, 'dump'));
      }
      this.restoreConfig.setInputDirectory(options.fromDir, true);
    }

    if (!this.restoreConfig.haveSetAllDatabases()) {
      this.restoreConfig.setDatabase(database);
    }
    this.restoreConfig.disableContinue();
    let retryCount = 0;
    do {
      this.results.restore = this.arangorestore();
      if (!this.instanceManager.checkInstanceAlive()) {
        this.results.failed = true;
        return false;
      }
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
    this.addArgs = {'server.database': database};
    this.results.test = this.runOneTest(file);
    this.addArgs = undefined;
    return this.validate(this.results.test);
  }

  runReTests(file, database) {
    this.print('revalidating modifications - ' + file);
    this.addArgs = {'server.database': database};
    this.results.test = this.runOneTest(file);
    this.addArgs = undefined;
    return this.validate(this.results.test);
  }

  tearDown(file) {
    this.print('teardown - ' + file);
    db._useDatabase('_system');
    this.results.tearDown = this.runOneTest(file);
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
    this.results.testRestoreOld = this.runOneTest(file);
    return this.validate(this.results.testRestoreOld);
  }

  restoreFoxxComplete(database) {
    this.print('Foxx Apps with full restore to ' + database);
    this.restoreConfig.setDatabase(database);
    this.restoreConfig.setIncludeSystem(true);
    this.restoreConfig.setInputDirectory('UnitTestsDumpSrc', true);
    this.results.restoreFoxxComplete = this.arangorestore();
    return this.validate(this.results.restoreFoxxComplete);
  }

  testFoxxRoutingReady() {
    for (let i = 0; i < 20; i++) {
      try {
        let reply = arango.GET_RAW('/this_route_is_not_here', true);
        if (reply.code === 404) {
          print("selfHeal was already executed - Foxx is ready!");
          return 0;
        }
        print(" Not yet ready, retrying: " + reply.parsedBody);
      } catch (e) {
        print(" Caught - need to retry. " + JSON.stringify(e));
      }
      sleep(3);
    }
    throw new Error("020: foxx routeing not ready on time!");
  };

  testFoxxComplete(file, database) {
    this.print('Test Foxx Apps after full restore - ' + file);
    db._useDatabase(database);
    this.testFoxxRoutingReady();
    this.addArgs = {'server.database': database};
    this.results.testFoxxComplete = this.runOneTest(file);
    this.addArgs = undefined;
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
    this.addArgs = {'server.database': database};
    this.results.testFoxxAppBundles = this.runOneTest(file);
    this.addArgs = undefined;
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
    this.addArgs = {'server.database': database};
    this.results.testFoxxFoxxAppBundles = this.runOneTest(file);
    this.addArgs = undefined;
    return this.validate(this.results.testFoxxAppBundles);
  }

  createHotBackup() {
    this.print("creating backup");
    let cmds = {
      "label": "testHotBackup"
    };
    this.results.createHotBackup = ct.run.arangoBackup(this.firstRunOptions, this.instanceManager, "create", cmds, this.instanceManager.rootDir, true);
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
    this.results.restoreHotBackup = ct.run.arangoBackup(this.firstRunOptions, this.instanceManager, "restore", cmds, this.instanceManager.rootDir, true);
    this.print("done restoring backup");
    return true;
  }

  listHotBackup() {
    return hb.get();
  }

  runRtaMakedata() {
    let res = {};
    let logFile = fs.join(fs.getTempPath(), `rta_out_makedata.log`);
    let rc = ct.run.rtaMakedata(this.options, this.instanceManager, 0, "creating test data", logFile, this.rtaArgs);
    if (!rc.status) {
      let rx = new RegExp(/\\n/g);
      this.results.RtaMakedata = {
        message:  'Makedata:\n' + fs.read(logFile).replace(rx, '\n'),
        status: false
      };
      this.results.failed += 1;
      return false;
    } else {
      fs.remove(logFile);
      this.results.RtaMakedata = {
        status: true
      };
      return true;
    }
  }

  dumpSrc() {
    if (!this.dumpConfig.haveSetAllDatabases()) {
      if (!this.dumpFrom('UnitTestsDumpSrc', true)) {
        this.results.dumpSrc = {
          message:  `dumpSrc: failed for UnitTestsDumpSrc`,
          status: false
        };
        return false;
      }
    }
    return true;
  }

  restoreSrc() {
    if (!this.dumpConfig.haveSetAllDatabases()) {
      return this.restoreTo('UnitTestsDumpDst',
                            { separate: true, fromDir: 'UnitTestsDumpSrc'});
    }
    return true;
  }

  dumpFromRta() {
    let success = true;
    const otherDBs = ['_system', 'UnitTestsDumpSrc', 'UnitTestsDumpDst', 'UnitTestsDumpFoxxComplete'];
    db._databases().forEach(db => { if (!otherDBs.find(x => x === db)) {this.allDatabases.push(db);}});
    if (!this.dumpConfig.haveSetAllDatabases()) {
      this.allDatabases.forEach(db => {
        if (!this.dumpFrom(db, true)) {
          this.results.RtaDump = {
            message: `RtaDump: failed for ${db}`,
            status: false
          };
          success = false;
          return false;
        }
      });
    }
    return success;
  }
  
  restoreRta() {
    let success = true;
    if (!this.restoreConfig.haveSetAllDatabases()) {
      if (!this.restoreConfig.hasJwt()) {
        // Since we restore afterwards, any dumped passwords
        // are in action again.
        this.restoreConfig.setAuth(
          this.dumpOptions.username,
          this.dumpOptions.password
        );
      }
      this.allDatabases.forEach(db => {
        if (!this.restoreTo(db, { separate: true, fromDir: db})) {
          this.results.RtaRestore = {
            message:  `RtaRestore: failed for ${db}`,
            status: false
          };
          success = false;
          return false;
        }
      });
    }
    db._useDatabase('_system');
    return success;
  }
  
  runRtaCheckData() {
    let res = {};
    let logFile = fs.join(fs.getTempPath(), `rta_out_checkdata.log`);
    let rc = ct.run.rtaMakedata(this.secondRunOptions, this.instanceManager, 1, "checking test data", logFile, this.rtaArgs);
    if (!rc.status) {
      let rx = new RegExp(/\\n/g);
      this.results.RtaCheckdata = {
        message: 'Checkdata:\n' + fs.read(logFile).replace(rx, '\n'),
        status: false
      };
      this.results.failed += 1;
      return false;
    } else {
      fs.remove(logFile);
      this.results.RtaCheckdata = {
        status: true
      };
      return true;
    }
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

function dump_backend_two_instances (firstRunOptions, secondRunOptions, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, tstFiles, afterServerStart, rtaArgs) {
  print(CYAN + which + ' tests...' + RESET);

  const helper = new DumpRestoreHelper(firstRunOptions, secondRunOptions, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, afterServerStart, rtaArgs);
  if (!helper.startFirstInstance()) {
    helper.destructor(false);
    return helper.extractResults();
  }

  const setupFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpSetup));
  const cleanupFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCleanup));
  const checkDumpFiles = tstFiles.dumpCheckDumpFiles ? tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCheckDumpFiles)) : undefined;
  const testFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpAgain));
  const tearDownFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpTearDown));

  try {
    if (firstRunOptions.hasOwnProperty("multipleDumps") && firstRunOptions.multipleDumps) {
      if (!helper.runSetupSuite(setupFile) ||
          !helper.runRtaMakedata() ||
          !helper.dumpFrom('_system', true) ||
          !helper.dumpFrom('UnitTestsDumpSrc', true) ||
          !helper.dumpFromRta() ||
          (checkDumpFiles && !helper.runCheckDumpFilesSuite(checkDumpFiles)) ||
          !helper.runCleanupSuite(cleanupFile) ||
          !helper.restartInstance() ||
          !helper.restoreSrc() ||
          !helper.restoreTo('_system', { separate: true }) ||
          !helper.restoreRta() ||
          !helper.runTests(testFile,'UnitTestsDumpDst') ||
          !helper.runRtaCheckData() ||
          !helper.tearDown(tearDownFile)) {
        helper.destructor(true);
        return helper.extractResults();
      }
    } else {
      if (!helper.runSetupSuite(setupFile) ||
          !helper.runRtaMakedata() ||
          !helper.dumpSrc() ||
          !helper.dumpFromRta() ||
          !helper.dumpFrom('_system', false) ||
          (checkDumpFiles && !helper.runCheckDumpFilesSuite(checkDumpFiles)) ||
          !helper.runCleanupSuite(cleanupFile) ||
          !helper.restartInstance() ||
          !helper.restoreSrc() ||
          !helper.restoreTo('_system', { separate: true, fromDir: 'dump' }) ||
          !helper.restoreRta() ||
          !helper.runRtaCheckData() ||
          !helper.runTests(testFile,'UnitTestsDumpDst') ||
          !helper.tearDown(tearDownFile)) {
        helper.destructor(true);
        return helper.extractResults();
      }
    }

    if (tstFiles.hasOwnProperty("dumpCheckGraph")) {
      const notCluster = getClusterStrings(secondRunOptions).notCluster;
      const restoreDir = tu.makePathUnix(tu.pathForTesting('client/dump/dump' + notCluster));
      const oldTestFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCheckGraph));
      if (!helper.restoreOld(restoreDir) ||
          !helper.testRestoreOld(oldTestFile)) {
        helper.destructor(true);
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
        helper.destructor(true);
        return helper.extractResults();
      }
    }
  } catch (ex) {
    print("Caught exception during testrun: " + ex + ex.stack);
    helper.destructor(false);
  }
  helper.destructor(true);
  return helper.extractResults();
}

function dump_backend (options, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, tstFiles, afterServerStart, rtaArgs) {
  return dump_backend_two_instances(options, options, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, tstFiles, afterServerStart, rtaArgs);
}

function dump (options) {
  let opts = _.clone(options);
  if (opts.cluster) {
    opts.dbServers = 3;
  }
  let c = getClusterStrings(opts);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-compressed.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph.js',
    foxxTest: 'check-foxx.js'
  };

  return dump_backend(opts, {}, {}, opts, opts, 'dump', tstFiles, function(){}, []);
}

function dumpMixedClusterSingle (options) {
  let clusterOptions = _.clone(options);
  clusterOptions.cluster = true;
  clusterOptions.dbServers = 3;
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

  return dump_backend_two_instances(clusterOptions, singleOptions, {}, {},
                                    options, options, 'dump_mixed_cluster_single',
                                    tstFiles, function(){}, [
                                      //'--testFoxx', 'false',
                                      // BTS-1617: disable 404 for now
                                      '--skip', '404,550,900,960']);
}

function dumpMixedSingleCluster (options) {
  let clusterOptions = _.clone(options);
  clusterOptions.cluster = true;
  clusterOptions.dbServers = 3;
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

  return dump_backend_two_instances(singleOptions, clusterOptions, {}, {},
                                    options, options, 'dump_mixed_single_cluster',
                                    tstFiles, function(){}, [
                                      // '--testFoxx', 'false',
                                      '--skip', '550,900,960']);
}

function dumpMultiple (options) {
  let dumpOptions = {
    dumpVPack: options.dumpVPack,
    dbServers: 3,
    allDatabases: true,
    deactivateCompression: true,
    parallelDump: true,
    splitFiles: true,
  };
  _.defaults(dumpOptions, options);
  let c = getClusterStrings(dumpOptions);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-uncompressed.js',
    dumpCleanup: 'cleanup-multiple.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph-multiple.js'
  };

  return dump_backend(dumpOptions, {}, {}, dumpOptions, dumpOptions, 'dump_multiple', tstFiles, function(){}, []);
}

function dumpWithCrashes (options) {
  let dumpOptions = {
    dumpVPack: options.dumpVPack,
    dbServers: 3,
    allDatabases: true,
    deactivateCompression: true,
    activateFailurePoint: true,
    threads: 1,
    useParallelDump: true,
    splitFiles: true,
  };
  _.defaults(dumpOptions, options);
  let c = getClusterStrings(dumpOptions);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-uncompressed.js',
    dumpCleanup: 'cleanup-multiple.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph-multiple.js'
  };

  return dump_backend(dumpOptions, {}, {}, dumpOptions, dumpOptions, 'dump_with_crashes', tstFiles, function(){}, []);
}

function dumpWithCrashesNonParallel (options) {
  let dumpOptions = {
    dumpVPack: options.dumpVPack,
    dbServers: 3,
    allDatabases: true,
    deactivateCompression: true,
    activateFailurePoint: true,
    threads: 1,
    useParallelDump: false,
    splitFiles: false,
  };
  _.defaults(dumpOptions, options);
  let c = getClusterStrings(dumpOptions);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-uncompressed.js',
    dumpCleanup: 'cleanup-multiple.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph-multiple.js'
  };

  return dump_backend(dumpOptions, {}, {}, dumpOptions, dumpOptions, 'dump_with_crashes_parallel', tstFiles, function(){}, []);
}

function dumpAuthentication (options) {
  const clientAuth = {
    'server.authentication': 'true'
  };

  let dumpAuthOpts = {
    username: 'foobaruser',
    password: 'foobarpasswd',
  };

  let restoreAuthOpts = {
    username: 'foobaruser',
    password: 'pinus',
  };

  _.defaults(dumpAuthOpts, options);
  _.defaults(restoreAuthOpts, options);
  dumpAuthOpts.dbServers = 3;
  dumpAuthOpts.useParallelDump = false;
  restoreAuthOpts.dbServers = 3;
  let tstFiles = {
    dumpSetup: 'dump-authentication-setup.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-nothing.js',
    dumpCleanup: 'cleanup-alter-user.js',
    dumpAgain: 'dump-authentication.js',
    dumpTearDown: 'dump-teardown.js',
    foxxTest: 'check-foxx.js'
  };

  let opts = Object.assign({}, options, tu.testServerAuthInfo, {
    multipleDumps: true,
    dbServers: 3
  });

  let ret= dump_backend(opts, _.clone(tu.testServerAuthInfo), clientAuth, dumpAuthOpts, restoreAuthOpts, 'dump_authentication', tstFiles, function(){}, []);
  options.cleanup = opts.cleanup;
  return ret;
}

function dumpJwt (options) {
  const clientAuth = {
    'server.authentication': 'true'
  };

  let dumpAuthOpts = _.clone(tu.testClientJwtAuthInfo);
  let restoreAuthOpts = _.clone(tu.testClientJwtAuthInfo);
  _.defaults(dumpAuthOpts, options);
  _.defaults(restoreAuthOpts, options);

  let tstFiles = {
    dumpSetup: 'dump-authentication-setup.js',
    dumpCleanup: 'cleanup-alter-user.js',
    dumpAgain: 'dump-authentication.js',
    dumpTearDown: 'dump-teardown.js',
  };

  let opts = Object.assign({}, options, tu.testServerAuthInfo, {
    multipleDumps: true,
    dbServers: 3
  });

  let ret = dump_backend(opts, tu.testServerAuthInfo, clientAuth, dumpAuthOpts, restoreAuthOpts, 'dump_jwt', tstFiles, function(){}, []);
  options.cleanup = opts.cleanup;
  return ret;
}

function dumpEncrypted (options) {
  // test is only meaningful in the Enterprise Edition
  if (!isEnterprise()) {
    print('skipping dump_encrypted test');
    return {
      dump_encrypted: {
        status: true,
        skipped: true
      }
    };
  }

  let c = getClusterStrings(options);

  let afterServerStart = function(instanceManager) {
    let keyFile = fs.join(instanceManager.rootDir, 'secret-key');
    fs.write(keyFile, 'DER-HUND-der-hund-der-hund-der-h'); // must be exactly 32 chars long
    return keyFile;
  };

  let dumpOptions = _.clone(options);
  dumpOptions.encrypted = true;
  dumpOptions.compressed = true; // Should be overruled by 'encrypted'
  dumpOptions.dbServers = 3;
  dumpOptions.splitFiles = false;

  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-encrypted.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    foxxTest: 'check-foxx.js'
  };

  return dump_backend(dumpOptions, {}, {}, dumpOptions, dumpOptions, 'dump_encrypted', tstFiles, afterServerStart, []);
}

function dumpNonParallel (options) {
  let c = getClusterStrings(options);

  let dumpOptions = _.clone(options);
  dumpOptions.useParallelDump = false;
  dumpOptions.splitFiles = false;
  dumpOptions.dbServers = 3;

  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-compressed.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph.js',
    foxxTest: 'check-foxx.js'
  };

  return dump_backend(dumpOptions, {}, {}, dumpOptions, dumpOptions, 'dump_parallel', tstFiles, function(){}, []);
}

function dumpMaskings (options) {
  // test is only meaningful in the Enterprise Edition
  if (!isEnterprise()) {
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
    maskings: 'maskings1.json',
    dbServers: 3
  };

  _.defaults(dumpMaskingsOpts, options);

  return dump_backend(dumpMaskingsOpts, {}, {}, dumpMaskingsOpts, options, 'dump_maskings', tstFiles, function(){}, []);
}

function hotBackup (options) {
  let c = getClusterStrings(options);
  console.warn(options);
  if (options.hasOwnProperty("dbServers") && options.dbServers > 1) {
    options.dbServers = 3;
  }
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
    dumpMoveShard: 'dump-move-shard.js',
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
  let keyDir;
  if (useEncryption) {
    keyDir = fs.join(fs.getTempPath(), 'arango_encryption');
    if (!fs.exists(keyDir)) {  // needed on win32
      fs.makeDirectory(keyDir);
    }

    let keyfile = fs.join(keyDir, 'secret');
    fs.write(keyfile, encryptionKey);

    addArgs['rocksdb.encryption-keyfolder'] = keyDir;
  }

  const helper = new DumpRestoreHelper(options, options, addArgs, {}, options, options, which, function(){}, []);
  if (!helper.startFirstInstance()) {
      helper.destructor(false);
    return helper.extractResults();
  }

  const setupFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpSetup));
  const dumpCheck = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCheck));
  const dumpModify = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpModify));
  const dumpMoveShard = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpMoveShard));
  const dumpRecheck  = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpRecheck));
  const tearDownFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpTearDown));
  try {
    if (!helper.runSetupSuite(setupFile) ||
        !helper.runRtaMakedata() ||
        !helper.dumpFrom('UnitTestsDumpSrc') ||
        !helper.restartInstance() ||
        !helper.restoreTo('UnitTestsDumpDst') ||
        !helper.isAlive() ||
        !helper.createHotBackup() ||
        !helper.isAlive() ||
        !helper.runTests(dumpModify,'UnitTestsDumpDst') ||
        !helper.isAlive() ||
        !helper.runTests(dumpMoveShard,'UnitTestsDumpDst') ||
        !helper.isAlive() ||
        !helper.runReTests(dumpRecheck,'UnitTestsDumpDst') ||
        !helper.isAlive() ||
        !helper.restoreHotBackup() ||
        !helper.runTests(dumpCheck, 'UnitTestsDumpDst')||
        !helper.runRtaCheckData() ||
        !helper.tearDown(tearDownFile)) {
      helper.destructor(true);
      return helper.extractResults();
    }

    if (tstFiles.hasOwnProperty("dumpCheckGraph")) {
      const notCluster = getClusterStrings(options).notCluster;
      const restoreDir = tu.makePathUnix(tu.pathForTesting('client/dump/dump' + notCluster));
      const oldTestFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCheckGraph));
      if (!helper.restoreOld(restoreDir) ||
          !helper.testRestoreOld(oldTestFile)) {
        helper.destructor(true);
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
        helper.destructor(true);
        return helper.extractResults();
      }
    }
  }
  catch (ex) {
    print("Caught exception during testrun: " + ex);
    helper.destructor(false);
  }
  helper.destructor(true);
  if (helper.doCleanup) {
    fs.removeDirectoryRecursive(keyDir, true);
  }
  return helper.extractResults();
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  opts['dumpVPack'] = false;
  
  testFns['dump'] = dump;
  testFns['dump_mixed_cluster_single'] = dumpMixedClusterSingle;
  testFns['dump_mixed_single_cluster'] = dumpMixedSingleCluster;
  testFns['dump_authentication'] = dumpAuthentication;
  testFns['dump_jwt'] = dumpJwt;
  testFns['dump_encrypted'] = dumpEncrypted;
  testFns['dump_maskings'] = dumpMaskings;
  testFns['dump_multiple'] = dumpMultiple;
  testFns['dump_with_crashes'] = dumpWithCrashes;
  testFns['dump_with_crashes_non_parallel'] = dumpWithCrashesNonParallel;
  testFns['dump_non_parallel'] = dumpNonParallel;
  testFns['hot_backup'] = hotBackup;

  tu.CopyIntoList(optionsDoc, optionsDocumentation);
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
