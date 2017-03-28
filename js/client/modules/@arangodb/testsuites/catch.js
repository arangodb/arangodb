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
  '   - `skipGeo`: if set to true the geo index tests are skipped'
];

const fs = require('fs');
const pu = require('@arangodb/process-utils');

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: Catch
// //////////////////////////////////////////////////////////////////////////////

function locateCatchTest (name) {
  var file = fs.join(pu.UNITTESTS_DIR, name + pu.executableExt);

  if (!fs.exists(file)) {
    return '';
  }
  return file;
}

function catchRunner (options) {
  let results = {};
  let rootDir = pu.UNITTESTS_DIR;

  const icuDir = pu.UNITTESTS_DIR + '/';
  require('internal').env.ICU_DATA = icuDir;
  const run = locateCatchTest('arangodbtests');
  if (!options.skipCatch) {
    if (run !== '') {
      let argv = [
        '[exclude:longRunning][exclude:cache]',
        '-r',
        'junit',
        '-o',
        fs.join('out', 'catch-standard.xml')];
      results.basics = pu.executeAndWait(run, argv, options, 'all-catch', rootDir);
    } else {
      results.basics = {
        status: false,
        message: 'binary "basics_suite" not found'
      };
    }
  }

  if (!options.skipCache) {
    if (run !== '') {
      let argv = ['[cache]',
                  '-r',
                  'junit',
                  '-o',
                  fs.join('out', 'catch-cache.xml')
                 ];
      results.cache_suite = pu.executeAndWait(run, argv, options,
                                           'cache_suite', rootDir);
    } else {
      results.cache_suite = {
        status: false,
        message: 'binary "cache_suite" not found'
      };
    }
  }

  if (!options.skipGeo) {
    if (run !== '') {
      let argv = ['[geo][exclude:longRunning]',
                  '-r',
                  'junit',
                  '-o',
                  fs.join('out', 'catch-geo.xml')
                 ];
      results.geo_suite = pu.executeAndWait(run, argv, options, 'geo_suite', rootDir);
    } else {
      results.geo_suite = {
        status: false,
        message: 'binary "geo_suite" not found'
      };
    }
  }

  return results;
}

function setup (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['catch'] = catchRunner;
  testFns['boost'] = catchRunner;

  opts['skipCatch'] = false;
  opts['skipCache'] = true;
  opts['skipGeo'] = false;

  defaultFns.push('catch');

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
}

exports.setup = setup;
