/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, assertFalse, assertNull, assertNotNull, fail */

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
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
