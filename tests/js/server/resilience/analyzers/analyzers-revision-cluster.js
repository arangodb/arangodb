/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse */

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
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertFalse(revision.hasOwnProperty("Coordinator"));
      assertFalse(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);

      // valid name 0
      analyzers.save(dbName + "::valid0", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 1
      analyzers.save(dbName + "::valid1", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid name
      try { analyzers.save(dbName + "::inv:alid", "identity"); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 2
      analyzers.save(dbName + "::valid2", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid repeated name 1
      try { analyzers.save(dbName + "::valid1", "delimiter", { "delimiter": "," }); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 0
      analyzers.remove(dbName + "::valid0", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove invalid repeated name 0
      try { analyzers.remove(dbName + "::valid0", true); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 2
      analyzers.remove(dbName + "::valid2", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 1
      analyzers.remove(dbName + "::valid1", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

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
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertFalse(revision.hasOwnProperty("Coordinator"));
      assertFalse(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);

      // valid name 0
      analyzers.save("valid0", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 1
      analyzers.save("valid1", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid name
      try { analyzers.save("inv:alid", "identity"); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 2
      analyzers.save("valid2", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid repeated name 1
      try { analyzers.save("valid1", "delimiter", { "delimiter": "," }); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 0
      analyzers.remove("valid0", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove invalid repeated name 0
      try { analyzers.remove("valid0", true); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 2
      analyzers.remove("valid2", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 1
      analyzers.remove("valid1", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

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
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertEqual(revision.Revision, revision.BuildingRevision);
      let revisionNumber = revision.Revision;

      // valid name 0
      analyzers.save(dbName + "::valid0", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 1
      analyzers.save(dbName + "::valid1", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid name
      try { analyzers.save(dbName + "::inv:alid", "identity"); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 2
      analyzers.save(dbName + "::valid2", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid repeated name 1
      try { analyzers.save(dbName + "::valid1", "delimiter", { "delimiter": "," }); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 0
      analyzers.remove(dbName + "::valid0", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove invalid repeated name 0
      try { analyzers.remove(dbName + "::valid0", true); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 2
      analyzers.remove(dbName + "::valid2", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 1
      analyzers.remove(dbName + "::valid1", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);
    },
    testAnalyzersPlanSystemDefault: function() {
      let dbName = "_system";
      db._useDatabase(dbName);

      let revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertEqual(revision.Revision, revision.BuildingRevision);
      let revisionNumber = revision.Revision;

      // valid name 0
      analyzers.save("::valid0", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 1
      analyzers.save("::valid1", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid name
      try { analyzers.save("::inv:alid", "identity"); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 2
      analyzers.save("::valid2", "identity");
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid repeated name 1
      try { analyzers.save("::valid1", "delimiter", { "delimiter": "," }); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 0
      analyzers.remove("::valid0", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove invalid repeated name 0
      try { analyzers.remove("::valid0", true); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 2
      analyzers.remove("::valid2", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 1
      analyzers.remove("::valid1", true);
      revisionNumber++;
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(revisionNumber, revision.Revision);
      assertEqual(revisionNumber, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(analyzersRevisionTestSuite);

return jsunity.done();
