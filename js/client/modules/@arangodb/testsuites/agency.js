
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'agency': 'run agency tests'
};
const optionsDocumentation = [
];

const tu = require('@arangodb/test-utils');

const testPaths = {
  'agency': [tu.pathForTesting('client/agency')]
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief agency tests
// //////////////////////////////////////////////////////////////////////////////

function agency (options) {
  let testCases = tu.scanTestPaths(testPaths.agency, options);

  let saveAgency = options.agency;
  let saveCluster = options.cluster;

  options.agency = true;
  options.cluster = false;
  let results = tu.performTests(options, testCases, 'agency', tu.runInArangosh);

  options.agency = saveAgency;
  options.cluster = saveCluster;

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['agency'] = agency;
  defaultFns.push('agency');
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
