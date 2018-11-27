/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse*/

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var ERRORS = require("@arangodb").errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function IResearchFeatureDDLTestSuite () {
  return {
    setUpAll : function () {
    },

    tearDownAll : function () {
      db._useDatabase("_system");

      db._dropView("TestView");
      db._dropView("TestView1");
      db._dropView("TestView2");
      db._drop("TestCollection");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      db._dropDatabase("TestDB");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchFeature tests
////////////////////////////////////////////////////////////////////////////////

    testStressAddRemoveView : function() {
      db._dropView("TestView");
      for (let i = 0; i < 100; ++i) {
        db._createView("TestView", "arangosearch", {});
        assertTrue(null != db._view("TestView"));
        db._dropView("TestView");
        assertTrue(null == db._view("TestView"));
      }
    },

    testStressAddRemoveViewWithDirectLinks : function() {
      db._drop("TestCollection0");
      db._dropView("TestView");
      db._create("TestCollection0");

      for (let i = 0; i < 100; ++i) {
        db.TestCollection0.save({ name : i.toString() });
        db._createView("TestView", "arangosearch", {links:{"TestCollection0":{ includeAllFields:true}}});
        var view = db._view("TestView");
        assertTrue(null != view);
        assertEqual(Object.keys(view.properties().links).length, 1);
        db._dropView("TestView");
        assertTrue(null == db._view("TestView"));
      }
    },

    testStressAddRemoveViewWithLink : function() {
      db._drop("TestCollection0");
      db._dropView("TestView");
      db._create("TestCollection0");

      var addLink = { links: { "TestCollection0": { includeAllFields:true} } };

      for (let i = 0; i < 100; ++i) {
        db.TestCollection0.save({ name : i.toString() });
        var view = db._createView("TestView", "arangosearch", {});
        view.properties(addLink, true); // partial update
        let properties = view.properties();
        assertTrue(Object === properties.links.constructor);
        assertEqual(1, Object.keys(properties.links).length);
        var indexes = db.TestCollection0.getIndexes(false);
        assertEqual(1, indexes.length);
        assertEqual("primary", indexes[0].type);
        indexes = db.TestCollection0.getIndexes(false, true);
        assertEqual(2, indexes.length);
        var link = indexes[1];
        assertEqual("primary", indexes[0].type);
        assertNotEqual(null, link);
        assertEqual("arangosearch", link.type);
        db._dropView("TestView");
        assertEqual(null, db._view("TestView"));
        assertEqual(1, db.TestCollection0.getIndexes(false, true).length);
      }
    },

    testStressAddRemoveLink : function() {
      db._drop("TestCollection0");
      db._dropView("TestView");
      db._create("TestCollection0");
      var view = db._createView("TestView", "arangosearch", {});

      var addLink = { links: { "TestCollection0": { includeAllFields:true} } };
      var removeLink = { links: { "TestCollection0": null } };

      for (let i = 0; i < 100; ++i) {
        db.TestCollection0.save({ name : i.toString() });
        view.properties(addLink, true); // partial update
        let properties = view.properties();
        assertTrue(Object === properties.links.constructor);
        assertEqual(1, Object.keys(properties.links).length);
        var indexes = db.TestCollection0.getIndexes(false, true);
        assertEqual(2, indexes.length);
        var link = indexes[1];
        assertEqual("primary", indexes[0].type);
        assertNotEqual(null, link);
        assertEqual("arangosearch", link.type);
        view.properties(removeLink, false);
        properties = view.properties();
        assertTrue(Object === properties.links.constructor);
        assertEqual(0, Object.keys(properties.links).length);
        assertEqual(1, db.TestCollection0.getIndexes(false, true).length);
      }
    },

    testRemoveLinkViaCollection : function() {
      db._drop("TestCollection0");
      db._dropView("TestView");

      var view = db._createView("TestView", "arangosearch", {});
      db._create("TestCollection0");
      db.TestCollection0.save({ name : 'foo' });
      var addLink = { links: { "TestCollection0": { includeAllFields: true } } };
      view.properties(addLink, true); // partial update
      let properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(1, Object.keys(properties.links).length);
      db._drop("TestCollection0");
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(0, Object.keys(properties.links).length);
    },

    testViewDDL: function() {
      // collections
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      db._dropView("TestView");
      db._create("TestCollection0");
      db._create("TestCollection1");
      db._create("TestCollection2");
      var view = db._createView("TestView", "arangosearch", {});

      for (var i = 0; i < 1000; ++i) {
        db.TestCollection0.save({ name : i.toString() });
        db.TestCollection1.save({ name : i.toString() });
        db.TestCollection2.save({ name : i.toString() });
      }

      var properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(0, Object.keys(properties.links).length);

      var meta = { links: { "TestCollection0": { includeAllFields:true } } };
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(1, Object.keys(properties.links).length);

      meta = { links: { "TestCollection1": { includeAllFields:true } } };
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(2, Object.keys(properties.links).length);

      meta = { links: { "TestCollection2": { includeAllFields:true } } };
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
      assertEqual(10, properties.cleanupIntervalStep);
      assertEqual(60000, properties.consolidationIntervalMsec);
      assertTrue(Object === properties.consolidationPolicy.constructor);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.1).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      meta = {
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(10, properties.cleanupIntervalStep);
      assertEqual(10000, properties.consolidationIntervalMsec);
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
      assertEqual(60000, properties.consolidationIntervalMsec);
      assertTrue(Object === properties.consolidationPolicy.constructor);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.75).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));
    },

    testLinkDDL: function() {
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      db._dropView("TestView");
      db._create("TestCollection0");
      db._create("TestCollection1");
      db._create("TestCollection2");
      var view = db._createView("TestView", "arangosearch", {});

      for (var i = 0; i < 1000; ++i) {
        db.TestCollection0.save({ name : i.toString() });
        db.TestCollection1.save({ name : i.toString() });
        db.TestCollection2.save({ name : i.toString() });
      }

      var meta = { links: {
        "TestCollection0": { },
        "TestCollection1": { analyzers: [ "text_en"], includeAllFields: true, trackListPositions: true, storeValues: "full" },
        "TestCollection2": { fields: {
          "b": { fields: { "b1": {} } },
          "c": { includeAllFields: true },
          "d": { trackListPositions: true, storeValues: "id" },
          "e": { analyzers: [ "text_de"] }
        } }
      } };
      view.properties(meta, true); // partial update
      var properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(3, Object.keys(properties.links).length);

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
      assertEqual("full", properties.links.TestCollection1.storeValues);
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

      meta = { links: { "TestCollection0": null, "TestCollection2": {} } };
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(2, Object.keys(properties.links).length);

      assertTrue(Object === properties.links.TestCollection1.constructor);
      assertTrue(Object === properties.links.TestCollection1.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection1.fields).length);
      assertTrue(Boolean === properties.links.TestCollection1.includeAllFields.constructor);
      assertEqual(true, properties.links.TestCollection1.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection1.trackListPositions.constructor);
      assertEqual(true, properties.links.TestCollection1.trackListPositions);
      assertTrue(String === properties.links.TestCollection1.storeValues.constructor);
      assertEqual("full", properties.links.TestCollection1.storeValues);
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

    testViewCreate: function() {
      // 1 empty collection
      db._dropView("TestView");
      db._drop("TestCollection0");
      var col0 = db._create("TestCollection0");
      var view = db._createView("TestView", "arangosearch", {});

      var meta = { links: { "TestCollection0": { includeAllFields: true } } };
      view.properties(meta, true); // partial update

      var result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(0, result.length);

      col0.save({ name: "quarter", text: "quick over" });
      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(1, result.length);
      assertEqual("quarter", result[0].name);

      // 1 non-empty collection
      db._dropView("TestView");
      db._drop("TestCollection0");
      col0 = db._create("TestCollection0");
      view = db._createView("TestView", "arangosearch", {});

      col0.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
      col0.save({ name: "half", text: "quick fox over lazy" });
      col0.save({ name: "other half", text: "the brown jumps the dog" });
      col0.save({ name: "quarter", text: "quick over" });

      meta = { links: { "TestCollection0": { includeAllFields: true } } };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);

      // 2 non-empty collections
      db._dropView("TestView");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      col0 = db._create("TestCollection0");
      var col1 = db._create("TestCollection1");
      view = db._createView("TestView", "arangosearch", {});

      col0.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
      col0.save({ name: "half", text: "quick fox over lazy" });
      col1.save({ name: "other half", text: "the brown jumps the dog" });
      col1.save({ name: "quarter", text: "quick over" });

      meta = { links: {
        "TestCollection0": { includeAllFields: true },
        "TestCollection1": { includeAllFields: true }
      } };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);

      // 1 empty collection + 2 non-empty collections
      db._dropView("TestView");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      col0 = db._create("TestCollection0");
      col1 = db._create("TestCollection1");
      var col2 = db._create("TestCollection2");
      view = db._createView("TestView", "arangosearch", {});

      col2.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
      col2.save({ name: "half", text: "quick fox over lazy" });
      col0.save({ name: "other half", text: "the brown jumps the dog" });
      col0.save({ name: "quarter", text: "quick over" });

      meta = { links: {
        "TestCollection0": { includeAllFields: true },
        "TestCollection1": { includeAllFields: true },
        "TestCollection2": { includeAllFields: true }
      } };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);
    },

    testViewCreateDuplicate: function() {
      db._dropView("TestView");
      var view = db._createView("TestView", "arangosearch", {});

      try {
        db._createView("TestView", "arangosearch", {});
        fail();
      } catch(e) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, e.errorNum);
      }
    },

    testViewModify: function() {
      // 1 empty collection
      db._dropView("TestView");
      db._drop("TestCollection0");
      var col0 = db._create("TestCollection0");
      var view = db._createView("TestView", "arangosearch", { "cleanupIntervalStep": 42 });

      var meta = { links: { "TestCollection0": { includeAllFields: true } } };
      view.properties(meta, true); // partial update

      meta = {
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      var result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(0, result.length);
      var properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      col0.save({ name: "quarter", text: "quick over" });
      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(1, result.length);
      assertEqual("quarter", result[0].name);

      // 1 non-empty collection
      db._dropView("TestView");
      db._drop("TestCollection0");
      col0 = db._create("TestCollection0");
      view = db._createView("TestView", "arangosearch", { "cleanupIntervalStep": 42 });

      col0.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
      col0.save({ name: "half", text: "quick fox over lazy" });
      col0.save({ name: "other half", text: "the brown jumps the dog" });
      col0.save({ name: "quarter", text: "quick over" });

      meta = { links: { "TestCollection0": { includeAllFields: true } } };
      view.properties(meta, true); // partial update

      meta = {
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      // 2 non-empty collections
      db._dropView("TestView");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      col0 = db._create("TestCollection0");
      var col1 = db._create("TestCollection1");
      view = db._createView("TestView", "arangosearch", { "cleanupIntervalStep": 42 });

      col0.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
      col0.save({ name: "half", text: "quick fox over lazy" });
      col1.save({ name: "other half", text: "the brown jumps the dog" });
      col1.save({ name: "quarter", text: "quick over" });

      meta = { links: {
        "TestCollection0": { includeAllFields: true },
        "TestCollection1": { includeAllFields: true }
      } };
      view.properties(meta, true); // partial update

      meta = {
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      // 1 empty collection + 2 non-empty collections
      db._dropView("TestView");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      col0 = db._create("TestCollection0");
      col1 = db._create("TestCollection1");
      var col2 = db._create("TestCollection2");
      view = db._createView("TestView", "arangosearch", { "cleanupIntervalStep": 42 });

      col2.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
      col2.save({ name: "half", text: "quick fox over lazy" });
      col0.save({ name: "other half", text: "the brown jumps the dog" });
      col0.save({ name: "quarter", text: "quick over" });

      meta = { links: {
        "TestCollection0": { includeAllFields: true },
        "TestCollection1": { includeAllFields: true },
        "TestCollection2": { includeAllFields: true }
      } };
      view.properties(meta, true); // partial update

      meta = {
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      view.properties({}, false); // full update (reset to defaults)
      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(0, result.length);
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(10, properties.cleanupIntervalStep);
      assertEqual(60000, properties.consolidationIntervalMsec);
      assertTrue(Object === properties.consolidationPolicy.constructor);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.1).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));
      assertTrue(Object === properties.links.constructor);
      assertEqual(0, Object.keys(properties.links).length);
    },

    testLinkModify: function() {
      db._dropView("TestView");
      db._drop("TestCollection0");
      var col0 = db._create("TestCollection0");
      var view = db._createView("TestView", "arangosearch", {});

      col0.save({ a: "foo", c: "bar", z: 0 });
      col0.save({ a: "foz", d: "baz", z: 1 });
      col0.save({ b: "bar", c: "foo", z: 2 });
      col0.save({ b: "baz", d: "foz", z: 3 });

      var meta = { links: { "TestCollection0": { fields: { a: {} } } } };
      view.properties(meta, true); // partial update

      var result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(0, result[0].z);
      assertEqual(1, result[1].z);

      meta = { links: { "TestCollection0": { fields: { b: {} } } } };
      view.properties(meta, true); // partial update

      var updatedMeta = view.properties();
      assertNotEqual(undefined, updatedMeta.links.TestCollection0.fields.b);
      assertEqual(undefined, updatedMeta.links.TestCollection0.fields.a);

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(2, result[0].z);
      assertEqual(3, result[1].z);

      meta = { links: { "TestCollection0": { fields: { c: {} } } } };
      view.properties(meta, false); // full update

      result = db._query("FOR doc IN  TestView OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(0, result[0].z);
      assertEqual(2, result[1].z);
    },

    testLinkSharingBetweenViews: function() {
      db._dropView("TestView1");
      db._dropView("TestView2");
      db._drop("TestCollection0");

      var col0 = db._create("TestCollection0");
      var view1 = db._createView("TestView1", "arangosearch", {});
      var view2 = db._createView("TestView2", "arangosearch", {});

      col0.save({ a: "foo", c: "bar", z: 0 });
      col0.save({ a: "foz", d: "baz", z: 1 });
      col0.save({ b: "bar", c: "foo", z: 2 });
      col0.save({ b: "baz", d: "foz", z: 3 });

      var meta1 = { links: { "TestCollection0": { fields: { a: {}, z: {} }, storeValues: "id" } } };
      var meta2 = { links: { "TestCollection0": { fields: { b: {}, z: {} }, storeValues: "id" } } };

      view1.properties(meta1, true); // partial update
      var result = db._query("FOR doc IN TestView1 SEARCH EXISTS(doc.a) OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(0, result[0].z);
      assertEqual(1, result[1].z);

      view2.properties(meta2, true); // partial update

      result = db._query("FOR doc IN TestView2 SEARCH EXISTS(doc.b) OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(2, result[0].z);
      assertEqual(3, result[1].z);

      result = (db._query("RETURN APPEND(FOR doc IN TestView1 SEARCH doc.z < 2 OPTIONS { waitForSync: true } SORT doc.z RETURN doc, FOR doc IN TestView2 SEARCH doc.z > 1 OPTIONS { waitForSync: true } SORT doc.z RETURN doc)").toArray())[0];

      assertEqual(4, result.length);
      result.forEach(function(r, i) {
        assertEqual(i, r.z);
      });
    },

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
      for (let i = 0; i < 10; i++) {
        col.insert({ i });
      }
      {
        const view = db._createView(viewName, 'arangosearch', {});
        view.properties({ links: { [colName]: { includeAllFields: true } } });
        db._dropView(viewName);
      } // forget variable `view`, it's invalid now
      assertEqual(db[viewName], undefined);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief test ensure that view is deleted within deleted database
    /// Regression test for arangodb/release-3.4#153.
    ////////////////////////////////////////////////////////////////////////////
    testLeftViewInDroppedDatabase: function () {
      const dbName = 'TestDB';
      const viewName = 'TestView';

      try { db._dropDatabase(dbName); } catch (e) {}

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

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(IResearchFeatureDDLTestSuite);

return jsunity.done();
