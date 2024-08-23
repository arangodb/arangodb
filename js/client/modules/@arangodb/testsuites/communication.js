/* jshint strict: false, sub: true */
/* global */
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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'communication': 'communication tests',
  'communication_ssl': 'communication tests with SSL'
};

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');

const testPaths = {
  'communication': [ tu.pathForTesting('client/communication') ],
};

function communication (options) {
  let testCases = tu.scanTestPaths(testPaths.communication, options);
  testCases = tu.splitBuckets(options, testCases);

  return new trs.runLocalInArangoshRunner(options, 'communication', {}).run(testCases);
}

function communicationSsl (options) {
  let opts = {
    'httpTrustedOrigin': 'http://was-erlauben-strunz.it',
    'protocol': 'ssl'
  };
  _.defaults(opts, options);
  let testCases = tu.scanTestPaths(testPaths.communication, options);
  testCases = tu.splitBuckets(options, testCases);

  return new trs.runLocalInArangoshRunner(opts, 'communication-ssl', {}).run(testCases);
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['communication'] = communication;
  testFns['communication_ssl'] = communicationSsl;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
