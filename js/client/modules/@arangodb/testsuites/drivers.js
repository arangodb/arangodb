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
  'driver': 'generic driver test - requires driver adaptor script',
};
const optionsDocumentation = [
  '   - `driversource`: directory of the respective driver',
  '   - `driverScript`: script to invoke inside the driver',
  '   - `driverScriptInterpreter`: interpreter to invoke script whith',
  '   - `driverOptions`: additional arguments to pass via the commandline'
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
const isEnterprise = require("@arangodb/test-helper").isEnterprise;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'driver': []
};
const host_re = new RegExp('([a-z]*)://([0-9.:]*):(\d*)');
// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function driver (options) {
  class runInDriverTest extends testRunnerBase {
    constructor(options, testname, ...optionalArgs) {
      super(options, testname, ...optionalArgs);
      this.info = "runInDriverTest";
    }
    runOneTest(file) {
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
      let m = this.instanceManager.url.split(host_re);
      
      let args = [
        this.options.driverScript,
        '--instanceUrl', this.instanceManager.url,
        '--instanceEndpoint', this.instanceManager.endpoint,
        '--port', m[4],
        '--host', m[2],
        '--auth', false,
        '--username', 'root',
        '--password', '',
        (isEnterprise())? '--enterprise' : '--no-enterprise',
        '--deployment-mode', topology
        
      ];
      if (testFilter) {
        args.push('--testsuite');
        args.push(testFilter);
      }
      if (this.options.testCase) {
        args.push('--filter');
        args.push(this.options.testCase);
      }
      if (this.options.hasOwnProperty('driverOptions')) {
        for (var key in this.options.driverOptions) {
          args.push('--' + key + '=' + this.options.driverOptions[key]);
        }
      }
      if (this.options.extremeVerbosity) {
        print(process.env);
        print(args);
      }
      let start = Date();
      let status = true;
      const res = executeExternal(this.options.driverScriptInterpreter, args, true, [], this.options.driversource);

      let allBuff = '';
      let count = 0;
      let rc;
      do {
        let buf = fs.readPipe(res.pid);
        print(buf);
        allBuff += buf;
        while ((buf.length === 1023) || count === 0) {
          count += 1;
          buf = fs.readPipe(res.pid);
          print(buf);
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
      results['timeout'] = false;
      results['status'] = status;
      results['message'] = allBuff;
      return results;
    }
  }
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }
  let testFilter = options.test; // no, testRunnerBase thy shal not evaluate this.
  localOptions.test = '';

  return new runInDriverTest(localOptions, 'driver_test').run(['driver_test.js']);
}


exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  opts.driverScript = 'run_tests.sh';
  opts.driverScriptInterpreter = '/bin/bash';
  testFns['driver'] = driver;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
  tu.CopyIntoList(optionsDoc, optionsDocumentation);
};
