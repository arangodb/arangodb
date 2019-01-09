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
  'catch': 'catch test suites'
};
const optionsDocumentation = [
  '   - `skipCache`: if set to true, the hash cache unittests are skipped',
  '   - `skipCatch`: if set to true the catch unittests are skipped',
  '   - `skipGeo`: obsolete and only here for downwards-compatibility'
];

const fs = require('fs');
const pu = require('@arangodb/process-utils');

const testPaths = {
  'catch': [],
  'boost': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: Catch
// //////////////////////////////////////////////////////////////////////////////

function locateCatchTest (name) {
  var file = fs.join(pu.UNITTESTS_DIR, name + pu.executableExt);

  if (!fs.exists(file)) {
    file = fs.join(pu.BIN_DIR, name + pu.executableExt);
    if (!fs.exists(file)) {
      return '';
    }
  }
  return file;
}

function catchRunner (options) {
  let results = { failed: 0 };
  let rootDir = pu.UNITTESTS_DIR;

  // we append one cleanup directory for the invoking logic...
  let dummyDir = fs.join(fs.getTempPath(), 'catch_dummy');
  if (!fs.exists(dummyDir)) {
    fs.makeDirectory(dummyDir);
  }
  pu.cleanupDBDirectoriesAppend(dummyDir);

  const run = locateCatchTest('arangodbtests');
  if (!options.skipCatch) {
    if (run !== '') {
      let argv = [
        '--log.line-number',
        options.extremeVerbosity ? "true" : "false",
        '[exclude:longRunning][exclude:cache]'
      ];
      results.basics = pu.executeAndWait(run, argv, options, 'all-catch', rootDir, false, options.coreCheck);
      results.basics.failed = results.basics.status ? 0 : 1;
      if (!results.basics.status) {
        results.failed += 1;
      }
    } else {
      results.failed += 1;
      results.basics = {
        failed: 1,
        status: false,
        message: 'binary "arangodbtests" not found when trying to run suite "all-catch"'
      };
    }
  }

  if (!options.skipCache) {
    if (run !== '') {
      let argv = [
        '--log.line-number',
        options.extremeVerbosity ? "true" : "false",
        '[cache][exclude:longRunning]',
        '-r',
        'junit',
        '-o',
        fs.join(options.testOutputDirectory, 'catch-cache.xml')
      ];
      results.cache_suite = pu.executeAndWait(run, argv, options,
                                              'cache_suite', rootDir, false, options.coreCheck);
      results.cache_suite.failed = results.cache_suite.status ? 0 : 1;
      if (!results.cache_suite.status) {
        results.failed += 1;
      }
    } else {
      results.failed += 1;
      results.cache_suite = {
        failed: 1,
        status: false,
        message: 'binary "arangodbtests" not found when trying to run suite "cache_suite"'
      };
    }
  }

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['catch'] = catchRunner;
  testFns['boost'] = catchRunner;

  opts['skipCatch'] = false;
  opts['skipCache'] = true;
  opts['skipGeo'] = false; // not used anymore - only here for downwards-compatibility

  defaultFns.push('catch');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
