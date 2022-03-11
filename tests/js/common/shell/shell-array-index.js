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

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;

function arrayIndexSuite () {
  const cn = "UnitTestsCollectionIdx";
  let collection = null;

  return {
    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

    tearDown : function () {
      internal.db._drop(cn);
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: create an array index
////////////////////////////////////////////////////////////////////////////////

    testCreateIndex : function () {
      var indexes = collection.getIndexes();
      
      assertEqual(1, indexes.length);

      collection.ensureIndex({ type: "persistent", fields: ["a[*]"] });
      collection.ensureIndex({ type: "persistent", fields: ["b[*]"], unique: true });

      indexes = collection.getIndexes();

      assertEqual(3, indexes.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get index
////////////////////////////////////////////////////////////////////////////////

    testIndex : function () {
      var id = collection.ensureIndex({ type: "persistent", fields: ["a[*]"] });

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

    testInsertAndReadArrayCombinedUnique : function () {
      collection.ensureIndex({ type: "persistent", fields: ["a[*]", "b[*]"], unique: true });

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
      collection.ensureIndex({ type: "persistent", fields: ["a[*]"], unique: true });

      collection.save({a: [1, 2]});

      try {
        collection.save({a: [1, 4]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple identical elements in array with unique constraint
////////////////////////////////////////////////////////////////////////////////

    testInsertAndReadArrayIdenticalElementsUnique : function () {
      collection.ensureIndex({ type: "persistent", fields: ["a[*]"], unique: true });

      collection.save({a: [1, 2, 1, 3, 1]});

      try {
        collection.save({a: [4, 1]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Multiple index batch inserts
////////////////////////////////////////////////////////////////////////////////

    testInsertBatches : function () {
      const n = 1000;

      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ a: [ "foo", "bar" ] }); 
      }
      collection.insert(docs);

      // this is expected to just work and not fail
      collection.ensureIndex({ type: "persistent", fields: ["a[*]"] }); 
      collection.ensureIndex({ type: "persistent", fields: ["a[*]", "b[*]"] });

      assertEqual(n, collection.count());
      assertEqual(3, collection.getIndexes().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Test update of an array index where entries are removed:
////////////////////////////////////////////////////////////////////////////////

    testArrayIndexUpdates : function () {
      collection.ensureIndex({type:"persistent", fields: ["a[*].b"], unique: true});

      let meta = collection.insert({a: [{b:"xyz"}]});

      try {
        collection.insert({a: [{b:"xyz"}]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      collection.update(meta._key, {a: []});

      let meta2 = collection.insert({a: [{b:"xyz"}]});  // must work again

      try {
        collection.insert({a: [{b:"xyz"}]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      collection.replace(meta2._key, {a: []});

      collection.insert({a: [{b:"xyz"}]});  // must work again
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Test update of an array index where entries are changed:
////////////////////////////////////////////////////////////////////////////////

    testArrayIndexUpdates2 : function () {
      collection.ensureIndex({type:"persistent", fields: ["a[*].b"], unique: true});

      let meta = collection.insert({a: [{b:"xyz"}]});

      try {
        collection.insert({a: [{b:"xyz"}]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      collection.update(meta._key, {a: [{b:"123"}]});

      let meta2 = collection.insert({a: [{b:"xyz"}]});  // must work again

      try {
        collection.insert({a: [{b:"xyz"}]});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      collection.replace(meta2._key, {a: [{b:"456"}]});

      collection.insert({a: [{b:"xyz"}]});  // must work again
    },

  };
}

jsunity.run(arrayIndexSuite);

return jsunity.done();
