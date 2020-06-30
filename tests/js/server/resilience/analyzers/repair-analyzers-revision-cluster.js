/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var analyzers = require("@arangodb/analyzers");
const expect = require('chai').expect;
const wait = require('internal').wait;
var internal = require('internal');
////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function repairAnalyzersRevisionTestSuite () {

  function waitForRevision(dbName, revisionNumber, buildingRevision) {
    let tries = 0;
    while(true) {
      global.ArangoClusterInfo.flush();
      let revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      if ((revision.revision === revisionNumber && 
             revision.buildingRevision === buildingRevision) || 
          tries >= 2) {
        assertEqual(revisionNumber, revision.revision);
        assertEqual(buildingRevision, revision.buildingRevision);
        return revision;
      }
      tries++;
      internal.sleep(1);
    }
    return undefined;
  }
  
  function waitForCompletedRevision(dbName, revisionNumber) {
    return waitForRevision(dbName, revisionNumber, revisionNumber);
  }

  function waitForAllAgencyJobs() {
    const prefix = global.ArangoAgency.prefix();
    const paths = [
      "Target/ToDo/",
      "Target/Pending/",
    ].map(p => `${prefix}/${p}`);

    const waitInterval = 1.0;
    const maxWaitTime = 300;

    let unfinishedJobs = Infinity;
    let timeout = false;
    const start = Date.now();

    while (unfinishedJobs > 0 && ! timeout) {
      const duration = (Date.now() - start) / 1000;
      const result = global.ArangoAgency.read([paths]);
      const target = result[0][prefix]["Target"];

      timeout = duration > maxWaitTime;

      unfinishedJobs = target["ToDo"].length + target["Pending"].length;

      wait(waitInterval);
    }

    if (timeout) {
      const duration = (Date.now() - start) / 1000;
      console.error(`Timeout after waiting for ${duration}s on all agency jobs. `
        + `${unfinishedJobs} jobs aren't finished.`);
    }

    return unfinishedJobs === 0;
  };

  return {
    setUpAll : function () {
    },

    tearDownAll : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief RepairAnalyzersRevision tests
////////////////////////////////////////////////////////////////////////////////
    testRepairPlan: function() {
      const n = 2;
      const dbName = "testDbName";
      let revisionNumber = 0;
      let coordinator = "";
      let rebootId = 1;
      // Create databases
      for (let i = 0; i < n; i++) {
        db._useDatabase("_system");
        try { db._dropDatabase(dbName + i); } catch (e) {}

        db._createDatabase(dbName + i);
        db._useDatabase(dbName + i);
        revisionNumber = 0;
        let revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName + i);
        assertTrue(revision.hasOwnProperty("revision"));
        assertTrue(revision.hasOwnProperty("buildingRevision"));
        assertFalse(revision.hasOwnProperty("coordinator"));
        assertFalse(revision.hasOwnProperty("coordinatorRebootId"));
        assertEqual(revisionNumber, revision.revision);
        assertEqual(revisionNumber, revision.buildingRevision);

        analyzers.save("valid", "identity");
        revisionNumber++;
        revision = waitForCompletedRevision(dbName + i, revisionNumber);
        coordinator = revision.coordinator;
        rebootId = revision.coordinatorRebootId;
      }
      // Break analyzers revision
      const value = {revision: revisionNumber,
          buildingRevision: revisionNumber + 1,
          coordinator: coordinator,
          coordinatorRebootId: rebootId + 1};
      for (let i = 0; i < n; i++) {
        global.ArangoAgency.set("Plan/Analyzers/" + dbName + i, value);
      }
      global.ArangoAgency.increaseVersion("Plan/Version");

      // Repair analyzers revision
      expect(waitForAllAgencyJobs(), 'Timeout while waiting for agency jobs to finish');

      // Check repair
      for (let i = 0; i < n; i++) {
        waitForCompletedRevision(dbName + i, revisionNumber);
        db._useDatabase(dbName + i);
        assertFalse(null === analyzers.analyzer("valid"));
        analyzers.remove("valid", true);
        waitForCompletedRevision(dbName + i, revisionNumber + 1);
      }

      // Break but in other direction
      revisionNumber = revisionNumber + 2; 
      for (let i = 0; i < n; i++) {
        db._useDatabase(dbName + i);
        analyzers.save("valid", "identity");
        let currentRevision = waitForCompletedRevision(dbName + i, revisionNumber);
        coordinator = currentRevision.coordinator;
        rebootId = currentRevision.coordinatorRebootId;
      }
      rebootId++;
      const value2 = {revision: revisionNumber - 1, // revision is going back -> hotbackup previous dump restore case
          buildingRevision: revisionNumber,
          coordinator: coordinator,
          coordinatorRebootId: rebootId + 1};
      for (let i = 0; i < n; i++) {
        global.ArangoAgency.set("Plan/Analyzers/" + dbName + i, value2);
      }
      global.ArangoAgency.increaseVersion("Plan/Version");

      // Repair analyzers revision
      expect(waitForAllAgencyJobs(), 'Timeout while waiting for agency jobs to finish');
      // Check repair
      for (let i = 0; i < n; i++) {
        waitForCompletedRevision(dbName + i, revisionNumber - 1);
        db._useDatabase(dbName + i);
        // revision is rollbacked. This analyzer is no more present
        assertTrue(null === analyzers.analyzer("valid"));
        // and we could create new one with same name
        analyzers.save("valid", "ngram", {min:2, max:3, preserveOriginal:true});
        analyzers.toArray();
        let ngram = analyzers.analyzer("valid");
        assertEqual("ngram", ngram.type());
      }
      // Drop databases
      db._useDatabase("_system");
      for (let i = 0; i < n; i++) {
        db._dropDatabase(dbName + i);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(repairAnalyzersRevisionTestSuite);

return jsunity.done();
