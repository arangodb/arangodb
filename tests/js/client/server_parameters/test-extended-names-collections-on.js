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
    'database.extended-names': "true",
  };
}
const jsunity = require('jsunity');
const internal = require('internal');
const db = internal.db;
const errors = internal.errors;
const ArangoCollection = require("@arangodb").ArangoCollection;
const isCluster = internal.isCluster;
const isEnterprise = internal.isEnterprise;

const traditionalName = "UnitTestsCollection";
const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";
      
const invalidNames = [
  "\u212b", // Angstrom, not normalized;
  "\u0041\u030a", // Angstrom, NFD-normalized;
  "\u0073\u0323\u0307", // s with two combining marks, NFD-normalized;
  "\u006e\u0303\u00f1", // non-normalized sequence;
];

const runGraphTest = (graph) => {
  const gn = "le-graph";

  const edgeDef = graph._edgeDefinitions(
    graph._relation(traditionalName, [extendedName], [extendedName]),
  );

  let g = graph._create(gn, edgeDef);
  try {
    let doc = db._graphs.document(gn);
    assertEqual(1, doc.edgeDefinitions.length);
    assertEqual(traditionalName, doc.edgeDefinitions[0].collection);
    assertEqual(1, doc.edgeDefinitions[0].from.length);
    assertEqual([extendedName], doc.edgeDefinitions[0].from);
    assertEqual(1, doc.edgeDefinitions[0].to.length);
    assertEqual([extendedName], doc.edgeDefinitions[0].to.sort());
    assertEqual([], doc.orphanCollections);

    let v1 = g[extendedName].save({_key: "1"});
    assertEqual(extendedName + "/1", v1._id);
    let v2 = g[extendedName].save({_key: "2"});
    assertEqual(extendedName + "/2", v2._id);

    assertEqual(2, g[extendedName].count());
  } finally {
    graph._drop(gn, true);
  }
};

