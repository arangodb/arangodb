/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNull, getOptions, fail */
// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const errors = internal.errors;
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;
const assertQueryError = helper.assertQueryError;
const assertQueryWarningAndNull = helper.assertQueryWarningAndNull;

if (getOptions === true) {
  return {
    'javascript.startup-options-denylist': [],
    'javascript.startup-options-allowlist': [],
    'javascript.environment-variables-denylist': [],
    'javascript.environment-variables-allowlist': [],
    'javascript.endpoints-denylist': [],
    'javascript.endpoints-allowlist': [],
    'javascript.files-allowlist': [],
  };
}



function ahuacatlUDFSecurityDefaultsTestSuite () {
  const aqlfunctions = require("@arangodb/aql/functions");
  const funcNames = ["fileread", "filewrite", "envread", "enuminenv", "getoptions"];

  return {
    setUp : function () {
      funcNames.forEach(function (f) {
        try { aqlfunctions.unregister("UnitTests::security::" + f); } catch (e) {}
      });

      // Try to read a file - should be forbidden with default settings
      aqlfunctions.register("UnitTests::security::fileread", function () {
        var fs = require('fs');
        return fs.read('/etc/passwd');
      });

      // Try to write a file - should be forbidden with default settings
      aqlfunctions.register("UnitTests::security::filewrite", function () {
        var fs = require('fs');
        fs.write('/root/arangodb-test-udf-security.txt', 'test');
        return true;
      });

      // Try to read an env variable - should return undefined/null with default settings
      aqlfunctions.register("UnitTests::security::envread", function () {
        return process.env.PATH;
      });

      // Try to enumerate env variables - should return empty array with default settings
      aqlfunctions.register("UnitTests::security::enuminenv", function () {
        return Object.keys(process.env);
      });

      // Try to read startup options - should return empty object with default settings
      aqlfunctions.register("UnitTests::security::getoptions", function () {
        var internal = require('internal');
        return internal.options();
      });
    },

    tearDown : function () {
      funcNames.forEach(function (f) {
        try { aqlfunctions.unregister("UnitTests::security::" + f); } catch (e) {}
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that file read is blocked by default security settings
////////////////////////////////////////////////////////////////////////////////

    testFileReadBlocked : function () {
      try {
        let x = getQueryResults("RETURN UnitTests::security::fileread()");
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that file write is blocked by default security settings
////////////////////////////////////////////////////////////////////////////////

    testFileWriteBlocked : function () {
      try {
        let x = getQueryResults("RETURN UnitTests::security::filewrite()");
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_QUERY_FUNCTION_RUNTIME_ERROR.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that env variables are not exposed with default security settings
////////////////////////////////////////////////////////////////////////////////

    testEnvVarNotExposed : function () {
      // With default settings (empty allowlist), env vars are not exposed
      let result = getQueryResults("RETURN UnitTests::security::envread()");
      assertNull(result[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that env variable enumeration returns empty with default security settings
////////////////////////////////////////////////////////////////////////////////

    testEnvVarsEnumerationEmpty : function () {
      // With default settings, no env vars should be enumerable
      let result = getQueryResults("RETURN UnitTests::security::enuminenv()");
      assertEqual([], result[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test that startup options are not exposed with default security settings
////////////////////////////////////////////////////////////////////////////////

    testStartupOptionsEmpty : function () {
      // With default settings (empty allowlist), no startup options should be exposed
      let result = getQueryResults("RETURN UnitTests::security::getoptions()");
      assertEqual({}, result[0]);
    }

  };
}

// TODO: these are the tests that should test the *default* settings
// of arangod; to not break any other tests we currently set the
// allowed paths/environmentvariabls/startupoptions/filepaths to .*
// for testing.
//
// jsunity.run(ahuacatlUDFSecurityDefaultsTestSuite);

return jsunity.done();
