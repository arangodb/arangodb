/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global fail, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the document interface
///
/// @file
///
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
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
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

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn, { waitForSync : false });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
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

    testInsertRemoveMulti : function () {
      var docs = collection.insert([{}, {}, {}]);
      assertEqual(docs.length, 3);
      collection.remove(docs);
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again by key
////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiByKey : function () {
      var docs = collection.insert([{}, {}, {}]);
      assertEqual(docs.length, 3);
      collection.remove(docs.map(function(x) { return x._key; }));
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again by id
////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiById : function () {
      var docs = collection.insert([{}, {}, {}]);
      assertEqual(docs.length, 3);
      collection.remove(docs.map(function(x) { return x._id; }));
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again, many case
////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiMany : function () {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({value:i});
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs);
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again by key, many case
////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiManyByKey : function () {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({value:i});
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs.map(function(x) { return x._key; }));
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again by id, many case
////////////////////////////////////////////////////////////////////////////////

    testInsertRemoveMultiManyById : function () {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({value:i});
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs.map(function(x) { return x._id; }));
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents w. given key and remove them again
////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMulti : function () {
      var docs = collection.insert([{_key:"a"}, {_key:"b"}, {_key:"c"}]);
      assertEqual(docs.length, 3);
      collection.remove(docs);
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again by key
////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiByKey : function () {
      var docs = collection.insert([{_key:"a"}, {_key:"b"}, {_key:"c"}]);
      assertEqual(docs.length, 3);
      collection.remove(docs.map(function(x) { return x._key; }));
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again by id
////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiById : function () {
      var docs = collection.insert([{_key:"a"}, {_key:"b"}, {_key:"c"}]);
      assertEqual(docs.length, 3);
      collection.remove(docs.map(function(x) { return x._id; }));
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again, many case
////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiMany : function () {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({_key:"K"+i, value:i});
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs);
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again by key, many case
////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiManyByKey : function () {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({_key:"K"+i, value:i});
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs.map(function(x) { return x._key; }));
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert multiple documents and remove them again by id, many case
////////////////////////////////////////////////////////////////////////////////

    testInsertWithKeyRemoveMultiManyById : function () {
      var l = [];
      for (var i = 0; i < 10000; i++) {
        l.push({_key:"K"+i, value:i});
      }
      var docs = collection.insert(l);
      assertEqual(docs.length, l.length);
      collection.remove(docs.map(function(x) { return x._id; }));
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert with unique constraint violation
////////////////////////////////////////////////////////////////////////////////

    testInsertErrorUniqueConstraint : function () {
      collection.insert([{_key: "a"}]);
      try {
        collection.insert([{_key: "b"}, {_key: "a"}]);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code,
                    err.errorNum);
      }
      assertEqual(collection.count(), 1);
      try {
        collection.insert([{_key: "a"}, {_key: "b"}]);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code,
                    err.errorNum);
      }
      assertEqual(collection.count(), 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief insert with bad key types
////////////////////////////////////////////////////////////////////////////////

    testInsertErrorBadKey : function () {
      var l = [null, false, true, 1, -1, {}, []];
      l.forEach(function(k) {
        try {
          collection.insert([{_key: "a"},{_key: k}]);
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code,
                      err.errorNum);
        }
      });
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief database methods _replace and _update and _remove cannot do babies:
////////////////////////////////////////////////////////////////////////////////

    testErrorDatabaseMethodsNoBabies : function () {
      collection.insert({_key: "b"});
      try {
        db._replace([cn + "/b"], [{_id: cn + "/b", value: 12}]);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                    err.errorNum);
      }
      try {
        db._update([cn + "/b"], [{_id: cn + "/b", value: 12}]);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                    err.errorNum);
      }
      try {
        db._remove([cn + "/b"]);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
                    err.errorNum);
      }
      assertEqual(collection.count(), 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace multiple documents
////////////////////////////////////////////////////////////////////////////////

    testReplaceMulti : function () {
      var docs = collection.insert([{value:1}, {value:1}, {value:1}]);
      assertEqual(docs.length, 3);
      var docs2 = collection.replace(docs, [{value:2}, {value:2}, {value:2}]);
      var docs3 = collection.toArray();
      assertEqual(docs3.length, 3);
      for (var i = 0; i < docs.length; i++) {
        assertEqual(docs3[i].value, 2);
      }
      try {
        collection.remove(docs);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
      assertEqual(collection.count(), 3);
      collection.remove(docs2);
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update multiple documents
////////////////////////////////////////////////////////////////////////////////

    testUpdateMulti : function () {
      var docs = collection.insert([{value:1}, {value:1}, {value:1}]);
      assertEqual(docs.length, 3);
      var docs2 = collection.update(docs, [{value:2}, {value:2}, {value:2}]);
      var docs3 = collection.toArray();
      assertEqual(docs3.length, 3);
      for (var i = 0; i < docs.length; i++) {
        assertEqual(docs3[i].value, 2);
      }
      try {
        collection.remove(docs);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
      assertEqual(collection.count(), 3);
      collection.remove(docs2);
      assertEqual(collection.count(), 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read multiple documents
////////////////////////////////////////////////////////////////////////////////

    testDocumentMulti : function () {
      var docs = collection.insert([{value:1}, {value:2}, {value:3}]);
      assertEqual(docs.length, 3);
      var keys = docs.map(function(x) { return x._key; });
      var ids = docs.map(function(x) { return x._id; });
      var docs2 = collection.document(docs);
      assertEqual(docs2.length, 3);
      assertEqual(docs2.map(function(x) { return x.value; }), [1,2,3]);
      docs2 = collection.document(keys.reverse());
      assertEqual(docs2.length, 3);
      assertEqual(docs2.map(function(x) { return x.value; }), [3,2,1]);
      docs2 = collection.document(ids.reverse());
      assertEqual(docs2.length, 3);
      assertEqual(docs2.map(function(x) { return x.value; }), [3,2,1]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test revision precondition for replace
////////////////////////////////////////////////////////////////////////////////

    testReplaceMultiPrecondition : function () {
      var docs = collection.insert([{value:1}, {value:2}, {value:3}]);
      var docs2 = collection.replace(docs, [{value:4}, {value:5}, {value:6}]);
      var docs3;
      try {
        docs3 = collection.replace(docs, [{value:7}, {value:8}, {value:9}]);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
      var test = [docs2[0], docs2[1], docs[2]];
      try {
        docs3 = collection.replace(test, [{value:7}, {value:8}, {value:9}]);
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err2.errorNum);
      }
      test = [docs[0], docs2[1], docs2[2]];
      try {
        docs3 = collection.replace(test, [{value:7}, {value:8}, {value:9}]);
        fail();
      }
      catch (err3) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err3.errorNum);
      }
      docs3 = collection.replace(test, [{value:7}, {value:8}, {value:9}],
                                 {overwrite: true});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test revision precondition for update
////////////////////////////////////////////////////////////////////////////////

    testUpdateMultiPrecondition : function () {
      var docs = collection.insert([{value:1}, {value:2}, {value:3}]);
      var docs2 = collection.replace(docs, [{value:4}, {value:5}, {value:6}]);
      var docs3;
      try {
        docs3 = collection.update(docs, [{value:7}, {value:8}, {value:9}]);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
      var test = [docs2[0], docs2[1], docs[2]];
      try {
        docs3 = collection.update(test, [{value:7}, {value:8}, {value:9}]);
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err2.errorNum);
      }
      test = [docs[0], docs2[1], docs2[2]];
      try {
        docs3 = collection.update(test, [{value:7}, {value:8}, {value:9}]);
        fail();
      }
      catch (err3) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err3.errorNum);
      }
      docs3 = collection.update(test, [{value:7}, {value:8}, {value:9}],
                                {overwrite: true});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test revision precondition for remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveMultiPrecondition : function () {
      var docs = collection.insert([{value:1}, {value:2}, {value:3}]);
      var docs2 = collection.replace(docs, [{value:4}, {value:5}, {value:6}]);
      var docs3;
      try {
        docs3 = collection.remove(docs);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
      var test = [docs2[0], docs2[1], docs[2]];
      try {
        docs3 = collection.remove(test);
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err2.errorNum);
      }
      test = [docs[0], docs2[1], docs2[2]];
      try {
        docs3 = collection.remove(test);
        fail();
      }
      catch (err3) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err3.errorNum);
      }
      docs3 = collection.remove(test, {overwrite: true});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test revision precondition for document
////////////////////////////////////////////////////////////////////////////////

    testDocumentMultiPrecondition : function () {
      var docs = collection.insert([{value:1}, {value:2}, {value:3}]);
      var docs2 = collection.replace(docs, [{value:4}, {value:5}, {value:6}]);
      var docs3;
      try {
        docs3 = collection.document(docs);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
      var test = [docs2[0], docs2[1], docs[2]];
      try {
        docs3 = collection.document(test);
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err2.errorNum);
      }
      test = [docs[0], docs2[1], docs2[2]];
      try {
        docs3 = collection.document(test);
        fail();
      }
      catch (err3) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err3.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bad arguments for insert/replace/update/remove for babies
////////////////////////////////////////////////////////////////////////////////

    testBadArguments : function () {
      // Insert
      var values = [null, false, true, 1, "abc", [], [1,2,3]];
      var docs;
      values.forEach(function(x) {
        try {
          docs = collection.insert([x]);
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                      err.errorNum);
        }
      });
      docs = collection.insert([{}, {}, {}]);
      values.forEach(function(x) {
        // Replace
        try {
          docs = collection.replace(docs, [x, x, x]);
          fail();
        }
        catch (err2) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                      err2.errorNum);
        }
        try {
          docs = collection.replace([x, x, x], [{}, {}, {}]);
          fail();
        }
        catch (err3) {
          if (typeof x === "string") {
            assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code,
                        err3.errorNum);
          } else {
            assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
                        err3.errorNum);
          }
        }
        // Update
        try {
          docs = collection.update(docs, [x, x, x]);
          fail();
        }
        catch (err2) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                      err2.errorNum);
        }
        try {
          docs = collection.update([x, x, x], [{}, {}, {}]);
          fail();
        }
        catch (err3) {
          if (typeof x === "string") {
            assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code,
                        err3.errorNum);
          } else {
            assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
                        err3.errorNum);
          }
        }
        // Remove
        try {
          docs = collection.remove([x, x, x]);
          fail();
        }
        catch (err4) {
          if (typeof x === "string") {
            assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code,
                        err4.errorNum);
          } else {
            assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
                        err4.errorNum);
          }
        }
        // Document
        try {
          docs = collection.document([x, x, x]);
          fail();
        }
        catch (err5) {
          if (typeof x === "string") {
            assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code,
                        err5.errorNum);
          } else {
            assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
                        err5.errorNum);
          }
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bad arguments II for insert/replace/update/remove for babies
////////////////////////////////////////////////////////////////////////////////

    testBadArguments2 : function () {
      // Insert
      var docs;
      docs = collection.insert([{}, {}, {}]);
      // Replace
      try {
        docs = collection.replace(docs[0], [{}]);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                    err.errorNum);
      }
      try {
        docs = collection.replace(docs, {});
        fail();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                    err2.errorNum);
      }
      try {
        docs = collection.replace(docs, [{}]);
        fail();
      }
      catch (err3) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                    err3.errorNum);
      }
      // Update
      try {
        docs = collection.update(docs[0], [{}]);
        fail();
      }
      catch (err4) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                    err4.errorNum);
      }
      try {
        docs = collection.update(docs, {});
        fail();
      }
      catch (err5) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                    err5.errorNum);
      }
      try {
        docs = collection.update(docs, [{}]);
        fail();
      }
      catch (err6) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
                    err6.errorNum);
      }
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionDocumentSuiteBabies);

return jsunity.done();
