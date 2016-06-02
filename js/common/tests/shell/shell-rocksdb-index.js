/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the rocksdb index
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: creation
////////////////////////////////////////////////////////////////////////////////

function RocksDBIndexSuite() {
  'use strict';
  var cn = "UnitTestsCollectionRocksDB";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // try...catch is necessary as some tests delete the collection itself!
      try {
        collection.unload();
        collection.drop();
      }
      catch (err) {
      }

      collection = null;
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation
////////////////////////////////////////////////////////////////////////////////

    testCreation : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: ["a"] });
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureIndex({ type: "persistent", fields: ["a"] });

      assertEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);

      idx = collection.ensureIndex({ type: "persistent", fields: ["a"], sparse: true });

      assertNotEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(true, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);
      id = idx.id;

      idx = collection.ensureIndex({ type: "persistent", fields: ["a"], sparse: true });

      assertEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(true, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: permuted attributes
////////////////////////////////////////////////////////////////////////////////

    testCreationPermuted : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: ["a", "b"] });
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a","b"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureIndex({ type: "persistent", fields: ["a", "b"] });

      assertEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a","b"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);
      
      idx = collection.ensureIndex({ type: "persistent", fields: ["b", "a"] });

      assertNotEqual(id, idx.id);
      id = idx.id;
      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["b","a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);
      
      idx = collection.ensureIndex({ type: "persistent", fields: ["b", "a"] });

      assertEqual(id, idx.id);
      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["b","a"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniqueDocuments : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: ["a", "b"] });

      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a","b"], idx.fields.sort());
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : 1, b : 1 });
      collection.save({ a : 1, b : 1 });

      collection.save({ a : 1 });
      collection.save({ a : 1 });
      collection.save({ a : null, b : 1 });
      collection.save({ a : null, b : 1 });
      collection.save({ c : 1 });
      collection.save({ c : 1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniqueDocumentsSparseIndex : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: ["a", "b"], sparse: true });

      assertEqual("persistent", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a","b"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : 1, b : 1 });
      collection.save({ a : 1, b : 1 });

      collection.save({ a : 1 });
      collection.save({ a : 1 });
      collection.save({ a : null, b : 1 });
      collection.save({ a : null, b : 1 });
      collection.save({ c : 1 });
      collection.save({ c : 1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: combination of indexes
////////////////////////////////////////////////////////////////////////////////

    testMultiIndexViolation1 : function () {
      collection.ensureIndex({ type: "persistent", fields: ["a"], unique: true });
      collection.ensureIndex({ type: "persistent", fields: ["b"] });

      collection.save({ a : "test1", b : 1});
      try {
        collection.save({ a : "test1", b : 1});
        fail();
      }
      catch (err1) {
      }

      var doc1 = collection.save({ a : "test2", b : 1});
      assertTrue(doc1._key !== "");

      try {
        collection.save({ a : "test1", b : 1});
        fail();
      }
      catch (err2) {
      }

      var doc2 = collection.save({ a : "test3", b : 1});
      assertTrue(doc2._key !== "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: combination of indexes
////////////////////////////////////////////////////////////////////////////////

    testMultiIndexViolationSparse1 : function () {
      collection.ensureIndex({ type: "persistent", fields: ["a"], unique: true, sparse: true });
      collection.ensureIndex({ type: "persistent", fields: ["b"], sparse: true });

      collection.save({ a : "test1", b : 1});
      try {
        collection.save({ a : "test1", b : 1});
        fail();
      }
      catch (err1) {
      }

      var doc1 = collection.save({ a : "test2", b : 1});
      assertTrue(doc1._key !== "");

      try {
        collection.save({ a : "test1", b : 1});
        fail();
      }
      catch (err2) {
      }

      var doc2 = collection.save({ a : "test3", b : 1});
      assertTrue(doc2._key !== "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: combination of indexes
////////////////////////////////////////////////////////////////////////////////

    testMultiIndexViolation2 : function () {
      collection.ensureIndex({ type: "persistent", fields: ["a"], unique: true });
      collection.ensureIndex({ type: "persistent", fields: ["b"] });

      collection.save({ a : "test1", b : 1});
      try {
        collection.save({ a : "test1", b : 1});
        fail();
      }
      catch (err1) {
      }

      var doc1 = collection.save({ a : "test2", b : 1});
      assertTrue(doc1._key !== "");

      try {
        collection.save({ a : "test1", b : 1});
        fail();
      }
      catch (err2) {
      }

      var doc2 = collection.save({ a : "test3", b : 1});
      assertTrue(doc2._key !== "");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: combination of indexes
////////////////////////////////////////////////////////////////////////////////

    testMultiIndexViolationSparse2 : function () {
      collection.ensureIndex({ type: "persistent", fields: ["a"], unique: true, sparse: true });
      collection.ensureIndex({ type: "persistent", fields: ["b"], sparse: true });

      collection.save({ a : "test1", b : 1});
      try {
        collection.save({ a : "test1", b : 1});
        fail();
      }
      catch (err1) {
      }

      var doc1 = collection.save({ a : "test2", b : 1});
      assertTrue(doc1._key !== "");

      try {
        collection.save({ a : "test1", b : 1});
        fail();
      }
      catch (err2) {
      }

      var doc2 = collection.save({ a : "test3", b : 1});
      assertTrue(doc2._key !== "");
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RocksDBIndexSuite);

return jsunity.done();

