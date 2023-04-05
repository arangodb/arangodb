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
    'database.extended-names': "true",
  };
}
const jsunity = require('jsunity');
const db = require('internal').db;
const errors = require('@arangodb').errors;

const traditionalName = "UnitTestsIndex";
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

    testCreateAndDropIndexTraditionalName: function() {
      let c = db._create(traditionalName);
      let idx = c.ensureIndex({ type: "persistent", fields: ["value"], name: traditionalName });
      assertEqual(traditionalName, idx.name);

      idx = c.index(traditionalName);
      assertEqual(traditionalName, idx.name);

      let indexes = c.indexes().map((idx) => idx.name);
      assertNotEqual(-1, indexes.indexOf(traditionalName));

      c.dropIndex(traditionalName);
      
      indexes = c.indexes().map((idx) => idx.name);
      assertEqual(-1, indexes.indexOf(traditionalName));
    },
    
    testCreateAndDropIndexExtendedName: function() {
      let c = db._create(traditionalName);
      let idx = c.ensureIndex({ type: "persistent", fields: ["value"], name: extendedName });
      assertEqual(extendedName, idx.name);

      idx = c.index(extendedName);
      assertEqual(extendedName, idx.name);

      let indexes = c.indexes().map((idx) => idx.name);
      assertNotEqual(-1, indexes.indexOf(extendedName));

      c.dropIndex(extendedName);
      
      indexes = c.indexes().map((idx) => idx.name);
      assertEqual(1, indexes.length);
      assertEqual(-1, indexes.indexOf(extendedName));
    },
    
    testCreateAndDropIndexExtendedNameViaDatabaseObject: function() {
      let c = db._create(extendedName);
      let idx1 = c.ensureIndex({ type: "persistent", fields: ["value"], name: extendedName });
      assertTrue(idx1.id.startsWith(extendedName + "/"));
      assertEqual(extendedName, idx1.name);
      
      let idx2 = db._index(c.name() + "/" + extendedName);
      assertTrue(idx2.id.startsWith(extendedName + "/"));
      assertEqual(extendedName, idx2.name);
      assertEqual(idx1.id, idx2.id);

      db._dropIndex(c.name() + "/" + extendedName);
      
      let indexes = c.indexes().map((idx) => idx.name);
      assertEqual(1, indexes.length);
      assertEqual(-1, indexes.indexOf(extendedName));
    },
    
    testCreateDuplicateIndexExtendedName: function() {
      let c = db._create(traditionalName);
      let idx1 = c.ensureIndex({ type: "persistent", fields: ["value"], name: extendedName });
      assertEqual(extendedName, idx1.name);
      assertTrue(idx1.isNewlyCreated);
      assertTrue(idx1.hasOwnProperty("id"));
      assertEqual(2, c.indexes().length);

      idx1 = c.index(extendedName);
      assertEqual(extendedName, idx1.name);

      let indexes = c.indexes().map((idx) => idx.name);
      assertNotEqual(-1, indexes.indexOf(extendedName));

      let idx2 = c.ensureIndex({ type: "persistent", fields: ["value"], name: extendedName });
      assertEqual(extendedName, idx2.name);
      assertFalse(idx2.isNewlyCreated);
      assertTrue(idx2.hasOwnProperty("id"));
      assertEqual(idx1.id, idx2.id);
      assertEqual(2, c.indexes().length);

      c.dropIndex(extendedName);
      
      indexes = c.indexes().map((idx) => idx.name);
      assertEqual(-1, indexes.indexOf(extendedName));
      assertEqual(1, c.indexes().length);
    },
    
    testCreateMultipleIndexExtendedName: function() {
      let c = db._create(traditionalName);
      let idx1 = c.ensureIndex({ type: "persistent", fields: ["value1"], name: extendedName + "1" });
      assertEqual(extendedName + "1", idx1.name);
      assertTrue(idx1.isNewlyCreated);
      assertTrue(idx1.hasOwnProperty("id"));
      assertEqual(2, c.indexes().length);
      
      let idx2 = c.ensureIndex({ type: "persistent", fields: ["value2"], name: extendedName + "2" });
      assertEqual(extendedName + "2", idx2.name);
      assertTrue(idx2.isNewlyCreated);
      assertTrue(idx2.hasOwnProperty("id"));
      assertEqual(3, c.indexes().length);

      idx1 = c.index(extendedName + "1");
      assertEqual(extendedName + "1", idx1.name);
      
      idx2 = c.index(extendedName + "2");
      assertEqual(extendedName + "2", idx2.name);

      let indexes = c.indexes().map((idx) => idx.name);
      assertNotEqual(-1, indexes.indexOf(extendedName + "1"));
      assertNotEqual(-1, indexes.indexOf(extendedName + "2"));

      c.dropIndex(extendedName + "1");
      indexes = c.indexes().map((idx) => idx.name);
      assertEqual(-1, indexes.indexOf(extendedName + "1"));
      assertEqual(2, c.indexes().length);
      
      c.dropIndex(extendedName + "2");
      indexes = c.indexes().map((idx) => idx.name);
      assertEqual(-1, indexes.indexOf(extendedName + "2"));
      assertEqual(1, c.indexes().length);
    },
    
    testCreateWithInvalidUtf8Names: function() {
      let c = db._create(traditionalName);
      invalidNames.forEach((name) => {
        try {
          c.ensureIndex({ type: "persistent", fields: ["value"], name: name });
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
    
    testDropNonExistingExtendedName: function() {
      let c = db._create(traditionalName);
      try {
        c.dropIndex(extendedName);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
      }
    },
    
    testDropWithInvalidUtf8Names: function() {
      let c = db._create(traditionalName);
      invalidNames.forEach((name) => {
        try {
          c.dropIndex(name);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
    
    testDropWithInvalidUtf8NamesViaDBObject: function() {
      let c = db._create(traditionalName);
      invalidNames.forEach((name) => {
        try {
          db._dropIndex(c.name() + "/" + name);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
