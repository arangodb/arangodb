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
  'server_http': 'http server tests in Mocha',
  'http_replication': 'http replication tests',
  'http_server': 'http server tests in Ruby',
  'ssl_server': 'https server tests'
};
const optionsDocumentation = [
  '   - `skipSsl`: omit the ssl_server rspec tests.',
  '   - `rspec`: the location of rspec program',
  '   - `ruby`: the location of ruby program; if empty start rspec directly'
];

/* Modules: */
const _ = require('lodash');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const yaml = require('js-yaml');
const platform = require('internal').platform;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
// const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const testPaths = {
  'http_replication': [tu.pathForTesting('HttpReplication', 'rb')],
  'http_server': [tu.pathForTesting('HttpInterface', 'rb')],
  'ssl_server': [tu.pathForTesting('HttpInterface', 'rb')],
  'server_http': [tu.pathForTesting('common/http')],
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: shell_http
// //////////////////////////////////////////////////////////////////////////////

function serverHttp (options) {
  // first starts to replace rspec:
  let testCases = tu.scanTestPaths(testPaths.server_http, options);

  return tu.performTests(options, testCases, 'server_http', tu.runThere);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: http_replication
// //////////////////////////////////////////////////////////////////////////////

function httpReplication (options) {
  var opts = {
    'replication': true
  };
  _.defaults(opts, options);
  
  let testCases = tu.scanTestPaths(testPaths.http_replication, options);

  testCases = tu.splitBuckets(options, testCases);

  return tu.performTests(opts, testCases, 'http_replication', tu.runInRSpec);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: http_server
// //////////////////////////////////////////////////////////////////////////////

function httpServer (options) {
  var opts = {
    'httpTrustedOrigin': 'http://was-erlauben-strunz.it'
  };
  _.defaults(opts, options);

  let testCases = tu.scanTestPaths(testPaths.http_server, options);

  testCases = tu.splitBuckets(options, testCases);

  return tu.performTests(opts, testCases, 'http_server', tu.runInRSpec);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: ssl_server
// //////////////////////////////////////////////////////////////////////////////

function sslServer (options) {
  if (options.skipSsl) {
    return {
      ssl_server: {
        status: true,
        skipped: true
      }
    };
  }
  var opts = {
    'httpTrustedOrigin': 'http://was-erlauben-strunz.it',
    'protocol': 'ssl'
  };
  _.defaults(opts, options);

  let testCases = tu.scanTestPaths(testPaths.ssl_server, options);

  testCases = tu.splitBuckets(options, testCases);

  return tu.performTests(opts, testCases, 'ssl_server', tu.runInRSpec);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['http_replication'] = httpReplication;
  testFns['http_server'] = httpServer;
  testFns['server_http'] = serverHttp;
  testFns['ssl_server'] = sslServer;

  defaultFns.push('server_http');
  defaultFns.push('http_server');
  defaultFns.push('ssl_server');

  opts['skipSsl'] = false;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
