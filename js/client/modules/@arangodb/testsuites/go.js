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
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
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
  let localOptions = _.clone(options);
  let beforeStart = time();  
  if (localOptions.cluster && localOptions.dbServers < 3) {
    localOptions.dbServers = 3;
  }
  const adbInstance = pu.startInstance('tcp', localOptions, {}, 'godriver');
  if (adbInstance === false) {
    results.failed += 1;
    results['test'] = {
      failed: 1,
      status: false,
      message: 'failed to start server!'
    };
  }
  if (localOptions.extremeVerbosity) {
    print(adbInstance);
  }
  let testrunStart = time();
  let results = {
    shutdown: true,
    startupTime: testrunStart - beforeStart
  };

  let opts = '';
  if (localOptions.testCase) {
    opts = '-run ' + localOptions.testCase;
  }
  if (localOptions.hasOwnProperty('goOptions')) {
    if (opts.length !== 0) {
      opts += ' ';
    }
    opts += localOptions.goOptions;
  }
  
  if (opts.length !== 0) {
    process.env['TEST_OPTIONS'] = opts;
  }
  process.env['TEST_ENDPOINTS'] = adbInstance.url;
  process.env['TEST_AUTHENTICATION'] = 'basic:root:';
  process.env['TEST_JWTSECRET'] = pu.getJwtSecret(localOptions);
  process.env['TEST_CONNECTION'] = '';
  process.env['TEST_CVERSION'] = '';
  process.env['TEST_CONTENT_TYPE'] = 'json';
  process.env['TEST_PPROF'] = '';
  if (localOptions.cluster) {
    process.env['TEST_MODE'] = 'cluster';
  } else if (localOptions.activefailover) {
    process.env['TEST_MODE'] = 'resilientsingle';
  } else {
    process.env['TEST_MODE'] = 'single';
  }
  process.env['TEST_BACKUP_REMOTE_REPO'] = '';
  process.env['TEST_BACKUP_REMOTE_CONFIG'] = '';
  process.env['GODEBUG'] = 'tls13=1';
  process.env['CGO_ENABLED'] = '0';
  if (localOptions.extremeVerbosity) {
    print(process.env);
  }
  let args = ['test', '-json', '-tags', 'auth', fs.join(localOptions.gosource, 'test')];
  let start = Date();
  const res = executeExternal('go', args,true);
  // let alljsonLines = []
  let b = '';
  let status = true;
  let rc = {};
  results['go_driver'] = {};
  let testResults = results['go_driver'];
  do {
    let buf = fs.readPipe(res.pid);
    b += buf;
    while (buf.length === 1023) {
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
            // alljsonLines.push(item)
            // print(item)
            let testcase = item.Test;
            if (!testResults.hasOwnProperty(testcase)) {
              testResults[testcase] = {
                "setUpDuration": 0,
                "tearDownDuration": 0,
                "status": false,
                "duration": 0,
                "message": ''
              };
            }
            let thiscase = testResults[testcase];
            switch(item.Action) {
            case 'fail':
              status = false;
              thiscase.status = false;
              thiscase.duration = item.Elapsed;
              break;
            case 'output':
              thiscase.message += item.Output;
              print(item.Output.replace(/^\s+|\s+$/g, ''));
              break;
            case 'pass':
              thiscase.status = true;
              thiscase.duration = item.Elapsed;
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
    let rc = statusExternal(res.pid);
    if (rc.status === 'NOT-FOUND') {
      break;
    }
  } while (rc.status === 'RUNNING');
  // fs.write('/tmp/bla.json', JSON.stringify(alljsonLines))
  let shutDownStart = time();
  print(Date() + ' Shutting down...');
  pu.shutdownInstance(adbInstance, localOptions, false);
  results['testDuration'] = shutDownStart - testrunStart;
  let end = time();
  results['go_driver']['status'] = status;
  results['go_driver']['startupTime'] =  0;
  results['testDuration'] =  end - start;
  results['shutdownTime'] = time() - shutDownStart;
  return results;
}


exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['go_driver'] = goDriver;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
