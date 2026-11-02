// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

const jsunity = require("jsunity");
const { assertTrue, assertFalse, assertEqual } = jsunity.jsUnity.assertions;
const pu = require('@arangodb/testutils/process-utils');
const fs = require("fs");
const { executeExternalAndWaitWithSanitizer } = require('@arangodb/test-helper');

function AutoUpgradeFullCompactionSuite () {
  'use strict';

  return {
    testAutoUpgradeWithFullCompactionSucceeds: function () {
      /*
       * Regression test: `--database.auto-upgrade true` combined with
       * `--database.auto-upgrade-full-compaction true` used to fail
       * deterministically with exit code 30 (TRI_EXIT_FULL_COMPACTION_FAILED)
       * and "Manual compaction paused", even on an empty database directory.
       * The ShutdownFeature was registered in upgrade mode and its start()
       * put the server into shutdown state before the storage engine and the
       * UpgradeFeature even started, so the full compaction saw the cancel
       * flag immediately.
       *
       * As we need the exit code, we start a single server as an external
       * process on a fresh database directory and wait for it to finish the
       * upgrade. The log is written to a file so the result can be checked.
       */
      const arangod = pu.ARANGOD_BIN;
      assertTrue(fs.isFile(arangod), "arangod not found!");

      const dataDir = fs.getTempFile();
      const logFile = fs.getTempFile();
      const args = [
        '-c', 'none',
        '--database.directory', dataDir,
        '--database.auto-upgrade', 'true',
        '--database.auto-upgrade-full-compaction', 'true',
        '--javascript.enabled', 'false',
        '--server.rest-server', 'false',
        '--log.output', 'file://' + logFile,
        '--log.foreground-tty', 'false',
        '--log.ids', 'true'
      ];

      try {
        const rc = executeExternalAndWaitWithSanitizer(arangod, args, 'shell-auto-upgrade-full-compaction-noncluster');
        const log = fs.isFile(logFile) ? fs.readFileSync(logFile).toString() : '';
        const details = `arangod result: ${JSON.stringify(rc)}\narangod log:\n${log}`;

        assertTrue(rc.hasOwnProperty("exit"), details);
        assertEqual(0, rc.exit, details);
        // e8f47: "full RocksDB compaction after upgrade completed successfully"
        assertTrue(log.includes('[e8f47]'), details);
        // 65594: "compaction of entire RocksDB database key range finished"
        assertTrue(log.includes('[65594]'), details);
        // e8f46: "full RocksDB compaction after upgrade failed"
        assertFalse(log.includes('[e8f46]'), details);
      } finally {
        if (fs.isFile(logFile)) {
          fs.remove(logFile);
        }
        if (fs.isDirectory(dataDir)) {
          fs.removeDirectoryRecursive(dataDir, true);
        }
      }
    }
  };
}

jsunity.run(AutoUpgradeFullCompactionSuite);

return jsunity.done();
