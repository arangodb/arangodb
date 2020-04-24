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

    testAnalyzersPlan: function() {
      db._useDatabase("_system");
      let dbName = "testDbName";
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      let revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertFalse(revision.hasOwnProperty("Coordinator"));
      assertFalse(revision.hasOwnProperty("RebootID"));
      assertEqual(0, revision.Revision);
      assertEqual(0, revision.BuildingRevision);

      // valid name 0
      analyzers.save(dbName + "::valid0", "identity");
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(1, revision.Revision);
      assertEqual(1, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 1
      analyzers.save(dbName + "::valid1", "identity");
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(2, revision.Revision);
      assertEqual(2, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid name
      try { analyzers.save(dbName + "::inv:alid", "identity"); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(2, revision.Revision);
      assertEqual(2, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // valid name 2
      analyzers.save(dbName + "::valid2", "identity");
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(3, revision.Revision);
      assertEqual(3, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // invalid repeated name 1
      try { analyzers.save(dbName + "::valid1", "delimiter"); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(3, revision.Revision);
      assertEqual(3, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 0
      analyzers.remove(dbName + "::valid0", true);
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(4, revision.Revision);
      assertEqual(4, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove invalid repeated name 0
      try { analyzers.remove(dbName + "::valid0", true); } catch(e) {};
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(4, revision.Revision);
      assertEqual(4, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      // remove valid name 2
      analyzers.remove(dbName + "::valid2", true);
      revision = global.ArangoClusterInfo.getAnalyzersRevision(dbName);
      assertTrue(revision.hasOwnProperty("Revision"));
      assertTrue(revision.hasOwnProperty("BuildingRevision"));
      assertTrue(revision.hasOwnProperty("Coordinator"));
      assertTrue(revision.hasOwnProperty("RebootID"));
      assertEqual(5, revision.Revision);
      assertEqual(5, revision.BuildingRevision);
      assertNotEqual("", revision.Coordinator);
      assertEqual(1, revision.RebootID);

      db._useDatabase("_system");
      db._dropDatabase(dbName);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(analyzersRevisionTestSuite);

return jsunity.done();
