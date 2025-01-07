/*jshint globalstrict:false, strict:false */
/*global arango, assertTrue, assertEqual, assertNotEqual, assertUndefined, assertNotUndefined, db*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test hotbackup in single server
///
/// Copyright 2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const serverHelper = require('@arangodb/test-helper');
const {readAgencyValueAt} = require("@arangodb/testutils/replicated-logs-helper");
const {errors} = require('internal');

'use strict';

function HotBackupSuite () {

  return {
    testRegressionCanRestoreBackupWithMissingAnalyzerEntry: function() {
      const dbName = "otherDB";
      const colName = "otherCollection";
      const colNameTwo = "additionalCollection";
      const dropExtraCollections = () => {
        try {
          db._dropDatabase(dbName);
        } catch (e) {}
        db._drop(colName);
      };

      const assertAdditionalDataDoesNotExist = () => {
        assertEqual(db._databases().indexOf(dbName), -1, "Setup issue: Database was not dropped before restore");
        assertEqual(db._collections().map(c => c.name()).indexOf(colName), -1, "Setup issue: Collection was not dropped before restore");
        assertNotEqual(db._collections().map(c => c.name()).indexOf(colNameTwo), -1, "Collection was not created between backup and restore");
      };
      const assertAdditionalDataExists = () => {
        assertNotEqual(db._databases().indexOf(dbName), -1, "Database was not restored");
        assertNotEqual(db._collections().map(c => c.name()).indexOf(colName), -1, "Collection was not restored");
        assertEqual(db._collections().map(c => c.name()).indexOf(colNameTwo), -1, "After Backup Collection was not erased");
      };
      try {
        db._createDatabase(dbName);
        db._create(colName);

        const req = arango.POST("/_admin/backup/create", {});
        if (!require("internal").isEnterprise()) {
          // Hotbackup is an enterprise feature this call should respond with 404.
          assertTrue(req.error);
          assertEqual(req.code, 404);
          return;
        }
        // Entering Enterprise test code
        const backupGood = req.result;

        // Now create the regression. In version 3.6 the entry /arango/Plan/Analyzers did not exist.


        assertNotUndefined(readAgencyValueAt("Plan/Analyzers"), "We should have analyzers in plan by default.");
        serverHelper.agency.remove("Plan/Analyzers");
        assertUndefined(readAgencyValueAt("Plan/Analyzers"), "Failed to simulate 3.6 behaviour by removing the analyzers entry");

        const backupRegressed = arango.POST("/_admin/backup/create", {}).result;

        // Drop the Collections, so we see if they get restored from Backup.
        dropExtraCollections();
        db._create(colNameTwo);
        assertAdditionalDataDoesNotExist();

        // Validate we can restore the backup with Analyzers entry, just for good measure.
        arango.POST("/_admin/backup/restore", {id: backupGood.id});
        assertAdditionalDataExists();

        dropExtraCollections();
        db._create(colNameTwo);
        assertAdditionalDataDoesNotExist();

        arango.POST("/_admin/backup/restore", {id: backupRegressed.id});
        assertAdditionalDataExists();

        // For good measure make sure we can use the Cluster after restore
        const c2 = db._create(colNameTwo);
        assertEqual(c2.name(), colNameTwo);
      } finally {
        // Cleanup if anything goes wrong
        dropExtraCollections();
        db._drop(colNameTwo);
      }
    },

    testRegressionFailedReplanIsReported: function() {
      if (global.instanceManager.debugCanUseFailAt()) {
        const req = arango.POST("/_admin/backup/create", {});
        if (!require("internal").isEnterprise()) {
          // Hotbackup is an enterprise feature this call should respond with 404.
          assertTrue(req.error);
          assertEqual(req.code, 404);
          return;
        }
        const backup = req.result;
        const colName = "additionalCollection";
        db._create(colName);
        try {
          global.instanceManager.debugSetFailAt("ClusterInfo::failReplanAgency");
          const response = arango.POST("/_admin/backup/restore", {id: backup.id});
          assertEqual(response.errorNum, errors.ERROR_DEBUG.code);
          assertNotEqual(db._collections().map(c => c.name()).indexOf(colName), -1, "Collection was removed on failed restore");
        } finally {
          global.instanceManager.debugClearFailAt();
          db._drop(colName);
        }

      }

    }
  };
}

jsunity.run(HotBackupSuite);
return jsunity.done();
