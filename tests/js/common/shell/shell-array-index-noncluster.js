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

    testHashIndex : function () {
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
/// @brief test: Multiple identical elements in unique array 
////////////////////////////////////////////////////////////////////////////////

    testHashInsertAndReadArrayCombinedUnique : function () {
      collection.ensureHashIndex("a[*]", "b[*]", {unique: true});

      collection.save({a: [1, 2], b: ["a", "b"]});

      // It should be possible to insert arbitarary null values
      
      // This should be insertable
      collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]});

      try {
        // This should not be insertable we have the one before
        collection.save({a: ["duplicate", null, "duplicate"], b: ["duplicate", null, "duplicate"]});
        fail();
      } catch (e) {
        assertEqual(e.errorNum, errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Unique index insertion and reading
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayUnique : function () {
      collection.ensureUniqueConstraint("a[*]");

      collection.save({a: [1, 2]});

      try {
        collection.save({a: [1, 4]});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple identical elements in array with unique constraint
////////////////////////////////////////////////////////////////////////////////

    testHashInsertAndReadArrayIdenticalElementsUnique : function () {
      collection.ensureUniqueConstraint("a[*]");

      collection.save({a: [1, 2, 1, 3, 1]});

      try {
        collection.save({a: [4, 1]});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple index batch inserts
////////////////////////////////////////////////////////////////////////////////

/*
    testHashInsertBatches : function () {
      // this really needs to be 1,000,000 documents to reproduce a bug that
      // occurred with exactly this value and no others
      for (var i = 0; i < 1000 * 1000; ++i) {
        collection.insert({ a: [ "foo", "bar" ] });  
      }

      // this is expected to just work and not fail
      collection.ensureIndex({ type: "hash", fields: ["a[*]"] }); 
      collection.ensureIndex({ type: "hash", fields: ["a[*]", "b[*]"] });

      assertEqual(1000 * 1000, collection.count());
      assertEqual(3, collection.getIndexes().length);
    }
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

    testSkipIndex : function () {
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
/// @brief test: Unique index insertion and reading
////////////////////////////////////////////////////////////////////////////////

    testSkipInsertAndReadArrayUnique : function () {
      collection.ensureUniqueSkiplist("a[*]");

      collection.save({a: [1, 2]});

      try {
        collection.save({a: [1, 4]});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple identical elements in array with unique constraint
////////////////////////////////////////////////////////////////////////////////

    testSkipInsertAndReadArrayIdenticalElementsUnique : function () {
      collection.ensureUniqueSkiplist("a[*]");

      collection.save({a: [1, 2, 1, 3, 1]});

      try {
        collection.save({a: [4, 1]});
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(arrayHashIndexSuite);
jsunity.run(arraySkiplistIndexSuite);

return jsunity.done();

