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

function checkServersGOOD(instanceInfo) {
  try {
    let health = internal.clusterHealth();
    let rc = instanceInfo.arangods.reduce((previous, arangod) => {
        if (arangod.role === "agent") return true;
        if (health.hasOwnProperty(arangod.id)) {
          if (health[arangod.id].Status === "GOOD") {
            return previous;
          } else {
            print(RED + "ClusterHealthCheck failed " + arangod.id + " has status "
                  + health[arangod.id].Status + " (which is not equal to GOOD)");
            return false;
          }
        } else {
          print(RED + "ClusterHealthCheck failed " + arangod.id
                + " does not have health property");
          return false;
        }
      }, true);
    return rc;
  } catch(e) {
    print("Error checking cluster health " + e);
    return false;
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: recovery_cluster
// //////////////////////////////////////////////////////////////////////////////

function runArangodRecovery (params) {
  let useEncryption = false;
  if (global.ARANGODB_CLIENT_VERSION) {
    let version = global.ARANGODB_CLIENT_VERSION(true);
    if (version.hasOwnProperty('enterprise-version')) {
      useEncryption = true;
    }
  }

  let additionalParams= {
    'foxx.queues': 'false',
    'server.statistics': 'false',
    'log.foreground-tty': 'true',
    'database.ignore-datafile-errors': 'false', // intentionally false!
  };
  
  // for cluster runs we have separate parameter set for servers and for testagent(arangosh)
  let additionalTestParams  = {};

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
  // arangosh has different name for parameter :(
  additionalTestParams['javascript.execute'] =  params.script;
  additionalTestParams['javascript.run-main'] = true;

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
    args['temp.path'] = params.instanceInfo.rootDir;
    
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
      let keyDir = fs.join(fs.getTempPath(), 'arango_encryption');
      if (!fs.exists(keyDir)) {  // needed on win32
        fs.makeDirectory(keyDir);
      }
      pu.cleanupDBDirectoriesAppend(keyDir);
        
      const key = '01234567890123456789012345678901';
      
      let keyfile = fs.join(keyDir, 'rocksdb-encryption-keyfile');
      fs.write(keyfile, key);

      // special handling for encryption-keyfolder tests
      if (params.script.match(/encryption-keyfolder/)) {
        args['rocksdb.encryption-keyfolder'] = keyDir;
        process.env["rocksdb-encryption-keyfolder"] = keyDir;
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
    params.instanceInfo = pu.startInstance(params.options.protocol,
                                           params.options,
                                           params.args,
                                           fs.join('recovery_cluster', params.count.toString()));
  } else {
    print(BLUE + "Restarting cluster " + RESET);
    pu.reStartInstance(params.options, params.instanceInfo, {});
    let tryCount = 10;
    while(tryCount > 0 && !checkServersGOOD(params.instanceInfo)) {
      print(RESET + "Waiting for all servers to go GOOD");
      internal.sleep(3); // give agency time to bootstrap DBServers
      --tryCount;
    }
  }
  let agentArgs = pu.makeArgs.arangosh(params.options);
  agentArgs['server.endpoint'] = tu.findEndpoint(params.options, params.instanceInfo);
  if (params.args['log.level']) {
    agentArgs['log.level'] = params.args['log.level'];
  }
  agentArgs['temp.path'] = params.instanceInfo.rootDir;
  Object.assign(agentArgs, additionalTestParams);
  require('internal').env.INSTANCEINFO = JSON.stringify(params.instanceInfo);
  try {
    pu.executeAndWait(pu.ARANGOSH_BIN, toArgv(agentArgs), params.options, 'arangosh', params.instanceInfo.rootDir, false);
  } catch(err) {
    print('Error while launching test:' + err);
    pu.shutdownInstance(params.instanceInfo, params.options, false);
    return; // without test server test will for sure fail
  }
  if (params.setup) {
    let dbServers = params.instanceInfo.arangods.slice().filter((a) => {return a.role === 'dbserver' || a.role === 'coordinator';});
    print(BLUE + "killing " + dbServers.length + " DBServers/Coordinators " + RESET);
    dbServers.forEach((arangod) => {
      internal.debugTerminateInstance(arangod.endpoint);
      // need this to properly mark spawned process as killed in internal test data
      arangod.exitStatus = internal.killExternal(arangod.pid, termSignal); 
      arangod.pid = 0;
    });
  } else {
    pu.shutdownInstance(params.instanceInfo, params.options, false);
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

  let recoveryTests = tu.scanTestPaths(testPaths.recovery_cluster, options);

  recoveryTests = tu.splitBuckets(options, recoveryTests);

  let count = 0;
  let orgTmp = process.env.TMPDIR;
  let tempDir = fs.join(fs.getTempPath(), 'recovery_cluster');
  fs.makeDirectoryRecursive(tempDir);
  process.env.TMPDIR = tempDir;
  pu.cleanupDBDirectoriesAppend(tempDir);

  for (let i = 0; i < recoveryTests.length; ++i) {
    let test = recoveryTests[i];
    let filtered = {};

    if (tu.filterTestcaseByOptions(test, options, filtered)) {
      count += 1;
      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running setup of test " + count + " - " + test + RESET);
      let params = {
        tempDir: tempDir,
        instanceInfo: {
          rootDir: fs.join(fs.getTempPath(), 'recovery_cluster', count.toString())
        },
        options: _.cloneDeep(options),
        script: test,
        setup: true,
        count: count,
        testDir: ""
      };
      runArangodRecovery(params);

      ////////////////////////////////////////////////////////////////////////
      print(BLUE + "running recovery of test " + count + " - " + test + RESET);
      params.options.disableMonitor = options.disableMonitor;
      params.setup = false;
      try {
        tu.writeTestResult(params.args['temp.path'], {
          failed: 1,
          status: false, 
          message: "unable to run recovery test " + test,
          duration: -1
        });
      } catch (er) {}
      runArangodRecovery(params);

      results[test] = tu.readTestResult(
        params.args['temp.path'],
        {
          status: false
        },
        test
      );
      if (!results[test].status) {
        print("Not cleaning up " + params.testDir);
        results.status = false;
      }
      else {
        pu.cleanupLastDirectory(params.options);
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + test + ' because of ' + filtered.filter);
      }
    }
  }
  process.env.TMPDIR = orgTmp;
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
