/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull*/

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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
const internal = require('internal');
var db = require("@arangodb").db;
var analyzers = require("@arangodb/analyzers");
var ERRORS = require("@arangodb").errors;
const isServer = require("@arangodb").isServer;
const isCluster = require("internal").isCluster();
const isEnterprise = require("internal").isEnterprise();
const request = require("@arangodb/request");
const {
  triggerMetrics,
} = require('@arangodb/test-helper');
const { checkIndexMetrics } = require("@arangodb/test-helper-common");
const tasks = require('@arangodb/tasks');
const fs = require('fs');

// define global collection which will be used across all testSuites. It will be dropped later
const collection123 = db._create("collection123");

// define global inverted index which will be used across all testSuites
let indexMetaGlobal = {};
if (isEnterprise) {
  indexMetaGlobal = {
    name: "inverted",
    type: "inverted",
    fields: [
      {
        "name": "value",
        "nested": [
          {
            "name": "nested_1",
            "nested": [
              {
                "name": "nested_2"
              }
            ]
          }
        ]
      },
      "name_1"
    ]
  };
} else {
  indexMetaGlobal = {
    name: "inverted",
    type: "inverted",
    "includeAllFields": true,
    fields: [
      "name_1",
      "value[*]"
    ]
  };
}
collection123.ensureIndex(indexMetaGlobal);

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function IResearchFeatureDDLTestSuite1() {
  let docCount = 0;
  return {
    setUpAll: function () {
    },

    tearDownAll: function () {
      db._useDatabase("_system");

      db._drop("FiguresCollection");
      db._dropView("TestView");
      db._dropView("TestView1");
      db._dropView("TestView2");
      db._dropView("TestNameConflictDB");
      db._dropView("testDatabaseAnalyzer");
      db._drop("TestCollection");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      db._drop("collection123");
      try {
        db._dropDatabase("TestDB");
      } catch (_) {
      }
    },

    testInvertedIndexFigures: function() {
      let c = db._create("FiguresCollection");
      let sync = 'FOR d in FiguresCollection OPTIONS { indexHint: "i", forceIndexHint: true, waitForSync:true } FILTER d.a == "a" RETURN d';
      let doc = {"a": "b", "_key": "1"};

      c.save({"a": "a"});
      c.ensureIndex({"name": "i", "type": "inverted", "fields": ["a"]});
      let figures = c.figures(true);
      assertEqual(figures.engine.indexes[1].type, "inverted");
      assertEqual(figures.engine.indexes[1].count, 1);

      c.save(doc);
      db._query(sync);
      figures = c.figures(true);
      assertEqual(figures.engine.indexes[1].type, "inverted");
      assertEqual(figures.engine.indexes[1].count, 2);

      c.remove(doc);
      db._query(sync);
      figures = c.figures(true);
      assertEqual(figures.engine.indexes[1].type, "inverted");
      assertEqual(figures.engine.indexes[1].count, 1);
    },

    testViewIsBuilding: function () {
      let { instanceRole } = require('@arangodb/testutils/instance');
      let IM = global.instanceManager;
      if (isCluster) {
        IM.debugSetFailAt("search::AlwaysIsBuildingCluster", instanceRole.coordinator);
      } else {
        IM.debugSetFailAt("search::AlwaysIsBuildingSingle");
      }

      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._dropView("TestView");
      db._create("TestCollection0");
      db._create("TestCollection1");

      db._createView("TestView", "arangosearch", {
        links: {
          "TestCollection0": { includeAllFields: true },
          "TestCollection1": { includeAllFields: true },
        }
      });
      let r = db._query("FOR d IN TestView SEARCH 1 == 1 RETURN d");
      assertEqual(r.getExtra().warnings, [{
        "code": 1240,
        "message": "ArangoSearch view 'TestView' building is in progress. Results can be incomplete."
      }]);
      if (isCluster) {
        IM.debugRemoveFailAt("search::AlwaysIsBuildingCluster", instanceRole.coordinator);
      } else {
        IM.debugRemoveFailAt("search::AlwaysIsBuildingSingle");
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief IResearchFeature tests
    ////////////////////////////////////////////////////////////////////////////////

    testStressAddRemoveView: function () {
      db._dropView("TestView");
      for (let i = 0; i < 100; ++i) {
        db._createView("TestView", "arangosearch", {});
        assertTrue(null != db._view("TestView"));
        db._dropView("TestView");
        assertTrue(null == db._view("TestView"));
      }
    },

    testStressAddRemoveViewWithDirectLinks: function () {
      db._drop("TestCollection0");
      db._dropView("TestView");
      let c = db._create("TestCollection0");
      c.ensureIndex(indexMetaGlobal);

      for (let i = 0; i < 100; ++i) {
        db.TestCollection0.save({ name: i.toString(), "value": [{ "nested_1": [{ "nested_2": `foo${i}` }] }] });
        db.collection123.save({ name_1: i.toString(), "value": [{ "nested_1": [{ "nested_2": `foo${i}` }] }] });
        docCount += 1;
        let meta = {};
        if (isEnterprise) {
          meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
        } else {
          meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": {} } } } };
        }
        db._createView("TestView", "arangosearch", meta);
        var view = db._view("TestView");
        assertTrue(null != view);
        assertEqual(Object.keys(view.properties().links).length, 2);
        db._dropView("TestView");
        assertTrue(null == db._view("TestView"));
      }

      let result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'wrong' RETURN doc").toArray();
      assertEqual(100, result.length);
    },

    testStressAddRemoveViewWithLink: function () {
      db._drop("TestCollection0");
      db._dropView("TestView");
      let c = db._create("TestCollection0");
      c.ensureIndex(indexMetaGlobal);

      let meta = {};
      if (isEnterprise) {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": {} } } } };
      }

      for (let i = 0; i < 100; ++i) {
        db.TestCollection0.save({ name: i.toString(), "value": [{ "nested_1": [{ "nested_2": i.toString() + "foo" }] }] });
        db.collection123.save({ name_1: i.toString(), "value": [{ "nested_1": [{ "nested_2": "foo123" }] }] });
        docCount += 1;
        var view = db._createView("TestView", "arangosearch", {});
        view.properties(meta, true); // partial update
        let properties = view.properties();
        assertTrue(Object === properties.links.constructor);
        assertEqual(2, Object.keys(properties.links).length);
        var indexes = db.TestCollection0.getIndexes(false);
        assertEqual(2, indexes.length);
        assertEqual("primary", indexes[0].type);
        indexes = db.TestCollection0.getIndexes(false, true);
        assertEqual(3, indexes.length);
        var ii = indexes[1];
        assertEqual("primary", indexes[0].type);
        assertNotEqual(null, link);
        assertEqual("inverted", ii.type);
        var link = indexes[2];
        assertEqual("primary", indexes[0].type);
        assertNotEqual(null, link);
        assertEqual("arangosearch", link.type);
        db._dropView("TestView");
        assertEqual(null, db._view("TestView"));
        assertEqual(2, db.TestCollection0.getIndexes(false, true).length);
      }
      let result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'wrong' RETURN doc").toArray();
      assertEqual(100, result.length);
    },

    testStressAddRemoveLink: function () {
      db._drop("TestCollection0");
      db._dropView("TestView");
      let c = db._create("TestCollection0");
      c.ensureIndex(indexMetaGlobal);
      var view = db._createView("TestView", "arangosearch", {});

      let meta = {};
      if (isEnterprise) {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": {} } } } };
      }
      var removeLink = { links: { "TestCollection0": null, "collection123": null } };

      for (let i = 0; i < 100; ++i) {
        db.TestCollection0.save({ name: i.toString(), "value": [{ "nested_1": [{ "nested_2": "foo123" }] }] });
        db.collection123.save({ name_1: i.toString(), "value": [{ "nested_1": [{ "nested_2": "foo123" }] }] });
        docCount += 1;
        view.properties(meta, true); // partial update
        let properties = view.properties();
        assertTrue(Object === properties.links.constructor);
        assertEqual(2, Object.keys(properties.links).length);
        var indexes = db.TestCollection0.getIndexes(false, true);
        assertEqual(3, indexes.length);
        var ii = indexes[1];
        assertNotEqual(null, ii);
        assertEqual("inverted", ii.type);
        var link = indexes[2];
        assertEqual("primary", indexes[0].type);
        assertNotEqual(null, link);
        assertEqual("arangosearch", link.type);
        view.properties(removeLink, false);
        properties = view.properties();
        assertTrue(Object === properties.links.constructor);
        assertEqual(0, Object.keys(properties.links).length);
        assertEqual(2, db.TestCollection0.getIndexes(false, true).length);
      }
      let result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'wrong' RETURN doc").toArray();
      assertEqual(100, result.length);
    },

    testRemoveLinkViaCollection: function () {
      db._drop("TestCollection0");
      db._dropView("TestView");

      var view = db._createView("TestView", "arangosearch", {});
      let c = db._create("TestCollection0");
      c.ensureIndex(indexMetaGlobal);
      db.TestCollection0.save({ name: 'foo' });
      let meta = {};
      if (isEnterprise) {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": {} } } } };
      }
      view.properties(meta, true); // partial update
      let properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(2, Object.keys(properties.links).length);
      db._drop("TestCollection0");
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(1, Object.keys(properties.links).length);
    },

    testAddDuplicateAnalyzers: function () {
      db._useDatabase("_system");
      try { db._dropDatabase("TestDuplicateDB"); } catch (e) { }
      try { analyzers.remove("myIdentity", true); } catch (e) { }

      analyzers.save("myIdentity", "identity");
      db._createDatabase("TestDuplicateDB");
      db._useDatabase("TestDuplicateDB");
      analyzers.save("myIdentity", "identity");
      db._create("TestCollection0");

      var view = db._createView("TestView", "arangosearch",
        {
          links:
          {
            "TestCollection0":
            {
              includeAllFields: true, analyzers:
                ["identity", "identity",
                  "TestDuplicateDB::myIdentity", "myIdentity",
                  "::myIdentity", "_system::myIdentity"
                ]
            }
          }
        }
      );
      var properties = view.properties();
      assertEqual(3, Object.keys(properties.links.TestCollection0.analyzers).length);

      let expectedAnalyzers = new Set();
      expectedAnalyzers.add("identity");
      expectedAnalyzers.add("myIdentity");
      expectedAnalyzers.add("::myIdentity");

      for (var i = 0; i < Object.keys(properties.links.TestCollection0.analyzers).length; i++) {
        assertTrue(String === properties.links.TestCollection0.analyzers[i].constructor);
        expectedAnalyzers.delete(properties.links.TestCollection0.analyzers[i]);
      }
      assertEqual(0, expectedAnalyzers.size);

      db._dropView("TestView");
      db._drop("TestCollection0");
      analyzers.remove("myIdentity", true);
      db._useDatabase("_system");
      db._dropDatabase("TestDuplicateDB");
      analyzers.remove("myIdentity", true);
    },

    testViewDDL: function () {
      // collections
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      db._dropView("TestView");
      let c0 = db._create("TestCollection0");
      let c1 = db._create("TestCollection1");
      let c2 = db._create("TestCollection2");
      [c0,c1,c2].forEach(c => c.ensureIndex(indexMetaGlobal));

      var view = db._createView("TestView", "arangosearch", {});

      for (var i = 0; i < 1000; ++i) {
        db.TestCollection0.save({ name: i.toString(), "value": [{ "nested_1": [{ "nested_2": `foo${i}` }] }] });
        db.TestCollection1.save({ name: i.toString(), "value": [{ "nested_1": [{ "nested_2": `foo${i}` }] }] });
        db.TestCollection2.save({ name: i.toString(), "value": [{ "nested_1": [{ "nested_2": `foo${i}` }] }] });
        db.collection123.save({ name_1: i.toString(), "value": [{ "nested_1": [{ "nested_2": `foo${i}` }] }] });
        docCount += 1;
      }
      let result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'wrong' RETURN doc").toArray();
      assertEqual(1000, result.length);
      
      result = db._query("FOR doc IN TestCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'wrong' RETURN doc").toArray();
      assertEqual(1000, result.length);
      
      result = db._query("FOR doc IN TestCollection2 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'wrong' RETURN doc").toArray();
      assertEqual(1000, result.length);

      var properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(0, Object.keys(properties.links).length);

      let meta = { links: { "TestCollection0": { includeAllFields: true } } };
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(1, Object.keys(properties.links).length);

      meta = { links: { "TestCollection1": { includeAllFields: true } } };
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(2, Object.keys(properties.links).length);

      meta = {};
      if (isEnterprise) {
        meta = { links: { "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "collection123": { "fields": { "value": {} } } } };
      }
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(3, Object.keys(properties.links).length);

      meta = { links: { "TestCollection2": { includeAllFields: true } } };
      view.properties(meta, false); // full update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(1, Object.keys(properties.links).length);

      // create with links
      db._dropView("TestView");
      view = db._createView("TestView", "arangosearch", meta);
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(1, Object.keys(properties.links).length);

      // consolidate
      db._dropView("TestView");
      view = db._createView("TestView", "arangosearch", {});

      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(2, properties.cleanupIntervalStep);
      assertEqual(1000, properties.commitIntervalMsec);
      assertEqual(1000, properties.consolidationIntervalMsec);
      assertTrue(Object === properties.consolidationPolicy.constructor);
      assertEqual(6, Object.keys(properties.consolidationPolicy).length);
      assertEqual("tier", properties.consolidationPolicy.type);
      assertEqual(1, properties.consolidationPolicy.segmentsMin);
      assertEqual(10, properties.consolidationPolicy.segmentsMax);
      assertEqual(5 * (1 << 30), properties.consolidationPolicy.segmentsBytesMax);
      assertEqual(2 * (1 << 20), properties.consolidationPolicy.segmentsBytesFloor);
      assertEqual((0.0).toFixed(6), properties.consolidationPolicy.minScore.toFixed(6));

      meta = {
        commitIntervalMsec: 12345,
        consolidationIntervalMsec: 20000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(2, properties.cleanupIntervalStep);
      assertEqual(12345, properties.commitIntervalMsec);
      assertEqual(20000, properties.consolidationIntervalMsec);
      assertTrue(Object === properties.consolidationPolicy.constructor);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      meta = {
        cleanupIntervalStep: 20,
        consolidationPolicy: { threshold: 0.75, type: "bytes_accum" }
      };
      view.properties(meta, false); // full update
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(20, properties.cleanupIntervalStep);
      assertEqual(1000, properties.commitIntervalMsec);
      assertEqual(1000, properties.consolidationIntervalMsec);
      assertTrue(Object === properties.consolidationPolicy.constructor);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.75).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));
    },

    testLinkDDL: function () {
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      db._dropView("TestView");
      let c0 = db._create("TestCollection0");
      let c1 = db._create("TestCollection1");
      let c2 = db._create("TestCollection2");
      [c0,c1,c2].forEach(c => c.ensureIndex(indexMetaGlobal));
      var view = db._createView("TestView", "arangosearch", {});

      for (var i = 0; i < 1000; ++i) {
        db.TestCollection0.save({ name: i.toString(), "value": [{ "nested_1": [{ "nested_2": `fo${i}` }] }] });
        db.TestCollection1.save({ name: i.toString(), "value": [{ "nested_1": [{ "nested_2": `fo${i}` }] }] });
        db.TestCollection2.save({ name: i.toString(), "value": [{ "nested_1": [{ "nested_2": `fo${i}` }] }] });
        db.collection123.save({ name_1: i.toString(), "value": [{ "nested_1": [{ "nested_2": `fo${i}` }] }] });
        docCount += 1;
      }

      let result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'wrong' RETURN doc").toArray();
      assertEqual(1000, result.length);

      result = db._query("FOR doc IN TestCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'wrong' RETURN doc").toArray();
      assertEqual(1000, result.length);

      result = db._query("FOR doc IN TestCollection2 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'wrong' RETURN doc").toArray();
      assertEqual(1000, result.length);

      let meta = {};
      if (isEnterprise) {
        meta = {
          links: {
            "TestCollection0": {},
            "TestCollection1": { analyzers: ["text_en"], includeAllFields: true, trackListPositions: true, storeValues: "value" },
            "TestCollection2": {
              fields: {
                "b": { fields: { "b1": {} } },
                "c": { includeAllFields: true },
                "d": { trackListPositions: true, storeValues: "id" },
                "e": { analyzers: ["text_de"] }
              }
            },
            "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
          }
        };
      } else {
        meta = {
          links: {
            "TestCollection0": {},
            "TestCollection1": { analyzers: ["text_en"], includeAllFields: true, trackListPositions: true, storeValues: "value" },
            "TestCollection2": {
              fields: {
                "b": { fields: { "b1": {} } },
                "c": { includeAllFields: true },
                "d": { trackListPositions: true, storeValues: "id" },
                "e": { analyzers: ["text_de"] }
              }
            },
            "collection123": { "fields": { "value": {} } }
          }
        };
      }

      view.properties(meta, true); // partial update
      var properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(4, Object.keys(properties.links).length);

      assertTrue(Object === properties.links.TestCollection0.constructor);
      assertTrue(Object === properties.links.TestCollection0.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection0.fields).length);
      assertTrue(Boolean === properties.links.TestCollection0.includeAllFields.constructor);
      assertEqual(false, properties.links.TestCollection0.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection0.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection0.trackListPositions);
      assertTrue(String === properties.links.TestCollection0.storeValues.constructor);
      assertEqual("none", properties.links.TestCollection0.storeValues);
      assertTrue(Array === properties.links.TestCollection0.analyzers.constructor);
      assertEqual(1, properties.links.TestCollection0.analyzers.length);
      assertTrue(String === properties.links.TestCollection0.analyzers[0].constructor);
      assertEqual("identity", properties.links.TestCollection0.analyzers[0]);


      assertTrue(Object === properties.links.TestCollection1.constructor);
      assertTrue(Object === properties.links.TestCollection1.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection1.fields).length);
      assertTrue(Boolean === properties.links.TestCollection1.includeAllFields.constructor);
      assertEqual(true, properties.links.TestCollection1.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection1.trackListPositions.constructor);
      assertEqual(true, properties.links.TestCollection1.trackListPositions);
      assertTrue(String === properties.links.TestCollection1.storeValues.constructor);
      assertEqual("value", properties.links.TestCollection1.storeValues);
      assertTrue(Array === properties.links.TestCollection1.analyzers.constructor);
      assertEqual(1, properties.links.TestCollection1.analyzers.length);
      assertTrue(String === properties.links.TestCollection1.analyzers[0].constructor);
      assertEqual("text_en", properties.links.TestCollection1.analyzers[0]);


      assertTrue(Object === properties.links.TestCollection2.constructor);
      assertTrue(Object === properties.links.TestCollection2.fields.constructor);
      assertEqual(4, Object.keys(properties.links.TestCollection2.fields).length);
      assertTrue(Object === properties.links.TestCollection2.fields.b.fields.constructor);
      assertEqual(1, Object.keys(properties.links.TestCollection2.fields.b.fields).length);
      assertTrue(Boolean === properties.links.TestCollection2.fields.c.includeAllFields.constructor);
      assertEqual(true, properties.links.TestCollection2.fields.c.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection2.fields.d.trackListPositions.constructor);
      assertEqual(true, properties.links.TestCollection2.fields.d.trackListPositions);
      assertTrue(String === properties.links.TestCollection2.fields.d.storeValues.constructor);
      assertEqual("id", properties.links.TestCollection2.fields.d.storeValues);
      assertTrue(Array === properties.links.TestCollection2.fields.e.analyzers.constructor);
      assertEqual(1, properties.links.TestCollection2.fields.e.analyzers.length);
      assertTrue(String === properties.links.TestCollection2.fields.e.analyzers[0].constructor);
      assertEqual("text_de", properties.links.TestCollection2.fields.e.analyzers[0]);

      assertTrue(Boolean === properties.links.TestCollection2.includeAllFields.constructor);
      assertEqual(false, properties.links.TestCollection2.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection2.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection2.trackListPositions);
      assertTrue(String === properties.links.TestCollection2.storeValues.constructor);
      assertEqual("none", properties.links.TestCollection2.storeValues);
      assertTrue(Array === properties.links.TestCollection2.analyzers.constructor);
      assertEqual(1, properties.links.TestCollection2.analyzers.length);
      assertTrue(String === properties.links.TestCollection2.analyzers[0].constructor);
      assertEqual("identity", properties.links.TestCollection2.analyzers[0]);

      assertTrue(Object === properties.links.collection123.constructor);
      assertTrue(Object === properties.links.collection123.fields.constructor);
      assertEqual(1, Object.keys(properties.links.collection123.fields).length);
      assertTrue(Boolean === properties.links.collection123.includeAllFields.constructor);
      assertEqual(false, properties.links.collection123.includeAllFields);
      assertTrue(Boolean === properties.links.collection123.trackListPositions.constructor);
      assertEqual(false, properties.links.collection123.trackListPositions);
      assertTrue(String === properties.links.collection123.storeValues.constructor);
      assertEqual("none", properties.links.collection123.storeValues);
      assertTrue(Array === properties.links.collection123.analyzers.constructor);
      assertEqual(1, properties.links.collection123.analyzers.length);
      assertTrue(String === properties.links.collection123.analyzers[0].constructor);
      assertEqual("identity", properties.links.collection123.analyzers[0]);

      meta = { links: { "TestCollection0": null, "TestCollection2": {} } };
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(3, Object.keys(properties.links).length);

      assertTrue(Object === properties.links.TestCollection1.constructor);
      assertTrue(Object === properties.links.TestCollection1.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection1.fields).length);
      assertTrue(Boolean === properties.links.TestCollection1.includeAllFields.constructor);
      assertEqual(true, properties.links.TestCollection1.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection1.trackListPositions.constructor);
      assertEqual(true, properties.links.TestCollection1.trackListPositions);
      assertTrue(String === properties.links.TestCollection1.storeValues.constructor);
      assertEqual("value", properties.links.TestCollection1.storeValues);
      assertTrue(Array === properties.links.TestCollection1.analyzers.constructor);
      assertEqual(1, properties.links.TestCollection1.analyzers.length);
      assertTrue(String === properties.links.TestCollection1.analyzers[0].constructor);
      assertEqual("text_en", properties.links.TestCollection1.analyzers[0]);


      assertTrue(Object === properties.links.TestCollection2.constructor);
      assertTrue(Object === properties.links.TestCollection2.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection2.fields).length);
      assertTrue(Boolean === properties.links.TestCollection2.includeAllFields.constructor);
      assertEqual(false, properties.links.TestCollection2.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection2.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection2.trackListPositions);
      assertTrue(String === properties.links.TestCollection2.storeValues.constructor);
      assertEqual("none", properties.links.TestCollection2.storeValues);
      assertTrue(Array === properties.links.TestCollection2.analyzers.constructor);
      assertEqual(1, properties.links.TestCollection2.analyzers.length);
      assertTrue(String === properties.links.TestCollection2.analyzers[0].constructor);
      assertEqual("identity", properties.links.TestCollection2.analyzers[0]);

      meta = { links: { "TestCollection0": { includeAllFields: true }, "TestCollection1": {} } };
      view.properties(meta, false); // full update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(2, Object.keys(properties.links).length);

      assertTrue(Object === properties.links.TestCollection0.constructor);
      assertTrue(Object === properties.links.TestCollection0.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection0.fields).length);
      assertTrue(Boolean === properties.links.TestCollection0.includeAllFields.constructor);
      assertEqual(true, properties.links.TestCollection0.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection0.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection0.trackListPositions);
      assertTrue(String === properties.links.TestCollection0.storeValues.constructor);
      assertEqual("none", properties.links.TestCollection0.storeValues);
      assertTrue(Array === properties.links.TestCollection0.analyzers.constructor);
      assertEqual(1, properties.links.TestCollection0.analyzers.length);
      assertTrue(String === properties.links.TestCollection0.analyzers[0].constructor);
      assertEqual("identity", properties.links.TestCollection0.analyzers[0]);


      assertTrue(Object === properties.links.TestCollection1.constructor);
      assertTrue(Object === properties.links.TestCollection1.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection1.fields).length);
      assertTrue(Boolean === properties.links.TestCollection1.includeAllFields.constructor);
      assertEqual(false, properties.links.TestCollection1.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection1.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection1.trackListPositions);
      assertTrue(String === properties.links.TestCollection1.storeValues.constructor);
      assertEqual("none", properties.links.TestCollection1.storeValues);
      assertTrue(Array === properties.links.TestCollection1.analyzers.constructor);
      assertEqual(1, properties.links.TestCollection1.analyzers.length);
      assertTrue(String === properties.links.TestCollection1.analyzers[0].constructor);
      assertEqual("identity", properties.links.TestCollection1.analyzers[0]);
    },

    testViewCreate: function () {
      // 1 empty collection
      db._dropView("TestView");
      db._drop("TestCollection0");
      var col0 = db._create("TestCollection0");
      col0.ensureIndex(indexMetaGlobal);
      var view = db._createView("TestView", "arangosearch", {});

      let meta = {};
      if (isEnterprise) {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": {} } } } };
      }
      view.properties(meta, true); // partial update

      var result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0), result.length); // In CE in view there is no docs 

      col0.save({ name: "quarter", text: "quick over", name_1: "quarter" });

      result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == 'quarter' RETURN doc").toArray();
      assertEqual(1, result.length);

      let id = db.collection123.save({ name_1: "quarter", text: "quick over" })["_id"];
      docCount += 1;
      result = db._query('FOR doc IN TestView SEARCH ANALYZER(doc.name == "quarter", "identity") OPTIONS { waitForSync: true } SORT doc.name RETURN doc').toArray();
      assertEqual(1, result.length);
      assertEqual("quarter", result[0].name);

      result = db._query('FOR doc IN TestCollection0 OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == "quarter" SORT doc.name_1 RETURN doc').toArray();
      assertEqual(1, result.length);
      assertEqual("quarter", result[0].name_1);

      // 1 non-empty collection
      db._dropView("TestView");
      db._drop("TestCollection0");
      col0 = db._create("TestCollection0");
      view = db._createView("TestView", "arangosearch", {});
      db.collection123.remove(id);
      docCount -= 1;

      col0.save({name_1: 'full', name: "full", text: "the quick brown fox jumps over the lazy dog", "value": [{ "nested_1": [{ "nested_2": "the" }] }] });
      col0.save({name_1: 'half', name: "half", text: "quick fox over lazy", "value": [{ "nested_1": [{ "nested_2": "quick" }] }] });
      col0.save({name_1: 'other half', name: "other half", text: "the brown jumps the dog", "value": [{ "nested_1": [{ "nested_2": "brown" }] }] });
      col0.save({name_1: 'quarter', name: "quarter", text: "quick over", "value": [{ "nested_1": [{ "nested_2": "jumps" }] }] });

      col0.ensureIndex(indexMetaGlobal);
      result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'a' RETURN doc").toArray();
      assertEqual(4, result.length);

      let key1 = db.collection123.save({ name_1: "full", text: "the quick brown fox jumps over the lazy dog", "value": [{ "nested_1": [{ "nested_2": "the" }] }] });
      let key2 = db.collection123.save({ name_1: "half", text: "quick fox over lazy", "value": [{ "nested_1": [{ "nested_2": "quick" }] }] });
      let key3 = db.collection123.save({ name_1: "other half", text: "the brown jumps the dog", "value": [{ "nested_1": [{ "nested_2": "brown" }] }] });
      let key4 = db.collection123.save({ name_1: "quarter", text: "quick over", "value": [{ "nested_1": [{ "nested_2": "jumps" }] }] });
      docCount += 4;

      meta = {};
      if (isEnterprise) {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
          }
        };
      } else {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "collection123": { "fields": { "value": {} } }
          }
        };
      }
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 4, result.length);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } filter HAS(doc, 'name') SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);

      // 2 non-empty collections
      db._dropView("TestView");
      db._drop("TestCollection0");
      db.collection123.remove(key1);
      db.collection123.remove(key2);
      db.collection123.remove(key3);
      db.collection123.remove(key4);
      docCount -= 4;
      db._drop("TestCollection1");
      col0 = db._create("TestCollection0");
      col0.ensureIndex(indexMetaGlobal);
      var col1 = db._create("TestCollection1");
      col1.ensureIndex(indexMetaGlobal);
      view = db._createView("TestView", "arangosearch", {});

      col0.save({ name_1: "half", name: "full", text: "the quick brown fox jumps over the lazy dog" });
      col0.save({ name_1: "half", name: "half", text: "quick fox over lazy" });
      col1.save({ name_1: "other half", name: "other half", text: "the brown jumps the dog" });
      col1.save({ name_1: "other half", name: "quarter", text: "quick over" });

      result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'a' RETURN doc").toArray();
      assertEqual(2, result.length);
      result = db._query("FOR doc IN TestCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'a' RETURN doc").toArray();
      assertEqual(2, result.length);

      key1 = db.collection123.save({ text: "the quick brown fox jumps over the lazy dog", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      key2 = db.collection123.save({ text: "quick fox over lazy", "value": [{ "nested_1": [{ "nested_2": "jumps" }] }] });
      key3 = db.collection123.save({ text: "the brown jumps the dog", "value": [{ "nested_1": [{ "nested_2": "over" }] }] });
      key4 = db.collection123.save({ text: "quick over", "value": [{ "nested_1": [{ "nested_2": "the" }] }] });
      docCount += 4;

      meta = {};
      if (isEnterprise) {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "TestCollection1": { includeAllFields: true },
            "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
          }
        };
      } else {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "TestCollection1": { includeAllFields: true }
          }
        };
      }

      view.properties(meta, true); // partial update
      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 4, result.length);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } filter HAS(doc, 'name') SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);

      result = db._query('FOR doc IN TestCollection0 OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == "half" SORT doc.name RETURN doc').toArray();
      assertEqual(2, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);

      result = db._query('FOR doc IN TestCollection1 OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == "other half" SORT doc.name RETURN doc').toArray();
      assertEqual(2, result.length);
      assertEqual("other half", result[0].name);
      assertEqual("quarter", result[1].name);

      // 1 empty collection + 2 non-empty collections
      db._dropView("TestView");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      db.collection123.remove(key1);
      db.collection123.remove(key2);
      db.collection123.remove(key3);
      db.collection123.remove(key4);
      docCount -= 4;
      col0 = db._create("TestCollection0");
      col1 = db._create("TestCollection1");
      var col2 = db._create("TestCollection2");
      view = db._createView("TestView", "arangosearch", {});
      col0.ensureIndex(indexMetaGlobal);
      col2.ensureIndex(indexMetaGlobal);

      col0.save({ name_1: "other half", name: "other half", text: "the brown jumps the dog" });
      col0.save({ name_1: "other half", name: "quarter", text: "quick over" });
      col2.save({ name_1: "full", name: "full", text: "the quick brown fox jumps over the lazy dog" });
      col2.save({ name_1: "full", name: "half", text: "quick fox over lazy" });

      result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'a' RETURN doc").toArray();
      assertEqual(2, result.length);
      result = db._query("FOR doc IN TestCollection2 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'a' RETURN doc").toArray();
      assertEqual(2, result.length);

      key1 = db.collection123.save({ text: "the quick brown fox jumps over the lazy dog", "value": [{ "nested_1": [{ "nested_2": "a" }] }] });
      key2 = db.collection123.save({ text: "quick fox over lazy", "value": [{ "nested_1": [{ "nested_2": "the" }] }] });
      key3 = db.collection123.save({ text: "the brown jumps the dog", "value": [{ "nested_1": [{ "nested_2": "are" }] }] });
      key4 = db.collection123.save({ text: "quick over", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      docCount += 4;

      meta = {};
      if (isEnterprise) {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "TestCollection1": { includeAllFields: true },
            "TestCollection2": { includeAllFields: true },
            "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
          }
        };
      } else {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "TestCollection1": { includeAllFields: true },
            "TestCollection2": { includeAllFields: true }
          }
        };
      }

      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 4, result.length);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } filter HAS(doc, 'name') SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);

      result = db._query('FOR doc IN TestCollection0 OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == "other half" SORT doc.name RETURN doc').toArray();
      assertEqual(2, result.length);
      assertEqual("other half", result[0].name);
      assertEqual("quarter", result[1].name);

      result = db._query('FOR doc IN TestCollection2 OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == "full" SORT doc.name RETURN doc').toArray();
      assertEqual(2, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);

      db.collection123.remove(key1);
      db.collection123.remove(key2);
      db.collection123.remove(key3);
      db.collection123.remove(key4);
      docCount -= 4;
    },

    testViewCreateDuplicate: function () {
      db._dropView("TestView");
      let meta = {};
      if (isEnterprise) {
        meta = { links: { "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "collection123": { "fields": { "value": {} } } } };
      }
      var view = db._createView("TestView", "arangosearch", meta);

      try {
        db._createView("TestView", "arangosearch", {});
        fail();
      } catch (e) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, e.errorNum);
      }
    },

    testViewModify: function () {
      // 1 empty collection
      db._dropView("TestView");
      db._drop("TestCollection0");
      var col0 = db._create("TestCollection0");
      var view = db._createView("TestView", "arangosearch", { "cleanupIntervalStep": 42 });

      let meta = {};
      if (isEnterprise) {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": {} } } } };
      }
      view.properties(meta, true); // partial update

      meta = {
        commitIntervalMsec: 12345,
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      let result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0), result.length);
      var properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(12345, properties.commitIntervalMsec);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      col0.save({ nameZ: "quarter", text: "quick over" });
      let key = db.collection123.save({ name: "quarter", text: "quick over", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] })["_key"];
      docCount += 1;
      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 1, result.length);
      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } filter HAS(doc, 'nameZ') SORT doc.nameZ RETURN doc").toArray();
      assertEqual(1, result.length);
      assertEqual("quarter", result[0].nameZ);

      // 1 non-empty collection
      db._dropView("TestView");
      db._drop("TestCollection0");
      db.collection123.remove(key);
      docCount -= 1;
      col0 = db._create("TestCollection0");
      col0.ensureIndex(indexMetaGlobal);
      view = db._createView("TestView", "arangosearch", { "cleanupIntervalStep": 42 });

      col0.save({ name_1: "full", nameX: "full", text: "the quick brown fox jumps over the lazy dog" });
      col0.save({ name_1: "half", nameX: "half", text: "quick fox over lazy" });
      col0.save({ name_1: "other half", nameX: "other half", text: "the brown jumps the dog" });
      col0.save({ name_1: "quarter", nameX: "quarter", text: "quick over" });

      result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'a' RETURN doc").toArray();
      assertEqual(4, result.length);

      let key1 = db.collection123.save({ text: "the quick brown fox jumps over the lazy dog", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] })["_key"];
      let key2 = db.collection123.save({ text: "quick fox over lazy", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] })["_key"];
      let key3 = db.collection123.save({ text: "the brown jumps the dog", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] })["_key"];
      let key4 = db.collection123.save({ text: "quick over", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] })["_key"];
      docCount += 4;

      meta = {};
      if (isEnterprise) {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "TestCollection0": { includeAllFields: true }, "collection123": { "fields": { "value": {} } } } };
      }
      view.properties(meta, true); // partial update

      meta = {
        commitIntervalMsec: 12345,
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.nameX RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 4, result.length);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } filter HAS(doc, 'nameX') SORT doc.nameX RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].nameX);
      assertEqual("half", result[1].nameX);
      assertEqual("other half", result[2].nameX);
      assertEqual("quarter", result[3].nameX);

      result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'empty' SORT doc.nameX RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].nameX);
      assertEqual("half", result[1].nameX);
      assertEqual("other half", result[2].nameX);
      assertEqual("quarter", result[3].nameX);

      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(12345, properties.commitIntervalMsec);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      // 2 non-empty collections
      db._dropView("TestView");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db.collection123.remove(key1);
      db.collection123.remove(key2);
      db.collection123.remove(key3);
      db.collection123.remove(key4);
      docCount -= 4;
      col0 = db._create("TestCollection0");
      var col1 = db._create("TestCollection1");
      view = db._createView("TestView", "arangosearch", { "cleanupIntervalStep": 42 });
      [col0, col1].forEach(c => c.ensureIndex(indexMetaGlobal));

      col0.save({ name_1: "full", nameY: "full", text: "the quick brown fox jumps over the lazy dog" });
      col0.save({ name_1: "half", nameY: "half", text: "quick fox over lazy" });
      col1.save({ name_1: "other half", nameY: "other half", text: "the brown jumps the dog" });
      col1.save({ name_1: "quarter", nameY: "quarter", text: "quick over" });

      result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'a' RETURN doc").toArray();
      assertEqual(2, result.length);
      result = db._query("FOR doc IN TestCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'a' RETURN doc").toArray();
      assertEqual(2, result.length);

      key1 = db.collection123.save({ name42: "full", text: "the quick brown fox jumps over the lazy dog", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      key2 = db.collection123.save({ name42: "half", text: "quick fox over lazy", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      key3 = db.collection123.save({ name42: "other half", text: "the brown jumps the dog", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      key4 = db.collection123.save({ name42: "quarter", text: "quick over", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      docCount += 4;

      meta = {};
      if (isEnterprise) {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "TestCollection1": { includeAllFields: true },
            "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
          }
        };
      } else {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "TestCollection1": { includeAllFields: true }
          }
        };
      }

      view.properties(meta, true); // partial update
      col0.ensureIndex(indexMetaGlobal);
      col1.ensureIndex(indexMetaGlobal);

      meta = {
        commitIntervalMsec: 12345,
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 4, result.length);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } filter HAS(doc, 'nameY') SORT doc.nameY RETURN doc").toArray();
      assertEqual("full", result[0].nameY);
      assertEqual("half", result[1].nameY);
      assertEqual("other half", result[2].nameY);
      assertEqual("quarter", result[3].nameY);

      result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'empty' SORT doc.nameY RETURN doc").toArray();
      assertEqual("full", result[0].nameY);
      assertEqual("half", result[1].nameY);

      result = db._query("FOR doc IN TestCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 != 'empty' SORT doc.nameY RETURN doc").toArray();
      assertEqual("other half", result[0].nameY);
      assertEqual("quarter", result[1].nameY);

      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(12345, properties.commitIntervalMsec);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      // 1 empty collection + 2 non-empty collections
      db._dropView("TestView");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      db.collection123.remove(key1);
      db.collection123.remove(key2);
      db.collection123.remove(key3);
      db.collection123.remove(key4);
      docCount -= 4;

      col0 = db._create("TestCollection0");
      col1 = db._create("TestCollection1");
      var col2 = db._create("TestCollection2");
      col2.ensureIndex(indexMetaGlobal);
      view = db._createView("TestView", "arangosearch", { "cleanupIntervalStep": 42 });
      [col0, col2].forEach(c => c.ensureIndex(indexMetaGlobal));

      col0.save({ name_1: "quarter", nameE: "other half", text: "the brown jumps the dog" });
      col0.save({ name_1: "quarter", nameE: "quarter", text: "quick over" });
      col2.save({ name_1: "quarter", nameE: "full", text: "the quick brown fox jumps over the lazy dog" });
      col2.save({ name_1: "quarter", nameE: "half", text: "quick fox over lazy" });

      result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == 'quarter' RETURN doc").toArray();
      assertEqual(2, result.length);
      result = db._query("FOR doc IN TestCollection2 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == 'quarter' RETURN doc").toArray();
      assertEqual(2, result.length);

      key1 = db.collection123.save({ name42: "full", text: "the quick brown fox jumps over the lazy dog", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      key2 = db.collection123.save({ name42: "half", text: "quick fox over lazy", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      key3 = db.collection123.save({ name42: "other half", text: "the brown jumps the dog", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      key4 = db.collection123.save({ name42: "quarter", text: "quick over", "value": [{ "nested_1": [{ "nested_2": "dog" }] }] });
      docCount += 4;

      meta = {};
      if (isEnterprise) {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "TestCollection1": { includeAllFields: true },
            "TestCollection2": { includeAllFields: true },
            "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
          }
        };
      } else {
        meta = {
          links: {
            "TestCollection0": { includeAllFields: true },
            "TestCollection1": { includeAllFields: true },
            "TestCollection2": { includeAllFields: true }
          }
        };
      }

      view.properties(meta, true); // partial update
      col0.ensureIndex(indexMetaGlobal);
      col1.ensureIndex(indexMetaGlobal);

      meta = {
        commitIntervalMsec: 12345,
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update
      col0.ensureIndex(indexMetaGlobal);
      col2.ensureIndex(indexMetaGlobal);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 4, result.length);
      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } filter HAS(doc, 'nameE') SORT doc.nameE RETURN doc").toArray();
      assertEqual(4, result.length);

      assertEqual("full", result[0].nameE);
      assertEqual("half", result[1].nameE);
      assertEqual("other half", result[2].nameE);
      assertEqual("quarter", result[3].nameE);

      result = db._query('FOR doc IN TestCollection0 OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == "quarter" SORT doc.nameE RETURN doc').toArray();
      assertEqual("other half", result[0].nameE);
      assertEqual("quarter", result[1].nameE);

      result = db._query('FOR doc IN TestCollection2 OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == "quarter" SORT doc.nameE RETURN doc').toArray();
      assertEqual("full", result[0].nameE);
      assertEqual("half", result[1].nameE);

      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(12345, properties.commitIntervalMsec);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      view.properties({}, false); // full update (reset to defaults)
      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(0, result.length);
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(2, properties.cleanupIntervalStep);
      assertEqual(1000, properties.commitIntervalMsec);
      assertEqual(1000, properties.consolidationIntervalMsec);
      assertTrue(Object === properties.consolidationPolicy.constructor);
      assertEqual(6, Object.keys(properties.consolidationPolicy).length);
      assertEqual("tier", properties.consolidationPolicy.type);
      assertEqual(1, properties.consolidationPolicy.segmentsMin);
      assertEqual(10, properties.consolidationPolicy.segmentsMax);
      assertEqual(5 * (1 << 30), properties.consolidationPolicy.segmentsBytesMax);
      assertEqual(2 * (1 << 20), properties.consolidationPolicy.segmentsBytesFloor);
      assertEqual((0.0).toFixed(6), properties.consolidationPolicy.minScore.toFixed(6));
      assertTrue(Object === properties.links.constructor);
      assertEqual(0, Object.keys(properties.links).length);
    },

    testViewModifyImmutableProperties: function () {
      db._dropView("TestView");

      let meta = {};
      if (isEnterprise) {
        meta = {
          writebufferActive: 25,
          writebufferIdle: 12,
          writebufferSizeMax: 44040192,
          locale: "C",
          version: 1,
          primarySort: [
            { field: "my.Nested.field", direction: "asc" },
            { field: "another.field", asc: false }
          ],
          primarySortCompression: "none",
          "cleanupIntervalStep": 42,
          "commitIntervalMsec": 12345,
          "links": {
            "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
          }
        };
      } else {
        meta = {
          writebufferActive: 25,
          writebufferIdle: 12,
          writebufferSizeMax: 44040192,
          locale: "C",
          version: 1,
          primarySort: [
            { field: "my.Nested.field", direction: "asc" },
            { field: "another.field", asc: false }
          ],
          primarySortCompression: "none",
          "cleanupIntervalStep": 42,
          "commitIntervalMsec": 12345,
          "links": {
            "collection123": { "fields": { "value": {} } }
          }
        };
      }

      var view = db._createView("TestView", "arangosearch", meta);

      var properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(25, properties.writebufferActive);
      assertEqual(12, properties.writebufferIdle);
      assertEqual(44040192, properties.writebufferSizeMax);
      assertEqual(undefined, properties.locale);
      assertEqual(undefined, properties.version);
      var primarySort = properties.primarySort;
      assertTrue(Array === primarySort.constructor);
      assertEqual(2, primarySort.length);
      assertEqual("my.Nested.field", primarySort[0].field);
      assertEqual(true, primarySort[0].asc);
      assertEqual("another.field", primarySort[1].field);
      assertEqual(false, primarySort[1].asc);
      assertEqual("none", properties.primarySortCompression);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(12345, properties.commitIntervalMsec);
      assertEqual(1000, properties.consolidationIntervalMsec);
      assertEqual(6, Object.keys(properties.consolidationPolicy).length);
      assertEqual("tier", properties.consolidationPolicy.type);
      assertEqual(1, properties.consolidationPolicy.segmentsMin);
      assertEqual(10, properties.consolidationPolicy.segmentsMax);
      assertEqual(5 * (1 << 30), properties.consolidationPolicy.segmentsBytesMax);
      assertEqual(2 * (1 << 20), properties.consolidationPolicy.segmentsBytesFloor);
      assertEqual((0.0).toFixed(6), properties.consolidationPolicy.minScore.toFixed(6));

      meta = {};
      if (isEnterprise) {
        meta = {
          writebufferActive: 225,
          writebufferIdle: 112,
          writebufferSizeMax: 414040192,
          locale: "en_EN.UTF-8",
          version: 2,
          primarySort: [{ field: "field", asc: false }],
          primarySortCompression: "lz4",
          "cleanupIntervalStep": 442,
          "links": {
            "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
          }
        };
      } else {
        meta = {
          writebufferActive: 225,
          writebufferIdle: 112,
          writebufferSizeMax: 414040192,
          locale: "en_EN.UTF-8",
          version: 2,
          primarySort: [{ field: "field", asc: false }],
          primarySortCompression: "lz4",
          "cleanupIntervalStep": 442,
          "links": {
            "collection123": { "fields": { "value": {} } }
          }
        };
      }

      view.properties(meta, false); // full update

      // check properties after upgrade
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(25, properties.writebufferActive);
      assertEqual(12, properties.writebufferIdle);
      assertEqual(44040192, properties.writebufferSizeMax);
      assertEqual(undefined, properties.locale);
      assertEqual(undefined, properties.version);
      primarySort = properties.primarySort;
      assertTrue(Array === primarySort.constructor);
      assertEqual(2, primarySort.length);
      assertEqual("my.Nested.field", primarySort[0].field);
      assertEqual(true, primarySort[0].asc);
      assertEqual("another.field", primarySort[1].field);
      assertEqual(false, primarySort[1].asc);
      assertEqual("none", properties.primarySortCompression);
      assertEqual(442, properties.cleanupIntervalStep);
      assertEqual(1000, properties.commitIntervalMsec);
      assertEqual(1000, properties.consolidationIntervalMsec);
      assertEqual(6, Object.keys(properties.consolidationPolicy).length);
      assertEqual("tier", properties.consolidationPolicy.type);
      assertEqual(1, properties.consolidationPolicy.segmentsMin);
      assertEqual(10, properties.consolidationPolicy.segmentsMax);
      assertEqual(5 * (1 << 30), properties.consolidationPolicy.segmentsBytesMax);
      assertEqual(2 * (1 << 20), properties.consolidationPolicy.segmentsBytesFloor);
      assertEqual((0.0).toFixed(6), properties.consolidationPolicy.minScore.toFixed(6));
    },

    testLinkModify: function () {
      db._dropView("TestView");
      db._drop("TestCollection0");
      var col0 = db._create("TestCollection0");
      var view = db._createView("TestView", "arangosearch", {});

      col0.save({ a: "foo", c: "bar", z: 0, name_1: 0 });
      col0.save({ a: "foz", d: "baz", z: 1, name_1: 1 });
      col0.save({ b: "bar", c: "foo", z: 2, name_1: 2 });
      col0.save({ b: "baz", d: "foz", z: 3, name_1: 3 });
      col0.ensureIndex(indexMetaGlobal);
      let result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 >= 0 RETURN doc").toArray();
      assertEqual(4, result.length);
      let key1 = db.collection123.save({ a: "foo", c: "bar", "value": [{ "nested_1": [{ "nested_2": "dog0" }] }] });
      let key2 = db.collection123.save({ a: "foz", d: "baz", "value": [{ "nested_1": [{ "nested_2": "dog10" }] }] });
      let key3 = db.collection123.save({ b: "bar", c: "foo", "value": [{ "nested_1": [{ "nested_2": "dog100" }] }] });
      let key4 = db.collection123.save({ b: "baz", d: "foz", "value": [{ "nested_1": [{ "nested_2": "dog1000" }] }] });
      docCount += 4;

      // a
      let meta = {};
      if (isEnterprise) {
        meta = {
          "links": {
            "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } },
            "TestCollection0": { fields: { a: {} } }
          }
        };
      } else {
        meta = {
          "links": {
            "collection123": { "fields": { "value": {} } },
            "TestCollection0": { fields: { a: {} } }
          }
        };
      }
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 2, result.length);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } FILTER doc.z != null SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(0, result[0].z);
      assertEqual(1, result[1].z);

      result = db._query('FOR doc IN TestCollection0 OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER doc.name_1 >= 0 SORT doc.z RETURN doc').toArray();
      assertEqual(4, result.length);
      assertEqual(0, result[0].z);
      assertEqual(1, result[1].z);
      assertEqual(2, result[2].z);
      assertEqual(3, result[3].z);

      // b
      meta = {};
      if (isEnterprise) {
        meta = { links: { "TestCollection0": { fields: { b: {} } }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "TestCollection0": { fields: { b: {} } }, "collection123": { "fields": { "value": {} } } } };
      }
      view.properties(meta, true); // partial update

      var updatedMeta = view.properties();
      assertNotEqual(undefined, updatedMeta.links.TestCollection0.fields.b);
      assertEqual(undefined, updatedMeta.links.TestCollection0.fields.a);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 2, result.length);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } filter HAS(doc, 'z') SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(2, result[0].z);
      assertEqual(3, result[1].z);

      // c
      meta = {};
      if (isEnterprise) {
        meta = { links: { "TestCollection0": { fields: { c: {} } }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta = { links: { "TestCollection0": { fields: { c: {} } }, "collection123": { "fields": { "value": {} } } } };
      }
      view.properties(meta, false); // full update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual((isEnterprise ? docCount : 0) + 2, result.length);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } filter HAS(doc, 'z') SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(0, result[0].z);
      assertEqual(2, result[1].z);
    },

    testLinkSharingBetweenViews: function () {
      db._dropView("TestView1");
      db._dropView("TestView2");
      db._drop("TestCollection0");

      var col0 = db._create("TestCollection0");
      var view1 = db._createView("TestView1", "arangosearch", {});
      var view2 = db._createView("TestView2", "arangosearch", {});

      col0.save({ a: "foo", c: "bar", z: 0, name_1: 0 });
      col0.save({ a: "foz", d: "baz", z: 1, name_1: 1 });
      col0.save({ b: "bar", c: "foo", z: 2, name_1: 2 });
      col0.save({ b: "baz", d: "foz", z: 3, name_1: 3 });
      let key1 = db.collection123.save({ a: "foo", c: "bar", z: 0, "value": [{ "nested_1": [{ "nested_2": "dog10" }] }] });
      let key2 = db.collection123.save({ a: "foz", d: "baz", z: 1, "value": [{ "nested_1": [{ "nested_2": "dog110" }] }] });
      let key3 = db.collection123.save({ b: "bar", c: "foo", z: 2, "value": [{ "nested_1": [{ "nested_2": "dog1100" }] }] });
      let key4 = db.collection123.save({ b: "baz", d: "foz", z: 3, "value": [{ "nested_1": [{ "nested_2": "dog11000" }] }] });
      docCount += 4;

      let meta1 = {};
      let meta2 = {};
      if (isEnterprise) {
        meta1 = { links: { "TestCollection0": { fields: { a: {}, z: {} }, storeValues: "id" }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
        meta2 = { links: { "TestCollection0": { fields: { b: {}, z: {} }, storeValues: "id" }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta1 = { links: { "TestCollection0": { fields: { a: {}, z: {} }, storeValues: "id" }, "collection123": { "fields": { "value": {} } } } };
        meta2 = { links: { "TestCollection0": { fields: { b: {}, z: {} }, storeValues: "id" }, "collection123": { "fields": { "value": {} } } } };
      }

      view1.properties(meta1, true); // partial update
      col0.ensureIndex(indexMetaGlobal);
      let result = db._query("FOR doc IN TestCollection0 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 >= 0 RETURN doc").toArray();
      assertEqual(4, result.length);

      result = db._query("FOR doc IN TestView1 SEARCH EXISTS(doc.a) OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(0, result[0].z);
      assertEqual(1, result[1].z);

      result = db._query("FOR doc IN TestView1 SEARCH EXISTS(doc.a) OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(0, result[0].z);
      assertEqual(1, result[1].z);

      result = db._query('FOR doc IN TestCollection0 OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} FILTER EXISTS(doc.name_1) SORT doc.z RETURN doc').toArray();
      assertEqual(4, result.length);
      assertEqual(0, result[0].z);
      assertEqual(1, result[1].z);
      assertEqual(2, result[2].z);
      assertEqual(3, result[3].z);

      view2.properties(meta2, true); // partial update

      result = db._query("FOR doc IN TestView2 SEARCH EXISTS(doc.b) OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(2, result[0].z);
      assertEqual(3, result[1].z);

      result = (db._query("RETURN APPEND(FOR doc IN TestView1 SEARCH doc.z < 2 OPTIONS { waitForSync: true } SORT doc.z RETURN doc, FOR doc IN TestView2 SEARCH doc.z > 1 OPTIONS { waitForSync: true } SORT doc.z RETURN doc)").toArray())[0];

      assertEqual(4, result.length);
      result.forEach(function (r, i) {
        assertEqual(i, r.z);
      });
    },
    testLinkWithAnalyzerFromOtherDb: function () {
      let databaseNameAnalyzer = "testDatabaseAnalyzer";
      let databaseNameView = "testDatabaseView";
      let tmpAnalyzerName = "TmpIdentity";
      let systemAnalyzerName = "SystemIdentity";
      db._useDatabase("_system");
      try { db._dropDatabase(databaseNameAnalyzer); } catch (e) { }
      try { db._dropDatabase(databaseNameView); } catch (e) { }
      analyzers.save(systemAnalyzerName, "identity");
      db._createDatabase(databaseNameAnalyzer);
      db._createDatabase(databaseNameView);
      db._useDatabase(databaseNameAnalyzer);
      let tmpIdentity = analyzers.save(tmpAnalyzerName, "identity");
      assertTrue(tmpIdentity != null);
      tmpIdentity = undefined;
      db._useDatabase(databaseNameView);
      db._create("FOO");

      let c = db._create("collection321");
      c.ensureIndex(indexMetaGlobal);
      db["collection321"].save({ name_1: "quarter", text: "quick over", "value": [{ "nested_1": [{ "nested_2": "jumps" }] }] });
      let result = db._query("FOR doc IN collection321 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == 'quarter' RETURN doc").toArray();
      assertEqual(1, result.length);

      try {
        let meta = {};
        if (isEnterprise) {
          meta = { links: { "FOO": { analyzers: [databaseNameAnalyzer + "::" + tmpAnalyzerName], "collection321": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } } };
        } else {
          meta = { links: { "FOO": { analyzers: [databaseNameAnalyzer + "::" + tmpAnalyzerName], "collection321": { "fields": { "value": {} } } } } };
        }
        db._createView("FOO_view", "arangosearch", meta);
        fail();
      } catch (e) {
        assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
          e.errorNum);
      }
      // but cross-db usage of system analyzers is ok
      let meta1 = {};
      let meta2 = {};
      if (isEnterprise) {
        meta1 = { links: { "FOO": { analyzers: ["::" + systemAnalyzerName] }, "collection321": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
        meta2 = { links: { "FOO": { analyzers: ["_system::" + systemAnalyzerName] }, "collection321": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
      } else {
        meta1 = { links: { "FOO": { analyzers: ["::" + systemAnalyzerName] }, "collection321": { "fields": { "value": {} } } } };
        meta2 = { links: { "FOO": { analyzers: ["_system::" + systemAnalyzerName] }, "collection321": { "fields": { "value": {} } } } };
      }
      db._createView("FOO_view", "arangosearch", meta1);
      db._createView("FOO_view2", "arangosearch", meta2);

      db._useDatabase("_system");
      db._dropDatabase(databaseNameAnalyzer);
      db._dropDatabase(databaseNameView);
      analyzers.remove(systemAnalyzerName, true);
    },
    // Commented as this situation is not documented (user should not do this), will be adressed later
    //testLinkWithAnalyzerFromOtherDbByAnalyzerDefinitions: function() {
    //  let databaseNameAnalyzer = "testDatabaseAnalyzer";
    //  let databaseNameView = "testDatabaseView";
    //
    //  db._useDatabase("_system");
    //  try { db._dropDatabase(databaseNameAnalyzer);} catch(e) {}
    //  try { db._dropDatabase(databaseNameView);} catch(e) {}
    //  db._createDatabase(databaseNameAnalyzer);
    //  db._createDatabase(databaseNameView);
    //  db._useDatabase(databaseNameAnalyzer);
    //  db._useDatabase(databaseNameView);
    //  db._create("FOO");
    //  try {
    //    db._createView("FOO_view", "arangosearch", 
    //      {
    //        links:{
    //          "FOO":{ 
    //            analyzerDefinitions:[{name:databaseNameAnalyzer + "::TmpIdentity", type: "identity"}],
    //            analyzers:[databaseNameAnalyzer + "::TmpIdentity"]
    //          }
    //        }
    //      });
    //    fail();
    //  } catch(e) {
    //    // analyzerDefinitions should be ignored
    //    // analyzer should not be found
    //    assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code, 
    //                e.errorNum);
    //  }
    //  db._useDatabase("_system");
    //  db._dropDatabase(databaseNameAnalyzer);
    //  db._dropDatabase(databaseNameView);
    //},
    ////////////////////////////////////////////////////////////////////////////
    /// @brief test create & drop of a view with a link.
    /// Regression test for arangodb/backlog#486.
    ////////////////////////////////////////////////////////////////////////////
    testCreateAndDropViewWithLink: function () {
      const colName = 'TestCollection';
      const viewName = 'TestView';

      db._drop(colName);
      db._dropView(viewName);

      const col = db._create(colName);
      col.ensureIndex(indexMetaGlobal);
      for (let i = 0; i < 10; i++) {
        col.insert({ i });
      }
      {
        const view = db._createView(viewName, 'arangosearch', {});
        let meta = {};
        if (isEnterprise) {
          meta = { links: { [colName]: { includeAllFields: true }, "collection123": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } } } };
        } else {
          meta = { links: { [colName]: { includeAllFields: true }, "collection123": { "fields": { "value": {} } } } };
        }
        view.properties(meta);
        db._dropView(viewName);
      } // forget variable `view`, it's invalid now
      assertEqual(db[viewName], undefined);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test link on analyzers collection
    ////////////////////////////////////////////////////////////////////////////////
    testIndexStats: function () {
      const arango = require("@arangodb").arango;
      const colName = 'TestCollection';
      const viewName = 'TestView';

      db._drop(colName);
      db._dropView(viewName);

      const col = db._create(colName);
      const view = db._createView(viewName, 'arangosearch', {
        commitIntervalMsec: 0,        // disable auto-commit
        consolidationIntervalMsec: 0, // disable consolidation
        cleanupIntervalStep: 0        // disable cleanup
      });
      let viewMeta = {};
      if (isEnterprise) {
        viewMeta = {
          links: {
            [colName]: {
              "includeAllFields": true,
              "fields": {
                "value": {
                  "nested": {
                    "nested_1": {
                      "nested": {
                        "nested_2": {}
                      }
                    }
                  }
                },
                "name_1": {}
              }
            }
          }
        };
      } else {
        viewMeta = {
          links: {
            [colName]: {
              "includeAllFields": true
            }
          }
        };
      }
      view.properties(viewMeta);

      let indexMeta = {};
      if (isEnterprise) {
        indexMeta = {
          name: "TestIndex",
          type: "inverted",
          fields: [
            {
              "name": "value",
              "nested": [
                {
                  "name": "nested_1",
                  "nested": [
                    {
                      "name": "nested_2"
                    }
                  ]
                }
              ]
            },
            "name_1"
          ],
          commitIntervalMsec: 0,        // disable auto-commit
          consolidationIntervalMsec: 0, // disable consolidation
          cleanupIntervalStep: 0        // disable cleanup
        };
      } else {
        indexMeta = {
          name: "TestIndex",
          type: "inverted",
          "includeAllFields": true,
          fields: [
            "name_1"
          ],
          commitIntervalMsec: 0,        // disable auto-commit
          consolidationIntervalMsec: 0, // disable consolidation
          cleanupIntervalStep: 0        // disable cleanup
        };
      }
      const idx = col.ensureIndex(indexMeta);
      const globalIdx = col.ensureIndex(indexMetaGlobal);

      const types = ["arangosearch", "inverted"];

      if (isCluster) {
        triggerMetrics();
      }

      // check link stats
      checkIndexMetrics(function () {
        for (const type of types) {
          let figures = db.TestCollection.getIndexes(true, true)
            .find(e => e.type === type)
            .figures;
          assertNotEqual(null, figures);
          assertTrue(Object === figures.constructor);
          assertEqual(6, Object.keys(figures).length);
          assertEqual(0, figures.indexSize);
          assertEqual(0, figures.numDocs);
          assertEqual(0, figures.numLiveDocs);
          assertEqual(0, figures.numPrimaryDocs);
          assertEqual(1, figures.numFiles);
          assertEqual(0, figures.numSegments);
        }
      });

      // insert documents
      col.save([{ foo: 'bar' }]);
      col.save([{ foo: 'baz' }]);
      col.save({ name_1: "abc", text: "the quick brown fox jumps over the lazy dog", "value": [{ "nested_1": [{ "nested_2": "the" }] }] });
      col.save({ name_1: "abc", text: "the quick brown fox", "value": [{ "nested_1": [{ "nested_2": "quick" }] }] });
      col.save({ name_1: "abc", text: "the quick brown fox", "value": [{ "nested_1": [] }] });

      let result = db._query("FOR doc IN TestCollection OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == 'abc' RETURN doc").toArray();
      assertEqual(3, result.length);

      if (isCluster) {
        triggerMetrics();
      }

      // check link stats
      checkIndexMetrics(function () {
        for (const type of types) {
          let figures = db.TestCollection.getIndexes(true, true)
            .find(e => e.type === type)
            .figures;
          assertNotEqual(null, figures);
          assertTrue(Object === figures.constructor);
          assertEqual(6, Object.keys(figures).length);
          assertEqual(0, figures.indexSize);
          assertEqual(0, figures.numDocs);
          assertEqual(0, figures.numLiveDocs);
          assertEqual(0, figures.numPrimaryDocs);
          assertEqual(1, figures.numFiles);
          assertEqual(0, figures.numSegments);
        }
      });

      let syncIndex = function (expected) {
        const syncQuery = `FOR d IN TestCollection OPTIONS {indexHint: "TestIndex", waitForSync:true}
                            FILTER d.name_1 != "unknown" COLLECT WITH COUNT INTO c RETURN c`;
        let count = db._query(syncQuery).toArray()[0];
        assertEqual(expected, count);
      };
      // ensure data is synchronized
      var res = db._query("FOR d IN TestView OPTIONS {waitForSync:true} SORT d.foo RETURN d").toArray();
      assertEqual(5, res.length);
      syncIndex(5);
      res = db._query(`for d in TestCollection OPTIONS { 'forceIndexHint': true, 'waitForSync': true, 'indexHint': 'TestIndex' }
                      filter d.name_1 == 'abc' return d`).toArray();
      assertEqual(3, res.length);
      res = db._query(`for d in TestCollection OPTIONS { 'forceIndexHint': true, 'waitForSync': true, 'indexHint': 'TestIndex' }
      filter d.name_1 == 'abc' return d `).toArray();
      assertEqual(3, res.length);

      if (isCluster) {
        triggerMetrics();
      }

      // check link stats
      checkIndexMetrics(function () {
        for (const type of types) {
          let figures = db.TestCollection.getIndexes(true, true)
            .find(e => e.type === type)
            .figures;
          assertNotEqual(null, figures);
          assertTrue(Object === figures.constructor);
          assertEqual(6, Object.keys(figures).length);
          assertTrue(1000 < figures.indexSize);
          if (type === "arangosearch") {
            assertEqual((isEnterprise ? 10 : 5), figures.numDocs);
            assertEqual((isEnterprise ? 10 : 5), figures.numLiveDocs);
          } else {
            assertEqual((isEnterprise ? 15 : 5), figures.numDocs);
            assertEqual((isEnterprise ? 15 : 5), figures.numLiveDocs);
          }
          assertEqual(5, figures.numPrimaryDocs);
          assertEqual(6, figures.numFiles);
          assertEqual(1, figures.numSegments);
        }
      });

      // remove document
      col.remove(res[0]._key);

      result = db._query("FOR doc IN TestCollection OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == 'abc' RETURN doc").toArray();
      assertEqual(2, result.length);

      // ensure data is synchronized
      res = db._query("FOR d IN TestView OPTIONS {waitForSync:true} RETURN d").toArray();
      assertEqual(4, res.length); // 5 docs in the beginning, 1 deleted afterwards
      res = db._query("FOR d IN TestView OPTIONS {waitForSync:true} SORT d.foo FILTER HAS(d, 'foo') RETURN d").toArray();
      assertEqual('bar', res[0].foo);
      syncIndex(4);

      if (isCluster) {
        triggerMetrics();
      }

      // check link stats
      checkIndexMetrics(function () {
        for (const type of types) {
          let figures = db.TestCollection.getIndexes(true, true)
            .find(e => e.type === type)
            .figures;
          assertNotEqual(null, figures);
          assertTrue(Object === figures.constructor);
          assertEqual(6, Object.keys(figures).length);
          assertTrue(1000 < figures.indexSize);
          if (type === "arangosearch") {
            assertEqual((isEnterprise ? 10 : 5), figures.numDocs);
            assertEqual((isEnterprise ? 7 : 4), figures.numLiveDocs);
          } else {
            assertEqual((isEnterprise ? 15 : 5), figures.numDocs);
            assertEqual((isEnterprise ? 12 : 4), figures.numLiveDocs);
          }
          assertEqual(5, figures.numPrimaryDocs);
          assertEqual(7, figures.numFiles);
          assertEqual(1, figures.numSegments);
        }
      });

      // truncate collection
      col.truncate({ compact: false });

      // ensure data is synchronized
      res = db._query("FOR d IN TestView OPTIONS {waitForSync:true} SORT d.foo RETURN d").toArray();
      assertEqual(0, res.length);
      syncIndex(0);

      if (isCluster) {
        triggerMetrics();
      }

      // check link stats
      checkIndexMetrics(function () {
        for (const type of types) {
          let figures = db.TestCollection.getIndexes(true, true)
            .find(e => e.type === type)
            .figures;
          assertNotEqual(null, figures);
          assertTrue(Object === figures.constructor);
          assertEqual(6, Object.keys(figures).length);
          assertEqual(0, figures.indexSize);
          assertEqual(0, figures.numDocs);
          assertEqual(0, figures.numLiveDocs);
          assertEqual(0, figures.numPrimaryDocs);
          assertEqual(1, figures.numFiles);
          assertEqual(0, figures.numSegments);
        }
      });
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test ensure that view is deleted within deleted database
    /// Regression test for arangodb/release-3.4#153.
    ////////////////////////////////////////////////////////////////////////////
    testLeftViewInDroppedDatabase: function () {
      const dbName = 'TestDB';
      const viewName = 'TestView';
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) { }

      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._createView(viewName, 'arangosearch', {});

      db._useDatabase("_system");
      db._dropDatabase(dbName);
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      assertEqual(db._views().length, 0);
      assertEqual(db[viewName], undefined);
    },

    // test for public issue #9652
    testAnalyzerWithStopwordsNameConflict: function () {
      const dbName = "TestNameConflictDB";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) { }

      db._createDatabase(dbName);
      db._useDatabase(dbName);

      let c1 = db._create("col1");
      let c2 = db._create("col2");
      let c3 = db._create("collection321");
      c3.save({ name_1: "quarter", text: "quick over", "value": [{ "nested_1": [{ "nested_2": "jumps" }] }] });
      c3.ensureIndex(indexMetaGlobal);

      let result = db._query("FOR doc IN collection321 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == 'quarter' RETURN doc").toArray();
      assertEqual(1, result.length);

      [c1,c2,c3].forEach(c => c.ensureIndex(indexMetaGlobal));

      analyzers.save("custom_analyzer",
        "text",
        {
          locale: "en.UTF-8",
          case: "lower",
          stopwords: ["the", "of", "inc", "co", "plc", "ltd", "ag"],
          accent: false,
          stemming: false
        },
        ["position", "norm", "frequency"]);

      let meta = {};
      if (isEnterprise) {
        meta = {
          "links": {
            "col1": {
              "analyzers": ["identity"],
              "fields": { "name": { "analyzers": ["custom_analyzer"] } },
              "includeAllFields": false,
              "storeValues": "none",
              "trackListPositions": false
            },
            "col2": {
              "analyzers": ["identity"],
              "fields": { "name": { "analyzers": ["custom_analyzer"] } },
              "includeAllFields": false,
              "storeValues": "none",
              "trackListPositions": false
            },
            "collection321": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
          }
        };
      } else {
        meta = {
          "links": {
            "col1": {
              "analyzers": ["identity"],
              "fields": { "name": { "analyzers": ["custom_analyzer"] } },
              "includeAllFields": false,
              "storeValues": "none",
              "trackListPositions": false
            },
            "col2": {
              "analyzers": ["identity"],
              "fields": { "name": { "analyzers": ["custom_analyzer"] } },
              "includeAllFields": false,
              "storeValues": "none",
              "trackListPositions": false
            },
            "collection321": { "fields": { "value": {} } }
          }
        };
      }
      let v = db._createView("view1", "arangosearch", meta);
      let properties = v.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(3, Object.keys(properties.links).length);

      assertTrue(Object === properties.links.col1.constructor);
      assertTrue(Object === properties.links.col1.fields.constructor);
      assertEqual(1, Object.keys(properties.links.col1.fields).length);
      assertTrue(Object === properties.links.col1.fields.name.constructor);
      assertTrue(Array === properties.links.col1.fields.name.analyzers.constructor);
      assertEqual(1, properties.links.col1.fields.name.analyzers.length);
      assertTrue(String === properties.links.col1.fields.name.analyzers[0].constructor);
      assertEqual("custom_analyzer", properties.links.col1.fields.name.analyzers[0]);
      assertTrue(Boolean === properties.links.col1.includeAllFields.constructor);
      assertEqual(false, properties.links.col1.includeAllFields);
      assertTrue(Boolean === properties.links.col1.trackListPositions.constructor);
      assertEqual(false, properties.links.col1.trackListPositions);
      assertTrue(String === properties.links.col1.storeValues.constructor);
      assertEqual("none", properties.links.col1.storeValues);
      assertTrue(Array === properties.links.col1.analyzers.constructor);
      assertEqual(1, properties.links.col1.analyzers.length);
      assertTrue(String === properties.links.col1.analyzers[0].constructor);
      assertEqual("identity", properties.links.col1.analyzers[0]);

      assertTrue(Object === properties.links.col2.constructor);
      assertTrue(Object === properties.links.col2.fields.constructor);
      assertEqual(1, Object.keys(properties.links.col2.fields).length);
      assertTrue(Object === properties.links.col2.fields.name.constructor);
      assertTrue(Array === properties.links.col2.fields.name.analyzers.constructor);
      assertEqual(1, properties.links.col2.fields.name.analyzers.length);
      assertTrue(String === properties.links.col2.fields.name.analyzers[0].constructor);
      assertEqual("custom_analyzer", properties.links.col2.fields.name.analyzers[0]);
      assertTrue(Boolean === properties.links.col2.includeAllFields.constructor);
      assertEqual(false, properties.links.col2.includeAllFields);
      assertTrue(Boolean === properties.links.col2.trackListPositions.constructor);
      assertEqual(false, properties.links.col2.trackListPositions);
      assertTrue(String === properties.links.col2.storeValues.constructor);
      assertEqual("none", properties.links.col2.storeValues);
      assertTrue(Array === properties.links.col2.analyzers.constructor);
      assertEqual(1, properties.links.col2.analyzers.length);
      assertTrue(String === properties.links.col2.analyzers[0].constructor);
      assertEqual("identity", properties.links.col2.analyzers[0]);

      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testLeftAnalyzerInDroppedDatabase: function () {
      const dbName = "TestNameDroppedDB";
      const analyzerName = "TestAnalyzer";
      try {
        db._useDatabase("_system");
        try { db._dropDatabase(dbName); } catch (e) { }
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        analyzers.save(analyzerName, "identity");
        // recreating database
        db._useDatabase("_system");
        db._dropDatabase(dbName);
        db._createDatabase(dbName);
        db._useDatabase(dbName);

        assertNull(analyzers.analyzer(analyzerName));
        // this should be no name conflict
        analyzers.save(analyzerName, "text", { "stopwords": [], "locale": "en" });
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test link on analyzers collection
    ////////////////////////////////////////////////////////////////////////////////
    testIndexAnalyzerCollection: function () {
      const dbName = "TestNameDroppedDB";
      const analyzerName = "TestAnalyzer";
      db._useDatabase("_system");
      assertNotEqual(null, db._collection("_analyzers"));
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }
      try {
        analyzers.remove(analyzerName, true);
      } catch (e) {
      }
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      assertEqual(0, db._analyzers.count());
      analyzers.save(analyzerName, "identity");
      // recreating database
      db._useDatabase("_system");
      db._dropDatabase(dbName);
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      assertEqual(0, db._analyzers.count());
      assertNull(analyzers.analyzer(analyzerName));
      // this should be no name conflict
      analyzers.save(analyzerName, "text", { "stopwords": [], "locale": "en" });
      assertEqual(1, db._analyzers.count());

      var view = db._createView("analyzersView", "arangosearch", {
        links: {
          _analyzers: {
            includeAllFields: true,
            analyzers: [analyzerName]
          }
        }
      });
      var res = db._query("FOR d IN analyzersView OPTIONS {waitForSync:true} RETURN d").toArray();
      assertEqual(1, db._analyzers.count());
      assertEqual(1, res.length);
      assertEqual(db._analyzers.toArray()[0], res[0]);

      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testAutoLoadAnalyzerOnViewCreation: function () {
      const dbName = "TestNameDroppedDB";
      const analyzerName = "TestAnalyzer";
      db._useDatabase("_system");
      assertNotEqual(null, db._collection("_analyzers"));
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        db._analyzers.save({ type: "identity", name: analyzerName });
        let c = db._create("collection321");
        c.ensureIndex(indexMetaGlobal);
        let meta = {};
        if (isEnterprise) {
          meta = {
            links: {
              _analyzers: {
                includeAllFields: true,
                analyzers: [analyzerName] // test only new database access. Load from _system is checked in gtest
              },
              "collection321": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
            }
          };
        } else {
          meta = {
            links: {
              _analyzers: {
                includeAllFields: true,
                analyzers: [analyzerName] // test only new database access. Load from _system is checked in gtest
              },
              "collection321": { "fields": { "value": {} } }
            }
          };
        }
        var view = db._createView("analyzersView", "arangosearch", meta);
        db["collection321"].save({ name_1: "quarter", text: "quick over", "value": [{ "nested_1": [{ "nested_2": "jumps" }] }] });

        let result = db._query("FOR doc IN collection321 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == 'quarter' RETURN doc").toArray();
        assertEqual(1, result.length);

        var res = db._query("FOR d IN analyzersView OPTIONS {waitForSync:true} RETURN d").toArray();
        assertEqual((isEnterprise ? 2 : 1), res.length);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
        db._drop("collection321");
      }
    },

    testAutoLoadAnalyzerOnViewUpdate: function () {
      const dbName = "TestNameDroppedDB";
      const analyzerName = "TestAnalyzer";
      db._useDatabase("_system");
      assertNotEqual(null, db._collection("_analyzers"));
      try {
        db._dropDatabase(dbName);
      } catch (e) {
      }
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        db._analyzers.save({ type: "identity", name: analyzerName });
        let c = db._create("collection321");
        c.ensureIndex(indexMetaGlobal);

        var view = db._createView("analyzersView", "arangosearch", {});
        let meta = {};
        if (isEnterprise) {
          meta = {
            links: {
              _analyzers: {
                includeAllFields: true,
                analyzers: [analyzerName] // test only new database access. Load from _system is checked in gtest
              },
              "collection321": { "fields": { "value": { "nested": { "nested_1": { "nested": { "nested_2": {} } } } } } }
            }
          };
        } else {
          meta = {
            links: {
              _analyzers: {
                includeAllFields: true,
                analyzers: [analyzerName] // test only new database access. Load from _system is checked in gtest
              },
              "collection321": { "fields": { "value": {} } }
            }
          };
        }
        db["collection321"].save({ name_1: "quarter", text: "quick over", "value": [{ "nested_1": [{ "nested_2": "jumps" }] }] });

        let result = db._query("FOR doc IN collection321 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.name_1 == 'quarter' RETURN doc").toArray();
        assertEqual(1, result.length);
        
        view.properties(meta, true);
        var res = db._query("FOR d IN analyzersView OPTIONS {waitForSync:true} RETURN d").toArray();
        assertEqual((isEnterprise ? 2 : 1), res.length);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
        db._drop("collection321");
      }
    }
  };
}

function IResearchFeatureDDLTestSuite2() {
  const db = require("@arangodb").db;
  const dbName = 'TestDB';
  const taskCreateLinkInBackground = 'CreateLinkInBackgroundMode_AuxTask';
  return {
    setUp: function () {
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) { }
      db._createDatabase(dbName);
    },
    tearDown: function () {
      try { tasks.unregister(taskCreateLinkInBackground); } catch (e) { }
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },
    ////////////////////////////////////////////////////////////////////////////
    /// @brief test creating link with 'inBackground' set to true
    ////////////////////////////////////////////////////////////////////////////
    testCreateLinkInBackgroundMode: function () {
      const colName = 'TestCollection';
      const viewName = 'TestView';
      const initialCount = 500;
      const inTransCount = 1000;
      const markerFileName = fs.join(fs.getTempPath(), "backgroundLinkMarker");
      try { fs.remove(markerFileName); } catch (e) { }
      db._useDatabase(dbName);
      let col = db._create(colName);
      col.ensureIndex(indexMetaGlobal);
      let v = db._createView(viewName, 'arangosearch', {});
      // some initial documents
      for (let i = 0; i < initialCount; ++i) {
        col.insert({ myField: 'test' + i, name_1: i.toString() });
      }
      let commandText = function (params) {
        var db = require('internal').db;
        db._executeTransaction({
          collections: { write: params.colName },
          action: function (params) {
            var fs = require('fs');
            var db = require('internal').db;
            var c = db._collection(params.colName);
            fs.write(params.markerFileName, "TEST");
            for (var i = 0; i < params.inTransCount; ++i) {
              c.insert({ myField: 'background' + i });
            }
            require('internal').sleep(20);
          },
          params: params
        });
      };
      tasks.register({
        command: commandText,
        params: { colName, inTransCount, dbName, markerFileName },
        name: taskCreateLinkInBackground
      });
      while (!fs.exists(markerFileName)) {
        require('internal').sleep(1); // give transaction some time to run 
      }
      v.properties({ links: { [colName]: { includeAllFields: true, inBackground: true } } });
      // check that all documents are visible
      let docs = db._query("FOR doc IN " + viewName + " OPTIONS { waitForSync: true } RETURN doc").toArray();
      assertEqual(initialCount + inTransCount, docs.length);

      // inBackground should not be returned as part of index definition
      let indexes = col.getIndexes(false, true);
      assertEqual(3, indexes.length);
      var index = indexes[1];
      assertEqual("inverted", index.type);
      assertTrue(undefined === index.inBackground);
      var link = indexes[2];
      assertEqual("arangosearch", link.type);
      assertTrue(undefined === link.inBackground);

      // inBackground should not be returned as part of link definition
      let propertiesReturned = v.properties();
      assertTrue(undefined === propertiesReturned.links[colName].inBackground);
    },
    testCachedColumns: function () {
      const colName = 'TestCollectionCache';
      const viewName = 'TestViewCache';
      try {
        let col = db._create(colName);
        col.ensureIndex(indexMetaGlobal);
        let v = db._createView(viewName, 'arangosearch',
          {
            primarySortCache: true,
            primaryKeyCache: true,
            links: { [colName]: { cache: true } }
          });
        let prop = v.properties();
        if (isEnterprise) {
          assertTrue(prop.primarySortCache);
          assertTrue(prop.primaryKeyCache);
          assertTrue(prop.links.TestCollectionCache.cache);
        } else {
          assertFalse(prop.hasOwnProperty("primarySortCache"));
          assertFalse(prop.hasOwnProperty("primaryKeyCache"));
          assertFalse(prop.links.TestCollectionCache.hasOwnProperty("cache"));
        }
      } finally {
        db._dropView(viewName);
        db._drop(colName);
      }
    },
    testViewOnlyOptions : function() {
      if (!isEnterprise) {
        return;
      }
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        let view = db._createView(viewName, "arangosearch", {
          consolidationIntervalMsec: 0,
          commitIntervalMsec: 0,
          primarySortCache: true,
          primaryKeyCache: true,
          optimizeTopK: ["BM25(@doc) DESC"],
          links: {
            [colName]: {
              storeValues: 'id',
              includeAllFields:true
            }}});
        let props = view.properties();
        assertUndefined(props.links[colName].primarySortCache);
        assertUndefined(props.links[colName].primaryKeyCache);
        assertUndefined(props.links[colName].optimizeTopK);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    }
  }; // return
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(IResearchFeatureDDLTestSuite1);
jsunity.run(IResearchFeatureDDLTestSuite2);
db._drop(collection123.name());

return jsunity.done();
