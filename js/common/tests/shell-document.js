////////////////////////////////////////////////////////////////////////////////
/// @brief test the document interface
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var console = require("console");

var arangodb = require("org/arangodb");

var ERRORS = arangodb.errors;
var db = arangodb.db;
var wait = require("internal").wait;

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function CollectionDocumentSuiteErrorHandling () {
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
      collection.unload();
      collection.drop();
      collection = null;
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleDatabase : function () {
      try {
        db._document("123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleCollection : function () {
      try {
        collection.document("");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle replace
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleReplace : function () {
      try {
        db._replace("123456  ", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle delete
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleDelete : function () {
      try {
        collection.remove("123/45/6");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingUnknownDocument: function () {
      try {
        collection.document(collection.name() + "/123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cross collection
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingCrossCollection : function () {
      try {
        collection.document("test123456/123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cross collection replace
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingCrossCollectionReplace : function () {
      try {
        collection.replace("test123456/123456", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief cross collection delete
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingCrossCollectionDelete : function () {
      try {
        collection.remove("test123456/123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code, err.errorNum);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function CollectionDocumentSuite () {
  var ERRORS = require("internal").errors;

  var cn = "UnitTestsCollectionBasics";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn, { waitForSync: false });

      collection.load();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      collection = null;
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ invalid type 
////////////////////////////////////////////////////////////////////////////////

    testSaveInvalidDocumentType : function () {
      [ 1, 2, 3, false, true, null, [ ] ].forEach(function (doc) {
        try {
          collection.save(doc);
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document w/ invalid type 
////////////////////////////////////////////////////////////////////////////////

    testUpdateInvalidDocumentType : function () {
      var d = collection.save({ _key: "test" });

      [ 1, 2, 3, false, true, null, [ ] ].forEach(function (doc) {
        try {
          collection.update(d, doc);
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document w/ invalid type 
////////////////////////////////////////////////////////////////////////////////

    testReplaceInvalidDocumentType : function () {
      var d = collection.save({ _key: "test" });

      [ 1, 2, 3, false, true, null, [ ] ].forEach(function (doc) {
        try {
          collection.replace(d, doc);
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ invalid primary key data types
////////////////////////////////////////////////////////////////////////////////

    testSaveInvalidDocumentKeyType : function () {
      [ 1, 2, 3, false, true, null, [ ], { } ].forEach(function (key) {
        try {
          collection.save({ _key: key });
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ primary key violation
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicate : function () {
      var d1, d2, doc;

      try {
        d1 = collection.save({ _key: "test", value: 1 });
        d2 = collection.save({ _key: "test", value: 2 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
      
      assertTypeOf("string", d1._id);
      assertTypeOf("string", d1._key);
      assertTypeOf("string", d1._rev);
      assertEqual("UnitTestsCollectionBasics/test", d1._id);
      assertEqual("test", d1._key);

      doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(1, doc.value);

      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ secondary key violation
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicateSecondary : function () {
      var d1, d2, doc;

      collection.ensureUniqueConstraint("value1");

      try {
        d1 = collection.save({ _key: "test1", value1: 1, value2: 1 });
        d2 = collection.save({ _key: "test2", value1: 1, value2: 2 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
      
      assertTypeOf("string", d1._id);
      assertTypeOf("string", d1._key);
      assertTypeOf("string", d1._rev);
      assertEqual("UnitTestsCollectionBasics/test1", d1._id);
      assertEqual("test1", d1._key);

      doc = collection.document("test1");
      assertEqual("UnitTestsCollectionBasics/test1", doc._id);
      assertEqual("test1", doc._key);
      assertEqual(1, doc.value1);
      assertEqual(1, doc.value2);

      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ primary key violation, then unload/reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicateUnloadReload : function () {
      var d1, d2, doc;

      try {
        d1 = collection.save({ _key: "test", value: 1 });
        d2 = collection.save({ _key: "test", value: 2 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
      
      d1 = null;
      d2 = null;

      collection.unload();
      while (collection.status() != arangodb.ArangoCollection.STATUS_UNLOADED) { 
        wait(1);
      }
      assertEqual(arangodb.ArangoCollection.STATUS_UNLOADED, collection.status());

      collection.load();

      doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(1, doc.value);
      
      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document w/ secondary key violation, then unload/reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicateSecondaryUnloadReload : function () {
      var d1, d2, doc;
      
      collection.ensureUniqueConstraint("value1");

      try {
        d1 = collection.save({ _key: "test1", value1: 1, value2: 1 });
        d2 = collection.save({ _key: "test1", value1: 1, value2: 2 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      d1 = null;
      d2 = null;

      collection.unload();
      while (collection.status() != arangodb.ArangoCollection.STATUS_UNLOADED) { 
        wait(1);
      }
      assertEqual(arangodb.ArangoCollection.STATUS_UNLOADED, collection.status());

      collection.load();

      doc = collection.document("test1");
      assertEqual("UnitTestsCollectionBasics/test1", doc._id);
      assertEqual("test1", doc._key);
      assertEqual(1, doc.value1);
      assertEqual(1, doc.value2);
      
      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document insert/delete w/ reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicates : function () {
      var d, i;

      for (i = 0; i < 10; ++i) {
        d = collection.save({ _key: "test", value: i }); 
        collection.remove(d);
      }
      d = null;

      assertEqual(0, collection.count());

      collection.unload();
      while (collection.status() != arangodb.ArangoCollection.STATUS_UNLOADED) { 
        wait(1);
      }
      assertEqual(arangodb.ArangoCollection.STATUS_UNLOADED, collection.status());

      collection.load();

      assertEqual(0, collection.count());
        
      var d = collection.save({ _key: "test", value: 200 }); 
      assertTypeOf("string", d._id);
      assertTypeOf("string", d._key);
      assertTypeOf("string", d._rev);
      assertEqual("UnitTestsCollectionBasics/test", d._id);
      assertEqual("test", d._key);

      var doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(200, doc.value);

      assertEqual(1, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document insert/delete w/ reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicatesSurvive : function () {
      var i;

      for (i = 0; i < 10; ++i) {
        var d = collection.save({ _key: "test", value: i }); 
        collection.remove(d);
      }
      collection.save({ _key: "test", value: 99 });

      assertEqual(1, collection.count());

      collection.unload();
      while (collection.status() != arangodb.ArangoCollection.STATUS_UNLOADED) { 
        wait(1);
      }
      assertEqual(arangodb.ArangoCollection.STATUS_UNLOADED, collection.status());

      collection.load();

      assertEqual(1, collection.count());
        
      var doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(99, doc.value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document insert/delete w/ reload
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentDuplicatesViolationSurvive : function () {
      var i;
        
      try {
        collection.remove("test");
        fail("whoops");
      }
      catch (e1) {
      }

      for (i = 0; i < 10; ++i) {
        try {
          collection.save({ _key: "test", value: i }); 
        }
        catch (e2) {
        }
      }

      assertEqual(1, collection.count());
      var doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(0, doc.value);
      doc = null;

      collection.unload();
      while (collection.status() != arangodb.ArangoCollection.STATUS_UNLOADED) { 
        wait(1);
      }
      assertEqual(arangodb.ArangoCollection.STATUS_UNLOADED, collection.status());

      collection.load();

      assertEqual(1, collection.count());
        
      var doc = collection.document("test");
      assertEqual("UnitTestsCollectionBasics/test", doc._id);
      assertEqual("test", doc._key);
      assertEqual(0, doc.value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document
////////////////////////////////////////////////////////////////////////////////

    testSaveDocument : function () {
      var doc = collection.save({ "Hello" : "World" });

      assertTypeOf("string", doc._id);
      assertTypeOf("string", doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentSyncFalse : function () {
      var doc = collection.save({ "Hello" : "World" }, false);

      assertTypeOf("string", doc._id);
      assertTypeOf("string", doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testSaveDocumentSyncTrue : function () {
      var doc = collection.save({ "Hello" : "World" }, true);

      assertTypeOf("string", doc._id);
      assertTypeOf("string", doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

    testReadDocument : function () {
      var d = collection.save({ "Hello" : "World" });

      var doc = collection.document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      doc = collection.document(d);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document with conflict
////////////////////////////////////////////////////////////////////////////////

    testReadDocumentConflict : function () {
      var d = collection.save({ "Hello" : "World" });

      var doc = collection.document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      var r = collection.replace(d, { "Hello" : "You" });

      doc = collection.document(r);

      assertEqual(d._id, doc._id);
      assertNotEqual(d._rev, doc._rev);

      try {
        collection.document(d);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        collection.replace(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = collection.document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = collection.replace(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = collection.document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 }, true, false);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 }, true, true);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.update(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        collection.update(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = collection.document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = collection.update(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = collection.document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
      
      var a5 = collection.update(a4, { b : 1, c : 2, d : "foo", e : null });
      assertEqual(a1._id, a5._id);
      assertNotEqual(a4._rev, a5._rev);
      
      var doc5 = collection.document(a1._id);
      assertEqual(a1._id, doc5._id);
      assertEqual(a5._rev, doc5._rev);
      assertEqual(4, doc5.a);
      assertEqual(1, doc5.b);
      assertEqual(2, doc5.c);
      assertEqual("foo", doc5.d);
      assertEqual(null, doc5.e);

      var a6 = collection.update(a5, { f : null, b : null, a : null, g : 2, c : 4 });
      assertEqual(a1._id, a6._id);
      assertNotEqual(a5._rev, a6._rev);
      
      var doc6 = collection.document(a1._id);
      assertEqual(a1._id, doc6._id);
      assertEqual(a6._rev, doc6._rev);
      assertEqual(null, doc6.a);
      assertEqual(null, doc6.b);
      assertEqual(4, doc6.c);
      assertEqual("foo", doc6.d);
      assertEqual(null, doc6.e);
      assertEqual(null, doc6.f);
      assertEqual(2, doc6.g);
      
      var a7 = collection.update(a6, { a : null, b : null, c : null, g : null }, true, false);
      assertEqual(a1._id, a7._id);
      assertNotEqual(a6._rev, a7._rev);
      
      var doc7 = collection.document(a1._id);
      assertEqual(a1._id, doc7._id);
      assertEqual(a7._rev, doc7._rev);
      assertEqual(undefined, doc7.a);
      assertEqual(undefined, doc7.b);
      assertEqual(undefined, doc7.c);
      assertEqual("foo", doc7.d);
      assertEqual(null, doc7.e);
      assertEqual(null, doc7.f);
      assertEqual(undefined, doc7.g);
      
      var a8 = collection.update(a7, { d : { "one" : 1, "two" : 2, "three" : 3 }, e : { }, f : { "one" : 1 }} );
      assertEqual(a1._id, a8._id);
      assertNotEqual(a7._rev, a8._rev);
      
      var doc8 = collection.document(a1._id);
      assertEqual(a1._id, doc8._id);
      assertEqual(a8._rev, doc8._rev);
      assertEqual({"one": 1, "two": 2, "three": 3}, doc8.d);
      assertEqual({}, doc8.e);
      assertEqual({"one": 1}, doc8.f);
      
      var a9 = collection.update(a8, { d : { "four" : 4 }, "e" : { "e1" : [ 1, 2 ], "e2" : 2 }, "f" : { "three" : 3 }} );
      assertEqual(a1._id, a9._id);
      assertNotEqual(a8._rev, a9._rev);
      
      var doc9 = collection.document(a1._id);
      assertEqual(a1._id, doc9._id);
      assertEqual(a9._rev, doc9._rev);
      assertEqual({"one": 1, "two": 2, "three": 3, "four": 4}, doc9.d);
      assertEqual({"e2": 2, "e1": [ 1, 2 ]}, doc9.e);
      assertEqual({"one": 1, "three": 3}, doc9.f);
      
      var a10 = collection.update(a9, { d : { "one" : -1, "two": null, "four" : null, "five" : 5 }, "e" : { "e1" : 1, "e2" : null, "e3" : 3 }}, true, false);
      assertEqual(a1._id, a10._id);
      assertNotEqual(a9._rev, a10._rev);
      
      var doc10 = collection.document(a1._id);
      assertEqual(a1._id, doc10._id);
      assertEqual(a10._rev, doc10._rev);
      assertEqual({"one": -1, "three": 3, "five": 5}, doc10.d);
      assertEqual({"e1": 1, "e3": 3}, doc10.e);
      assertEqual({"one": 1, "three": 3}, doc10.f);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        collection.remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var a3 = collection.remove(a1, true);

      assertEqual(a3, true);

      var a4 = collection.remove(a1, true);

      assertEqual(a4, false);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.update(a1, { a : 2 }, true, false);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.update(a1, { a : 2 }, true, true);

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document, waitForSync=false
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocumentSyncFalse : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.remove(a1, true, false);
      assertEqual(a2, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document, waitForSync=true
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocumentSyncTrue : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = collection.remove(a1, true, true);
      assertEqual(a2, true);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a deleted document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDeletedDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      collection.remove(a1);

      try {
        collection.remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check figures
////////////////////////////////////////////////////////////////////////////////

    testFiguresAfterOperations : function () {
      var figures;

      collection.save({ _key : "a1" });
      collection.save({ _key : "a2" });
      collection.save({ _key : "a3" });

      figures = collection.figures();
      assertEqual(3, figures.alive.count);
      assertEqual(0, figures.dead.count);

      // insert a few duplicates
      try {
        collection.save({ _key : "a1" });
        fail();
      }
      catch (e1) {
      }
      
      try {
        collection.save({ _key : "a2" });
        fail();
      }
      catch (e2) {
      }

      // we should see the same figures 
      figures = collection.figures();
      assertEqual(3, figures.alive.count);
      assertEqual(0, figures.dead.count);

      // now remove some documents
      collection.remove("a2");
      collection.remove("a3");

      // we should see two live docs less
      figures = collection.figures();
      assertEqual(1, figures.alive.count);
      assertEqual(2, figures.dead.count);

      // replacing one document does not change alive, but increases dead!
      collection.replace("a1", { });
      figures = collection.figures();
      assertEqual(1, figures.alive.count);
      assertEqual(3, figures.dead.count);

      // this doc does not exist. should not change the figures
      try {
        collection.replace("a2", { });
        fail();
      }
      catch (e3) {
      }
      
      figures = collection.figures();
      assertEqual(1, figures.alive.count);
      assertEqual(3, figures.dead.count);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   vocbase methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function DatabaseDocumentSuiteErrorHandling () {
  var cn = "UnitTestsCollectionBasics";
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandle : function () {
      try {
        db._document("  123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle replace
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleReplace : function () {
      try {
        db._replace("123456  ", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle delete
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandleDelete : function () {
      try {
        db._remove("123/45/6");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown document identifier
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingUnknownDocument : function () {
      var collection = db._create(cn, { waitForSync : false });

      try {
        db._document(collection.name() + "/123456");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }

      collection.drop();
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function DatabaseDocumentSuite () {
  var cn = "UnitTestsCollectionBasics";
  var ERRORS = require("internal").errors;
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      collection = db._create(cn, { waitForSync : false });

      collection.load();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      collection.drop();
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document
////////////////////////////////////////////////////////////////////////////////

    testReadDocument : function () {
      var d = collection.save({ "Hello" : "World" });

      var doc = db._document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      doc = db._document(d);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read a document with conflict
////////////////////////////////////////////////////////////////////////////////

    testReadDocumentConflict : function () {
      var d = collection.save({ "Hello" : "World" });

      var doc = db._document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);

      var r = collection.replace(d, { "Hello" : "You" });

      doc = db._document(r);

      assertEqual(d._id, doc._id);
      assertNotEqual(d._rev, doc._rev);

      try {
        db._document(d);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document
////////////////////////////////////////////////////////////////////////////////

    testReplaceDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._replace(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = db._document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = db._replace(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = db._document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document
////////////////////////////////////////////////////////////////////////////////

    testUpdateDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = db._update(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._update(a1, { a : 3 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var doc2 = db._document(a1._id);

      assertEqual(a1._id, doc2._id);
      assertEqual(a2._rev, doc2._rev);
      assertEqual(2, doc2.a);

      var a4 = db._update(a1, { a : 4 }, true);

      assertEqual(a1._id, a4._id);
      assertNotEqual(a1._rev, a4._rev);
      assertNotEqual(a2._rev, a4._rev);

      var doc4 = db._document(a1._id);

      assertEqual(a1._id, doc4._id);
      assertEqual(a4._rev, doc4._rev);
      assertEqual(4, doc4.a);
      
      var a5 = db._update(a4, { b : 1, c : 2, d : "foo", e : null });
      assertEqual(a1._id, a5._id);
      assertNotEqual(a4._rev, a5._rev);
      
      var doc5 = db._document(a1._id);
      assertEqual(a1._id, doc5._id);
      assertEqual(a5._rev, doc5._rev);
      assertEqual(4, doc5.a);
      assertEqual(1, doc5.b);
      assertEqual(2, doc5.c);
      assertEqual("foo", doc5.d);
      assertEqual(null, doc5.e);

      var a6 = db._update(a5, { f : null, b : null, a : null, g : 2, c : 4 });
      assertEqual(a1._id, a6._id);
      assertNotEqual(a5._rev, a6._rev);
      
      var doc6 = db._document(a1._id);
      assertEqual(a1._id, doc6._id);
      assertEqual(a6._rev, doc6._rev);
      assertEqual(null, doc6.a);
      assertEqual(null, doc6.b);
      assertEqual(4, doc6.c);
      assertEqual("foo", doc6.d);
      assertEqual(null, doc6.e);
      assertEqual(null, doc6.f);
      assertEqual(2, doc6.g);
      
      var a7 = db._update(a6, { a : null, b : null, c : null, g : null }, true, false);
      assertEqual(a1._id, a7._id);
      assertNotEqual(a6._rev, a7._rev);
      
      var doc7 = db._document(a1._id);
      assertEqual(a1._id, doc7._id);
      assertEqual(a7._rev, doc7._rev);
      assertEqual(undefined, doc7.a);
      assertEqual(undefined, doc7.b);
      assertEqual(undefined, doc7.c);
      assertEqual("foo", doc7.d);
      assertEqual(null, doc7.e);
      assertEqual(null, doc7.f);
      assertEqual(undefined, doc7.g);
      
      var a8 = db._update(a7, { d : { "one" : 1, "two" : 2, "three" : 3 }, e : { }, f : { "one" : 1 }} );
      assertEqual(a1._id, a8._id);
      assertNotEqual(a7._rev, a8._rev);
      
      var doc8 = db._document(a1._id);
      assertEqual(a1._id, doc8._id);
      assertEqual(a8._rev, doc8._rev);
      assertEqual({"one": 1, "two": 2, "three": 3}, doc8.d);
      assertEqual({}, doc8.e);
      assertEqual({"one": 1}, doc8.f);
      
      var a9 = db._update(a8, { d : { "four" : 4 }, "e" : { "e1" : [ 1, 2 ], "e2" : 2 }, "f" : { "three" : 3 }} );
      assertEqual(a1._id, a9._id);
      assertNotEqual(a8._rev, a9._rev);
      
      var doc9 = db._document(a1._id);
      assertEqual(a1._id, doc9._id);
      assertEqual(a9._rev, doc9._rev);
      assertEqual({"one": 1, "two": 2, "three": 3, "four": 4}, doc9.d);
      assertEqual({"e2": 2, "e1": [ 1, 2 ]}, doc9.e);
      assertEqual({"one": 1, "three": 3}, doc9.f);
      
      var a10 = db._update(a9, { d : { "one" : -1, "two": null, "four" : null, "five" : 5 }, "e" : { "e1" : 1, "e2" : null, "e3" : 3 }}, true, false);
      assertEqual(a1._id, a10._id);
      assertNotEqual(a9._rev, a10._rev);
      
      var doc10 = db._document(a1._id);
      assertEqual(a1._id, doc10._id);
      assertEqual(a10._rev, doc10._rev);
      assertEqual({"one": -1, "three": 3, "five": 5}, doc10.d);
      assertEqual({"e1": 1, "e3": 3}, doc10.e);
      assertEqual({"one": 1, "three": 3}, doc10.f);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      var a2 = db._replace(a1, { a : 2 });

      assertEqual(a1._id, a2._id);
      assertNotEqual(a1._rev, a2._rev);

      try {
        db._remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CONFLICT.code, err.errorNum);
      }

      var a3 = db._remove(a1, true);

      assertEqual(a3, true);

      var a4 = db._remove(a1, true);

      assertEqual(a4, false);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a deleted document
////////////////////////////////////////////////////////////////////////////////

    testDeleteDeletedDocument : function () {
      var a1 = collection.save({ a : 1});

      assertTypeOf("string", a1._id);
      assertTypeOf("string", a1._rev);

      db._remove(a1);

      try {
        db._remove(a1);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document too big for a journal 
////////////////////////////////////////////////////////////////////////////////

    testDocumentTooLarge : function () {
      // create a very big and silly document, just to blow up the datafiles
      var doc = { };
      for (var i = 0; i < 50000; ++i) {
        var val = "thequickbrownfoxjumpsoverthelazydog" + i;
        doc[val] = val;
      }

      assertEqual(0, collection.count());
      try {
        collection.save(doc);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_NO_JOURNAL.code, err.errorNum);
      }
      assertEqual(0, collection.count());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a document bigger than shape file size
////////////////////////////////////////////////////////////////////////////////

    testBigShape : function () {
      // create a very big and silly document, just to blow up the datafiles
      var doc = { _key : "mydoc" };
      for (var i = 0; i < 50000; ++i) {
        doc["thequickbrownfox" + i] = 1;
      }

      assertEqual(0, collection.count());
      collection.save(doc);
      assertEqual(1, collection.count());
      assertEqual("mydoc", collection.document("mydoc")._key);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionDocumentSuiteErrorHandling);
jsunity.run(CollectionDocumentSuite);

jsunity.run(DatabaseDocumentSuiteErrorHandling);
jsunity.run(DatabaseDocumentSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
