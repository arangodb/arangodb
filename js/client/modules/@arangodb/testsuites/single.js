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
  'single_client': 'run one test suite isolated via the arangosh; options required\n' +
    '            run without arguments to get more detail',
  'single_server': 'run one test suite on the server; options required\n' +
    '            run without arguments to get more detail'
};
const optionsDocumentation = [
];

const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');

const testPaths = {
  'single_server': [],
  'single_client': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: single_client
// //////////////////////////////////////////////////////////////////////////////

function singleUsage (testsuite, list) {
  print('single_' + testsuite + ': No test specified!\n Available tests:');
  let filelist = '';

  for (let fileNo in list) {
    if (/\.js$/.test(list[fileNo])) {
      filelist += ' ' + list[fileNo];
    }
  }

  print(filelist);
  print('usage: single_' + testsuite + ' \'{"test":"<testfilename>"}\'');
  print(' where <testfilename> is one from the list above.');

  return {
    usage: {
      status: false,
      message: 'No test specified!'
    }
  };
}

function singleClient (options) {
  options.writeXmlReport = false;

  if (options.test !== undefined) {
    let instanceInfo = pu.startInstance('tcp', options, {}, 'single_client');

    if (instanceInfo === false) {
      return {
        single_client: {
          status: false,
          message: 'failed to start server!'
        }
      };
    }

    const te = options.test;

    print('\n' + Date() + ' arangosh: Trying ', te, '...');

    let result = {};

    result[te] = tu.runInArangosh(options, instanceInfo, te);

    print('Shutting down...');

    if (result[te].status === false) {
      options.cleanup = false;
    }

    result['shutdown'] = pu.shutdownInstance(instanceInfo, options);

    print('done.');

    return result;
  } else {
    // findTests();
    return singleUsage('client', []); // TODO find testcase
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: single_server
// //////////////////////////////////////////////////////////////////////////////

function singleServer (options) {
  options.writeXmlReport = false;

  if (options.test === undefined) {
    // findTests(); //TODO
    return singleUsage('server', []); // TODO testsCases.server);
  }

  let instanceInfo = pu.startInstance('tcp', options, {}, 'single_server');

  if (instanceInfo === false) {
    return {
      single_server: {
        status: false,
        message: 'failed to start server!'
      }
    };
  }

  const te = options.test;

  print('\n' + Date() + ' arangod: Trying', te, '...');

  let result = {};
  let reply = tu.runThere(options, instanceInfo, tu.makePathGeneric(te));

  if (reply.hasOwnProperty('status')) {
    result[te] = reply;

    if (result[te].status === false) {
      options.cleanup = false;
    }
  }

  print('Shutting down...');

  if (result[te].status === false) {
    options.cleanup = false;
  }

  result['shutdown'] = pu.shutdownInstance(instanceInfo, options);

  print('done.');

  return result;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['single_server'] = singleServer;
  testFns['single_client'] = singleClient;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
