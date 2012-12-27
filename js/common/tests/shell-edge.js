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
        edge.save("123456", v1, {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }

      try {
        edge.save(v1, "123456", {});
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
        var e = edge.edges(1);
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
        var e = edge.inEdges(1);
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
        var e = edge.outEdges(1);
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

      internal.db._drop(dn);
      var c = internal.db._create(dn);
      
      try {
        var e = c.edges("the fox");
      } 
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_COLLECTION_TYPE_INVALID.code, err.errorNum);
      }

      internal.db._drop(dn);
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
