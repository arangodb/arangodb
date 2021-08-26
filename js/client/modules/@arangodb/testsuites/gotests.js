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
  'gotests': 'integration tests written in golang',
};
const optionsDocumentation = [
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
const platform = require('internal').platform;
const time = require('internal').time;
const wait = require('internal').wait;

const testPaths = {
  'gotests': [ "tests/go" ]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function readAndCollect(pid, buffer) {
  while (true) {
    let buf = fs.readPipe(pid);
    if (buf.length === 0) {
      break;
    }
    buffer.b += buf;
  }
}

function readAndPrint(pid, buffer) {
  while (true) {
    let buf = fs.readPipe(pid);
    if (buf.length === 0) {
      break;
    }
    buffer.b += buf;
    let lineStart = 0;
    let maxBuffer = buffer.b.length;
    for (let j = 0; j < maxBuffer; j++) {
      if (buffer.b[j] === '\n') { // \n
        // OK, we've got a complete line. lets parse it.
        const line = buffer.b.slice(lineStart, j);
        lineStart = j + 1;
        print(line);
      }
    }
    buffer.b = buffer.b.slice(lineStart, buffer.b.length);
  }
}

function goTests (options) {
  function runInGoTest (options, instanceInfo, file, addArgs) {
    print("Hugo:", instanceInfo);
    process.env['TEST_ENDPOINTS'] = instanceInfo.urls.join(',');
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
    let args = ['run'];

    if (options.hasOwnProperty('goOptions')) {
      for (var key in options.goOptions) {
        args.push('-'+key);
        args.push(options.goOptions[key]);
      }
    }
    let pos = file.lastIndexOf("/");
    let basename = file.slice(pos+1);
    let path = file.slice(0, pos);
    args.push(basename)
    args.push("helpers.go")
    if (options.extremeVerbosity) {
      print(process.env);
      print(args);
    }
    let start = Date();
    const res = executeExternal('go', args, true, [], path);
    let results = {};
    let status = true;
    let rc = {};
    let buf = {b:''};
    do {
      readAndCollect(res.pid, buf);
      rc = statusExternal(res.pid);
    } while (rc.status === 'RUNNING');
    //if (options.extremeVerbosity || rc.exit !== 0) {
      print(buf.b)
    //}
    if (rc.exit !== 0) {
      status = false;
    }
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

  let testCases = tu.scanTestPaths(testPaths.gotests, options);
  let rc = tu.performTests(localOptions, testCases, 'gotests', runInGoTest);
  options.cleanup = options.cleanup && localOptions.cleanup;
  return rc;
}


exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['gotests'] = goTests;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
