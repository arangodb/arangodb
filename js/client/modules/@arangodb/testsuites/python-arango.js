/* jshint strict: false, sub: true */
/* global print db */
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

const functionsDocumentation = {
  'python_driver': 'python_arango client driver test',
};
const optionsDocumentation = [
  '   - `pythonsource`: directory of the java driver',
  '   - `pythonOptions`: additional arguments to pass via the commandline',
  '                    can be found in arangodb-java-driver/src/test/java/utils/TestUtils.java',
];

const internal = require('internal');

const executeExternal = internal.executeExternal;
const executeExternalAndWait = internal.executeExternalAndWait;
const statusExternal = internal.statusExternal;

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const { runWithAllureReport } = require('@arangodb/testutils/testrunners');
const yaml = require('js-yaml');
const platform = require('internal').platform;
const time = require('internal').time;
const isEnterprise = require("@arangodb/test-helper").isEnterprise;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'python_driver': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

class runInPythonTest extends runWithAllureReport {
  constructor(options, testname, ...optionalArgs) {
    let opts = _.clone(tu.testClientJwtAuthInfo);
    opts['password'] = 'pythonarango';
    opts['username'] = 'root';
    opts["encryptionAtRest"] = true;
    opts.extraArgs = {
      // needed by Python jwt tests, otherwise the returned token is "invalid", see RestAuthHandler.cpp:58
      'server.authentication': 'true',
      // need by Python tests to query the options API
      'server.options-api': 'admin',
    };
    //opts['arangodConfig'] = 'arangod-auth.conf';
    _.defaults(opts, options);
    super(opts, testname, ...optionalArgs);
    this.info = "runInPythonTest";
  }
  checkSutCleannessBefore() {}
  checkSutCleannessAfter() { return true; }
  runOneTest(file) {
    print(this.instanceManager.setPassvoid('pythonarango'));
    let topology;
    let testResultsDir = fs.join(this.instanceManager.rootDir, 'pythonresults');
    let results = {
      'message': ''
    };
    
    let args = [
      '--root', 'root',
      '--password', 'pythonarango',
      '--secret', fs.read(this.instanceManager.restKeyFile),
      '--junitxml', 'test-results/junit.xml',
      '--log-cli-level', 'DEBUG',
      '--skip', 'backup', 'foxx', 'js-transactions',
      '--host', '127.0.0.1',
      '--port', `${this.instanceManager.endpointPort}`,
      '--alluredir', testResultsDir,
    ];
    if (this.options.cluster) {
      args = args.concat([
        '--cluster',
        '--port', `${this.instanceManager.endpointPorts[1]}`,
        '--port', `${this.instanceManager.endpointPorts[2]}`,
      ]);
    }
    if (this.options.testCase) {
      args = args.concat(['-k', this.options.testCase]);
    }
    if (this.options.pythonOptions !== '') {
      for (var key in this.options.pythonOptions) {
        args.push('--' + key + '=' + this.options.pythonOptions[key]);
      }
    }
    if (this.options.extremeVerbosity) {
      print(args);
    }
    let start = Date();
    let status = true;
    const cwd = fs.normalize(fs.makeAbsolute(this.options.pythonsource));
    const rc = executeExternalAndWait('pytest', args, false, 0, [], cwd);
    if (rc.exit !== 0) {
      status = false;
    }
    results = {
      status: status,
      failed: (status)?0:1,
    };
    print(results);
    this.getAllureResults(testResultsDir, results, status);
    return results;
  }
}

function pythonDriver (options) {
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
    localOptions.coordinators = 3;
  }
  let rc = new runInPythonTest(localOptions, 'python_test').run([ 'python_test.js']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['python_driver'] = pythonDriver;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
  tu.CopyIntoList(optionsDoc, optionsDocumentation);
  tu.CopyIntoObject(opts, {
    'pythonOptions': '',
    'pythonsource': '../python-arango-async',
  });
};
