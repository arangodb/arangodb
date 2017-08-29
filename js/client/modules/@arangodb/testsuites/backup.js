/* jshint strict: false, sub: true */
/* global print */
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
// / @author Michael Hackstein
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'BackupNoAuthSysTests':   'complete backup tests without authentication, with    system collections',
  'BackupNoAuthNoSysTests': 'complete backup tests without authentication, without system collections',
  'BackupAuthSysTests':     'complete backup tests with    authentication, with    system collections',
  'BackupAuthNoSysTests':   'complete backup tests with    authentication, without system collections'
};
const optionsDocumentation = [
];

const _ = require('lodash');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');

const CYAN = require('internal').COLORS.COLOR_CYAN;
const RESET = require('internal').COLORS.COLOR_RESET;
const log = (text) => {
  print(`${CYAN}${Date()}: Backup - ${text}${RESET}`);
};
const makePath = (name) => {
  return tu.makePathUnix(`js/server/tests/backup/${name}`);
};

const isAlive = (info, options) => {
  return pu.arangod.check.instanceAlive(info, options);
};

const asRoot = {
  username: 'root',
  password: ''
};

const syssys = 'systemsystem';
const sysNoSys = 'systemnosystem';

const failPreStartMessage = (msg) => {
  return {
    state: false,
    message: msg
  };
};

var dumpPath;

// //////////////////////////////////////////////////////////////////////////////
// / We start a temporary system to generate the dumps that are agnostic
// / of whether its a cluster or not.
// //////////////////////////////////////////////////////////////////////////////
const generateDumpData = (options) => {
  if (dumpPath !== undefined) {
    return dumpPath;
  }
  const auth = {
    'server.authentication': 'false'
  };

  let instanceInfo = pu.startInstance('tcp', options, auth, 'backup');

  if (instanceInfo === false) {
    return failPreStartMessage('failed to start dataGenerator server!');
  }

  log('Setting up');
  let path = '';

  try {
    let setup = tu.runInArangosh(options, instanceInfo, makePath('backup-setup.js'), auth);
    if (!setup.status === true || !isAlive(instanceInfo, options)) {
      log('Setup failed');
      setup.failed = 1;
      setup.state = false;
      return setup;
    }

    log('Create dump _system incl system collections');
    path = instanceInfo.rootDir;

    _.defaults(asRoot, options);

    let dump = pu.run.arangoDumpRestore(asRoot, instanceInfo, 'dump', '_system', path, syssys, true);
    if (dump.status === false || !isAlive(instanceInfo, options)) {
      log('Dump failed');
      dump.failed = 1;
      dump.state = false;
      return dump;
    }

    log('Create dump _system excl system collections');

    dump = pu.run.arangoDumpRestore(asRoot, instanceInfo, 'dump', '_system', path, sysNoSys, false);
    if (dump.status === false || !isAlive(instanceInfo, options)) {
      log('Dump failed');
      dump.failed = 1;
      dump.state = false;
      return dump;
    }

    log('Dump successful');
  } finally {
    log('Shutting down dump server');
    if (isAlive(instanceInfo, options)) {
      pu.shutdownInstance(instanceInfo, options);
    }
    log('done.');
    print();
  }

  dumpPath = path;
  return path;
};

// //////////////////////////////////////////////////////////////////////////////
// / set up the test according to the testcase.
// //////////////////////////////////////////////////////////////////////////////
const setServerOptions = (options, serverOptions, customInstanceInfos, startStopHandlers) => {
  let path = generateDumpData(_.clone(options));
  if (typeof path === 'object' && path.failed === 1) {
    log('DUMPING FAILED!');
    return path;
  }

  startStopHandlers['path'] = path;

  const auth = { };
  if (startStopHandlers.useAuth) {
    serverOptions['server.authentication'] = 'true';
    serverOptions['server.jwt-secret'] = 'haxxmann';
  } else {
    serverOptions['server.authentication'] = 'false';
  }
  return {
    state: true
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / set up the test according to the testcase.
// //////////////////////////////////////////////////////////////////////////////
const setupBackupTest = (options, serverOptions, instanceInfo, customInstanceInfos, startStopHandlers) => {
  let restore = pu.run.arangoDumpRestore(startStopHandlers.user,
                                         instanceInfo,
                                         'restore',
                                         '_system',
                                         startStopHandlers.path,
                                         startStopHandlers.restoreDir);

  if (restore.status === false || !isAlive(instanceInfo, options)) {
    log('Restore failed');
    restore.failed = 1;
    return {
      state: false,
      message: restore.message
    };
  }

  return {
    state: true
  };
};

// //////////////////////////////////////////////////////////////////////////////
// / testcases themselves
// //////////////////////////////////////////////////////////////////////////////

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: backup No Authentication, with System collections
// //////////////////////////////////////////////////////////////////////////////

const BackupNoAuthSysTests = (options) => {
  log('Test dump without authentication, restore _system incl system collections');

  let startStopHandlers = {
    preStart: setServerOptions,
    postStart: setupBackupTest,
    useAuth: false,
    user: {},
    restoreDir: syssys
  };

  return tu.performTests(options,
                         ['js/server/tests/backup/backup-system-incl-system.js'],
                         'BackupNoAuthSysTests',
                         tu.runInArangosh, {},
                         startStopHandlers);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: backup No Authentication, no System collections
// //////////////////////////////////////////////////////////////////////////////

const BackupNoAuthNoSysTests = (options) => {
  log('Test dump without authentication, restore _system excl system collections');

  let startStopHandlers = {
    preStart: setServerOptions,
    postStart: setupBackupTest,
    useAuth: false,
    user: {},
    restoreDir: sysNoSys
  };

  return tu.performTests(options,
                         ['js/server/tests/backup/backup-system-excl-system.js'],
                         'BackupNoAuthNoSysTests',
                         tu.runInArangosh, {},
                         startStopHandlers);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: backup with Authentication, with System collections
// //////////////////////////////////////////////////////////////////////////////

const BackupAuthSysTests = (options) => {
  log('Test dump with authentication, restore _system incl system collections');

  let startStopHandlers = {
    preStart: setServerOptions,
    postStart: setupBackupTest,
    useAuth: true,
    user: asRoot,
    restoreDir: syssys
  };

  return tu.performTests(options,
                         ['js/server/tests/backup/backup-system-incl-system.js'],
                         'BackupAuthSysTests',
                         tu.runInArangosh, {},
                         startStopHandlers);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: backup with Authentication, no System collections
// //////////////////////////////////////////////////////////////////////////////

const BackupAuthNoSysTests = (options) => {
  log('Test dump with authentication, restore _system excl system collections');

  let startStopHandlers = {
    preStart: setServerOptions,
    postStart: setupBackupTest,
    useAuth: true,
    user: asRoot,
    restoreDir: sysNoSys
  };

  return tu.performTests(options,
                         ['js/server/tests/backup/backup-system-excl-system.js'],
                         'BackupAuthNoSysTests',
                         tu.runInArangosh, {},
                         startStopHandlers);
};

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['BackupNoAuthSysTests'] = BackupNoAuthSysTests;
  testFns['BackupNoAuthNoSysTests'] = BackupNoAuthNoSysTests;
  testFns['BackupAuthSysTests'] = BackupAuthSysTests;
  testFns['BackupAuthNoSysTests'] = BackupAuthNoSysTests;

  defaultFns.push('BackupNoAuthSysTests');
  defaultFns.push('BackupNoAuthNoSysTests');
  defaultFns.push('BackupAuthSysTests');
  defaultFns.push('BackupAuthNoSysTests');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
