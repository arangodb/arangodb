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
const platform = require('internal').platform;

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
      collection = internal.db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // we need try...catch here because at least one test drops the collection itself!
      try {
        collection.unload();
        collection.drop();
      } catch (err) {
      }
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: indexes
////////////////////////////////////////////////////////////////////////////////

    testIndexes : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      collection.ensureGeoIndex("a");
      collection.ensureGeoIndex("a", "b");

      res = collection.indexes();

      assertEqual(3, res.length);
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
/// @brief test: get index by name
////////////////////////////////////////////////////////////////////////////////

    testIndexByName : function () {
      var id = collection.ensureGeoIndex("a");

      var idx = collection.index(id.name);
      assertEqual(id.id, idx.id);
      assertEqual(id.name, idx.name);
      
      var fqn = `${collection.name()}/${id.name}`;
      idx = internal.db._index(fqn);
      assertEqual(id.id, idx.id);
      assertEqual(id.name, idx.name);

      idx = collection.index(fqn);
      assertEqual(id.id, idx.id);
      assertEqual(id.name, idx.name);
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
/// @brief drop index by id string
////////////////////////////////////////////////////////////////////////////////

    testDropIndexByName : function () {
      // pick up the numeric part (starts after the slash)
      var name = collection.ensureGeoIndex("a").name;
      var res = collection.dropIndex(collection.name() + "/" + name);
      assertTrue(res);

      res = collection.dropIndex(collection.name() + "/" + name);
      assertFalse(res);

      name = collection.ensureGeoIndex("a").name;
      res = collection.dropIndex(name);
      assertTrue(res);

      res = collection.dropIndex(name);
      assertFalse(res);

      name = collection.ensureGeoIndex("a").name;
      res = internal.db._dropIndex(collection.name() + "/" + name);
      assertTrue(res);

      res = internal.db._dropIndex(collection.name() + "/" + name);
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
        } catch (err) {
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
      } catch (e1) {
        assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, e1.errorNum);
      }

      try {
        collection.getIndexes();
        fail();
      } catch (e2) {
        assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, e2.errorNum);
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
      collection = internal.db._createEdgeCollection(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get primary
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetPrimary : function () {
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

    testEdgeGetGeoConstraint1 : function () {
      collection.ensureGeoConstraint("lat", "lon", false);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "lat", "lon" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo constraint
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetGeoConstraint2 : function () {
      collection.ensureGeoConstraint("lat", "lon", true);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "lat", "lon" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo constraint
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetGeoConstraint3 : function () {
      collection.ensureGeoConstraint("lat", true, true);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.geoJson);
      assertTrue(idx.sparse);
      assertEqual([ "lat" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetGeoIndex1 : function () {
      collection.ensureGeoIndex("lat", true, true);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.geoJson);
      assertTrue(idx.sparse);
      assertEqual([ "lat" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get geo index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetGeoIndex2 : function () {
      collection.ensureGeoIndex("lat", "lon");
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("geo", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "lat", "lon" ], idx.fields);
      assertTrue(idx.hasOwnProperty("id"));
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique hash index
////////////////////////////////////////////////////////////////////////////////

    testEdgeGetHashUnique : function () {
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

    testEdgeGetSparseHashUnique : function () {
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

    testEdgeGetHash : function () {
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

    testEdgeGetSparseHash : function () {
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

    testEdgeGetSkiplistUnique : function () {
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

    testEdgeGetSparseSkiplistUnique : function () {
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

    testEdgeGetSkiplist : function () {
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

    testEdgeGetSparseSkiplist : function () {
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

    testEdgeGetFulltext: function () {
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
/// @brief test suite: test multi-index rollback
////////////////////////////////////////////////////////////////////////////////

function multiIndexRollbackSuite() {
  'use strict';
  var cn = "UnitTestsCollectionIdx";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._createEdgeCollection(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test rollback on index insertion
////////////////////////////////////////////////////////////////////////////////

    testIndexRollback: function () {
      collection.ensureIndex({ type: "hash", fields: ["_from", "_to", "link"], unique: true });
      collection.ensureIndex({ type: "hash", fields: ["_to", "ext"], unique: true, sparse: true });

      var res = collection.getIndexes();

      assertEqual(4, res.length);
      assertEqual("primary", res[0].type);
      assertEqual("edge", res[1].type);
      assertEqual("hash", res[2].type);
      assertEqual("hash", res[3].type);

      var docs = [
        {"_from": "fromC/a", "_to": "toC/1", "link": "one"},
        {"_from": "fromC/b", "_to": "toC/1", "link": "two"},
        {"_from": "fromC/c", "_to": "toC/1", "link": "one"}
      ];

      collection.insert(docs);
      assertEqual(3, collection.count());

      try {
        internal.db._query('FOR doc IN [ {_from: "fromC/a", _to: "toC/1", link: "one", ext: 2337789}, {_from: "fromC/b", _to: "toC/1", link: "two", ext: 2337799}, {_from: "fromC/c", _to: "toC/1", link: "one", ext: 2337789} ] UPSERT {_from: doc._from, _to: doc._to, link: doc.link} INSERT { _from: doc._from, _to: doc._to, link: doc.link, ext: doc.ext} UPDATE {ext: doc.ext} IN ' + collection.name());
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      res = internal.db._query("FOR doc IN " + collection.name() + " FILTER doc._to == 'toC/1' RETURN doc._from").toArray();
      assertEqual(3, res.length);
    }

  };
}

function parallelIndexSuite() {
  'use strict';
  let cn = "UnitTestsCollectionIdx";
  let tasks = require("@arangodb/tasks");

  return {

    setUp : function () {
      internal.db._drop(cn);
      internal.db._create(cn);
    },

    tearDown : function () {
      tasks.get().forEach(function(task) {
        if (task.id.match(/^UnitTest/) || task.name.match(/^UnitTest/)) {
          try {
            tasks.unregister(task);
          }
          catch (err) {
          }
        }
      });
      internal.db._drop(cn);
    },

    testCreateInParallel: function () {
      let noIndices = 80;
      if (platform.substr(0, 3) === 'win') {
        // Relax condition for windows - TODO: fix this.
        noIndices = 40;
      }
      for (let i = 0; i < noIndices; ++i) {
        let command = 'require("internal").db._collection("' + cn + '").ensureIndex({ type: "hash", fields: ["value' + i + '"] });';
        tasks.register({ name: "UnitTestsIndexCreate" + i, command: command });
      }

      let time = require("internal").time;
      let start = time();
      while (true) {
        let indexes = require("internal").db._collection(cn).getIndexes();
        if (indexes.length === noIndices + 1) {
          // primary index + user-defined indexes
          break;
        }
        if (time() - start > 180) {
          // wait for 3 minutes maximum
          fail("Timeout creating " + noIndices + " indices after 3 minutes: " + JSON.stringify(indexes));
        }
        require("internal").wait(0.5, false);
      }
        
      let indexes = require("internal").db._collection(cn).getIndexes();
      assertEqual(noIndices + 1, indexes.length);
    },

    testCreateInParallelDuplicate: function () {
      let n = 100;
      for (let i = 0; i < n; ++i) {
        let command = 'require("internal").db._collection("' + cn + '").ensureIndex({ type: "hash", fields: ["value' + (i % 4) + '"] });';
        tasks.register({ name: "UnitTestsIndexCreate" + i, command: command });
      }

      let time = require("internal").time;
      let start = time();
      while (true) {
        let indexes = require("internal").db._collection(cn).getIndexes();
        if (indexes.length === 4 + 1) {
          // primary index + user-defined indexes
          break;
        }
        if (time() - start > 180) {
          // wait for 3 minutes maximum
          fail("Timeout creating indices after 3 minutes: " + JSON.stringify(indexes));
        }
        require("internal").wait(0.5, false);
      }
      
      // wait some extra time because we just have 4 distinct indexes
      // these will be created relatively quickly. by waiting here a bit
      // we give the other pending tasks a chance to execute too (but they
      // will not do anything because the target indexes already exist)
      require("internal").wait(5, false);
        
      let indexes = require("internal").db._collection(cn).getIndexes();
      assertEqual(4 + 1, indexes.length);
    }

  };
}

jsunity.run(indexSuite);
jsunity.run(getIndexesSuite);
jsunity.run(getIndexesEdgesSuite);
jsunity.run(multiIndexRollbackSuite);
jsunity.run(parallelIndexSuite);

return jsunity.done();
