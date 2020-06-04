/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue, assertFalse, assertNull, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the hash index
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
var ERRORS = require("internal").errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function HashIndexSuite() {
  'use strict';
  var cn = "UnitTestsCollectionHash";
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
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessTopAttribute : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["a"] });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : null });
      collection.save({ a : 0 });
      collection.save({ a : 1 });
      collection.save({ a : 2 });
      try {
        collection.save({ a : 2 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessSubAttribute : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["a.b"] });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a.b"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : { b : null } });
      collection.save({ a : { b : 0 } });
      collection.save({ a : { b : 1 } });
      collection.save({ a : { b : 2 } });
      try {
        collection.save({ a : { b : 2 } });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessSubAttributeKey : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["a._key"] });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a._key"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : { _key : null } });
      collection.save({ a : { _key : 0 } });
      collection.save({ a : { _key : 1 } });
      collection.save({ a : { _key : 2 } });
      try {
        collection.save({ a : { _key : 2 } });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessArrayAttribute : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["a[*].b"] });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a[*].b"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : [ { b : null } ] });
      collection.save({ a : [ { b : 0 } ] });
      collection.save({ a : [ { b : 1 } ] });
      collection.save({ a : [ { b : 2 } ] });
      try {
        collection.save({ a : [ { b : 2 } ] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessArrayAttributeKey : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["a[*]._key"] });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a[*]._key"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : [ { _key : null } ] });
      collection.save({ a : [ { _key : 0 } ] });
      collection.save({ a : [ { _key : 1 } ] });
      collection.save({ a : [ { _key : 2 } ] });
      try {
        collection.save({ a : [ { _key : 2 } ] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(4, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testDeduplicationDefault : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["a[*].b"] });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a[*].b"], idx.fields);
      assertEqual(true, idx.deduplicate);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : [ { b : 1 }, { b : 1 } ] });
      collection.save({ a : [ { b : 2 }, { b : 2 } ] });
      collection.save({ a : [ { b : 3 }, { b : 4 } ] });
      try {
        collection.save({ a : [ { b : 2 } ] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(3, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testDeduplicationOn : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["a[*].b"], deduplicate: true });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a[*].b"], idx.fields);
      assertEqual(true, idx.deduplicate);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : [ { b : 1 }, { b : 1 } ] });
      collection.save({ a : [ { b : 2 }, { b : 2 } ] });
      collection.save({ a : [ { b : 3 }, { b : 4 } ] });
      try {
        collection.save({ a : [ { b : 2 } ] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(3, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testDeduplicationOff : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["a[*].b"], deduplicate: false });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a[*].b"], idx.fields);
      assertEqual(false, idx.deduplicate);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : [ { b : 1 } ] });
      collection.save({ a : [ { b : 2 } ] });
      collection.save({ a : [ { b : 3 }, { b : 4 } ] });
      try {
        collection.save({ a : [ { b : 5 }, { b : 5 } ] });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(3, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: hash index creation
////////////////////////////////////////////////////////////////////////////////

    testCreationUniqueConstraint : function () {
      var idx = collection.ensureHashIndex("a");
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("hash", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureHashIndex("a");

      assertEqual(id, idx.id);
      assertEqual("hash", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);

      idx = collection.ensureHashIndex("a", { sparse: true });

      assertNotEqual(id, idx.id);
      assertEqual("hash", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(true, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);
      id = idx.id;

      idx = collection.ensureHashIndex("a", { sparse: true });

      assertEqual(id, idx.id);
      assertEqual("hash", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(true, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: permuted attributes
////////////////////////////////////////////////////////////////////////////////

    testCreationPermutedUniqueConstraint : function () {
      var idx = collection.ensureHashIndex("a", "b");
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureHashIndex("b", "a");

      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
      assertNotEqual(id, idx.id);
      assertTrue(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniqueDocuments : function () {
      var idx = collection.ensureHashIndex("a", "b");

      assertEqual("hash", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
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
      var idx = collection.ensureHashIndex("a", "b", { sparse: true });

      assertEqual("hash", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
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
      collection.ensureUniqueConstraint("a");
      collection.ensureSkiplist("b");

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
      collection.ensureUniqueConstraint("a", { sparse: true });
      collection.ensureSkiplist("b", { sparse: true });

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
      collection.ensureUniqueSkiplist("a");
      collection.ensureHashIndex("b");

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
      collection.ensureUniqueSkiplist("a", { sparse: true });
      collection.ensureHashIndex("b", { sparse: true });

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
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniquenessAndLookup : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["value"] });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["value"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      var bound = 1000;
      var i;
      for (i = -bound; i < bound; ++i) {
        collection.insert({ value: i });
      }
      
      internal.db._executeTransaction({
        collections: { write: cn },
        action: function(params) {
          // need to run compaction in the rocksdb case, as the lookups
          // may use bloom filters afterwards but not for memtables
          require("internal").db[params.cn].compact();
        },
        params: { cn }
      });

      assertEqual(2 * bound, collection.count());

      for (i = -bound; i < bound; ++i) {
        var docs = collection.byExample({ value: i }).toArray();
        assertEqual(1, docs.length);
        assertEqual(i, docs[0].value);
        
        collection.update(docs[0]._key, docs[0]);
      }

      for (i = -bound; i < bound; ++i) {
        try {
          collection.insert({ value: i });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
        }
      }
    },

    testUniquenessAndLookup2 : function () {
      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["value"] });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["value"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      var i = 0;
      while (i < 100000) {
        for (var j = 0; j < 20; ++j) {
          collection.insert({ value: i++ });
        }
        i *= 2;
      }

      internal.db._executeTransaction({
        collections: { write: cn },
        action: function(params) {
          // need to run compaction in the rocksdb case, as the lookups
          // may use bloom filters afterwards but not for memtables
          require("internal").db[params.cn].compact();
        },
        params: { cn }
      });
        
      i = 0;
      while (i < 100000) {
        for (j = 0; j < 20; ++j) {
          var docs = collection.byExample({ value: i }).toArray();
          assertEqual(1, docs.length);
          assertEqual(i, docs[0].value);
          collection.update(docs[0]._key, docs[0]);
          ++i;
        }
        i *= 2;
      }
    },

    testUniqueIndexNullSubattribute : function () {
      let idx = collection.ensureIndex({ type: "hash", unique: true, fields: ["a.b"] });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a.b"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      // as "a" is null here, "a.b" should also be null, at least it should not fail when accessing it via the index
      collection.insert({ _key: "test", a : null });
      collection.update("test", { something: "test2" });

      let doc = collection.document("test");
      assertNull(doc.a);
      assertEqual("test2", doc.something);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(HashIndexSuite);

return jsunity.done();

