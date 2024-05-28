/* jshint strict: false, sub: true */
/* global print */
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
  'authentication': 'authentication tests',
  'authentication_parameters': 'authentication parameters tests'
};

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');
const tr = require('@arangodb/testutils/testrunner');
const trs = require('@arangodb/testutils/testrunners');
const im = require('@arangodb/testutils/instance-manager');
const yaml = require('js-yaml');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const download = require('internal').download;

const testPaths = {
  'authentication': [tu.pathForTesting('client/authentication')],
  'authentication_parameters': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: authentication
// //////////////////////////////////////////////////////////////////////////////

function authenticationClient (options) {
  print(CYAN + 'Client Authentication tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.authentication, options);

  testCases = tu.splitBuckets(options, testCases);

  return new trs.runInArangoshRunner(
    options,
    'authentication',
    Object.assign({},
                  tu.testServerAuthInfo, {
                    'cluster.create-waits-for-sync-replication': false
                  }),
    tr.sutFilters.checkUsers).run(testCases);
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: authentication parameters
// //////////////////////////////////////////////////////////////////////////////

const authTestExpectRC = [
  [401, 401, 401, 401, 401, 401, 401],
  [401, 401, 401, 401, 401, 404, 404],
  [404, 404, 200, 301, 301, 404, 404]
];

const authTestUrls = [
  '/_api/',
  '/_api',
  '/_api/version',
  '/_admin/html',
  '/_admin/html/',
  '/test',
  '/the-big-fat-fox'
];

const authTestNames = [
  'Full',
  'SystemAuth',
  'None'
];

const authTestServerParams = [{
  'server.authentication': 'true',
  'server.authentication-system-only': 'false'
}, {
  'server.authentication': 'true',
  'server.authentication-system-only': 'true'
}, {
  'server.authentication': 'false',
  'server.authentication-system-only': 'true'
}];

function checkBodyForJsonToParse (request) {
  if (request.hasOwnProperty('body')) {
    request.body = JSON.parse(request.hasOwnProperty('body'));
  }
}

function authenticationParameters (options) {
  if (options.cluster) {
    print('skipping Authentication with parameters tests on cluster!');
    return {
      authentication: {
        status: true,
        skipped: true
      }
    };
  }

  print(CYAN + 'Authentication with parameters tests...' + RESET);

  let downloadOptions = {
    followRedirects: false,
    returnBodyOnError: true
  };

  if (options.valgrind) {
    downloadOptions.timeout = 300;
  }

  let continueTesting = true;
  let results = {};

  for (let test = 0; test < 3; test++) {
    let cleanup = true;

    let instanceManager = new im.instanceManager('tcp', options,
                                                 authTestServerParams[test],
                                                 'authentication_parameters_' + authTestNames[test]);
    instanceManager.prepareInstance();
    instanceManager.launchTcpDump("");
    if (!instanceManager.launchInstance()) {
      return {
        authentication_parameters: {
          status: false,
          message: 'failed to launch instance'
        }
      };
    }
    instanceManager.reconnect();

    print(CYAN + Date() + ' Starting ' + authTestNames[test] + ' test' + RESET);

    const testName = 'auth_' + authTestNames[test];
    results[testName] = {
      failed: 0,
      total: 0
    };

    for (let i = 0; i < authTestUrls.length; i++) {
      const authTestUrl = authTestUrls[i];

      ++results[testName].total;

      print(CYAN + '  URL: ' + instanceManager.url + authTestUrl + RESET);

      if (!continueTesting) {
        print(RED + 'Skipping ' + authTestUrl + ', server is gone.' + RESET);

        results[testName][authTestUrl] = {
          status: false,
          message: instanceManager.exitStatus
        };

        results[testName].failed++;
        instanceManager.exitStatus = 'server is gone.';
        cleanup = false;
        break;
      }

      let reply = download(instanceManager.url + authTestUrl, '', downloadOptions);

      if (reply.code === authTestExpectRC[test][i]) {
        results[testName][authTestUrl] = {
          status: true
        };
      } else {
        checkBodyForJsonToParse(reply);

        ++results[testName].failed;

        results[testName][authTestUrl] = {
          status: false,
          message: 'we expected ' +
            authTestExpectRC[test][i] +
            ' and we got ' + reply.code +
            ' Full Status: ' + yaml.safeDump(reply)
        };
        cleanup = false;
      }

      continueTesting = instanceManager.checkInstanceAlive();
    }

    results[testName].status = results[testName].failed === 0;

    print(CYAN + 'Shutting down ' + authTestNames[test] + ' test...' + RESET);
    results['shutdown'] = instanceManager.shutdownInstance();
    print(CYAN + 'done with ' + authTestNames[test] + ' test.' + RESET);
  }

  print();

  return results;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['authentication'] = authenticationClient;
  testFns['authentication_parameters'] = authenticationParameters;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
