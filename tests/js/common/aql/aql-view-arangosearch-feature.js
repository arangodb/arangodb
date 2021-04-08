/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertNotUndefined, assertNotEqual, assertEqual, assertTrue, assertFalse, assertNull, assertNotNull, fail, db._query */

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
const arango = require('@arangodb').arango;
const internal = require('internal');
const isCluster = internal.isCluster();
const isEnterprise = internal.isEnterprise();
////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function iResearchFeatureAqlTestSuite () {
  let cleanup = function() {
    db._useDatabase("_system");
    db._analyzers.toArray().forEach(function(analyzer) {
      try { analyzers.remove(analyzer.name, true); } catch (err) {}
    });
    assertEqual(0, db._analyzers.count(), db._analyzers.toArray());
  };

  return {
    setUp : function () {
      cleanup();
    },

    tearDown : function () {
      cleanup();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchAnalyzerFeature tests
////////////////////////////////////////////////////////////////////////////////
    testAnalyzersCollectionPresent: function() {
      let dbName = "analyzersCollTestDb";
      try { 
        db._dropDatabase(dbName); 
      } catch (e) {}

      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        assertNotNull(db._collection("_analyzers"));
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },

    testAnalyzersInvalidPropertiesDiscarded : function() {
      {
        try {analyzers.remove("normPropAnalyzer"); } catch (e) {}
        let oldCount = db._analyzers.count();
        let analyzer = analyzers.save("normPropAnalyzer", "norm", { "locale":"en", "invalid_param":true});
        try {
          assertEqual(oldCount + 1, db._analyzers.count());
          assertNotNull(analyzer);
          assertUndefined(analyzer.properties.invalid_param);
        } finally {
          analyzers.remove("normPropAnalyzer", true);
        }
        assertEqual(oldCount, db._analyzers.count());
      }
      {
        try {analyzers.remove("textPropAnalyzer"); } catch (e) {}
        let oldCount = db._analyzers.count();
        let analyzer = analyzers.save("textPropAnalyzer", "text", {"stopwords" : [], "locale":"en", "invalid_param":true});
        try {
          assertEqual(oldCount + 1, db._analyzers.count());
          assertNotNull(analyzer);
          assertUndefined(analyzer.properties.invalid_param);
        } finally {
          analyzers.remove("textPropAnalyzer", true);
        }
        assertEqual(oldCount, db._analyzers.count());
      }
      {
        try {analyzers.remove("textPropAnalyzerWithNgram"); } catch (e) {}
        let oldCount = db._analyzers.count();
        let analyzer = analyzers.save("textPropAnalyzerWithNgram", "text", {"stopwords" : [], "locale":"en", "edgeNgram" : { "min" : 2, "invalid_param":true}});
        try {
          assertEqual(oldCount + 1, db._analyzers.count());
          assertNotNull(analyzer);
          assertUndefined(analyzer.properties.invalid_param);
        } finally {
          analyzers.remove("textPropAnalyzerWithNgram", true);
        }
        assertEqual(oldCount, db._analyzers.count());
      }
      {
        try {analyzers.remove("delimiterPropAnalyzer"); } catch (e) {}
        let oldCount = db._analyzers.count();
        let analyzer = analyzers.save("delimiterPropAnalyzer", "delimiter", { "delimiter":"|", "invalid_param":true});
        try {
          assertEqual(oldCount + 1, db._analyzers.count());
          assertNotNull(analyzer);
          assertUndefined(analyzer.properties.invalid_param);
        } finally {
          analyzers.remove("delimiterPropAnalyzer", true);
        }
        assertEqual(oldCount, db._analyzers.count());
      }
      {
        try {analyzers.remove("stemPropAnalyzer"); } catch (e) {}
        let oldCount = db._analyzers.count();
        let analyzer = analyzers.save("stemPropAnalyzer", "stem", { "locale":"en", "invalid_param":true});
        try {
          assertEqual(oldCount + 1, db._analyzers.count());
          assertNotNull(analyzer);
          assertUndefined(analyzer.properties.invalid_param);
        } finally {
          analyzers.remove("stemPropAnalyzer", true);
        }
        assertEqual(oldCount, db._analyzers.count());
      }
      {
        try {analyzers.remove("ngramPropAnalyzer"); } catch (e) {}
        let oldCount = db._analyzers.count();
        let analyzer = analyzers.save("ngramPropAnalyzer", "ngram", { "min":1, "max":5, "preserveOriginal":true, "invalid_param":true});
        try {
          assertEqual(oldCount + 1, db._analyzers.count());
          assertNotNull(analyzer);
          assertUndefined(analyzer.properties.invalid_param);
        } finally {
          analyzers.remove("ngramPropAnalyzer", true);
        }
        assertEqual(oldCount, db._analyzers.count());
      }
    },

    testAnalyzerRemovalWithDatabaseName_InSystem: function() {
      let dbName = "analyzerWrongDbName1";
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      try {
        try {
          db._useDatabase(dbName);
          analyzers.save("MyTrigram", "ngram", { min: 2, max: 3, preserveOriginal: true });
          db._useDatabase("_system");
          analyzers.remove(dbName + "::MyTrigram");
          fail(); // removal with db name in wrong current used db should also fail
        } catch(e) {
          assertEqual(require("internal").errors.ERROR_FORBIDDEN .code,
                         e.errorNum);
        } finally { 
          db._useDatabase(dbName);
          analyzers.remove(dbName + "::MyTrigram", true);
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    testAnalyzerCreateRemovalWithDatabaseName_InDb: function() {
      let dbName = "analyzerWrongDbName2";
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      try {
        try {
          analyzers.save(dbName + "::MyTrigram", "ngram", { min: 2, max: 3, preserveOriginal: true });
          fail();
        } catch(e) {
          assertEqual(require("internal").errors.ERROR_FORBIDDEN.code,
                         e.errorNum);
        }
        db._useDatabase(dbName);
        analyzers.save(dbName + "::MyTrigram", "ngram", { min: 2, max: 3, preserveOriginal: true }); 
        analyzers.remove(dbName + "::MyTrigram", true); 
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    testAnalyzerUseOnDBServer_InDb: function() {
      let dbName = "analyzerUseOnDbServer";
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        analyzers.save("MyTrigram", "ngram", { min: 2, max: 3, preserveOriginal: true });

        // NOOPT guarantees that TOKENS function will be executed on DB server
        let res = db._query("FOR d IN _analyzers FILTER NOOPT(LENGTH(TOKENS('foobar', 'MyTrigram')) > 0) RETURN d").toArray();
        assertEqual(1, res.length);
        assertEqual("_analyzers/MyTrigram", res[0]._id);
        assertEqual("MyTrigram", res[0]._key);
        assertEqual("MyTrigram", res[0].name);
        assertEqual("ngram", res[0].type);
        assertEqual(6, Object.keys(res[0].properties).length);
        assertEqual("", res[0].properties.startMarker);
        assertEqual("", res[0].properties.endMarker);
        assertEqual("binary", res[0].properties.streamType);
        assertEqual(2, res[0].properties.min);
        assertEqual(3, res[0].properties.max);
        assertTrue(res[0].properties.preserveOriginal);
        assertTrue(Array === res[0].features.constructor);
        assertEqual(0, res[0].features.length);
        assertEqual([ ], res[0].features);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    testAnalyzerCreateRemovalWithDatabaseName_InSystem: function() {
      let dbName = "analyzerWrongDbName3";
      try { db._dropDatabase(dbName); } catch (e) {}
      try { analyzers.remove("MyTrigram"); } catch (e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        // cross-db request should fail
        try {
          analyzers.save("::MyTrigram", "ngram", { min: 2, max: 3, preserveOriginal: true });
          fail();
        } catch(e) {
          assertEqual(require("internal").errors.ERROR_FORBIDDEN.code,
                         e.errorNum);
        }
        try {
          analyzers.save("_system::MyTrigram", "ngram", { min: 2, max: 3, preserveOriginal: true });
          fail();
        } catch(e) {
          assertEqual(require("internal").errors.ERROR_FORBIDDEN.code,
                         e.errorNum);
        }
        // in _system db requests should be ok
        db._useDatabase("_system");
        analyzers.save("::MyTrigram", "ngram", { min: 2, max: 3, preserveOriginal: true }); 
        analyzers.remove("::MyTrigram", true); 
        analyzers.save("_system::MyTrigram", "ngram", { min: 2, max: 3, preserveOriginal: true }); 
        analyzers.remove("_system::MyTrigram", true); 
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
        try {analyzers.remove("::MyTrigram", true); } catch (e) {} 
      }
    },
    testAnalyzerInvalidName: function() {
      let dbName = "analyzerWrongDbName4";
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        try {
          // invalid ':' in name
          analyzers.save(dbName + "::MyTr:igram", "ngram", { min: 2, max: 3, preserveOriginal: true });
          fail();
        } catch(e) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      e.errorNum);
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    testAnalyzerGetFromOtherDatabase: function() {
      let dbName = "analyzerDbName";
      let anotherDbName = "anotherDbName";
      try { db._dropDatabase(dbName); } catch (e) {}
      try { db._dropDatabase(anotherDbName); } catch (e) {}
      db._createDatabase(dbName);
      try {
        db._createDatabase(anotherDbName);
        try {
          db._useDatabase(dbName);
          let analyzer = analyzers.save("MyTrigram", "ngram", { min: 2, max: 3, preserveOriginal: true });
          assertNotNull(analyzer);
          db._useDatabase(anotherDbName);
          try {
            analyzers.analyzer(dbName + "::MyTrigram");
            fail();
          } catch(e) {
            assertEqual(require("internal").errors.ERROR_FORBIDDEN .code,
                           e.errorNum);
          }
        } finally {
          db._useDatabase("_system");
          db._dropDatabase(anotherDbName);
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    testAnalyzerGetFromSystemDatabaseDifferentRevision: function() {
      if (!isCluster) { // only cluster has revisions. Do not waste time on single
       return;
      }
      let dbName = "analyzerDbName";
      let analyzerName = "my_identity";
      try { db._dropDatabase(dbName); } catch (e) {}
      try { analyzers.remove(analyzerName, true); } catch (e) {}
      db._createDatabase(dbName, {sharding: "single"});
      try {
        let analyzer = analyzers.save(analyzerName, "identity", {}); // so system revision is at least 1
        try {
          db._useDatabase(dbName); // fresh database will have revision 0 ( 0 < 1 so using db revision for system analyzer will fail!)
          db._create("test_coll"); 
          db._createView("tv", "arangosearch", {links: { test_coll: { includeAllFields:true, analyzers:[ "::" + analyzerName ] } } });
          db.test_coll.save({field: "value1"});
          var res = db._query("FOR d IN tv SEARCH ANALYZER(d.field == 'value1', '::" + analyzerName + "') OPTIONS {waitForSync:true}  RETURN d");
          assertEqual(1, res.toArray().length);
        } finally {
          db._useDatabase("_system");
          analyzers.remove(analyzerName, true);
        }
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    testAnalyzers: function() {
      let oldList = analyzers.toArray();
      let oldListInCollection = db._analyzers.toArray();
      assertTrue(Array === oldList.constructor);

      assertEqual(0, db._analyzers.count(), db._analyzers.toArray());
      // creation
      analyzers.save("testAnalyzer", "stem", { "locale":"en"}, [ "frequency" ]);

      // properties
      let analyzer = analyzers.analyzer(db._name() + "::testAnalyzer");
      assertNotNull(analyzer);
      assertEqual(db._name() + "::testAnalyzer", analyzer.name());
      assertEqual("stem", analyzer.type());
      assertEqual(1, Object.keys(analyzer.properties()).length);
      assertEqual("en", analyzer.properties().locale);
      assertTrue(Array === analyzer.features().constructor);
      assertEqual(1, analyzer.features().length);
      assertEqual([ "frequency" ], analyzer.features());

      // check persisted analyzer
      let savedAnalyzer = db._analyzers.toArray()[0];
      assertNotNull(savedAnalyzer);
      assertEqual(8, Object.keys(savedAnalyzer).length);
      assertEqual("_analyzers/testAnalyzer", savedAnalyzer._id);
      assertEqual("testAnalyzer", savedAnalyzer._key);
      assertEqual("testAnalyzer", savedAnalyzer.name);
      assertEqual("stem", savedAnalyzer.type);
      assertNotUndefined(savedAnalyzer.revision);
      if(isCluster) {
        assertNotEqual(0, savedAnalyzer.revision); // we have added analyzer so revision should increment at least once
      } else {
        assertEqual(0, savedAnalyzer.revision); // for single server it is always 0
      }
      assertEqual(analyzer.properties(), savedAnalyzer.properties);
      assertEqual(analyzer.features(), savedAnalyzer.features);

      analyzer = undefined; // release reference 

      // check the analyzers collection in database
      assertEqual(oldListInCollection.length + 1, db._analyzers.toArray().length);
      let dbAnalyzer = db._query("FOR d in _analyzers FILTER d.name=='testAnalyzer' RETURN d").toArray();
      assertEqual(1, dbAnalyzer.length);
      assertEqual("_analyzers/testAnalyzer", dbAnalyzer[0]._id);
      assertEqual("testAnalyzer", dbAnalyzer[0]._key);
      assertEqual("testAnalyzer", dbAnalyzer[0].name);
      assertEqual("stem", dbAnalyzer[0].type);
      assertEqual(1, Object.keys(dbAnalyzer[0].properties).length);
      assertEqual("en", dbAnalyzer[0].properties.locale);
      assertTrue(Array === dbAnalyzer[0].features.constructor);
      assertEqual(1, dbAnalyzer[0].features.length);
      assertEqual([ "frequency" ], dbAnalyzer[0].features);
      dbAnalyzer = undefined;

      // listing
      let list = analyzers.toArray();
      assertTrue(Array === list.constructor);
      assertEqual(oldList.length + 1, list.length);

      list = undefined; // release reference

      // force server-side V8 garbage collection
      if (db._connection !== undefined) { // client test
        arango.POST('/_admin/execute?returnAsJSON=true', 'require("internal").wait(0.1, true);');
      } else {
        require("internal").wait(0.1, true);
      }

      // removal
      analyzers.remove("testAnalyzer");
      assertNull(analyzers.analyzer(db._name() + "::testAnalyzer"));
      assertEqual(oldList.length, analyzers.toArray().length);
      // check the analyzers collection in database
      assertEqual(oldListInCollection.length, db._analyzers.toArray().length);
    },

    testAnalyzersFeatures: function() {
      try {
        analyzers.save("testAnalyzer", "identity", {}, [ "unknown" ]);
        fail(); // unsupported feature
      } catch(e) {
        assertTrue(e instanceof TypeError ||
                   require("internal").errors.ERROR_BAD_PARAMETER.code === e.errorNum);
      }

      try {
        analyzers.save("testAnalyzer", "identity", {}, [ "position" ]);
        fail(); // feature with dependency
      } catch(e) {
        assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                    e.errorNum);
      }

      // feature with dependency satisfied
      analyzers.save("testAnalyzer", "identity", {}, [ "frequency", "position" ]);
      analyzers.remove("testAnalyzer", true);
    },

    testAnalyzersPrefix: function() {
      let dbName = "TestDB";
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);

        let oldList = analyzers.toArray();
        assertTrue(Array === oldList.constructor);

        // creation
        db._useDatabase("_system");
        analyzers.save("testAnalyzer", "identity", {}, [ "frequency" ]);
        db._useDatabase(dbName);
        analyzers.save("testAnalyzer", "identity", {}, [ "norm" ]);

        // retrieval (dbName)
        db._useDatabase(dbName);

        {
          let analyzer = analyzers.analyzer("testAnalyzer");
          assertNotNull(analyzer);
          assertEqual(db._name() + "::testAnalyzer", analyzer.name());
          assertEqual("identity", analyzer.type());
          assertEqual(0, Object.keys(analyzer.properties()).length);
          assertTrue(Array === analyzer.features().constructor);
          assertEqual(1, analyzer.features().length);
          assertEqual([ "norm" ], analyzer.features());
        }

        {
          let analyzer = analyzers.analyzer("::testAnalyzer");
          assertNotNull(analyzer);
          assertEqual("_system::testAnalyzer", analyzer.name());
          assertEqual("identity", analyzer.type());
          assertEqual(0, Object.keys(analyzer.properties()).length);
          assertTrue(Array === analyzer.features().constructor);
          assertEqual(1, analyzer.features().length);
          assertEqual([ "frequency" ], analyzer.features());
        }

        // listing
        let list = analyzers.toArray();
        assertTrue(Array === list.constructor);
        assertEqual(oldList.length + 2, list.length);

        // removal
        analyzers.remove("testAnalyzer", true);
        assertNull(analyzers.analyzer("testAnalyzer"));
        db._useDatabase("_system");
        analyzers.remove("testAnalyzer", true);
        db._useDatabase(dbName); // switch back to check analyzer with global name
        assertNull(analyzers.analyzer("::testAnalyzer"));
        assertEqual(oldList.length, analyzers.toArray().length);
      } finally {
        db._useDatabase(dbName);
        try { analyzers.remove("testAnalyzer", true); } catch (e) {}
        db._useDatabase("_system");
        try { analyzers.remove("testAnalyzer", true); } catch (e) {}
        db._dropDatabase(dbName);
      }
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief OneShard analyzers loading tests
////////////////////////////////////////////////////////////////////////////////
    testAnalyzerGetFromSameDatabaseOneShard: function() {
      if (!isEnterprise || !isCluster) {
       return;
      }
      let dbName = "analyzerDbName";
      let analyzerName = "my_identity";
      try { db._dropDatabase(dbName); } catch (e) {}
      db._createDatabase(dbName, {sharding: "single"});
      try {
        db._useDatabase(dbName);
        let analyzer = analyzers.save(analyzerName, "identity", {});
        db._create("test_coll");
        db._createView("tv", "arangosearch", 
                       {links: { test_coll: { includeAllFields:true,
                                              analyzers:[ "" + analyzerName ] } } });
        db.test_coll.save({"field": "value1"});
        var res = db._query("FOR d IN tv SEARCH ANALYZER(d.field == 'value1', '" + analyzerName + 
                            "') OPTIONS {waitForSync:true}  RETURN d");
        assertEqual(1, res.toArray().length);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief IResearchFeature tests
////////////////////////////////////////////////////////////////////////////////
    testTokensFunctions : function() {
      // null argument
      {
        let result = db._query(
          "RETURN TOKENS(null)",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(1, result[0].length);
        assertEqual([""], result[0]);
      } 
      // array of strings 
      {
        let result = db._query(
          "RETURN TOKENS(['a quick brown fox jumps', 'jumps over lazy dog'], 'text_en')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(2, result[0].length);
        assertTrue(Array === result[0][0].constructor);
        assertTrue(Array === result[0][1].constructor);
        assertEqual(5, result[0][0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0][0]);
        assertEqual(4, result[0][1].length);
        assertEqual([ "jump", "over", "lazi", "dog" ], result[0][1]);
      }
      // array of arrays of strings 
      {
        let result = db._query(
          "RETURN TOKENS([['a quick brown fox jumps', 'jumps over lazy dog'], " + 
          "['may the force be with you', 'yet another brick'] ], 'text_en')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(2, result[0].length);

        assertTrue(Array === result[0][0].constructor);
        assertEqual(2, result[0][0].length);
        assertTrue(Array === result[0][0][0].constructor);
        assertEqual(5, result[0][0][0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0][0][0]);
        assertEqual(4, result[0][0][1].length);
        assertEqual([ "jump", "over", "lazi", "dog" ], result[0][0][1]);
        assertEqual(2, result[0][1].length);
        assertEqual(6, result[0][1][0].length);
        assertEqual([ "may", "the", "forc", "be", "with", "you" ], result[0][1][0]);
        assertEqual(3, result[0][1][1].length);
        assertEqual([ "yet", "anoth", "brick" ], result[0][1][1]);
      }
      // deep array
      {
        let result = db._query(
          "RETURN TOKENS([[[[[['a quick brown fox jumps', 'jumps over lazy dog']]]]]], 'text_en')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertEqual(1, result[0].length);
        assertEqual(1, result[0][0].length);
        assertEqual(1, result[0][0][0].length);
        assertEqual(1, result[0][0][0][0].length);
        assertEqual(1, result[0][0][0][0][0].length);
        assertEqual(2, result[0][0][0][0][0][0].length);
        assertTrue(Array === result[0][0][0][0][0][0][0].constructor);
        assertTrue(Array === result[0][0][0][0][0][0][1].constructor);
        assertEqual(5, result[0][0][0][0][0][0][0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0][0][0][0][0][0][0]);
        assertEqual(4, result[0][0][0][0][0][0][1].length);
        assertEqual([ "jump", "over", "lazi", "dog" ], result[0][0][0][0][0][0][1]);
      }
      // number
      {
         let result = db._query(
          "RETURN TOKENS(3.14)",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(["oMAJHrhR64Uf", "sMAJHrhR6w==", "wMAJHrg=", "0MAJ"], result[0]);
      }
      // array of numbers
      {
         let result = db._query(
          "RETURN TOKENS([1, 2, 3.14])",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(3, result[0].length);
        assertEqual([
          ["oL/wAAAAAAAA", "sL/wAAAAAA==", "wL/wAAA=", "0L/w"],
          ["oMAAAAAAAAAA", "sMAAAAAAAA==", "wMAAAAA=", "0MAA"],
          ["oMAJHrhR64Uf", "sMAJHrhR6w==", "wMAJHrg=", "0MAJ"]], result[0]);
      }
      // bool
      {
         let result = db._query(
          "RETURN TOKENS(true)",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(["/w=="], result[0]);
      }
      // array of bools
      {
         let result = db._query(
          "RETURN TOKENS([true, false, true])",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(3, result[0].length);
        assertEqual(["/w=="], result[0][0]);
        assertEqual(["AA=="], result[0][1]);
        assertEqual(["/w=="], result[0][2]);
      }
      // mix of different types
      {
        let result = db._query(
          "RETURN TOKENS(['quick fox', null, true, 3.14, 'string array', 5, [true, 4, 'one two'], true], 'text_en')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(8, result[0].length);
        assertEqual(['quick', 'fox'], result[0][0]);
        assertEqual([""], result[0][1]);
        assertEqual(["/w=="], result[0][2]);
        assertEqual(["oMAJHrhR64Uf", "sMAJHrhR6w==", "wMAJHrg=", "0MAJ"], result[0][3]);
        assertEqual(['string', 'array'], result[0][4]);
        assertEqual(["oMAUAAAAAAAA", "sMAUAAAAAA==", "wMAUAAA=", "0MAU"], result[0][5]);
        assertTrue(Array === result[0][6].constructor);
        assertEqual(3, result[0][6].length);
        assertEqual(["/w=="], result[0][6][0]);
        assertEqual(["oMAQAAAAAAAA", "sMAQAAAAAA==", "wMAQAAA=", "0MAQ"], result[0][6][1]);
        assertEqual(['one', 'two'], result[0][6][2]);
        assertEqual(["/w=="], result[0][7]);
      }
       // mix of different types without text analyzer (identity will be used)
      {
        let result = db._query(
          "RETURN TOKENS(['quick fox', null, true, 3.14, 'string array', 5, [true, 4, 'one two'], true])",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(8, result[0].length);
        assertEqual(['quick fox'], result[0][0]);
        assertEqual([""], result[0][1]);
        assertEqual(["/w=="], result[0][2]);
        assertEqual(["oMAJHrhR64Uf", "sMAJHrhR6w==", "wMAJHrg=", "0MAJ"], result[0][3]);
        assertEqual(['string array'], result[0][4]);
        assertEqual(["oMAUAAAAAAAA", "sMAUAAAAAA==", "wMAUAAA=", "0MAU"], result[0][5]);
        assertTrue(Array === result[0][6].constructor);
        assertEqual(3, result[0][6].length);
        assertEqual(["/w=="], result[0][6][0]);
        assertEqual(["oMAQAAAAAAAA", "sMAQAAAAAA==", "wMAQAAA=", "0MAQ"], result[0][6][1]);
        assertEqual(['one two'], result[0][6][2]);
        assertEqual(["/w=="], result[0][7]);
      }
      // mix of different types (but without text)
      {
        let result = db._query(
          "RETURN TOKENS([null, true, 3.14, 5, [true, 4], true])",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(6, result[0].length);
        assertEqual([""], result[0][0]);
        assertEqual(["/w=="], result[0][1]);
        assertEqual(["oMAJHrhR64Uf", "sMAJHrhR6w==", "wMAJHrg=", "0MAJ"], result[0][2]);
        assertEqual(["oMAUAAAAAAAA", "sMAUAAAAAA==", "wMAUAAA=", "0MAU"], result[0][3]);
        assertTrue(Array === result[0][4].constructor);
        assertEqual(2, result[0][4].length);
        assertEqual(["/w=="], result[0][4][0]);
        assertEqual(["oMAQAAAAAAAA", "sMAQAAAAAA==", "wMAQAAA=", "0MAQ"], result[0][4][1]);
        assertEqual(["/w=="], result[0][5]);
      }

      // empty array
      {
        let result = db._query(
          "RETURN TOKENS([])",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(1, result[0].length);
        assertEqual([], result[0][0]);
      }
       // array of empty arrays
      {
        let result = db._query(
          "RETURN TOKENS([[],[]])",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(2, result[0].length);
        assertEqual([[]], result[0][0]);
        assertEqual([[]], result[0][1]);
      }
       // empty nested array
      {
        let result = db._query(
          "RETURN TOKENS([[]])",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(1, result[0].length);
        assertEqual([[]], result[0][0]);
      }
      //// failures
      //no parameters
      {
        try {
          let result = db._query(
            "RETURN TOKENS()",
            null,
            { }
          );
          fail();
        } catch(err) {
           assertEqual(require("internal").errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code,
                       err.errorNum);
        }
      }
      //too many parameters
      {
        try {
          let result = db._query(
            "RETURN TOKENS('test', 'identity', 'unexpected parameter')",
            null,
            { }
          );
          fail();
        } catch(err) {
           assertEqual(require("internal").errors.ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH.code,
                       err.errorNum);
        }
      }
      //invalid first parameter type
      {
        try {
          let result = db._query(
            "RETURN TOKENS({'test': true}, 'identity')",
            null,
            { }
          );
          fail();
        } catch(err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }
      //invalid second parameter type
      {
        try {
          let result = db._query(
            "RETURN TOKENS('test', 123)",
            null,
            { }
          );
          fail();
        } catch(err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }

    },
    
    testTokensFunctionWithNumberAnalyzer : function() {
      let analyzer = analyzers.save("gd","aql",
        { queryString: "RETURN @param",
          returnType: "number"}, []);
      assertNotNull(analyzer);
      try {
        let result = db._query("RETURN TOKENS('1', 'gd')").toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(1, result[0].length);
        assertEqual([["oL/wAAAAAAAA", "sL/wAAAAAA==", "wL/wAAA=", "0L/w"]],
                    result[0]);
      } finally {
        analyzers.remove("gd", true);
      }
    },

    testDefaultAnalyzers : function() {
      // invalid
      {
        try {
          db._query("RETURN TOKENS('a quick brown fox jumps', 'invalid')").toArray();
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }

      // text_de
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_de')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jumps" ], result[0]);
      }

      // text_en
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_en')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_es
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_es')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jumps" ], result[0]);
      }

      // text_fi
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_fi')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brow", "fox", "jumps" ], result[0]);
      }

      // text_fr
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_fr')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_it
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_it')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jumps" ], result[0]);
      }

      // text_nl
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_nl')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_no
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_no')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_pt
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_pt')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jumps" ], result[0]);
      }

      // text_ru (codepoints)
      {
        let result = db._query(
          "RETURN TOKENS('ArangoDB - \u044D\u0442\u043E \u043C\u043D\u043E\u0433\u043E\u043C\u043E\u0434\u0435\u043B\u044C\u043D\u0430\u044F \u0431\u0430\u0437\u0430 \u0434\u0430\u043D\u043D\u044B\u0445', 'text_ru')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "arangodb", "\u044D\u0442", "\u043C\u043D\u043E\u0433\u043E\u043C\u043E\u0434\u0435\u043B\u044C\u043D", "\u0431\u0430\u0437", "\u0434\u0430\u043D" ], result[0]);
        assertEqual([ "arangodb", "эт", "многомодельн", "баз", "дан" ], result[0]);
      }

      // text_ru (unicode)
      {
        let result = db._query(
          "RETURN TOKENS('ArangoDB - это многомодельная база данных', 'text_ru')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "arangodb", "\u044D\u0442", "\u043C\u043D\u043E\u0433\u043E\u043C\u043E\u0434\u0435\u043B\u044C\u043D", "\u0431\u0430\u0437", "\u0434\u0430\u043D" ], result[0]);
        assertEqual([ "arangodb", "эт", "многомодельн", "баз", "дан" ], result[0]);
      }

      // text_sv
      {
        let result = db._query(
          "RETURN TOKENS('a quick brown fox jumps', 'text_sv')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(5, result[0].length);
        assertEqual([ "a", "quick", "brown", "fox", "jump" ], result[0]);
      }

      // text_zh (codepoints)
      {
        let result = db._query(
           "RETURN TOKENS('ArangoDB \u662F\u4E00\u4E2A\u591A\u6A21\u578B\u6570\u636E\u5E93\u3002', 'text_zh')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(7, result[0].length);
        assertEqual([ "arangodb", "\u662F", "\u4E00\u4E2A", "\u591A", "\u6A21\u578B", "\u6570\u636E", "\u5E93" ], result[0]);
        assertEqual([ "arangodb", "是", "一个", "多", "模型", "数据", "库" ], result[0]);
      }

      // text_zh (unicode)
      {
        let result = db._query(
          "RETURN TOKENS('ArangoDB 是一个多模型数据库。', 'text_zh')",
          null,
          { }
        ).toArray();
        assertEqual(1, result.length);
        assertTrue(Array === result[0].constructor);
        assertEqual(7, result[0].length);
        assertEqual([ "arangodb", "\u662F", "\u4E00\u4E2A", "\u591A", "\u6A21\u578B", "\u6570\u636E", "\u5E93" ], result[0]);
        assertEqual([ "arangodb", "是", "一个", "多", "模型", "数据", "库" ], result[0]);
      }

    },

    testNormAnalyzer : function() {
      let analyzerName = "normUnderTest";
      // case upper
      {
        analyzers.save(analyzerName, "norm", { "locale" : "en", "case": "upper" });
        try {
          let result = db._query(
            "RETURN TOKENS('fOx', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "FOX" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // case lower
      {
        analyzers.save(analyzerName, "norm",  {  "locale" : "en", "case": "lower" });
        try {
          let result = db._query(
            "RETURN TOKENS('fOx', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "fox" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // case none
      {
        analyzers.save(analyzerName, "norm", {  "locale" : "en", "case": "none" });
        try {
          let result = db._query(
            "RETURN TOKENS('fOx', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "fOx" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
       // accent removal
      {
        analyzers.save(analyzerName, "norm", {  "locale" : "de_DE.UTF8", "case": "none", "accent":false });
        try {
          let result = db._query(
            "RETURN TOKENS('\u00F6\u00F5', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "\u006F\u006F" ], result[0]);
        } finally {
        analyzers.remove(analyzerName, true);
        }
      }
       // accent leave
      {
        analyzers.save(analyzerName, "norm", {  "locale" : "de_DE.UTF8", "case": "none", "accent":true });
        try {
          let result = db._query(
            "RETURN TOKENS('\u00F6\u00F5', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "\u00F6\u00F5" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // no properties
      {
        try {
          analyzers.save(analyzerName, "norm");
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }
    },
    testCustomStemAnalyzer : function() {
      let analyzerName = "stemUnderTest";
      {
        analyzers.save(analyzerName, "stem", {  "locale" : "en"});
        try {
          let result = db._query(
            "RETURN TOKENS('jumps', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "jump" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // no properties
      {
        try {
          analyzers.save(analyzerName, "stem");
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }
    },
    testCustomTextAnalyzer : function() {
      let analyzerName = "textUnderTest";
      // case upper
      {
        analyzers.save(analyzerName, "text", { "locale" : "en", "case": "upper", "stopwords": [] });
        try {
          let result = db._query(
            "RETURN TOKENS('fOx', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "FOX" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // case lower
      {
        analyzers.save(analyzerName, "text", { "locale" : "en", "case": "lower", "stopwords": [] });
        try {
          let result = db._query(
            "RETURN TOKENS('fOx', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "fox" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // case none
      {
        analyzers.save(analyzerName, "text", {  "locale" : "en", "case": "none", "stopwords": [] });
        try {
          let result = db._query(
            "RETURN TOKENS('fOx', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "fOx" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
       // accent removal
      {
        analyzers.save(analyzerName, "text", {  "locale" : "de_DE.UTF8", "case": "none", "accent":false, "stopwords": [], "stemming":false });
        try {
          let result = db._query(
            "RETURN TOKENS('\u00F6\u00F5', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "\u006F\u006F" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
       // accent leave
      {
        analyzers.save(analyzerName, "text", {  "locale" : "de_DE.UTF8", "case": "none", "accent":true, "stopwords": [], "stemming":false});
        try {
          let result = db._query(
            "RETURN TOKENS('\u00F6\u00F5', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "\u00F6\u00F5" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // no stemming
      {
        analyzers.save(analyzerName, "text", {  "locale" : "en", "case": "none", "stemming":false, "stopwords": [] });
        try {
          let result = db._query(
            "RETURN TOKENS('jumps', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "jumps" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // stemming
      {
        analyzers.save(analyzerName, "text", {  "locale" : "en", "case": "none", "stemming":true, "stopwords": [] });
        try {
          let result = db._query(
            "RETURN TOKENS('jumps', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "jump" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // empty ngram object
      {
        analyzers.save(analyzerName, "text", {  "locale" : "en", "case": "none", "stemming":false, "stopwords": [], "edgeNgram":{} });
        try {
          let props = analyzers.analyzer(analyzerName).properties().edgeNgram;
          assertEqual(undefined, props);
          let result = db._query(
            "RETURN TOKENS('jumps', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "jumps" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // non empty ngram object
      {
        analyzers.save(analyzerName, "text", {  "locale" : "en", "case": "none", "stemming":false, "stopwords": [], "edgeNgram":{ "min": 1, "max":2} });
        try {
          let props = analyzers.analyzer(analyzerName).properties().edgeNgram;
          assertNotEqual(undefined, props);
          assertEqual(1, props.min);
          assertEqual(2, props.max);
          assertEqual(undefined, props.preserveOriginal);
          let result = db._query(
            "RETURN TOKENS('quick brown foxy', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(6, result[0].length);
          assertEqual([ "q", "qu", "b", "br", "f", "fo" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // non empty ngram object
      {
        analyzers.save(analyzerName, "text", {  "locale" : "en", "case": "none", "stemming":false, "stopwords": [], "edgeNgram":{ "min": 1, "max":2, "preserveOriginal": true} });
        try {
          let props = analyzers.analyzer(analyzerName).properties().edgeNgram;
          assertNotEqual(undefined, props);
          assertEqual(1, props.min);
          assertEqual(2, props.max);
          assertEqual(true, props.preserveOriginal);
          let result = db._query(
            "RETURN TOKENS('quick brown foxy', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(9, result[0].length);
          assertEqual([ "q", "qu", "quick", "b", "br", "brown", "f", "fo", "foxy" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // non empty ngram object
      {
        analyzers.save(analyzerName, "text", {  "locale" : "en", "case": "none", "stemming":false, "stopwords": [], "edgeNgram":{ "min": 1, "max":200, "preserveOriginal": false} });
        try {
          let props = analyzers.analyzer(analyzerName).properties().edgeNgram;
          assertNotEqual(undefined, props);
          assertEqual(1, props.min);
          assertEqual(200, props.max);
          assertEqual(false, props.preserveOriginal);
          let result = db._query(
            "RETURN TOKENS('quick brown foxy', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(14, result[0].length);
          assertEqual([ "q", "qu", "qui", "quic", "quick", "b", "br", "bro", "brow", "brown", "f", "fo", "fox", "foxy" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // non empty ngram object with stemming
      {
        analyzers.save(analyzerName, "text", {  "locale" : "en", "case": "none", "stemming":true, "stopwords": [], "edgeNgram":{ "min": 1 } });
        try {
          let props = analyzers.analyzer(analyzerName).properties().edgeNgram;
          assertNotEqual(undefined, props);
          assertEqual(1, props.min);
          assertEqual(undefined, props.max);
          assertEqual(undefined, props.preserveOriginal);
          let result = db._query(
            "RETURN TOKENS('quick brown foxy', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(14, result[0].length);
          assertEqual([ "q", "qu", "qui", "quic", "quick", "b", "br", "bro", "brow", "brown", "f", "fo", "fox", "foxi" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // Greek stemming - test for snowball 2.0
      {
        analyzers.save(analyzerName, "text", {  "locale" : "el_GR.UTF8", "case": "none", "stemming":true, "stopwords": [] });
        try {
          let result = db._query(
            "RETURN TOKENS('Αθλητές', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ "αθλητ" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // no properties
      {
        try {
          analyzers.save(analyzerName, "text");
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }
    },

    testCustomNGramAnalyzer : function() {
      let analyzerName = "textUnderTest";

      // without preserveOriginal
      {
        analyzers.save(analyzerName, "ngram", { "min":1, "max":2, "preserveOriginal":false  });
        try {
          let props = analyzers.analyzer(analyzerName).properties();
          assertEqual(1, props.min);
          assertEqual(2, props.max);
          assertEqual(false, props.preserveOriginal);
          assertEqual("", props.startMarker);
          assertEqual("", props.endMarker);
          assertEqual("binary", props.streamType);
          let result = db._query(
            "RETURN TOKENS('quick', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(9, result[0].length);
          assertEqual([ "q", "qu", "u", "ui", "i", "ic", "c", "ck", "k"  ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // with preserveOriginal
      {
        analyzers.save(analyzerName, "ngram", { "min":1, "max":2, "preserveOriginal":true});
        try {
          let props = analyzers.analyzer(analyzerName).properties();
          assertEqual(1, props.min);
          assertEqual(2, props.max);
          assertEqual(true, props.preserveOriginal);
          assertEqual("", props.startMarker);
          assertEqual("", props.endMarker);
          assertEqual("binary", props.streamType);
          let result = db._query(
            "RETURN TOKENS('quick', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(10, result[0].length);
          assertEqual([ "q", "qu", "quick", "u", "ui", "i", "ic", "c", "ck", "k"  ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // with optional start marker
      {
        analyzers.save(analyzerName, "ngram", { "min":1, "max":2, "preserveOriginal":true, "startMarker":"$"});
        try {
          let props = analyzers.analyzer(analyzerName).properties();
          assertEqual(1, props.min);
          assertEqual(2, props.max);
          assertEqual(true, props.preserveOriginal);
          assertEqual("$", props.startMarker);
          assertEqual("", props.endMarker);
          assertEqual("binary", props.streamType);
          let result = db._query(
            "RETURN TOKENS('quick', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(10, result[0].length);
          assertEqual([ "$q", "$qu", "$quick", "u", "ui", "i", "ic", "c", "ck", "k"  ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // with optional end marker
      {
        analyzers.save(analyzerName, "ngram", { "min":1, "max":2, "preserveOriginal":true, "startMarker":"$", "endMarker":"^"});
        try {
          let props = analyzers.analyzer(analyzerName).properties();
          assertEqual(1, props.min);
          assertEqual(2, props.max);
          assertEqual(true, props.preserveOriginal);
          assertEqual("$", props.startMarker);
          assertEqual("^", props.endMarker);
          assertEqual("binary", props.streamType);
          let result = db._query(
            "RETURN TOKENS('quick', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(11, result[0].length);
          assertEqual([ "$q", "$qu", "$quick", "quick^", "u", "ui", "i", "ic", "c", "ck^", "k^" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // utf8 sequence
      {
        analyzers.save(analyzerName, "ngram", { "min":3, "max":2, "preserveOriginal":false, "streamType":"utf8"});
        try {
          let props = analyzers.analyzer(analyzerName).properties();
          assertEqual(3, props.min);
          assertEqual(3, props.max);
          assertEqual(false, props.preserveOriginal);
          assertEqual("", props.startMarker);
          assertEqual("", props.endMarker);
          assertEqual("utf8", props.streamType);
          let result = db._query(
            "RETURN TOKENS('хорошо', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(4, result[0].length);
          assertEqual([ "хор", "оро", "рош", "ошо"  ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }

      // invalid properties 
      {
        try {
          analyzers.save(analyzerName, "ngram", { "min":1, "max":2 } );
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }

      // invalid properties
      {
        try {
          analyzers.save(analyzerName, "ngram", { "max":2, "preserveOriginal":false } );
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }

      // invalid properties
      {
        try {
          analyzers.save(analyzerName, "ngram", { "min":2, "preserveOriginal":false } );
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }

      // invalid properties
      {
        try {
          analyzers.save(analyzerName, "ngram", { "max":2, "min":2, "preserveOriginal":false, "streamType":"invalid" } );
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }
    },
    
    testCustomPipelineAnalyzer : function() {
      let analyzerName = "pipeUnderTest";
      try { analyzers.remove(analyzerName, true); } catch(e) {}
      {
        analyzers.save(analyzerName,"pipeline",
                        {pipeline:[
                          {type:"delimiter", properties:{delimiter:" "}},
                          {type:"delimiter", properties:{delimiter:","}},
                          {type:"norm", properties:{locale:"en", "case":"upper"}}
                        ]}, ["position", "frequency"]);
        try {
          let result = db._query(
            "RETURN TOKENS('Quick,BrOwn FoX', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(3, result[0].length);
          assertEqual([ "QUICK", "BROWN", "FOX" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // upper + ngram utf8 sequence
      {
        analyzers.save(analyzerName,"pipeline",
                        {pipeline:[
                          {type:"norm", properties:{locale:"ru_RU.UTF8", "case":"upper"}},
                          {type:"ngram", properties: { "preserveOriginal":false, min:2, max:3, streamType:"utf8"}}]});
       
        try {
          let props = analyzers.analyzer(analyzerName).properties();
          assertEqual(2, props.pipeline[1].properties.min);
          assertEqual(3, props.pipeline[1].properties.max);
          assertEqual(false, props.pipeline[1].properties.preserveOriginal);
          assertEqual("utf8", props.pipeline[1].properties.streamType);
          assertEqual("upper", props.pipeline[0].properties["case"]);
          assertEqual(true, props.pipeline[0].properties.accent);
          let result = db._query(
            "RETURN TOKENS('хорошо', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(9, result[0].length);
          assertEqual([ "ХО", "ХОР", "ОР", "ОРО", "РО", "РОШ", "ОШ", "ОШО", "ШО" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // invalid properties
      {
        try {
          analyzers.save(analyzerName, "pipeline", { pipeline:2 } );
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }
    },
    
    testCustomAqlAnalyzer : function() {
      let analyzerName = "calcUnderTest";
      try { analyzers.remove(analyzerName, true); } catch(e) {}
      // soundex expression
      {
        analyzers.save(analyzerName,"aql",{queryString:"RETURN SOUNDEX(@param)"});
        try {
          let result = db._query(
            "RETURN TOKENS(['Andrei', 'Andrey'], '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(2, result[0].length);
          assertEqual([ ["A536"], ["A536"] ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // datetime
      {
        analyzers.save(analyzerName,"aql",{queryString:"RETURN DATE_ISO8601(@param)"});
        try {
          let result = db._query(
            "RETURN TOKENS('1974-06-09', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].length);
          assertEqual([ '1974-06-09T00:00:00.000Z' ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // cycle
      {
        analyzers.save(analyzerName,"aql",{queryString:"FOR d IN 1..TO_NUMBER(@param) FILTER d%2==0 RETURN TO_STRING(d)"});
        try {
          let result = db._query(
            "RETURN TOKENS('4', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(2, result[0].length);
          assertEqual(['2', '4'], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // pipeline for upper + ngram utf8 sequence
      {
        analyzers.save(analyzerName,"pipeline",
                        {pipeline:[
                          {type:"aql", properties:{queryString:"RETURN UPPER(@param)"}},
                          {type:"ngram", properties: { "preserveOriginal":false, min:2, max:3, streamType:"utf8"}}]});
       
        try {
          let result = db._query(
            "RETURN TOKENS('хорошо', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(9, result[0].length);
          assertEqual([ "ХО", "ХОР", "ОР", "ОРО", "РО", "РОШ", "ОШ", "ОШО", "ШО" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // pipeline for ngram utf8 sequence + upper
      {
        analyzers.save(analyzerName,"pipeline",
                        {pipeline:[
                          {type:"ngram", properties: { "preserveOriginal":false, min:2, max:3, streamType:"utf8"}},
                          {type:"aql", properties:{queryString:"RETURN UPPER(@param)"}}]});
       
        try {
          let result = db._query(
            "RETURN TOKENS('хорошо', '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(9, result[0].length);
          assertEqual([ "ХО", "ХОР", "ОР", "ОРО", "РО", "РОШ", "ОШ", "ОШО", "ШО" ], result[0]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
      // invalid properties
      {
        try {
          analyzers.save(analyzerName, "aql", { queryString:"" } );
          fail();
        } catch (err) {
          assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                      err.errorNum);
        }
      }
    },
    
    testCustomStopwordsAnalyzer : function() {
      let analyzerName = "stopwordsUnderTest";
      try { analyzers.remove(analyzerName, true); } catch(e) {}
      {
        analyzers.save(analyzerName,"stopwords",
                        {stopwords:["QWE", "qwe", "qqq"]}, ["position", "frequency"]);
        try {
          let result = db._query(
            "RETURN TOKENS(['QWE', 'qqq', 'aaa', 'qWe'], '" + analyzerName + "' )",
            null,
            { }
          ).toArray();
          assertEqual(1, result.length);
          assertEqual(4, result[0].length);
          assertEqual([ ], result[0][0]);
          assertEqual([ ], result[0][1]);
          assertEqual([ "aaa" ], result[0][2]);
          assertEqual([ "qWe" ], result[0][3]);
        } finally {
          analyzers.remove(analyzerName, true);
        }
      }
    },
    
    testCustomAqlAnalyzerInView : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"andrey"});
        col.save({field:"mike"});
        col.save({field:"frank"});
        analyzers.save("calcUnderTest","aql",{queryString:"RETURN SOUNDEX(@param)"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        assertEqual(2, db._query("FOR d IN @@v SEARCH ANALYZER(d.field != SOUNDEX('andrei'), 'calcUnderTest')" + 
                                 "OPTIONS { waitForSync: true } RETURN d ",
                                { '@v':viewName }).toArray().length);
        assertEqual(1, db._query("FOR d IN @@v  " + 
                                 "SEARCH ANALYZER(d.field == SOUNDEX('andrei'), 'calcUnderTest') OPTIONS { waitForSync: true } RETURN d ",
                                { '@v':viewName }).toArray().length);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerNumericInView : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"RETURN TO_NUMBER(@param)",
                                              returnType:"number"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH d.field < 2" + 
                             "OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("1", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH d.field == 3 OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res3.length);
        assertEqual("3", res3[0].field);
        let res23 = db._query("FOR d IN @@v SEARCH IN_RANGE(d.field, 2, 3, true, true) " + 
                             "OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res23.length);
        assertEqual("2", res23[0].field);
        assertEqual("3", res23[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerNumericInViewStringRetval : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"RETURN TO_STRING(@param)",
                                              returnType:"number"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH d.field < 2" + 
                             "OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("1", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH d.field == 3 OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res3.length);
        assertEqual("3", res3[0].field);
        let res23 = db._query("FOR d IN @@v SEARCH IN_RANGE(d.field, 2, 3, true, true) " + 
                             "OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res23.length);
        assertEqual("2", res23[0].field);
        assertEqual("3", res23[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerNumericArrayInView : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"FOR a IN 1..@param RETURN a",
                                              returnType:"number"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH d.field > 2" + 
                             "OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("3", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH d.field == 2 OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res3.length);
        assertEqual("2", res3[0].field);
        assertEqual("3", res3[1].field);
        let res23 = db._query("FOR d IN @@v SEARCH IN_RANGE(d.field, 2, 3, true, true) " + 
                             "OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res23.length);
        assertEqual("2", res23[0].field);
        assertEqual("3", res23[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerNumericArrayInViewStringRetVal : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"FOR a IN 1..@param RETURN TO_STRING(a)",
                                              returnType:"number"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH d.field > 2" + 
                             "OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("3", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH d.field == 2 OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res3.length);
        assertEqual("2", res3[0].field);
        assertEqual("3", res3[1].field);
        let res23 = db._query("FOR d IN @@v SEARCH IN_RANGE(d.field, 2, 3, true, true) " + 
                             "OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res23.length);
        assertEqual("2", res23[0].field);
        assertEqual("3", res23[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerStringInView : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"RETURN @param",
                                              returnType:"string"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH ANALYZER(d.field > '2', 'calcUnderTest')" +  
                             "OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("3", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH ANALYZER(d.field == '2', 'calcUnderTest') " +
                             " OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res3.length);
        assertEqual("2", res3[0].field);
        let res23 = db._query("FOR d IN @@v SEARCH ANALYZER(IN_RANGE(d.field, '2', '3', true, true) " +
                              ", 'calcUnderTest') OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res23.length);
        assertEqual("2", res23[0].field);
        assertEqual("3", res23[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerStringInViewNumberRetVal : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"RETURN TO_NUMBER(@param)",
                                              returnType:"string"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH ANALYZER(d.field > '2', 'calcUnderTest')" +  
                             "OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("3", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH ANALYZER(d.field == '2', 'calcUnderTest') " +
                             " OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res3.length);
        assertEqual("2", res3[0].field);
        let res23 = db._query("FOR d IN @@v SEARCH ANALYZER(IN_RANGE(d.field, '2', '3', true, true) " +
                              ", 'calcUnderTest') OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res23.length);
        assertEqual("2", res23[0].field);
        assertEqual("3", res23[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerStringArrayInView : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"FOR a IN 1..@param RETURN a",
                                              returnType:"string"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH ANALYZER(d.field > '2', 'calcUnderTest')" +  
                             "OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("3", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH ANALYZER(d.field == '2', 'calcUnderTest') " +
                             " OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res3.length);
        assertEqual("2", res3[0].field);
        assertEqual("3", res3[1].field);
        let res23 = db._query("FOR d IN @@v SEARCH ANALYZER(IN_RANGE(d.field, '2', '3', true, true) " +
                              ", 'calcUnderTest') OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res23.length);
        assertEqual("2", res23[0].field);
        assertEqual("3", res23[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerStringArrayInViewNumberRetVal : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"FOR a IN 1..@param RETURN TO_NUMBER(a)",
                                              returnType:"string"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH ANALYZER(d.field > '2', 'calcUnderTest')" +  
                             "OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("3", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH ANALYZER(d.field == '2', 'calcUnderTest') " +
                             " OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res3.length);
        assertEqual("2", res3[0].field);
        assertEqual("3", res3[1].field);
        let res23 = db._query("FOR d IN @@v SEARCH ANALYZER(IN_RANGE(d.field, '2', '3', true, true) " +
                              ", 'calcUnderTest') OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res23.length);
        assertEqual("2", res23[0].field);
        assertEqual("3", res23[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerBoolInView : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"RETURN TO_NUMBER(@param) == 2 ",
                                              returnType:"bool"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH d.field == true" + 
                             " OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("2", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH d.field == false " +
                             " OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res3.length);
        assertEqual("1", res3[0].field);
        assertEqual("3", res3[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerBoolInViewNumberRetVal : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"RETURN TO_NUMBER(@param) == 2 ? 1 : 0 ",
                                              returnType:"bool"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH d.field == true" + 
                             " OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(1, res1.length);
        assertEqual("2", res1[0].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH d.field == false " +
                             " OPTIONS { waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res3.length);
        assertEqual("1", res3[0].field);
        assertEqual("3", res3[1].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerBoolArrayInView : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"FOR a IN 1..@param RETURN a == 2",
                                              returnType:"bool"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH d.field == true" + 
                             " OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res1.length);
        assertEqual("2", res1[0].field);
        assertEqual("3", res1[1].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH d.field == false OPTIONS " +
                             "{ waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(3, res3.length);
        assertEqual("1", res3[0].field);
        assertEqual("2", res3[1].field);
        assertEqual("3", res3[2].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerBoolArrayInViewNumberRetVal : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"1"});
        col.save({field:"2"});
        col.save({field:"3"});
        analyzers.save("calcUnderTest","aql",{queryString:"FOR a IN 1..@param RETURN a == 2 ? 1 : 0",
                                              returnType:"bool"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['calcUnderTest']}}});
        let res1 = db._query("FOR d IN @@v SEARCH d.field == true" + 
                             " OPTIONS { waitForSync: true } RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(2, res1.length);
        assertEqual("2", res1[0].field);
        assertEqual("3", res1[1].field);
        let res3 = db._query("FOR d IN @@v  " + 
                             "SEARCH d.field == false OPTIONS " +
                             "{ waitForSync: true } SORT d.field ASC RETURN d ",
                             { '@v':viewName }).toArray();
        assertEqual(3, res3.length);
        assertEqual("1", res3[0].field);
        assertEqual("2", res3[1].field);
        assertEqual("3", res3[2].field);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase(dbName);
      }
    },
    
    testCustomAqlAnalyzerInvalidTypeInView : function() {
      try {
        let invalid  =  analyzers.save("calcUnderTest","aql",{queryString:"FOR a IN 1..@param RETURN a % 2 == 0",
                                       returnType:"null"});
        fail();                               
      } catch(err) {
        assertEqual(require("internal").errors.ERROR_BAD_PARAMETER.code,
                    err.errorNum);
      }
    },
    
    testInvalidTypeAnalyzer : function() {
      let analyzerName = "unknownUnderTest";
      try {
        analyzers.save(analyzerName, "unknownAnalyzerType");
        fail();
      } catch (err) {
        assertEqual(require("internal").errors.ERROR_NOT_IMPLEMENTED.code,
                    err.errorNum);
      }
    },
    testDisjunctionWithExclude : function() {
      let dbName = "testDb";
      let colName = "testCollection";
      let viewName = "testView";
      db._useDatabase("_system");
      try { db._dropDatabase(dbName); } catch(e) {}
      db._createDatabase(dbName);
      try {
        db._useDatabase(dbName);
        let col = db._create(colName);
        col.save({field:"value"});
        db._createView(viewName, "arangosearch", 
                                  {links: 
                                    {[colName]: 
                                      {storeValues: 'id', 
                                       includeAllFields:true, 
                                       analyzers:['identity']}}});
        assertEqual(1, db._query("FOR d IN @@v SEARCH ANALYZER(d.field != 'nothing', 'identity') OR true == false " + 
                                 "OPTIONS { waitForSync: true } RETURN d ",
                                { '@v':viewName }).toArray().length);
        assertEqual(1, db._query("FOR d IN @@v  " + 
                                 "SEARCH ANALYZER(d.field != 'nothing', 'identity') OR ANALYZER(EXISTS(d.field) " + 
                                 " && true == false, 'identity')  OPTIONS { waitForSync: true } RETURN d ",
                                { '@v':viewName }).toArray().length);
        let actual1 = db._createStatement({ "query": "FOR s IN `testView` SEARCH ANALYZER(s.field != 'nothing' " + 
                                                     "OR  true == false, 'identity') RETURN s.field" });
        assertEqual(1, actual1.execute().toArray().length);
        let actual2 = db._createStatement({ "query": "FOR s IN `testView` SEARCH ANALYZER(s.field != 'nothing' "+ 
                                                     " OR (EXISTS(s.field) && true == false), 'identity') RETURN s.field" });
        assertEqual(1, actual2.execute().toArray().length);
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

jsunity.run(iResearchFeatureAqlTestSuite);

return jsunity.done();
