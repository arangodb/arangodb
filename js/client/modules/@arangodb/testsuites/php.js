/* jshint strict: false, sub: true */
/* global print db */
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

const functionsDocumentation = {
  'php_driver': 'php client driver test',
};
const optionsDocumentation = [
  '   - `phpsource`: directory of the php driver',
  '   - `phpkeepalive`: whether to use connection keepalive',
  '   - `phpOptions`: additional arguments to pass via the commandline'
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
  'php_driver': []
};
const host_re = new RegExp('([a-z]*)://([0-9.:]*):(\d*)');
// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function phpDriver (options) {
  class runInPhpTest extends testRunnerBase {
    constructor(options, testname, ...optionalArgs) {
      super(options, testname, ...optionalArgs);
      this.info = "runInPhpTest";
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
      } else if (this.options.activefailover) {
        topology = 'ACTIVE_FAILOVER';
        matchTopology = /^ACTIVE_FAILOVER/;
      } else {
        topology = 'SINGLE_SERVER';
        matchTopology = /^SINGLE_SERVER/;
      }
      let enterprise = 'false';
      if (global.ARANGODB_CLIENT_VERSION(true).hasOwnProperty('enterprise-version')) {
        enterprise = 'true';
      }
      let m = this.instanceManager.url.split(host_re);
      process.env['ARANGO_ROOT_PASSWORD'] = '';
      process.env['ARANGO_USE_AUTHENTICATION'] = false;
      process.env['ARANGO_HOST'] = m[2];
      process.env['ARANGO_PORT'] = m[4];
      let args = [
        '--configuration'
      ];
      if (this.options.phpkeepalive) {
        args.push('./tests/phpunit-connection-keep-alive.xml');
      } else {
        args.push('./tests/phpunit-connection-close.xml');
      }
      if (testFilter) {
        args.push('--testsuite');
        args.push(testFilter);
      }
      if (this.options.testCase) {
        args.push('--filter');
        args.push(this.options.testCase);
      }
      if (this.options.hasOwnProperty('phpOptions')) {
        for (var key in this.options.phpOptions) {
          args.push('--' + key + '=' + this.options.phpOptions[key]);
        }
      }
      if (this.options.extremeVerbosity) {
        print(process.env);
        print(args);
      }
      let start = Date();
      let status = true;
      const res = executeExternal('phpunit', args, true, [], this.options.phpsource);

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
  let testFilter = options.test; // no, testRunnerBase, thy shal not evaluate this.
  localOptions.test = '';

  let rc = new runInPhpTest(localOptions, 'php_test').run([ 'php_test.js']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}


exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  opts['phpkeepalive'] = true;
  Object.assign(allTestPaths, testPaths);
  testFns['php_driver'] = phpDriver;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
