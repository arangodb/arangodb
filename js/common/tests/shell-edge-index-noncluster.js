/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertNotNull */

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
var db = arangodb.db;
var wait = require("internal").wait;


function EdgeIndexSuite () {
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
      edge = db._createEdgeCollection(en);

      db._drop(vn);
      vertex = db._create(vn);

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
/// @brief test index presence
////////////////////////////////////////////////////////////////////////////////

    testIndexPresence : function () {
      var indexes = edge.getIndexes();
      assertEqual(2, indexes.length);
      assertEqual("edge", indexes[1].type);
      assertEqual([ "_from", "_to" ], indexes[1].fields);
      assertFalse(indexes[1].unique);
      assertFalse(indexes[1].sparse);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityEmpty : function () {
      var edgeIndex = edge.getIndexes()[1];
      assertTrue(edgeIndex.hasOwnProperty("selectivityEstimate"));
      assertEqual(1, edgeIndex.selectivityEstimate);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityOneDoc : function () {
      edge.save(v1, v2, { });
      var edgeIndex = edge.getIndexes()[1];
      assertTrue(edgeIndex.hasOwnProperty("selectivityEstimate"));
      assertEqual(1, edgeIndex.selectivityEstimate);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityDuplicateDocs : function () {
      var i, c, edgeIndex, expectedSelectivity;

      for (i = 0; i < 2000; ++i) {
        edge.save(v1, v2, { });
        edgeIndex = edge.getIndexes()[1];
        expectedSelectivity = 1 / edge.count();
        // allow for some floating-point deviations
        assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
      }

      var n = edge.count();
      assertEqual(2000, n);

      for (i = 0; i < n; ++i) {
        var doc = edge.any();
        assertNotNull(doc);
        edge.remove(doc._key);

        edgeIndex = edge.getIndexes()[1];
        c = edge.count();
        expectedSelectivity = (c === 0 ? 1 : 1 / c);
        // allow for some floating-point deviations
        assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
      } 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityUniqueDocs : function () {
      for (var i = 0; i < 2000; ++i) {
        edge.save(vn + "/from" + i, vn + "/to" + i, { });
        var edgeIndex = edge.getIndexes()[1];
        assertTrue(1, edgeIndex.selectivityEstimate);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityUniqueDocsFrom : function () {
      for (var i = 0; i < 2000; ++i) {
        edge.save(vn + "/from" + i, vn + "/1", { });
        var edgeIndex = edge.getIndexes()[1];
        var expectedSelectivity = (1 + (1 / (i + 1))) * 0.5; 
        assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test index selectivity
////////////////////////////////////////////////////////////////////////////////

    testIndexSelectivityRepeatingDocs : function () {
      for (var i = 0; i < 2000; ++i) {
        if (i > 0) {
          var edgeIndex = edge.getIndexes()[1];
          var expectedSelectivity = (1 + (Math.min(i, 20) / i)) * 0.5; 
          assertTrue(Math.abs(expectedSelectivity - edgeIndex.selectivityEstimate) <= 0.001);
        }
        edge.save(vn + "/from" + (i % 20), vn + "/to" + i, { });
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(EdgeIndexSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
