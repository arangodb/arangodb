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
  'js_driver': 'javascript client driver test',
};
const optionsDocumentation = [
  '   - `jssource`: directory of the java driver',
  '   - `jsOptions`: additional arguments to pass via the commandline'
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
const testRunnerBase = require('@arangodb/testutils/testrunner').testRunner;
const yaml = require('js-yaml');
const platform = require('internal').platform;
const time = require('internal').time;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'js_driver': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function jsDriver (options) {
  class runInJsTest extends testRunnerBase {
    constructor(options, testname, ...optionalArgs) {
      super(options, testname, ...optionalArgs);
      this.info = "runInJsTest";
    }
    run(file) {
      let topology;
      let results = {
        'message': ''
      };
      let matchTopology;
      if (this.options.cluster) {
        topology = 'CLUSTER';
        matchTopology = /^CLUSTER/;
      } else {
        topology = 'SINGLE_SERVER';
        matchTopology = /^SINGLE_SERVER/;
      }
      process.env['ARANGO_VERSION']='30700'; // todo db._version(),
      process.env['TEST_ARANGODB_URL'] = this.instanceManager.endpoints.join(',');
      process.env['TEST_ARANGODB_URL_SELF_REACHABLE'] = this.instanceManager.url;
      
      // testResultsDir
      let args = [
        '-s', // Silent, only json
        'arango-test'
      ];
      if (this.options.testCase) {
        args.push('--grep');
        args.push(this.options.testCase);
      }
      if (this.options.hasOwnProperty('jsOptions')) {
        for (var key in this.options.jsOptions) {
          args.push('--' + key + '=' + this.options.jsOptions[key]);
        }
      }
      if (this.options.extremeVerbosity) {
        print(args);
      }
      let start = Date();
      let status = true;
      const res = executeExternal('yarn', args, true, [], this.options.jssource);

      let allBuff = '';
      let count = 0;
      let rc;
      do {
        let buf = fs.readPipe(res.pid);
        allBuff += buf;
        while ((buf.length === 1023) || count === 0) {
          count += 1;
          buf = fs.readPipe(res.pid);
          allBuff += buf;
        }
        rc = statusExternal(res.pid);
        if (rc.status === 'NOT-FOUND') {
          break;
        }
      } while (rc.status === 'RUNNING');
      if (rc.exit !== 0) {
        status = false;
      }
      let testResults = JSON.parse(allBuff);
      let totalSuccess = true;
      testResults.tests.forEach(test => {
        let isSucces = _.isEmpty(test.err);
        let message = test.fullTitle + '\n' + test.file + '\n';
        print((isSucces ? GREEN + '[     PASSED ] ':
               RED + '[   FAILED   ] ') + test.title + RESET);
        if (!isSucces) {
          print(test);
          totalSuccess = false;
          message += test.err.message + '\n' + test.err.stack;
        }
        
        results[test.title] = {
          "setUpDuration": 0,
          "tearDownDuration": 0,
          "status": isSucces,
          "duration": test.duration,
          "message": message
        };
      });
      results['timeout'] = false;
      results['status'] = totalSuccess;
      results['message'] = totalSuccess?'':'did you remember running yarn in the source?';
      // this.instanceManager.dumpAgency();
      return results;
    }
  }
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }

  let rc = new runInJsTest(localOptions, 'js_test').run([ 'js_test.js']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}


exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['js_driver'] = jsDriver;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
  tu.CopyIntoList(optionsDoc, optionsDocumentation);
};
