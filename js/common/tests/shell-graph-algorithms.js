/*jslint indent: 2,
         nomen: true,
         maxlen: 80,
         sloppy: true */
/*global require,
    db,
    assertEqual, assertTrue,
    print,
    PRINT_OBJECT,
    console,
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
  Helper = require("test-helper").Helper;

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Dijkstra
////////////////////////////////////////////////////////////////////////////////

function dijkstraSuite() {
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
/// @brief test the get neighbors function
////////////////////////////////////////////////////////////////////////////////

    testGetAllNeighbors : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        result_array = [];

      graph.addEdge(v1, v2);
      graph.addEdge(v3, v1);

      result_array = v1.getNeighbors('both');

      assertEqual(result_array.length, 2);
      assertEqual(result_array[0], 2);
      assertEqual(result_array[1], 3);
    },

    testGetOutboundNeighbors : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        result_array = [];

      graph.addEdge(v1, v2);
      graph.addEdge(v3, v1);

      result_array = v1.getNeighbors('outbound');

      assertEqual(result_array.length, 1);
      assertEqual(result_array[0], 2);
    },

    testGetInboundNeighbors : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        result_array = [];

      graph.addEdge(v1, v2);
      graph.addEdge(v3, v1);

      result_array = v1.getNeighbors('inbound');

      assertEqual(result_array.length, 1);
      assertEqual(result_array[0], 3);
    },

    testGetNeighborsWithPathLabel : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        result_array = [];

      graph.addEdge(v1, v2, 8, 'a');
      graph.addEdge(v1, v3, 9, 'b');

      result_array = v1.getNeighbors('both', ['a']);

      assertEqual(result_array.length, 1);
      assertEqual(result_array[0], 2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path
////////////////////////////////////////////////////////////////////////////////

    testPathesForTree : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        tree = {},
        pathes;

      tree[v1.getId()] = [v2.getId(), v3.getId()];
      tree[v2.getId()] = [v4.getId(), v5.getId()];

      pathes = v1.pathesForTree(tree);
      assertEqual(pathes.length, 3);
      assertEqual(pathes[0].length, 3);
      assertEqual(pathes[1].length, 3);
      assertEqual(pathes[2].length, 2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path
////////////////////////////////////////////////////////////////////////////////

    testGetAShortDistinctPath : function () {
      var v1, v2, e1, path;

      v1 = graph.addVertex(1);
      v2 = graph.addVertex(2);

      e1 = graph.addEdge(v1, v2);

      path = v1.pathTo(v2)[0];
      assertEqual(path.length, 2);
      assertEqual(path[0].toString(), v1.getId());
      assertEqual(path[1].toString(), v2.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a longer, distinct path
////////////////////////////////////////////////////////////////////////////////

    testGetALongerDistinctPath : function () {
      var v1, v2, v3, e1, e2, path;

      v1 = graph.addVertex(1);
      v2 = graph.addVertex(2);
      v3 = graph.addVertex(3);

      e1 = graph.addEdge(v1, v2);
      e2 = graph.addEdge(v2, v3);

      path = v1.pathTo(v3)[0];
      assertEqual(path.length, 3);
      assertEqual(path[0].toString(), v1.getId());
      assertEqual(path[1].toString(), v2.getId());
      assertEqual(path[2].toString(), v3.getId());
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(dijkstraSuite);
return jsunity.done();
