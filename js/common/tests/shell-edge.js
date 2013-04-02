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

var arangodb = require("org/arangodb");
var console = require("console");

var ArangoCollection = arangodb.ArangoCollection;
var db = arangodb.db;
var ERRORS = arangodb.errors;
var wait = require("internal").wait;

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function CollectionEdgeSuiteErrorHandling () {
  var vn = "UnitTestsCollectionVertex";
  var vertex = null;

  var en = "UnitTestsCollectionEdge";
  var edge = null;

  var v1 = null;
  var v2 = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(en);
      edge = db._createEdgeCollection(en, { waitForSync : false });

      db._drop(vn);
      vertex = db._create(vn, { waitForSync : false });

      v1 = vertex.save({ a : 1 });
      v2 = vertex.save({ a : 2 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      edge.unload();
      edge.drop();
      edge = null;
      wait(0.0);

      vertex.unload();
      vertex.drop();
      vertex = null;
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandle : function () {
      try {
        edge.save("  123456", v1, {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }

      try {
        edge.save(v1, "123456  ", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
      
      try {
        edge.save(v1, "1234/56/46", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function CollectionEdgeSuite () {
  var vn = "UnitTestsCollectionVertex";
  var vertex = null;

  var en = "UnitTestsCollectionEdge";
  var edge = null;

  var v1 = null;
  var v2 = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(en);
      edge = db._createEdgeCollection(en, { waitForSync : false });
      assertEqual(ArangoCollection.TYPE_EDGE, edge.type());

      db._drop(vn);
      vertex = db._create(vn, { waitForSync : false });

      v1 = vertex.save({ a : 1 });
      v2 = vertex.save({ a : 2 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      edge.drop();
      vertex.drop();
      edge = null;
      vertex = null;
      wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create an edge referring to a vertex documents by keys
////////////////////////////////////////////////////////////////////////////////

    testSaveEdgeKeys : function () {
      var k1 = vertex.save({ _key: "vx1", vx: 1 });
      var k2 = vertex.save({ _key: "vx2", vx: 2 });
      var k3 = vertex.save({ _key: "vx3", vx: 3 });

      var d1 = vertex.document(k1._key);
      assertEqual("vx1", k1._key);
      assertEqual(vn + "/vx1", k1._id);
      assertEqual("vx1", d1._key);
      assertEqual(1, d1.vx);
      assertEqual(vn + "/vx1", d1._id);

      var d2 = vertex.document(k2._key);
      assertEqual("vx2", k2._key);
      assertEqual(vn + "/vx2", k2._id);
      assertEqual("vx2", d2._key);
      assertEqual(2, d2.vx);
      assertEqual(vn + "/vx2", d2._id);
      
      var d3 = vertex.document(vn + "/vx3");
      assertEqual("vx3", k3._key);
      assertEqual(vn + "/vx3", k3._id);
      assertEqual("vx3", d3._key);
      assertEqual(3, d3.vx);
      assertEqual(vn + "/vx3", d3._id);

      var e1 = edge.save(vn + "/vx1", vn + "/vx2", { _key: "ex1", connect: "vx1->vx2" });
      var e2 = edge.save(vn + "/vx2", vn + "/vx1", { _key: "ex2", connect: "vx2->vx1" });
      var e3 = edge.save(vn + "/vx3", vn + "/vx1", { _key: "ex3", connect: "vx3->vx1" });

      d1 = edge.document("ex1");
      assertEqual("ex1", d1._key);
      assertEqual(en + "/ex1", d1._id);
      assertEqual(vn + "/vx1", d1._from);
      assertEqual(vn + "/vx2", d1._to);
      assertEqual("vx1->vx2", d1.connect);
      assertEqual("ex1", e1._key);
      assertEqual(en + "/ex1", e1._id);

      d2 = edge.document("ex2");
      assertEqual("ex2", d2._key);
      assertEqual(en + "/ex2", d2._id);
      assertEqual(vn + "/vx2", d2._from);
      assertEqual(vn + "/vx1", d2._to);
      assertEqual("vx2->vx1", d2.connect);
      assertEqual("ex2", e2._key);
      assertEqual(en + "/ex2", e2._id);
     
      d3 = edge.document(en + "/ex3");
      assertEqual("ex3", d3._key);
      assertEqual(en + "/ex3", d3._id);
      assertEqual(vn + "/vx3", d3._from);
      assertEqual(vn + "/vx1", d3._to);
      assertEqual("vx3->vx1", d3.connect);
      assertEqual("ex3", e3._key);
      assertEqual(en + "/ex3", e3._id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create an edge referring to an unloaded vertex collection
////////////////////////////////////////////////////////////////////////////////

    testSaveEdgeUnloaded : function () {
      var k1 = vertex.save({ _key: "vx1", vx: 1 });
      var k2 = vertex.save({ _key: "vx2", vx: 2 });

      assertEqual("vx1", k1._key);
      assertEqual(vn + "/vx1", k1._id);
      assertEqual("vx2", k2._key);
      assertEqual(vn + "/vx2", k2._id);

      vertex.unload();
      edge.unload();

      console.log("waiting for collections to unload"); 
      wait(4);

      var e1 = edge.save(vn + "/vx1", vn + "/vx2", { _key: "ex1", connect: "vx1->vx2" });
      var e2 = edge.save(vn + "/vx2", vn + "/vx1", { _key: "ex2", connect: "vx2->vx1" });
      
      vertex.unload();
      edge.unload();

      console.log("waiting for collections to unload"); 
      wait(4);
      var e3 = edge.save(k1, k2, { _key: "ex3", connect: "vx1->vx2" });
      
      d1 = edge.document("ex1");
      assertEqual("ex1", d1._key);
      assertEqual(en + "/ex1", d1._id);
      assertEqual(vn + "/vx1", d1._from);
      assertEqual(vn + "/vx2", d1._to);
      assertEqual("vx1->vx2", d1.connect);
      assertEqual("ex1", e1._key);
      assertEqual(en + "/ex1", e1._id);

      d2 = edge.document("ex2");
      assertEqual("ex2", d2._key);
      assertEqual(en + "/ex2", d2._id);
      assertEqual(vn + "/vx2", d2._from);
      assertEqual(vn + "/vx1", d2._to);
      assertEqual("vx2->vx1", d2.connect);
      assertEqual("ex2", e2._key);
      assertEqual(en + "/ex2", e2._id);
      
      d3 = edge.document("ex3");
      assertEqual("ex3", d3._key);
      assertEqual(en + "/ex3", d3._id);
      assertEqual(vn + "/vx1", d3._from);
      assertEqual(vn + "/vx2", d3._to);
      assertEqual("vx1->vx2", d3.connect);
      assertEqual("ex3", e3._key);
      assertEqual(en + "/ex3", e3._id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create an edge
////////////////////////////////////////////////////////////////////////////////

    testSaveEdge : function () {
      var doc = edge.save(v1, v2, { "Hallo" : "World" });

      assertTypeOf("string", doc._id);
      assertTypeOf("string", doc._rev);
      assertMatch(/^UnitTestsCollectionEdge\//, doc._id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create an edge that reference non-existing vertex collections
////////////////////////////////////////////////////////////////////////////////

    testSaveEdgeInvalidVertexCollection : function () {
      [ "UnitTestsCollectionNonExistingVertex/12345", "UnitTestsCollectionNonExistingVertex/456745" ].forEach(function(key) {
        try {
          edge.save(key, key, { });
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_ARANGO_COLLECTION_NOT_FOUND.code, err.errorNum);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create an edge that reference an unloaded collection
////////////////////////////////////////////////////////////////////////////////

    testSaveEdgeUnloadedVertexCollection : function () {
      vertex.unload();
      wait(4);

      var e = edge.save(v1._id, v2._id, { });
      
      assertMatch(/^UnitTestsCollectionEdge\//, e._id);
      var doc = edge.document(e._id);

      assertMatch(/^UnitTestsCollectionEdge\//, doc._id);
      assertMatch(/^UnitTestsCollectionVertex\//, doc._from);
      assertMatch(/^UnitTestsCollectionVertex\//, doc._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read an edge
////////////////////////////////////////////////////////////////////////////////

    testReadEdge : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      var doc = edge.document(d._id);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);
      assertEqual(v1._id, doc._from);
      assertEqual(v2._id, doc._to);

      doc = edge.document(d);

      assertEqual(d._id, doc._id);
      assertEqual(d._rev, doc._rev);
      assertEqual(v1._id, doc._from);
      assertEqual(v2._id, doc._to);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief edges query
////////////////////////////////////////////////////////////////////////////////

    testReadEdges : function () {
      var d1 = edge.save(v1, v2, { "Hallo" : "World" });
      var d2 = edge.save(v2, v1, { "World" : "Hallo" });

      var e = edge.edges(v1);

      assertEqual(2, e.length);

      if (e[0]._id == d1._id) {
        assertEqual(v2._id, e[0]._to);
        assertEqual(v1._id, e[0]._from);

        assertEqual(d2._id, e[1]._id);
        assertEqual(v1._id, e[1]._to);
        assertEqual(v2._id, e[1]._from);
      }
      else {
        assertEqual(v1._id, e[0]._to);
        assertEqual(v2._id, e[0]._from);

        assertEqual(d1._id, e[1]._id);
        assertEqual(v2._id, e[1]._to);
        assertEqual(v1._id, e[1]._from);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief in edges query
////////////////////////////////////////////////////////////////////////////////

    testReadInEdges : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      var e = edge.inEdges(v2);

      assertEqual(1, e.length);
      assertEqual(d._id, e[0]._id);
      assertEqual(v2._id, e[0]._to);
      assertEqual(v1._id, e[0]._from);

      var f = edge.inEdges(v1);

      assertEqual(0, f.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief out edges query
////////////////////////////////////////////////////////////////////////////////

    testReadOutEdges : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      var e = edge.outEdges(v1);

      assertEqual(1, e.length);
      assertEqual(d._id, e[0]._id);
      assertEqual(v2._id, e[0]._to);
      assertEqual(v1._id, e[0]._from);

      var f = edge.outEdges(v2);

      assertEqual(0, f.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief edges query list
////////////////////////////////////////////////////////////////////////////////

    testReadEdgesList : function () {
      var d1 = edge.save(v1, v2, { "Hallo" : "World" });
      var d2 = edge.save(v2, v1, { "World" : "Hallo" });

      var e = edge.edges([v1]);

      assertEqual(2, e.length);

      if (e[0]._id == d1._id) {
        assertEqual(v2._id, e[0]._to);
        assertEqual(v1._id, e[0]._from);

        assertEqual(d2._id, e[1]._id);
        assertEqual(v1._id, e[1]._to);
        assertEqual(v2._id, e[1]._from);
      }
      else {
        assertEqual(v1._id, e[0]._to);
        assertEqual(v2._id, e[0]._from);

        assertEqual(d1._id, e[1]._id);
        assertEqual(v2._id, e[1]._to);
        assertEqual(v1._id, e[1]._from);
      }

      e = edge.edges([v1, v2]);

      assertEqual(4, e.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief in edges query list
////////////////////////////////////////////////////////////////////////////////

    testReadInEdgesList : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      var e = edge.inEdges([v2]);

      assertEqual(1, e.length);
      assertEqual(d._id, e[0]._id);
      assertEqual(v2._id, e[0]._to);
      assertEqual(v1._id, e[0]._from);

      var f = edge.inEdges([v1]);

      assertEqual(0, f.length);

      e = edge.inEdges([v1, v2]);

      assertEqual(1, e.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief out edges query list
////////////////////////////////////////////////////////////////////////////////

    testReadOutEdgesList : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      var e = edge.outEdges([v1]);

      assertEqual(1, e.length);
      assertEqual(d._id, e[0]._id);
      assertEqual(v2._id, e[0]._to);
      assertEqual(v1._id, e[0]._from);

      var f = edge.outEdges([v2]);

      assertEqual(0, f.length);

      e = edge.outEdges([v1, v2]);

      assertEqual(1, e.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief edges query
////////////////////////////////////////////////////////////////////////////////

    testReadEdgesUnloaded : function () {
      var d1 = edge.save(v1, v2, { "Hallo" : "World" })._id;
      var d2 = edge.save(v2, v1, { "World" : "Hallo" })._id;

      var e1 = edge.document(d1);
      var e2 = edge.document(d2);

      assertMatch(/^UnitTestsCollectionEdge\//, e1._id);
      assertMatch(/^UnitTestsCollectionVertex\//, e1._from);
      assertMatch(/^UnitTestsCollectionVertex\//, e1._to);

      assertMatch(/^UnitTestsCollectionEdge\//, e2._id);
      assertMatch(/^UnitTestsCollectionVertex\//, e2._from);
      assertMatch(/^UnitTestsCollectionVertex\//, e2._to);

      e1 = null;
      e2 = null;

      vertex.unload();
      edge.unload();
      wait(4);

      var e = edge.edges(v1);

      assertEqual(2, e.length);

      if (e[0]._id == d1) {
        assertEqual(v2._id, e[0]._to);
        assertEqual(v1._id, e[0]._from);

        assertEqual(d2, e[1]._id);
        assertEqual(v1._id, e[1]._to);
        assertEqual(v2._id, e[1]._from);
      }
      else {
        assertEqual(v1._id, e[0]._to);
        assertEqual(v2._id, e[0]._from);

        assertEqual(d1, e[1]._id);
        assertEqual(v2._id, e[1]._to);
        assertEqual(v1._id, e[1]._from);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief edges query invalid
////////////////////////////////////////////////////////////////////////////////

    testReadEdgesInvalid1 : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      try {
        var e = edge.edges("t/h/e/f/o/x");
      } 
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief edges query invalid
////////////////////////////////////////////////////////////////////////////////

    testReadEdgesInvalid2 : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      try {
        var e = edge.edges("123456  ");
      } 
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief in edges query invalid
////////////////////////////////////////////////////////////////////////////////

    testReadInEdgesInvalid1 : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      try {
        var e = edge.inEdges("the//fox");
      } 
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief in edges query invalid
////////////////////////////////////////////////////////////////////////////////

    testReadInEdgesInvalid2 : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      try {
        var e = edge.inEdges(" 123456 ");
      } 
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief out edges query invalid
////////////////////////////////////////////////////////////////////////////////

    testReadOutEdgesInvalid1 : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      try {
        var e = edge.outEdges("the//fox");
      } 
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief out edges query invalid
////////////////////////////////////////////////////////////////////////////////

    testReadOutEdgesInvalid2 : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      try {
        var e = edge.outEdges("123456  ");
      } 
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid collection type for edges
////////////////////////////////////////////////////////////////////////////////

    testEdgesCollectionTypeInvalid : function () {
      var dn = "UnitTestsCollectionInvalid";

      db._drop(dn);
      var c = db._create(dn);
      
      try {
        var e = c.edges("the fox");
      } 
      catch (err) {
        assertInstanceOf(TypeError, err);
      }

      db._drop(dn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read edges of a small graph
////////////////////////////////////////////////////////////////////////////////

    testEdgesGraph : function () {
      var a = vertex.save( {"name" : "a" });
      var b = vertex.save( {"name" : "b" });
      var c = vertex.save( {"name" : "c" });
      var d = vertex.save( {"name" : "d" });
      var e = vertex.save( {"name" : "e" });
      var f = vertex.save( {"name" : "f" });

      edge.save(a, a, { "what" : "a->a" });
      edge.save(a, b, { "what" : "a<->b" });
      edge.save(a, c, { "what" : "a->c" });
      edge.save(d, a, { "what" : "d->a" });
      edge.save(c, d, { "what" : "c->d" });
      edge.save(d, f, { "what" : "d<->f" });
      edge.save(f, e, { "what" : "f->e" });
      edge.save(e, e, { "what" : "e->e" });

      assertEqual(3, edge.outEdges(a).length);
      assertEqual(0, edge.outEdges(b).length);
      assertEqual(1, edge.outEdges(c).length);
      assertEqual(2, edge.outEdges(d).length);
      assertEqual(1, edge.outEdges(e).length);
      assertEqual(1, edge.outEdges(f).length);
      
      assertEqual(2, edge.inEdges(a).length);
      assertEqual(1, edge.inEdges(b).length);
      assertEqual(1, edge.inEdges(c).length);
      assertEqual(1, edge.inEdges(d).length);
      assertEqual(2, edge.inEdges(e).length);
      assertEqual(1, edge.inEdges(f).length);
      
      assertEqual(4, edge.edges(a).length);
      assertEqual(1, edge.edges(b).length);
      assertEqual(2, edge.edges(c).length);
      assertEqual(3, edge.edges(d).length);
      assertEqual(2, edge.edges(e).length);
      assertEqual(2, edge.edges(f).length);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionEdgeSuiteErrorHandling);
jsunity.run(CollectionEdgeSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
