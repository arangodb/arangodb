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
var console = require("console");
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
      assertEqual(true, res);

      res = collection.dropIndex(id.id);
      assertEqual(false, res);

      id = collection.ensureGeoIndex("a");
      res = collection.dropIndex(id);
      assertEqual(true, res);

      res = collection.dropIndex(id);
      assertEqual(false, res);

      id = collection.ensureGeoIndex("a");
      res = internal.db._dropIndex(id);
      assertEqual(true, res);

      res = internal.db._dropIndex(id);
      assertEqual(false, res);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief drop index by id string
////////////////////////////////////////////////////////////////////////////////

    testDropIndexString : function () {
      // pick up the numeric part (starts after the slash)
      var id = collection.ensureGeoIndex("a").id.substr(cn.length + 1);
      var res = collection.dropIndex(collection.name() + "/" + id);
      assertEqual(true, res);

      res = collection.dropIndex(collection.name() + "/" + id);
      assertEqual(false, res);

      id = collection.ensureGeoIndex("a").id.substr(cn.length + 1);
      res = collection.dropIndex(parseInt(id, 10));
      assertEqual(true, res);

      res = collection.dropIndex(parseInt(id, 10));
      assertEqual(false, res);

      id = collection.ensureGeoIndex("a").id.substr(cn.length + 1);
      res = internal.db._dropIndex(collection.name() + "/" + id);
      assertEqual(true, res);

      res = internal.db._dropIndex(collection.name() + "/" + id);
      assertEqual(false, res);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief access a non-existing index
////////////////////////////////////////////////////////////////////////////////

    testGetNonExistingIndexes : function () {
      tests = [ "2444334000000", "9999999999999" ].forEach(function (id) {
        try {
          var idx = collection.index(id);
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
      assertEqual(true, idx.unique);
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
      assertEqual(true, idx.unique);
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
      assertEqual(true, idx.unique);
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
      assertEqual(false, idx.unique);
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
      assertEqual(false, idx.unique);
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
      assertEqual(true, idx.unique);
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
      assertEqual(true, idx.unique);
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
      assertEqual(false, idx.unique);
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
      assertEqual(false, idx.unique);
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
      assertEqual(false, idx.unique);
      assertEqual(false, idx["undefined"]);
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
      assertEqual(false, idx.unique);
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
      assertEqual(true, idx.unique);
      assertEqual([ "_id" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get edge
////////////////////////////////////////////////////////////////////////////////

    testGetEdge : function () {
      var res = collection.getIndexes();

      assertEqual(2, res.length);
      var idx = res[1];

      assertEqual("edge", idx.type);
      assertEqual(false, idx.unique);
      assertEqual([ "_from", "_to" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique hash index
////////////////////////////////////////////////////////////////////////////////

    testGetHashUnique1 : function () {
      collection.ensureUniqueConstraint("value");
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual([ "value" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: get unique skiplist index
////////////////////////////////////////////////////////////////////////////////

    testGetSkiplistUnique1 : function () {
      collection.ensureUniqueSkiplist("value");
      var res = collection.getIndexes();

      assertEqual(3, res.length);
      var idx = res[2];

      assertEqual("skiplist", idx.type);
      assertEqual(true, idx.unique);
      assertEqual([ "value" ], idx.fields);
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
      assertEqual(false, idx.unique);
      assertEqual(false, idx["undefined"]);
      assertEqual([ [ "value", [ "one", "two", "three" ] ] ], idx.fields);
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
      assertEqual(false, idx.unique);
      assertEqual([ "value" ], idx.fields);
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
jsunity.run(getIndexesSuite);
jsunity.run(getIndexesEdgesSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
