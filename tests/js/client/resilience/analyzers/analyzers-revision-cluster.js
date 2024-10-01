/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, fail, assertMatch */

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
/// @author Yuriy Popov
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var analyzers = require("@arangodb/analyzers");
var internal = require('internal');
var ERRORS = require("@arangodb").errors;
const {
  arangoClusterInfoFlush,
  arangoClusterInfoGetAnalyzersRevision,
  arangoClusterInfoWaitForPlanVersion,
} = require("@arangodb/test-helper");
let IM = global.instanceManager;
////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function analyzersRevisionTestSuite () {

  function waitForRevision(dbName, revisionNumber, buildingRevision) {
  	let tries = 0;
    while(tries < 2) {
      arangoClusterInfoFlush();
      let revision = arangoClusterInfoGetAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      assertTrue(revision.hasOwnProperty("coordinator"));
      assertTrue(revision.hasOwnProperty("coordinatorRebootId"));
      if ((revision.revision === revisionNumber && 
      	     revision.buildingRevision === buildingRevision) || 
      	  tries >= 2) {
        assertEqual(revisionNumber, revision.revision);
        assertEqual(buildingRevision, revision.buildingRevision);
        break;
      }
      tries++;
      internal.sleep(1);
    }
  }
  
  function waitForCompletedRevision(dbName, revisionNumber) {
  	waitForRevision(dbName, revisionNumber, revisionNumber);
  }

  function checkAnalyzers(dbName, prefix, revisionNumber) {
    // valid name 0
    analyzers.save(prefix + "valid0", "identity");
    revisionNumber++;
    waitForCompletedRevision(dbName, revisionNumber);
    // valid name 1
    analyzers.save(prefix + "valid1", "identity");
    revisionNumber++;
    waitForCompletedRevision(dbName, revisionNumber);

    // invalid name
    try { analyzers.save(prefix + "inv:alid", "identity"); } catch(e) {};
    waitForCompletedRevision(dbName, revisionNumber);

    // valid name 2
    analyzers.save(prefix + "valid2", "identity");
    revisionNumber++;
    waitForCompletedRevision(dbName, revisionNumber);

    // invalid repeated name 1
    try { 
      analyzers.save(prefix + "valid1", "delimiter", { "delimiter": "," });
      fail(); 
    } catch(e) {};
    waitForCompletedRevision(dbName, revisionNumber);

    // remove valid name 0
    analyzers.remove(prefix + "valid0", true);
    revisionNumber++;
    waitForCompletedRevision(dbName, revisionNumber);

    // remove invalid repeated name 0
    try { 
      analyzers.remove(prefix + "valid0", true);
      fail(); 
    } catch(e) {};
    waitForCompletedRevision(dbName, revisionNumber);

    // remove valid name 2
    analyzers.remove(prefix + "valid2", true);
    revisionNumber++;
    waitForCompletedRevision(dbName, revisionNumber);

    // remove valid name 1
    analyzers.remove(prefix + "valid1", true);
    revisionNumber++;
    waitForCompletedRevision(dbName, revisionNumber);
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

      let revision = arangoClusterInfoGetAnalyzersRevision(dbName);
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

      let delRevision = "";
      try { delRevision = arangoClusterInfoGetAnalyzersRevision(dbName); } catch(e) {
        assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
      assertMatch(/ArangoError: <databaseName> is invalid/, delRevision);
    },
    testAnalyzersPlanWithoutDbName: function() {
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      let revisionNumber = 0;

      let revision = arangoClusterInfoGetAnalyzersRevision(dbName);
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

      let delRevision = "";
      try { delRevision = arangoClusterInfoGetAnalyzersRevision(dbName); } catch(e) {
        assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
      assertMatch(/ArangoError: <databaseName> is invalid/, delRevision);
    },
    testAnalyzersPlanSystem: function() {
      let dbName = "_system";
      db._useDatabase(dbName);

      let revision = arangoClusterInfoGetAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      assertEqual(revision.revision, revision.buildingRevision);
      let revisionNumber = revision.revision;

      checkAnalyzers(dbName, dbName + "::", revisionNumber);
    },
    testAnalyzersPlanSystemDefault: function() {
      let dbName = "_system";
      db._useDatabase(dbName);

      let revision = arangoClusterInfoGetAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("revision"));
      assertTrue(revision.hasOwnProperty("buildingRevision"));
      assertEqual(revision.revision, revision.buildingRevision);
      let revisionNumber = revision.revision;

      checkAnalyzers(dbName, "", revisionNumber);
    },
    testAnalyzersPlanBuiltIn: function() {
      try { analyzers.save("text_en", "text", "{ \"locale\": \"en.UTF-8\", \"stopwords\": [ ] }", [ "frequency", "norm", "position" ]); } catch(e) {
        assertTrue(false);
      }
    },
    testAnalyzersCleanupAfterFailedInsert: function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        IM.debugSetFailAt('FailStoreAnalyzer');
        try {
          analyzers.save("FailedToStore", "identity");
          fail();
        } catch(e) {
          assertEqual(ERRORS.ERROR_DEBUG.code, e.errorNum);
        }
        assertTrue(null === analyzers.analyzer(dbName + "::FailedToStore"));
        waitForCompletedRevision(dbName, 0);
      } finally {
        db._useDatabase("_system");
        try { db._dropDatabase(dbName); } catch (e) {}
        IM.debugRemoveFailAt('FailStoreAnalyzer');
      }
    },
    testAnalyzersCleanupAfterFailedInsertCommit: function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        IM.debugSetFailAt('FinishModifyingAnalyzerCoordinator'); 
        try {
          analyzers.save("FailedToStore", "identity");
          fail();
        } catch(e) {
          assertEqual(ERRORS.ERROR_DEBUG.code, e.errorNum);
        }
        assertTrue(null === analyzers.analyzer(dbName + "::FailedToStore"));
        // commit and abort attempt will fail
        // observe unstable state of Agency
        waitForRevision(dbName, 0, 1);
        // now let`s allow scheduled cleanup to do the job
        IM.debugRemoveFailAt('FinishModifyingAnalyzerCoordinator');
        let tries = 0;
        while (tries < 5) {
          tries++;
          arangoClusterInfoFlush();
          let recovered_revision = arangoClusterInfoGetAnalyzersRevision(dbName);
          if (recovered_revision.buildingRevision === 0) {
            break;
          }
          internal.sleep(5);
        }
        assertTrue(tries < 5);
        // same name should be fine - if cleanup is ok
        analyzers.save("FailedToStore", "identity");
        waitForCompletedRevision(dbName, 1);
        let analyzer = analyzers.analyzer(dbName + "::FailedToStore");
        assertFalse(null === analyzer);
        assertEqual('identity', analyzer.type());
      } finally {
        db._useDatabase("_system");
        try { db._dropDatabase(dbName); } catch (e) {}
        IM.debugRemoveFailAt('FinishModifyingAnalyzerCoordinator');
      }
    },
    testAnalyzersCleanupAfterFailedRemoveUpdate: function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        analyzers.save("TestAnalyzer", "identity");
        IM.debugSetFailAt('UpdateAnalyzerForRemove');
        try {
          analyzers.remove("TestAnalyzer", true);
          fail();
        } catch(e) {
          assertEqual(ERRORS.ERROR_DEBUG.code, e.errorNum);
        }
        analyzers.save("TestAnalyzer2", "identity"); // to trigger cleanup
        assertFalse(null === analyzers.analyzer(dbName + "::TestAnalyzer")); // check analyzer still here
      } finally {
        db._useDatabase("_system");
        try { db._dropDatabase(dbName); } catch (e) {}
        IM.debugRemoveFailAt('UpdateAnalyzerForRemove');
      }
    },
    testAnalyzersCleanupAfterFailedRemoveCommit: function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        analyzers.save("TestAnalyzer", "identity");
        IM.debugSetFailAt('FinishModifyingAnalyzerCoordinator');
        try {
          analyzers.remove("TestAnalyzer", true);
          fail();
        } catch(e) {
          assertEqual(ERRORS.ERROR_DEBUG.code, e.errorNum);
        }
        IM.debugRemoveFailAt('FinishModifyingAnalyzerCoordinator');
        // wait for repair procedures
        let tries = 0;
        while (tries < 5) {
          tries++;
          arangoClusterInfoFlush();
          let recovered_revision = arangoClusterInfoGetAnalyzersRevision(dbName);
          if (recovered_revision.buildingRevision === 0) {
            break;
          }
          internal.sleep(5);
        }
        analyzers.save("TestAnalyzer2", "identity"); // to trigger cleanup and next revision
        assertFalse(null === analyzers.analyzer(dbName + "::TestAnalyzer")); // check analyzer still here
      } finally {
        db._useDatabase("_system");
        try { db._dropDatabase(dbName); } catch (e) {}
        IM.debugRemoveFailAt('FinishModifyingAnalyzerCoordinator');
      }
    },
    testAnalyzersInsertOnUpdatedDatabase: function() {
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        // remove analyzers revision record for this database from agency
        let preconditions = {};
        let operations = {};
        operations['/arango/Plan/Analyzers/' + dbName] = {'op': 'delete'};
        IM.agencyMgr.call("write", [[operations, preconditions]]);
        IM.agencyMgr.increaseVersion("Plan/Version");
        let requiredVersion = IM.agencyMgr.get("Plan/Version").arango.Plan.Version;
        // Avoid incorrect parsing here:
        assertTrue(Number.isInteger(requiredVersion));
        assertTrue(requiredVersion > 0);
        // Wait until the coordinator has seen the Plan update
        arangoClusterInfoWaitForPlanVersion(requiredVersion);
        // Continue with the test.
        analyzers.save("TestAnalyzer", "identity");
        waitForCompletedRevision(dbName, 1);
      } finally {
        db._useDatabase("_system");
        try { db._dropDatabase(dbName); } catch (e) {}
      }
    },
    testAnalyzersInsertOnUpdatedDatabaseFullAnalyzers: function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        IM.debugClearFailAt();
        IM.debugSetFailAt('AlwaysSwapAnalyzersRevision');
        // remove  full Analyzers part from agency
        let preconditions = {};
        let operations = {};
        operations['/arango/Plan/Analyzers'] = {'op': 'delete'};
        IM.agencyMgr.call("write", [[operations, preconditions]]);
        IM.agencyMgr.increaseVersion("Plan/Version");
        let requiredVersion = IM.agencyMgr.get("Plan/Version").arango.Plan.Version;
        // Avoid incorrect parsing here:
        assertTrue(Number.isInteger(requiredVersion));
        assertTrue(requiredVersion > 0);
        // Wait until the coordinator has seen the Plan update
        arangoClusterInfoWaitForPlanVersion(requiredVersion);
        // Continue with the test.
        IM.debugClearFailAt();
        analyzers.save("TestAnalyzer", "identity");
        waitForCompletedRevision(dbName, 1);
      } finally {
        IM.debugClearFailAt();
        db._useDatabase("_system");
        try { db._dropDatabase(dbName); } catch (e) {}
      }
    },
    testAnalyzersReadingRevisionsForUpdatedDatabase: function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}
      try {
        try {
          analyzers.save("SystemTestAnalyzer", "identity");
        } catch(ee) {
          console.error("Trouble in creating SystemTestAnalyzer: ", JSON.stringify(ee));
          throw ee;
        }
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        IM.debugClearFailAt();
        IM.debugSetFailAt('AlwaysSwapAnalyzersRevision');
        // remove Analyzers part  to simulate freshly updated cluster
        // where system already has created custom analyzers and revisions
        // but other database have not
        let preconditions = {};
        let operations = {};
        operations['/arango/Plan/Analyzers/' + dbName] = {'op': 'delete'};
        IM.agencyMgr.call("write", [[operations, preconditions]]);
        IM.agencyMgr.increaseVersion("Plan/Version");
        let requiredVersion = IM.agencyMgr.get("Plan/Version").arango.Plan.Version;
        // Avoid incorrect parsing here:
        assertTrue(Number.isInteger(requiredVersion));
        assertTrue(requiredVersion > 0);
        // Wait until the coordinator has seen the Plan update
        arangoClusterInfoWaitForPlanVersion(requiredVersion);
        // Continue with the test.
        db._query("RETURN TOKENS('Test', '::SystemTestAnalyzer') "); // check system revision is still read
        IM.debugClearFailAt();
      } finally {
        IM.debugClearFailAt();
        db._useDatabase("_system");
        analyzers.remove("SystemTestAnalyzer", true);
        try { db._dropDatabase(dbName); } catch (e) {}
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(analyzersRevisionTestSuite);

return jsunity.done();
