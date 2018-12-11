/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global fail, assertEqual, assertTypeOf, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2016 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;
var internal = require("internal");
var wait = internal.wait;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: babies for documents
////////////////////////////////////////////////////////////////////////////////

function CollectionDocumentSuiteBabies() {
  'use strict';
  var cn = "UnitTestsCollectionBasics";
  var collection = null;

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function() {
      db._drop(cn);
      collection = db._create(cn, {
        waitForSync: false
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function() {
      if (collection) {
        collection.unload();
        collection.drop();
        collection = null;
      }
      wait(0.0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again
    ////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMulti: function() {
      var docs = collection.insert([{}, {}, {}]);
      assertEqual(docs.length, 3);
      collection.remove(docs);
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again by key
    ////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiByKey: function() {
      var docs = collection.insert([{}, {}, {}]);
      assertEqual(docs.length, 3);
      collection.remove(docs.map(function(x) {
        return x._key;
      }));
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again by id
    ////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiById: function() {
      var docs = collection.insert([{}, {}, {}]);
      assertEqual(docs.length, 3);
      collection.remove(docs.map(function(x) {
        return x._id;
      }));
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again, many case
    ////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiMany: function() {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({
          value: i
        });
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs);
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again by key, many case
    ////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiManyByKey: function() {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({
          value: i
        });
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs.map(function(x) {
        return x._key;
      }));
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again by id, many case
    ////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiManyById: function() {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({
          value: i
        });
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs.map(function(x) {
        return x._id;
      }));
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents w. given key and remove them again
    ////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMulti: function() {
      var docs = collection.insert([{
        _key: "a"
      }, {
        _key: "b"
      }, {
        _key: "c"
      }]);
      assertEqual(docs.length, 3);
      collection.remove(docs);
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again by key
    ////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiByKey: function() {
      var docs = collection.insert([{
        _key: "a"
      }, {
        _key: "b"
      }, {
        _key: "c"
      }]);
      assertEqual(docs.length, 3);
      collection.remove(docs.map(function(x) {
        return x._key;
      }));
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again by id
    ////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiById: function() {
      var docs = collection.insert([{
        _key: "a"
      }, {
        _key: "b"
      }, {
        _key: "c"
      }]);
      assertEqual(docs.length, 3);
      collection.remove(docs.map(function(x) {
        return x._id;
      }));
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again, many case
    ////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiMany: function() {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({
          _key: "K" + i,
          value: i
        });
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs);
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again by key, many case
    ////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiManyByKey: function() {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({
          _key: "K" + i,
          value: i
        });
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs.map(function(x) {
        return x._key;
      }));
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert multiple documents and remove them again by id, many case
    ////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiManyById: function() {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({
          _key: "K" + i,
          value: i
        });
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs.map(function(x) {
        return x._id;
      }));
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert with unique constraint violation
    ////////////////////////////////////////////////////////////////////////////////

    testInsertErrorUniqueConstraint: function() {
      collection.insert([{
        _key: "a"
      }]);
      var docs =  collection.insert([{
        _key: "b"
      }, {
        _key: "a"
      }]);
      assertEqual(docs.length, 2);
      assertEqual(docs[0]._key, "b"); // The first is inserted
      assertTrue(docs[1].error);
      assertEqual(docs[1].errorNum, ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
      assertEqual(collection.count(), 2);

      docs = collection.insert([{
        _key: "a"
      }, {
        _key: "c"
      }]);
      assertEqual(docs.length, 2);
      assertTrue(docs[0].error);
      assertEqual(docs[0].errorNum, ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
      assertEqual(docs[1]._key, "c"); // The second is inserted
      assertEqual(collection.count(), 3);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief insert with bad key types
    ////////////////////////////////////////////////////////////////////////////////

    testInsertErrorBadKey: function() {
      var l = [null, false, true, 1, -1, {},
        []
      ];
      l.forEach(function(k) {
        var docs = collection.insert([{
          _key: "a"
        }, {
          _key: k
        }]);
        assertEqual(docs.length, 2);
        assertEqual(docs[0]._key, "a");
        assertTrue(docs[1].error);
        assertEqual(docs[1].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
        collection.remove("a");
      });
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief database methods _replace and _update and _remove cannot do babies:
    ////////////////////////////////////////////////////////////////////////////////

    testErrorDatabaseMethodsNoBabies: function() {
      collection.insert({
        _key: "b"
      });
      try {
        db._replace([cn + "/b"], [{
          _id: cn + "/b",
          value: 12
        }]);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
          err.errorNum);
      }
      try {
        db._update([cn + "/b"], [{
          _id: cn + "/b",
          value: 12
        }]);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
          err.errorNum);
      }
      try {
        db._remove([cn + "/b"]);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
          err.errorNum);
      }
      assertEqual(collection.count(), 1);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief replace multiple documents
    ////////////////////////////////////////////////////////////////////////////////

    testReplaceMulti: function() {
      var docs = collection.insert([{
        value: 1
      }, {
        value: 1
      }, {
        value: 1
      }]);
      assertEqual(docs.length, 3);
      var docs2 = collection.replace(docs, [{
        value: 2
      }, {
        value: 2
      }, {
        value: 2
      }]);
      var docs3 = collection.toArray();
      assertEqual(docs3.length, 3);
      for (var i = 0; i < docs.length; i++) {
        assertEqual(docs3[i].value, 2);
      }
      var result = collection.remove(docs);
      assertEqual(result.length, 3);
      // All should conflict!
      for (i = 0; i < result.length; i++) {
        assertTrue(result[i].error);
        assertEqual(result[i].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);
      }
      assertEqual(collection.count(), 3);
      collection.remove(docs2);
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief update multiple documents
    ////////////////////////////////////////////////////////////////////////////

    testUpdateMulti: function() {
      var docs = collection.insert([{
        value: 1
      }, {
        value: 1
      }, {
        value: 1
      }]);
      assertEqual(docs.length, 3);
      var docs2 = collection.update(docs, [{
        value: 2
      }, {
        value: 2
      }, {
        value: 2
      }]);
      var docs3 = collection.toArray();
      assertEqual(docs3.length, 3);
      for (var i = 0; i < docs.length; i++) {
        assertEqual(docs3[i].value, 2);
      }
      var result = collection.remove(docs);
      assertEqual(result.length, 3);
      // All should conflict!
      for (i = 0; i < result.length; i++) {
        assertTrue(result[i].error);
        assertEqual(result[i].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);
      }
      assertEqual(collection.count(), 3);
      collection.remove(docs2);
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief read multiple documents
    ////////////////////////////////////////////////////////////////////////////////

    testDocumentMulti: function() {
      var docs = collection.insert([{
        value: 1
      }, {
        value: 2
      }, {
        value: 3
      }]);
      assertEqual(docs.length, 3);
      var keys = docs.map(function(x) {
        return x._key;
      });
      var ids = docs.map(function(x) {
        return x._id;
      });
      var docs2 = collection.document(docs);
      assertEqual(docs2.length, 3);
      assertEqual(docs2.map(function(x) {
        return x.value;
      }), [1, 2, 3]);
      docs2 = collection.document(keys.reverse());
      assertEqual(docs2.length, 3);
      assertEqual(docs2.map(function(x) {
        return x.value;
      }), [3, 2, 1]);
      docs2 = collection.document(ids.reverse());
      assertEqual(docs2.length, 3);
      assertEqual(docs2.map(function(x) {
        return x.value;
      }), [3, 2, 1]);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test revision precondition for replace
    ////////////////////////////////////////////////////////////////////////////////

    testReplaceMultiPrecondition: function() {
      var docs = collection.insert([{
        value: 1
      }, {
        value: 2
      }, {
        value: 3
      }]);
      var docs2 = collection.replace(docs, [{
        value: 4
      }, {
        value: 5
      }, {
        value: 6
      }]);
      var docs3 = collection.replace(docs, [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }]);
      assertEqual(docs3.length, docs.length);
      for (var i = 0; i < docs3.length; ++i) {
        assertEqual(docs3[i].error, true);
        assertEqual(docs3[i].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);
      }

      // Only the third is not ok
      var test = [docs2[0], docs2[1], docs[2]];
      docs3 = collection.replace(test, [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }]);
      assertEqual(docs3.length, test.length);

      assertEqual(docs3[0]._key, docs[0]._key);
      assertEqual(docs3[1]._key, docs[1]._key);

      assertEqual(docs3[2].error, true);
      assertEqual(docs3[2].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);

      // Only the third is ok
      test = [docs[0], docs2[1], docs2[2]];

      docs3 = collection.replace(test, [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }]);

      assertEqual(docs3[0].error, true);
      assertEqual(docs3[0].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);
      assertEqual(docs3[1].error, true);
      assertEqual(docs3[1].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);

      assertEqual(docs3[2]._key, docs[2]._key);

      docs3 = collection.replace(test, [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }], {
        overwrite: true
      });
      assertEqual(docs3[0]._key, docs[0]._key);
      assertEqual(docs3[1]._key, docs[1]._key);
      assertEqual(docs3[2]._key, docs[2]._key);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test revision precondition for update
    ////////////////////////////////////////////////////////////////////////////////

    testUpdateMultiPrecondition: function() {
      var docs = collection.insert([{
        value: 1
      }, {
        value: 2
      }, {
        value: 3
      }]);
      var docs2 = collection.replace(docs, [{
        value: 4
      }, {
        value: 5
      }, {
        value: 6
      }]);
      var docs3;
      docs3 = collection.update(docs, [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }]);
      assertEqual(docs3.length, 3);
      for (var i = 0; i < docs3.length; ++i) {
        assertEqual(docs3[i].error, true);
        assertEqual(docs3[i].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);
      }

      // Only the third is not ok
      var test = [docs2[0], docs2[1], docs[2]];
      docs3 = collection.update(test, [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }]);
      assertEqual(docs3.length, test.length);

      assertEqual(docs3[0]._key, docs[0]._key);
      assertEqual(docs3[1]._key, docs[1]._key);

      assertEqual(docs3[2].error, true);
      assertEqual(docs3[2].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);

      // Only the third is ok
      test = [docs[0], docs2[1], docs2[2]];
      docs3 = collection.update(test, [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }]);

      assertEqual(docs3[0].error, true);
      assertEqual(docs3[0].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);
      assertEqual(docs3[1].error, true);
      assertEqual(docs3[1].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);

      assertEqual(docs3[2]._key, docs[2]._key);

      docs3 = collection.update(test, [{
        value: 7
      }, {
        value: 8
      }, {
        value: 9
      }], {
        overwrite: true
      });

      assertEqual(docs3[0]._key, docs[0]._key);
      assertEqual(docs3[1]._key, docs[1]._key);
      assertEqual(docs3[2]._key, docs[2]._key);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test revision precondition for remove
    ////////////////////////////////////////////////////////////////////////////////

    testRemoveMultiPrecondition: function() {
      var docs = collection.insert([{
        value: 1
      }, {
        value: 2
      }, {
        value: 3
      }]);
      var docs2 = collection.replace(docs, [{
        value: 4
      }, {
        value: 5
      }, {
        value: 6
      }]);
      var docs3 = collection.remove(docs);
      assertEqual(docs3.length, 3);
      // All should conflict!
      for (var i = 0; i < docs.length; i++) {
        assertTrue(docs3[i].error);
        assertEqual(docs3[i].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);
      }

      var test = [docs2[0], docs2[1], docs[2]];
      docs3 = collection.remove(test);
      assertEqual(docs3.length, 3);
      // The first 2 are successful
      for (i = 0; i < 2; i++) {
        assertFalse(docs3[i].hasOwnProperty("error"));
        assertFalse(docs3[i].hasOwnProperty("errorNum"));
      }
      // The third conflicts
      assertTrue(docs3[2].error);
      assertEqual(docs3[2].errorNum, ERRORS.ERROR_ARANGO_CONFLICT.code);
      assertEqual(collection.count(), 1);

      test = [docs[0], docs2[1], docs2[2]];
      docs3 = collection.remove(test);
      assertEqual(docs3.length, 3);

      // The first two do not exist
      assertTrue(docs3[0].error);
      assertEqual(docs3[0].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
      assertTrue(docs3[1].error);
      assertEqual(docs3[1].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);

      // The third is removed
      assertFalse(docs3[2].hasOwnProperty("error"));
      assertFalse(docs3[2].hasOwnProperty("errorNum"));
      assertEqual(collection.count(), 0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test revision precondition for document
    ////////////////////////////////////////////////////////////////////////////////

    testDocumentMultiPrecondition: function() {
      var docs = collection.insert([{
        value: 1
      }, {
        value: 2
      }, {
        value: 3
      }]);
      var docs2 = collection.replace(docs, [{
        value: 4
      }, {
        value: 5
      }, {
        value: 6
      }]);
      var docs3 = collection.document(docs);
      assertEqual(docs3.length, 3);
      for (var i = 0; i < docs3.length; ++i) {
        assertTrue(docs3[i].error);
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, docs3[i].errorNum);
      }
      // Only the third fails
      var test = [docs2[0], docs2[1], docs[2]];
      docs3 = collection.document(test);
      assertEqual(docs3.length, 3);
      assertEqual(docs3[0].value, 4);
      assertEqual(docs3[1].value, 5);
      assertTrue(docs3[2].error);
      assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, docs3[2].errorNum);

      // Only the first fails
      test = [docs[0], docs2[1], docs2[2]];
      docs3 = collection.document(test);
      assertEqual(docs3.length, 3);
      assertTrue(docs3[0].error);
      assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, docs3[0].errorNum);
      assertEqual(docs3[1].value, 5);
      assertEqual(docs3[2].value, 6);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test bad arguments for insert/replace/update/remove for babies
    ////////////////////////////////////////////////////////////////////////////////

    testBadArguments: function() {
      // Insert
      var values = [null, false, true, 1, "abc", [],
        [1, 2, 3]
      ];
      var docs;
      values.forEach(function(x) {
        docs = collection.insert([x]);
        assertEqual(docs.length, 1);
        assertEqual(docs[0].error, true);
        assertEqual(docs[0].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
      });
      var origDocs = collection.insert([{}, {}, {}]);
      var expectedLength = origDocs.length;
      values.forEach(function(x) {
        // Replace
        docs = collection.replace(origDocs, [x, x, x]);
        assertEqual(docs.length, expectedLength);
        for (var i = 0; i < expectedLength; ++i) {
          assertEqual(docs[i].error, true);
          assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
        }

        // Replace
        if (typeof x !== "string") {
          try {
            docs = collection.replace([x, x, x], [{}, {}, {}]);
            assertEqual(docs.length, expectedLength);
            for (i = 0; i < expectedLength; ++i) {
              assertEqual(docs[i].error, true);
              if (typeof x === "string") {
                assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
              } else {
                assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code);
              }
            }
          } catch (err) {
            // In cluster case the coordinator will directly figure out that no
            // keys are given
            assertEqual(err.errorNum, ERRORS.ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN.code);
          }
        }

        // Update
        // Version 1
        docs = collection.update(origDocs, [x, x, x]);
        assertEqual(docs.length, expectedLength);
        for (i = 0; i < expectedLength; ++i) {
          assertEqual(docs[i].error, true);
          assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
        }

        docs = collection.update([x, x, x], [{}, {}, {}]);
        assertEqual(docs.length, expectedLength);
        for (i = 0; i < expectedLength; ++i) {
          assertEqual(docs[i].error, true);
          if (typeof x === "string") {
            assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
          } else {
            assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code);
          }
        }

        // Remove
        // Version 1

        docs = collection.remove([x, x, x]);
        assertEqual(docs.length, expectedLength);
        for (i = 0; i < expectedLength; ++i) {
          assertEqual(docs[i].error, true);
          if (typeof x === "string") {
            assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
          } else {
            assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code);
          }
        }

        // Document
        docs = collection.document([x, x, x]);
        assertEqual(docs.length, expectedLength);
        for (i = 0; i < expectedLength; ++i) {
          assertEqual(docs[i].error, true);
          if (typeof x === "string") {
            assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
          } else {
            assertEqual(docs[i].errorNum, ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code);
          }
        }
      });
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test bad arguments II for insert/replace/update/remove for babies
    ////////////////////////////////////////////////////////////////////////////////

    testBadArguments2: function() {
      // Insert
      var docs;
      docs = collection.insert([{}, {}, {}]);
      // Replace
      try {
        docs = collection.replace(docs[0], [{}]);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
          err.errorNum);
      }
      try {
        docs = collection.replace(docs, {});
        fail();
      } catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
          err2.errorNum);
      }
      try {
        docs = collection.replace(docs, [{}]);
        fail();
      } catch (err3) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
          err3.errorNum);
      }
      // Update
      try {
        docs = collection.update(docs[0], [{}]);
        fail();
      } catch (err4) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
          err4.errorNum);
      }
      try {
        docs = collection.update(docs, {});
        fail();
      } catch (err5) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
          err5.errorNum);
      }
      try {
        docs = collection.update(docs, [{}]);
        fail();
      } catch (err6) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
          err6.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test insert/replace/update/remove with empty lists
    ////////////////////////////////////////////////////////////////////////////////

    testEmptyBabiesList: function() {
      // Insert
      let result = collection.insert([]);
      assertTrue(Array.isArray(result));
      assertEqual(result.length, 0);

      // Update
      result = collection.update([], []);
      assertTrue(Array.isArray(result));
      assertEqual(result.length, 0);

      // Replace
      result = collection.replace([], []);
      assertTrue(Array.isArray(result));
      assertEqual(result.length, 0);

      // Remove
      result = collection.remove([]);
      assertTrue(Array.isArray(result));
      assertEqual(result.length, 0);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: returnNew and returnOld options
////////////////////////////////////////////////////////////////////////////////

function CollectionDocumentSuiteReturnStuff() {
  'use strict';
  var cn = "UnitTestsCollectionBasics";
  var collection = null;

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function() {
      db._drop(cn);
      collection = db._create(cn, {
        waitForSync: false
      });

      collection.load();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function() {
      collection.drop();
      wait(0.0);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief create with and without returnNew
    ////////////////////////////////////////////////////////////////////////////////

    testCreateMultiReturnNew: function() {
      var res = collection.insert([{
        "Hallo": 12
      }]);
      assertTypeOf("object", res);
      assertTrue(Array.isArray(res));
      assertEqual(res.length, 1);
      assertTypeOf("object", res[0]);
      assertEqual(3, Object.keys(res[0]).length);
      assertTypeOf("string", res[0]._id);
      assertTypeOf("string", res[0]._key);
      assertTypeOf("string", res[0]._rev);

      // Now with returnNew: true
      res = collection.insert([{
        "Hallo": 12
      }], {
        returnNew: true
      });
      assertTypeOf("object", res);
      assertTrue(Array.isArray(res));
      assertTypeOf("object", res[0]);
      assertEqual(4, Object.keys(res[0]).length);
      assertTypeOf("string", res[0]._id);
      assertTypeOf("string", res[0]._key);
      assertTypeOf("string", res[0]._rev);
      assertTypeOf("object", res[0]["new"]);
      assertEqual(12, res[0]["new"].Hallo);
      assertEqual(4, Object.keys(res[0]["new"]).length);

      // Now with returnNew: false
      res = collection.insert([{
        "Hallo": 12
      }], {
        returnNew: false
      });
      assertTypeOf("object", res);
      assertTrue(Array.isArray(res));
      assertTypeOf("object", res[0]);
      assertEqual(3, Object.keys(res[0]).length);
      assertTypeOf("string", res[0]._id);
      assertTypeOf("string", res[0]._key);
      assertTypeOf("string", res[0]._rev);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief remove with and without returnOld
    ////////////////////////////////////////////////////////////////////////////////

    testRemoveMultiReturnOld: function() {
      var res = collection.insert([{
        "Hallo": 12
      }]);
      var res2 = collection.remove([res[0]._key]);

      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(3, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);

      // Now with returnOld: true
      res = collection.insert([{
        "Hallo": 12
      }]);
      res2 = collection.remove([res[0]._key], {
        returnOld: true
      });
      assertTypeOf("object", res2[0]);
      assertEqual(4, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
      assertTypeOf("object", res2[0].old);
      assertEqual(12, res2[0].old.Hallo);
      assertEqual(4, Object.keys(res2[0].old).length);

      // Now with returnOld: false
      res = collection.insert([{
        "Hallo": 12
      }]);
      res2 = collection.remove([res[0]._key], {
        returnOld: false
      });
      assertTypeOf("object", res2[0]);
      assertEqual(3, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief replace with and without returnOld and returnNew
    ////////////////////////////////////////////////////////////////////////////////

    testReplaceMultiReturnOldNew: function() {
      var res = collection.insert({
        "Hallo": 12
      });
      var res2 = collection.replace([res._key], [{
        "Hallo": 13
      }]);

      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(4, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);

      // Now with returnOld: true
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.replace([res._key], [{
        "Hallo": 13
      }], {
        returnOld: true
      });
      assertTrue(Array.isArray(res2));
      assertEqual(1, res2.length);
      assertTypeOf("object", res2[0]);
      assertEqual(5, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
      assertTypeOf("object", res2[0].old);
      assertEqual(12, res2[0].old.Hallo);
      assertEqual(4, Object.keys(res2[0].old).length);

      // Now with returnOld: false
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.replace([res._key], [{
        "Hallo": 14
      }], {
        returnOld: false
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(4, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);

      // Now with returnNew: true
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.replace([res._key], [{
        "Hallo": 14
      }], {
        returnNew: true
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(5, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
      assertTypeOf("object", res2[0]["new"]);
      assertEqual(14, res2[0]["new"].Hallo);
      assertEqual(4, Object.keys(res2[0]["new"]).length);

      // Now with returnNew: false
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.replace([res._key], [{
        "Hallo": 15
      }], {
        returnNew: false
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(4, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);

      // Now with returnNew: true and returnOld:true
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.replace([res._key], [{
        "Hallo": 16
      }], {
        returnNew: true,
        returnOld: true
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(6, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
      assertTypeOf("object", res2[0].old);
      assertTypeOf("object", res2[0]["new"]);
      assertEqual(16, res2[0]["new"].Hallo);
      assertEqual(12, res2[0].old.Hallo);
      assertEqual(4, Object.keys(res2[0]["new"]).length);
      assertEqual(4, Object.keys(res2[0].old).length);

      // Now with returnOld: false and returnNew: false
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.replace([res._key], [{
        "Hallo": 15
      }], {
        returnNew: false,
        returnOld: false
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(4, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief update with and without returnOld and returnNew
    ////////////////////////////////////////////////////////////////////////////////

    testUpdateMultiReturnOldNew: function() {
      var res = collection.insert({
        "Hallo": 12
      });
      var res2 = collection.update([res._key], [{
        "Hallo": 13
      }]);

      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(4, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);

      // Now with returnOld: true
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.update([res._key], [{
        "Hallo": 13
      }], {
        returnOld: true
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(5, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
      assertTypeOf("object", res2[0].old);
      assertEqual(12, res2[0].old.Hallo);
      assertEqual(4, Object.keys(res2[0].old).length);

      // Now with returnOld: false
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.update([res._key], [{
        "Hallo": 14
      }], {
        returnOld: false
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(4, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);

      // Now with returnNew: true
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.update([res._key], [{
        "Hallo": 14
      }], {
        returnNew: true
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(5, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
      assertTypeOf("object", res2[0]["new"]);
      assertEqual(14, res2[0]["new"].Hallo);
      assertEqual(4, Object.keys(res2[0]["new"]).length);

      // Now with returnOld: false
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.update([res._key], [{
        "Hallo": 15
      }], {
        returnNew: false
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(4, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);

      // Now with returnNew: true and returnOld:true
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.update([res._key], [{
        "Hallo": 16
      }], {
        returnNew: true,
        returnOld: true
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(6, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
      assertTypeOf("object", res2[0].old);
      assertTypeOf("object", res2[0]["new"]);
      assertEqual(16, res2[0]["new"].Hallo);
      assertEqual(12, res2[0].old.Hallo);
      assertEqual(4, Object.keys(res2[0]["new"]).length);
      assertEqual(4, Object.keys(res2[0].old).length);

      // Now with returnOld: false and returnNew: false
      res = collection.insert({
        "Hallo": 12
      });
      res2 = collection.update([res._key], [{
        "Hallo": 15
      }], {
        returnNew: false,
        returnOld: false
      });
      assertTypeOf("object", res2);
      assertTrue(Array.isArray(res2));
      assertTypeOf("object", res2[0]);
      assertEqual(4, Object.keys(res2[0]).length);
      assertTypeOf("string", res2[0]._id);
      assertTypeOf("string", res2[0]._key);
      assertTypeOf("string", res2[0]._rev);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionDocumentSuiteBabies);
jsunity.run(CollectionDocumentSuiteReturnStuff);

return jsunity.done();
