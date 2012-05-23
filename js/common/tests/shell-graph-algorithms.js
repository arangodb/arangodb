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

    testPushToNeighbors : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        test_array = [];

      graph.addEdge(v1, v2);
      graph.addEdge(v1, v3);

      v1._pushNeigborsToArray(test_array);

      assertEqual(test_array[0], 2);
      assertEqual(test_array[1], 3);
    },

    testShortestDistanceFor : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        todo_list = [v1.getId(), v2.getId()], // [ID]
        distances = {}, // {ID => [Node]}
        node_id_found; // Node

      distances[v1.getId()] = [v1, v2, v3];
      distances[v2.getId()] = [v2, v3];
      distances[v3.getId()] = [v3];

      node_id_found = v1._getShortestDistanceFor(todo_list, distances);
      assertEqual(node_id_found, v2.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path
////////////////////////////////////////////////////////////////////////////////

    testGetAShortDistinctPath : function () {
      var v1, v2, e1;

      v1 = graph.addVertex(1);
      v2 = graph.addVertex(2);

      e1 = graph.addEdge(v1, v2);

      assertEqual(v1.pathTo(v2).length, 2);
      assertEqual(v1.pathTo(v2)[0].getId(), v1.getId());
      assertEqual(v1.pathTo(v2)[1].getId(), v2.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a longer, distinct path
////////////////////////////////////////////////////////////////////////////////

    testGetALongerDistinctPath : function () {
      var v1, v2, v3, e1, e2;

      v1 = graph.addVertex(1);
      v2 = graph.addVertex(2);
      v3 = graph.addVertex(3);

      e1 = graph.addEdge(v1, v2);
      e2 = graph.addEdge(v2, v3);

      assertEqual(v1.pathTo(v3).length, 3);
      assertEqual(v1.pathTo(v3)[0].getId(), v1.getId());
      assertEqual(v1.pathTo(v3)[1].getId(), v2.getId());
      assertEqual(v1.pathTo(v3)[2].getId(), v3.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief compare with Neo4j on a random network of size 500
////////////////////////////////////////////////////////////////////////////////

    testComparisonWithNeo4j500 : function () {
      var base_path = "js/common/test-data/random500/",
        v1,
        v2,
        e,
        path,
        results = [],
        counter;

      Helper.process(base_path + "generated_edges.csv", function (row) {
        v1 = graph.getOrAddVertex(row[1]);
        v2 = graph.getOrAddVertex(row[2]);
        e = graph.addEdge(v1, v2);
      });

      Helper.process(base_path + "generated_testcases.csv", function (row) {
        v1 = graph.getVertex(row[0]);
        v2 = graph.getVertex(row[1]);
        if (v1 !== null && v2 !== null) {
          path = v1.pathTo(v2);
          results.push(path);
        }
      });

      v1 = "";
      v2 = "";
      counter = 0;
      Helper.process(base_path + "neo4j-dijkstra.csv", function (row) {
        var result;

        if (!(v1 === row[0] && v2 === row[row.length - 1])) {
          v1 = row[0];
          v2 = row[row.length - 1];
          result = results[counter];

          assertEqual(v1, result[0].getId());
          assertEqual(v2, result[result.length - 1].getId());

          if (result.length !== row.length) {
            console.log("Neo4j: Path from " + v1 + " to " + v2 + " has length " + result.length + " (should be " + row.length + ")");
          }
          counter += 1;
        }
        //assertEqual(raw_row, results[index]);
      });
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(dijkstraSuite);
return jsunity.done();