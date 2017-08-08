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
  'backup': 'complete backup tests with and without authentication'
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

const failMessage = (msg) => {
  return {
      failed: 1,
      'backup': {
        status: false,
        message: msg
      }
    };
};

const generateDumpData = (options, useAuth, user) => {
   const auth = {
    'server.authentication': useAuth ? 'true' : 'false'
  };

  let instanceInfo = pu.startInstance('tcp', options, auth, 'backup');

  if (instanceInfo === false) {
    return failMessage('failed to start dataGenerator server!');
  }

  log('Setting up');
  let path = '';

  try {
    let setup = tu.runInArangosh(options, instanceInfo, makePath('backup-setup.js'), auth);
    if (!setup.status === true || !isAlive(instanceInfo, options)) {
      log('Setup failed');
      setup.failed = 1;
      return setup;
    }

    log('Create dump _system incl system collections');
    path = instanceInfo.rootDir;

    _.defaults(asRoot, options);

    let dump = pu.run.arangoDumpRestore(asRoot, instanceInfo, 'dump', '_system', path, syssys, true);
    if (dump.status === false || !isAlive(instanceInfo, options)) {
      log('Dump failed');
      dump.failed = 1;
      return dump;
    }

    log('Create dump _system excl system collections');

    dump = pu.run.arangoDumpRestore(asRoot, instanceInfo, 'dump', '_system', path, sysNoSys, false);
    if (dump.status === false || !isAlive(instanceInfo, options)) {
      log('Dump failed');
      dump.failed = 1;
      return dump;
    }

    log('Dump successful, using auth ' + useAuth);
  } finally {
    log('Shutting down dump server');
    if (isAlive(instanceInfo, options)) {
      pu.shutdownInstance(instanceInfo, options);
    }
    log('done.');
    print();
  }

  return path;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: backup
// //////////////////////////////////////////////////////////////////////////////

const allBackupTests = (options) => {
  log("Starting tests");

  log("Test dump without authentication");
  let path = generateDumpData(options, false);
  if (path.failed === 1) {
    // Sorry dumping failed.
    return path;
  }

  let res;
  log("Switch off Authentication");

  log("Test restore _system incl system collections");
  res = backupTest(options, '_system', path, syssys, 'backup-system-incl-system.js', false);
  if (res.failed === 1) {
    return res;
  }

  log("Test restore _system excl system collections");
  res = backupTest(options, '_system', path, sysNoSys, 'backup-system-excl-system.js', false);
  if (res.failed === 1) {
    return res;
  }

  log("Switch on Authentication");

  log("Test restore _system incl system collections");
  res = backupTest(options, '_system', path, syssys, 'backup-system-incl-system.js', true, asRoot);
  if (res.failed === 1) {
    return res;
  }

  log("Test restore _system excl system collections");
  res = backupTest(options, '_system', path, sysNoSys, 'backup-system-excl-system.js', true, asRoot);
  if (res.failed === 1) {
    return res;
  }

  return res;
};


const backupTest = (options, db, path, folder, testFile, useAuth, user = {}) => {
  const auth = { };
  if (useAuth) {
    auth['server.authentication'] = 'true';
    auth['server.jwt-secret'] = 'haxxmann';
  } else {
    auth['server.authentication'] = 'false';
  }

  log('Reboot fresh instance');
  let instanceInfo = pu.startInstance('tcp', options, auth, 'backup');
  if (instanceInfo === false) {
    return failMessage('failed to start fresh server for backup!');
  }

  try {
    let restore = pu.run.arangoDumpRestore(user, instanceInfo, 'restore', db, path, folder);
    if (restore.status === false || !isAlive(instanceInfo, options)) {
      log('Restore failed');
      restore.failed = 1;
      return restore;
    }

    log('Validate Result');
    let test = tu.runInArangosh(user, instanceInfo, makePath(testFile), { 'server.database': db });

    if (test.status === false || !isAlive(instanceInfo, options)) {
      log('Validation failed');
      test.failed = 1;
      return test;
    }
  } finally {
    log('Shutting down...');
    if (useAuth) {
      options['server.authentication'] = auth['server.authentication'];
      options['server.jwt-secret'] = auth['server.jwt-secret'];
    }
    if (isAlive(instanceInfo, options)) {
      pu.shutdownInstance(instanceInfo, options);
    }
    log('done.');
    print();
  }

  return { failed: 0 };
};

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['backup'] = allBackupTests;
  defaultFns.push('backup');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
