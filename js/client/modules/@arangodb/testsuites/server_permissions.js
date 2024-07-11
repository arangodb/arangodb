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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const _ = require('lodash');
const time = require('internal').time;
const fs = require('fs');
const yaml = require('js-yaml');

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');
const im = require('@arangodb/testutils/instance-manager');

const toArgv = internal.toArgv;
const executeScript = internal.executeScript;
const executeExternalAndWait = internal.executeExternalAndWait;
const testHelper = require("@arangodb/test-helper");
const ArangoError = require('@arangodb').ArangoError;
const isEnterprise = testHelper.isEnterprise;

const platform = internal.platform;

const BLUE = internal.COLORS.COLOR_BLUE;
const CYAN = internal.COLORS.COLOR_CYAN;
const GREEN = internal.COLORS.COLOR_GREEN;
const RED = internal.COLORS.COLOR_RED;
const RESET = internal.COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const functionsDocumentation = {
  'server_permissions': 'permissions test for the server',
  'server_parameters': 'specifies startup parameters for the instance'
};

const testPaths = {
  'server_permissions': [tu.pathForTesting('client/server_permissions')],
  'server_parameters': [tu.pathForTesting('client/server_parameters')]
};

class permissionsRunner extends trs.runLocalInArangoshRunner {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "permissionsRunner";
  }
  run(testList) {
    let tmpDir = fs.getTempPath();
    let beforeStart = time();
    this.continueTesting = true;
    this.testList = testList;
    let testrunStart = time();
    this.results = {
      shutdown: true,
      status: true,
      startupTime: testrunStart - beforeStart
    };
    let serverDead = false;
    let count = 0;
    let forceTerminate = false;
    for (let i = 0; i < this.testList.length; i++) {
      testHelper.flushInstanceInfo();
      let te = this.testList[i];
      let filtered = {};
      if (tu.filterTestcaseByOptions(te, this.options, filtered)) {
        let loopCount = 0;
        count += 1;
        // pass on JWT secret
        let shutdownStatus;
        let clonedOpts = _.clone(this.options);
        let paramsFirstRun = {};

        let fileContent = fs.read(te);
        let content = `(function(){ const runSetup = false; const getOptions = true; ${fileContent} 
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF

        let paramsSecondRun = executeScript(content, true, te);
        let rootDir = fs.join(fs.getTempPath(), count.toString());
        let runSetup = paramsSecondRun.hasOwnProperty('runSetup');
        clonedOpts['startupMaxCount'] = 600; // Slow startups may occur on slower machines.
        if (paramsSecondRun.hasOwnProperty('server.jwt-secret')) {
          clonedOpts['server.jwt-secret'] = paramsSecondRun['server.jwt-secret'];
        }
        if (paramsSecondRun.hasOwnProperty('database.password')) {
          clonedOpts['server.password'] = paramsSecondRun['database.password'];
          clonedOpts['password'] = paramsSecondRun['database.password'];
          paramsFirstRun['server.password'] = paramsSecondRun['database.password'];
        }
        print('\n' + (new Date()).toISOString() + GREEN + " [============] " + this.info + ': Trying', te, '...', RESET);

        if (runSetup) {
          delete paramsSecondRun.runSetup;
          if (this.options.extremeVerbosity !== 'silent') {
            print(paramsFirstRun);
          }
          this.instanceManager = new im.instanceManager(clonedOpts.protocol,
                                                       clonedOpts,
                                                       paramsFirstRun,
                                                       this.friendlyName,
                                                       rootDir);
	        global.theInstanceManager = this.instanceManager;
          this.instanceManager.prepareInstance();
          this.instanceManager.launchTcpDump("");
          if (!this.instanceManager.launchInstance()) {
            this.instanceManager.destructor(false);
            this.options.cleanup = false;
            throw new Error("failed to launch instance");
          }
          this.instanceManager.reconnect();
          try {
            print(CYAN + 'Running Setup of: ' + te + RESET);
            content = `(function(){ const runSetup = true; const getOptions = false; ${fileContent}
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF

            if (!executeScript(content, true, te)) {
              this.options.cleanup = false;
              throw new Error("setup of test failed");
            }
          } catch (ex) {
            this.options.cleanup = false;
            if (ex instanceof ArangoError &&
                ex.errorNum === internal.errors.ERROR_DISABLED.code) {
              forceTerminate = true;
            }
            shutdownStatus = this.instanceManager.shutdownInstance(forceTerminate);                               // stop
            this.instanceManager.destructor(false);
            this.results[te] = {
              status: false,
              messages: 'Warmup of system failed: ' + ex,
              shutdown: shutdownStatus
            };
            this.results['shutdown'] = this.results['shutdown'] && shutdownStatus;
            this.results.status = false;
            return this.results;
          }
          if (this.instanceManager.shutdownInstance(forceTerminate)) {                                            // stop
            this.instanceManager.arangods.forEach(function(arangod) {
              arangod.pid = null;
            });
            if (this.options.extremeVerbosity !== 'silent') {
              print(paramsSecondRun);
            }
            try {
              // if failurepoints are active, disable SUT-sanity checks:
              if (paramsSecondRun.hasOwnProperty('server.failure-point')) {
                print("Failure points active, marking suspended");
                this.instanceManager.arangods.forEach(
                  arangod => { arangod.suspended = true; });
              }
              this.instanceManager.reStartInstance(paramsSecondRun);      // restart with restricted permissions
            } catch (ex) {
              this.options.cleanup = false;
              if (ex instanceof ArangoError &&
                  ex.errorNum === internal.errors.ERROR_DISABLED.code) {
                forceTerminate = true;
              }
              this.results[te] = {
                message: "Aborting testrun; failed to launch instance: " +
                  ex.message + " - " +
                  JSON.stringify(this.instanceManager.getStructure()),
                status: false,
                shutdown: false
              };
              this.results.status = false;
              this.instanceManager.shutdownInstance(true);
              return this.results;
            }
          }
          else {
            this.results.shutdown = false;
            this.results[te] = {
              status: false,
              message: "failed to stop instance",
              shutdown: false
            };
          }
        } else {
          this.instanceManager = new im.instanceManager(clonedOpts.protocol,
                                                       clonedOpts,
                                                       paramsSecondRun,
                                                       this.friendlyName,
                                                       rootDir);
          global.theInstanceManager = this.instanceManager;
          // if failurepoints are active, disable SUT-sanity checks:
          let failurePoints = paramsSecondRun.hasOwnProperty('server.failure-point');
          if (failurePoints) {
            print("Failure points active, marking suspended");
            this.instanceManager.arangods.forEach(
              arangod => { arangod.suspended = true; });
          }
          this.instanceManager.prepareInstance();
          this.instanceManager.launchTcpDump("");
          try {
            if (!this.instanceManager.launchInstance()) {
              this.results[te] = {
                message: "Aborting testrun; failed to launch instance: " + JSON.stringify(this.instanceManager.getStructure()),
                status: false,
                shutdown: false
              };
              this.results.status = false;
              this.instanceManager.shutdownInstance(true);
              this.instanceManager.destructor(false);
              return this.results;
            }
          } catch (ex) {
            this.options.cleanup = false;
            this.results[te] = {
              message: "Aborting testrun; failed to launch instance: " +
                ex.message + " - " +
                JSON.stringify(this.instanceManager.getStructure()),
              status: false,
              shutdown: false
            };
            this.results.shutdown = false;
            this.results.status = false;
            this.instanceManager.shutdownInstance(true);
            this.instanceManager.destructor(false);
            return this.results;
          }
          if (!failurePoints) {
            this.instanceManager.reconnect();
          }
        }

        this.results[te] = this.runOneTest(te);
        if (this.instanceManager.addArgs.hasOwnProperty("authOpts") &&
            this.instanceInfo.addArgs.hasOwnProperty("server.jwt-secret")) {
          // Reconnect to set the server credentials right
          arango.reconnect(arango.getEndpoint(), '_system', "root", "", true,
                           this.instanceManager.addArgs["server.jwt-secret"]);
        }
        this.results.status = this.results.status && this.results[te].status;
        shutdownStatus = this.instanceManager.shutdownInstance(false);
        this.results['shutdown'] = this.results['shutdown'] && shutdownStatus;
        this.instanceManager.destructor(this.results[te].status && shutdownStatus);
      } else {
        if (this.options.extremeVerbosity) {
          print('Skipped ' + te + ' because of ' + filtered.filter);
        }
      }
    }
    if (this.results.status && this.options.cleanup) {
      fs.list(tmpDir).forEach(file => {
        let fullfile = fs.join(tmpDir, file);
        if (fs.isDirectory(fullfile)) {
          fs.removeDirectoryRecursive(fullfile, true);
        } else {
          fs.remove(fullfile);
        }
      });
    }
    return this.results;
  }
}

