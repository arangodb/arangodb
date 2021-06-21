/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertFalse, assertEqual, arango, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
const fs = require('fs');
const pu = require('@arangodb/testutils/process-utils');
const console = require('console');

// name of environment variable
const PATH = 'PROMTOOL_PATH';
  
// detect the path to promtool
let promtoolPath = internal.env[PATH];
if (!promtoolPath) {
  promtoolPath = '.';
}
promtoolPath = fs.join(promtoolPath, 'promtool' + pu.executableExt);

function promtoolSuite () {
  'use strict';
  
  return {
    testApiV2: function() {
      let toRemove = [];

      let res = arango.GET_RAW('/_admin/metrics/v2');
      assertEqual(200, res.code);
      let body = String(res.body);

      // store output of /_admin/metrics/v2 into a temp file
      let input = fs.getTempFile();
      try {
        fs.writeFileSync(input, body);
        toRemove.push(input);

        // this is where we will capture the output from promtool
        let output = fs.getTempFile();
        toRemove.push(output);

        // build command string. this will be unsafe if input/output
        // contain quotation marks and such. but as these parameters are
        // under our control this is very unlikely
        let command = promtoolPath + ' check metrics < "' + input + '" > "' + output + '" 2>&1';
        // pipe contents of temp file into promtool
        let actualRc = internal.executeExternalAndWait('sh', ['-c', command]);
        assertTrue(actualRc.hasOwnProperty('exit'));
        assertEqual(0, actualRc.exit);

        let promtoolResult = fs.readFileSync(output).toString();
        // no errors found means an empty result file
        assertEqual('', promtoolResult);
      } finally {
        // remove temp files
        toRemove.forEach((f) => {
          fs.remove(f);
        });
      }
    },
  };
}

if (internal.platform === 'linux') {
  // this test intentionally only executes on Linux, and only if PROMTOOL_PATH
  // is set to the path containing the `promtool` executable. if the PROMTOOL_PATH
  // is set, but the executable cannot be found, the test will error out.
  // the test also requires `sh` to be a shell that supports input/output redirection,
  // and `true` to be an executable that returns exit code 0 (we use sh -c true` as a
  // test to check the shell functionality).
  if (fs.exists(promtoolPath)) {
    let actualRc = internal.executeExternalAndWait('sh', ['-c', 'true']);
    if (actualRc.hasOwnProperty('exit') && actualRc.exit === 0) {
      jsunity.run(promtoolSuite);
    } else {
      console.warn('skipping test because no working sh can be found');
    }
  } else if (!internal.env.hasOwnProperty(PATH)) {
    console.warn('skipping test because promtool is not found. you can set ' + PATH + ' accordingly');
  } else {
    fail('promtool not found in PROMTOOL_PATH (' + internal.env[PATH] + ')');
  }
} else {
  console.warn('skipping test because we are not on Linux');
}

return jsunity.done();
