/* jshint strict: false, sub: true */
/* global print */
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
// / @author Michael Hackstein
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'BackupNoAuthSysTests':   'complete backup tests without authentication, with    system collections',
  'BackupNoAuthNoSysTests': 'complete backup tests without authentication, without system collections',
  'BackupAuthSysTests':     'complete backup tests with    authentication, with    system collections',
  'BackupAuthNoSysTests':   'complete backup tests with    authentication, without system collections'
};
const fs = require('fs');
const _ = require('lodash');
const pu = require('@arangodb/testutils/process-utils');
const ct = require('@arangodb/testutils/client-tools');
const tu = require('@arangodb/testutils/test-utils');
const tr = require('@arangodb/testutils/testrunner');
const trs = require('@arangodb/testutils/testrunners');
const im = require('@arangodb/testutils/instance-manager');

const CYAN = require('internal').COLORS.COLOR_CYAN;
const RESET = require('internal').COLORS.COLOR_RESET;
const log = (text) => {
  print(`${CYAN}${Date()}: Backup - ${text}${RESET}`);
};
const makePath = (name) => {
  return tu.makePathUnix(tu.pathForTesting(`client/backup/${name}`));
};

const asRoot = {
  username: 'root',
  password: ''
};

const syssys = 'systemsystem';
const sysNoSys = 'systemnosystem';

const testPaths = {
  'BackupNoAuthSysTests': [tu.pathForTesting('client/backup/backup-system-incl-system.js')],
  'BackupNoAuthNoSysTests': [tu.pathForTesting('client/backup/backup-system-excl-system.js')],
  'BackupAuthSysTests': [tu.pathForTesting('client/backup/backup-system-incl-system.js')],
  'BackupAuthNoSysTests': [tu.pathForTesting('client/backup/backup-system-excl-system.js')]
};

const failPreStartMessage = (msg) => {
  return {
    state: false,
    shutdown: true,
    message: msg
  };
};

var dumpPath;



