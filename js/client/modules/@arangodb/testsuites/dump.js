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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'dump': 'dump tests',
  'dump_encrypted': 'encrypted dump tests',
  'dump_authentication': 'dump tests with authentication'
};
const optionsDocumentation = [
  '   - `skipEncrypted` : if set to true the encryption tests are skipped'
];

const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const fs = require('fs');
const _ = require('lodash');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'dump': [tu.pathForTesting('server/dump')],
  'dump_encrypted': [tu.pathForTesting('server/dump')],
  'dump_authentication': [tu.pathForTesting('server/dump')]
};

class DumpRestoreHelper {
  constructor(instanceInfo, options, clientAuth, dumpOptions, afterServerStart) {
    this.instanceInfo = instanceInfo;
    this.options = options;
    this.clientAuth = clientAuth;
    this.dumpOptions = dumpOptions;
    this.fn = afterServerStart(instanceInfo);
    this.results = {failed: 1};
    this.arangosh = tu.runInArangosh.bind(this, this.options, this.instanceInfo);
    this.arangorestore = pu.run.arangoDumpRestore.bind(this, this.dumpOptions, this.instanceInfo, 'restore');
    this.arangodump = pu.run.arangoDumpRestore.bind(this, this.dumpOptions, this.instanceInfo, 'dump');
  }

  isAlive() {
    return pu.arangod.check.instanceAlive(this.instanceInfo, this.options);
  }

  validate(phaseInfo) {
    phaseInfo.failed = (phaseInfo.status !== true || !this.isAlive() ? 1 : 0);
    return phaseInfo.failed === 0;
  };
 
  extractResults() {
    if (this.fn !== undefined) {
      fs.remove(this.fn);
    }
    print(CYAN + 'Shutting down...' + RESET);
    pu.shutdownInstance(this.instanceInfo, this.options);
    print(CYAN + 'done.' + RESET);

    print();

    return this.results;
  }

  runSetupSuite(path) {
    this.results.setup = this.arangosh(path, this.clientAuth);
    return this.validate(this.results.setup);
  }

  dumpFrom(database) {
    this.results.dump = this.arangodump(database);
    return this.validate(this.results.dump);
  }

  restoreTo(database) {
    this.results.restore = this.arangorestore(database);
    return this.validate(this.results.restore);
  }

  runTests(file, database) {
    this.results.test = this.arangosh(file, {'server.database': database});
    return this.validate(this.results.test);
  }

  tearDown(file) {
    this.results.tearDown = this.arangosh(file);
    return this.validate(this.results.tearDown);
  }

  restoreOld(directory) {
    this.results.restoreOld = this.arangorestore('_system', pu.TOP_DIR, directory);
    return this.validate(this.results.restoreOld);
  }

  testRestoreOld(file) {
    this.results.testRestoreOld = this.arangosh(file);
    return this.validate(this.results.testRestoreOld);
  }
};

function getClusterStrings(options)
{
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: dump
// //////////////////////////////////////////////////////////////////////////////
function dump_backend (options, serverAuthInfo, clientAuth, dumpOptions, which, tstFiles, afterServerStart) {
  print(CYAN + which + ' tests...' + RESET);

  let instanceInfo = pu.startInstance('tcp', options, serverAuthInfo, which);

  if (instanceInfo === false) {
    let rc =  {
      failed: 1,
    };
    rc[which] = {
      status: false,
      message: 'failed to start server!'
    };
    return rc;
  }
  const helper = new DumpRestoreHelper(instanceInfo, options, clientAuth, dumpOptions, afterServerStart);
 
  print(CYAN + Date() + ': Setting up' + RESET);

  const setupFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpSetup));
  if (!helper.runSetupSuite(setupFile)) {
    return helper.extractResults();
  }

  print(CYAN + Date() + ': ' + which + ' and Restore - dump' + RESET);

  if (!helper.dumpFrom('UnitTestsDumpSrc')) {
    return helper.extractResults();
  }

  print(CYAN + Date() + ': ' + which + ' and Restore - restore' + RESET);

  if (!helper.restoreTo('UnitTestsDumpDst')) {
    return helper.extractResults();
  }

  print(CYAN + Date() + ': ' + which + ' and Restore - dump after restore' + RESET);

  const testFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpAgain));
  if (!helper.runTests(testFile,'UnitTestsDumpDst')) {
    return helper.extractResults();
  }

  print(CYAN + Date() + ': ' + which + ' and Restore - teardown' + RESET);

  const tearDownFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpTearDown));
  if (!helper.tearDown(tearDownFile)) {
    return helper.extractResults();
  }

  if (tstFiles.hasOwnProperty("dumpCheckGraph")) {
    print(CYAN + Date() + ': Dump and Restore - restoreOld' + RESET);
    let notCluster = getClusterStrings(options).notCluster;
    let restoreDir = tu.makePathUnix(tu.pathForTesting('server/dump/dump' + notCluster));
    if (!helper.restoreOld(restoreDir)) {
      return helper.extractResults();
    }

    const oldTestFile = tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpCheckGraph));
    if (!helper.testRestoreOld(oldTestFile)) {
      return helper.extractResults();
    }
  }
  return helper.extractResults();
}

// /////////////////////////////////////////////////////////////////////////////

function dump (options) {
  let c = getClusterStrings(options);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpAgain: 'dump-' + options.storageEngine + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph.js'
  };

  return dump_backend(options, {}, {}, options, 'dump', tstFiles, function(){});
}

function dumpAuthentication (options) {
  if (options.cluster) {
    if (options.extremeVerbosity) {
      print(CYAN + 'Skipped because of cluster.' + RESET);
    }

    return {
      'dump_authentication': {
        'status': true,
        'message': 'skipped because of cluster',
        'skipped': true
      }
    };
  }

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

  _.defaults(dumpAuthOpts, options);
  let tstFiles = {
    dumpSetup: 'dump-authentication-setup.js',
    dumpAgain: 'dump-authentication.js',
    dumpTearDown: 'dump-teardown.js'
  };

  return dump_backend(options, serverAuthInfo, clientAuth, dumpAuthOpts, 'dump_authentication', tstFiles, function(){});

}

function dumpEncrypted (options) {
  // test is only meaningful in the enterprise version
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
  
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster + '.js',
    dumpAgain: 'dump-' + options.storageEngine + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js'
  };

  return dump_backend(options, {}, {}, dumpOptions, 'dump_encrypted', tstFiles, afterServerStart);
}

// /////////////////////////////////////////////////////////////////////////////
exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['dump'] = dump;
  defaultFns.push('dump');

  testFns['dump_encrypted'] = dumpEncrypted;
  defaultFns.push('dump_encrypted');

  testFns['dump_authentication'] = dumpAuthentication;
  defaultFns.push('dump_authentication');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
