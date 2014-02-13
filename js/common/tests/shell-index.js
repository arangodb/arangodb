/*jslint indent: 2,
         nomen: true,
         maxlen: 80 */
/*global require,
    db,
    assertEqual, assertTrue,
    ArangoCollection */

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

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: basics
////////////////////////////////////////////////////////////////////////////////

function indexSuite() {
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

      collection.unload();
      internal.wait(4);

      assertEqual(idx.id, collection.index(idx.id).id);
      assertEqual(idx.id, collection.getIndexes()[1].id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief access an existing index of a dropped collection
////////////////////////////////////////////////////////////////////////////////

    testGetIndexDropped : function () {
      var idx = collection.ensureHashIndex("test");

      collection.drop();

      try {
        collection.index(idx.id).id;
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

// -----------------------------------------------------------------------------
// --SECTION--                                                      ensure index
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: ensureIndex
////////////////////////////////////////////////////////////////////////////////

function ensureIndexSuite() {
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
        collection.drop();
      }
      catch (err) {
      }
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ids
////////////////////////////////////////////////////////////////////////////////

    testEnsureId1 : function () {
      var id = "273475235";
      var idx = collection.ensureIndex({ type: "hash", fields: [ "a" ], id: id });
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertEqual([ "a" ], res.fields);
      assertEqual(collection.name() + "/" + id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ids
////////////////////////////////////////////////////////////////////////////////

    testEnsureId2 : function () {
      var id = "2734752388";
      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "b", "d" ], id: id });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertEqual([ "b", "d" ], res.fields);
      assertEqual(collection.name() + "/" + id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid type
////////////////////////////////////////////////////////////////////////////////

    testEnsureTypeFail1 : function () {
      try {
        // invalid type given
        collection.ensureIndex({ type: "foo" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid type
////////////////////////////////////////////////////////////////////////////////

    testEnsureTypeFail2 : function () {
      try {
        // no type given
        collection.ensureIndex({ something: "foo" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid type
////////////////////////////////////////////////////////////////////////////////

    testEnsureTypeForbidden1 : function () {
      try {
        // no type given
        collection.ensureIndex({ type: "primary" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid type
////////////////////////////////////////////////////////////////////////////////

    testEnsureTypeForbidden2 : function () {
      try {
        // no type given
        collection.ensureIndex({ type: "edge" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureHash : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "hash", fields: [ "a" ] });
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertEqual([ "a" ], res.fields);
      
      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureUniqueConstraint : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: [ "b", "c" ] });
      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertEqual([ "b", "c" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertTrue(res.unique);
      assertEqual([ "b", "c" ], res.fields);
      
      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashFail : function () {
      try {
        collection.ensureIndex({ type: "hash" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplist : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "a" ] });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertEqual([ "a" ], res.fields);
      
      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureUniqueSkiplist : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "skiplist", unique: true, fields: [ "b", "c" ] });
      assertEqual("skiplist", idx.type);
      assertTrue(idx.unique);
      assertEqual([ "b", "c" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertTrue(res.unique);
      assertEqual([ "b", "c" ], res.fields);
      
      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistFail : function () {
      try {
        collection.ensureIndex({ type: "skiplist" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ fulltext
////////////////////////////////////////////////////////////////////////////////

    testEnsureFulltext : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "fulltext", fields: [ "a" ] });
      assertEqual("fulltext", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a" ], idx.fields);
      assertEqual(2, idx.minLength);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("fulltext", res.type);
      assertFalse(res.unique);
      assertEqual([ "a" ], res.fields);
      assertEqual(2, res.minLength);
      
      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ fulltext
////////////////////////////////////////////////////////////////////////////////

    testEnsureFulltextFail : function () {
      try {
        collection.ensureIndex({ type: "fulltext", fields: [ "a", "b" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeo : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "geo1", fields: [ "a" ] });
      assertEqual("geo1", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a" ], idx.fields);
      assertFalse(idx.ignoreNull);
      assertFalse(idx.geoJson);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("geo1", res.type);
      assertFalse(res.unique);
      assertEqual([ "a" ], res.fields);
      assertFalse(res.ignoreNull);
      assertFalse(res.geoJson);
      
      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoConstraint : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "geo2", fields: [ "a", "b" ], unique: true });
      assertEqual("geo2", idx.type);
      assertTrue(idx.unique);
      assertEqual([ "a", "b" ], idx.fields);
      assertFalse(idx.ignoreNull);
      assertFalse(idx.geoJson);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("geo2", res.type);
      assertTrue(res.unique);
      assertEqual([ "a", "b" ], res.fields);
      assertFalse(res.ignoreNull);
      assertFalse(res.geoJson);
      
      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoFail1 : function () {
      try {
        collection.ensureIndex({ type: "geo1", fields: [ "a", "b" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoFail2 : function () {
      try {
        collection.ensureIndex({ type: "geo1", fields: [ ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoFail3 : function () {
      try {
        collection.ensureIndex({ type: "geo2", fields: [ "a" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoFail4 : function () {
      try {
        collection.ensureIndex({ type: "geo2", fields: [ "a", "b", "c" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ bitarray
////////////////////////////////////////////////////////////////////////////////

    testEnsureBitarray : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "bitarray", fields: [ [ "a", [ 1, 2 ] ], [ "b", [ 3, 4 ] ] ] });
      assertEqual("bitarray", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx["undefined"]);
      assertEqual([ [ "a", [ 1, 2 ] ], [ "b", [ 3, 4 ] ] ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("bitarray", res.type);
      assertFalse(res.unique);
      assertFalse(res["undefined"]);
      assertEqual([ [ "a", [ 1, 2 ] ], [ "b", [ 3, 4 ] ] ], res.fields);
      
      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ bitarray
////////////////////////////////////////////////////////////////////////////////

    testEnsureUndefBitarray : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "bitarray", fields: [ [ "a", [ 1, 2 ] ] ], "undefined" : true });
      assertEqual("bitarray", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx["undefined"]);
      assertEqual([ [ "a", [ 1, 2 ] ] ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("bitarray", res.type);
      assertFalse(res.unique);
      assertTrue(res["undefined"]);
      assertEqual([ [ "a", [ 1, 2 ] ] ], res.fields);
      
      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ bitarray
////////////////////////////////////////////////////////////////////////////////

    testEnsureBitarrayFail1 : function () {
      try {
        collection.ensureIndex({ type: "bitarray", fields: [ "a" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ bitarray
////////////////////////////////////////////////////////////////////////////////

    testEnsureBitarrayFail2 : function () {
      try {
        collection.ensureIndex({ type: "bitarray", fields: [ "a", "b" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ bitarray
////////////////////////////////////////////////////////////////////////////////

    testEnsureBitarrayFail3 : function () {
      try {
        collection.ensureIndex({ type: "bitarray", fields: [ [ "a", "b" ] ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ bitarray
////////////////////////////////////////////////////////////////////////////////

    testEnsureBitarrayFail4 : function () {
      try {
        collection.ensureIndex({ type: "bitarray", fields: [ [ "a" ] ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                        getIndexes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: return value of getIndexes
////////////////////////////////////////////////////////////////////////////////

function getIndexesSuite() {
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
      assertEqual([ "_id" ], idx.fields);
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
      assertEqual([ "value1", "value2" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get bitarray index
////////////////////////////////////////////////////////////////////////////////

    testGetBitarray: function () {
      collection.ensureBitarray("value", [ "one", "two", "three" ]);
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("bitarray", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx["undefined"]);
      assertEqual([ [ "value", [ "one", "two", "three" ] ] ], idx.fields);
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
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                              getIndexes for edges
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: return value of getIndexes for an edge collection
////////////////////////////////////////////////////////////////////////////////

function getIndexesEdgesSuite() {
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
      assertEqual([ "_id" ], idx.fields);
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
      assertTrue(idx.unique);
      assertFalse(idx.ignoreNull);
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
      assertTrue(idx.unique);
      assertTrue(idx.ignoreNull);
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
      assertTrue(idx.unique);
      assertTrue(idx.geoJson);
      assertTrue(idx.ignoreNull);
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
      assertFalse(idx.ignoreNull);
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
      assertFalse(idx.ignoreNull);
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
/// @brief test: get bitarray index
////////////////////////////////////////////////////////////////////////////////

    testGetBitarray: function () {
      collection.ensureBitarray("value", [ "one", "two", "three" ]);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("bitarray", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx["undefined"]);
      assertEqual([ [ "value", [ "one", "two", "three" ] ] ], idx.fields);
      assertEqual(collection.name(), idx.id.substr(0, collection.name().length));
      assertNotEqual(collection.name() + "/0", idx.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get bitarray index
////////////////////////////////////////////////////////////////////////////////

    testGetUndefBitarray: function () {
      collection.ensureUndefBitarray("value", [ "one", "two", "three" ]);
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("bitarray", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx["undefined"]);
      assertEqual([ [ "value", [ "one", "two", "three" ] ] ], idx.fields);
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

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(indexSuite);
jsunity.run(ensureIndexSuite);
jsunity.run(getIndexesSuite);
jsunity.run(getIndexesEdgesSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