class backupTestRunner extends trs.runInArangoshRunner {
  constructor(options, testname, useAuth, user, restoreDir, checkUsers=[]) {
    super(options, testname, {}, tr.sutFilters.checkCollections.concat(checkUsers));
    this.user = user;
    this.useAuth = useAuth;
    this.dumpPath = undefined;
    this.restoreDir = restoreDir;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / We start a temporary system to generate the dumps that are agnostic
  // / of whether its a cluster or not.
  // //////////////////////////////////////////////////////////////////////////////
  generateDumpData() {
    let options = _.clone(this.options);
    if (this.dumpPath !== undefined) {
      return dumpPath;
    }

    this.instanceManager = new im.instanceManager(this.options.protocol,
                                                  this.options,
                                                  _.clone(tu.testServerAuthInfo),
                                                  'backup',
                                                  fs.join(fs.getTempPath(), this.friendlyName));
          
    this.instanceManager.prepareInstance();
    this.instanceManager.launchTcpDump("");
    if (!this.instanceManager.launchInstance()) {
      return failPreStartMessage('failed to start dataGenerator server!');
    }
    this.instanceManager.reconnect();

    log('Setting up');
    let path = '';

    try {
      let setup = this.runOneTest(makePath('backup-setup.js'));
      if (!setup.status === true || !this.healthCheck()) {
        log('Setup failed');
        setup.failed = 1;
        setup.state = false;
        return setup;
      }

      log('Create dump _system incl system collections');
      path = this.instanceManager.rootDir;

      _.defaults(asRoot, options);
      let dump = ct.run.arangoDumpRestore(asRoot, this.instanceManager,
                                          'dump', '_system', path, syssys,
                                          true, options.coreCheck);
      if (dump.status === false || !this.healthCheck()) {
        log('Dump failed');
        dump.failed = 1;
        dump.state = false;
        return dump;
      }

      log('Create dump _system excl system collections');

      dump = ct.run.arangoDumpRestore(asRoot, this.instanceManager, 'dump', '_system',
                                      path, sysNoSys, false, options.coreCheck);
      if (dump.status === false || !this.healthCheck()) {
        log('Dump failed: ' + JSON.stringify(dump));
        dump.failed = 1;
        dump.state = false;
        return dump;
      }

      log('Dump successful');
    } finally {
      log('Shutting down dump server');

      if (this.healthCheck()) {
        options = Object.assign({}, options, tu.testServerAuthInfo);
        if (!this.instanceManager.shutdownInstance()) {
          path = {
            state: false,
            failed: 1,
            shutdown: false,
            message: "shutdown of dump server failed"
          };
        }
        this.instanceManager.arangods[0].pid = null;
        this.instanceManager.arangods[0].exitStatus = null;
        fs.removeDirectoryRecursive(this.instanceManager.arangods[0].dataDir);
        fs.makeDirectory(this.instanceManager.arangods[0].dataDir);
        
      }
      log('done.');
      print();
    }

    dumpPath = path;
    return path;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / set up the test according to the testcase.
  // //////////////////////////////////////////////////////////////////////////////
  preStart() {
    this.dumpPath = this.generateDumpData();
    if (typeof this.dumpPath === 'object' && this.dumpPath.failed === 1) {
      log('DUMPING FAILED!');
      return this.dumpPath;
    }
    if (this.useAuth) {
      this.serverOptions = _.clone(tu.testServerAuthInfo);
      this.options = Object.assign({}, this.options, tu.testServerAuthInfo);
    } else {
      this.serverOptions['server.authentication'] = 'false';
    }
    return {
      state: true,
      shutdown: true
    };
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / set up the test according to the testcase.
  // //////////////////////////////////////////////////////////////////////////////
  postStart(){
    let restore = ct.run.arangoDumpRestore(this.user,
                                           this.instanceManager,
                                           'restore',
                                           '_system',
                                           this.dumpPath,
                                           this.restoreDir,
                                           true,
                                           this.options.coreCheck);

    if (restore.status === false || !this.healthCheck()) {
      log('Restore failed');
      restore.failed = 1;
      return {
        state: false,
        message: restore.message,
        shutdown: true
      };
    }

    return {
      shutdown: true,
      state: true
    };
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / testcases themselves
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: backup No Authentication, with System collections
// //////////////////////////////////////////////////////////////////////////////

const BackupNoAuthSysTests = (options) => {
  log('Test dump without authentication, restore _system incl system collections');

  return new backupTestRunner(options,
                              'BackupNoAuthSysTests',
                              tr.sutFilters.checkUsers,
                              {},
                              syssys).run(
                                testPaths.BackupNoAuthSysTests);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: backup No Authentication, no System collections
// //////////////////////////////////////////////////////////////////////////////

const BackupNoAuthNoSysTests = (options) => {
  log('Test dump without authentication, restore _system excl system collections');
  return new backupTestRunner(options,
                              'BackupNoAuthSysTests',
                              tr.sutFilters.checkUsers,
                              {},
                              sysNoSys).run(
                                testPaths.BackupNoAuthNoSysTests);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: backup with Authentication, with System collections
// //////////////////////////////////////////////////////////////////////////////

const BackupAuthSysTests = (options) => {
  log('Test dump with authentication, restore _system incl system collections');
  return new backupTestRunner(options,
                              'BackupAuthSysTests',
                              [],
                              asRoot,
                              syssys).run(
                                testPaths.BackupAuthSysTests);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: backup with Authentication, no System collections
// //////////////////////////////////////////////////////////////////////////////

const BackupAuthNoSysTests = (options) => {
  log('Test dump with authentication, restore _system excl system collections');
  return new backupTestRunner(options,
                              'BackupAuthNoSysTests',
                              [],
                              asRoot,
                              sysNoSys).run(
                                testPaths.BackupAuthNoSysTests);
};

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['BackupNoAuthSysTests'] = BackupNoAuthSysTests;
  testFns['BackupNoAuthNoSysTests'] = BackupNoAuthNoSysTests;
  testFns['BackupAuthSysTests'] = BackupAuthSysTests;
  testFns['BackupAuthNoSysTests'] = BackupAuthNoSysTests;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
