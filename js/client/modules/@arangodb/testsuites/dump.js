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

const tu = require('@arangodb/testutils/test-utils');
const fs = require('fs');
const _ = require('lodash');
const { DumpRestoreHelper, getClusterStrings } = require('@arangodb/testutils/dump');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const functionsDocumentation = {
  'dump': 'dump tests',
  'dump_mixed_cluster_single': 'dump tests - dump cluster restore single',
  'dump_mixed_single_cluster': 'dump tests - dump single restore cluster',
  'dump_authentication': 'dump tests with authentication',
  'dump_jwt': 'dump tests with JWT',
  'dump_encrypted': 'encrypted dump tests',
  'dump_maskings': 'masked dump tests',
  'dump_multiple_same': 'restore multiple DBs at once to the same installation',
  'dump_multiple_two': 'restore multiple DBs at once to a fresh installation',
  'dump_with_crashes': 'restore and crash the client multiple times',
  'dump_with_crashes_parallel': 'restore and crash the client multiple times - parallel version',
  'dump_parallel': 'use experimental parallel dump',
};

const optionsDocumentation = [
  '   - `dumpVPack`: if set to true to enable vpack dumping',
];

const testPaths = {
  'dump': [tu.pathForTesting('client/dump')],
  'dump_mixed_cluster_single': [tu.pathForTesting('client/dump')],
  'dump_mixed_single_cluster': [tu.pathForTesting('client/dump')],
  'dump_authentication': [tu.pathForTesting('client/dump')],
  'dump_jwt': [tu.pathForTesting('client/dump')],
  'dump_encrypted': [tu.pathForTesting('client/dump')],
  'dump_maskings': [tu.pathForTesting('client/dump')],
  'dump_multiple_same': [tu.pathForTesting('client/dump')],
  'dump_multiple_two': [tu.pathForTesting('client/dump')],
  'dump_with_crashes': [tu.pathForTesting('client/dump')],
  'dump_with_crashes_parallel': [tu.pathForTesting('client/dump')],
  'dump_parallel': [tu.pathForTesting('client/dump')],
};

function dump_backend_two_instances (firstRunOptions, secondRunOptions,
                                     serverAuthInfo, clientAuth,
                                     dumpOptions, restoreOptions,
                                     which, tstFiles, afterServerStart,
                                     rtaArgs, restartServer) {
  print(CYAN + which + ' tests...' + RESET);
  const helper = new DumpRestoreHelper(firstRunOptions, secondRunOptions, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, afterServerStart, rtaArgs, restartServer);
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
  return dump_backend_two_instances(options, options, serverAuthInfo, clientAuth, dumpOptions, restoreOptions, which, tstFiles, afterServerStart, rtaArgs, false);
}

function dump (options) {
  let opts = _.clone(options);
  if (opts.cluster) {
    opts.dbServers = 3;
  }
  opts.extraArgs['vector-index'] = true;

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
  clusterOptions.extraArgs['vector-index'] = true;
  let singleOptions = _.clone(options);
  singleOptions.cluster = false;
  singleOptions.extraArgs['vector-index'] = true;
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
                                      // BTS-1617: disable 404 for now
                                      '--skip', '404'], true);
}

function dumpMixedSingleCluster (options) {
  let clusterOptions = _.clone(options);
  clusterOptions.cluster = true;
  clusterOptions.dbServers = 3;
  clusterOptions.extraArgs['vector-index'] = true;
  let singleOptions = _.clone(options);
  singleOptions.cluster = false;
  singleOptions.extraArgs['vector-index'] = true;
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
                                      '--skip', '550,900,960'], true);
}

function dumpMultipleTwo (options) {
  let dumpOptions = {
    dumpVPack: options.dumpVPack,
    dbServers: 3,
    allDatabases: true,
    deactivateCompression: true,
    parallelDump: true,
    splitFiles: true,
    extraArgs: { 'vector-index': true },
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

  return dump_backend_two_instances(dumpOptions, _.clone(dumpOptions), {}, {}, dumpOptions, dumpOptions,
                                    'dump_multiple_two', tstFiles, function(){}, [], true);
}
function dumpMultipleSame (options) {
  let dumpOptions = {
    dumpVPack: options.dumpVPack,
    dbServers: 3,
    allDatabases: true,
    deactivateCompression: true,
    parallelDump: true,
    splitFiles: true,
    extraArgs: { 'vector-index': true },
  };
  _.defaults(dumpOptions, options);
  let c = getClusterStrings(dumpOptions);
  let tstFiles = {
    dumpSetup: 'dump-setup' + c.cluster +'.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-uncompressed.js',
    dumpCleanup: 'cleanup-multiple.js',
    dumpAgain: 'dump' + c.cluster + '.js',
    dumpTearDown: 'dump-teardown' + c.cluster + '.js',
    dumpCheckGraph: 'check-graph-multiple.js'
  };

  return dump_backend_two_instances(dumpOptions, _.clone(dumpOptions), {}, {},
                                    dumpOptions, dumpOptions,
                                    'dump_multiple_same', tstFiles, function(){}, [], false);
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
    extraArgs: { 'vector-index': true },
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
    extraArgs: { 'vector-index': true },
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
  dumpAuthOpts.extraArgs['vector-index'] = true;
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
    extraArgs: { 'vector-index': true },
    multipleDumps: true,
    dbServers: 3
  });

  let ret = dump_backend(opts, tu.testServerAuthInfo, clientAuth, dumpAuthOpts, restoreAuthOpts, 'dump_jwt', tstFiles, function(){}, []);
  options.cleanup = opts.cleanup;
  return ret;
}

function dumpEncrypted (options) {
  // test is only meaningful in the Enterprise Edition
  let c = getClusterStrings(options);

  let afterServerStart = function(instanceManager) {
    let keyFile = fs.join(instanceManager.rootDir, 'secret-key');
    fs.write(keyFile, 'DER-HUND-der-hund-der-hund-der-h'); // must be exactly 32 chars long
    return keyFile;
  };

  let dumpOptions = _.clone(options);
  dumpOptions.extraArgs['vector-index'] = true;
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
  dumpOptions.extraArgs['vector-index'] = true;

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
  let tstFiles = {
    dumpSetup: 'dump-maskings-setup.js',
    dumpCheckDumpFiles: 'dump-check-dump-files-nothing.js',
    dumpCleanup: 'cleanup-nothing.js',
    dumpAgain: 'dump-maskings.js',
    dumpTearDown: 'dump-teardown.js'
  };

  let dumpMaskingsOpts = {
    extraArgs: { 'vector-index': true },
    maskings: 'maskings1.json',
    dbServers: 3
  };

  _.defaults(dumpMaskingsOpts, options);

  return dump_backend(dumpMaskingsOpts, {}, {}, dumpMaskingsOpts, options, 'dump_maskings', tstFiles, function(){}, []);
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
  testFns['dump_multiple_same'] = dumpMultipleSame;
  testFns['dump_multiple_two'] = dumpMultipleTwo;
  testFns['dump_with_crashes'] = dumpWithCrashes;
  testFns['dump_with_crashes_non_parallel'] = dumpWithCrashesNonParallel;
  testFns['dump_non_parallel'] = dumpNonParallel;

  tu.CopyIntoList(optionsDoc, optionsDocumentation);
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
