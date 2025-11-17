/* jshint strict: false, sub: true */
/* global print, arango, db */
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
const errors = require('internal').errors;

const functionsDocumentation = {
  'hot_backup': 'hotbackup tests'
};

const testPaths = {
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

  let which = "hot_backup";
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
  }
  helper.destructor(true);
  if (helper.doCleanup) {
    fs.removeDirectoryRecursive(keyDir, true);
  }
  return helper.extractResults();
}

function hotBackup_load_backend (options, which, args) {
  const encryptionKey = '01234567890123456789012345678901';
  console.warn(options);
  options.extraArgs['experimental-vector-index'] = true;
  if (options.hasOwnProperty("dbServers") && options.dbServers > 1) {
    options.dbServers = 3;
  }

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
    print(1)
    helper.destructor(false);
    return helper.extractResults();
  }

  try {
    if (
        //!helper.runRtaMakedata() ||
        !helper.isAlive() ||
        !helper.runTestFn(args.preRestoreFn) ||
        !helper.spawnStressArangosh(args.noiseScript, which) ||
        !helper.createHotBackup() ||
        !helper.stopStressArangosh() ||
        !helper.restoreHotBackup() ||
        !helper.runTestFn(args.postRestoreFn) //||
        //!helper.runRtaCheckData()
    ) {
      print(2)
      helper.destructor(true);
      return helper.extractResults();
    }

  }
  catch (ex) {
    print("Caught exception during testrun: " + ex);
  }
  print(4)
  helper.destructor(true);
  if (helper.doCleanup) {
    fs.removeDirectoryRecursive(keyDir, true);
  }
  return helper.extractResults();
}

function hotBackup_load (options) {

  let which = "hot_backup_load";
  return hotBackup_load_backend(options, which, {
    noiseScript: "",
    preRestoreFn: function() {
      return {status: true, testresult: {}};
    },
    postRestoreFn:function() {
      return {status: true, testresult: {}};
    }
  });
}

function hotBackup_views (options) {
  let testCol1;
  let testCol2;
  let txn;
  let txn_col;
  let which = "hot_backup_load";
  return hotBackup_load_backend(options, which, {
    noiseScript: `
let i=0;
while(true) {
  console.log('Noise starts');
  try {
    db._useDatabase('test_view');
    for (; i < 10000; i++) {
        db.test_collection.insert({"number": i, "field1": "stone"});
    }
    print('done');
    break;
  }
  catch (ex) {
    console.log(ex);
//    require('internal').sleep(0.5);
//    try {
//      arango.reconnect(endpoint, '_system', 'root', passvoid)
//    } catch(ex) { console.log(ex); }
    break;
  }
}
`,
    preRestoreFn: function() {
      db._createDatabase('test_view');
      db._useDatabase('test_view');
      testCol1 = db._create('test_collection', {numberOfShards:20} );
      testCol1.ensureIndex({type: 'inverted', name: 'inverted', fields: ["field1"]});
      db._createView("test_view", "arangosearch",
                     {"links": {
                       "test_collection": {
                         "includeAllFields": true}}});
      testCol2 = db._create('test_collection2', {numberOfShards:20} );
      db._createView("test_view_squared", "arangosearch",
                     {"links": {
                       "test_collection2": {
                         "includeAllFields": true}}});
      txn = db._createTransaction({collections: {
        read: ["test_collection2"], write: ["test_collection2"]}});
      txn_col = txn.collection("test_collection2");
      txn_col.insert({"foo": "bar"});
      print('--------------------------------------------------------------------------------111')
      return {status: true, testresult: {}};
    },
    postRestoreFn:function() {
      try {
        print('--------------------------------------------------------------------------------211')
        db._useDatabase('test_view');
        let p = db._query(
          `LET x = (FOR v IN test_collection RETURN v._key)
               FOR w IN test_view
                 FILTER w._key NOT IN x
                 RETURN w
            `).toArray();
        print('--------------------------------------------------------------------------------311')
        if (p.length !== 0) {
          throw new Error(`query result wasn't empty: ${p}`);
        }
        let q = db._query(
          `LET x = (FOR v IN test_view RETURN v._key)
               FOR w IN test_collection
                 FILTER w._key NOT IN x
                 RETURN w
            `).toArray();
        if (q.length !== 0) {
          throw new Error(`query result wasn't empty: ${q}`);
        }
        print('--------------------------------------------------------------------------------411')

        // Check that the inverted index is consistent with the documents
        // in the collection.
        // I could not come up with a reliable way to do this: the AQL optimiser
        // is free to optimise the index into the first query, and then
        // this check is void. in the same way it could happen that the optimiser
        // decidesd that the filter in the second query is not best served by
        // the inverted index and the test is void.
        p = db._query("FOR v IN test_collection RETURN v._key").toArray();
        q = db._query(`
            FOR w IN test_collection
                 FILTER w.field1 == "stone"
                 RETURN w._key
            `).toArray();
        if (p !== q) {
          throw new Error(`query result wasn't empty: ${JSON.stringify(p)} != ${JSON.stringify(q)}`);
        }
        print('--------------------------------------------------------------------------------511')

        print('Aborting Transaction if still there');
        try {
          txn.abort();
        } catch (ex) {
          print('probably gone.');
          if (ex.errorNum !== errors.ERROR_TRANSACTION_NOT_FOUND.code) {
            print(ex);
            throw(ex);
          }
        }
        db._useDatabase('_system');
        db._dropDatabase('test_view');
        print('--------------------------------------------------------------------------------61')
        require('internal').sleep(30)
        return {status: true, testresult: {'all': {}}};
      }
      catch (ex) {
        print(ex)
        return {status: false, testresult: {'all': ex}};
      }
    }
  });
}


exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['hot_backup'] = hotBackup;
  testFns['hot_backup_load'] = hotBackup_load;
  testFns['hot_backup_views'] = hotBackup_views;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
