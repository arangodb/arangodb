/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup, assertEqual, assertTrue, assertNotEqual, arango */

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
// / @author Jure Bajic
// / @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// Test that crash dumps are created when the server crashes and can be 
// retrieved via the crash dump API.

const internal = require('internal');
const jsunity = require('jsunity');
const IM = global.instanceManager;
const crashesEndpoint = '/_admin/crashes/';

if (runSetup === true) {
  'use strict';
  
  // Make some API calls before crash to ensure there's data to record
  arango.GET('/_api/version');
  arango.GET('/_api/database/current');
  
  // Verify no crashes exist before we crash
  let response = arango.GET(crashesEndpoint);
  let crashes = response.result || response;
  if (!response.error && Array.isArray(crashes) && crashes.length !== 0) {
    throw new Error('Expected no crashes before setup, found: ' + JSON.stringify(crashes));
  }
  
  // Produce a crash to trigger the crash handler
  IM.debugTerminate("CRASH-HANDLER-TEST-TERMINATE");
  
  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testCrashDumpCreatedAndAccessible: function () {
      // Check for crash dumps via API
      let response = arango.GET(crashesEndpoint);
      assertTrue(!response.error, 'Should not return error: ' + JSON.stringify(response));
      assertEqual(200, response.code, 'Should return 200');

      let crashes = response.result || response;
      assertTrue(Array.isArray(crashes), 'crashes should be an array');

      // We should have exactly one crash now
      assertEqual(1, crashes.length,
                 'Expected exactly one crash dump, found: ' + crashes.length);

      // Get the crash
      let crashId = crashes[0];
      assertNotEqual(crashId, undefined, 'Crash ID should not be undefined');
      
      // Fetch crash contents
      let contentsResponse = arango.GET(crashesEndpoint + encodeURIComponent(crashId));
      assertTrue(!contentsResponse.error, `Should get crash contents: ${JSON.stringify(contentsResponse)}`);
      assertEqual(200, contentsResponse.code, 'Should return 200 for crash contents');

      let contents = contentsResponse.result || contentsResponse;
      assertTrue(contents.hasOwnProperty('crashId'), 'Should have crashId');
      assertEqual(contents.crashId, crashId, 'crashId should match');
      assertTrue(contents.hasOwnProperty('files'), 'Should have files');

      // Assert specific expected files are present
      assertTrue(contents.files.hasOwnProperty('ApiRecording.json'),
                 'Crash dump should contain ApiRecording.json');
      assertTrue(contents.files.hasOwnProperty('AsyncRegistry.json'),
                 'Crash dump should contain AsyncRegistry.json');
      assertTrue(contents.files.hasOwnProperty('backtrace.txt'),
                 'Crash dump should contain backtrace.txt');
      assertTrue(contents.files.hasOwnProperty('system_info.txt'),
                 'Crash dump should contain system_info.txt');

      // Clean up - delete the crash
      let deleteResponse = arango.DELETE(crashesEndpoint + encodeURIComponent(crashId));
      assertTrue(!deleteResponse.error, 'Should delete crash');
      
      let deleteResult = deleteResponse.result || deleteResponse;
      assertTrue(deleteResult.deleted, 'Crash should be deleted');

      // Verify crash is gone
      let verifyResponse = arango.GET(crashesEndpoint);
      let verifyCrashes = verifyResponse.result || verifyResponse;
      assertEqual(0, verifyCrashes.length, 'Crash should be deleted');
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
