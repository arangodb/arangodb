/* jshint strict: false, sub: true */
/* global print, params */
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

function startParameterTest(options, testpath, suiteName) {
  let count = 0;
  let results = { shutdown: true };
  let filtered = {};
  const tests = tu.scanTestPaths(testpath, options);

  tests.forEach(function (testFile, i) {
    count += 1;
    if (tu.filterTestcaseByOptions(testFile, options, filtered)) {
      // pass on JWT secret
      let instanceInfo;
      let shutdownStatus;
      let clonedOpts = _.clone(options);
      let paramsFirstRun = {};
      let serverOptions = {};

      let fileContent = fs.read(testFile);
      let content = `(function(){ const runSetup = false; const getOptions = true; ${fileContent} 
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF

      let paramsSecondRun = executeScript(content, true, testFile);
      let rootDir = fs.join(fs.getTempPath(), count.toString());
      let runSetup = paramsSecondRun.hasOwnProperty('runSetup');
      if (paramsSecondRun.hasOwnProperty('server.jwt-secret')) {
        clonedOpts['server.jwt-secret'] = paramsSecondRun['server.jwt-secret'];
      }
      if (runSetup) {
        delete paramsSecondRun.runSetup;
        if (options.extremeVerbosity) {
          print(paramsFirstRun);
        }
        instanceInfo = pu.startInstance(options.protocol, options, paramsFirstRun, suiteName, rootDir); // first start
        pu.cleanupDBDirectoriesAppend(instanceInfo.rootDir);      
        try {
          print(BLUE + '================================================================================' + RESET);
          print(CYAN + 'Running Setup of: ' + testFile + RESET);
          content = `(function(){ const runSetup = true; const getOptions = false; ${fileContent}
}())`; // DO NOT JOIN WITH THE LINE ABOVE -- because of content could contain '//' at the very EOF

          if (!executeScript(content, true, testFile)) {
            throw new Error("setup of test failed");
          }
        } catch (ex) {
          shutdownStatus = pu.shutdownInstance(instanceInfo, clonedOpts, false);                                     // stop
          results[testFile] = {
            status: false,
            messages: 'Warmup of system failed: ' + ex,
            shutdown: shutdownStatus
          };
          results['shutdown'] = results['shutdown'] && shutdownStatus;
          return;
        }
        if (pu.shutdownInstance(instanceInfo, clonedOpts, false)) {                                                     // stop
          instanceInfo.arangods.forEach(function(arangod) {
            arangod.pid = null;
          });
          if (options.extremeVerbosity) {
            print(paramsSecondRun);
          }
          pu.reStartInstance(clonedOpts, instanceInfo, paramsSecondRun);      // restart with restricted permissions
        }
        else {
          results[testFile] = {
            status: false,
            message: "failed to stop instance",
            shutdown: false
          };
        }
      } else {
        instanceInfo = pu.startInstance(options.protocol, options, paramsSecondRun, suiteName, rootDir); // one start
      }

      results[testFile] = tu.runInLocalArangosh(options, instanceInfo, testFile, {});
      shutdownStatus = pu.shutdownInstance(instanceInfo, clonedOpts, false);

      results['shutdown'] = results['shutdown'] && shutdownStatus;
      
      if (!results[testFile].status || !shutdownStatus) {
        print("Not cleaning up " + instanceInfo.rootDir);
        results.status = false;
      }
      else {
        pu.cleanupLastDirectory(options);
      }
    } else {
      if (options.extremeVerbosity) {
        print('Skipped ' + testFile + ' because of ' + filtered.filter);
      }
    }
  });
  return results;
}

function server_permissions(options) {
  return startParameterTest(options, testPaths.server_permissions, "server_permissions");
}

function server_parameters(options) {
  return startParameterTest(options, testPaths.server_parameters, "server_parameters");
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
  return tu.performTests(copyOptions, testCases, 'server_secrets', tu.runInLocalArangosh, additionalArguments);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['server_permissions'] = server_permissions;
  testFns['server_parameters'] = server_parameters;
  testFns['server_secrets'] = server_secrets;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
};
