/* jshint strict: false, sub: true */
/* global print, params */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
// / @author Jan Christoph Uhde
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////


// server is started in unicode directory

const tu = require('@arangodb/test-utils');

const functionsDocumentation = {
  'paths': 'paths test for server'
};

const testPaths = {
  'paths_server': [tu.pathForTesting('server/paths')]
};

function paths_server(options) {
  let testCases = tu.scanTestPaths(testPaths.paths_server);
  return tu.performTests(options, testCases, require("fs").join('server_paths', "some dog", "ла́ять", "犬", "Kläffer") , tu.runThere);
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['paths_server'] = paths_server;
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
};
