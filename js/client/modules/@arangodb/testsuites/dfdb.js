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
  'dfdb': 'start test'
};
const optionsDocumentation = [
];

const fs = require('fs');
const pu = require('@arangodb/process-utils');

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: dfdb
// //////////////////////////////////////////////////////////////////////////////

function dfdb (options) {
  const dataDir = fs.join(fs.getTempPath(), 'dfdb');
  fs.makeDirectory(dataDir);

  const args = ['-c', fs.join(pu.CONFIG_DIR, 'arango-dfdb.conf'), dataDir];
  if (options.storageEngine !== undefined) {
    args.push('--server.storage-engine');
    args.push(options.storageEngine);
  }

  fs.makeDirectoryRecursive(dataDir);
  pu.cleanupDBDirectoriesAppend(dataDir);

  let results = { failed: 0 };

  results.dfdb = pu.executeAndWait(pu.ARANGOD_BIN, args, options, 'dfdb', dataDir);

  print();
  results.dfdb.failed = results.dfdb.status ? 0 : 1;
  if (!results.dfdb.status) {
    results.failed += 1;
  }

  return results;
}

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['dfdb'] = dfdb;
  defaultFns.push('dfdb');
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
