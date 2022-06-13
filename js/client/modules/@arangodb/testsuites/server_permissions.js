/* jshint strict: false, sub: true */
/* global print, arango */
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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const time = require('internal').time;
const fs = require('fs');
const yaml = require('js-yaml');

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const im = require('@arangodb/testutils/instance-manager');

const toArgv = require('internal').toArgv;
const executeScript = require('internal').executeScript;
const executeExternalAndWait = require('internal').executeExternalAndWait;

const platform = require('internal').platform;

const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const functionsDocumentation = {
  'server_permissions': 'permissions test for the server',
  'server_parameters': 'specifies startup parameters for the instance'
};

const testPaths = {
  'server_permissions': [tu.pathForTesting('client/server_permissions')],
  'server_parameters': [tu.pathForTesting('client/server_parameters')]
};

class permissionsRunner extends tu.runLocalInArangoshRunner {
  constructor(options, testname, ...optionalArgs) {
    super(options, testname, ...optionalArgs);
    this.info = "runInDriverTest";
  }
  run(testList) {
    let beforeStart = time();
    this.continueTesting = true;
    this.testList = testList;
    let testrunStart = time();
    this.results = {
      shutdown: true,
      startupTime: testrunStart - beforeStart
    };
    let serverDead = false;
    let count = 0;
    let forceTerminate = false;
    for (let i = 0; i < this.testList.length; i++) {
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
        if (paramsSecondRun.hasOwnProperty('server.jwt-secret')) {
          clonedOpts['server.jwt-secret'] = paramsSecondRun['server.jwt-secret'];
        }
        if (paramsSecondRun.hasOwnProperty('database.password')) {
          clonedOpts['server.password'] = paramsSecondRun['database.password'];
          clonedOpts['password'] = paramsSecondRun['database.password'];
          paramsFirstRun['server.password'] = paramsSecondRun['database.password'];
        }

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
          
          this.instanceManager.prepareInstance();
          this.instanceManager.launchTcpDump("");
          if (!this.instanceManager.launchInstance()) {
            this.instanceManager.destructor();
            throw new Error("failed to launch instance");
          }
          this.instanceManager.reconnect();
          try {
            print(BLUE + '================================================================================' + RESET);
            print(CYAN + 'Running Setup of: ' + te + RESET);
            content = `(function(){ const runSetup = true; const getOptions = false; ${fileContent}
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF

            if (!executeScript(content, true, te)) {
              throw new Error("setup of test failed");
            }
          } catch (ex) {
            shutdownStatus = this.instanceManager.shutdownInstance(false);                                     // stop
            this.instanceManager.destructor();
            this.results[te] = {
              status: false,
              messages: 'Warmup of system failed: ' + ex,
              shutdown: shutdownStatus
            };
            this.results['shutdown'] = this.results['shutdown'] && shutdownStatus;
            return;
          }
          if (this.instanceManager.shutdownInstance(false)) {                                                     // stop
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
              this.instanceManager.destructor();
              return this.results;
            }
          } catch (ex) {
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
            this.instanceManager.destructor();
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
        shutdownStatus = this.instanceManager.shutdownInstance(false);
        this.results['shutdown'] = this.results['shutdown'] && shutdownStatus;
        this.instanceManager.destructor();
        if (!this.results[te].status || !shutdownStatus) {
          print("Not cleaning up " + this.instanceManager.rootDir);
          this.results.status = false;
          this.results[te].status = false;
          return this.results;
        }
        else {
          pu.cleanupLastDirectory(clonedOpts);
        }
      } else {
        if (this.options.extremeVerbosity !== 'silent') {
          print('Skipped ' + te + ' because of ' + filtered.filter);
        }
      }
    }
    return this.results;
  }
}

function server_permissions(options) {
  return new permissionsRunner(options, "server_permissions").run(
    tu.scanTestPaths(testPaths.server_permissions, options));
}

function server_parameters(options) {
  return new permissionsRunner(options, "server_parameters").run(
    tu.scanTestPaths(testPaths.server_parameters, options));
}

function server_secrets(options) {

  let secretsDir = fs.join(fs.getTempPath(), 'arango_jwt_secrets');
  fs.makeDirectory(secretsDir);
  pu.cleanupDBDirectoriesAppend(secretsDir);

  fs.write(fs.join(secretsDir, 'secret1'), 'jwtsecret-1');
  fs.write(fs.join(secretsDir, 'secret2'), 'jwtsecret-2');

  process.env["jwt-secret-folder"] = secretsDir;

  // Now set up TLS with the first server key:
  let keyfileDir = fs.join(fs.getTempPath(), 'arango_tls_keyfile');
  let keyfileName = fs.join(keyfileDir, "server.pem");
  fs.makeDirectory(keyfileDir);
  pu.cleanupDBDirectoriesAppend(keyfileDir);

  fs.copyFile("./UnitTests/server.pem", keyfileName);

  process.env["tls-keyfile"] = keyfileName;

  let copyOptions = _.clone(options);
  // necessary to fix shitty process-utils handling
  copyOptions['server.jwt-secret-folder'] = secretsDir;
  copyOptions['protocol'] = "ssl";

  const testCases = tu.scanTestPaths([tu.pathForTesting('client/server_secrets')], copyOptions);
  let additionalArguments = {
    'server.authentication': 'true',
    'server.jwt-secret-folder': secretsDir,
    'cluster.create-waits-for-sync-replication': false,
    'ssl.keyfile': keyfileName
  };
  let version = global.ARANGODB_CLIENT_VERSION(true);
  if (version.hasOwnProperty('enterprise-version')) {
    additionalArguments['ssl.server-name-indication']
      = "hans.arangodb.com=./UnitTests/tls.keyfile";
  }
  return new tu.runLocalInArangoshRunner(copyOptions, 'server_secrets', additionalArguments).run(testCases);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['server_permissions'] = server_permissions;
  testFns['server_parameters'] = server_parameters;
  testFns['server_secrets'] = server_secrets;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
};
