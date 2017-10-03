/* jshint strict: false, sub: true */
/* global print */
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
  'endpoints': 'endpoints tests'
};
const optionsDocumentation = [
  '   - `skipEndpoints`: if set to true endpoints tests are skipped'
];

const fs = require('fs');
const pu = require('@arangodb/process-utils');
const tu = require('@arangodb/test-utils');

const platform = require('internal').platform;

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

function endpoints (options) {
  print(CYAN + 'Endpoints tests...' + RESET);

  let endpoints = {
    'tcpv4': function () {
      return 'tcp://127.0.0.1:' + pu.findFreePort(options.minPort, options.maxPort);
    },
    'tcpv6': function () {
      return 'tcp://[::1]:' + pu.findFreePort(options.minPort, options.maxPort);
    },
    'unix': function () {
      if (platform.substr(0, 3) === 'win') {
        return undefined;
      }
      return 'unix://./arangodb-tmp.sock';
    }
  };
  // we append one cleanup directory for the invoking logic...
  let dummyDir = fs.join(fs.getTempPath(), 'enpointsdummy');
  fs.makeDirectory(dummyDir);
  pu.cleanupDBDirectoriesAppend(dummyDir);

  return Object.keys(endpoints).reduce((results, endpointName) => {
    results.failed = 0;
    let testName = 'endpoint-' + endpointName;
    results[testName] = (function () {
      let endpoint = endpoints[endpointName]();
      if (endpoint === undefined || options.cluster || options.skipEndpoints) {
        return {
          failed: 0,
          status: true,
          skipped: true
        };
      } else {
        let instanceInfo = pu.startInstance('tcp', Object.assign(options, {useReconnect: true}), {
          'server.endpoint': endpoint
        }, testName);

        if (instanceInfo === false) {
          result.failed += 1;
          return {
            failed: 1,
            status: false,
            message: 'failed to start server!'
          };
        }

        let result = tu.runInArangosh(options, instanceInfo, 'js/client/tests/endpoint-spec.js');

        print(CYAN + 'Shutting down...' + RESET);
        // mop: mehhh...when launched with a socket we can't use download :S
        pu.shutdownInstance(instanceInfo, Object.assign(options, {useKillExternal: true}));
        print(CYAN + 'done.' + RESET);

        if (!result.status) {
          result.failed += 1;
        } else {
          pu.cleanupLastDirectory(options);
        }
        return result;
      }
    }());
    return results;
  }, {});
}

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['endpoints'] = endpoints;

  opts['skipEndpoints'] = false;

  defaultFns.push('endpoints');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
