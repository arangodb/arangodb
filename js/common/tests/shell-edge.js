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

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: error handling
////////////////////////////////////////////////////////////////////////////////

function collectionEdgeSuiteErrorHandling () {
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
      edges._drop(en);
      edge = edges._create(en, { waitForSync : false });

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
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }

      try {
        edge.save(v1, "123456", {});
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_AVOCADO_DOCUMENT_HANDLE_BAD.code, err.errorNum);
      }
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function collectionEdgeSuite () {
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
      edges._drop(en);
      edge = edges._create(en, { waitForSync : false });

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

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(collectionEdgeSuiteErrorHandling);
jsunity.run(collectionEdgeSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
