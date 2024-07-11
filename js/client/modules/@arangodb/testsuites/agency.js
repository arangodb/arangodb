
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
// / @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'agency': 'run agency tests'
};

const tu = require('@arangodb/testutils/test-utils');
const tr = require('@arangodb/testutils/testrunner');
const trs = require('@arangodb/testutils/testrunners');

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
  let results = new trs.runInArangoshRunner(
    options,  'agency', {},
    (tr.sutFilters.checkUsers.concat(tr.sutFilters.checkCollections)).concat(tr.sutFilters.checkDBs))
      .run(testCases);

  options.agency = saveAgency;
  options.cluster = saveCluster;

  return results;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['agency'] = agency;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