const runInvalidGraphTest = (graph) => {
  const gn = "le-graph";

  const cases = [
    graph._edgeDefinitions(graph._relation(invalidNames[0], [extendedName], [extendedName])),
    graph._edgeDefinitions(graph._relation(invalidNames[1], [extendedName], [extendedName])),
    graph._edgeDefinitions(graph._relation(traditionalName, [invalidNames[0]], [extendedName])),
    graph._edgeDefinitions(graph._relation(traditionalName, [invalidNames[1]], [extendedName])),
    graph._edgeDefinitions(graph._relation(traditionalName, [extendedName], [invalidNames[0]])),
    graph._edgeDefinitions(graph._relation(traditionalName, [extendedName], [invalidNames[1]])),
    graph._edgeDefinitions(graph._relation(invalidNames[0], [invalidNames[1]], [invalidNames[2]])),
  ];
  
  cases.forEach((edgeDef) => {
    try {
      graph._create(gn, edgeDef);
      fail();
    } catch (err) {
      assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
    }
  });
    
  try {
    // try using an invalid collection name for an orpha collection
    graph._create(gn, graph._edgeDefinitions(graph._relation(traditionalName, [extendedName], [extendedName])), [invalidNames[0]]);
    fail();
  } catch (err) {
    assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
  }
};

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

    testCreateDistributeShardsLikeTraditionalProto: function() {
      if (!isCluster()) {
        return;
      }
      let proto = db._create(traditionalName, { numberOfShards: 4, replicationFactor: 2 });
      let c = db._create(extendedName, { distributeShardsLike: proto.name() });

      try {
        let properties = c.properties();
        assertEqual(4, properties.numberOfShards);
        assertEqual(2, properties.replicationFactor);
        assertEqual(proto.name(), properties.distributeShardsLike);
      } finally {
        // note: we must always drop the dependent collection first, not the prototype
        c.drop();
      }
    },
    
    testCreateDistributeShardsLikeExtendedProto: function() {
      if (!isCluster()) {
        return;
      }
      let proto = db._create(extendedName, { numberOfShards: 4, replicationFactor: 2 });
      let c = db._create(traditionalName, { distributeShardsLike: proto.name() });

      try {
        let properties = c.properties();
        assertEqual(4, properties.numberOfShards);
        assertEqual(2, properties.replicationFactor);
        assertEqual(proto.name(), properties.distributeShardsLike);
      } finally {
        // note: we must always drop the dependent collection first, not the prototype
        c.drop();
      }
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
    
    testCollectionSingleInsert: function() {
      let c = db._create(extendedName, { numberOfShards: 3, replicationFactor: 2 });
      let res = c.insert({ _key: "test1", value: 1 });
      assertEqual(extendedName + "/test1", res._id);

      let doc = c.document(extendedName + "/test1");
      assertEqual(extendedName + "/test1", doc._id);
      
      try {
        c.insert({ _key: "test1", value: 1 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
      
      res = c.insert({ _key: "test2", value: 1 });
      assertEqual(extendedName + "/test2", res._id);
      
      doc = c.document(extendedName + "/test2");
      assertEqual(extendedName + "/test2", doc._id);
    },
    
    testCollectionBatchInsert: function() {
      let c = db._create(extendedName, { numberOfShards: 3, replicationFactor: 2 });
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      let res = c.insert(docs);
      assertEqual(100, res.length);
      res.forEach((res, i) => {
        assertEqual(extendedName + "/test" + i, res._id);
      });

      // cause unique constraint violations
      res = c.insert(docs);
      assertEqual(100, res.length);
      res.forEach((res) => {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, res.errorNum);
      });
      
      // use overwriteMode update
      docs = docs.map((doc) => { doc.value2 = "test" + doc.value1; return doc; });
      res = c.insert(docs, { overwriteMode: "update" });
      assertEqual(100, res.length);
      res.forEach((res, i) => {
        assertEqual(extendedName + "/test" + i, res._id);
      });
      
      // use overwriteMode replace
      docs = docs.map((doc) => { doc.value3 = doc.value2; return doc; });
      res = c.insert(docs, { overwriteMode: "replace" });
      assertEqual(100, res.length);
      res.forEach((res, i) => {
        assertEqual(extendedName + "/test" + i, res._id);
      });
    },
    
    testCollectionSingleUpdate: function() {
      let c = db._create(extendedName, { numberOfShards: 3, replicationFactor: 2 });
      let res = c.insert({ _key: "test1", value1: 1 });
      assertEqual(extendedName + "/test1", res._id);

      let doc = c.update("test1", { value2: 42 });
      assertEqual(extendedName + "/test1", doc._id);

      doc = c.document("test1");
      assertEqual(extendedName + "/test1", doc._id);
      assertEqual(1, doc.value1);
      assertEqual(42, doc.value2);
      
      doc = c.update(extendedName + "/test1", { value3: 23 });
      assertEqual(extendedName + "/test1", res._id);
      
      doc = c.document("test1");
      assertEqual(extendedName + "/test1", doc._id);
      assertEqual(1, doc.value1);
      assertEqual(42, doc.value2);
      assertEqual(23, doc.value3);
      
      doc = db._update(extendedName + "/test1", { value4: 666 });
      doc = c.document("test1");
      assertEqual(extendedName + "/test1", doc._id);
      assertEqual(1, doc.value1);
      assertEqual(42, doc.value2);
      assertEqual(23, doc.value3);
      assertEqual(666, doc.value4);
    },
    
    testCollectionBatchUpdate: function() {
      let c = db._create(extendedName, { numberOfShards: 3, replicationFactor: 2 });
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      docs = [];
      let keys = [];
      for (let i = 50; i < 102; ++i) {
        keys.push({ _key: "test" + i });
        docs.push({ value2: i });
      }
      let res = c.update(keys, docs);
      assertEqual(52, res.length);
      res.forEach((res, i) => {
        if (i < 50) {
          assertEqual(extendedName + "/test" + (50 + i), res._id);
        } else {
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, res.errorNum);
        }
      });
     
      res = db._query("FOR doc IN `" + extendedName + "` FILTER HAS(doc, 'value1') RETURN 1").toArray();
      assertEqual(100, res.length);
      res = db._query("FOR doc IN @@coll FILTER HAS(doc, 'value2') RETURN 1", { "@coll": extendedName }).toArray();
      assertEqual(50, res.length);
      
      // update by id
      docs = [];
      keys = [];
      for (let i = 0; i < 50; ++i) {
        keys.push(extendedName + "/test" + i);
        docs.push({ value2: i });
      }
      res = c.update(keys, docs);
      assertEqual(50, res.length);
      res.forEach((res, i) => {
        assertEqual(extendedName + "/test" + i, res._id);
      });

      res = db._query("FOR doc IN `" + extendedName + "` FILTER HAS(doc, 'value1') RETURN 1").toArray();
      assertEqual(100, res.length);
      res = db._query("FOR doc IN `" + extendedName + "` FILTER HAS(doc, 'value2') RETURN 1").toArray();
      assertEqual(100, res.length);
    },
    
    testCollectionSingleReplace: function() {
      let c = db._create(extendedName, { numberOfShards: 3, replicationFactor: 2 });
      let res = c.insert({ _key: "test1", value1: 1 });
      assertEqual(extendedName + "/test1", res._id);

      let doc = c.replace("test1", { value2: 42 });
      assertEqual(extendedName + "/test1", doc._id);

      doc = c.document("test1");
      assertEqual(extendedName + "/test1", doc._id);
      assertFalse(doc.hasOwnProperty("value1"));
      assertEqual(42, doc.value2);
      
      doc = c.replace(extendedName + "/test1", { value3: 23 });
      assertEqual(extendedName + "/test1", res._id);
      
      doc = c.document("test1");
      assertEqual(extendedName + "/test1", doc._id);
      assertFalse(doc.hasOwnProperty("value1"));
      assertFalse(doc.hasOwnProperty("value2"));
      assertEqual(23, doc.value3);
      
      doc = db._replace(extendedName + "/test1", { value4: 666 });
      doc = c.document("test1");
      assertEqual(extendedName + "/test1", doc._id);
      assertFalse(doc.hasOwnProperty("value1"));
      assertFalse(doc.hasOwnProperty("value2"));
      assertFalse(doc.hasOwnProperty("value3"));
      assertEqual(666, doc.value4);
    },

    testCollectionBatchReplace: function() {
      let c = db._create(extendedName, { numberOfShards: 3, replicationFactor: 2 });
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      docs = [];
      let keys = [];
      for (let i = 50; i < 102; ++i) {
        keys.push({ _key: "test" + i });
        docs.push({ value2: i });
      }
      let res = c.replace(keys, docs);
      assertEqual(52, res.length);
      res.forEach((res, i) => {
        if (i < 50) {
          assertEqual(extendedName + "/test" + (50 + i), res._id);
        } else {
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, res.errorNum);
        }
      });
     
      res = db._query("FOR doc IN `" + extendedName + "` FILTER HAS(doc, 'value1') RETURN 1").toArray();
      assertEqual(50, res.length);
      res = db._query("FOR doc IN `" + extendedName + "` FILTER HAS(doc, 'value2') RETURN 1").toArray();
      assertEqual(50, res.length);
      
      // update by id
      docs = [];
      keys = [];
      for (let i = 0; i < 50; ++i) {
        keys.push(extendedName + "/test" + i);
        docs.push({ value2: i });
      }
      res = c.replace(keys, docs);
      assertEqual(50, res.length);
      res.forEach((res, i) => {
        assertEqual(extendedName + "/test" + i, res._id);
      });
      
      res = db._query("FOR doc IN `" + extendedName + "` FILTER HAS(doc, 'value1') RETURN 1").toArray();
      assertEqual(0, res.length);
      res = db._query("FOR doc IN `" + extendedName + "` FILTER HAS(doc, 'value2') RETURN 1").toArray();
      assertEqual(100, res.length);
    },
    
    testCollectionSingleRemove: function() {
      let c = db._create(extendedName, { numberOfShards: 3, replicationFactor: 2 });
      let res = c.insert({ _key: "test1", value: 1 });
      assertEqual(extendedName + "/test1", res._id);

      let doc = c.remove("test1");
      assertEqual(extendedName + "/test1", doc._id);

      assertFalse(c.exists("test1"));
      
      try {
        c.remove("test1");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
      
      res = c.insert({ _key: "test1", value: 1 });
      assertEqual(extendedName + "/test1", res._id);
      
      doc = c.remove(extendedName + "/test1");
      assertEqual(extendedName + "/test1", doc._id);

      assertFalse(c.exists("test1"));
      
      res = c.insert({ _key: "test1", value: 1 });
      assertEqual(extendedName + "/test1", res._id);
      
      doc = db._remove(extendedName + "/test1");
      assertEqual(extendedName + "/test1", doc._id);

      assertFalse(c.exists("test1"));
    },
    
    testCollectionBatchRemove: function() {
      let c = db._create(extendedName, { numberOfShards: 3, replicationFactor: 2 });
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      // remove by key, cause a few errors
      docs = [];
      for (let i = 50; i < 102; ++i) {
        docs.push({ _key: "test" + i });
      }
      let res = c.remove(docs);
      assertEqual(52, res.length);
      res.forEach((res, i) => {
        if (i < 50) {
          assertEqual(extendedName + "/test" + (50 + i), res._id);
        } else {
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, res.errorNum);
        }
      });
     
      assertEqual(50, c.count());
      
      // remove by id
      docs = [];
      for (let i = 0; i < 50; ++i) {
        docs.push(extendedName + "/test" + i);
      }
      res = c.remove(docs);
      assertEqual(50, res.length);
      res.forEach((res, i) => {
        assertEqual(extendedName + "/test" + i, res._id);
      });
      
      assertEqual(0, c.count());
    },
    
    testCollectionCount: function() {
      let c = db._create(extendedName, { numberOfShards: 4 });
      assertEqual(0, c.count());

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      assertEqual(100, c.count());

      if (!isCluster()) {
        return;
      }

      let count = c.count(true);
      let keys = Object.keys(count);
      let total = 0;
      assertEqual(4, keys.length);
      keys.forEach((key) => {
        total += count[key];
      });

      assertEqual(100, total);
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

    testCollectionShards: function() {
      if (!isCluster()) {
        return;
      }
      let c = db._create(extendedName, { numberOfShards: 4, replicationFactor: 2 });
      let shards = c.shards();
      assertEqual(4, shards.length);
      
      shards = c.shards(true);
      let keys = Object.keys(shards);
      assertEqual(4, keys.length);
      keys.forEach((key) => {
        assertEqual(2, shards[key].length);
      });
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
    
    testExtendedNameInBindParameters: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let res = db._query("FOR doc IN @@collection SORT doc.value1 RETURN doc", { "@collection": extendedName }).toArray();
      assertEqual(100, res.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual("test" + i, res[i]._key);
        assertEqual(extendedName + "/test" + i, res[i]._id);
        assertEqual(i, res[i].value1);
      }
    },
    
    testExtendedNamesQuoting: function() {
      const names = [
        "Десятую",
        "a b c",
        "💩🍺🌧t⛈c🌩_⚡🔥💥🌨",
      ];
      
      names.forEach((name) => {
        let c = db._create(name);
        try {
          try {
            // doesn't work without quoting
            db._query("FOR doc IN " + name + " RETURN doc");
            fail();
          } catch (err) {
            assertEqual(errors.ERROR_QUERY_PARSE.code, err.errorNum);
          }
          
          // works without quoting
          let res = db._query("FOR doc IN `" + name + "` RETURN doc").toArray();
          assertEqual(0, res.length);
        } finally {
          db._drop(name);
        }
      });
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
    
    testQueryIdViaPrimaryIndex: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let res = db._query("FOR doc IN `" + extendedName + "` SORT doc._id RETURN doc._id").toArray();
      // sort by the numeric part of _id
      res.sort((l, r) => {
        return parseInt(l.replace(/^.*\/test/, '')) - parseInt(r.replace(/^.*\/test/, ''));
      });

      assertEqual(100, res.length);
      for (let i = 0; i < 100; ++i) {
        assertEqual(extendedName + "/test" + i, res[i]);
      }
    },
    
    testQueryIdViaPrimaryIndexWithEqFilter: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let res = db._query("FOR doc IN `" + extendedName + "` FILTER doc._id == '" + extendedName + "/test42' SORT doc._id RETURN doc._id").toArray();
      assertEqual(1, res.length);
      assertEqual(extendedName + "/test42", res[0]);
      
      res = db._query("FOR doc IN `" + extendedName + "` FILTER doc._id == '" + extendedName + "/test42' SORT doc._id RETURN doc").toArray();
      assertEqual(1, res.length);
      assertEqual(extendedName + "/test42", res[0]._id);
    },
    
    testQueryIdViaPrimaryIndexWithGtFilter: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, value1: i });
      }
      c.insert(docs);

      let res = db._query("FOR doc IN `" + extendedName + "` FILTER doc._id >= '" + extendedName + "/test90' SORT doc._id RETURN doc._id").toArray();
      // sort by the numeric part of _id
      res.sort((l, r) => {
        return parseInt(l.replace(/^.*\/test/, '')) - parseInt(r.replace(/^.*\/test/, ''));
      });

      assertEqual(10, res.length);
      for (let i = 0; i < 10; ++i) {
        assertEqual(extendedName + "/test" + (i + 90), res[i]);
      }
      
      res = db._query("FOR doc IN `" + extendedName + "` FILTER doc._id >= '" + extendedName + "/test90' SORT doc._id RETURN doc").toArray();
      // sort by the numeric part of _id
      res.sort((l, r) => {
        return parseInt(l._id.replace(/^.*\/test/, '')) - parseInt(r._id.replace(/^.*\/test/, ''));
      });

      assertEqual(10, res.length);
      for (let i = 0; i < 10; ++i) {
        assertEqual(extendedName + "/test" + (i + 90), res[i]._id);
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
    
    testTraversal: function() {
      // vertices
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);

      // edges
      c = db._createEdgeCollection(traditionalName);
      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, _from: extendedName + "/test" + i, _to: extendedName + "/test" + (i % 10), value: i });
      }
      c.insert(docs);

      let res = db._query("WITH `" + extendedName + "` FOR v, e, p IN 1..3 OUTBOUND '" + extendedName + "/test0' " + traditionalName + " RETURN [v, e]").toArray();
      assertEqual(1, res.length);
      assertEqual(extendedName + "/test0", res[0][0]._id);
      assertEqual("test0", res[0][0]._key);
      
      assertEqual(extendedName + "/test0", res[0][1]._from);
      assertEqual(extendedName + "/test0", res[0][1]._to);
    },
    
    testEdgeIndex: function() {
      let c = db._createEdgeCollection(extendedName);
      
      let indexes = c.indexes();
      assertEqual(2, indexes.length);

      assertEqual("primary", indexes[0].type);
      assertEqual("edge", indexes[1].type);
      assertEqual(extendedName + "/2", indexes[1].id);
    },
  
    testEdgeQueriesUsingExtendedVertices: function() {
      // vertices
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);

      // edges
      c = db._createEdgeCollection(traditionalName);
      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, _from: extendedName + "/test" + i, _to: extendedName + "/test" + (i % 10), value: i });
      }
      c.insert(docs);

      let res = db._query("FOR doc IN `" + traditionalName + "` FILTER doc._from == '" + extendedName + "/test0' RETURN doc").toArray();
      assertEqual(1, res.length);
      assertEqual(extendedName + "/test0", res[0]._from);
      assertEqual(extendedName + "/test0", res[0]._to);
      
      // query again until we have a cache hit in the in-memory cache for edges
      let tries = 0;
      while (++tries < 60) {
        let res = db._query("FOR doc IN `" + traditionalName + "` FILTER doc._from == '" + extendedName + "/test0' RETURN doc");
        let stats = res.getExtra().stats;
        res = res.toArray();
        assertEqual(1, res.length);
        assertEqual(extendedName + "/test0", res[0]._from);
        assertEqual(extendedName + "/test0", res[0]._to);
        if (stats.cacheHits > 0) {
          break;
        }
        internal.sleep(0.5);
      }
      assertTrue(tries < 60);

      res = db._query("FOR doc IN `" + traditionalName + "` FILTER doc._to == @to RETURN doc", { to: extendedName + "/test0" }).toArray();
      assertEqual(10, res.length);
      // sort by the numeric part of _from
      res.sort((l, r) => {
        return parseInt(l._from.replace(/^.*\/test/, '')) - parseInt(r._from.replace(/^.*\/test/, ''));
      });

      for (let i = 0; i < 10; ++i) {
        assertEqual(extendedName + "/test" + (i * 10), res[i]._from);
        assertEqual(extendedName + "/test0", res[i]._to);
      }
      
      // query again until we have a cache hit in the in-memory cache for edges
      tries = 0;
      while (++tries < 60) {
        let res = db._query("FOR doc IN `" + traditionalName + "` FILTER doc._to == '" + extendedName + "/test0' RETURN doc");
        let stats = res.getExtra().stats;
        res = res.toArray();
        assertEqual(10, res.length);
        // sort by the numeric part of _from
        res.sort((l, r) => {
          return parseInt(l._from.replace(/^.*\/test/, '')) - parseInt(r._from.replace(/^.*\/test/, ''));
        });
        for (let i = 0; i < 10; ++i) {
          assertEqual(extendedName + "/test" + (i * 10), res[i]._from);
          assertEqual(extendedName + "/test0", res[i]._to);
        }
        if (stats.cacheHits > 0) {
          break;
        }
        internal.sleep(0.5);
      }
      assertTrue(tries < 60);
    },
    
    testEdgesUsedInUDF: function() {
      // vertices
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i });
      }
      c.insert(docs);

      // edges
      c = db._createEdgeCollection(traditionalName);
      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i, _from: extendedName + "/test" + i, _to: extendedName + "/test" + (i % 10), value: i });
      }
      c.insert(docs);
      
      const udf = require("@arangodb/aql/functions");

      // register UDF to modify edge data
      udf.register("SOME::FUNC", function(doc) {
        doc.value2 = doc._from + "-" + doc._to;
        doc._from = doc._from + "hello";
        return doc;
      });

      try {
        let res = db._query("FOR doc IN `" + traditionalName + "` SORT doc.value RETURN SOME::FUNC(doc)").toArray();
        
        assertEqual(100, res.length);
        for (let i = 0; i < 100; ++i) {
          let doc = res[i];
          assertEqual("test" + i, doc._key);
          assertEqual(extendedName + "/test" + i + "hello", doc._from);
          assertEqual(extendedName + "/test" + (i % 10), doc._to);
          assertEqual(i, doc.value);
          assertEqual(extendedName + "/test" + i + "-" + extendedName + "/test" + (i % 10), doc.value2);
        }
      } finally {
        udf.unregister("SOME::FUNC");
      }
    },
    
    testGraphExtendedCollectionNames: function() {
      const graph = require("@arangodb/general-graph");
      runGraphTest(graph);
    },
    
    testSmartGraphExtendedCollectionNames: function() {
      if (!isEnterprise()) {
        return;
      }

      const graph = require("@arangodb/smart-graph");
      runGraphTest(graph);
    },
    
    testGraphWithInvalidCollectionNames: function() {
      const graph = require("@arangodb/general-graph");
      runInvalidGraphTest(graph);
    },
    
    testSmartGraphInvalidCollectionNames: function() {
      if (!isEnterprise()) {
        return;
      }

      const graph = require("@arangodb/smart-graph");
      runInvalidGraphTest(graph);
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
