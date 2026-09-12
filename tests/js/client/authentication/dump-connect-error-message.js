/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {assertEqual, assertTrue} = jsunity.jsUnity.assertions;
const fs = require("fs");
const pu = require("@arangodb/testutils/process-utils");
const {executeExternalAndWaitWithSanitizer} = require("@arangodb/test-helper");
const IM = global.instanceManager;

/**
 * Runs arangodump with the given connection arguments.
 *
 * Returns the exit code together with everything arangodump logged, so that
 * tests can assert on the error message a user would see.
 */
const runDump = (connectionArgs) => {
  const outputDirectory = fs.getTempFile();
  const logFile = fs.getTempFile();
  try {
    const rc = executeExternalAndWaitWithSanitizer(pu.ARANGODUMP_BIN, [
      "--server.endpoint", IM.endpoint,
      "--output-directory", outputDirectory,
      "--log.output", "file://" + logFile,
      "--log.foreground-tty", "false",
      ...connectionArgs,
    ], "dump-connect-error-message");
    const log = fs.isFile(logFile) ? fs.readFileSync(logFile).toString() : "";
    return {exit: rc.exit, log};
  } finally {
    fs.removeDirectoryRecursive(outputDirectory, true);
    if (fs.isFile(logFile)) {
      fs.remove(logFile);
    }
  }
};

/**
 * Checks that arangodump surfaces the server's HTTP error when its first
 * request to the server (GET /_api/version, which arangodump uses to verify
 * the connection) is rejected, instead of only reporting a generic connection
 * failure.
 */
function DumpConnectErrorMessageSuite() {
  'use strict';

  return {
    testWrongPasswordShowsServerError: function () {
      const result = runDump([
        "--server.database", "_system",
        "--server.username", "root",
        "--server.password", "wrong-password",
      ]);
      assertEqual(1, result.exit, result.log);
      assertTrue(result.log.includes("Could not connect to endpoint"), result.log);
      assertTrue(result.log.includes("HTTP 401"), result.log);
      assertTrue(result.log.includes("User not authenticated"), result.log);
    },

    testUnknownDatabaseShowsServerError: function () {
      const result = runDump([
        "--server.database", "UnitTestsDatabaseDoesNotExist",
        "--server.username", "root",
        "--server.password", "",
      ]);
      assertEqual(1, result.exit, result.log);
      assertTrue(result.log.includes("Could not connect to endpoint"), result.log);
      assertTrue(result.log.includes("HTTP 404"), result.log);
      assertTrue(result.log.includes("database not found"), result.log);
    },
  };
}

jsunity.run(DumpConnectErrorMessageSuite);
return jsunity.done();
