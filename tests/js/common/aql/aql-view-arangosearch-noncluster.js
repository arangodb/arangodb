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

var jsunity = require("jsunity");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
var db = require("@arangodb").db;
var analyzers = require("@arangodb/analyzers");
var ERRORS = require("@arangodb").errors;

const base = require("fs").join(require('internal').pathForTesting('common'),
  'aql', 'aql-view-arangosearch-common-noncluster.inc');
const iResearchAqlTestSuite = require("internal").load(base);

jsunity.run(function IResearchAqlTestSuite_ArangoSearch_Noncluster() {
  let suite = {};

  deriveTestSuite(
    iResearchAqlTestSuite({ views: true, oldMangling: true}),
    suite,
    "_ArangoSearch_NonCluster"
  );
  suite.setUpAll = function () {
      suite.internal.createCollectionsAndData();
      db._dropView("UnitTestsViewArrayView");
      let arrayV = db._createView("UnitTestsWithArrayView", "arangosearch", {});

      let meta = {
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
                                
                                
      let wsv = db._createView("WithStoredValues", "arangosearch", 
                               {storedValues: [["field1"], ["field2"], ["field3"]]});
      wsv.properties({links:{TestsCollectionWithLongFields: {
                              storeValues: "id",
                              analyzers: ["customAnalyzer"],
                              fields: {
                                field1: {},
                                field2: {},
                                field3: {}}}}});
      let wpsl = db._createView("WithLongPrimarySort", "arangosearch", 
                               {primarySort: [{field: "field1", direction: "asc"}],
                                storedValues: [["field2"], ["field3"]]});
      wpsl.properties({links:{TestsCollectionWithLongFields: {
                              storeValues: "id",
                              analyzers: ["customAnalyzer"],
                              fields: {
                                field1: {},
                                field2: {},
                                field3: {}}}}});
                                
      {
        let queryView = "DisjunctionView";
        db._dropView(queryView);
        let view = db._createView(queryView, "arangosearch", { 
              links: { 
                  DisjunctionCollection : { 
                      fields: {
                        value: { analyzers:['identity', 'text_en'] }
                      }
                  }
              }
        });
      }
    };
  suite.tearDownAll = function () {
      db._dropView("UnitTestsWithArrayView");
      db._dropView("WithPrimarySort");
      db._dropView("WithStoredValues");
      db._dropView("WithLongPrimarySort");
      db._dropView("UnitTestsViewAlias");
      analyzers.remove("customAnalyzer", true);
      db._dropView("DisjunctionView");
      suite.internal.dropCollections();
    };
  suite.setUp = function () {
      db._drop("UnitTestsCollection");
      let c = db._create("UnitTestsCollection");

      db._drop("UnitTestsCollection2");
      let c2 = db._create("UnitTestsCollection2");

      db._dropView("UnitTestsView");
      let v = db._createView("UnitTestsView", "arangosearch", {});
      var meta = {
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
      db._createView("CompoundView", "arangosearch",
        { links : {
          UnitTestsCollection: { includeAllFields: true },
          UnitTestsCollection2 : { includeAllFields: true }
        }}
      );

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
      c.save({ name: "quarter", text: "quick quick over" });

      c.save({ name: "numeric", anotherNumericField: 0 });
      c.save({ name: "null", anotherNullField: null });
      c.save({ name: "bool", anotherBoolField: true });
      c.save({ _key: "foo", xyz: 1 });
    };

  suite.tearDown = function () {
      var meta = { links : { "UnitTestsCollection": null } };
      db._view("UnitTestsView").properties(meta);
      db._dropView("UnitTestsView");
      db._dropView("CompoundView");
      db._drop("UnitTestsCollection");
      db._drop("UnitTestsCollection2");
    };
  suite.testPhraseFilter = function () {
      var result0 = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, 'quick brown fox jumps', 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result0.length, 1);
      assertEqual(result0[0].name, 'full');

      var result1 = db._query("FOR doc IN UnitTestsView SEARCH PHRASE(doc.text, [ 'quick brown fox jumps' ], 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result1.length, 1);
      assertEqual(result1[0].name, 'full');

      var result2 = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text, 'quick brown fox jumps'), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result2.length, 1);
      assertEqual(result2[0].name, 'full');

      var result3 = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text, [ 'quick brown fox jumps' ]), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result3.length, 1);
      assertEqual(result3[0].name, 'full');
      
      var result4 = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text,  'quick ', 1, ' fox jumps'), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result4.length, 1);
      assertEqual(result4[0].name, 'full');
      
      var result5 = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text, [ 'quick ', 1, ' fox jumps' ]), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result5.length, 1);
      assertEqual(result5[0].name, 'full');
      
      var result6 = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text,  'quick ', 0, 'brown', 0, [' fox',  ' jumps']), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result6.length, 1);
      assertEqual(result6[0].name, 'full');
      
      var result6v = db._query("LET phraseStruct = NOOPT([' fox',  ' jumps']) "
                               + "FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text,  'quick ', 0, 'brown', 0, phraseStruct), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result6v.length, 1);
      assertEqual(result6v[0].name, 'full');
      
            
      var result6v2 = db._query("LET phraseStruct = NOOPT(['quick ', 0, 'brown', 0, [' fox',  ' jumps']]) "
                               + "FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text, phraseStruct), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result6v2.length, 1);
      assertEqual(result6v2[0].name, 'full');
      
      var result7 = db._query("FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text, [ 'quick ', 'brown', ' fox jumps' ]), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result7.length, 1);
      assertEqual(result7[0].name, 'full');
      
      var result7v = db._query("LET phraseStruct = NOOPT([ 'quick ', 'brown', ' fox jumps' ]) "
                              + "FOR doc IN UnitTestsView SEARCH ANALYZER(PHRASE(doc.text, phraseStruct), 'text_en') OPTIONS { waitForSync : true } RETURN doc").toArray();

      assertEqual(result7v.length, 1);
      assertEqual(result7v[0].name, 'full');
    };
  suite.testViewInSubquery = function() {
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
        var links = db._createEdgeCollection("links");
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
    };
  suite.testCheckIssue10090 = function () {
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
        } finally {
          db._dropView("vIssue10090");
          db._drop("cIssue10090");
        }
      });

      docs.forEach(function(element, i) { assertEqual(res[i].length, 1); });
    };
   
  suite.testQueryOptimizationOptions = function() {
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
    };
  suite.testNGramMatch = function() {
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
          assertEqual('2', res[1].value1);
          assertEqual('3', res[2].value1);
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
    };

  suite.testNGramMatchFunction =  function() {
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
    };
  suite.testGeo = function() {
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
    };
  return suite;
});
return jsunity.done();
