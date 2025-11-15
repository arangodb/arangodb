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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'go_driver': 'go client driver test',
};
const optionsDocumentation = [
  '   - `gosource`: directory of the go driver',
  '   - `goDriverVersion`: version [1,2] driver tests',
  '   - `goOptions`: additional arguments to pass via the `TEST_OPTIONS` environment, i.e. ` -timeout 180m` (prepend blank!)'
];

const internal = require('internal');

const executeExternal = internal.executeExternal;
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
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
// const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'go_driver': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function goDriver (options) {
  class runGoTest extends testRunnerBase {
    constructor(options, testname, ...optionalArgs) {
      let opts = {};
      if (options.cluster) {
        // tests lean on JWT enabled components
        opts = _.clone(tu.testClientJwtAuthInfo);
      }
      _.defaults(opts, options);
      if (options.cluster) {
        // go tests lean on 1 being the default replication factor
        opts.extraArgs['cluster.default-replication-factor'] = 1;
        if (opts.goDriverVersion >= 2) {
          opts.coordinators = 2;
        }
      } else {
        opts.extraArgs['server.authentication'] = true;
      }
      opts['arangodConfig'] = 'arangod-auth.conf';
      super(opts, testname, ...optionalArgs);
      this.info = "runInGoTest";
    }
    checkSutCleannessBefore() {}
    checkSutCleannessAfter() { return true; }
    runOneTest(file) {
      const goVersionArgs = [
        {
          "path": "./test/",
          "wd": ""
        }, {
          "path": "./tests",
          "wd": "/v2/",
        }][options.goDriverVersion -1];

      process.env['TEST_ENDPOINTS'] = this.instanceManager.urls.join(',');
      process.env['TEST_AUTHENTICATION'] = 'basic:root:';
      let jwt = this.instanceManager.JWT; 
      if (jwt) {
        process.env['TEST_JWTSECRET'] = jwt;
      }
      process.env['TEST_CONNECTION'] = '';
      process.env['TEST_CVERSION'] = '';
      process.env['TEST_CONTENT_TYPE'] = 'json';
      process.env['TEST_PPROF'] = '';
      if (this.options.cluster) {
        process.env['TEST_MODE'] = 'cluster';
      } else {
        process.env['TEST_MODE'] = 'single';
      }
      process.env['TEST_BACKUP_REMOTE_REPO'] = '';
      process.env['TEST_BACKUP_REMOTE_CONFIG'] = '';
      process.env['GODEBUG'] = 'tls13=1';
      process.env['CGO_ENABLED'] = '0';
      if (this.instanceManager.JWT) {
        process.env['TEST_JWTSECRET'] = this.instanceManager.JWT;
      }
      let args = ['test', '-json', '-tags', 'auth', goVersionArgs['path']];
      if (options.goDriverVersion === 2 && (options.cluster || options.isInstrumented)) {
        args.push('-parallel');
        args.push('1');
      }
      if (this.options.testCase) {
        args.push('-run');
        args.push(this.options.testCase);
      }
      if (this.options.goOptions !== '') {
        for (var key in this.options.goOptions) {
          args.push('-'+key);
          args.push(this.options.goOptions[key]);
        }
      }
      args.push('-timeout');
      args.push(`${options.oneTestTimeout/60}m`);
      if (this.options.extremeVerbosity) {
        print(process.env);
        print(args);
      }
      let start = Date();
      const res = executeExternal('go', args, true, [], `${this.options.gosource}${goVersionArgs['wd']}`);
      // let alljsonLines = []
      let b = '';
      let results = {};
      let status = true;
      let rc = {};
      let count = 0;
      do {
        let buf = fs.readPipe(res.pid);
        b += buf;
        while ((buf.length === 1023) || count === 0) {
          count += 1;
          let lineStart = 0;
          let maxBuffer = b.length;
          for (let j = 0; j < maxBuffer; j++) {
            if (b[j] === '\n') { // \n
              // OK, we've got a complete line. lets parse it.
              let oldLineStart = lineStart;
              const line = b.slice(lineStart, j);
              lineStart = j + 1;
              try {
                let item = JSON.parse(line);
                if (this.options.extremeVerbosity) {
                  print(item);
                }
                // alljsonLines.push(item)
                // print(item)
                let testcase = 'WARN';
                if (item.hasOwnProperty('Test')) {
                  testcase = item.Test;
                } else if (item.hasOwnProperty('Output')) {
                  if (item.Output === 'PASS\n') {
                    // this is the final PASS, ignore it.
                    print(item.Output);
                    continue;
                  } else if (item.Output.substring(0, 3) === 'ok '){
                    print(item.Output);
                    continue;
                  } else {
                    status = false;
                  }
                }
                if (!results.hasOwnProperty(testcase)) {
                  results[testcase] = {
                    "setUpDuration": 0,
                    "tearDownDuration": 0,
                    "status": testcase !== 'WARN',
                    "duration": 0,
                    "message": ''
                  };
                }
                let thiscase = results[testcase];
                switch(item.Action) {
                case 'start':
                  break;
                case 'fail':
                  status = false;
                  thiscase.status = false;
                  thiscase.duration = item.Elapsed;
                  break;
                case 'pause':
                  thiscase.message += `${item.Time} => PAUSE\n`;
                  break;
                case 'cont':
                  thiscase.message += `${item.Time} => Continue!\n`;
                  break;
                case 'output':
                  thiscase.message += item.Output;
                  print(item.Time + " - " + item.Output.replace(/^\s+|\s+$/g, ''));
                  break;
                case 'pass':
                  thiscase.status = true;
                  thiscase.duration = item.Elapsed * 1000; // s -> ms
                  break;
                case 'build-output':
                case 'build-fail':
                  print(`ERROR: ${item.Output}`);
                  break;
                case 'run':
                  // nothing interesting to see here...
                  break;
                case 'skip':
                  thiscase.status = true;
                  break;
                default:
                  status = false;
                  print("Don't know what to do with this line! " + line);
                  break;
                }
              } catch (x) {
                print(x);
                print(x.stack);
                status = false;
                print("Error while parsing line? - " + x);
                print("offending Line: " + line);
                // doesn't seem to be a good idea: lineStart = oldLineStart;
              }
            }
          }
          b = b.slice(lineStart, b.length);
          buf = fs.readPipe(res.pid);
          b += buf;
        }
        rc = statusExternal(res.pid);
        if (rc.status === 'NOT-FOUND') {
          break;
        }
      } while (rc.status === 'RUNNING');
      if (rc.exit !== 0) {
        status = false;
      }
      // fs.write('/tmp/bla.json', JSON.stringify(alljsonLines))
      results['timeout'] = false;
      results['status'] = status;
      results['message'] = '';
      return results;
    }
  }
  let localOptions = Object.assign({}, options, tu.testServerAuthInfo);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }

  let rc = new runGoTest(localOptions, 'go_test').run([ 'go_test.js']);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}


exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['go_driver'] = goDriver;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
  tu.CopyIntoObject(opts, {
    'goDriverVersion': 2,
    'goOptions': '',
    'gosource': '../go-driver',
  });
  tu.CopyIntoList(optionsDoc, optionsDocumentation);
};
