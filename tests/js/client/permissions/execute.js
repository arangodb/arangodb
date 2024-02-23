/*jshint globalstrict:false, strict:false */
/* global fail, getOptions, assertEqual */

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
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'javascript.allow-external-process-control': false,
  };
}

var jsunity = require('jsunity');

function testSuite() {
  let env = require('process').env;
  const internal = require('internal');
  const executeExternal = internal.executeExternal;
  const executeExternalAndWait = internal.executeExternalAndWait;
  const killExternal = internal.killExternal;
  const statusExternal = internal.statusExternal;
  const getExternalSpawned = internal.getExternalSpawned;
  const suspendExternal = internal.suspendExternal;
  const continueExternal = internal.continueExternal;
  const arangodb = require("@arangodb");

  let command = "/bin/true";

  return {
    testExternalProcesses : function() {
      try {
        let rv = executeExternal(command);
        killExternal(rv.pid);
        fail();
      } catch (err) {
        assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum);
      }
      try {
        let rv = executeExternalAndWait(command);
        fail();
      } catch (err) {
        assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum);
      }
      try {
        let rv = statusExternal(env['PID']);
        fail();
      } catch (err) {
        assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum);
      }
      try {
        let rv = killExternal(env['PID']);
        fail();
      } catch (err) {
        assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum);
      }
      try {
        let rv = getExternalSpawned();
        fail();
      } catch (err) {
        assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum);
      }
      try {
        let rv = suspendExternal(env['PID']);
        fail();
      } catch (err) {
        assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum);
      }
      try {
        let rv = continueExternal(env['PID']);
        fail();
      } catch (err) {
        assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum);
      }
    }
  };
}

jsunity.run(testSuite);
return jsunity.done();
