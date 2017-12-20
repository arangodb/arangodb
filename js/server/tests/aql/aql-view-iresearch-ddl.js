/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertTrue, assertFalse, AQL_EXECUTE */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function iResearchFeatureAqlTestSuite () {
  return {
    setUpAll : function () {
    },

    tearDownAll : function () {
      db._drop("TestCollection");
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchFeature tests
////////////////////////////////////////////////////////////////////////////////

    testViewDDL: function() {
      // collections
      {
        db._drop("TestCollection0");
        db._drop("TestCollection1");
        db._drop("TestCollection2");
        db._dropView("TestView");
        db._create("TestCollection0");
        db._create("TestCollection1");
        db._create("TestCollection2");
        var view = db._createView("TestView", "iresearch", {});

        var properties = view.properties();
        assertTrue(Array === properties.collections.constructor);
        assertEqual(0, properties.collections.length);

        var meta = { links: { "TestCollection0": {} } };
        view.properties(meta, true); // partial update
        properties = view.properties();
        assertTrue(Array === properties.collections.constructor);
        assertEqual(1, properties.collections.length);

        meta = { links: { "TestCollection1": {} } };
        view.properties(meta, true); // partial update
        properties = view.properties();
        assertTrue(Array === properties.collections.constructor);
        assertEqual(2, properties.collections.length);

        meta = { links: { "TestCollection2": {} } };
        view.properties(meta, false); // full update
        properties = view.properties();
        assertTrue(Array === properties.collections.constructor);
        assertEqual(1, properties.collections.length);
      }

      // commit
      {
        db._dropView("TestView");
        var view = db._createView("TestView", "iresearch", {});

        var properties = view.properties();
        assertTrue(Object === properties.commit.constructor);
        assertEqual(10, properties.commit.cleanupIntervalStep);
        assertEqual(60000, properties.commit.commitIntervalMsec);
        assertEqual(5000, properties.commit.commitTimeoutMsec);
        assertTrue(Object === properties.commit.consolidate.constructor);
        assertEqual(4, Object.keys(properties.commit.consolidate).length);
        assertTrue(Object === properties.commit.consolidate.bytes.constructor);
        assertEqual(10, properties.commit.consolidate.bytes.intervalStep);
        assertEqual((0.85).toFixed(6), properties.commit.consolidate.bytes.threshold.toFixed(6));
        assertTrue(Object === properties.commit.consolidate.bytes_accum.constructor);
        assertEqual(10, properties.commit.consolidate.bytes_accum.intervalStep);
        assertEqual((0.85).toFixed(6), properties.commit.consolidate.bytes_accum.threshold.toFixed(6));
        assertTrue(Object === properties.commit.consolidate.count.constructor);
        assertEqual(10, properties.commit.consolidate.count.intervalStep);
        assertEqual((0.85).toFixed(6), properties.commit.consolidate.count.threshold.toFixed(6));
        assertTrue(Object === properties.commit.consolidate.fill.constructor);
        assertEqual(10, properties.commit.consolidate.fill.intervalStep);
        assertEqual((0.85).toFixed(6), properties.commit.consolidate.fill.threshold.toFixed(6));

        var meta = { commit: {
          commitIntervalMsec: 10000,
          consolidate: {
            bytes: { intervalStep: 20, threshold: 0.5 },
            bytes_accum: {},
            count: {}
          }
        } };
        view.properties(meta, true); // partial update
        properties = view.properties();
        assertTrue(Object === properties.commit.constructor);
        assertEqual(10, properties.commit.cleanupIntervalStep);
        assertEqual(10000, properties.commit.commitIntervalMsec);
        assertEqual(5000, properties.commit.commitTimeoutMsec);
        assertTrue(Object === properties.commit.consolidate.constructor);
        assertEqual(3, Object.keys(properties.commit.consolidate).length);
        assertTrue(Object === properties.commit.consolidate.bytes.constructor);
        assertEqual(20, properties.commit.consolidate.bytes.intervalStep);
        assertEqual((0.5).toFixed(6), properties.commit.consolidate.bytes.threshold.toFixed(6));
        assertTrue(Object === properties.commit.consolidate.bytes_accum.constructor);
        assertEqual(10, properties.commit.consolidate.bytes_accum.intervalStep);
        assertEqual((0.85).toFixed(6), properties.commit.consolidate.bytes_accum.threshold.toFixed(6));
        assertTrue(Object === properties.commit.consolidate.count.constructor);
        assertEqual(10, properties.commit.consolidate.count.intervalStep);
        assertEqual((0.85).toFixed(6), properties.commit.consolidate.count.threshold.toFixed(6));

        meta = { commit: {
          cleanupIntervalStep: 20,
          consolidate: { count: { intervalStep: 30, threshold: 0.75 } }
        } };
        view.properties(meta, false); // full update
        properties = view.properties();
        assertTrue(Object === properties.commit.constructor);
        assertEqual(20, properties.commit.cleanupIntervalStep);
        assertEqual(60000, properties.commit.commitIntervalMsec);
        assertEqual(5000, properties.commit.commitTimeoutMsec);
        assertTrue(Object === properties.commit.consolidate.constructor);
        assertEqual(1, Object.keys(properties.commit.consolidate).length);
        assertTrue(Object === properties.commit.consolidate.count.constructor);
        assertEqual(30, properties.commit.consolidate.count.intervalStep);
        assertEqual((0.75).toFixed(6), properties.commit.consolidate.count.threshold.toFixed(6));
      }

      // data path
      {
        db._dropView("TestView");
        var view = db._createView("TestView", "iresearch", {});

        var properties = view.properties();
        assertTrue(String === properties.dataPath.constructor);
        assertTrue(properties.dataPath.length > 0);

        var meta = { dataPath: "TestPath" };
        view.properties(meta);
        properties = view.properties();
        assertTrue(String === properties.dataPath.constructor);
        assertTrue(properties.dataPath.length > 0);
        assertEqual("TestPath", properties.dataPath);
      }

      // locale
      {
        db._dropView("TestView");
        var view = db._createView("TestView", "iresearch", {});

        var properties = view.properties();
        assertTrue(String === properties.dataPath.constructor);
        assertTrue(properties.locale.length > 0);

        var meta = { locale: "de_DE.UTF-16" };
        view.properties(meta);
        properties = view.properties();
        assertTrue(String === properties.locale.constructor);
        assertTrue(properties.locale.length > 0);
        assertEqual("de_DE.UTF-8", properties.locale);
      }

      // threads max idle/total
      {
        db._dropView("TestView");
        var view = db._createView("TestView", "iresearch", {});

        var properties = view.properties();
        assertTrue(Number === properties.threadsMaxIdle.constructor);
        assertEqual(5, properties.threadsMaxIdle);
        assertTrue(Number === properties.threadsMaxTotal.constructor);
        assertEqual(5, properties.threadsMaxTotal);

        var meta = { threadsMaxIdle: 42 };
        view.properties(meta, true); // partial update
        properties = view.properties();
        assertTrue(Number === properties.threadsMaxIdle.constructor);
        assertEqual(42, properties.threadsMaxIdle);
        assertTrue(Number === properties.threadsMaxTotal.constructor);
        assertEqual(5, properties.threadsMaxTotal);

        meta = { threadsMaxTotal: 1 };
        view.properties(meta, true); // partial update
        properties = view.properties();
        assertTrue(Number === properties.threadsMaxIdle.constructor);
        assertEqual(42, properties.threadsMaxIdle);
        assertTrue(Number === properties.threadsMaxTotal.constructor);
        assertEqual(1, properties.threadsMaxTotal);

        meta = { threadsMaxIdle: 0 };
        view.properties(meta, false); // full update
        properties = view.properties();
        assertTrue(Number === properties.threadsMaxIdle.constructor);
        assertEqual(0, properties.threadsMaxIdle);
        assertTrue(Number === properties.threadsMaxTotal.constructor);
        assertEqual(5, properties.threadsMaxTotal);
      }
    },

    testLinkDDL: function() {
      db._drop("TestCollection0");
      db._drop("TestCollection1");
      db._drop("TestCollection2");
      db._dropView("TestView");
      db._create("TestCollection0");
      db._create("TestCollection1");
      db._create("TestCollection2");
      var view = db._createView("TestView", "iresearch", {});

      var meta = { links: {
        "TestCollection0": {},
        "TestCollection1": { boost: 7, includeAllFields: true, trackListPositions: true, tokenizers: [ "text_en"] },
        "TestCollection2": { boost: 42, fields: {
          "a": { boost: 43 },
          "b": { fields: { "b1": {} } },
          "c": { includeAllFields: true },
          "d": { trackListPositions: true },
          "e": { tokenizers: [ "text_de"] }
        } }
      } };
      view.properties(meta, true); // partial update
      var properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(3, Object.keys(properties.links).length);

      assertTrue(Object === properties.links.TestCollection0.constructor);
      assertTrue(Number === properties.links.TestCollection0.boost.constructor);
      assertEqual((1.0).toFixed(6), properties.links.TestCollection0.boost.toFixed(6));
      assertTrue(Object === properties.links.TestCollection0.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection0.fields).length);
      assertTrue(Boolean === properties.links.TestCollection0.includeAllFields.constructor);
      assertEqual(false, properties.links.TestCollection0.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection0.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection0.trackListPositions);
      assertTrue(Array === properties.links.TestCollection0.tokenizers.constructor);
      assertEqual(1, properties.links.TestCollection0.tokenizers.length);
      assertTrue(String === properties.links.TestCollection0.tokenizers[0].constructor);
      assertEqual("identity", properties.links.TestCollection0.tokenizers[0]);


      assertTrue(Object === properties.links.TestCollection1.constructor);
      assertTrue(Number === properties.links.TestCollection1.boost.constructor);
      assertEqual((7.0).toFixed(6), properties.links.TestCollection1.boost.toFixed(6));
      assertTrue(Object === properties.links.TestCollection1.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection1.fields).length);
      assertTrue(Boolean === properties.links.TestCollection1.includeAllFields.constructor);
      assertEqual(true, properties.links.TestCollection1.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection1.trackListPositions.constructor);
      assertEqual(true, properties.links.TestCollection1.trackListPositions);
      assertTrue(Array === properties.links.TestCollection1.tokenizers.constructor);
      assertEqual(1, properties.links.TestCollection1.tokenizers.length);
      assertTrue(String === properties.links.TestCollection1.tokenizers[0].constructor);
      assertEqual("text_en", properties.links.TestCollection1.tokenizers[0]);


      assertTrue(Object === properties.links.TestCollection2.constructor);
      assertTrue(Number === properties.links.TestCollection2.boost.constructor);

      assertEqual((42.0).toFixed(6), properties.links.TestCollection2.boost.toFixed(6));
      assertTrue(Object === properties.links.TestCollection2.fields.constructor);
      assertEqual(5, Object.keys(properties.links.TestCollection2.fields).length);

      assertTrue(Number === properties.links.TestCollection2.fields.a.boost.constructor);
      assertEqual((43.0).toFixed(6), properties.links.TestCollection2.fields.a.boost.toFixed(6));

      assertTrue(Object === properties.links.TestCollection2.fields.b.fields.constructor);
      assertEqual(1, Object.keys(properties.links.TestCollection2.fields.b.fields).length);

      assertTrue(Boolean === properties.links.TestCollection2.fields.c.includeAllFields.constructor);
      assertEqual(true, properties.links.TestCollection2.fields.c.includeAllFields);

      assertTrue(Boolean === properties.links.TestCollection2.fields.d.trackListPositions.constructor);
      assertEqual(true, properties.links.TestCollection2.fields.d.trackListPositions);

      assertTrue(Array === properties.links.TestCollection2.fields.e.tokenizers.constructor);
      assertEqual(1, properties.links.TestCollection2.fields.e.tokenizers.length);
      assertTrue(String === properties.links.TestCollection2.fields.e.tokenizers[0].constructor);
      assertEqual("text_de", properties.links.TestCollection2.fields.e.tokenizers[0]);

      assertTrue(Boolean === properties.links.TestCollection2.includeAllFields.constructor);
      assertEqual(false, properties.links.TestCollection2.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection2.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection2.trackListPositions);
      assertTrue(Array === properties.links.TestCollection2.tokenizers.constructor);
      assertEqual(1, properties.links.TestCollection2.tokenizers.length);
      assertTrue(String === properties.links.TestCollection2.tokenizers[0].constructor);
      assertEqual("identity", properties.links.TestCollection2.tokenizers[0]);

      meta = { links: { "TestCollection0": null, "TestCollection2": {} } };
      view.properties(meta, true); // partial update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(2, Object.keys(properties.links).length);

      assertTrue(Object === properties.links.TestCollection1.constructor);
      assertTrue(Number === properties.links.TestCollection1.boost.constructor);
      assertEqual((7.0).toFixed(6), properties.links.TestCollection1.boost.toFixed(6));
      assertTrue(Object === properties.links.TestCollection1.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection1.fields).length);
      assertTrue(Boolean === properties.links.TestCollection1.includeAllFields.constructor);
      assertEqual(true, properties.links.TestCollection1.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection1.trackListPositions.constructor);
      assertEqual(true, properties.links.TestCollection1.trackListPositions);
      assertTrue(Array === properties.links.TestCollection1.tokenizers.constructor);
      assertEqual(1, properties.links.TestCollection1.tokenizers.length);
      assertTrue(String === properties.links.TestCollection1.tokenizers[0].constructor);
      assertEqual("text_en", properties.links.TestCollection1.tokenizers[0]);


      assertTrue(Object === properties.links.TestCollection2.constructor);
      assertTrue(Number === properties.links.TestCollection2.boost.constructor);
      assertEqual((1.0).toFixed(6), properties.links.TestCollection2.boost.toFixed(6));
      assertTrue(Object === properties.links.TestCollection2.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection2.fields).length);
      assertTrue(Boolean === properties.links.TestCollection2.includeAllFields.constructor);
      assertEqual(false, properties.links.TestCollection2.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection2.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection2.trackListPositions);
      assertTrue(Array === properties.links.TestCollection2.tokenizers.constructor);
      assertEqual(1, properties.links.TestCollection2.tokenizers.length);
      assertTrue(String === properties.links.TestCollection2.tokenizers[0].constructor);
      assertEqual("identity", properties.links.TestCollection2.tokenizers[0]);

      meta = { links: { "TestCollection0": { includeAllFields: true }, "TestCollection1": {} } };
      view.properties(meta, false); // full update
      properties = view.properties();
      assertTrue(Object === properties.links.constructor);
      assertEqual(2, Object.keys(properties.links).length);

      assertTrue(Object === properties.links.TestCollection0.constructor);
      assertTrue(Number === properties.links.TestCollection0.boost.constructor);
      assertEqual((1.0).toFixed(6), properties.links.TestCollection0.boost.toFixed(6));
      assertTrue(Object === properties.links.TestCollection0.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection0.fields).length);
      assertTrue(Boolean === properties.links.TestCollection0.includeAllFields.constructor);
      assertEqual(true, properties.links.TestCollection0.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection0.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection0.trackListPositions);
      assertTrue(Array === properties.links.TestCollection0.tokenizers.constructor);
      assertEqual(1, properties.links.TestCollection0.tokenizers.length);
      assertTrue(String === properties.links.TestCollection0.tokenizers[0].constructor);
      assertEqual("identity", properties.links.TestCollection0.tokenizers[0]);


      assertTrue(Object === properties.links.TestCollection1.constructor);
      assertTrue(Number === properties.links.TestCollection1.boost.constructor);
      assertEqual((1.0).toFixed(6), properties.links.TestCollection1.boost.toFixed(6));
      assertTrue(Object === properties.links.TestCollection1.fields.constructor);
      assertEqual(0, Object.keys(properties.links.TestCollection1.fields).length);
      assertTrue(Boolean === properties.links.TestCollection1.includeAllFields.constructor);
      assertEqual(false, properties.links.TestCollection1.includeAllFields);
      assertTrue(Boolean === properties.links.TestCollection1.trackListPositions.constructor);
      assertEqual(false, properties.links.TestCollection1.trackListPositions);
      assertTrue(Array === properties.links.TestCollection1.tokenizers.constructor);
      assertEqual(1, properties.links.TestCollection1.tokenizers.length);
      assertTrue(String === properties.links.TestCollection1.tokenizers[0].constructor);
      assertEqual("identity", properties.links.TestCollection1.tokenizers[0]);
    },

    testViewCreate: function() {
      // 1 empty collection
      {
        db._dropView("TestView");
        db._drop("TestCollection0");
        var col0 = db._create("TestCollection0");
        var view = db._createView("TestView", "iresearch", {});

        var meta = { links: { "TestCollection0": { includeAllFields: true } } };
        view.properties(meta, true); // partial update

        var result = AQL_EXECUTE("FOR doc IN VIEW TestView SORT doc.name RETURN doc", null, { waitForSync: true }).json;
        assertEqual(0, result.length);

        col0.save({ name: "quarter", text: "quick over" });
        result = AQL_EXECUTE("FOR doc IN VIEW TestView SORT doc.name RETURN doc", null, { waitForSync: true }).json;
        assertEqual(1, result.length);
        assertEqual("quarter", result[0].name);
      }

      // 1 non-empty collection
      {
        db._dropView("TestView");
        db._drop("TestCollection0");
        var col0 = db._create("TestCollection0");
        var view = db._createView("TestView", "iresearch", {});

        col0.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
        col0.save({ name: "half", text: "quick fox over lazy" });
        col0.save({ name: "other half", text: "the brown jumps the dog" });
        col0.save({ name: "quarter", text: "quick over" });

        var meta = { links: { "TestCollection0": { includeAllFields: true } } };
        view.properties(meta, true); // partial update

        var result = AQL_EXECUTE("FOR doc IN VIEW TestView SORT doc.name RETURN doc", null, { waitForSync: true }).json;
        assertEqual(4, result.length);
        assertEqual("full", result[0].name);
        assertEqual("half", result[1].name);
        assertEqual("other half", result[2].name);
        assertEqual("quarter", result[3].name);
      }

      // 2 non-empty collections
      {
        db._dropView("TestView");
        db._drop("TestCollection0");
        db._drop("TestCollection1");
        var col0 = db._create("TestCollection0");
        var col1 = db._create("TestCollection1");
        var view = db._createView("TestView", "iresearch", {});

        col0.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
        col0.save({ name: "half", text: "quick fox over lazy" });
        col1.save({ name: "other half", text: "the brown jumps the dog" });
        col1.save({ name: "quarter", text: "quick over" });

        var meta = { links: {
          "TestCollection0": { includeAllFields: true },
          "TestCollection1": { includeAllFields: true }
        } };
        view.properties(meta, true); // partial update

        var result = AQL_EXECUTE("FOR doc IN VIEW TestView SORT doc.name RETURN doc", null, { waitForSync: true }).json;
        assertEqual(4, result.length);
        assertEqual("full", result[0].name);
        assertEqual("half", result[1].name);
        assertEqual("other half", result[2].name);
        assertEqual("quarter", result[3].name);
      }

      // 1 empty collection + 2 non-empty collections
      {
        db._dropView("TestView");
        db._drop("TestCollection0");
        db._drop("TestCollection1");
        db._drop("TestCollection2");
        var col0 = db._create("TestCollection0");
        var col1 = db._create("TestCollection1");
        var col2 = db._create("TestCollection2");
        var view = db._createView("TestView", "iresearch", {});

        col2.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
        col2.save({ name: "half", text: "quick fox over lazy" });
        col0.save({ name: "other half", text: "the brown jumps the dog" });
        col0.save({ name: "quarter", text: "quick over" });

        var meta = { links: {
          "TestCollection0": { includeAllFields: true },
          "TestCollection1": { includeAllFields: true },
          "TestCollection2": { includeAllFields: true }
        } };
        view.properties(meta, true); // partial update

        var result = AQL_EXECUTE("FOR doc IN VIEW TestView SORT doc.name RETURN doc", null, { waitForSync: true }).json;
        assertEqual(4, result.length);
        assertEqual("full", result[0].name);
        assertEqual("half", result[1].name);
        assertEqual("other half", result[2].name);
        assertEqual("quarter", result[3].name);
      }
    },

    testViewModify: function() {
      // FIXME TODO implement for 1/2/3 empty/non-empty collections (change view properties)
    },

    testLinkModify: function() {
      // FIXME TODO recreate link with different properties on non-empty collections (query docs to verify)
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(iResearchFeatureAqlTestSuite);

return jsunity.done();
