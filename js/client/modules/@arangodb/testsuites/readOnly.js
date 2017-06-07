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
// / @author Manuel Baesler
// //////////////////////////////////////////////////////////////////////////////

const functionsDocumentation = {
  'readOnly': 'read only tests'
};
const optionsDocumentation = [
  '   - `skipReadOnly` : if set to true the read only tests are skipped',
];

const pu = require('@arangodb/process-utils');
const request = require('@arangodb/request');

// const BLUE = require('internal').COLORS.COLOR_BLUE;
const CYAN = require('internal').COLORS.COLOR_CYAN;
// const GREEN = require('internal').COLORS.COLOR_GREEN;
// const RED = require('internal').COLORS.COLOR_RED;
const RESET = require('internal').COLORS.COLOR_RESET;
// const YELLOW = require('internal').COLORS.COLOR_YELLOW;

// //////////////////////////////////////////////////////////////////////////////
// / @brief TEST: readOnly
// //////////////////////////////////////////////////////////////////////////////

function readOnly (options) {
  const results = { failed: 0 };

  if (options.skipReadOnly === true) {
    print('skipping readOnly tests!');
    return {
      failed: 0,
      readOnly: {
        failed: 0,
        status: true,
        skipped: true
      }
    };
  } // if

  if (options.cluster) {
    print('skipping readOnly tests on cluster!');
    return {
      failed: 0,
      readOnly: {
        failed: 0,
        status: true,
        skipped: true
      }
    };
  }

  const conf = {
        'server.authentication': true,
        'server.authentication-system-only': false,
      };

  print(CYAN + 'readOnly tests...' + RESET);


  const adbInstance = pu.startInstance('tcp', options, conf, 'readOnly');
  if (adbInstance === false) {
    results.failed += 1;
    results['test'] = {
      failed: 1,
      status: false,
      message: 'failed to start server!'
    };
  }

  pu.shutdownInstance(adbInstance, options);


  print(results);
  return results;
}

exports.setup = function (testFns, defaultFns, opts, fnDocs, optionsDoc) {
  testFns['readOnly'] = readOnly;

  for (var attrname in functionsDocumentation) { fnDocs[attrname] = functionsDocumentation[attrname]; }
  for (var i = 0; i < optionsDocumentation.length; i++) { optionsDoc.push(optionsDocumentation[i]); }
};
