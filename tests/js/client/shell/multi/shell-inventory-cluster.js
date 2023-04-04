/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual, _ */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for inventory
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

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const analyzers = require("@arangodb/analyzers");
const isEnterprise = require("internal").isEnterprise();

function clusterInventorySuite () {
  let validateViewAttributes = function (view) {
    assertEqual("string", typeof view.globallyUniqueId);
    assertEqual("string", typeof view.id);
    assertEqual("string", typeof view.name);
    assertEqual("string", typeof view.type);
    assertEqual("number", typeof view.cleanupIntervalStep);
    assertEqual("number", typeof view.commitIntervalMsec);
    assertEqual("object", typeof view.consolidationPolicy);
    assertEqual("string", typeof view.consolidationPolicy.type);
    if (view.consolidationPolicy.hasOwnProperty("segmentsBytesFloor")) {
      assertEqual("number", typeof view.consolidationPolicy.segmentsBytesFloor);
    }
    if (view.consolidationPolicy.hasOwnProperty("segmentsBytesMax")) {
      assertEqual("number", typeof view.consolidationPolicy.segmentsBytesMax);
    }
    if (view.consolidationPolicy.hasOwnProperty("segmentsMax")) {
      assertEqual("number", typeof view.consolidationPolicy.segmentsMax);
    }
    if (view.consolidationPolicy.hasOwnProperty("segmentsMin")) {
      assertEqual("number", typeof view.consolidationPolicy.segmentsMin);
    }
    if (view.consolidationPolicy.hasOwnProperty("minScore")) {
      assertEqual("number", typeof view.consolidationPolicy.minScore);
    }
    if (view.consolidationPolicy.hasOwnProperty("threshold")) {
      assertEqual("number", typeof view.consolidationPolicy.threshold);
    }
    assertTrue(Array.isArray(view.primarySort));
    assertEqual("string", typeof view.primarySortCompression);
    assertTrue(Array.isArray(view.storedValues));
    view.storedValues.forEach(function(storedValue) {
      assertEqual("object", typeof storedValue);
      assertTrue(Array.isArray(storedValue.fields));
      assertEqual("string", typeof storedValue.compression);
    });
    //assertEqual("number", typeof view.version);
    assertEqual("number", typeof view.writebufferActive);
    assertEqual("number", typeof view.writebufferIdle);
    assertEqual("number", typeof view.writebufferSizeMax);

    assertEqual("object", typeof view.links);
    Object.keys(view.links).forEach(function(collection) {
      let link = view.links[collection];
      if (isEnterprise) {
        assertEqual(12, Object.keys(link).length);
      } else {
        assertEqual(11, Object.keys(link).length);
      }
      assertEqual("number", typeof link.version);
      assertEqual(1, link.version);
      assertTrue(Array.isArray(link.analyzerDefinitions));
      assertEqual("string", typeof link.collectionName);
      assertEqual(collection, link.collectionName);
      link.analyzerDefinitions.forEach(function(analyzer) {
        assertEqual("object", typeof analyzer);
        assertEqual("string", typeof analyzer.name);
        assertEqual("string", typeof analyzer.type);
        assertEqual("object", typeof analyzer.properties);
        assertTrue(Array.isArray(analyzer.features));
      });
      assertTrue(Array.isArray(link.analyzers));
      assertEqual("object", typeof link.fields);

      assertEqual("boolean", typeof link.includeAllFields);
      assertTrue(Array.isArray(link.primarySort));
      assertTrue(link.hasOwnProperty("storeValues"));
      assertTrue(link.hasOwnProperty("storedValues"));
      assertTrue(Array.isArray(link.storedValues));
      assertEqual("boolean", typeof link.trackListPositions);
    });
  };
  
  let validateSearchAliasAttributes = function (view) {
    assertEqual("string", typeof view.globallyUniqueId);
    assertEqual("string", typeof view.id);
    assertEqual("string", typeof view.name);
    assertEqual("string", typeof view.type);

    assertTrue(Array.isArray(view.indexes));
    view.indexes.forEach(function(index) {
      assertEqual("string", typeof index.collection);
      assertEqual(db._collection(index.collection).name(), index.collection);
      let indexes = db._collection(index.collection).indexes().filter((idx) => idx.name === index.index);
      assertEqual(1, indexes.length);
      assertEqual(indexes[0].name, index.index);
      assertEqual("inverted", indexes[0].type);
    });
  };

  let validateCollectionAttributes = function (collection) {
    let parameters = collection.parameters;
    assertEqual("object", typeof parameters);
    assertEqual("string", typeof parameters.globallyUniqueId);
    assertEqual("string", typeof parameters.id);
    assertEqual("boolean", typeof parameters.isSmart);
    assertEqual("boolean", typeof parameters.isSystem);
    assertEqual("object", typeof parameters.keyOptions);
    assertEqual("boolean", typeof parameters.keyOptions.allowUserKeys);
    assertEqual("string", typeof parameters.keyOptions.type);
    assertEqual("number", typeof parameters.minReplicationFactor);
    assertEqual("number", typeof parameters.writeConcern);
    assertEqual("string", typeof parameters.name);
    assertTrue(typeof parameters.replicationFactor === 'number' || parameters.replicationFactor === 'satellite');
    assertEqual("number", typeof parameters.status);
    assertEqual("number", typeof parameters.type);
    assertEqual("boolean", typeof parameters.waitForSync);

    let indexes = collection.indexes;
    assertTrue(Array.isArray(indexes));
    indexes.forEach(function (index) {
      assertEqual("string", typeof index.id, index);
      assertEqual("string", typeof index.type, index);
      assertEqual("string", typeof index.name, index);
      assertTrue(Array.isArray(index.fields), index);
      if (index.type === 'inverted') {
        index.fields.forEach(function (field) {
          assertEqual("string", typeof field.name);
        });
        assertEqual(index.analyzerDefinitions, [
          {
            "name": "custom",
            "type": "delimiter",
            "properties": {
              "delimiter": " "
            },
            "features": [
              "frequency"
            ]
          },
          {
            "name": "identity",
            "type": "identity",
            "properties": {},
            "features": [
              "frequency",
              "norm"
            ]
          }
        ]);
      } else {
        index.fields.forEach(function (field) {
          assertEqual("string", typeof field);
        });
      }
      assertEqual("boolean", typeof index.unique, index);
      assertEqual("boolean", typeof index.sparse, index);
      if (index.hasOwnProperty("deduplicate")) {
        assertEqual("boolean", typeof index.deduplicate, index);
      }
    });

  };

  return {

    setUpAll: function () {
      try {
        db._dropDatabase("UnitTestsDumpSrc");
      } catch (err1) {
      }
      db._createDatabase("UnitTestsDumpSrc");

      try {
        db._dropDatabase("UnitTestsDumpDst");
      } catch (err2) {
      }
      db._createDatabase("UnitTestsDumpDst");

      db._useDatabase("UnitTestsDumpSrc");

      db._create("UnitTestsDumpEmpty", { waitForSync: true });

      db._createEdgeCollection("UnitTestsDumpEdges");

      let c = db._create("UnitTestsDumpIndexes");
      c.ensureIndex({ type: "hash", fields: ["a_uc"], unique: true });
      c.ensureIndex({ type: "skiplist", fields: ["a_s1", "a_s2"] });
      c.ensureIndex({ type: "hash", fields: ["a_h1", "a_h2"] });
      c.ensureIndex({ type: "skiplist", fields: ["a_su"], unique: true });
      c.ensureIndex({ type: "hash", fields: ["a_hs1", "a_hs2"], sparse: true });
      c.ensureIndex({ type: "skiplist", fields: ["a_ss1", "a_ss2"], sparse: true });
      c.ensureIndex({ type: "fulltext", fields: ["a_f"] });
      c.ensureIndex({ type: "geo", fields: ["a_la", "a_lo"] });
      
      let analyzer = analyzers.save("custom", "delimiter", { delimiter : " " }, [ "frequency" ]);

      // setup an empty view
      db._createView("UnitTestsDumpViewEmpty", "arangosearch", {});

      let view = db._createView("UnitTestsDumpView", "arangosearch", {});
      let viewMeta = {};
      if (isEnterprise) {
        viewMeta = {
          cleanupIntervalStep: 456,
          consolidationPolicy: {
            threshold: 0.3,
            type: "bytes_accum"
          },
          commitIntervalMsec: 12345,
          consolidationIntervalMsec: 0,
          links: {
            "UnitTestsDumpEmpty" : {
              includeAllFields: true,
              fields: {
                text: { analyzers: [ "text_en", analyzer.name ], "fields": { "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}} }
              }
            }
          }
        };
      } else {
        viewMeta = {
          cleanupIntervalStep: 456,
          consolidationPolicy: {
            threshold: 0.3,
            type: "bytes_accum"
          },
          commitIntervalMsec: 12345,
          consolidationIntervalMsec: 0,
          links: {
            "UnitTestsDumpEmpty" : {
              includeAllFields: true,
              fields: {
                text: { analyzers: [ "text_en", analyzer.name ], "fields": { "value": {}}}
              }
            }
          }
        };
      }
      view.properties(viewMeta);
      
      c = db._create("UnitTestsDumpSearchAliasCollection");
      let idx = c.ensureIndex({ type: "inverted", fields: [{ name: "value", analyzer: analyzer.name }] });
     
      view = db._createView("UnitTestsDumpViewSearchAlias", "search-alias", {});
      view.properties({
        indexes: [
          {
            "collection": c.name(),
            "index": idx.name
          }
        ]
      });
      
      c = db._create("UnitTestsDumpSearchAliasCollection2");
      idx = c.ensureIndex({ type: "inverted", fields: [{ name: "value", analyzer: analyzer.name }] });
     
      db._createView("UnitTestsDumpViewSearchAlias2", "search-alias", {
        indexes: [
          {
            "collection": c.name(),
            "index": idx.name
          }
        ]
      });
     
      db._create("UnitTestsDumpShards", { numberOfShards : 9 });
      if (isEnterprise) {
        db._create("UnitTestsDumpSatelliteCollection", {"replicationFactor": "satellite"});
      }

      db._create("UnitTestsDumpReplicationFactor1", { replicationFactor: 1, numberOfShards: 7 });
      db._create("UnitTestsDumpReplicationFactor2", { replicationFactor: 2, numberOfShards: 6 });
    },
    
    tearDownAll : function () {
      db._useDatabase("_system");
      db._dropDatabase("UnitTestsDumpSrc");
      db._dropDatabase("UnitTestsDumpDst");
    },

    testData : function () {
      db._useDatabase("UnitTestsDumpSrc");
      
      let results = arango.GET("/_api/replication/clusterInventory");
      assertTrue(results.hasOwnProperty("collections"));

      let collections = results.collections;
      let byName = {};
      collections.forEach(function (collection) {
        validateCollectionAttributes(collection);
        byName[collection.parameters.name] = collection;
      });
      
      let collection = byName["UnitTestsDumpEmpty"];
      assertEqual(2, collection.parameters.type);
      assertEqual(0, collection.indexes.length);
      
      collection = byName["UnitTestsDumpEdges"];
      assertEqual(3, collection.parameters.type);
      assertEqual(0, collection.indexes.length);

      collection = byName["UnitTestsDumpIndexes"];
      assertEqual(8, collection.indexes.length);

      assertEqual(["a_uc"], collection.indexes[0].fields);
      assertEqual("hash", collection.indexes[0].type);
      assertFalse(collection.indexes[0].sparse);
      assertTrue(collection.indexes[0].unique);
      assertEqual(["a_s1", "a_s2"], collection.indexes[1].fields);
      assertEqual("skiplist", collection.indexes[1].type);
      assertFalse(collection.indexes[1].sparse);
      assertFalse(collection.indexes[1].unique);
      assertEqual(["a_h1", "a_h2"], collection.indexes[2].fields);
      assertEqual("hash", collection.indexes[2].type);
      assertFalse(collection.indexes[2].sparse);
      assertFalse(collection.indexes[2].unique);
      assertEqual(["a_su"], collection.indexes[3].fields);
      assertEqual("skiplist", collection.indexes[3].type);
      assertFalse(collection.indexes[3].sparse);
      assertTrue(collection.indexes[3].unique);
      assertEqual(["a_hs1", "a_hs2"], collection.indexes[4].fields);
      assertEqual("hash", collection.indexes[4].type);
      assertTrue(collection.indexes[4].sparse);
      assertFalse(collection.indexes[4].unique);
      assertEqual(["a_ss1", "a_ss2"], collection.indexes[5].fields);
      assertEqual("skiplist", collection.indexes[5].type);
      assertTrue(collection.indexes[5].sparse);
      assertFalse(collection.indexes[5].unique);
      assertEqual(["a_f"], collection.indexes[6].fields);
      assertEqual("fulltext", collection.indexes[6].type);
      assertEqual(["a_la", "a_lo"], collection.indexes[7].fields);
      assertEqual("geo", collection.indexes[7].type);
      
      assertTrue(results.hasOwnProperty("views"));
      assertTrue(Array.isArray(results.views));
      assertEqual(4, results.views.length);
      
      results.views.forEach(function (view) {
        assertTrue(view.type === "arangosearch" || view.type === "search-alias");
        if (view.type === "arangosearch") {
          validateViewAttributes(view);
        } else if (view.type === "search-alias") {
          validateSearchAliasAttributes(view);
        }
      });

      // make view result order deterministic
      results.views.sort(function(l, r) {
        if (l.name !== r.name) {
          return l.name < r.name ? -1 : 1;
        }
        return 0;
      });

      let view = results.views[0];
      assertEqual("arangosearch", view.type);
      assertEqual("UnitTestsDumpView", view.name);
      assertEqual(456, view.cleanupIntervalStep);
      assertEqual(12345, view.commitIntervalMsec);
      assertEqual(0, view.consolidationIntervalMsec);
      assertEqual("bytes_accum", view.consolidationPolicy.type);
      assertEqual(0.3.toFixed(2), view.consolidationPolicy.threshold.toFixed(2));
      assertEqual([], view.primarySort);

      let links = view.links;
      assertEqual(1, Object.keys(links).length);
      assertEqual("UnitTestsDumpEmpty", Object.keys(links)[0]);
      let link = links["UnitTestsDumpEmpty"];
      if (isEnterprise) {
        assertEqual(12, Object.keys(link).length);
      } else {
        assertEqual(11, Object.keys(link).length);
      }
      assertEqual("number", typeof link.version);
      assertEqual(1, link.version);
      assertEqual("string", typeof link.collectionName);
      assertEqual("UnitTestsDumpEmpty", link.collectionName);
      assertTrue(link.includeAllFields);
      assertEqual([], link.primarySort);
      assertEqual("none", link.storeValues);
      assertFalse(link.trackListPositions);

      assertTrue(Array.isArray(link.analyzers));
      assertEqual(1, link.analyzers.length);
      assertEqual("identity", link.analyzers[0]);

      assertEqual(1, Object.keys(link.fields).length);
      assertEqual("text", Object.keys(link.fields)[0]);
      let field = link.fields["text"];
      assertEqual(2, Object.keys(field).length);
      assertEqual("analyzers", Object.keys(field)[0]);
      assertTrue(Array.isArray(field.analyzers));
      assertEqual(["custom", "text_en"], field.analyzers.sort());

      assertEqual("fields", Object.keys(field)[1]);
      assertTrue(typeof field.fields === 'object');
      if (isEnterprise) {
        assertTrue(_.isEqual(field.fields.value.nested.nested_1.nested.nested_2, {}));
      } else {
        assertTrue(_.isEqual(field.fields.value, {}));
      }

      assertTrue(Array.isArray(link.analyzerDefinitions));
      assertEqual(3, link.analyzerDefinitions.length);

      let a = link.analyzerDefinitions[0];
      assertEqual("custom", a.name);
      assertEqual("delimiter", a.type);
      assertEqual({"delimiter": " "}, a.properties);
      assertEqual(["frequency"], a.features);
      
      a = link.analyzerDefinitions[1];
      assertEqual("identity", a.name);
      assertEqual("identity", a.type);
      assertEqual({}, a.properties);
      assertEqual(["frequency", "norm"], a.features.sort());
      
      a = link.analyzerDefinitions[2];
      assertEqual("text_en", a.name);
      assertEqual("text", a.type);
      assertEqual({locale: "en", case: "lower", stopwords: [], accent: false, stemming: true}, a.properties);
      assertEqual(["frequency", "norm", "position"], a.features.sort());

      view = results.views[1];
      assertEqual("arangosearch", view.type);
      assertEqual("UnitTestsDumpViewEmpty", view.name);
      assertEqual(2, view.cleanupIntervalStep);
      assertEqual(1000, view.commitIntervalMsec);
      assertEqual(1000, view.consolidationIntervalMsec);
      assertEqual("tier", view.consolidationPolicy.type);
      assertEqual([], view.primarySort);
        
      view = results.views[2];
      assertEqual("search-alias", view.type);
      assertEqual("UnitTestsDumpViewSearchAlias", view.name);
      assertEqual(1, view.indexes.length);
      assertEqual("UnitTestsDumpSearchAliasCollection", view.indexes[0].collection);
        
      view = results.views[3];
      assertEqual("search-alias", view.type);
      assertEqual("UnitTestsDumpViewSearchAlias2", view.name);
      assertEqual("UnitTestsDumpSearchAliasCollection2", view.indexes[0].collection);
    },

    testEmptyDatabase : function () {
      db._useDatabase("UnitTestsDumpDst");
      
      let results = arango.GET("/_api/replication/clusterInventory");
      assertTrue(results.hasOwnProperty("collections"));
      assertTrue(Array.isArray(results.collections));
      assertNotEqual(0, results.collections.length);

      results.collections.forEach(function(collection) {
        validateCollectionAttributes(collection);
        assertEqual('_', collection.parameters.name[0]);
        assertTrue(collection.parameters.isSystem);
      });
      
      assertTrue(results.hasOwnProperty("views"));
      assertTrue(Array.isArray(results.views));
      assertEqual(0, results.views.length);
    }
    
  };
}

jsunity.run(clusterInventorySuite);
return jsunity.done();
