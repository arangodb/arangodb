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
const { isEnterprise, versionHas } = require("@arangodb/test-helper");

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const encryptionKeySha256 = "861009ec4d599fab1f40abc76e6f89880cff5833c79c548c99f9045f191cd90b";

let timeoutFactor = 1;
if (versionHas('asan') || versionHas('tsan')) {
  timeoutFactor = 8;
} else if (versionHas('coverage')) {
    timeoutFactor = 16;
}

class DumpRestoreHelper extends trs.runLocalInArangoshRunner {
  constructor(firstRunOptions, secondRunOptions, serverOptions, clientAuth, dumpOptions, restoreOptions, which, afterServerStart, rtaArgs, restartServer) {
    super(firstRunOptions, which, serverOptions, tr.sutFilters.checkUsers);
    this.serverOptions = serverOptions;
    this.firstRunOptions = firstRunOptions;
    this.secondRunOptions = secondRunOptions;
    this.restartServer = restartServer;
    this.clientAuth = clientAuth;
    this.dumpOptions = dumpOptions;
    this.restoreOptions = restoreOptions;
    this.allDatabases = [];
    this.allDumps = [];
    if (this.firstRunOptions.skipServerJS) {
      // TODO: what about 550,900,960 - QA-703?
      if (rtaArgs.length === 2) {
        rtaArgs[1] += ",070,071,801,550,900,960";
      } else {
        rtaArgs = ['--skip', "070,071,801,550,900,960"].concat(rtaArgs);
      }
      rtaArgs = [
        '--testFoxx', 'false'
      ].concat(rtaArgs);
    }
    this.rtaArgs = [ 'DUMPDB', '--numberOfDBs', '1'].concat(rtaArgs);
    this.rtaSkiplist = "";
    this.rtaDisabledTests = [];
    this.rtaDisabledTestsFull = [];
    this.rtaNegFilter = "";
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
    this.clientInstances = [];
  }

  destructor(cleanup) {
    print('destructor')
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
    this.doCleanup = this.serverOptions.cleanup && (this.results.failed === 0) && cleanup;
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
    this.instanceManager.options['rtaNegFilter'] = this.rtaNegFilter;
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
      this.instanceManager.options['rtaNegFilter'] = this.rtaNegFilter;
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

  runTestFn(testFunction) {
    this.print("running snippet");

    let ret = testFunction();
    this.validate(ret.testresult);
    return ret.status;
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
    db._useDatabase(database);
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
    if (!this.firstRunOptions.skipServerJS) {
      this.print('Foxx Apps with full restore to ' + database);
      this.restoreConfig.setDatabase(database);
      this.restoreConfig.setIncludeSystem(true);
      this.restoreConfig.setInputDirectory('UnitTestsDumpSrc', true);
      this.results.restoreFoxxComplete = this.arangorestore();
      return this.validate(this.results.restoreFoxxComplete);
    }
    return true;
  }

  testFoxxRoutingReady() {
    if (!this.firstRunOptions.skipServerJS) {
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
    }
    return 0;
  }

  testFoxxComplete(file, database) {
    if (!this.firstRunOptions.skipServerJS) {
      this.print('Test Foxx Apps after full restore - ' + file);
      db._useDatabase(database);
      this.testFoxxRoutingReady();
      this.addArgs = {'server.database': database};
      this.results.testFoxxComplete = this.runOneTest(file);
      this.addArgs = undefined;
      return this.validate(this.results.testFoxxComplete);
    }
    return true;
  }

  restoreFoxxAppsBundle(database) {
    if (!this.firstRunOptions.skipServerJS) {
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
    return true;
  }

  testFoxxAppsBundle(file, database) {
    if (!this.firstRunOptions.skipServerJS) {
      this.print('Test Foxx Apps after _apps then _appbundles restore - ' + file);
      db._useDatabase(database);
      this.addArgs = {'server.database': database};
      this.results.testFoxxAppBundles = this.runOneTest(file);
      this.addArgs = undefined;
      return this.validate(this.results.testFoxxAppBundles);
    }
    return true;
  }

  restoreFoxxBundleApps(database) {
    if (!this.firstRunOptions.skipServerJS) {
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
    return true;
  }

  testFoxxBundleApps(file, database) {
    if (!this.firstRunOptions.skipServerJS) {
      this.print('Test Foxx Apps after _appbundles then _apps restore - ' + file);
      db._useDatabase(database);
      this.addArgs = {'server.database': database};
      this.results.testFoxxFoxxAppBundles = this.runOneTest(file);
      this.addArgs = undefined;
      return this.validate(this.results.testFoxxAppBundles);
    }
    return true;
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
    let first = true;
    while (first || this.serverOptions.loopEternal) {
      this.print("restoring backup - start");
      first = false;
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
    }
    return true;
  }

  listHotBackup() {
    return hb.get();
  }

  runRtaMakedata() {
    let res = {};
    let logFile = fs.join(fs.getTempPath(), `rta_out_makedata.log`);
    let rc = ct.run.rtaMakedata(this.instanceManager.options, this.instanceManager, 0, "creating test data", logFile, this.rtaArgs);
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
    db._useDatabase('_system');
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

  spawnStressArangosh(snippet, key) {
    global.instanceManager = this.instanceManager;
    let testFn = fs.getTempFile();
    fs.write(testFn, "x");
    snippet = `require('fs').remove('${testFn}');
    let endpoint = '${this.instanceManager.endpoint}';
    let passvoid = '${this.instanceManager.options.password}';
    ${snippet}`;
    this.clientInstances.push(
      ct.run.launchPlainSnippetInBG(snippet, key)
    )
    while (fs.exists(testFn)) {
      sleep(0.1);
    }
    return true;
  }
  stopStressArangosh() {
    return ct.run.joinForceBGShells(this.options, this.clientInstances);
  }

};


exports.DumpRestoreHelper = DumpRestoreHelper;
exports.getClusterStrings = function(options) {
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
};
