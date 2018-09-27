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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'version': 'version check test'
};

const fs = require('fs');
const pu = require('@arangodb/process-utils');

const testPaths = {
  'version': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: version
// //////////////////////////////////////////////////////////////////////////////

function version(options) {
  // create empty data directory
  const dataDir = fs.join(fs.getTempPath(), 'version');

  // use same configuration as dfdb, works here.
  const args = ['-c', fs.join(pu.CONFIG_DIR, 'arango-dfdb.conf'), dataDir];
  if (options.storageEngine !== undefined) {
    args.push('--server.storage-engine');
    args.push(options.storageEngine);
  }
  args.push('--database.check-version');
  args.push('true');
  
  args.push('--server.rest-server');
  args.push('false');

  fs.makeDirectoryRecursive(dataDir);
  pu.cleanupDBDirectoriesAppend(dataDir);

  let results = { failed: 0 };

  results.version = pu.executeAndWait(pu.ARANGOD_BIN, args, options, 'version', dataDir, false, options.coreCheck);

  print();
  results.version.failed = results.version.status ? 0 : 1;
  if (!results.version.status) {
    results.failed += 1;
  }

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['version'] = version;
  defaultFns.push('version');
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
};
