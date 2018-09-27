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
  let cluster;
  let notCluster = getClusterStrings(options).notCluster;
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
  let fn = afterServerStart(instanceInfo);
  
  print(CYAN + Date() + ': Setting up' + RESET);
  
  let results = { failed: 1 };
  results.setup = tu.runInArangosh(
    options,
    instanceInfo,
    tu.makePathUnix(fs.join(testPaths[which][0], tstFiles.dumpSetup)),
    clientAuth);

  results.setup.failed = 1;

  if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
      (results.setup.status === true)) {
    results.setup.failed = 0;

    print(CYAN + Date() + ': ' + which + ' and Restore - dump' + RESET);

    results.dump = pu.run.arangoDumpRestore(
      dumpOptions,
      instanceInfo,
      'dump',
      'UnitTestsDumpSrc');

    results.dump.failed = 1;
    if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
        (results.dump.status === true)) {
      results.dump.failed = 0;

      print(CYAN + Date() + ': ' + which + ' and Restore - restore' + RESET);

      results.restore = pu.run.arangoDumpRestore(
        dumpOptions,
        instanceInfo,
        'restore',
        'UnitTestsDumpDst');

      results.restore.failed = 1;
      if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
          (results.restore.status === true)) {
        results.restore.failed = 0;

        print(CYAN + Date() + ': ' + which + ' and Restore - dump after restore' + RESET);

        results.test = tu.runInArangosh(
          options,
          instanceInfo,
          tu.makePathUnix(
            fs.join(testPaths[which][0],
                    tstFiles.dumpAgain)),
          {
	    'server.database': 'UnitTestsDumpDst'
          });
        results.test.failed = 1;
        if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
            (results.test.status === true)) {
          results.test.failed = 0;

          print(CYAN + Date() + ': ' + which + ' and Restore - teardown' + RESET);

          results.tearDown = tu.runInArangosh(
            options,
            instanceInfo,
            tu.makePathUnix(
              fs.join(testPaths[which][0],
                      tstFiles.dumpTearDown)));
          results.tearDown.failed = 1;
          if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
              (results.tearDown.status === true)) {
            results.tearDown.failed = 0;

	    if (tstFiles.dumpCheckGraph === false) {
              results.failed = 0;
	    } else {
              print(CYAN + Date() + ': Dump and Restore - restoreOld' + RESET);
              let restoreDir = tu.makePathUnix(tu.pathForTesting('server/dump/dump' + notCluster));

              results.restoreOld = pu.run.arangoDumpRestore(
                options,
                instanceInfo,
                'restore',
                '_system',
                pu.TOP_DIR,
                restoreDir);
              results.restoreOld.failed = 1;
              if (pu.arangod.check.instanceAlive(instanceInfo, options) &&
                  (results.restoreOld.status === true)) {
                results.restoreOld.failed = 0;

                results.testRestoreOld = tu.runInArangosh(
                  options,
                  instanceInfo,
                  fs.join(testPaths[which][0],
                          tstFiles.dumpCheckGraph));

                results.testRestoreOld.failed = 1;

                if (results.testRestoreOld.status) {
                  results.testRestoreOld.failed = 10;
                  results.failed = 0;
                }
              }
            }
          }
        }
      }
    }
  }
  if (fn !== undefined) {
    fs.remove(fn);
  }
  print(CYAN + 'Shutting down...' + RESET);
  pu.shutdownInstance(instanceInfo, options);
  print(CYAN + 'done.' + RESET);

  print();

  return results;
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
    dumpTearDown: 'dump-teardown.js',
    dumpCheckGraph: false
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
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: false
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