function server_permissions(options) {
  let testCases = tu.scanTestPaths(testPaths.server_permissions, options);
  testCases = tu.splitBuckets(options, testCases);
  return new permissionsRunner(options, "server_permissions").run(testCases);
}

function server_parameters(options) {
  let testCases = tu.scanTestPaths(testPaths.server_parameters, options);
  testCases = tu.splitBuckets(options, testCases);
  require('internal').env.OPTIONS=JSON.stringify(options);
  return new permissionsRunner(options, "server_parameters").run(testCases);
}

function server_secrets(options) {

  let secretsDir = fs.join(fs.getTempPath(), 'arango_jwt_secrets');
  fs.makeDirectory(secretsDir);
  let secretFiles = [
    fs.join(secretsDir, 'secret1'),
    fs.join(secretsDir, 'secret2')
  ];
  fs.write(secretFiles[0], 'jwtsecret-1');
  fs.write(secretFiles[1], 'jwtsecret-2');

  process.env["jwt-secret-folder"] = secretsDir;

  // Now set up TLS with the first server key:
  let keyfileDir = fs.join(fs.getTempPath(), 'arango_tls_keyfile');
  let keyfileName = fs.join(keyfileDir, "server.pem");
  fs.makeDirectory(keyfileDir);

  fs.copyFile(fs.join("etc", "testing", "server.pem"), keyfileName);

  process.env["tls-keyfile"] = keyfileName;

  let copyOptions = _.clone(options);
  // necessary to fix shitty process-utils handling
  copyOptions['server.jwt-secret-folder'] = secretsDir;
  copyOptions['jwtFiles'] = secretFiles;
  copyOptions['protocol'] = "ssl";

  const testCases = tu.scanTestPaths([tu.pathForTesting('client/server_secrets')], copyOptions);
  let additionalArguments = {
    'server.authentication': 'true',
    'server.jwt-secret-folder': secretsDir,
    'cluster.create-waits-for-sync-replication': false,
    'ssl.keyfile': keyfileName
  };
  if (isEnterprise()) {
    additionalArguments['ssl.server-name-indication']
      = "hans.arangodb.com=./etc/testing/tls.keyfile";
  }
  let rc = new trs.runLocalInArangoshRunner(copyOptions, 'server_secrets', additionalArguments).run(testCases);
  if (rc.status && options.cleanup) {
    fs.removeDirectoryRecursive(keyfileDir, true);
    fs.removeDirectoryRecursive(secretsDir, true);
  }
  return rc;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['server_permissions'] = server_permissions;
  testFns['server_parameters'] = server_parameters;
  testFns['server_secrets'] = server_secrets;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
