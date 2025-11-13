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
const hb = require("@arangodb/hotbackup");

const { DumpRestoreHelper, getClusterStrings } = require('@arangodb/testutils/dump');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const functionsDocumentation = {
  'hot_backup': 'hotbackup tests'
};

const testPaths = {
  'dump': [tu.pathForTesting('client/dump')],
  'hot_backup': [tu.pathForTesting('client/dump')]
};

function hotBackup (options) {
  const encryptionKey = '01234567890123456789012345678901';
  let c = getClusterStrings(options);
  console.warn(options);
  options.extraArgs['experimental-vector-index'] = true;
  if (options.hasOwnProperty("dbServers") && options.dbServers > 1) {
    options.dbServers = 3;
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

  const helper = new DumpRestoreHelper(options, options, addArgs, {}, options, options, which, function(){}, [], false);
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

    if (tstFiles.hasOwnProperty("foxxTest") && !options.skipServerJS) {
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
  testFns['hot_backup'] = hotBackup;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
