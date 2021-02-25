/* jshint strict: false, sub: true */
/* global print */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'wal_cleanup': 'wal file cleanup tests'
};

const _ = require('lodash');
const tu = require('@arangodb/testutils/test-utils');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const download = require('internal').download;

const testPaths = {
  'walCleanup': [tu.pathForTesting('client/wal_cleanup')]
};

////////////////////////////////////////////////////////////////////////////////
/// @brief TEST: wal cleanup
////////////////////////////////////////////////////////////////////////////////

function walCleanup (options) {
  print(CYAN + 'WAL cleanup tests...' + RESET);
  let testCases = tu.scanTestPaths(testPaths.walCleanup, options);

  let opts = _.clone(options);
  opts.extraArgs['rocksdb.wal-file-timeout-initial'] = '3';
  opts.cluster = true;

  let rc = tu.performTests(opts, testCases, 'wal_cleanup', tu.runInArangosh, {
    'server.authentication': 'false',
    'rocksdb.wal-file-timeout-initial': '3'
  });
  options.cleanup = options.cleanup && opts.cleanup;
  return rc;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['wal_cleanup'] = walCleanup;

  defaultFns.push('wal_cleanup');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
};
