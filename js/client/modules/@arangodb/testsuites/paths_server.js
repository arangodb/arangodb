/* jshint strict: false, sub: true */
/* global print, params */
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
// / @author Jan Christoph Uhde
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////


// server is started in unicode directory

const tu = require('@arangodb/testutils/test-utils');
const trs = require('@arangodb/testutils/testrunners');
const fs = require('fs');
const _ = require('lodash');

const functionsDocumentation = {
  'paths': 'paths test for server'
};

const testPaths = {
  'paths_server': [tu.pathForTesting('server/paths')]
};

function paths_server(options) {
  let testCases = tu.scanTestPaths(testPaths.paths_server, options);
  let weirdNames = ['some dog', 'ла́ять', '犬', 'Kläffer'];
  let tmpPath = fs.getTempPath();
  let tmp = fs.join(tmpPath, "x", weirdNames[0], weirdNames[1], weirdNames[2], weirdNames[3]);
  process.env.TMPDIR = tmp;
  process.env.TEMP = tmp;
  process.env.TMP = tmp;
  fs.makeDirectoryRecursive(process.env.TMPDIR);
  let opts = _.clone(options);
  // procdump can't stand weird characters in programm options
  opts.disableMonitor = true;
  let rc = new trs.runOnArangodRunner(opts, fs.join('server_paths', weirdNames[0], weirdNames[1], weirdNames[2], weirdNames[3])).run(testCases);
  process.env.TMPDIR = tmpPath;
  process.env.TEMP = tmpPath;
  process.env.TMP = tmpPath;
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['paths_server'] = paths_server;
  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
