/*jshint globalstrict:false, strict:false */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the edges interface
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

var arangodb = require("@arangodb");
var ArangoCollection = arangodb.ArangoCollection;
var db = arangodb.db;
var wait = require("internal").wait;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: normal operations
////////////////////////////////////////////////////////////////////////////////

function CollectionEdgeSuite () {
  'use strict';
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
/// @brief edges query list
////////////////////////////////////////////////////////////////////////////////

    testReadEdgesList : function () {
      var d1 = edge.save(v1, v2, { "Hello" : "World" });
      var d2 = edge.save(v2, v1, { "World" : "Hello" });

      var e = edge.edges([v1]);

      assertEqual(2, e.length);

      if (e[0]._id === d1._id) {
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

      // The list of edges is UNIQUE. So we find both edges exactly once
      assertEqual(2, e.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief in edges query list
////////////////////////////////////////////////////////////////////////////////

    testReadInEdgesList : function () {
      var d = edge.save(v1, v2, { "Hello" : "World" });

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
      var d = edge.save(v1, v2, { "Hello" : "World" });

      var e = edge.outEdges([v1]);

      assertEqual(1, e.length);
      assertEqual(d._id, e[0]._id);
      assertEqual(v2._id, e[0]._to);
      assertEqual(v1._id, e[0]._from);

      var f = edge.outEdges([v2]);

      assertEqual(0, f.length);

      e = edge.outEdges([v1, v2]);

      assertEqual(1, e.length);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CollectionEdgeSuite);

return jsunity.done();

