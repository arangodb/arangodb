/* jshint strict: false, sub: true */
/* global print */
'use strict';

// /////////////////////////////////////////////////////////////////////////////
// DISCLAIMER
// 
// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
// Copyright 2014 triagens GmbH, Cologne, Germany
// 
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//      http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// 
// Copyright holder is ArangoDB GmbH, Cologne, Germany
// 
// @author Max Neunhoeffer
// /////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'config': 'checks the config file parsing'
};
const optionsDocumentation = [
  '   - `skipConfig`: omit the noisy configuration tests'
];

const fs = require('fs');
const pu = require('@arangodb/process-utils');
const yaml = require('js-yaml');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
const YELLOW = require('internal').COLORS.COLOR_YELLOW;

const time = require('internal').time;
const toArgv = require('internal').toArgv;

const testPaths = {
  'config': []
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: config
// //////////////////////////////////////////////////////////////////////////////

function config (options) {
  if (options.skipConfig) {
    return {
      config: {
        failed: 0,
        status: true,
        skipped: true
      }
    };
  }

  let results = {
    failed: 0,
    absolute: {
      failed: 0,
      status: true,
      total: 0,
      duration: 0
    },
    relative: {
      failed: 0,
      status: true,
      total: 0,
      duration: 0
    }
  };

  const ts = [
    'arangod',
    'arangobench',
    'arangodump',
    'arangoimport',
    'arangorestore',
    'arangoexport',
    'arangosh',
    'arango-dfdb',
    'foxx-manager'
  ];

  let rootDir = pu.UNITTESTS_DIR;

  print('--------------------------------------------------------------------------------');
  print('absolute config tests');
  print('--------------------------------------------------------------------------------');

  if (options.skipTest('TEST', 'config.absolute')) {
    print(YELLOW + "[SKIPPED] config.absolute" + RESET + "\n");
    results.absolute.skipped = true;
  } else {
    // we append one cleanup directory for the invoking logic...
    let dummyDir = fs.join(fs.getTempPath(), 'configdummy');
    fs.makeDirectory(dummyDir);
    pu.cleanupDBDirectoriesAppend(dummyDir);

    let startTime = time();

    for (let i = 0; i < ts.length; i++) {
      const test = ts[i];
      print(CYAN + 'checking "' + test + '"' + RESET);

      const args = {
        'configuration': fs.join(pu.CONFIG_ARANGODB_DIR, test + '.conf'),
        'flatCommands': ['--check-configuration']
      };

      const run = fs.join(pu.BIN_DIR, test);

      results.absolute[test] = pu.executeAndWait(run, toArgv(args), options, test, rootDir, false, options.coreCheck);

      if (!results.absolute[test].status) {
        results.absolute.status = false;
        results.absolute.failed += 1;
        results.failed += 1;
      }

      results.absolute.total++;

      if (options.verbose) {
        print('Args for [' + test + ']:');
        print(yaml.safeDump(args));
        print('Result: ' + results.absolute[test].status);
      }
    }

    results.absolute.duration = time() - startTime;
  }

  print('\n--------------------------------------------------------------------------------');
  print('relative config tests');
  print('--------------------------------------------------------------------------------');

  if (options.skipTest('TEST', 'config.relative')) {
    print(YELLOW + "[SKIPPED] config.relative" + RESET + "\n");
    results.relative.skipped = true;
  } else {
    let startTime = time();

    for (let i = 0; i < ts.length; i++) {
      const test = ts[i];
      print(CYAN + 'checking "' + test + '"' + RESET);

      const args = {
        'configuration': fs.join(pu.CONFIG_RELATIVE_DIR, test + '.conf'),
        'flatCommands': ['--check-configuration']
      };

      const run = fs.join(pu.BIN_DIR, test);

      results.relative[test] = pu.executeAndWait(run, toArgv(args), options, test, rootDir, false, options.coreCheck);

      if (!results.relative[test].status) {
        results.failed += 1;
        results.relative.failed += 1;
        results.relative.status = false;
      }

      results.relative.total++;

      if (options.verbose) {
        print('Args for (relative) [' + test + ']:');
        print(yaml.safeDump(args));
        print('Result: ' + results.relative[test].status);
      }
    }

    results.relative.duration = time() - startTime;
  }

  print();

  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc, allTestPaths) {
  Object.assign(allTestPaths, testPaths);
  testFns['config'] = config;
  defaultFns.push('config');
  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
