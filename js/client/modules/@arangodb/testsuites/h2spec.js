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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'h2spec': 'h2spec HTTP/2 tests',
};
const optionsDocumentation = [
  '   - `h2SpecPath`: directory of the h2spec executable',
];

const internal = require('internal');

const executeExternal = internal.executeExternal;
const statusExternal = internal.statusExternal;

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');
const time = require('internal').time;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
// const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'h2spec': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: h2spec
// //////////////////////////////////////////////////////////////////////////////

function h2Spec (options) {
  function runH2SpecTest (options, instanceInfo, file, addArgs) {
    let host = instanceInfo.endpoint.replace(/:\d+$/, '').replace(/^(tcp|ssl|vst|http|https):\/\//, '');
    let port = instanceInfo.endpoint.replace(/^.*?:(\d+)$/, '$1');
    let tls = instanceInfo.protocol === 'ssl';

    process.env['GODEBUG'] = 'tls13=1';
    process.env['CGO_ENABLED'] = '0';
    
    // -p port
    // -t tls
    // -h host
    // -k insecure
    let args = ['-k', '-h', host, '-p', port];
    if (tls) {
      args.push('-t');
    }

    // the test cases we support
    args.push('generic/1');
    args.push('generic/2/1');
    args.push('generic/2/4');
    args.push('generic/2/5');
    args.push('generic/3');
    args.push('generic/4');
    args.push('generic/5');
    args.push('http2/3');
    args.push('http2/4');
    args.push('http2/5.1.2');
    args.push('http2/5.3');
    args.push('http2/5.4');
    args.push('http2/5.5');
    args.push('http2/6');
    args.push('http2/7');
    args.push('http2/8');
    args.push('hpack/2');
    args.push('hpack/4');
    args.push('hpack/5');
    args.push('hpack/6');

    if (!options.h2SpecPath) {
      options.h2SpecPath = '.';
    }
    
    if (options.extremeVerbosity) {
      print(process.env);
      print(args);
    }

    let path = fs.join(options.h2SpecPath, 'h2spec');

    if (!fs.exists(path)) {
      return {
        status: false,
        message: "h2spec executable not found in path " + path
      };
    }

    let start = Date();
    const res = executeExternal(path, args, true, [], options.h2SpecPath);
    let b = '';
    let results = {};
    let status = true;
    let rc = {};
    let result = '';
    do {
      let buf = fs.readPipe(res.pid);

      result += buf.slice(0, buf.length);
      let lines = result.split('\n');
      if (lines.length > 1) {
        print(lines.join('\n'));
        result = lines[lines.length - 1];
      }

      rc = statusExternal(res.pid);

      if (rc.status === 'NOT-FOUND') {
        break;
      }
    } while (rc.status === 'RUNNING');
    if (result.length > 0) {
      print(result);
    }
    if (rc.exit !== 0) {
      status = false;
    }
    results['timeout'] = false;
    results['status'] = status;
    results['message'] = '';
    return results;
  }
  runH2SpecTest.info = 'runH2SpecTest';
  return tu.performTests(options, [ 'h2spec_test.js'], 'h2spec_test', runH2SpecTest);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['h2spec'] = h2Spec;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
