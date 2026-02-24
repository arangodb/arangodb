/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup, assertEqual, assertTrue, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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

// Test that crash dumps are created when a dbserver crashes and can be 
// retrieved via the crash dump API.

const jsunity = require('jsunity');
const request = require('@arangodb/request');
const IM = global.instanceManager;
const crashesEndpoint = '/_admin/crashes';

if (runSetup === true) {
  'use strict';
  
  // Find the first dbserver
  const dbserver = IM.arangods.find(s => s.isRole('dbserver'));
  if (dbserver === undefined) {
    throw new Error('Could not find dbserver');
  }
  const dbserverUrl = dbserver.url;
  
  // Make some API calls to the dbserver before crash
  request({ url: dbserverUrl + '/_api/version', method: 'get', timeout: 10 });
  
  // Verify no crashes exist before we crash on dbserver
  const response = request({ url: dbserverUrl + crashesEndpoint, method: 'get', timeout: 10 });
  const crashes = response.json.result || response.json;
  if (response.status === 200 && Array.isArray(crashes) && crashes.length !== 0) {
    throw new Error('Expected no crashes before setup on dbserver');
  }
  
  // Crash only the dbserver
  dbserver.debugTerminate('CRASH-HANDLER-TEST-SEGFAULT');
  
  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testCrashDumpDBServerCreatedAndAccessible: function () {
      // Find the dbserver again after restart
      const dbserver = IM.arangods.find(s => s.isRole('dbserver'));
      assertTrue(dbserver !== undefined, 'Could not find dbserver after restart');
      const dbserverUrl = dbserver.url;
      
      // Check for crash dumps on the dbserver
      const response = request({ url: dbserverUrl + crashesEndpoint, method: 'get', timeout: 10 });
      assertEqual(200, response.status, 'Should return 200');

      const crashes = response.json.result || response.json;
      assertTrue(Array.isArray(crashes), 'crashes should be an array');

      // We should have exactly one crash now
      assertEqual(1, crashes.length,
                 'Expected exactly one crash dump on dbserver, found: ' + crashes.length);

      // Get the crash
      const crashId = crashes[0];
      assertNotEqual(crashId, undefined, 'Crash ID should not be undefined');
      
      // Fetch crash contents
      const contentsResponse = request({ 
        url: dbserverUrl + crashesEndpoint + '/' + encodeURIComponent(crashId), 
        method: 'get', 
        timeout: 10 
      });
      assertEqual(200, contentsResponse.status, 'Should return 200 for crash contents');

      const contents = contentsResponse.json.result || contentsResponse.json;
      assertTrue(contents.hasOwnProperty('crashId'), 'Should have crashId');
      assertEqual(contents.crashId, crashId, 'crashId should match');
      assertTrue(contents.hasOwnProperty('files'), 'Should have files');

      // Assert specific expected files are present
      assertTrue(contents.files.hasOwnProperty('ApiRecording.json'),
                 'Crash dump should contain ApiRecording.json');
      assertTrue(contents.files.hasOwnProperty('AsyncRegistry.json'),
                 'Crash dump should contain AsyncRegistry.json');
      assertTrue(contents.files.hasOwnProperty('Activities.json'),
                 'Crash dump should contain Activities.json');
      assertTrue(contents.files.hasOwnProperty('backtrace.txt'),
                 'Crash dump should contain backtrace.txt');
      assertTrue(contents.files.hasOwnProperty('system_info.txt'),
                 'Crash dump should contain system_info.txt');

      // Clean up - delete the crash
      const deleteResponse = request({ 
        url: dbserverUrl + crashesEndpoint + '/' + encodeURIComponent(crashId), 
        method: 'delete', 
        timeout: 10 
      });
      assertEqual(200, deleteResponse.status, 'Should delete crash');

      // Verify crash is gone
      const verifyResponse = request({ url: dbserverUrl + crashesEndpoint, method: 'get', timeout: 10 });
      const verifyCrashes = verifyResponse.json.result || verifyResponse.json;
      assertEqual(0, verifyCrashes.length, 'Crash should be deleted');
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
