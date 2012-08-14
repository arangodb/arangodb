/*jslint indent: 2,
         nomen: true,
         maxlen: 80,
         sloppy: true */
/*global require,
    db,
    assertEqual, assertTrue, assertException,
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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the get total, in and out degree of a vertex
////////////////////////////////////////////////////////////////////////////////

    testDistanceTo : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4);

      graph.addEdge(v1, v2);
      graph.addEdge(v1, v3);
      graph.addEdge(v3, v4);
      graph.addEdge(v4, v1);

      assertEqual(v1.distanceTo(v2), 1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the distance between unconnected vertices
////////////////////////////////////////////////////////////////////////////////

    testDistanceToForUnconnectedVertices : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2);

      assertEqual(v1.distanceTo(v2), Infinity);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to get the eccentricity and closeness of a vertex
////////////////////////////////////////////////////////////////////////////////

    testEccentricityAndCloseness : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5);

      graph.addEdge(v1, v2);
      graph.addEdge(v1, v3);
      graph.addEdge(v1, v4);
      graph.addEdge(v4, v5);

      assertEqual(v1.measurement("eccentricity"), 2);
      assertEqual(v1.measurement("closeness"), 5);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to get an unknown measurement for a vertex
////////////////////////////////////////////////////////////////////////////////

    testUnknownMeasurementOnVertex : function () {
      var v1 = graph.addVertex(1);

      assertException(function () {
        v1.measurement("unknown");
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to get diameter and radius for a graph
////////////////////////////////////////////////////////////////////////////////

    testDiameterAndRadius : function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5);

      graph.addEdge(v1, v2);
      graph.addEdge(v1, v3);
      graph.addEdge(v1, v4);
      graph.addEdge(v4, v5);

      assertEqual(graph.measurement("diameter"), 3);
      assertEqual(graph.measurement("radius"), 2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to get an unknown measurement for a graph
////////////////////////////////////////////////////////////////////////////////

    testUnknownMeasurementOnGraph : function () {
      assertException(function () {
        graph.measurement("unknown");
      });
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Geodesic Distances and Betweenness
////////////////////////////////////////////////////////////////////////////////

function geodesicSuite() {
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
/// @brief Test Geodesics
////////////////////////////////////////////////////////////////////////////////

    testGeodesics: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        geodesics;

      graph.addEdge(v1, v2);
      graph.addEdge(v2, v3);
      graph.addEdge(v2, v4);
      graph.addEdge(v3, v4);
      graph.addEdge(v3, v5);
      graph.addEdge(v4, v5);

      geodesics = graph.geodesics().map(function(geodesic) {
        return geodesic.length;
      });
      geodesics.sort();

      assertEqual(geodesics.length, 12);
      assertEqual(geodesics.indexOf(2), 0);
      assertEqual(geodesics.indexOf(3), 6);
      assertEqual(geodesics.indexOf(4), 10);
      assertEqual(geodesics[11], 4);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test Grouped Geodesics
////////////////////////////////////////////////////////////////////////////////

    testGroupedGeodesics: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        geodesics;

      graph.addEdge(v1, v2);
      graph.addEdge(v2, v3);
      graph.addEdge(v2, v4);
      graph.addEdge(v3, v4);
      graph.addEdge(v3, v5);
      graph.addEdge(v4, v5);

      geodesics = graph.geodesics({ grouped: true }).map(function(geodesic) {
        return geodesic.length;
      });
      geodesics.sort();

      assertEqual(geodesics.length, 10);
      assertEqual(geodesics.indexOf(1), 0);
      assertEqual(geodesics.indexOf(2), 8);
      assertEqual(geodesics[9], 2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test Grouped Geodesics with Threshold
////////////////////////////////////////////////////////////////////////////////

    testGroupedGeodesicsWithThreshold: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        options,
        geodesics;

      graph.addEdge(v1, v2);
      graph.addEdge(v2, v3);
      graph.addEdge(v2, v4);
      graph.addEdge(v3, v4);
      graph.addEdge(v3, v5);
      graph.addEdge(v4, v5);

      options = { grouped: true, threshold: true };
      geodesics = graph.geodesics(options).sort();

      assertEqual(geodesics.length, 4);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test Betweenness on Vertices
////////////////////////////////////////////////////////////////////////////////

    testBetweenness: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5);

      graph.addEdge(v1, v2);
      graph.addEdge(v2, v3);
      graph.addEdge(v2, v4);
      graph.addEdge(v3, v4);
      graph.addEdge(v3, v5);
      graph.addEdge(v4, v5);

      assertEqual(v1.measurement('betweenness'), 0);
      assertEqual(v2.measurement('betweenness'), 3);
      assertEqual(v3.measurement('betweenness'), 1);
      assertEqual(v4.measurement('betweenness'), 1);
      assertEqual(v5.measurement('betweenness'), 0);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Normalized Measurements on Graphs
////////////////////////////////////////////////////////////////////////////////

function normalizedSuite() {
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
/// @brief test the normalized closeness of a graph
////////////////////////////////////////////////////////////////////////////////

    testCloseness: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        closeness;

      graph.addEdge(v1, v2);
      graph.addEdge(v2, v3);
      graph.addEdge(v2, v4);
      graph.addEdge(v3, v4);
      graph.addEdge(v3, v5);
      graph.addEdge(v4, v5);

      closeness = graph.normalizedMeasurement("closeness");

      assertEqual(closeness[v1.getId()].toPrecision(1), '0.6');
      assertEqual(closeness[v2.getId()].toPrecision(1), '1');
      assertEqual(closeness[v3.getId()].toPrecision(1), '1');
      assertEqual(closeness[v4.getId()].toPrecision(1), '1');
      assertEqual(closeness[v5.getId()].toPrecision(1), '0.7');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the normalized betweenness of a graph
////////////////////////////////////////////////////////////////////////////////

    testBetweenness: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        betweenness;

      graph.addEdge(v1, v2);
      graph.addEdge(v2, v3);
      graph.addEdge(v2, v4);
      graph.addEdge(v3, v4);
      graph.addEdge(v3, v5);
      graph.addEdge(v4, v5);

      betweenness = graph.normalizedMeasurement("betweenness");

      assertEqual(betweenness[v1.getId()].toPrecision(1), '0');
      assertEqual(betweenness[v2.getId()].toPrecision(1), '1');
      assertEqual(betweenness[v3.getId()].toPrecision(1), '0.3');
      assertEqual(betweenness[v4.getId()].toPrecision(1), '0.3');
      assertEqual(betweenness[v5.getId()].toPrecision(1), '0');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the normalized eccentricity of a graph
////////////////////////////////////////////////////////////////////////////////

    testEccentricity: function () {
      var v1 = graph.addVertex(1),
        v2 = graph.addVertex(2),
        v3 = graph.addVertex(3),
        v4 = graph.addVertex(4),
        v5 = graph.addVertex(5),
        eccentricity;

      graph.addEdge(v1, v2);
      graph.addEdge(v2, v3);
      graph.addEdge(v2, v4);
      graph.addEdge(v3, v4);
      graph.addEdge(v3, v5);
      graph.addEdge(v4, v5);

      eccentricity = graph.normalizedMeasurement("eccentricity");

      assertEqual(eccentricity[v1.getId()].toPrecision(1), '0.7');
      assertEqual(eccentricity[v2.getId()].toPrecision(1), '1');
      assertEqual(eccentricity[v3.getId()].toPrecision(1), '1');
      assertEqual(eccentricity[v4.getId()].toPrecision(1), '1');
      assertEqual(eccentricity[v5.getId()].toPrecision(1), '0.7');
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(measurementSuite);
jsunity.run(geodesicSuite);
jsunity.run(normalizedSuite);

return jsunity.done();
