/*jslint indent: 2,
         nomen: true,
         maxlen: 80,
         sloppy: true */
/*global require,
    db,
    assertEqual, assertTrue,
    print,
    PRINT_OBJECT,
    AvocadoCollection, AvocadoEdgesCollection,
    processCsvFile */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the graph class
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

var jsunity = require("jsunity"),
  Helper = require("test-helper").Helper,
  console = require("console");

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Get Neighbor Function
////////////////////////////////////////////////////////////////////////////////

function measurementSuite() {
  var Graph = require("graph").Graph,
    graph_name = "UnitTestsCollectionGraph",
    vertex = "UnitTestsCollectionVertex",
    edge = "UnitTestsCollectionEdge",
    graph = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      try {
        try {
          // Drop the graph if it exsits
          graph = new Graph(graph_name);
          print("FOUND: ");
          PRINT_OBJECT(graph);
          graph.drop();
        } catch (err1) {
        }

        graph = new Graph(graph_name, vertex, edge);
      } catch (err2) {
        console.error("[FAILED] setup failed:" + err2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        if (graph !== null) {
          graph.drop();
        }
      } catch (err) {
        console.error("[FAILED] tear-down failed:" + err);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the get total, in and out degree of a vertex
////////////////////////////////////////////////////////////////////////////////

    testGetDegrees : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3);

      graph.addEdge(v1, v2);
      graph.addEdge(v3, v1);

      assertEqual(v1.degree(), 2);
      assertEqual(v1.inDegree(), 1);
      assertEqual(v1.outDegree(), 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the get total, in and out degree of a vertex
////////////////////////////////////////////////////////////////////////////////

    testGetOrderAndSize : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3);

      graph.addEdge(v1, v2);
      graph.addEdge(v3, v1);

      assertEqual(graph.order(), 3);
      assertEqual(graph.size(), 2);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(measurementSuite);
return jsunity.done();