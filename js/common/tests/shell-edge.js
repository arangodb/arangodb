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
var internal = require("internal");
var console = require("console");

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function CollectionEdgeSuiteErrorHandling () {
  var ERRORS = internal.errors;

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
      internal.db._drop(en);
      edge = internal.db._createEdgeCollection(en, { waitForSync : false });

      internal.db._drop(vn);
      vertex = internal.db._create(vn, { waitForSync : false });

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
      internal.wait(0.0);

      vertex.unload();
      vertex.drop();
      vertex = null;
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bad handle
////////////////////////////////////////////////////////////////////////////////

    testErrorHandlingBadHandle : function () {
      try {
        edge.save("123~456", v1, {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }

      try {
        edge.save(v1, "123~456", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function CollectionEdgeSuite () {
  var ERRORS = require("internal").errors;

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
      internal.db._drop(en);
      edge = internal.db._createEdgeCollection(en, { waitForSync : false });
      assertEqual(ArangoCollection.TYPE_EDGE, edge.type());

      internal.db._drop(vn);
      vertex = internal.db._create(vn, { waitForSync : false });

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
      internal.wait(0.0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create an edge
////////////////////////////////////////////////////////////////////////////////

    testSaveEdge : function () {
      var doc = edge.save(v1, v2, { "Hallo" : "World" });

      assertTypeOf("string", doc._id);
      assertTypeOf("number", doc._rev);
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
/// @brief edges query invalid
////////////////////////////////////////////////////////////////////////////////

    testReadEdgesInvalid1 : function () {
      var d = edge.save(v1, v2, { "Hallo" : "World" });

      try {
        var e = edge.edges("the fox");
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
        var e = edge.edges("123~456");
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
        var e = edge.inEdges("the fox");
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
        var e = edge.inEdges("123~456");
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
        var e = edge.outEdges("the fox");
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
        var e = edge.outEdges("123~456");
      } 
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },
    
////////////////////////////////////////////////////////////////////////////////
/// @brief test rollback of edge insert
////////////////////////////////////////////////////////////////////////////////

    testRollbackEdgeInsert : function () {
      var v1 = vertex.save({ "x" : 1 });
      var v2 = vertex.save({ "x" : 2 });

      edge.ensureUniqueConstraint("myid");
      edge.save(v1, v2, { "myid" : 1 });
      try {
        edge.save(v1, v2, { "myid" : 1 });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test rollback of edge update
////////////////////////////////////////////////////////////////////////////////

    testRollbackEdgeUpdate : function () {
      var v1 = vertex.save({ "x" : 1 });
      var v2 = vertex.save({ "x" : 2 });

      edge.ensureUniqueConstraint("myid");
      edge.save(v1, v2, { "myid" : 1 });
      var e2 = edge.save(v1, v2, { "myid" : 2 });

      try {
        edge.update(e2, { "myid" : 1 });
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief invalid collection type for edges
////////////////////////////////////////////////////////////////////////////////

    testEdgesCollectionTypeInvalid : function () {
      var dn = "UnitTestsCollectionInvalid";

      internal.db._drop(dn);
      var c = internal.db._create(dn);
      
      try {
        var e = c.edges("the fox");
      } 
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code, err.errorNum);
      }

      internal.db._drop(dn);
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief read edges of a small graph
////////////////////////////////////////////////////////////////////////////////

    testEdgesGraphBi : function () {
      var a = vertex.save( {"name" : "a" });
      var b = vertex.save( {"name" : "b" });
      var c = vertex.save( {"name" : "c" });
      var d = vertex.save( {"name" : "d" });
      var e = vertex.save( {"name" : "e" });
      var f = vertex.save( {"name" : "f" });
      var g = vertex.save( {"name" : "g" });
      var h = vertex.save( {"name" : "h" });
      var i = vertex.save( {"name" : "i" });
      var j = vertex.save( {"name" : "j" });

      edge.save(a, a, { "what" : "a->a", "_bidirectional" : false });
      edge.save(a, b, { "what" : "a<->b", "_bidirectional" : true });
      edge.save(a, c, { "what" : "a->c", "_bidirectional" : false });
      edge.save(d, a, { "what" : "d->a", "_bidirectional" : false });
      edge.save(c, d, { "what" : "c->d", "_bidirectional" : false });
      edge.save(d, f, { "what" : "d<->f", "_bidirectional" : true });
      edge.save(f, e, { "what" : "f->e", "_bidirectional" : false });
      edge.save(e, e, { "what" : "e->e", "_bidirectional" : false });
      edge.save(g, g, { "what" : "g->g", "_bidirectional" : false });
      edge.save(g, h, { "what" : "g<->h", "_bidirectional" : true });
      edge.save(h, g, { "what" : "h<->g", "_bidirectional" : true });
      edge.save(h, i, { "what" : "h<->i", "_bidirectional" : true });
      edge.save(i, i, { "what" : "i<->i", "_bidirectional" : true });
      edge.save(j, j, { "what" : "j->j", "_bidirectional" : false });

      assertEqual(3, edge.outEdges(a).length);
      assertEqual(1, edge.outEdges(b).length);
      assertEqual(1, edge.outEdges(c).length);
      assertEqual(2, edge.outEdges(d).length);
      assertEqual(1, edge.outEdges(e).length);
      assertEqual(2, edge.outEdges(f).length);
      assertEqual(3, edge.outEdges(g).length);
      assertEqual(3, edge.outEdges(h).length);
      assertEqual(2, edge.outEdges(i).length);
      assertEqual(1, edge.outEdges(j).length);

      assertEqual(3, edge.inEdges(a).length);
      assertEqual(1, edge.inEdges(b).length);
      assertEqual(1, edge.inEdges(c).length);
      assertEqual(2, edge.inEdges(d).length);
      assertEqual(3, edge.inEdges(a).length);
      assertEqual(2, edge.inEdges(e).length);
      assertEqual(1, edge.inEdges(f).length);
      assertEqual(3, edge.inEdges(g).length);
      assertEqual(3, edge.inEdges(a).length);
      assertEqual(3, edge.inEdges(h).length);
      assertEqual(2, edge.inEdges(i).length);
      assertEqual(1, edge.inEdges(j).length);
      
      assertEqual(4, edge.edges(a).length);
      assertEqual(1, edge.edges(b).length);
      assertEqual(2, edge.edges(c).length);
      assertEqual(3, edge.edges(d).length);
      assertEqual(2, edge.edges(e).length);
      assertEqual(2, edge.edges(f).length);
      assertEqual(3, edge.edges(g).length);
      assertEqual(3, edge.edges(h).length);
      assertEqual(2, edge.edges(i).length);
      assertEqual(1, edge.edges(j).length);
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
