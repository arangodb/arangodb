/* jshint strict: false, sub: true */
/* global */
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
  'communication': 'communication tests',
  'communication_ssl': 'communication tests with SSL'
};
const optionsDocumentation = [];

const _ = require('lodash');
const tu = require('@arangodb/test-utils');

const testPaths = {
  'communication': [ tu.pathForTesting('client/communication') ],
};

function communication (options) {
  let testCases = tu.scanTestPaths(testPaths.communication, options);
  testCases = tu.splitBuckets(options, testCases);

  return tu.performTests(options, testCases, 'communication', tu.runInLocalArangosh);
}

function communicationSsl (options) {
  let opts = {
    'httpTrustedOrigin': 'http://was-erlauben-strunz.it',
    'protocol': 'ssl'
  };
  _.defaults(opts, options);
  let testCases = tu.scanTestPaths(testPaths.communication, options);
  testCases = tu.splitBuckets(options, testCases);

  return tu.performTests(opts, testCases, 'communication-ssl', tu.runInLocalArangosh);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['communication'] = communication;
  testFns['communication_ssl'] = communicationSsl;
  
  // intentionally not turned on by default, as the suite may take a lot of time
  // defaultFns.push('communication');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
