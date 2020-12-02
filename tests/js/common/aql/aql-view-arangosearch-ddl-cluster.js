/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull*/

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
var analyzers = require("@arangodb/analyzers");
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
/// @brief IResearchFeatureDDLTestSuite tests
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

    testAddDuplicateAnalyzers : function() {
      db._useDatabase("_system");
      try { db._dropDatabase("TestDuplicateDB"); } catch (e) {}
      try { analyzers.remove("myIdentity", true); } catch (e) {}
        
      analyzers.save("myIdentity", "identity");
      db._createDatabase("TestDuplicateDB");
      db._useDatabase("TestDuplicateDB");
      analyzers.save("myIdentity", "identity");
      db._create("TestCollection0");

      var view = db._createView("TestView", "arangosearch", 
        { links : 
          { "TestCollection0" : 
            { includeAllFields: true, analyzers: 
                [ "identity", "identity", 
                "TestDuplicateDB::myIdentity" , "myIdentity", 
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
      assertEqual(2, properties.cleanupIntervalStep);
      assertEqual(1000, properties.commitIntervalMsec);
      assertEqual(1000, properties.consolidationIntervalMsec);
      assertTrue(Object === properties.consolidationPolicy.constructor);
      assertEqual(6, Object.keys(properties.consolidationPolicy).length);
      assertEqual("tier", properties.consolidationPolicy.type);
      assertEqual(1, properties.consolidationPolicy.segmentsMin);
      assertEqual(10, properties.consolidationPolicy.segmentsMax);
      assertEqual(5*(1 << 30), properties.consolidationPolicy.segmentsBytesMax);
      assertEqual(2*(1 << 20), properties.consolidationPolicy.segmentsBytesFloor);
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
        "TestCollection1": { analyzers: [ "text_en"], includeAllFields: true, trackListPositions: true, storeValues: "value" },
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

    testViewCreate: function() {
      // 1 empty collection
      db._dropView("TestView");
      db._drop("TestCollection0");
      var col0 = db._create("TestCollection0");
      var view = db._createView("TestView", "arangosearch", {});

      var meta = { links: { "TestCollection0": { includeAllFields: true } } };
      view.properties(meta, true); // partial update

      var result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(0, result.length);

      col0.save({ name: "quarter", text: "quick over" });
      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
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

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
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

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
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

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
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
        commitIntervalMsec: 12345,
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      var result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(0, result.length);
      var properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(12345, properties.commitIntervalMsec);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      col0.save({ name: "quarter", text: "quick over" });
      
      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
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
        commitIntervalMsec: 12345,
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);
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
        commitIntervalMsec: 12345,
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);
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
        commitIntervalMsec: 12345,
        consolidationIntervalMsec: 10000,
        consolidationPolicy: { threshold: 0.5, type: "bytes_accum" },
      };
      view.properties(meta, true); // partial update

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
      assertEqual(4, result.length);
      assertEqual("full", result[0].name);
      assertEqual("half", result[1].name);
      assertEqual("other half", result[2].name);
      assertEqual("quarter", result[3].name);
      properties = view.properties();
      assertTrue(Object === properties.constructor);
      assertEqual(42, properties.cleanupIntervalStep);
      assertEqual(12345, properties.commitIntervalMsec);
      assertEqual(10000, properties.consolidationIntervalMsec);
      assertEqual(2, Object.keys(properties.consolidationPolicy).length);
      assertEqual("bytes_accum", properties.consolidationPolicy.type);
      assertEqual((0.5).toFixed(6), properties.consolidationPolicy.threshold.toFixed(6));

      view.properties({}, false); // full update (reset to defaults)

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.name RETURN doc").toArray();
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
      assertEqual(5*(1 << 30), properties.consolidationPolicy.segmentsBytesMax);
      assertEqual(2*(1 << 20), properties.consolidationPolicy.segmentsBytesFloor);
      assertEqual((0.0).toFixed(6), properties.consolidationPolicy.minScore.toFixed(6));
      assertTrue(Object === properties.links.constructor);
      assertEqual(0, Object.keys(properties.links).length);
    },

    testViewModifyImmutableProperties: function() {
      db._dropView("TestView");

      var view = db._createView("TestView", "arangosearch", { 
        writebufferActive: 25,
        writebufferIdle: 12,
        writebufferSizeMax: 44040192,
        locale: "C",
        version: 1,
        primarySortCompression:"none",
        primarySort: [
          { field: "my.Nested.field", direction: "asc" },
          { field: "another.field", asc: false }
        ],
        "cleanupIntervalStep": 42,
        "commitIntervalMsec": 12345 
      });

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
      assertEqual(5*(1 << 30), properties.consolidationPolicy.segmentsBytesMax);
      assertEqual(2*(1 << 20), properties.consolidationPolicy.segmentsBytesFloor);
      assertEqual((0.0).toFixed(6), properties.consolidationPolicy.minScore.toFixed(6));

      view.properties({ 
        writebufferActive: 225,
        writebufferIdle: 112,
        writebufferSizeMax: 414040192,
        locale: "en_EN.UTF-8",
        version: 2,
        primarySort: [ { field: "field", asc: false } ],
        primarySortCompression:"lz4",
        "cleanupIntervalStep": 442
      }, false); // full update

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
      assertEqual(5*(1 << 30), properties.consolidationPolicy.segmentsBytesMax);
      assertEqual(2*(1 << 20), properties.consolidationPolicy.segmentsBytesFloor);
      assertEqual((0.0).toFixed(6), properties.consolidationPolicy.minScore.toFixed(6));
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

      var result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(0, result[0].z);
      assertEqual(1, result[1].z);

      meta = { links: { "TestCollection0": { fields: { b: {} } } } };
      view.properties(meta, true); // partial update

      var updatedMeta = view.properties();
      assertNotEqual(undefined, updatedMeta.links.TestCollection0.fields.b);
      assertEqual(undefined, updatedMeta.links.TestCollection0.fields.a);

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
      assertEqual(2, result.length);
      assertEqual(2, result[0].z);
      assertEqual(3, result[1].z);

      meta = { links: { "TestCollection0": { fields: { c: {} } } } };
      view.properties(meta, false); // full update

      result = db._query("FOR doc IN TestView OPTIONS { waitForSync: true } SORT doc.z RETURN doc").toArray();
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

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test link on analyzers collection
    /// FIXME not supported in cluster atm
    ////////////////////////////////////////////////////////////////////////////////
    testIndexStats : function() {
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
      view.properties({ links: { [colName]: { includeAllFields: true } } });

      // check link stats
      {
        let figures = db.TestCollection.getIndexes(true, true)
                                    .find(e => e.type === 'arangosearch')
                                    .figures;
        assertNotEqual(null, figures);
        assertTrue(Object === figures.constructor);
        assertEqual(6, Object.keys(figures).length);
        assertEqual(0, figures.indexSize);
        assertEqual(0, figures.numDocs);
        assertEqual(0, figures.numLiveDocs);
        assertEqual(0, figures.numBufferedDocs);
        assertEqual(0, figures.numFiles);
        assertEqual(0, figures.numSegments);
      }

      // insert documents
      col.save({ foo: 'bar' });
      col.save({ foo: 'baz' });

      // check link stats
      {
        let figures = db.TestCollection.getIndexes(true, true)
                                    .find(e => e.type === 'arangosearch')
                                    .figures;
        assertNotEqual(null, figures);
        assertTrue(Object === figures.constructor);
        assertEqual(6, Object.keys(figures).length);
        assertEqual(0, figures.indexSize);
        assertEqual(0, figures.numDocs);
        assertEqual(0, figures.numLiveDocs);
        assertEqual(0, figures.numBufferedDocs);
        assertEqual(0, figures.numFiles);
        assertEqual(0, figures.numSegments);
      }

      // ensure data is synchronized
      var res = db._query("FOR d IN TestView OPTIONS {waitForSync:true} SORT d.foo RETURN d").toArray();
      assertEqual(2, res.length);
      assertEqual('bar', res[0].foo);
      assertEqual('baz', res[1].foo);

      // check link stats
      {
        let figures = db.TestCollection.getIndexes(true, true)
                                    .find(e => e.type === 'arangosearch')
                                    .figures;
        assertNotEqual(null, figures);
        assertTrue(Object === figures.constructor);
        assertEqual(6, Object.keys(figures).length);
        assertEqual(0, figures.indexSize);
        assertEqual(0, figures.numDocs);
        assertEqual(0, figures.numLiveDocs);
        assertEqual(0, figures.numBufferedDocs);
        assertEqual(0, figures.numFiles);
        assertEqual(0, figures.numSegments);
      }

      // remove document
      col.remove(res[0]._key);

      // ensure data is synchronized
      res = db._query("FOR d IN TestView OPTIONS {waitForSync:true} SORT d.foo RETURN d").toArray();
      assertEqual(1, res.length);
      assertEqual('baz', res[0].foo);

      // check link stats
      {
        let figures = db.TestCollection.getIndexes(true, true)
                                    .find(e => e.type === 'arangosearch')
                                    .figures;
        assertNotEqual(null, figures);
        assertTrue(Object === figures.constructor);
        assertEqual(6, Object.keys(figures).length);
        assertEqual(0, figures.indexSize);
        assertEqual(0, figures.numDocs);
        assertEqual(0, figures.numLiveDocs);
        assertEqual(0, figures.numBufferedDocs);
        assertEqual(0, figures.numFiles);
        assertEqual(0, figures.numSegments);
      }

      // truncate collection
      col.truncate({ compact: false });

      // ensure data is synchronized
      res = db._query("FOR d IN TestView OPTIONS {waitForSync:true} SORT d.foo RETURN d").toArray();
      assertEqual(0, res.length);

      // check link stats
      {
        let figures = db.TestCollection.getIndexes(true, true)
                                    .find(e => e.type === 'arangosearch')
                                    .figures;
        assertNotEqual(null, figures);
        assertTrue(Object === figures.constructor);
        assertEqual(6, Object.keys(figures).length);
        assertEqual(0, figures.indexSize);
        assertEqual(0, figures.numDocs);
        assertEqual(0, figures.numLiveDocs);
        assertEqual(0, figures.numBufferedDocs);
        assertEqual(0, figures.numFiles);
        assertEqual(0, figures.numSegments);
      }
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

    testLinkWithAnalyzerFromOtherDb: function() {
      let databaseNameAnalyzer = "testDatabaseAnalyzer";
      let databaseNameView = "testDatabaseView";
      let tmpAnalyzerName = "TmpIdentity";
      let systemAnalyzerName = "SystemIdentity";
      db._useDatabase("_system");
      try { db._dropDatabase(databaseNameAnalyzer);} catch(e) {}
      try { db._dropDatabase(databaseNameView);} catch(e) {}
      analyzers.save(systemAnalyzerName, "identity");
      db._createDatabase(databaseNameAnalyzer);
      db._createDatabase(databaseNameView);
      db._useDatabase(databaseNameAnalyzer);
      let tmpIdentity = analyzers.save(tmpAnalyzerName, "identity");
      assertTrue(tmpIdentity != null);
      tmpIdentity = undefined;
      db._useDatabase(databaseNameView);
      db._create("FOO");
      try {
        db._createView("FOO_view", "arangosearch", {links:{"FOO":{analyzers:[databaseNameAnalyzer + "::" + tmpAnalyzerName]}}});
        fail();
      } catch(e) {
        assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                    e.errorNum);
      }
      // but cross-db usage of system analyzers is ok
      db._createView("FOO_view", "arangosearch", {links:{"FOO":{analyzers:["::" + systemAnalyzerName]}}});
      db._createView("FOO_view2", "arangosearch", {links:{"FOO":{analyzers:["_system::" + systemAnalyzerName]}}});

      db._useDatabase("_system");
      db._dropDatabase(databaseNameAnalyzer);
      db._dropDatabase(databaseNameView);
      analyzers.remove(systemAnalyzerName, true);
    },
    // Commented as this situation is not documented (user should not do this), will be adressed later
    //testLinkWithAnalyzerFromOtherDbByAnalyzerDefinitions: function() {
    //  let databaseNameAnalyzer = "testDatabaseAnalyzer";
    //  let databaseNameView = "testDatabaseView";

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
    /// @brief test ensure that view is deleted within deleted database
    /// Regression test for arangodb/release-3.4#153.
    ////////////////////////////////////////////////////////////////////////////
    testLeftViewInDroppedDatabase: function () {
      const dbName = 'TestDB';
      const viewName = 'TestView';
      db._useDatabase("_system");
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

 // test for public issue #9652
    testAnalyzerWithStopwordsNameConflict : function() {
      const dbName = "TestNameConflictDB";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) {}

      db._createDatabase(dbName);
      db._useDatabase(dbName);

      db._create("col1");
      db._create("col2");
      analyzers.save("custom_analyzer", 
        "text", 
        { 
          locale: "en.UTF-8", 
          case: "lower", 
          stopwords: ["the", "of", "inc", "co", "plc", "ltd", "ag"], 
          accent: false, 
          stemming: false
        },
        ["position", "norm","frequency"]);

      let v = db._createView("view1", "arangosearch", {
        "links": {
          "col1": {
            "analyzers": ["identity"],
            "fields": { "name": { "analyzers": ["custom_analyzer"]}},
            "includeAllFields": false,
            "storeValues": "none",
            "trackListPositions": false
          },
          "col2": {
            "analyzers": ["identity"],
            "fields": { "name": { "analyzers": ["custom_analyzer"]}},
            "includeAllFields": false,
            "storeValues": "none",
            "trackListPositions": false
          }
        }});
      let properties = v.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(2, Object.keys(properties.links).length);

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
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch (e) {}
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
      analyzers.save(analyzerName, "text", {"stopwords" : [], "locale":"en"});

      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testIndexAnalyzerCollection : function() {
      const dbName = "TestNameDroppedDB";
      const analyzerName = "TestAnalyzer";
      db._useDatabase("_system");
      assertNotEqual(null, db._collection("_analyzers"));
      try { db._dropDatabase(dbName); } catch (e) {}
      try { analyzers.remove(analyzerName); } catch (e) {}
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
      analyzers.save(analyzerName, "text", {"stopwords" : [], "locale":"en"});
      assertEqual(1, db._analyzers.count());

      var view = db._createView("analyzersView", "arangosearch", {
        links: {
          _analyzers : {
            includeAllFields:true,
            analyzers: [ analyzerName ]
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
    testAutoLoadAnalyzerOnViewCreation : function() {
      const dbName = "TestNameDroppedDB";
      const analyzerName = "TestAnalyzer";
      db._useDatabase("_system");
      assertNotEqual(null, db._collection("_analyzers"));
      try { db._dropDatabase(dbName); } catch (e) {}
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        db._analyzers.save({type:"identity", name: analyzerName});
        var view = db._createView("analyzersView", "arangosearch", {
          links: {
            _analyzers : {
              includeAllFields:true,
              analyzers: [ analyzerName] // test only new database access. Load from _system is checked in gtest
            }
          }
        });
        var res = db._query("FOR d IN analyzersView OPTIONS {waitForSync:true} RETURN d").toArray();
        assertEqual(1, res.length);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    testAutoLoadAnalyzerOnViewUpdate : function() {
      const dbName = "TestNameDroppedDB";
      const analyzerName = "TestAnalyzer";
      db._useDatabase("_system");
      assertNotEqual(null, db._collection("_analyzers"));
      try { db._dropDatabase(dbName); } catch (e) {}
      try {
        db._createDatabase(dbName);
        db._useDatabase(dbName);
        db._analyzers.save({type:"identity", name: analyzerName});
        var view = db._createView("analyzersView", "arangosearch", {});
        var links = {
          links: {
            _analyzers : {
              includeAllFields:true,
              analyzers: [ analyzerName] // test only new database access. Load from _system is checked in gtest
            }
          }
        };
        view.properties(links, true);
        var res = db._query("FOR d IN analyzersView OPTIONS {waitForSync:true} RETURN d").toArray();
        assertEqual(1, res.length);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(IResearchFeatureDDLTestSuite);

return jsunity.done();
