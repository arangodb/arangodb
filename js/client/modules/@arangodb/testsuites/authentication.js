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

  return new trs.runLocalInArangoshRunner(
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

// server parameters per authentication configuration under test
const authTestConfigs = {
  Full: {
    'server.authentication': 'true',
    'server.authentication-system-only': 'false'
  },
  SystemAuth: {
    'server.authentication': 'true',
    'server.authentication-system-only': 'true'
  },
  None: {
    'server.authentication': 'false',
    'server.authentication-system-only': 'true'
  }
};

// every 301 below is a redirect to the web UI of the _system database
const authTestRedirectLocation = '/_db/_system/_admin/aardvark/index.html';

// expected HTTP response code per URL and authentication configuration
const authTestUrls = {
  '/_api/':           { Full: 401, SystemAuth: 401, None: 404 },
  '/_api':            { Full: 401, SystemAuth: 401, None: 404 },
  '/_api/version':    { Full: 401, SystemAuth: 401, None: 200 },
  '/_admin/html':     { Full: 401, SystemAuth: 401, None: 301 },
  '/_admin/html/':    { Full: 401, SystemAuth: 401, None: 301 },
  '/':                { Full: 301, SystemAuth: 301, None: 301 },
  '//':               { Full: 301, SystemAuth: 301, None: 301 },
  '/_db/_system/':    { Full: 301, SystemAuth: 301, None: 301 },
  '/_db/_system':     { Full: 401, SystemAuth: 401, None: 301 },
  '/test':            { Full: 401, SystemAuth: 404, None: 404 },
  '/the-big-fat-fox': { Full: 401, SystemAuth: 404, None: 404 }
};

function checkBodyForJsonToParse (request) {
  if (request.hasOwnProperty('body')) {
    request.body = JSON.parse(request.hasOwnProperty('body'));
  }
}

function authenticationParameters (options) {
  if (options.skipServerJS) {
    return {
      authentication: {
        status: true,
        message: 'server javascript not enabled. please recompile with -DUSE_V8=on'
      },
      status: true
    };
  }
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

  for (const [authTestName, authTestServerParams] of Object.entries(authTestConfigs)) {
    let cleanup = true;

    let instanceManager = new im.instanceManager('tcp', options,
                                                 authTestServerParams,
                                                 'authentication_parameters_' + authTestName);
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

    print(CYAN + Date() + ' Starting ' + authTestName + ' test' + RESET);

    const testName = 'auth_' + authTestName;
    results[testName] = {
      failed: 0,
      total: 0
    };

    for (const [authTestUrl, expectedRCs] of Object.entries(authTestUrls)) {
      const expectedRC = expectedRCs[authTestName];

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

      if (reply.code !== expectedRC) {
        checkBodyForJsonToParse(reply);

        ++results[testName].failed;

        results[testName][authTestUrl] = {
          status: false,
          message: 'we expected ' +
            expectedRC +
            ' and we got ' + reply.code +
            ' Full Status: ' + yaml.safeDump(reply)
        };
        cleanup = false;
      } else if (reply.code === 301 &&
                 reply.headers['location'] !== authTestRedirectLocation) {
        ++results[testName].failed;

        results[testName][authTestUrl] = {
          status: false,
          message: 'we expected a redirect to ' +
            authTestRedirectLocation +
            ' and we got ' + reply.headers['location'] +
            ' Full Status: ' + yaml.safeDump(reply)
        };
        cleanup = false;
      } else {
        results[testName][authTestUrl] = {
          status: true
        };
      }

      continueTesting = instanceManager.checkInstanceAlive();
    }

    results[testName].status = results[testName].failed === 0;

    print(CYAN + 'Shutting down ' + authTestName + ' test...' + RESET);
    results['shutdown'] = instanceManager.shutdownInstance();
    print(CYAN + 'done with ' + authTestName + ' test.' + RESET);
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
