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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function analyzersRevisionTestSuite () {

  function checkAnalyzers(dbName, prefix, revisionNumber) {
    // valid name 0
    analyzers.save(prefix + "valid0", "identity");
    revisionNumber++;
    let revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
    assertTrue(revision.hasOwnProperty("revision"));
    assertTrue(revision.hasOwnProperty("buildingRevision"));
    assertTrue(revision.hasOwnProperty("coordinator"));
    assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
    assertEqual(revisionNumber, revision.revision);
    assertEqual(revisionNumber, revision.buildingRevision);

    // valid name 1
    analyzers.save(prefix + "valid1", "identity");
    revisionNumber++;
    revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
    assertTrue(revision.hasOwnProperty("revision"));
    assertTrue(revision.hasOwnProperty("buildingRevision"));
    assertTrue(revision.hasOwnProperty("coordinator"));
    assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
    assertEqual(revisionNumber, revision.revision);
    assertEqual(revisionNumber, revision.buildingRevision);

    // invalid name
    try { analyzers.save(prefix + "inv:alid", "identity"); } catch(e) {};
    revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
    assertTrue(revision.hasOwnProperty("revision"));
    assertTrue(revision.hasOwnProperty("buildingRevision"));
    assertTrue(revision.hasOwnProperty("coordinator"));
    assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
    assertEqual(revisionNumber, revision.revision);
    assertEqual(revisionNumber, revision.buildingRevision);

    // valid name 2
    analyzers.save(prefix + "valid2", "identity");
    revisionNumber++;
    revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
    assertTrue(revision.hasOwnProperty("revision"));
    assertTrue(revision.hasOwnProperty("buildingRevision"));
    assertTrue(revision.hasOwnProperty("coordinator"));
    assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
    assertEqual(revisionNumber, revision.revision);
    assertEqual(revisionNumber, revision.buildingRevision);

    // invalid repeated name 1
    try { analyzers.save(prefix + "valid1", "delimiter", { "delimiter": "," }); } catch(e) {};
    revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
    assertTrue(revision.hasOwnProperty("revision"));
    assertTrue(revision.hasOwnProperty("buildingRevision"));
    assertTrue(revision.hasOwnProperty("coordinator"));
    assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
    assertEqual(revisionNumber, revision.revision);
    assertEqual(revisionNumber, revision.buildingRevision);

    // remove valid name 0
    analyzers.remove(prefix + "valid0", true);
    revisionNumber++;
    revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
    assertTrue(revision.hasOwnProperty("revision"));
    assertTrue(revision.hasOwnProperty("buildingRevision"));
    assertTrue(revision.hasOwnProperty("coordinator"));
    assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
    assertEqual(revisionNumber, revision.revision);
    assertEqual(revisionNumber, revision.buildingRevision);

    // remove invalid repeated name 0
    try { analyzers.remove(prefix + "valid0", true); } catch(e) {};
    revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
    assertTrue(revision.hasOwnProperty("revision"));
    assertTrue(revision.hasOwnProperty("buildingRevision"));
    assertTrue(revision.hasOwnProperty("coordinator"));
    assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
    assertEqual(revisionNumber, revision.revision);
    assertEqual(revisionNumber, revision.buildingRevision);

    // remove valid name 2
    analyzers.remove(prefix + "valid2", true);
    revisionNumber++;
    revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
    assertTrue(revision.hasOwnProperty("revision"));
    assertTrue(revision.hasOwnProperty("buildingRevision"));
    assertTrue(revision.hasOwnProperty("coordinator"));
    assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
    assertEqual(revisionNumber, revision.revision);
    assertEqual(revisionNumber, revision.buildingRevision);

    // remove valid name 1
    analyzers.remove(prefix + "valid1", true);
    revisionNumber++;
    revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
    assertTrue(revision.hasOwnProperty("revision"));
    assertTrue(revision.hasOwnProperty("buildingRevision"));
    assertTrue(revision.hasOwnProperty("coordinator"));
    assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
    assertEqual(revisionNumber, revision.revision);
    assertEqual(revisionNumber, revision.buildingRevision);
  }

  return {
    setUpAll : function () {
    },

    tearDownAll : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief AnalyzersRevision tests
////////////////////////////////////////////////////////////////////////////////

    testAnalyzersPlanDbName: function() {
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

      checkAnalyzers(dbName, dbName + "::", revisionNumber);

      assertEqual(0, db._analyzers.count());

      db._useDatabase("_system");
      db._dropDatabase(dbName);

      let delRevision = {};
      try { delRevision = global.ArangoClusterInfo.getAnalyzersRevision(dbName); } catch(e) {
        assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
      assertEqual({}, delRevision);
    },
    testAnalyzersPlanWithoutDbName: function() {
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

      checkAnalyzers(dbName, "", revisionNumber);

      assertEqual(0, db._analyzers.count());

      db._useDatabase("_system");
      db._dropDatabase(dbName);

      let delRevision = {};
      try { delRevision = global.ArangoClusterInfo.getAnalyzersRevision(dbName); } catch(e) {
        assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
      assertEqual({}, delRevision);
    },
    testAnalyzersPlanSystem: function() {
      let dbName = "_system";
      db._useDatabase(dbName);

      let revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      assertEqual(revision.revision, revision.buildingRevision);
      let revisionNumber = revision.revision;

      checkAnalyzers(dbName, dbName + "::", revisionNumber);
    },
    testAnalyzersPlanSystemDefault: function() {
      let dbName = "_system";
      db._useDatabase(dbName);

      let revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      assertEqual(revision.revision, revision.buildingRevision);
      let revisionNumber = revision.revision;

      checkAnalyzers(dbName, "", revisionNumber);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(analyzersRevisionTestSuite);

return jsunity.done();
