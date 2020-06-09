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
  '   - `gosource`: directory of the go driver'
];

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const yaml = require('js-yaml');
const platform = require('internal').platform;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'go_driver': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function goDriver (options) {
  
  const adbInstance = pu.startInstance('tcp', options, {}, 'godriver');
  if (adbInstance === false) {
    results.failed += 1;
    results['test'] = {
      failed: 1,
      status: false,
      message: 'failed to start server!'
    };
  }
  print(adbInstance)
  process.env['TEST_ENDPOINTS'] = adbInstance.url;
  process.env['TEST_AUTHENTICATION'] = 'basic:root:';
  process.env['TEST_JWTSECRET'] = 'testing';
  process.env['TEST_CONNECTION'] = '';
  process.env['TEST_CVERSION'] = '';
  process.env['TEST_CONTENT_TYPE'] = 'json';
  process.env['TEST_PPROF'] = '';
  if (options.cluster) {
    process.env['TEST_MODE'] = 'cluster'
  } else if (options.activefailover) {
    process.env['TEST_MODE'] = 'resilientsingle'
  } else {
    process.env['TEST_MODE'] = 'single';
  }
  process.env['TEST_BACKUP_REMOTE_REPO'] = '';
  process.env['TEST_BACKUP_REMOTE_CONFIG'] = '';
  process.env['GODEBUG'] = 'tls13=1';
  process.env['CGO_ENABLED'] = '0';
  const internal = require('internal');

  const executeExternalAndWait = internal.executeExternalAndWait;
  const executeExternal = internal.executeExternal;
  const statusExternal = internal.statusExternal;
  executeExternalAndWait('pwd', [])
  let args = ['test', '-json', '-tags', 'auth', fs.join(options.gosource, 'test')]
  let start = Date();
  const res = executeExternal('go', args,true);
  let alljsonLines = []
  let b = ''
  print(b)
  print(res)
  let rc = {}
  do {
    let buf = fs.readPipe(res.readPipe);
    //print(buf)
    print(buf.length)
    b += buf;
    while (buf.length === 1023) {
      print('.')
      let lineStart = 0;
      let maxBuffer = b.length;
      for (let j = 0; j < maxBuffer; j++) {
        // print(', ' + j + ' - ' + b[j])
        if (b[j] === '\n') { // \n
          let oldLineStart = lineStart;
          print(typeof(b))
          const line = b.slice(lineStart, j);
          lineStart = j + 1;
          try {
            print(line)
            ljson = JSON.parse(line);
            alljsonLines.push(ljson)
            print(ljson)
          } catch (x) {
            print("whut? " + line)
            lineStart = oldLineStart;
          }
        }
        
      }
      b = b.slice(lineStart, b.length);
      buf = fs.readPipe(res.readPipe);
    }
    let rc = statusExternal(res.pid);
    print(rc)
    if (rc.status === 'NOT-FOUND') {
      break;
    }
  } while (rc.status === 'RUNNING')
  fs.write('/tmp/bla.json', JSON.stringify(alljsonLines))
  let end = Date();
}


exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['go_driver'] = goDriver;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
