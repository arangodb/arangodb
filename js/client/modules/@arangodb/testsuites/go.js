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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'go_driver': 'go client driver test',
};
const optionsDocumentation = [
  '   - `gosource`: directory of the go driver',
  '   - `goOptions`: additional argumnets to pass via the `TEST_OPTIONS` environment, i.e. ` -timeout 180m` (prepend blank!)'
];

const internal = require('internal');

const executeExternal = internal.executeExternal;
const statusExternal = internal.statusExternal;

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
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
  function runInGoTest (options, instanceInfo, file, addArgs) {
    process.env['TEST_ENDPOINTS'] = instanceInfo.url;
    process.env['TEST_AUTHENTICATION'] = 'basic:root:';
    let jwt = pu.getJwtSecret(options);
    if (jwt) {
      process.env['TEST_JWTSECRET'] = jwt;
    }
    process.env['TEST_CONNECTION'] = '';
    process.env['TEST_CVERSION'] = '';
    process.env['TEST_CONTENT_TYPE'] = 'json';
    process.env['TEST_PPROF'] = '';
    if (options.cluster) {
      process.env['TEST_MODE'] = 'cluster';
    } else if (options.activefailover) {
      process.env['TEST_MODE'] = 'resilientsingle';
    } else {
      process.env['TEST_MODE'] = 'single';
    }
    process.env['TEST_BACKUP_REMOTE_REPO'] = '';
    process.env['TEST_BACKUP_REMOTE_CONFIG'] = '';
    process.env['GODEBUG'] = 'tls13=1';
    process.env['CGO_ENABLED'] = '0';
    let args = ['test', '-json', '-tags', 'auth', './test/'];

    if (options.testCase) {
      args.push('-run');
      args.push(options.testCase);
    }
    if (options.hasOwnProperty('goOptions')) {
      for (var key in options.goOptions) {
        args.push('-'+key);
        args.push(options.goOptions[key]);
      }
    }
    if (options.extremeVerbosity) {
      print(process.env);
      print(args);
    }
    let start = Date();
    const res = executeExternal('go', args, true, [], options.gosource);
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
              if (options.extremeVerbosity) {
                print(item);
              }
              // alljsonLines.push(item)
              // print(item)
              let testcase = 'WARN';
              if (item.hasOwnProperty('Test')) {
                testcase = item.Test;
              } else {
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
              case 'fail':
                status = false;
                thiscase.status = false;
                thiscase.duration = item.Elapsed;
                break;
              case 'output':
                thiscase.message += item.Output;
                print(item.Time + " - " + item.Output.replace(/^\s+|\s+$/g, ''));
                break;
              case 'pass':
                thiscase.status = true;
                thiscase.duration = item.Elapsed * 1000; // s -> ms
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
  runInGoTest.info = 'runInGoTest';

  let localOptions = _.clone(options);
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }
  localOptions['server.jwt-secret'] = 'haxxmann';

  return tu.performTests(localOptions, [ 'go_test.js'], 'go_test', runInGoTest);
}


exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['go_driver'] = goDriver;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
