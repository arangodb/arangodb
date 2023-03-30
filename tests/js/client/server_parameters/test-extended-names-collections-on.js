/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull, assertNotNull, assertUndefined, fail */

////////////////////////////////////////////////////////////////////////////////
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.extended-names-indexes': "true",
    'database.extended-names-collections': "true",
  };
}
const jsunity = require('jsunity');
const db = require('internal').db;
const errors = require('internal').errors;
const ArangoCollection = require("@arangodb").ArangoCollection;
const isCluster = require('internal').isCluster;

const traditionalName = "UnitTestsCollection";
const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";
      
const invalidNames = [
  "\u212b", // Angstrom, not normalized;
  "\u0041\u030a", // Angstrom, NFD-normalized;
  "\u0073\u0323\u0307", // s with two combining marks, NFD-normalized;
  "\u006e\u0303\u00f1", // non-normalized sequence;
];

function testSuite() {
  return {
    tearDown: function() {
      db._drop(traditionalName);
      db._drop(extendedName);
    },
    
    testCreateTraditionalRenameToExtended: function() {
      if (isCluster()) {
        // renaming not supported in cluster
        return;
      }

      let c = db._create(traditionalName);
      assertTrue(c instanceof ArangoCollection);
    
      c.rename(extendedName);
      assertNull(db._collection(traditionalName));
      assertNotNull(db._collection(extendedName));
      c.rename(traditionalName);
      assertNotNull(db._collection(traditionalName));
      assertNull(db._collection(extendedName));
    },

    testCreateExtendedName: function() {
      let c = db._create(extendedName);
      assertTrue(c instanceof ArangoCollection);
      
      c = db._collection(extendedName);
      assertNotNull(c);

      assertNotEqual(-1, db._collections().map((c) => c.name()).indexOf(extendedName));

      let properties = c.properties();
      assertTrue(properties.hasOwnProperty("waitForSync"));
      assertFalse(properties.waitForSync);

      // change properties
      c.properties({ waitForSync: true });
      
      // fetch em back
      properties = c.properties();
      assertTrue(properties.hasOwnProperty("waitForSync"));
      assertTrue(properties.waitForSync);

      c.drop();
      c = db._collection(extendedName);
      assertNull(c);
    },
    
    testCreateExtendedNameCluster: function() {
      if (!isCluster()) {
        return;
      }

      let c = db._create(extendedName, { numberOfShards: 9, replicationFactor: 2 });
      assertTrue(c instanceof ArangoCollection);
      
      c = db._collection(extendedName);
      assertNotNull(c);
      
      let properties = c.properties();
      assertEqual(9, properties.numberOfShards);
      assertEqual(2, properties.replicationFactor);
      
      // change properties
      c.properties({ replicationFactor: 1 });
      
      // fetch em back
      properties = c.properties();
      assertEqual(9, properties.numberOfShards);
      assertEqual(1, properties.replicationFactor);
    },
    
    testCreateEdgeExtendedName: function() {
      let c = db._createEdgeCollection(extendedName);
      assertTrue(c instanceof ArangoCollection);
      
      c = db._collection(extendedName);
      assertNotNull(c);

      assertNotEqual(-1, db._collections().map((c) => c.name()).indexOf(extendedName));
      
      let properties = c.properties();
      assertTrue(properties.hasOwnProperty("waitForSync"));
      assertFalse(properties.waitForSync);
      
      // change properties
      c.properties({ waitForSync: true });
      
      // fetch em back
      properties = c.properties();
      assertTrue(properties.hasOwnProperty("waitForSync"));
      assertTrue(properties.waitForSync);

      c.drop();
      c = db._collection(extendedName);
      assertNull(c);
    },
    
    testCreateInvalidUtf8Names: function() {
      invalidNames.forEach((name) => {
        try {
          db._create(name);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
    
    testRenameToInvalidUtf8: function() {
      if (isCluster()) {
        // renaming not supported in cluster
        return;
      }

      let c = db._create(extendedName);
      invalidNames.forEach((name) => {
        try {
          c.rename(name);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
    
    testCreateIndexExtendedNames: function() {
      let c = db._create(extendedName);
      assertTrue(c instanceof ArangoCollection);
    
      let idx1 = c.ensureIndex({ type: "persistent", fields: ["value1"], name: extendedName + "1" });
      let idx2 = c.ensureIndex({ type: "geo", fields: ["lat", "lon"], name: extendedName + "2" });
      let idx3 = c.ensureIndex({ type: "inverted", fields: ["value2"], name: extendedName + "3" });

      let indexes = c.indexes();
      assertEqual(4, indexes.length);

      assertEqual("primary", indexes[0].type);
      assertEqual(extendedName + "/0", indexes[0].id);
      assertTrue(indexes[0].id.startsWith(extendedName + "/"));
      
      assertEqual("persistent", indexes[1].type);
      assertEqual(idx1.id, indexes[1].id);
      assertTrue(indexes[1].id.startsWith(extendedName + "/"));
      assertEqual(extendedName + "1", indexes[1].name);
      
      assertEqual("geo", indexes[2].type);
      assertEqual(idx2.id, indexes[2].id);
      assertTrue(indexes[2].id.startsWith(extendedName + "/"));
      assertEqual(extendedName + "2", indexes[2].name);
      
      assertEqual("inverted", indexes[3].type);
      assertEqual(idx3.id, indexes[3].id);
      assertTrue(indexes[3].id.startsWith(extendedName + "/"));
      assertEqual(extendedName + "3", indexes[3].name);

      c.dropIndex(extendedName + "1");
      c.dropIndex(extendedName + "2");
      c.dropIndex(extendedName + "3");
      
      indexes = c.indexes();
      assertEqual(1, indexes.length);
    },
    
    testCollectionInvalidUtf8Names: function() {
      // db._collection() returns null for non-existing collections...
      invalidNames.forEach((name) => {
        // drop view
        assertNull(db._collection(name));
      });
    },
    
    testDropCollectionInvalidUtf8Names: function() {
      // db._drop() returns undefined for non-existing collections...
      invalidNames.forEach((name) => {
        assertUndefined(db._drop(name));
      });
    },
    
    testCollectionAny: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let doc = c.any();
      assertNotNull(doc);
      assertTrue(doc._id.startsWith(extendedName + "/"));
      
      c.truncate();

      doc = c.any();
      assertNull(doc);
    },
    
    testCollectionAll: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      docs = c.all().toArray();
      docs.sort((l, r) => {
        return l.value1 - r.value1;
      });

      assertEqual(100, docs.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual(i, docs[i].value1);
        assertEqual(extendedName + "/test" + i, docs[i]._id);
        assertEqual("test" + i, docs[i]._key);
      }
    },
    
    testCollectionFirstExample: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let doc = c.firstExample({ value1: 36 });
      assertEqual(extendedName + "/test36", doc._id);
      assertEqual("test36", doc._key);
      assertEqual(36, doc.value1);
    },
    
    testCollectionByExample: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      docs = c.byExample({ value1: 36 }).toArray();
      assertEqual(1, docs.length);
      let doc = docs[0];
      assertEqual(extendedName + "/test36", doc._id);
      assertEqual("test36", doc._key);
      assertEqual(36, doc.value1);
    },
    
    testCollectionRemoveByExample: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      assertEqual(1, c.removeByExample({ value1: 36 }));

      assertNull(c.firstExample({ value1: 36 }));
      assertEqual([], c.byExample({ value1: 36 }).toArray());
      assertFalse(c.exists("test36"));

      assertEqual(0, c.removeByExample({ value1: 36 }));
    },
    
    testCollectionUpdateByExample: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      assertEqual(1, c.updateByExample({ value1: 36 }, { value1: 37, value2: "foo" }));

      assertNull(c.firstExample({ value1: 36 }));
      assertEqual(1, c.byExample({ value1: 37, value2: "foo" }).toArray().length);
      assertEqual(2, c.byExample({ value1: 37 }).toArray().length);
      
      let doc = c.firstExample({ value1: 37, value2: "foo" });
      assertNotNull(doc);
    },
    
    testCollectionReplaceByExample: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      assertEqual(1, c.replaceByExample({ value1: 36 }, { value2: "foo" }));

      assertNull(c.firstExample({ value1: 36 }));
      assertEqual(1, c.byExample({ value2: "foo" }).toArray().length);
      
      assertEqual(0, c.byExample({ value1: 36, value2: "foo" }).toArray().length);
      assertEqual(1, c.byExample({ value2: "foo" }).toArray().length);
      let doc = c.firstExample({ value2: "foo" });
      assertNotNull(doc);
    },
    
    testCollectionExists: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      for (let i = 50; i < 102; ++i) {
        let doc = c.exists("test" + i);
        if (i < 100) {
          assertEqual(extendedName + "/test" + i, doc._id);
          assertEqual("test" + i, doc._key);
        } else {
          assertFalse(doc);
        }
        
        doc = c.exists(extendedName + "/test" + i);
        if (i < 100) {
          assertEqual(extendedName + "/test" + i, doc._id);
          assertEqual("test" + i, doc._key);
        } else {
          assertFalse(doc);
        }
        
        doc = db._exists(extendedName + "/test" + i);
        // db._exists() only returns true/false, for whatever reason
        if (i < 100) {
          assertTrue(doc);
        } else {
          assertFalse(doc);
        }
      }
    },
    
    testCollectionDocument: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      for (let i = 0; i < 100; ++i) {
        let doc = c.document("test" + i);
        assertEqual(extendedName + "/test" + i, doc._id);
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value1);
        
        doc = db._document(extendedName + "/test" + i);
        assertEqual(extendedName + "/test" + i, doc._id);
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value1);
      }
    },
    
    testCollectionCount: function() {
      let c = db._create(extendedName);
      assertEqual(0, c.count());

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      assertEqual(100, c.count());
    },
    
    testCollectionFigures: function() {
      let c = db._create(extendedName);
      assertEqual(0, c.count());

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let figures = c.figures();
      assertTrue(figures.hasOwnProperty("indexes"));
      assertEqual(1, figures.indexes.count);
      
      figures = c.figures(true);
      assertTrue(figures.hasOwnProperty("indexes"));
      assertEqual(1, figures.indexes.count);
      assertTrue(figures.hasOwnProperty("engine"));
      assertEqual(1, figures.engine.indexes.length);
    },
    
    testCollectionTruncate: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      assertEqual(100, c.count());
      c.truncate();
      assertEqual(0, c.count());
    },
    
    testQueryIdValues: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let res = db._query("FOR doc IN `" + extendedName + "` SORT doc.value1 RETURN doc").toArray();
      assertEqual(100, res.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual("test" + i, res[i]._key);
        assertEqual(extendedName + "/test" + i, res[i]._id);
        assertEqual(i, res[i].value1);
      }
    },
    
    testParseIdentifierAqlFunction: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let res = db._query("FOR doc IN `" + extendedName + "` SORT doc.value1 RETURN PARSE_IDENTIFIER(doc)").toArray();
      assertEqual(100, res.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual(extendedName, res[i].collection);
        assertEqual("test" + i, res[i].key);
      }
      
      res = db._query("FOR doc IN `" + extendedName + "` SORT doc.value1 RETURN PARSE_IDENTIFIER(doc._id)").toArray();
      assertEqual(100, res.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual(extendedName, res[i].collection);
        assertEqual("test" + i, res[i].key);
      }
    },
    
    testDocumentAqlFunction: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let res = db._query("FOR i IN 0..99 RETURN DOCUMENT('" + extendedName + "', CONCAT('test', i))").toArray();
      assertEqual(100, res.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual("test" + i, res[i]._key);
        assertEqual(extendedName + "/test" + i, res[i]._id);
        assertEqual(i, res[i].value1);
      }
      
      res = db._query("FOR i IN 0..99 RETURN DOCUMENT(CONCAT('" + extendedName + "/test', i))").toArray();
      assertEqual(100, res.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual("test" + i, res[i]._key);
        assertEqual(extendedName + "/test" + i, res[i]._id);
        assertEqual(i, res[i].value1);
      }
    },
    
    testEdgesOutQuery: function() {
      let c = db._createEdgeCollection(extendedName);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, _from: "v/test" + i, _to: "v/test" + (i % 10), value: i });
      }
      c.insert(docs);

      let edges = c.outEdges("v/test0");
      assertEqual(1, edges.length);
      assertEqual(extendedName + "/test0", edges[0]._id);
      assertEqual(0, edges[0].value);
      
      edges = c.inEdges("v/test0");
      edges.sort((l, r) => { return l.value - r.value; });
      assertEqual(10, edges.length);
      for (let i = 0; i < 10; ++i) { 
        assertEqual(extendedName + "/test" + (i * 10), edges[i]._id);
        assertEqual(i * 10, edges[i].value);
      }
      
      edges = c.edges("v/test0");
      edges.sort((l, r) => { return l.value - r.value; });
      assertEqual(10, edges.length);
      for (let i = 0; i < 10; ++i) { 
        assertEqual(extendedName + "/test" + (i * 10), edges[i]._id);
        assertEqual(i * 10, edges[i].value);
      }
    },
  
  };
}

jsunity.run(testSuite);
return jsunity.done();
