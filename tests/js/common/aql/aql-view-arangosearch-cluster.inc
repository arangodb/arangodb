/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNotUndefined, fail*/

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for iresearch usage
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function() {
  var db = require("@arangodb").db;
  const analyzers = require("@arangodb/analyzers");
  const ERRORS = require("@arangodb").errors;

  // this file is loaded into the various shard / rf test files
  return function (args) {
    var c;
    var c2;
    var v;
    var v2;
    var cg;
    var vg;
    var meta;

    function getNodes(query, type, bindVars, options) {
      let stmt = db._createStatement(query);
      if (typeof bindVars === "object") {
        stmt.bind(bindVars);
      }
      if (typeof options === "object") {
        stmt.setOptions(options);
      }
      return stmt.explain()
                 .plan
                 .nodes
                 .filter(node => node.type === type);
    }

    console.info("Test suite arguments: " + JSON.stringify(args));

    return {
      setUpAll : function () {
        analyzers.save("text_en", "text", "{ \"locale\": \"en.UTF-8\", \"stopwords\": [ ] }", [ "frequency", "norm", "position" ]);
        
        db._drop("AuxUnitTestsCollection");
        let auxCol = db._create("AuxUnitTestsCollection");
        auxCol.save({ foobar: ['foo', 'bar'], foo: ['foo'], bar:['bar'], empty: []});
        
        db._drop("UnitTestsCollection");
        c = db._create("UnitTestsCollection", args);

        db._drop("UnitTestsCollection2");
        c2 = db._create("UnitTestsCollection2", args);

        db._drop("AnotherUnitTestsCollection");
        var ac = db._create("AnotherUnitTestsCollection", args);

        db._drop("UnitTestsWithArrayCollection");
        let arrayCol = db._create("UnitTestsWithArrayCollection");
        arrayCol.save({ c: 0, a: ['foo', 'bar', 'baz']});
        // this will allow to catch if accidentally "all" filter will be used
        arrayCol.save({ c: 1, a: ['afoo', 'abar', 'abaz']});

        db._dropView("UnitTestsView");
        v = db._createView("UnitTestsView", "arangosearch", {});
        meta = {
          links: { 
            "UnitTestsCollection": { 
              includeAllFields: true,
              storeValues: "id",
              fields: {
                text: { analyzers: [ "text_en" ] }
              }
            }
          }
        };
        v.properties(meta);

        db._dropView("CompoundView");
        v2 = db._createView("CompoundView", "arangosearch",
          { links : {
            UnitTestsCollection: { includeAllFields: true },
            UnitTestsCollection2 : { includeAllFields: true }
          }}
        );

        db._drop("UnitTestsGraphCollection");
        cg = db._create("UnitTestsGraphCollection", args);

        vg = db._createView("UnitTestsGraphView", "arangosearch", {});
        meta = {
          links: { 
            "UnitTestsGraphCollection": { 
              includeAllFields: true,
              storeValues: "id",
              fields: {
                text: { analyzers: [ "text_en" ] }
              }
            }
          }
        };
        vg.properties(meta);

        db._drop("UnitTestsGraph");
        var g = db._createEdgeCollection("UnitTestsGraph", args);

        ac.save({ a: "foo", id : 0 });
        ac.save({ a: "ba", id : 1 });

        for (let i = 0; i < 5; i++) {
          c.save({ a: "foo", b: "bar", c: i });
          c.save({ a: "foo", b: "baz", c: i });
          c.save({ a: "bar", b: "foo", c: i });
          c.save({ a: "baz", b: "foo", c: i });

          c2.save({ a: "foo", b: "bar", c: i });
          c2.save({ a: "bar", b: "foo", c: i });
          c2.save({ a: "baz", b: "foo", c: i });
        }

        c.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
        c.save({ name: "half", text: "quick fox over lazy" });
        c.save({ name: "other half", text: "the brown jumps the dog" });
        c.save({ name: "quarter", text: "quick over" });

        c.save({ name: "numeric", anotherNumericField: 0 });
        c.save({ name: "null", anotherNullField: null });
        c.save({ name: "bool", anotherBoolField: true });
        c.save({ _key: "foo", xyz: 1 });

        cg.save({ _key: "begin", vName: "vBegin" });
        cg.save({ _key: "intermediate", vName: "vIntermediate" });
        cg.save({ _key: "end", vName: "vEnd" });

        g.save({ _from: "UnitTestsGraphCollection/begin", _to: "UnitTestsGraphCollection/intermediate" });
        g.save({ _from: "UnitTestsGraphCollection/intermediate", _to: "UnitTestsGraphCollection/end" });

        db._dropView("UnitTestsViewArrayView");

        let arrayV = db._createView("UnitTestsWithArrayView", "arangosearch", {});
        meta = {
          links: { 
            UnitTestsWithArrayCollection: { 
              includeAllFields: true,
              fields: {
                a: { analyzers: [ "identity" ] }
             }
            }
          }
        };
        arrayV.properties(meta);
        
        db._drop("TestsCollectionWithManyFields");
        let mfc = db._create("TestsCollectionWithManyFields");
        mfc.save({field1:"1value", field2:"2value", field3: 1, field4: 11111, field5: 1, field6: 1});
        mfc.save({field1:"1value1", field2:"2value1", field3: 2, field4: 11112, field5: 2, field6: 2});
        mfc.save({field1:"1value2", field2:"2value2", field3: 3, field4: 11113, field5: 3, field6: 3});
        mfc.save({field1:"1value3", field2:"2value3", field3: 4, field4: 11114, field5: 4, field6: 4});
        
        try { analyzers.remove("customAnalyzer", true); } catch(err) {}
        analyzers.save("customAnalyzer", "text",  {"locale": "en.utf-8",
                                                   "case": "lower",
                                                   "stopwords": [],
                                                   "accent": false,
                                                   "stemming": false},
                                                   ["position", "norm", "frequency"]);
                                                   
        let wps = db._createView("WithPrimarySort", "arangosearch", 
                                 {primarySort: [{field: "field1", direction: "asc"},
                                                {field: "field2", direction: "asc"},
                                                {field: "field3", direction: "asc"},
                                                {field: "field4", direction: "asc"},
                                                {field: "_key", direction: "asc"}]});
                                      
        wps.properties({links:{TestsCollectionWithManyFields: {
                                storeValues: "id",
                                analyzers: ["customAnalyzer"],
                                fields: {
                                  field1: {},
                                  field2: {},
                                  field3: {},
                                  field4: {},
                                  field5: {},
                                  field6: {},
                                  _key: {}}}}});
      },

      tearDownAll : function () {
        meta = { links : { "UnitTestsCollection": null } };
        v.properties(meta);
        v.drop();

        v2.drop();

        meta = { links : { "UnitTestsGraphCollection": null } };
        vg.properties(meta);
        vg.drop();

        db._drop("UnitTestsCollection");
        db._drop("UnitTestsCollection2");
        db._drop("AnotherUnitTestsCollection");
        db._drop("UnitTestsGraph");
        db._drop("UnitTestsGraphCollection");
        db._drop("AuxUnitTestsCollection");
        db._dropView("UnitTestsWithArrayView");
        db._drop("UnitTestsWithArrayCollection");
        db._dropView("WithPrimarySort");
        db._drop("TestsCollectionWithManyFields");
        analyzers.remove("customAnalyzer", true);
      },

      testViewInFunctionCall : function () {
        try {
          db._query("FOR doc IN 1..1 RETURN COUNT(UnitTestsView)");
        } catch (e) {
          assertEqual(ERRORS.ERROR_NOT_IMPLEMENTED.code, e.errorNum);
        }
      },

      testV8FunctionInSearch: function () {
        try {
          db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' && APPLY('LOWER', 'abc') == 'ABC' OPTIONS { waitForSync: true } RETURN doc");
          fail();
        } catch (e) {
          assertEqual(ERRORS.ERROR_NOT_IMPLEMENTED.code, e.errorNum);
        }
      },

      testAnalyzerFunctionInReturnStatement: function () {
        var result = db._query("FOR doc IN CompoundView SEARCH doc.name == 'full' OPTIONS { waitForSync: true } RETURN ANALYZER(doc.text, 'text_en')").toArray();
        assertEqual(result.length, 1);
        assertEqual(result[0], "the quick brown fox jumps over the lazy dog");
      },

      testTransactionRegistration : function () {
        // ensure data is synced
        db._query("FOR d IN CompoundView OPTIONS {waitForSync:true} LIMIT 1 RETURN 1");

        // implicit read lock
        var result = db._executeTransaction({
          collections: { },
          action: function () {
            var db = require("@arangodb").db;
            return db._query("FOR d IN CompoundView SEARCH d.name == 'full' RETURN d.text").toArray();
          }
        });
        assertEqual("the quick brown fox jumps over the lazy dog", result[0]);
        assertEqual(1, result.length);
      },

      testViewCollectionOptions : function() {
        var result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } RETURN doc").toArray();

        assertEqual(result.length, 10);
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          assertTrue(res._id.startsWith('UnitTestsCollection/'));
        });

        result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ @collectionName ] } RETURN doc", { collectionName : 'UnitTestsCollection' }).toArray();

        assertEqual(result.length, 10);
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          assertTrue(res._id.startsWith('UnitTestsCollection/'));
        });

        result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : @collections } RETURN doc", { collections : [ 'UnitTestsCollection' ] }).toArray();

        assertEqual(result.length, 10);
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          assertTrue(res._id.startsWith('UnitTestsCollection/'));
        });

        result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ " + c2._id + " ] } RETURN doc").toArray();

        assertEqual(result.length, 5);
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          assertTrue(res._id.startsWith('UnitTestsCollection2/'));
        });

        result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ '" + c2._id + "', 'UnitTestsCollection' ] } RETURN doc").toArray();

        assertEqual(result.length, 15);
        var CountC1 = 0;
        var CountC2 = 0;
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          if (res._id.startsWith('UnitTestsCollection2/')) {
            ++CountC2;
          } else if (res._id.startsWith('UnitTestsCollection/')) {
            ++CountC1;
          }
        });
        assertEqual(CountC1, 10);
        assertEqual(CountC2, 5);

        result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : null } RETURN doc").toArray();

        assertEqual(result.length, 15);
        CountC1 = 0;
        CountC2 = 0;
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          if (res._id.startsWith('UnitTestsCollection2/')) {
            ++CountC2;
          } else if (res._id.startsWith('UnitTestsCollection/')) {
            ++CountC1;
          }
        });
        assertEqual(CountC1, 10);
        assertEqual(CountC2, 5);

        result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : @collections } RETURN doc", { collections: null }).toArray();

        assertEqual(result.length, 15);
        CountC1 = 0;
        CountC2 = 0;
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          if (res._id.startsWith('UnitTestsCollection2/')) {
            ++CountC2;
          } else if (res._id.startsWith('UnitTestsCollection/')) {
            ++CountC1;
          }
        });
        assertEqual(CountC1, 10);
        assertEqual(CountC2, 5);

        result = db._query("FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [] } RETURN doc").toArray();
        assertEqual(result.length, 0);

        result = db._query(
          "FOR doc IN CompoundView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } FOR doc2 IN CompoundView SEARCH doc2.a == 'foo' OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection2' ] } RETURN { doc, doc2 }"
        ).toArray();

        assertEqual(result.length, 50);
        result.forEach(function(res) {
          assertEqual(res.doc.a, "foo");
          assertEqual(res.doc2.a, "foo");
          assertTrue(res.doc._id.startsWith('UnitTestsCollection/'));
          assertTrue(res.doc2._id.startsWith('UnitTestsCollection2/'));
        });
      },

      testAttributeEqualityFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a == 'foo' OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 10);
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
        });
      },

      testMultipleAttributeEqualityFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a == 'foo' && doc.b == 'bar' OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 5);
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          assertEqual(res.b, "bar");
        });
      },

      testMultipleAttributeEqualityFilterSortAttribute : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a == 'foo' && doc.b == 'bar' OPTIONS { waitForSync: true } SORT doc.c RETURN doc").toArray();

        assertEqual(result.length, 5);
        var last = -1;
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          assertEqual(res.b, "bar");
          assertEqual(res.c, last + 1);
          last = res.c;
        });
      },

      testMultipleAttributeEqualityFilterSortAttributeDesc : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a == 'foo' AND doc.b == 'bar' OPTIONS { waitForSync: true } SORT doc.c DESC RETURN doc").toArray();

        assertEqual(result.length, 5);
        var last = 5;
        result.forEach(function(res) {
          assertEqual(res.a, "foo");
          assertEqual(res.b, "bar");
          assertEqual(res.c, last - 1);
          last = res.c;
        });
      },

      testAttributeLessFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c < 2 OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 8);
        result.forEach(function(res) {
          assertTrue(res.c < 2);
        });
      },

      testAttributeLeqFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c <= 2 OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 12);
        result.forEach(function(res) {
          assertTrue(res.c <= 2);
        });
      },

      testAttributeGeqFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c >= 2 OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 12);
        result.forEach(function(res) {
          assertTrue(res.c >= 2);
        });
      },

      testAttributeGreaterFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c > 2 OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 8);
        result.forEach(function(res) {
          assertTrue(res.c > 2);
        });
      },

      testAttributeOpenIntervalFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c > 1 AND doc.c < 3 OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 4);
        result.forEach(function(res) {
          assertTrue(res.c > 1 && res.c < 3);
        });
      },

      testAttributeClosedIntervalFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c >= 1 AND doc.c <= 3 OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 12);
        result.forEach(function(res) {
          assertTrue(res.c >= 1 && res.c <= 3);
        });
      },

      testAttributeIntervalExclusionFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c < 1 OR doc.c > 3 OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 8);
        result.forEach(function(res) {
          assertTrue(res.c < 1 || res.c > 3);
        });
      },

      testAttributeNeqFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.a != 'foo' OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 18); // include documents without attribute 'a'
        result.forEach(function(res) {
          assertFalse(res.a === 'foo');
        });
      },

      testStartsWithFilter : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, 'fo') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 10);
        result.forEach(function(res) {
          assertEqual(res.a, 'foo');
        });
      },

      testStartsWithFilter2 : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.b, 'ba') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 10);
        result.forEach(function(res) {
          assertTrue(res.b === 'bar' || res.b === 'baz');
        });
      },

      testStartsWithFilterSort : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.b, 'ba') && doc.c == 0 OPTIONS { waitForSync: true } SORT doc.b RETURN doc").toArray();

        assertEqual(result.length, 2);
        assertEqual(result[0].b, 'bar');
        assertEqual(result[1].b, 'baz');
        assertEqual(result[0].c, 0);
        assertEqual(result[1].c, 0);
      },

      testStartsWithFilterArrayWithoutMinMatchCountViaReference : function () {
        var result = db._query("LET x = NOOPT(['fo', 'g']) FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, x) OPTIONS { waitForSync : true } RETURN doc").toArray();
  
        assertEqual(result.length, 10);
        result.forEach(function(res) {
          assertEqual(res.a, 'foo');
        });
      },

      testStartsWithFilterArrayWithoutMinMatchCount : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, ['fo', 'g']) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 10);
        result.forEach(function(res) {
          assertEqual(res.a, 'foo');
        });
      },

      testStartsWithFilterEmptyArray : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, []) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 0);
      },

      testStartsWithFilterEmptyArrayMinMatchCountZero : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, [], 0) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 28);
      },

      testStartsWithFilterArrayWithMinMatchCountZero : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, ['fo', 'g'], 0) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 28);
      },

      testStartsWithFilterArrayWithMinMatchCountTrue : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, ['fo', 'g'], 1) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 10);
        result.forEach(function(res) {
          assertEqual(res.a, 'foo');
        });
      },

      testStartsWithFilterArrayWithMinMatchCountFalse : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH STARTS_WITH(doc.a, ['fo', 'g'], 2) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 0);
      },

      testWildcardFilter : function() {
        {
          let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '_a_' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
          assertEqual(2, result.length);
          assertEqual("bar", result[0].a);
          assertEqual("baz", result[1].a);
        }

        {
          let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '_ar' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
          assertEqual(1, result.length);
          assertEqual("bar", result[0].a);
        }

        {
          let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '_a__' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
          assertEqual(0, result.length);
        }

        {
          let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE 'ba_' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
          assertEqual("bar", result[0].a);
          assertEqual("baz", result[1].a);
        }

        {
          let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE 'b__' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
          assertEqual(2, result.length);
          assertEqual("bar", result[0].a);
          assertEqual("baz", result[1].a);
        }

        {
          let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE 'b%' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
          assertEqual(2, result.length);
          assertEqual("bar", result[0].a);
          assertEqual("baz", result[1].a);
        }

        {
          let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '%r' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
          assertEqual(1, result.length);
          assertEqual("bar", result[0].a);
        }

        {
          let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE 'b%%' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
          assertEqual(2, result.length);
          assertEqual("bar", result[0].a);
          assertEqual("baz", result[1].a);
        }

        {
          let result = db._query("FOR doc IN UnitTestsView SEARCH doc.a LIKE '%a%' OPTIONS { waitForSync:true } FILTER doc.c == 0 SORT doc.a RETURN doc").toArray();
          assertEqual(2, result.length);
          assertEqual("bar", result[0].a);
          assertEqual("baz", result[1].a);
        }
      },

      testPhraseFilter : function () {
        var result0 = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, 'quick brown fox jumps', 'text_en') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result0.length, 1);
        assertEqual(result0[0].name, 'full');

        var result1 = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [ 'quick brown fox jumps' ], 'text_en') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result1.length, 1);
        assertEqual(result1[0].name, 'full');

        var result2 = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text, 'quick brown fox jumps'), 'text_en') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result2.length, 1);
        assertEqual(result2[0].name, 'full');

        var result3 = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text, [ 'quick brown fox jumps' ]), 'text_en') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result3.length, 1);
        assertEqual(result3[0].name, 'full');
        
        var result6v = db._query("LET phraseStruct = NOOPT([' fox',  ' jumps']) "
                               + "FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text,  'quick ', 0, 'brown', 0, phraseStruct), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result6v.length, 1);
        assertEqual(result6v[0].name, 'full');
      
            
        var result6v2 = db._query("LET phraseStruct = NOOPT(['quick ', 0, 'brown', 0, [' fox',  ' jumps']]) "
                               + "FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text, phraseStruct), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result6v2.length, 1);
        assertEqual(result6v2[0].name, 'full');
      },

      testExistsFilter : function () {
        var expected = new Set();
        expected.add("full");
        expected.add("half");
        expected.add("other half");
        expected.add("quarter");

        var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.text) OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, expected.size);
        result.forEach(function(res) {
          assertTrue(expected.delete(res.name));
        });
        assertEqual(expected.size, 0);
      },

      testExistsFilterByAnalyzer: function () {
        var expected = new Set();
        expected.add("full");
        expected.add("half");
        expected.add("other half");
        expected.add("quarter");

        var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.text, 'analyzer', 'text_en') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, expected.size);
        result.forEach(function(res) {
          assertTrue(expected.delete(res.name));
        });
        assertEqual(expected.size, 0);
      },

      testExistsFilterByIdentityAnalyzer: function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.text, 'analyzer') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(0, result.length);
      },

      testExistsFilterByContextAnalyzer: function () {
        var expected = new Set();
        expected.add("full");
        expected.add("half");
        expected.add("other half");
        expected.add("quarter");

        var result = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(EXISTS(doc.text, 'analyzer'), 'text_en') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, expected.size);
        result.forEach(function(res) {
          assertTrue(expected.delete(res.name));
        });
        assertEqual(expected.size, 0);
      },

      testExistsFilterByString: function () {
        var expected = new Set();
        expected.add("full");
        expected.add("half");
        expected.add("other half");
        expected.add("quarter");

        var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.text, 'string') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, expected.size);
        result.forEach(function(res) {
          assertTrue(expected.delete(res.name));
        });
        assertEqual(expected.size, 0);
      },

      testExistsFilterByType : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.text, 'type') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, 0);
      },

      testExistsFilterByTypeNull : function () {
        var expected = new Set();
        expected.add("null");

        var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc.anotherNullField, 'null') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, expected.size);
        result.forEach(function(res) {
          assertTrue(expected.delete(res.name));
        });
        assertEqual(expected.size, 0);
      },

      testExistsFilterByTypeBool : function () {
        var expected = new Set();
        expected.add("bool");

        var result = db._query("FOR doc IN UnitTestsView SEARCH EXISTS(doc['anotherBoolField'], 'bool') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, expected.size);
        result.forEach(function(res) {
          assertTrue(expected.delete(res.name));
        });
        assertEqual(expected.size, 0);
      },

      testExistsFilterByTypeNumeric : function () {
        var expected = new Set();
        expected.add("numeric");

        var result = db._query("LET suffix='NumericField' LET fieldName = CONCAT('another', suffix) FOR doc IN UnitTestsView SEARCH EXISTS(doc[fieldName], 'numeric') OPTIONS { waitForSync: true } RETURN doc").toArray();

        assertEqual(result.length, expected.size);
        result.forEach(function(res) {
          assertTrue(expected.delete(res.name));
        });
        assertEqual(expected.size, 0);
      },

      testViewInInnerLoop : function() {
        var expected = new Set(); // FIXME is there a better way to compare objects in js?
        expected.add(JSON.stringify({ a: "foo", b: "bar", c: 0 }));
        expected.add(JSON.stringify({ a: "foo", b: "baz", c: 0 }));
        expected.add(JSON.stringify({ a: "bar", b: "foo", c: 1 }));
        expected.add(JSON.stringify({ a: "baz", b: "foo", c: 1 }));

        var result = db._query(
          "FOR adoc IN AnotherUnitTestsCollection" +
          "  FOR doc IN UnitTestsView SEARCH adoc.id == doc.c && STARTS_WITH(doc['a'], adoc.a) OPTIONS { waitForSync: true } " +
          "RETURN doc"
        ).toArray();


        assertEqual(result.length, expected.size);
        result.forEach(function(res) {
          assertTrue(expected.delete(JSON.stringify({ a: res.a, b: res.b, c: res.c })));
        });
        assertEqual(expected.size, 0);
      },

      testViewInInnerLoopMultipleFilters : function() {
        var expected = new Set(); // FIXME is there a better way to compare objects in js?
        expected.add(JSON.stringify({ a: "foo", b: "bar", c: 0 }));
        expected.add(JSON.stringify({ a: "foo", b: "baz", c: 0 }));

        var result = db._query(
          "FOR adoc IN AnotherUnitTestsCollection FILTER adoc.id < 1" +
          "  FOR doc IN UnitTestsView SEARCH adoc.id == doc.c && STARTS_WITH(doc['a'], adoc.a) OPTIONS { waitForSync: true } " +
          "RETURN doc"
        ).toArray();


        assertEqual(result.length, expected.size);
        result.forEach(function(res) {
          assertTrue(expected.delete(JSON.stringify({ a: res.a, b: res.b, c: res.c })));
        });
        assertEqual(expected.size, 0);
      },

      testViewInInnerLoopSortByAttribute : function() {
        var expected = [];
        expected.push({ a: "bar", b: "foo", c: 1 });
        expected.push({ a: "baz", b: "foo", c: 1 });
        expected.push({ a: "foo", b: "bar", c: 0 });
        expected.push({ a: "foo", b: "baz", c: 0 });

        var result = db._query(
          "FOR adoc IN AnotherUnitTestsCollection" +
          "  FOR doc IN UnitTestsView SEARCH adoc.id == doc.c && STARTS_WITH(doc['a'], adoc.a) OPTIONS { waitForSync: true } " +
          "SORT doc.c DESC, doc.a, doc.b " +
          "RETURN doc"
        , null, { waitForSync: true }).toArray();

        assertEqual(result.length, expected.length);
        var i = 0;
        result.forEach(function(res) {
          var doc = expected[i++];
          assertEqual(doc.a, res.a);
          assertEqual(doc.b, res.b);
          assertEqual(doc.c, res.c);
        });
      },

      testViewInInnerLoopSortByAttributeWINDOW : function() {
        var expected = [];
        expected.push({ a: "bar", b: "foo", c: 1, unique: 1 });
        expected.push({ a: "baz", b: "foo", c: 1, unique: 1 });
        expected.push({ a: "foo", b: "bar", c: 0, unique: 2 });
        expected.push({ a: "foo", b: "baz", c: 0, unique: 2 });

        var result = db._query(
          "FOR adoc IN AnotherUnitTestsCollection" +
          "  FOR doc IN UnitTestsView SEARCH adoc.id == doc.c && STARTS_WITH(doc['a'], adoc.a) OPTIONS { waitForSync: true } " +
          "SORT doc.c DESC, doc.a, doc.b " +
          "WINDOW {preceding: 'unbounded'} AGGREGATE u = COUNT_DISTINCT(doc.c)" +
          "RETURN MERGE(doc, {unique: u})"
        , null, { waitForSync: true }).toArray();

        assertEqual(result.length, expected.length);
        var i = 0;
        result.forEach(function(res) {
          var doc = expected[i++];
          assertEqual(doc.a, res.a);
          assertEqual(doc.b, res.b);
          assertEqual(doc.c, res.c);
          assertEqual(doc.unique, res.unique);
        });
      },

      testViewInInnerLoopSortByAttributeWithNonDeterministic : function() {
        var expected = [];
        expected.push({ a: "bar", b: "foo", c: 1 });
        expected.push({ a: "baz", b: "foo", c: 1 });
        expected.push({ a: "foo", b: "bar", c: 0 });
        expected.push({ a: "foo", b: "baz", c: 0 });

        var result = db._query(
          "FOR adoc IN AnotherUnitTestsCollection " +
          "FOR doc IN UnitTestsView SEARCH RAND() != -10 && STARTS_WITH(doc['a'], adoc.a) && adoc.id == doc.c OPTIONS { waitForSync : true } " +
          "SORT doc.c DESC, doc.a, doc.b " +
          "RETURN doc"
        ).toArray();

        assertEqual(result.length, expected.length);
        var i = 0;
        result.forEach(function(res) {
          var doc = expected[i++];
          assertEqual(doc.a, res.a);
          assertEqual(doc.b, res.b);
          assertEqual(doc.c, res.c);
        });
      },

      testJoinTwoViewsSortByAttribute : function() {
        var expected = [];
        expected.push({ a: "bar", b: "foo", c: 1 });
        expected.push({ a: "baz", b: "foo", c: 1 });
        expected.push({ a: "foo", b: "bar", c: 1 });
        expected.push({ a: "foo", b: "baz", c: 1 });
        expected.push({ a: "bar", b: "foo", c: 0 });
        expected.push({ a: "baz", b: "foo", c: 0 });
        expected.push({ a: "foo", b: "bar", c: 0 });
        expected.push({ a: "foo", b: "baz", c: 0 });

        var query = "FOR doc0 IN CompoundView OPTIONS { collections: ['UnitTestsCollection2'], waitForSync:true } " +
                  "  FOR doc1 IN UnitTestsView SEARCH doc0.c == doc1.c && STARTS_WITH(doc1['a'], doc0.a) OPTIONS { waitForSync: true } " +
                  "FILTER doc1.c < 2 " +
                  "SORT doc1.c DESC, doc1.a, doc1.b " +
                  "RETURN doc1";

        var viewNodes = getNodes(query, "EnumerateViewNode");
        assertEqual(2, viewNodes.length);
        var remoteNodes = getNodes(query, "RemoteNode");
        assertEqual(3, remoteNodes.length);

        var viewNode0 = viewNodes[0];
        assertNotUndefined(viewNode0);
        assertEqual(db.UnitTestsCollection2.count(), viewNode0.estimatedNrItems);
        assertEqual(viewNode0.estimatedCost, viewNode0.estimatedNrItems + 1);

        var viewNode1 = viewNodes[1];
        assertNotUndefined(viewNode1);
        assertEqual(db.UnitTestsCollection2.count()*db.UnitTestsCollection.count(), viewNode1.estimatedNrItems);
        assertEqual(viewNode1.estimatedCost, remoteNodes[1].estimatedCost + viewNode1.estimatedNrItems);

        var result = db._query(query, null, { waitForSync: true }).toArray();

        assertEqual(result.length, expected.length);
        var i = 0;
        result.forEach(function(res) {
          var doc = expected[i++];
          assertEqual(doc.a, res.a);
          assertEqual(doc.b, res.b);
          assertEqual(doc.c, res.c);
        });
      },

      testWithKeywordForViewInGraph : function() {
        var results = [];

        results[0] = db._query(
          "WITH UnitTestsGraphCollection " + 
          "FOR doc IN UnitTestsGraphView " +
          "SEARCH doc.vName == 'vBegin' OPTIONS {waitForSync: true} " +
          "FOR v IN 2..2 OUTBOUND doc UnitTestsGraph " +
          "RETURN v").toArray();

        results[1] = db._query(
          "WITH UnitTestsGraphView " +
          "FOR doc IN UnitTestsGraphView " +
          "SEARCH doc.vName == 'vBegin' OPTIONS {waitForSync: true} " +
          "FOR v IN 2..2 OUTBOUND doc UnitTestsGraph " +
          "RETURN v").toArray();

        results[2] = db._query(
          "WITH UnitTestsGraphCollection, UnitTestsGraphView " +
          "FOR doc IN UnitTestsGraphView " +
          "SEARCH doc.vName == 'vBegin' OPTIONS {waitForSync: true} " +
          "FOR v IN 2..2 OUTBOUND doc UnitTestsGraph " +
          "RETURN v").toArray();

        results.forEach(function(res, index) {
          assertTrue(res.length, 1, "result length not 1 at index " + index);
          assertEqual(res[0].vName, "vEnd");
        });
      },

      testViewInSubquery : function() {
        var entitiesData = [
          {
            "_key": "person1",
            "_id": "entities/person1",
            "_rev": "_YOr40eu--_",
            "type": "person",
            "id": "person1"
          },
          {
            "_key": "person5",
            "_id": "entities/person5",
            "_rev": "_YOr48rO---",
            "type": "person",
            "id": "person5"
          },
          {
            "_key": "person4",
            "_id": "entities/person4",
            "_rev": "_YOr5IGu--_",
            "type": "person",
            "id": "person4"
          },
          {
            "_key": "person3",
            "_id": "entities/person3",
            "_rev": "_YOr5PBK--_",
            "type": "person",
            "id": "person3"
          },
          {
            "_key": "person2",
            "_id": "entities/person2",
            "_rev": "_YOr5Umq--_",
            "type": "person",
            "id": "person2"
          }
        ];

        var linksData = [
          {
            "_key": "3301",
            "_id": "links/3301",
            "_from": "entities/person1",
            "_to": "entities/person2",
            "_rev": "_YOrbp_S--_",
            "type": "relationship",
            "subType": "married",
            "from": "person1",
            "to": "person2"
          },
          {
            "_key": "3377",
            "_id": "links/3377",
            "_from": "entities/person4",
            "_to": "entities/person5",
            "_rev": "_YOrbxN2--_",
            "type": "relationship",
            "subType": "married",
            "from": "person4",
            "to": "person5"
          },
          {
            "_key": "3346",
            "_id": "links/3346",
            "_from": "entities/person1",
            "_to": "entities/person3",
            "_rev": "_YOrb4kq--_",
            "type": "relationship",
            "subType": "married",
            "from": "person1",
            "to": "person3"
          }
        ];

        try {
          // create entities collection
          var entities = db._createDocumentCollection("entities");

          entitiesData.forEach(function(doc) {
            entities.save(doc);
          });

          // create links collection
          var links = db._createEdgeCollection("links", args);
          linksData.forEach(function(doc) {
            links.save(doc);
          });

          var entitiesView = db._createView("entities_view", "arangosearch", {
            "writebufferSizeMax": 33554432,
            "consolidationPolicy": {
              "type": "bytes_accum",
              "threshold": 0.10000000149011612
            },
            "writebufferActive": 0,
            "consolidationIntervalMsec": 60000,
            "cleanupIntervalStep": 10,
            "links": {
              "entities": {
                "analyzers": [
                  "identity"
                ],
                "fields": {},
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            },
            "type": "arangosearch",
            "writebufferIdle": 64
          });

          var linksView = db._createView("links_view", "arangosearch", {
            "writebufferSizeMax": 33554432,
            "consolidationPolicy": {
              "type": "bytes_accum",
              "threshold": 0.10000000149011612
            },
            "writebufferActive": 0,
            "consolidationIntervalMsec": 60000,
            "cleanupIntervalStep": 10,
            "links": {
              "links": {
                "analyzers": [
                  "identity"
                ],
                "fields": {},
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            },
            "type": "arangosearch",
            "writebufferIdle": 64
          });

          var expectedResult = [
            { id: "person1", marriedIds: ["person2", "person3"] },
            { id: "person2", marriedIds: ["person1" ] },
            { id: "person3", marriedIds: ["person1" ] },
            { id: "person4", marriedIds: ["person5" ] },
            { id: "person5", marriedIds: ["person4" ] }
          ];

          var queryString = 
            "FOR org IN entities_view SEARCH org.type == 'person' OPTIONS {waitForSync:true} " + 
            "LET marriedIds = ( " +
            " LET entityIds = ( " +
            " FOR l IN links_view SEARCH l.type == 'relationship' AND l.subType == 'married' AND (l.from == org.id OR l.to == org.id) OPTIONS {waitForSync:true} " +
            "    RETURN DISTINCT l.from == org.id ? l.to : l.from" +
            "  ) " +
            "  FOR entityId IN entityIds SORT entityId RETURN entityId " +
            ") " +
            "LIMIT 10 " +
            "SORT org._key " + 
            "RETURN { id: org._key, marriedIds: marriedIds }";

          var result = db._query(queryString).toArray();

          assertEqual(result.length, expectedResult.length);

          var i = 0;
          result.forEach(function(doc) {
            var expectedDoc = expectedResult[i++];
            assertEqual(expectedDoc.org, doc.org);
            assertEqual(expectedDoc.marriedIds, doc.marriedIds);
          });
        } finally {
          db._dropView("entities_view");
          db._dropView("links_view");
          db._drop("entities");
          db._drop("links");
        }
      },

      testAttributeInRangeOpenInterval : function () {
        var query = "FOR doc IN UnitTestsView SEARCH IN_RANGE(doc.c, 1, 3, false, false) OPTIONS { waitForSync : true } RETURN doc";

        var viewNode = getNodes(query, "EnumerateViewNode")[0];
        assertNotUndefined(viewNode);
        assertEqual(db.UnitTestsCollection.count(), viewNode.estimatedNrItems);
        assertEqual(viewNode.estimatedCost, viewNode.estimatedNrItems + 1);

        var result = db._query(query).toArray();
        assertEqual(result.length, 4);
        result.forEach(function(res) {
          assertTrue(res.c > 1 && res.c < 3);
        });
      },

      testAttributeInRangeClosedInterval : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH IN_RANGE(doc.c, 1, 3, true, true) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 12);
        result.forEach(function(res) {
          assertTrue(res.c >= 1 && res.c <= 3);
        });
      },

      testScorersWithDistinct : function() {
        var result = db._query(
          "LET values = (FOR y IN UnitTestsCollection FILTER y.c == 0 RETURN DISTINCT y.a) " +
          "LET x = FIRST(FOR x IN values FILTER x == 'foo' RETURN x) " +
          "FOR doc IN UnitTestsView SEARCH doc.a == x && doc.c == 0 OPTIONS { waitForSync: true } " +
          "LET score = BM25(doc, 20.01, 0.01)*0.0001 + 1.0 " + 
          "COLLECT a = doc.a AGGREGATE maxScore = MAX(score) " +
          "RETURN { a: a, score: maxScore }"
        ).toArray();

        assertEqual(result.length, 1);
        result.forEach(function (res) {
          assertEqual(res.a, "foo");
          assertTrue(res.score > 1 && res.score < 2);
        });
      },

      testScorersWithDistinctWINDOW : function() {
        var result = db._query(
          "LET values = (FOR y IN UnitTestsCollection FILTER y.c == 0 RETURN DISTINCT y.a) " +
          "LET x = FIRST(FOR x IN values FILTER x == 'foo' RETURN x) " +
          "FOR doc IN UnitTestsView SEARCH doc.a == x && doc.c == 0 OPTIONS { waitForSync: true } " +
          "LET score = BM25(doc, 20.01, 0.01)*0.0001 + 1.0 " + 
          "WINDOW {preceding: 100} AGGREGATE maxScore = MAX(score) " +
          "RETURN { a: doc.a, score: maxScore }"
        ).toArray();

        let max = 0;
        result.forEach(function (res) {
          assertEqual(res.a, "foo");
          assertTrue(res.score > 1 && res.score < 2);
          assertTrue(max <= res.score);
          max = res.score;
        });
      },

      testAttributeNotInRangeOpenInterval : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH NOT(IN_RANGE(doc.c, 1, 3, false, false)) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 24);
        result.forEach(function(res) {
          assertTrue(res.c === undefined || res.c <= 1 || res.c >= 3);
        });
      },
      testAttributeNotInRangeClosedInterval : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH NOT(IN_RANGE(doc.c, 1, 3, true, true)) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 16);
        result.forEach(function(res) {
          assertTrue(res.c === undefined || res.c < 1 || res.c > 3);
        });
      },
      testAttributeInRange : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c IN 1..3 OPTIONS { waitForSync : true } RETURN doc").toArray();
       
        assertEqual(result.length, 12);
        result.forEach(function(res) {
          assertTrue(res.c >= 1 || res.c <= 3);
        });
      },
      testAttributeNotInRange : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c NOT IN 1..3 OPTIONS { waitForSync : true } RETURN doc").toArray();
      
        assertEqual(result.length, 16);
        result.forEach(function(res) {
          assertTrue(res.c === undefined || res.c < 1 || res.c > 3);
        });
      },
      testAttributeInArray : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c IN [ 1, 3 ] OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 8);
        result.forEach(function(res) {
          assertTrue(res.c === 1 || res.c === 3);
        });
      },
      testAttributeNotInArray : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH doc.c NOT IN [ 1, 3 ] OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 20);
        result.forEach(function(res) {
          assertTrue(res.c === undefined || res.c !== 1 && res.c !== 3);
        });
      },
      testAttributeInExpression : function () {
        var result = db._query("FOR c IN [[[1, 3]]] FOR doc IN UnitTestsView  SEARCH 1 IN FLATTEN(c) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, db.UnitTestsCollection.toArray().length);
      },
      testAttributeNotInExpression: function () {
        var result = db._query("FOR c IN [[[1, 3]]] FOR doc IN UnitTestsView  SEARCH 1 NOT IN FLATTEN(c) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 0);
      },
      testAttributeInExpressionNonDet : function () {
        var result = db._query("FOR c IN [[[1, 3]]] FOR doc IN UnitTestsView  SEARCH 1 IN NOOPT(FLATTEN(c)) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, db.UnitTestsCollection.toArray().length);
      },
      testAttributeNotInExpressionNonDet: function () {
        var result = db._query("FOR c IN [[[1, 3]]] FOR doc IN UnitTestsView  SEARCH 1 NOT IN NOOPT(FLATTEN(c)) OPTIONS { waitForSync : true } RETURN doc").toArray();

        assertEqual(result.length, 0);
      },
      testAnalyzerFunctionPrematureCall : function () {
        assertEqual(
          db._query("FOR d in UnitTestsView SEARCH ANALYZER(d.a IN TOKENS('#', 'text_en'), 'text_en') OPTIONS { waitForSync : true } RETURN d").toArray().length,
          0);
        assertEqual(
          db._query("FOR d in UnitTestsView SEARCH ANALYZER(d.a NOT IN TOKENS('#', 'text_en'), 'text_en') OPTIONS { waitForSync : true } RETURN d").toArray().length,
          28);
      },
      testBoostFunctionPrematureCall : function () {
        assertEqual(
          db._query("FOR d in UnitTestsView SEARCH BOOST(d.a IN TOKENS('#', 'text_en'), 2) OPTIONS { waitForSync : true }  SORT BM25(d) RETURN d").toArray().length,
          0);
        assertEqual(
          db._query("FOR d in UnitTestsView SEARCH BOOST(d.a NOT IN TOKENS('#', 'text_en'), 2) OPTIONS { waitForSync : true }  SORT BM25(d) RETURN d").toArray().length,
          28);
      },
      testMinMatchFunctionPrematureCall : function () {
        assertEqual(
          db._query("FOR d in UnitTestsView SEARCH MIN_MATCH(d.a IN TOKENS('#', 'text_en'), d.a IN TOKENS('#', 'text_de'), 1) OPTIONS { waitForSync : true } RETURN d").toArray().length,
          0);
        assertEqual(
          db._query("FOR d in UnitTestsView SEARCH MIN_MATCH(false, true, true, 2) OPTIONS { waitForSync : true }  RETURN d").toArray().length,
          28);
        assertEqual(
          db._query("FOR d in UnitTestsView SEARCH MIN_MATCH(false, false, false, 0) OPTIONS { waitForSync : true }  RETURN d").toArray().length,
          28);
      },
      testCheckIssue10090 : function () {
        let docs = [{ "type": "person", "text": "foo" }, { "type": "person", "text": "foo bar" }];
        let res = [];
        docs.forEach(function(element, i) {
          try
          {
            let c, v;

            c = db._create("cIssue10090", { waitForSync: true});
            v = db._createView("vIssue10090", "arangosearch",
              {
                links: {
                  "cIssue10090": {
                    analyzers: [
                      "identity"
                    ],
                    fields: {
                      "text": {
                        analyzers: [
                          "text_en",
                          "identity"
                        ]
                      },
                      "type": {}
                    },
                  }
                }
              }
            );

            c.save(i === 0 ? docs : docs.reverse());
            res.push(db._query("FOR d IN vIssue10090 SEARCH d.type == 'person' AND PHRASE(d.text, 'foo bar', 'text_en') OPTIONS { waitForSync: true } RETURN d").toArray());
          }
          finally {
            db._dropView("vIssue10090");
            db._drop("cIssue10090");
          }
        });

        docs.forEach(function(element, i) { assertEqual(res[i].length, 1); });
      },
      testArrayComparsionOperatorsInOnSimpleField : function () {
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] ANY IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(15, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a === 'foo' || doc.a === 'bar');
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  [] ANY IN d.a RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] ANY NOT IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  [] ANY NOT IN d.a RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo'] ANY NOT IN d.a RETURN d").toArray();
          assertEqual(18, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a !== 'foo');
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] NONE IN d.a RETURN d").toArray();
          assertEqual(13, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a !== 'foo' && doc.a !== 'bar');
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  [] NONE IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] NONE NOT IN d.a RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  ['bar'] NONE NOT IN d.a RETURN d").toArray();
          assertEqual(5, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a === 'bar');
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  [] NONE NOT IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] ALL IN d.a RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  [] ALL IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  ['bar'] ALL IN d.a RETURN d").toArray();
          assertEqual(5, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a === 'bar');
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  ['foo', 'bar'] ALL NOT IN d.a RETURN d").toArray();
          assertEqual(13, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a !== 'foo' && doc.a !== 'bar');
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH  [] ALL NOT IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
      },
      testArrayComparsionOperatorsInOnSimpleFieldWitScopedValue : function () {
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.foobar ANY IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(15, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a === 'foo' || doc.a === 'bar');
          });
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.empty ANY IN d.a RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.foobar ANY NOT IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.empty ANY NOT IN d.a RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.foo ANY NOT IN d.a RETURN d").toArray();
          assertEqual(18, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a !== 'foo');
          });
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.foobar NONE IN d.a RETURN d").toArray();
          assertEqual(13, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a !== 'foo' && doc.a !== 'bar');
          });
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.empty NONE IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.foobar NONE NOT IN d.a RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.bar NONE NOT IN d.a RETURN d").toArray();
          assertEqual(5, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a === 'bar');
          });
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.empty NONE NOT IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.foobar ALL IN d.a RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.empty ALL IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.bar ALL IN d.a RETURN d").toArray();
          assertEqual(5, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a === 'bar');
          });
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.foobar ALL NOT IN d.a RETURN d").toArray();
          assertEqual(13, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.a !== 'foo' && doc.a !== 'bar');
          });
        }
        {
          let result = db._query("FOR a IN AuxUnitTestsCollection " + 
            "FOR d IN UnitTestsView SEARCH  a.empty ALL NOT IN d.a RETURN d").toArray();
          assertEqual(28, result.length);
        }
      },
      testArrayComparsionOperatorsInOnArrayField : function() {
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['foo', 'bar'] ALL IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(1, result.length);
          assertEqual(0, result[0].c);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['foo', 'bar', 'none'] ALL IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  [ 'none', 'nani', 'afoo'] ALL NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(1, result.length);
          assertEqual(0, result[0].c);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['afoo', 'foo', 'none', 'nani'] ALL NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['none', 'bar'] ANY IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(1, result.length);
          assertEqual(0, result[0].c);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  [ 'nani', 'none'] ANY IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  [ 'foo', 'none', 'nani', 'afoo'] ANY NOT IN d.a OPTIONS { waitForSync : true } SORT d.c ASC RETURN d").toArray();
          assertEqual(2, result.length);
          assertEqual(0, result[0].c);
          assertEqual(1, result[1].c);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  [ 'foo', 'bar'] ANY NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(1, result.length);
          assertEqual(1, result[0].c);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['none', 'nani', 'afoo'] NONE IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(1, result.length);
          assertEqual(0, result[0].c);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['none', 'nani', 'bar', 'afoo'] NONE IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['none', 'foo', 'bar'] NONE NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsWithArrayView SEARCH  ['bar', 'foo'] NONE NOT IN d.a OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(1, result.length);
          assertEqual(0, result[0].c);
        }
      },
      testArrayComparsionOperatorsGreaterOnSimpleField : function() {
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [2, 3, 4] ALL > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 0 || doc.c === 1);
          });
        }
        {
          let result = db._query("FOR a IN [[2, 3, 4]] FOR d IN UnitTestsView SEARCH a ALL > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 0 || doc.c === 1);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [2, 3, 4] ALL >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(12, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 0 || doc.c === 1 || doc.c === 2);
          });
        }
        {
          let result = db._query("FOR a IN [[2, 3, 4]] FOR d IN UnitTestsView SEARCH a ALL >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(12, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 0 || doc.c === 1 || doc.c === 2);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] ALL > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ALL > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] ALL >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ALL >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2] ANY > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c < 2);
          });
        }
        {
          let result = db._query("FOR a IN [[1, 2]] FOR d IN UnitTestsView SEARCH a ANY > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c < 2);
          });

        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2] ANY >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(12, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c <= 2);
          });
        }
        {
          let result = db._query("FOR a IN [[1, 2]] FOR d IN UnitTestsView SEARCH a ANY >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(12, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c <= 2);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] ANY > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ANY > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] ANY >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ANY >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2, 3] NONE > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4 || doc.c === 3);
          });
        }
        {
          let result = db._query("FOR a IN [[1, 2, 3]] FOR d IN UnitTestsView SEARCH a NONE > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4 || doc.c === 3);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2, 3] NONE >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(4, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4);
          });
        }
        {
          let result = db._query("FOR a IN [[1, 2, 3]] FOR d IN UnitTestsView SEARCH a NONE >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(4, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] NONE > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a NONE > d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] NONE >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a NONE >= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
      },
      testArrayComparsionOperatorsLessOnSimpleField : function() {
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [2, 3] ALL < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(4, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4);
          });
        }
        {
          let result = db._query("FOR a IN [[2,3]] FOR d IN UnitTestsView SEARCH a ALL < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(4, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [2, 3] ALL <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4 || doc.c === 3);
          });
        }
        {
          let result = db._query("FOR a IN [[2,3]] FOR d IN UnitTestsView SEARCH a ALL <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4 || doc.c === 3);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] ALL < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ALL < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] ALL <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ALL <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [3, 4] ANY < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(4, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4);
          });
        }
        {
          let result = db._query("FOR a IN [[3,4]] FOR d IN UnitTestsView SEARCH a ANY < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(4, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [3, 4] ANY <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4 || doc.c === 3);
          });
        }
        {
          let result = db._query("FOR a IN [[3,4]] FOR d IN UnitTestsView SEARCH a ANY <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 4 || doc.c === 3);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] ANY < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ANY < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] ANY <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a ANY <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(0, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2, 3] NONE < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 1 || doc.c === 0);
          });
        }
        {
          let result = db._query("FOR a IN [[1,2,3]] FOR d IN UnitTestsView SEARCH a NONE < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(8, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 1 || doc.c === 0);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [1, 2, 3] NONE <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(4, result.length);
          result.forEach(function(doc) {
            assertTrue( doc.c === 0);
          });
        }
        {
          let result = db._query("FOR a IN [[1,2,3]] FOR d IN UnitTestsView SEARCH a NONE <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(4, result.length);
          result.forEach(function(doc) {
            assertTrue(doc.c === 0);
          });
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] NONE < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a NONE < d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR d IN UnitTestsView SEARCH [] NONE <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
        {
          let result = db._query("FOR a IN [[]] FOR d IN UnitTestsView SEARCH a NONE <= d.c OPTIONS { waitForSync : true } RETURN d").toArray();
          assertEqual(28, result.length);
        }
      },
      testQueryOptimizationOptions : function() {
        let queryOptColl = "QueryOptOptionsCol";
        let queryOptView = "QueryOptView";
        try {
          db._drop(queryOptColl);
          db._dropView(queryOptView);
          
          let coll = db._create(queryOptColl);
          let view = db._createView(queryOptView, 'arangosearch',
                                   { links: { "QueryOptOptionsCol": { includeAllFields: true } } });
          coll.save({ value1: "1", value2: "A",
            valueArray: ["A", "B", "C"]
          }); 
          coll.save({ value1: "2", value2: "B",
            valueArray: ["D", "B", "C"]
          });
          coll.save({ value1: "3", value2: "C",
            valueArray: ["E", "D", "C"]
          });          
          coll.save({ value1: "4", value2: "D",
            valueArray: ["A", "E", "D"]
          });   
          coll.save({ value1: "5", value2: "E",
            valueArray: ["F", "G", "B"]
          });   
          
          // 1 2 3 4 expected
          let resAuto = db._query("FOR d IN " + queryOptView  + 
            " SEARCH ( NOT(d.value1 IN ['R']) AND d.value2 IN ['A', 'B', 'C', 'D'] OR d.valueArray IN ['D', 'A', 'B']) AND (d.valueArray == 'A' OR (d.valueArray == 'C' AND d.valueArray == 'D')) " + 
            " OPTIONS { waitForSync : true, conditionOptimization: 'auto' } SORT d.value1 ASC RETURN d").toArray();
          assertEqual(4, resAuto.length);  
          let resNoDnf = db._query("FOR d IN " + queryOptView  + 
            " SEARCH ( NOT(d.value1 IN ['R']) AND d.value2 IN ['A', 'B', 'C', 'D'] OR d.valueArray IN ['D', 'A', 'B']) AND (d.valueArray == 'A' OR (d.valueArray == 'C' AND d.valueArray == 'D')) " + 
            " OPTIONS { waitForSync : true, conditionOptimization: 'nodnf' } SORT d.value1 ASC RETURN d").toArray();
          assertEqual(resNoDnf, resAuto); 
          let resNoNeg = db._query("FOR d IN " + queryOptView  + 
            " SEARCH ( NOT(d.value1 IN ['R']) AND d.value2 IN ['A', 'B', 'C', 'D'] OR d.valueArray IN ['D', 'A', 'B']) AND (d.valueArray == 'A' OR (d.valueArray == 'C' AND d.valueArray == 'D')) " + 
            " OPTIONS { waitForSync : true, conditionOptimization: 'noneg' } SORT d.value1 ASC RETURN d").toArray();
          assertEqual(resNoNeg, resAuto);
          let resNone = db._query("FOR d IN " + queryOptView  + 
            " SEARCH ( NOT(d.value1 IN ['R']) AND d.value2 IN ['A', 'B', 'C', 'D'] OR d.valueArray IN ['D', 'A', 'B']) AND (d.valueArray == 'A' OR (d.valueArray == 'C' AND d.valueArray == 'D')) " + 
            " OPTIONS { waitForSync : true, conditionOptimization: 'none' } SORT d.value1 ASC RETURN d").toArray();
          assertEqual(resNone, resAuto);         
        } finally {
          db._drop(queryOptColl);
          db._dropView(queryOptView);
        }
      },
      testNGramMatch : function() {
        let queryColl = "NgramMatchCol";
        let queryView = "NgramMatchView";
        let queryAnalyzer = "ngram_match_myngram";
        try {
          db._drop(queryColl);
          db._dropView(queryView);
          analyzers.save(queryAnalyzer, 
                         "ngram", 
                         {
                           "min":2, 
                           "max":2, 
                           "streamType":"utf8", 
                           "preserveOriginal":false
                         },
                         ["frequency", "position", "norm"]);
          let coll = db._create(queryColl);
          let view = db._createView(queryView,
                                    "arangosearch",
                                    { 
                                      links: { 
                                              NgramMatchCol : { analyzers: [ queryAnalyzer],
                                                                includeAllFields: true, 
                                                                trackListPositions: true} 
                                              }
                                    });
          coll.save({ value1: "1", value2: "Jack Daniels"}); 
          coll.save({ value1: "2", value2: "Jack Sparrow"}); 
          coll.save({ value1: "3", value2: "Jack The Reaper"}); 
          coll.save({ value1: "4", value2: "Jackhammer"}); 
          coll.save({ value1: "5", value2: "Jacky Chan"}); 
          {
            let res = db._query("FOR d IN " + queryView  + 
              " SEARCH NGRAM_MATCH(d.value2, 'Jack Sparrow', 0.7, '" + queryAnalyzer + "') " + 
              " OPTIONS { waitForSync : true} SORT BM25(d) DESC RETURN d").toArray();
            assertEqual(1, res.length);  
            assertEqual('2', res[0].value1);
          }
          // same but with default threshold (0.7 also)
          {
            let res = db._query("FOR d IN " + queryView  + 
              " SEARCH NGRAM_MATCH(d.value2, 'Jack Sparrow', '" + queryAnalyzer + "') " + 
              " OPTIONS { waitForSync : true} SORT BM25(d) DESC RETURN d").toArray();
            assertEqual(1, res.length);  
            assertEqual('2', res[0].value1);
          }
          // same but with default threshold (0.7 also) and analyzer via ANALYZER func
          {
            let res = db._query("FOR d IN " + queryView  + 
              " SEARCH ANALYZER(NGRAM_MATCH(d.value2, 'Jack Sparrow'), '" + queryAnalyzer + "') " + 
              " OPTIONS { waitForSync : true} SORT BM25(d) DESC RETURN d").toArray();
            assertEqual(1, res.length);  
            assertEqual('2', res[0].value1);
          }
          {
            let res = db._query("FOR d IN " + queryView  + 
              " SEARCH NGRAM_MATCH(d.value2, 'Jack Doniels', 0.3, '" + queryAnalyzer + "') " + 
              " OPTIONS { waitForSync : true} SORT BM25(d) DESC RETURN d").toArray();
            assertEqual(3, res.length);  
            assertEqual('1', res[0].value1);
            assertEqual('3', res[1].value1);
            assertEqual('2', res[2].value1);
          }
          {
            let res = db._query("FOR d IN " + queryView  + 
              " SEARCH NGRAM_MATCH(d.value2, 'Jack Doniels', 0.3, '" + queryAnalyzer + "') " + 
              " OPTIONS { waitForSync : true} SORT TFIDF(d) DESC RETURN d").toArray();
            assertEqual(3, res.length);  
            assertEqual('1', res[0].value1);
            assertEqual('2', res[1].value1);
            assertEqual('3', res[2].value1);
          }
        } finally {
          db._drop(queryColl);
          db._dropView(queryView);
          analyzers.remove(queryAnalyzer, true);
        }
      },
      testNGramMatchFunction : function() {
        let queryAnalyzer = "ngram_match_myngram";
        try {
          analyzers.save(queryAnalyzer, 
                         "ngram", 
                         {
                           "min":2, 
                           "max":2, 
                           "streamType":"utf8", 
                           "preserveOriginal":false
                         },
                         ["frequency", "position", "norm"]);
          {
            let res = db._query("RETURN NGRAM_MATCH('Capitan Jack Sparrow', 'Jack Sparow', 1, '" + queryAnalyzer + "') ").toArray();
            assertEqual(1, res.length);  
            assertTrue(res[0]);
          }
          {
            let res = db._query("RETURN NGRAM_MATCH('Capitan Jack Sparrow', 'Jack Sparowe', 1, '" + queryAnalyzer + "') ").toArray();
            assertEqual(1, res.length);  
            assertFalse(res[0]);
          }
          {
            let res = db._query("RETURN NGRAM_MATCH('Capitan Jack Sparrow', 'Jack Sparowe', 0.9, '" + queryAnalyzer + "') ").toArray();
            assertEqual(1, res.length);  
            assertTrue(res[0]);
          }
        } finally {
          analyzers.remove(queryAnalyzer, true);
        }
      },

      testAnalyzerNotOrFalse : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(doc.a != 'foo' OR FALSE, 'identity') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertNotEqual(0, result.length);
      },

      testLevenshteinMatch0 : function() {
        var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'lazi', 0), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
        assertEqual(2, res.length);
        assertEqual("full", res[0].name);
        assertEqual("half", res[1].name);
      },

      testLevenshteinMatch1 : function() {
        var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'lzi', 1), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
        assertEqual(2, res.length);
        assertEqual("full", res[0].name);
        assertEqual("half", res[1].name);
      },

      testLevenshteinDamerauMatch1 : function() {
        var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'lzai', 1, true), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
        assertEqual(2, res.length);
        assertEqual("full", res[0].name);
        assertEqual("half", res[1].name);
      },

      testLevenshteinDamerauMatch1NoLimit : function() {
        var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'lzai', 1, true), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
        assertEqual(2, res.length);
        assertEqual("full", res[0].name);
        assertEqual("half", res[1].name);
      },

      testLevenshteinMatch2 : function() {
        var res = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(LEVENSHTEIN_MATCH(doc.text, 'dog', 2, false), 'text_en') OPTIONS { waitForSync : true } SORT doc.name RETURN doc").toArray();
        assertEqual(3, res.length);
        assertEqual("full", res[0].name);
        assertEqual("half", res[1].name);
        assertEqual("other half", res[2].name);
      },

      testPhraseTerm : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {TERM: 'quick'}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseTermViaArray : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{TERM: 'quick'}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseStartsWith : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {STARTS_WITH: 'qui'}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseStartsWithViaArray : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{STARTS_WITH: 'qui'}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseWildcard : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {WILDCARD: 'qu_ck'}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseWildcardViaArray : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{WILDCARD: 'qu_ck'}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseLevenshteinMatch : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {LEVENSHTEIN_MATCH: ['queck', 1, false]}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseLevenshteinMatchNoLimit : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {LEVENSHTEIN_MATCH: ['queck', 1, false, 0]}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseLevenshteinMatchViaArray : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{LEVENSHTEIN_MATCH: ['queck', 1, false]}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseTerms : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {TERMS: ['quick', 'fast']}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseTermsViaArray : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{TERMS: ['quick', 'fast']}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseInRange : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, {IN_RANGE: ['quic', 'ruick', false, true]}, 0, 'brown', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testPhraseInRangeViaArray : function () {
        var result = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [{IN_RANGE: ['quic', 'ruick', false, true]}, 'brown'], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();
        assertEqual(1, result.length);
      },

      testVolatileFilter : function() {
        let result = db._query("FOR doc IN AnotherUnitTestsCollection LET kk = NOEVAL(doc.id) "
                               + " FOR c IN UnitTestsWithArrayView SEARCH c.c == kk "
                               + " OPTIONS { waitForSync : true } SORT c.c RETURN c ").toArray();
        assertEqual(2, result.length);
        assertEqual(result[0].c, 0);
        assertEqual(result[1].c, 1);
      },
      
      testQueryWithMultipleSortView : function() {
        let res= db._query("FOR doc IN WithPrimarySort SEARCH ANALYZER(doc.field3 == 1, 'customAnalyzer') "
        + " OPTIONS { waitForSync : true } SORT doc._key  LIMIT 0, 50  RETURN doc ").toArray();
        assertEqual(1, res.length);
      },

      testGeo: function() {
        let queryColl = "GeoCollection";
        let queryView = "GeoView";
        let queryAnalyzer = "mygeo";
        let geoData = [
          { "id": 1,  "geometry": { "type": "Point", "coordinates": [ 37.615895, 55.7039   ] } },
          { "id": 2,  "geometry": { "type": "Point", "coordinates": [ 37.615315, 55.703915 ] } },
          { "id": 3,  "geometry": { "type": "Point", "coordinates": [ 37.61509, 55.703537  ] } },
          { "id": 4,  "geometry": { "type": "Point", "coordinates": [ 37.614183, 55.703806 ] } },
          { "id": 5,  "geometry": { "type": "Point", "coordinates": [ 37.613792, 55.704405 ] } },
          { "id": 6,  "geometry": { "type": "Point", "coordinates": [ 37.614956, 55.704695 ] } },
          { "id": 7,  "geometry": { "type": "Point", "coordinates": [ 37.616297, 55.704831 ] } },
          { "id": 8,  "geometry": { "type": "Point", "coordinates": [ 37.617053, 55.70461  ] } },
          { "id": 9,  "geometry": { "type": "Point", "coordinates": [ 37.61582, 55.704459  ] } },
          { "id": 10, "geometry": { "type": "Point", "coordinates": [ 37.614634, 55.704338 ] } },
          { "id": 11, "geometry": { "type": "Point", "coordinates": [ 37.613121, 55.704193 ] } },
          { "id": 12, "geometry": { "type": "Point", "coordinates": [ 37.614135, 55.703298 ] } },
          { "id": 13, "geometry": { "type": "Point", "coordinates": [ 37.613663, 55.704002 ] } },
          { "id": 14, "geometry": { "type": "Point", "coordinates": [ 37.616522, 55.704235 ] } },
          { "id": 15, "geometry": { "type": "Point", "coordinates": [ 37.615508, 55.704172 ] } },
          { "id": 16, "geometry": { "type": "Point", "coordinates": [ 37.614629, 55.704081 ] } },
          { "id": 17, "geometry": { "type": "Point", "coordinates": [ 37.610235, 55.709754 ] } },
          { "id": 18, "geometry": { "type": "Point", "coordinates": [ 37.605,    55.707917 ] } },
          { "id": 19, "geometry": { "type": "Point", "coordinates": [ 37.545776, 55.722083 ] } },
          { "id": 20, "geometry": { "type": "Point", "coordinates": [ 37.559509, 55.715895 ] } },
          { "id": 21, "geometry": { "type": "Point", "coordinates": [ 37.701645, 55.832144 ] } },
          { "id": 22, "geometry": { "type": "Point", "coordinates": [ 37.73735,  55.816715 ] } },
          { "id": 23, "geometry": { "type": "Point", "coordinates": [ 37.75589,  55.798193 ] } },
          { "id": 24, "geometry": { "type": "Point", "coordinates": [ 37.659073, 55.843711 ] } },
          { "id": 25, "geometry": { "type": "Point", "coordinates": [ 37.778549, 55.823659 ] } },
          { "id": 26, "geometry": { "type": "Point", "coordinates": [ 37.729797, 55.853733 ] } },
          { "id": 27, "geometry": { "type": "Point", "coordinates": [ 37.608261, 55.784682 ] } },
          { "id": 28, "geometry": { "type": "Point", "coordinates": [ 37.525177, 55.802825 ] } },
          { "id": 29, "geometry": { "type": "Polygon", "coordinates": [
            [[ 37.614323, 55.705898 ],
             [ 37.615825, 55.705898 ],
             [ 37.615825, 55.70652  ],
             [ 37.614323, 55.70652  ],
             [ 37.614323, 55.705898 ]]
          ]}}
       ];
  
        try {
          db._drop(queryColl);
          db._dropView(queryView);
          analyzers.save(queryAnalyzer, "geojson", {});
          let coll = db._create(queryColl);
          let view = db._createView(queryView, "arangosearch", { 
                links: { 
                    [queryColl] : { 
                        fields: {
                          id: { },
                          geometry: { analyzers: [queryAnalyzer] }
                        }
                    }
                }
            });
  
          geoData.forEach(doc => coll.save(doc));
  
          {
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_DISTANCE(@origin, d.geometry) < 100, @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "origin" : geoData[7].geometry,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ 7, 8, 9, 14 ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
          {
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_DISTANCE(@origin, d.geometry) > 0 && GEO_DISTANCE(@origin, d.geometry) < 100, @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "origin" : geoData[7].geometry,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ 7, 9, 14 ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }

          {
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_DISTANCE(@origin, d.geometry) >= 0 && GEO_DISTANCE(@origin, d.geometry) < 100, @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "origin" : geoData[7].geometry,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ 7, 8, 9, 14 ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
          {
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_IN_RANGE(@origin, d.geometry, 0, 100), @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "origin" : geoData[7].geometry,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ 7, 8, 9, 14 ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
          {
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_DISTANCE(d.geometry, @origin) < 100, @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "origin" : geoData[7].geometry,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ 7, 8, 9, 14 ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
          {
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_CONTAINS(d.geometry, @origin), @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "origin" : geoData[7].geometry,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ geoData[7].id ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
          {
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_CONTAINS(@origin, d.geometry), @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "origin" : geoData[7].geometry,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ geoData[7].id ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
  
          {
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_INTERSECTS(d.geometry, @origin), @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "origin" : geoData[7].geometry,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ geoData[7].id ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
          {
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_INTERSECTS(@origin, d.geometry), @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "origin" : geoData[7].geometry,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ geoData[7].id ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
          {
            let shape = {
                "type": "Polygon",
                "coordinates": [
                    [
                        [37.590322, 55.695583],
                        [37.626114, 55.695583],
                        [37.626114, 55.71488],
                        [37.590322, 55.71488],
                        [37.590322, 55.695583]
                    ]
                ]};
  
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_CONTAINS(@shape, d.geometry), @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "shape" : shape,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 29 ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
          {
            let shape = {
                "type": "Polygon",
                "coordinates": [
                    [
                        [37.590322, 55.695583],
                        [37.626114, 55.695583],
                        [37.626114, 55.71488],
                        [37.590322, 55.71488],
                        [37.590322, 55.695583]
                    ]
                ]};
  
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_INTERSECTS(@shape, d.geometry), @queryAnalyzer) && d.id > 15 " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "shape" : shape,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            let expected = [ 16, 17, 18, 29 ];
            assertEqual(expected.length, res.length);
            for (let i = 0; i < expected.length; ++i) {
              assertEqual(expected[0], res[0].id);
            }
          }
  
          {
            let shape = {
                "type": "Polygon",
                "coordinates": [
                    [
                        [37.590322, 55.695583],
                        [37.626114, 55.695583],
                        [37.626114, 55.71488],
                        [37.590322, 55.71488],
                        [37.590322, 55.695583]
                    ]
                ]};
  
            let queryString = 
              "FOR d IN @@view " + 
              "SEARCH ANALYZER(GEO_CONTAINS(d.geometry, @shape), @queryAnalyzer) " + 
              "OPTIONS { waitForSync: true } " +
              "SORT d.id ASC " + 
              "RETURN d";
  
            let bindVars = {
              "@view" : queryView,
              "shape" : shape,
              "queryAnalyzer" : queryAnalyzer
            };
            let res = db._query(queryString, bindVars).toArray();
            assertEqual(0, res.length);
          }
  
        } finally {
          db._drop(queryColl);
          db._dropView(queryView);
          analyzers.remove(queryAnalyzer, true);
        }
      }
    };
  };
}());
