/*jslint indent: 2, nomen: true, maxlen: 80, sloppy: true */
/*global require, assertEqual, assertTrue */

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

var jsunity = require("jsunity");

var console = require("console");

var Helper = require("org/arangodb/test-helper").Helper;

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Get Neighbor Function
////////////////////////////////////////////////////////////////////////////////

function neighborSuite() {
  var Graph = require("org/arangodb/graph").Graph,
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
      var v1 = graph.addVertex("1"),
        v2 = graph.addVertex("2"),
        v3 = graph.addVertex("3"),
        result_array = [];

      graph.addEdge(v1, v2);
      graph.addEdge(v3, v1);

      result_array = v1.getNeighbors({ direction: 'both' });

      assertEqual(result_array.length, "2");
      assertEqual(result_array[0].id, "2");
      assertEqual(result_array[1].id, "3");
    },

    testGetOutboundNeighbors : function () {
      var v1 = graph.addVertex("1"),
        v2 = graph.addVertex("2"),
        v3 = graph.addVertex("3"),
        result_array = [];

      graph.addEdge(v1, v2);
      graph.addEdge(v3, v1);

      result_array = v1.getNeighbors({ direction: 'outbound' });

      assertEqual(result_array.length, "1");
      assertEqual(result_array[0].id, "2");
    },

    testGetInboundNeighbors : function () {
      var v1 = graph.addVertex("1"),
        v2 = graph.addVertex("2"),
        v3 = graph.addVertex("3"),
        result_array = [];

      graph.addEdge(v1, v2);
      graph.addEdge(v3, v1);

      result_array = v1.getNeighbors({ direction: 'inbound' });

      assertEqual(result_array.length, "1");
      assertEqual(result_array[0].id, "3");
    },

    testGetNeighborsWithPathLabel : function () {
      var v1 = graph.addVertex("1"),
        v2 = graph.addVertex("2"),
        v3 = graph.addVertex("3"),
        result_array = [];

      graph.addEdge(v1, v2, 8, 'a');
      graph.addEdge(v1, v3, 9, 'b');

      result_array = v1.getNeighbors({ direction: 'both', labels: ['a', 'c'] });

      assertEqual(result_array.length, "1");
      assertEqual(result_array[0].id, "2");
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Dijkstra
////////////////////////////////////////////////////////////////////////////////

function dijkstraSuite() {
  var Graph = require("org/arangodb/graph").Graph,
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path on a directed graph
////////////////////////////////////////////////////////////////////////////////

    testGetADirectedDistinctPath : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        e1 = graph.addEdge(v1, v2),
        e2 = graph.addEdge(v2, v3),
        e3 = graph.addEdge(v3, v4),
        e4 = graph.addEdge(v4, v1),
        pathes = v1.pathTo(v3, {direction: "outbound"});

      assertEqual(pathes.length, 1);
      assertEqual(pathes[0][0].toString(), v1.getId());
      assertEqual(pathes[0][1].toString(), v2.getId());
      assertEqual(pathes[0][2].toString(), v3.getId());
    },


    testGetCorrectCachedResults : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        e1 = graph.addEdge(v1, v2),
        e2 = graph.addEdge(v2, v3),
        e3,
        pathes = v1.pathTo(v3, {cached: true});

      e3 = graph.addEdge(v1, v3);

      pathes = v1.pathTo(v3, {cached: true});

      assertEqual(pathes.length, 1);
      assertEqual(pathes[0][0].toString(), v1.getId());
      assertEqual(pathes[0][1].toString(), v3.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path on a directed graph
////////////////////////////////////////////////////////////////////////////////

    testGetADirectedLabeledPath : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        e1 = graph.addEdge(v1, v2, 5, "no"),
        e2 = graph.addEdge(v1, v3, 6, "yes"),
        e3 = graph.addEdge(v3, v4, 7, "yeah"),
        e4 = graph.addEdge(v4, v2, 8, "yes"),
        pathes = v1.pathTo(v2, { labels: ["yes", "yeah"] });

      assertEqual(pathes.length, 1);
      assertEqual(pathes[0][0].toString(), v1.getId());
      assertEqual(pathes[0][1].toString(), v3.getId());
      assertEqual(pathes[0][2].toString(), v4.getId());
      assertEqual(pathes[0][3].toString(), v2.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path on a weighted graph
////////////////////////////////////////////////////////////////////////////////

    testGetADirectedWeightedPath : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        e1 = graph.addEdge(v1, v2, 5, "5", { my_weight: 5 }),
        e2 = graph.addEdge(v1, v3, 6, "6", { my_weight: 1 }),
        e3 = graph.addEdge(v3, v4, 7, "7", { my_weight: 1 }),
        e4 = graph.addEdge(v4, v2, 8, "8", { my_weight: 1 }),
        pathes = v1.pathTo(v2, { weight: "my_weight" });

      assertEqual(pathes.length, 1);
      assertEqual(pathes[0][0].toString(), v1.getId());
      assertEqual(pathes[0][1].toString(), v3.getId());
      assertEqual(pathes[0][2].toString(), v4.getId());
      assertEqual(pathes[0][3].toString(), v2.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path on a weighted graph
////////////////////////////////////////////////////////////////////////////////

    testGetADirectedWeightedPathWithUndefinedWeights : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        e1 = graph.addEdge(v1, v2, 5, "5"),
        e2 = graph.addEdge(v1, v3, 6, "6", { my_weight: 1 }),
        e3 = graph.addEdge(v3, v4, 7, "7", { my_weight: 1 }),
        e4 = graph.addEdge(v4, v2, 8, "8", { my_weight: 1 }),
        pathes = v1.pathTo(v2, { weight: "my_weight" });

      assertEqual(pathes.length, 1);
      assertEqual(pathes[0][0].toString(), v1.getId());
      assertEqual(pathes[0][1].toString(), v3.getId());
      assertEqual(pathes[0][2].toString(), v4.getId());
      assertEqual(pathes[0][3].toString(), v2.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path on a weighted graph
////////////////////////////////////////////////////////////////////////////////

    testGetADirectedWeightedPathWithDefaultWeight : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        e1 = graph.addEdge(v1, v2, 5, "5"),
        e2 = graph.addEdge(v1, v3, 6, "6", { my_weight: 1 }),
        e3 = graph.addEdge(v3, v4, 7, "7", { my_weight: 1 }),
        e4 = graph.addEdge(v4, v2, 8, "8", { my_weight: 1 }),
        pathes = v1.pathTo(v2, { weight: "my_weight", default_weight: 1 });

      assertEqual(pathes.length, 1);
      assertEqual(pathes[0][0].toString(), v1.getId());
      assertEqual(pathes[0][1].toString(), v2.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path on a weighted graph with a custom function
////////////////////////////////////////////////////////////////////////////////

    testGetAPathWithACustomWeightFunction : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        e1 = graph.addEdge(v1, v2, 5, "5", { my_weight: 5 }),
        e2 = graph.addEdge(v1, v3, 6, "6", { my_weight: 1 }),
        e3 = graph.addEdge(v3, v4, 7, "7", { my_weight: 1 }),
        e4 = graph.addEdge(v4, v2, 8, "8", { my_weight: 1 }),
        pathes;

      pathes = v1.pathTo(v2, {
        weight_function: function (edge) {
          return edge.getProperty("my_weight");
        }
      });

      assertEqual(pathes.length, 1);
      assertEqual(pathes[0][0].toString(), v1.getId());
      assertEqual(pathes[0][1].toString(), v3.getId());
      assertEqual(pathes[0][2].toString(), v4.getId());
      assertEqual(pathes[0][3].toString(), v2.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a short, distinct path while excluding certain edges
////////////////////////////////////////////////////////////////////////////////

    testGetAPathWithEdgeExclusion: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        e1 = graph.addEdge(v1, v2, 5, "5", { rating: 3 }),
        e2 = graph.addEdge(v1, v3, 6, "6", { rating: 5 }),
        e3 = graph.addEdge(v3, v4, 7, "7", { rating: 4 }),
        e4 = graph.addEdge(v4, v2, 8, "8", { rating: 6 }),
        pathes;

      pathes = v1.pathTo(v2, {
        only: function (edge) {
          return (edge.getProperty("rating") > 3);
        }
      });

      assertEqual(pathes.length, 1);
      assertEqual(pathes[0][0].toString(), v1.getId());
      assertEqual(pathes[0][1].toString(), v3.getId());
      assertEqual(pathes[0][2].toString(), v4.getId());
      assertEqual(pathes[0][3].toString(), v2.getId());
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Common Neighbors and Attributes
////////////////////////////////////////////////////////////////////////////////

function commonSuite() {
  var Graph = require("org/arangodb/graph").Graph,
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
/// @brief Test CommonNeighborsWith
////////////////////////////////////////////////////////////////////////////////

    testCommonNeighborsWith: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        e1 = graph.addEdge(v1, v3),
        e2 = graph.addEdge(v1, v4),
        e3 = graph.addEdge(v2, v4),
        e4 = graph.addEdge(v2, v5),
        commonNeighbors;

      commonNeighbors = v1.commonNeighborsWith(v2);

      assertEqual(commonNeighbors, 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test normalized CommonNeighborsWith
////////////////////////////////////////////////////////////////////////////////

    testNormalizedCommonNeighborsWith: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        e1 = graph.addEdge(v1, v3),
        e2 = graph.addEdge(v1, v4),
        e3 = graph.addEdge(v2, v4),
        e4 = graph.addEdge(v2, v5),
        commonNeighbors;

      commonNeighbors = v1.commonNeighborsWith(v2, { normalized: true});

      assertEqual(commonNeighbors, (1 / 3));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test listed CommonNeighborsWith
////////////////////////////////////////////////////////////////////////////////

    testListedCommonNeighborsWith: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        e1 = graph.addEdge(v1, v3),
        e2 = graph.addEdge(v1, v4),
        e3 = graph.addEdge(v2, v4),
        e4 = graph.addEdge(v2, v5),
        commonNeighbors;

      commonNeighbors = v1.commonNeighborsWith(v2, { listed: true});

      assertEqual(commonNeighbors.length, 1);
      assertEqual(commonNeighbors[0], v4.getId());
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief Test CommonPropertiesWith
////////////////////////////////////////////////////////////////////////////////

    testCommonPropertiesWith: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        commonProperties;

      v1.setProperty("a", 1);
      v2.setProperty("a", 2);
      v1.setProperty("b", 1);
      v2.setProperty("b", 1);
      v2.setProperty("c", 0);

      commonProperties = v1.commonPropertiesWith(v2);

      assertEqual(commonProperties, 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test Normalized CommonPropertiesWith
////////////////////////////////////////////////////////////////////////////////

    testNormalizedCommonPropertiesWith: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        commonProperties;

      v1.setProperty("a", 1);
      v2.setProperty("a", 2);
      v1.setProperty("b", 1);
      v2.setProperty("b", 1);
      v2.setProperty("c", 0);

      commonProperties = v1.commonPropertiesWith(v2, { normalized: true });

      assertEqual(commonProperties, (1 / 3));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test Listed CommonPropertiesWith
////////////////////////////////////////////////////////////////////////////////

    testListedCommonPropertiesWith: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        commonProperties;

      v1.setProperty("a", 1);
      v2.setProperty("a", 2);
      v1.setProperty("b", 1);
      v2.setProperty("b", 1);
      v2.setProperty("c", 0);

      commonProperties = v1.commonPropertiesWith(v2, { listed: true });

      assertEqual(commonProperties.length, 1);
      assertEqual(commonProperties[0], "b");
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(neighborSuite);
jsunity.run(dijkstraSuite);
jsunity.run(commonSuite);

return jsunity.done();
