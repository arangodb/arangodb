/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the index
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
var errors = internal.errors;
var testHelper = require("@arangodb/test-helper").Helper;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: basics
////////////////////////////////////////////////////////////////////////////////

function indexSuite() {
  'use strict';
  var cn = "UnitTestsCollectionIdx";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { waitForSync : false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // we need try...catch here because at least one test drops the collection itself!
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
/// @brief test: get indexes
////////////////////////////////////////////////////////////////////////////////

    testGetIndexes : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      collection.ensureGeoIndex("a");
      collection.ensureGeoIndex("a", "b");

      res = collection.getIndexes();

      assertEqual(3, res.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get index
////////////////////////////////////////////////////////////////////////////////

    testIndex : function () {
      var id = collection.ensureGeoIndex("a");

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
/// @brief drop index
////////////////////////////////////////////////////////////////////////////////

    testDropIndex : function () {
      var id = collection.ensureGeoIndex("a");
      var res = collection.dropIndex(id.id);
      assertTrue(res);

      res = collection.dropIndex(id.id);
      assertFalse(res);

      id = collection.ensureGeoIndex("a");
      res = collection.dropIndex(id);
      assertTrue(res);

      res = collection.dropIndex(id);
      assertFalse(res);

      id = collection.ensureGeoIndex("a");
      res = internal.db._dropIndex(id);
      assertTrue(res);

      res = internal.db._dropIndex(id);
      assertFalse(res);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop index by id string
////////////////////////////////////////////////////////////////////////////////

    testDropIndexString : function () {
      // pick up the numeric part (starts after the slash)
      var id = collection.ensureGeoIndex("a").id.substr(cn.length + 1);
      var res = collection.dropIndex(collection.name() + "/" + id);
      assertTrue(res);

      res = collection.dropIndex(collection.name() + "/" + id);
      assertFalse(res);

      id = collection.ensureGeoIndex("a").id.substr(cn.length + 1);
      res = collection.dropIndex(parseInt(id, 10));
      assertTrue(res);

      res = collection.dropIndex(parseInt(id, 10));
      assertFalse(res);

      id = collection.ensureGeoIndex("a").id.substr(cn.length + 1);
      res = internal.db._dropIndex(collection.name() + "/" + id);
      assertTrue(res);

      res = internal.db._dropIndex(collection.name() + "/" + id);
      assertFalse(res);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief access a non-existing index
////////////////////////////////////////////////////////////////////////////////

    testGetNonExistingIndexes : function () {
      [ "2444334000000", "9999999999999" ].forEach(function (id) {
        try {
          collection.index(id);
          fail();
        }
        catch (err) {
          assertEqual(errors.ERROR_ARANGO_INDEX_NOT_FOUND.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief access an existing index of an unloaded collection
////////////////////////////////////////////////////////////////////////////////

    testGetIndexUnloaded : function () {
      var idx = collection.ensureHashIndex("test");

      testHelper.waitUnload(collection);

      assertEqual(idx.id, collection.index(idx.id).id);
      assertEqual(idx.id, collection.getIndexes()[1].id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief access an existing index of a dropped collection
////////////////////////////////////////////////////////////////////////////////

    testGetIndexDropped : function () {
      var idx = collection.ensureHashIndex("test");

      collection.drop();

      if (internal.coverage || internal.valgrind) {
        internal.wait(2, false);
      }
      try {
        collection.index(idx.id);
        fail();
      }
      catch (e1) {
        assertEqual(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, e1.errorNum);
      }

      try {
        collection.getIndexes();
        fail();
      }
      catch (e2) {
        assertEqual(errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, e2.errorNum);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: return value of getIndexes
////////////////////////////////////////////////////////////////////////////////

function getIndexesSuite() {
  'use strict';
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
      collection.drop();
      collection = null;
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get primary
////////////////////////////////////////////////////////////////////////////////

    testGetPrimary : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);
      var idx = res[0];

      assertEqual("primary", idx.type);
      assertTrue(idx.unique);
      assertEqual([ "_key" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetHashUnique1 : function () {
      collection.ensureUniqueConstraint("value");
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetHashUnique2 : function () {
      collection.ensureUniqueConstraint("value1", "value2");
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseHashUnique1 : function () {
      collection.ensureUniqueConstraint("value", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseHashUnique2 : function () {
      collection.ensureUniqueConstraint("value1", "value2", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetHashNonUnique1 : function () {
      collection.ensureHashIndex("value");
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetHashNonUnique2 : function () {
      collection.ensureHashIndex("value1", "value2");
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseHashNonUnique1 : function () {
      collection.ensureHashIndex("value", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseHashNonUnique2 : function () {
      collection.ensureHashIndex("value1", "value2", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSkiplistUnique1 : function () {
      collection.ensureUniqueSkiplist("value");
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("skiplist", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSkiplistUnique2 : function () {
      collection.ensureUniqueSkiplist("value1", "value2");
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("skiplist", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseSkiplistUnique1 : function () {
      collection.ensureUniqueSkiplist("value", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("skiplist", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseSkiplistUnique2 : function () {
      collection.ensureUniqueSkiplist("value1", "value2", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("skiplist", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSkiplistNonUnique1 : function () {
      collection.ensureSkiplist("value");
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSkiplistNonUnique2 : function () {
      collection.ensureSkiplist("value1", "value2");
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseSkiplistNonUnique1 : function () {
      collection.ensureSkiplist("value", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseSkiplistNonUnique2 : function () {
      collection.ensureSkiplist("value1", "value2", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get fulltext index
////////////////////////////////////////////////////////////////////////////////

    testGetFulltext: function () {
      collection.ensureFulltextIndex("value");
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("fulltext", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetRocksDBUnique1 : function () {
      collection.ensureIndex({ type: "persistent", unique: true, fields: ["value"] });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetRocksDBUnique2 : function () {
      collection.ensureIndex({ type: "persistent", unique: true, fields: ["value1", "value2"] });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseRocksDBUnique1 : function () {
      collection.ensureIndex({ type: "persistent", unique: true, fields: ["value"], sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseRocksDBUnique2 : function () {
      collection.ensureIndex({ type: "persistent", unique: true, fields: ["value1", "value2"], sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetRocksDBNonUnique1 : function () {
      collection.ensureIndex({ type: "persistent", fields: ["value"] });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetRocksDBNonUnique2 : function () {
      collection.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseRocksDBNonUnique1 : function () {
      collection.ensureIndex({ type: "persistent", fields: ["value"], sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get non-unique rocksdb index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseRocksDBNonUnique2 : function () {
      collection.ensureIndex({ type: "persistent", fields: ["value1", "value2"], sparse: true });
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value1", "value2" ], idx.fields);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: return value of getIndexes for an edge collection
////////////////////////////////////////////////////////////////////////////////

function getIndexesEdgesSuite() {
  'use strict';
  var cn = "UnitTestsCollectionIdx";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._createEdgeCollection(cn, { waitForSync : false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      collection = null;
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get primary
////////////////////////////////////////////////////////////////////////////////

    testGetPrimary : function () {
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[0];

      assertEqual("primary", idx.type);
      assertTrue(idx.unique);
      assertEqual([ "_key" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get edge
////////////////////////////////////////////////////////////////////////////////

    testGetEdge : function () {
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("edge", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "_from", "_to" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo constraint
////////////////////////////////////////////////////////////////////////////////

    testGetGeoConstraint1 : function () {
      collection.ensureGeoConstraint("lat", "lon", false);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo2", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual([ "lat", "lon" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo constraint
////////////////////////////////////////////////////////////////////////////////

    testGetGeoConstraint2 : function () {
      collection.ensureGeoConstraint("lat", "lon", true);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo2", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual([ "lat", "lon" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo constraint
////////////////////////////////////////////////////////////////////////////////

    testGetGeoConstraint3 : function () {
      collection.ensureGeoConstraint("lat", true, true);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo1", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.geoJson);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual([ "lat" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo index
////////////////////////////////////////////////////////////////////////////////

    testGetGeoIndex1 : function () {
      collection.ensureGeoIndex("lat", true, false);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo1", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.geoJson);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual([ "lat" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo index
////////////////////////////////////////////////////////////////////////////////

    testGetGeoIndex2 : function () {
      collection.ensureGeoIndex("lat", "lon");
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo2", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertEqual([ "lat", "lon" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetHashUnique : function () {
      collection.ensureUniqueConstraint("value");
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseHashUnique : function () {
      collection.ensureUniqueConstraint("value", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get hash index
////////////////////////////////////////////////////////////////////////////////

    testGetHash : function () {
      collection.ensureHashIndex("value");
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get hash index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseHash : function () {
      collection.ensureHashIndex("value", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSkiplistUnique : function () {
      collection.ensureUniqueSkiplist("value");
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("skiplist", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseSkiplistUnique : function () {
      collection.ensureUniqueSkiplist("value", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("skiplist", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSkiplist : function () {
      collection.ensureSkiplist("value");
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "value" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSparseSkiplist : function () {
      collection.ensureSkiplist("value", { sparse: true });
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "value" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get fulltext index
////////////////////////////////////////////////////////////////////////////////

    testGetFulltext: function () {
      collection.ensureFulltextIndex("value");
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("fulltext", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "value" ], idx.fields);
      assertEqual(2, idx.minLength);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(indexSuite);
jsunity.run(getIndexesSuite);
jsunity.run(getIndexesEdgesSuite);

return jsunity.done();

