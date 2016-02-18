/*global assertEqual, fail*/

////////////////////////////////////////////////////////////////////////////////
/// @brief test the array index
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

"use strict";

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;

function arrayHashIndexSuite () {

  var cn = "UnitTestsCollectionIdx";
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
      internal.db._drop(cn);
      collection = null;
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: create an array index
////////////////////////////////////////////////////////////////////////////////

    testCreateHashIndex : function () {
      var indexes = collection.getIndexes();
      
      assertEqual(1, indexes.length);

      collection.ensureHashIndex("a[*]");
      collection.ensureUniqueConstraint("b[*]");

      indexes = collection.getIndexes();

      assertEqual(3, indexes.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get index
////////////////////////////////////////////////////////////////////////////////

    testIndex : function () {
      var id = collection.ensureHashIndex("a[*]");

      var idx = collection.index(id.id);
      assertEqual(id.id, idx.id);

      idx = collection.index(id);
      assertEqual(id.id, idx.id);

      idx = internal.db._index(id.id);
      assertEqual(id.id, idx.id);

      idx = internal.db._index(id);
      assertEqual(id.id, idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Simple index insertion and reading
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArraySimple : function () {
      var idx = collection.ensureHashIndex("a[*]").id;

      // Should not be inserted into the index
      collection.save({a: 1});

      var res = collection.BY_EXAMPLE_HASH(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 0);

      // Should be inserted into the index
      var id1 = collection.save({a: [1]})._id;

      res = collection.BY_EXAMPLE_HASH(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id1);

      // Should be inserted into the index twice
      var id2 = collection.save({a: [1, 2]})._id;

      // Check for first array entry
      res = collection.BY_EXAMPLE_HASH(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 2);
      if (res[0]._id === id1) {
        assertEqual(res[1]._id, id2);
      }
      else {
        assertEqual(res[0]._id, id2);
        assertEqual(res[1]._id, id1);
      }

      // Check for second array entry
      res = collection.BY_EXAMPLE_HASH(idx, {a: 2}, 0, null).documents;
      assertEqual(res[0]._id, id2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Combined index insertion and reading
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayCombined : function () {
      var idx = collection.ensureHashIndex("a[*]", "b[*]").id;

      var id = collection.save({a: [1, 2], b: ["a", "b"]})._id;

      // All combinations should be in the index.
      var res = collection.BY_EXAMPLE_HASH(idx, {a: 1, b: "a"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 2, b: "a"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 1, b: "b"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 2, b: "b"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      // It should be possible to insert arbitarary null values
      
      var id1 = collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]})._id;
      var id2 = collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]})._id;
      var id3 = collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]})._id;
      var ids = [id1, id2, id3].sort();
      res = collection.BY_EXAMPLE_HASH(idx, {a: "duplicate", b: "duplicate"}, 0, null).documents;
      res = res.map(function(r) { return r._id; }).sort();
      assertEqual(res.length, 3);
      assertEqual(res, ids);

      res = collection.BY_EXAMPLE_HASH(idx, {a: "duplicate", b: null}, 0, null).documents;
      res = res.map(function(r) { return r._id; }).sort();
      assertEqual(res.length, 3);
      assertEqual(res, ids);

      res = collection.BY_EXAMPLE_HASH(idx, {a: null, b: "duplicate"}, 0, null).documents;
      res = res.map(function(r) { return r._id; }).sort();
      assertEqual(res.length, 3);
      assertEqual(res, ids);

      res = collection.BY_EXAMPLE_HASH(idx, {a: null, b: null}, 0, null).documents;
      res = res.map(function(r) { return r._id; }).sort();
      assertEqual(res.length, 3);
      assertEqual(res, ids);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple identical elements in sparse array 
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayCombinedSparse : function () {
      var idx = collection.ensureHashIndex("a[*]", "b[*]", {sparse: true}).id;
      // Sparse does not have any effect on array attributes.

      var id = collection.save({a: [1, 2], b: ["a", "b"]})._id;

      // All combinations should be in the index.
      var res = collection.BY_EXAMPLE_HASH(idx, {a: 1, b: "a"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 2, b: "a"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 1, b: "b"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 2, b: "b"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      // It should be possible to insert arbitarary null values
      
      var id1 = collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]})._id;
      var id2 = collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]})._id;
      var id3 = collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]})._id;
      var ids = [id1, id2, id3].sort();
      res = collection.BY_EXAMPLE_HASH(idx, {a: "duplicate", b: "duplicate"}, 0, null).documents;
      res = res.map(function(r) { return r._id; }).sort();
      assertEqual(res.length, 3);
      assertEqual(res, ids);

      res = collection.BY_EXAMPLE_HASH(idx, {a: "duplicate", b: null}, 0, null).documents;
      assertEqual(res.length, 3);

      res = collection.BY_EXAMPLE_HASH(idx, {a: null, b: "duplicate"}, 0, null).documents;
      assertEqual(res.length, 3);

      res = collection.BY_EXAMPLE_HASH(idx, {a: null, b: null}, 0, null).documents;
      assertEqual(res.length, 3);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple identical elements in unique array 
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayCombinedUnique : function () {
      var idx = collection.ensureHashIndex("a[*]", "b[*]", {unique: true}).id;

      var id = collection.save({a: [1, 2], b: ["a", "b"]})._id;

      // All combinations should be in the index.
      var res = collection.BY_EXAMPLE_HASH(idx, {a: 1, b: "a"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 2, b: "a"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 1, b: "b"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 2, b: "b"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      // It should be possible to insert arbitarary null values
      
      // This should be insertable
      var id1 = collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]})._id;

      try {
        // This should not be insertable we have the one before
        collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]});
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
      }

      var ids = [id1];
      res = collection.BY_EXAMPLE_HASH(idx, {a: "duplicate", b: "duplicate"}, 0, null).documents;
      res = res.map(function(r) { return r._id; }).sort();
      assertEqual(res.length, 1);
      assertEqual(res, ids);

      res = collection.BY_EXAMPLE_HASH(idx, {a: "duplicate", b: null}, 0, null).documents;
      res = res.map(function(r) { return r._id; }).sort();
      assertEqual(res.length, 1);
      assertEqual(res, ids);

      res = collection.BY_EXAMPLE_HASH(idx, {a: null, b: "duplicate"}, 0, null).documents;
      res = res.map(function(r) { return r._id; }).sort();
      assertEqual(res.length, 1);
      assertEqual(res, ids);

      res = collection.BY_EXAMPLE_HASH(idx, {a: null, b: null}, 0, null).documents;
      res = res.map(function(r) { return r._id; }).sort();
      assertEqual(res.length, 1);
      assertEqual(res, ids);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple identical elements in array 
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayIdenticalElements : function () {
      var idx = collection.ensureHashIndex("a[*]").id;

      var id = collection.save({a: [1, 2, 1, 3, 1]})._id;

      // Document should be inserted once for each array entry
      var res = collection.BY_EXAMPLE_HASH(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 2}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 3}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Unique index insertion and reading
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayUnique : function () {
      var idx = collection.ensureUniqueConstraint("a[*]").id;

      var id = collection.save({a: [1, 2]})._id;

      // Document should be inserted once for each array entry
      var res = collection.BY_EXAMPLE_HASH(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 2}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      try {
        collection.save({a: [1, 4]});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      res = collection.BY_EXAMPLE_HASH(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 4}, 0, null).documents;
      assertEqual(res.length, 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple identical elements in array with unique constraint
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayIdenticalElementsUnique : function () {
      var idx = collection.ensureUniqueConstraint("a[*]").id;

      var id = collection.save({a: [1, 2, 1, 3, 1]})._id;

      // Document should be inserted once for each array entry
      var res = collection.BY_EXAMPLE_HASH(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 2}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 3}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      try {
        collection.save({a: [4, 1]});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      res = collection.BY_EXAMPLE_HASH(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_HASH(idx, {a: 4}, 0, null).documents;
      assertEqual(res.length, 0);
    },

    testInsertAndReadArraySparse : function () {
      var idx = collection.ensureHashIndex("a[*]", "b", {sparse: true}).id;

      // None of these should be found
      collection.save({a: [1, 2]});
      collection.save({b: 2});
      collection.save({a: [], b: 2});
      collection.save({a: [], b: null});
      collection.save({a: null, b: 2});

      var res = collection.BY_EXAMPLE_HASH(idx, {}, 0, null).documents;
      assertEqual(res.length, 0);

      res = collection.BY_EXAMPLE_HASH(idx, {a: null}, 0, null).documents;
      assertEqual(res.length, 0);

      res = collection.BY_EXAMPLE_HASH(idx, {b: null}, 0, null).documents;
      assertEqual(res.length, 0);

      // This should be found
      var id6 = collection.save({a: [1, 2], b: 2})._id;
      res = collection.BY_EXAMPLE_HASH(idx, {a: 1, b: 2}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id6);

      // This should be found
      var id7 = collection.save({a: [3, null, 5], b: 1})._id;
      res = collection.BY_EXAMPLE_HASH(idx, {a: 3, b: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id7);

      
      res = collection.BY_EXAMPLE_HASH(idx, {a: 5, b: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id7);

      // And this also
      res = collection.BY_EXAMPLE_HASH(idx, {a: null, b: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id7);
    },

    /* TODO Decission required here
    testInsertAndReadNestedElements: function () {
      var idx = collection.ensureHashIndex("a[*].b").id;
      var id1 = collection.save({a: [{b: 1}]})._id;
      var id2 = collection.save({a: [{b: 1}, {b: 2}]})._id;
      collection.save({a: [1,2,3]});
      collection.save({b: [1,2,3]});

      var res = collection.BY_EXAMPLE_HASH(idx, {a: {b: 1} }, 0, null).documents;
      assertEqual(res.length, 2);
      assertEqual(res[0]._id, id1);
      assertEqual(res[0]._id, id2);

      res = collection.BY_EXAMPLE_HASH(idx, {a: {b: 2} }, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id2);
    },
    */

  };
}

function arraySkiplistIndexSuite () {

  var cn = "UnitTestsCollectionIdx";
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
      internal.db._drop(cn);
      collection = null;
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: create an array index
////////////////////////////////////////////////////////////////////////////////

    testCreateSkiplistIndex : function () {
      var indexes = collection.getIndexes();
      
      assertEqual(1, indexes.length);

      collection.ensureSkiplist("a[*]");
      collection.ensureUniqueSkiplist("b[*]");

      indexes = collection.getIndexes();

      assertEqual(3, indexes.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get index
////////////////////////////////////////////////////////////////////////////////

    testIndex : function () {
      var id = collection.ensureSkiplist("a[*]");

      var idx = collection.index(id.id);
      assertEqual(id.id, idx.id);

      idx = collection.index(id);
      assertEqual(id.id, idx.id);

      idx = internal.db._index(id.id);
      assertEqual(id.id, idx.id);

      idx = internal.db._index(id);
      assertEqual(id.id, idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Simple index insertion and reading
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArraySimple : function () {
      var idx = collection.ensureSkiplist("a[*]").id;

      // Should not be inserted into the index
      collection.save({a: 1});

      var res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 0);

      // Should be inserted into the index
      var id1 = collection.save({a: [1]})._id;

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id1);

      // Should be inserted into the index twice
      var id2 = collection.save({a: [1, 2]})._id;

      // Check for first array entry
      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 2);
      if (res[0]._id === id1) {
        assertEqual(res[1]._id, id2);
      }
      else {
        assertEqual(res[0]._id, id2);
        assertEqual(res[1]._id, id1);
      }

      // Check for second array entry
      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 2}, 0, null).documents;
      assertEqual(res[0]._id, id2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Combined index insertion and reading
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayCombined : function () {
      var idx = collection.ensureSkiplist("a[*]", "b[*]").id;

      var id = collection.save({a: [1, 2], b: ["a", "b"]})._id;

      // All combinations should be in the index.
      var res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1, b: "a"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 2, b: "a"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1, b: "b"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 2, b: "b"}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple identical elements in array 
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayIdenticalElements : function () {
      var idx = collection.ensureSkiplist("a[*]").id;

      var id = collection.save({a: [1, 2, 1, 3, 1]})._id;

      // Document should be inserted once for each array entry
      var res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 2}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 3}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Unique index insertion and reading
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayUnique : function () {
      var idx = collection.ensureUniqueSkiplist("a[*]").id;

      var id = collection.save({a: [1, 2]})._id;

      // Document should be inserted once for each array entry
      var res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 2}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      try {
        collection.save({a: [1, 4]});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 4}, 0, null).documents;
      assertEqual(res.length, 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple identical elements in array with unique constraint
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayIdenticalElementsUnique : function () {
      var idx = collection.ensureUniqueSkiplist("a[*]").id;

      var id = collection.save({a: [1, 2, 1, 3, 1]})._id;

      // Document should be inserted once for each array entry
      var res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 2}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 3}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      try {
        collection.save({a: [4, 1]});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 4}, 0, null).documents;
      assertEqual(res.length, 0);
    },

    testInsertAndReadArraySparse : function () {
      var idx = collection.ensureSkiplist("a[*]", "b", {sparse: true}).id;

      // None of these should be found
      collection.save({a: [1, 2]});
      collection.save({b: 2});
      collection.save({a: [], b: 2});
      collection.save({a: [], b: null});
      collection.save({a: null, b: 2});

      var res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: null, b: 2}, 0, null).documents;
      assertEqual(res.length, 0);

      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1, b: null}, 0, null).documents;
      assertEqual(res.length, 0);

      // This should be found
      var id6 = collection.save({a: [1, 2], b: 2})._id;
      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 1, b: 2}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id6);

      // This should be found
      var id7 = collection.save({a: [3, null, 5], b: 1})._id;
      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 3, b: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id7);

      
      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: 5, b: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id7);

      // And this also
      res = collection.BY_EXAMPLE_SKIPLIST(idx, {a: null, b: 1}, 0, null).documents;
      assertEqual(res.length, 1);
      assertEqual(res[0]._id, id7);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(arrayHashIndexSuite);
jsunity.run(arraySkiplistIndexSuite);

return jsunity.done();

