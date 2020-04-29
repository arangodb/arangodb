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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function repairAnalyzersRevisionTestSuite () {

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

    while(unfinishedJobs > 0 && ! timeout) {
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
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      let revisionNumber = 0;

      let revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      assertFalse(revision.hasOwnProperty("coordinator"));
      assertFalse(revision.hasOwnProperty("coordinatorRebootId"));
      assertEqual(revisionNumber, revision.revision);
      assertEqual(revisionNumber, revision.buildingRevision);

      analyzers.save("valid", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      assertTrue(revision.hasOwnProperty("coordinator"));
      assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
      assertEqual(revisionNumber, revision.revision);
      assertEqual(revisionNumber, revision.buildingRevision);

      let value = {"revision": revisionNumber - 1,
          "buildingRevision": revisionNumber,
          "coordinator": revision.coordinator,
          "coordinatorRebootId": revision.coordinatorRebootId + 1};

      // Break analyzers revision
      global.ArangoAgency.set(`Plan/Analyzers/${dbName}`, value);
      global.ArangoAgency.increaseVersion("Plan/Version");

      // Repair analyzers revision
      expect(waitForAllAgencyJobs(), 'Timeout while waiting for agency jobs to finish');

      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      assertFalse(revision.hasOwnProperty("coordinator"));
      assertFalse(revision.hasOwnProperty("coordinatorRebootId"));
      assertEqual(revisionNumber - 1, revision.revision);
      assertEqual(revisionNumber - 1, revision.buildingRevision);

      analyzers.remove("valid", true);
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      assertTrue(revision.hasOwnProperty("coordinator"));
      assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
      assertEqual(revisionNumber, revision.revision);
      assertEqual(revisionNumber, revision.buildingRevision);

      db._useDatabase("_system");
      db._dropDatabase(dbName);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(repairAnalyzersRevisionTestSuite);

return jsunity.done();
