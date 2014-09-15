/*global require, db, assertEqual, assertTrue, ArangoCollection */

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
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ensureIndexSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
