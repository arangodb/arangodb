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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'version': 'version check test'
};

const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const tu = require('@arangodb/testutils/test-utils');

const testPaths = {
  'version': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: version
// //////////////////////////////////////////////////////////////////////////////

function version(options) {
  // create empty data directory
  const dataDir = fs.join(fs.getTempPath(), 'version');

  const args = ['-c', 'none', dataDir];
  if (options.storageEngine !== undefined) {
    args.push('--server.storage-engine');
    args.push(options.storageEngine);
  }
  args.push('--database.auto-upgrade');
  args.push('false');
  args.push('--database.upgrade-check');
  args.push('false');
  args.push('--database.check-version');
  args.push('true');
  args.push('--foxx.queues');
  args.push('false');
  args.push('--server.rest-server');
  args.push('false');
  args.push('--server.statistics');
  args.push('false');
  args.push('--javascript.startup-directory');
  args.push(fs.join('.', 'js'));
  args.push('--javascript.app-path');
  args.push(fs.join('.', 'js', 'apps'));
  args.push('--javascript.v8-contexts');
  args.push('1');
  
  fs.makeDirectoryRecursive(dataDir);

  let results = { failed: 0 };

  results.version = pu.executeAndWait(pu.ARANGOD_BIN, args, options, 'version', dataDir, options.coreCheck);

  print();
  results.version.failed = results.version.status ? 0 : 1;
  if (!results.version.status) {
    results.failed += 1;
  }
  if (options.cleanup && results.failed === 0) {
    fs.removeDirectoryRecursive(dataDir, true);
  }
  return results;
}

exports.setup = function (testFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['version'] = version;

  tu.CopyIntoObject(fnDocs, functionsDocumentation);
};
