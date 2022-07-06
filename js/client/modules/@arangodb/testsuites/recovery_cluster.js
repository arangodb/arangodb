/* jshint strict: false, sub: true */
/* global print */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'recovery_cluster': 'run recovery tests for cluster'
};
const optionsDocumentation = [
];

const fs = require('fs');
const internal = require('internal');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const im = require('@arangodb/testutils/instance-manager');
const inst = require('@arangodb/testutils/instance');
const tmpDirMmgr = require('@arangodb/testutils/tmpDirManager').tmpDirManager;
const _ = require('lodash');

const toArgv = require('internal').toArgv;

const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const BLUE = require('internal').COLORS.COLOR_BLUE;

const termSignal = 15;

const testPaths = {
  'recovery_cluster': [tu.pathForTesting('server/recovery')]
};

/// ensure that we have enough db servers in cluster tests
function ensureServers(options, numServers) {
  if (options.cluster && options.dbServers < numServers) {
    let localOptions = _.clone(options);
    localOptions.dbServers = numServers;
    return localOptions;
  }
  return options;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: recovery_cluster
// //////////////////////////////////////////////////////////////////////////////

function runArangodRecovery (params, useEncryption) {
  let additionalParams= {
    'foxx.queues': 'false',
    'server.statistics': 'false',
    'log.foreground-tty': 'true',
    'database.ignore-datafile-errors': 'false', // intentionally false!
    'temp.path': params.temp_path
  };
  
  if (useEncryption) {
    // randomly turn on or off hardware-acceleration for encryption for both
    // setup and the actual test. given enough tests, this will ensure that we run
    // a good mix of accelerated and non-accelerated encryption code. in addition,
    // we shuffle between the setup and the test phase, so if there is any
    // incompatibility between the two modes, this will likely find it
    additionalParams['rocksdb.encryption-hardware-acceleration'] = (Math.random() * 100 >= 50) ? "true" : "false";
  }
  let argv = [];
  let binary = pu.ARANGOD_BIN;
  // for cluster runs we have separate parameter set for servers and for testagent(arangosh)
  let additionalTestParams  = {
    // arangosh has different name for parameter :(
    'javascript.execute':  params.script,
    'javascript.run-main': true,
    'temp.path': params.temp_path
  };

  if (params.setup) {
    additionalTestParams['server.request-timeout'] = '60';
    additionalTestParams['javascript.script-parameter'] = 'setup';

    // special handling for crash-handler recovery tests
    if (params.script.match(/crash-handler/)) {
      // forcefully enable crash handler, even if turned off globally
      // during testing
      internal.env["ARANGODB_OVERRIDE_CRASH_HANDLER"] = "on";
    }

    params.options.disableMonitor = true;
    params.options =  ensureServers(params.options);
    let args = {};
    
    // enable development debugging if extremeVerbosity is set
    if (params.options.extremeVerbosity === true) {
      args['log.level'] = 'development=info';
    }
    args = Object.assign(args, params.options.extraArgs);
    args = Object.assign(args, {
      'rocksdb.wal-file-timeout-initial': 10,
      'replication.auto-start': 'true'
    });

    if (useEncryption) {
      params.keyDir = fs.join(fs.getTempPath(), 'arango_encryption');
      if (!fs.exists(params.keyDir)) {  // needed on win32
        fs.makeDirectory(params.keyDir);
      }
        
      const key = '01234567890123456789012345678901';
      
      let keyfile = fs.join(params.keyDir, 'rocksdb-encryption-keyfile');
      fs.write(keyfile, key);

      // special handling for encryption-keyfolder tests
      if (params.script.match(/encryption-keyfolder/)) {
        args['rocksdb.encryption-keyfolder'] = params.keyDir;
        process.env["rocksdb-encryption-keyfolder"] = params.keyDir;
      } else {
        args['rocksdb.encryption-keyfile'] = keyfile;
        process.env["rocksdb-encryption-keyfile"] = keyfile;
      }
    }

    params.args = args;
      
  } else {
    additionalTestParams['javascript.script-parameter'] = 'recovery';
  }

  if (params.setup) {
    params.args = Object.assign(params.args, additionalParams);
    params.instanceManager = new im.instanceManager(params.options.protocol,
                                                    params.options,
                                                    params.args,
                                                    fs.join('recovery_cluster',
                                                            params.count.toString()));
    params.instanceManager.prepareInstance();
    params.instanceManager.launchTcpDump("");
    if (!params.instanceManager.launchInstance()) {
      params.instanceManager.destructor(false);
      return {
        export: {
          status: false,
          message: 'failed to start server!'
        }
      };
    }
  } else {
    print(BLUE + "Restarting cluster " + RESET);
    params.instanceManager.reStartInstance();
    let tryCount = 10;
    while(tryCount > 0 && !params.instanceManager._checkServersGOOD()) {
      print(RESET + "Waiting for all servers to go GOOD");
      internal.sleep(3); // give agency time to bootstrap DBServers
      --tryCount;
    }
  }
  params.instanceManager.reconnect();
  let agentArgs = pu.makeArgs.arangosh(params.options);
  agentArgs['server.endpoint'] = params.instanceManager.findEndpoint();
  if (params.args['log.level']) {
    agentArgs['log.level'] = params.args['log.level'];
  }

  Object.assign(agentArgs, additionalTestParams);
  require('internal').env.INSTANCEINFO = JSON.stringify(params.instanceManager.getStructure());
  try {
    pu.executeAndWait(pu.ARANGOSH_BIN, toArgv(agentArgs), params.options, 'arangosh', params.instanceManager.rootDir, false);
  } catch(err) {
    print('Error while launching test:' + err);
    params.instanceManager.shutdownInstance(false);
    params.instanceManager.destructor(false);
    return; // without test server test will for sure fail
  }
  if (params.setup) {
    let dbServers = params.instanceManager.arangods.filter(
      (a) => {
        return a.isRole(inst.instanceRole.dbServer) ||
          a.isRole(inst.instanceRole.coordinator);
      });
    print(BLUE + "killing " + dbServers.length + " DBServers/Coordinators " + RESET);
    dbServers.forEach((arangod) => {
      internal.debugTerminateInstance(arangod.endpoint);
      // need this to properly mark spawned process as killed in internal test data
      arangod.exitStatus = internal.killExternal(arangod.pid, termSignal); 
      arangod.pid = 0;
    });
  } else {
    params.instanceManager.shutdownInstance(false);
  }
}

function recovery (options) {
  if ((!global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] ||
       global.ARANGODB_CLIENT_VERSION(true)['failure-tests'] === 'false') ||
      (!global.ARANGODB_CLIENT_VERSION(true)['maintainer-mode'] ||
       global.ARANGODB_CLIENT_VERSION(true)['maintainer-mode'] === 'false')) {
    return {
      recovery: {
        status: false,
        message: 'failure-tests not enabled. please recompile with -DUSE_FAILURE_TESTS=On and -DUSE_MAINTAINER_MODE=On'
      },
      status: false
    };
  }
  
  if (!options.cluster) {
    return {
      recovery: {
        status: false,
        message: 'cluster_recovery suite need cluster option to be set to true!'
      },
      status: false
    };
  }
  let results = {
    status: true
  };
  let useEncryption = false;
  if (global.ARANGODB_CLIENT_VERSION) {
    let version = global.ARANGODB_CLIENT_VERSION(true);
    if (version.hasOwnProperty('enterprise-version')) {
      useEncryption = true;
    }
  }

  let recoveryTests = tu.scanTestPaths(testPaths.recovery_cluster, options);

  recoveryTests = tu.splitBuckets(options, recoveryTests);

  let count = 0;
  let tmpMgr = new tmpDirMmgr('recovery_cluster', options);

  for (let i = 0; i < recoveryTests.length; ++i) {
    let test = recoveryTests[i];
    let filtered = {};

    if (tu.filterTestcaseByOptions(test, options, filtered)) {
      count += 1;
      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running setup of test " + count + " - " + test + RESET);
      let params = {
        tempDir: tmpMgr.tempDir,
        rootDir: fs.join(fs.getTempPath(), 'recovery_cluster', count.toString()),
        options: _.cloneDeep(options),
        script: test,
        setup: true,
        count: count,
        keyDir: "",
        temp_path: fs.join(tmpMgr.tempDir, count.toString())
      };
      fs.makeDirectoryRecursive(params.rootDir);
      fs.makeDirectoryRecursive(params.temp_path);
      runArangodRecovery(params, useEncryption);

      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running recovery of test " + count + " - " + test + RESET);
      params.options.disableMonitor = options.disableMonitor;
      params.setup = false;
      try {
        tu.writeTestResult(params.temp_path, {
          failed: 1,
          status: false, 
          message: "unable to run recovery test " + test,
          duration: -1
        });
    } catch (er) { print(er);}
      try {
        runArangodRecovery(params, useEncryption);
      } catch (err) {
        results[test] = {
          failed: 1,
          status: false,
          message: "Crashed! \n" + err + "\nAborting execution of more tests",
          duration: -1
        };
        results.status = false;
        results.crashed = true;
        print("skipping more tests!");
        return results;
      }

      results[test] = tu.readTestResult(
        params.temp_path,
        {
          status: false
        },
        test
      );
      params.instanceManager.destructor(results[test].status);
      if (results[test].status) {
//        the instance manager destructor cleans this out:
//        fs.removeDirectoryRecursive(params.rootDir, true);
        if (params.keyDir !== "") {
          fs.removeDirectoryRecursive(params.keyDir);
        }
      } else {
        print("Not cleaning up " + params.rootDir);
        results.status = false;
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
    }
  }
  tmpMgr.destructor(options.cleanup && results.status);
  if (count === 0) {
    print(RED + 'No testcase matched the filter.' + RESET);
    return {
      ALLTESTS: {
        status: true,
        skipped: true
      },
      status: true
    };
  }

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['recovery_cluster'] = recovery;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
