/*jshint globalstrict:false, strict:false, unused:false */
/*global assertEqual, assertException */

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
  console = require("console");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Get Neighbor Function
////////////////////////////////////////////////////////////////////////////////

function measurementSuite() {
  'use strict';
  var Graph = require("@arangodb/general-graph"),
    graph_name = "UnitTestsCollectionGraph",
    vertex = "UnitTestsCollectionVertex",
    edge = "UnitTestsCollectionEdge",
    graph,
    addVertex = function(key) {
      return graph[vertex].save({_key: String(key)});
    },
    addEdge = function(from, to) {
      return graph[edge].save(from._id, to._id, {});
    };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      try {
        // Drop the graph if it exsits
        if (Graph._exists(graph_name)) {
          Graph._drop(graph_name, true);
        }
        graph = Graph._create(graph_name,
          [Graph._relation(edge, vertex, vertex)]
        );
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
          Graph._drop(graph_name, true);
        }
      } catch (err) {
        console.error("[FAILED] tear-down failed:" + err);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the get total, in and out degree of a vertex
////////////////////////////////////////////////////////////////////////////////

    testGetDegrees : function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3);

      addEdge(v1, v2);
      addEdge(v3, v1);

      /*
      assertEqual(v1.degree(), 2);
      assertEqual(v1.inDegree(), 1);
      assertEqual(v1.outDegree(), 1);
      */
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the order and the size of the graph
////////////////////////////////////////////////////////////////////////////////

    testGetOrderAndSize : function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3);

      addEdge(v1, v2);
      addEdge(v3, v1);

      /*
      assertEqual(graph.order(), 3);
      assertEqual(graph.size(), 2);
      */
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the distance of two vertices
////////////////////////////////////////////////////////////////////////////////

    testDistanceTo : function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4);

      addEdge(v1, v2);
      addEdge(v1, v3);
      addEdge(v3, v4);
      addEdge(v4, v1);

      var dist = graph._distanceTo(v1._id, v2._id);
      assertEqual(dist.length, 1);
      assertEqual(dist[0].distance, 1);
      assertEqual(dist[0].vertex, v2._id);
      assertEqual(dist[0].startVertex, v1._id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the distance between unconnected vertices
////////////////////////////////////////////////////////////////////////////////

    testDistanceToForUnconnectedVertices : function () {
      var v1 = addVertex(1),
        v2 = addVertex(2);

      var dist = graph._distanceTo(v1._id, v2._id);
      assertEqual(dist.length, 0);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to get the eccentricity and closeness of a vertex
////////////////////////////////////////////////////////////////////////////////

    testEccentricityAndCloseness : function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4),
        v5 = addVertex(5);

      addEdge(v1, v2);
      addEdge(v1, v3);
      addEdge(v1, v4);
      addEdge(v4, v5);

      var ecc = graph._absoluteEccentricity(v1._id);
      var clos = graph._absoluteCloseness(v1._id);

      assertEqual(Object.keys(ecc).length, 1);
      assertEqual(ecc[v1._id], 2);
      assertEqual(Object.keys(clos).length, 1);
      assertEqual(clos[v1._id], 5);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test to get diameter and radius for a graph
////////////////////////////////////////////////////////////////////////////////

    testDiameterAndRadius : function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4),
        v5 = addVertex(5);

      addEdge(v1, v2);
      addEdge(v1, v3);
      addEdge(v1, v4);
      addEdge(v4, v5);

      assertEqual(graph._diameter(), 3);
      assertEqual(graph._radius(), 2);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Geodesic Distances and Betweenness
////////////////////////////////////////////////////////////////////////////////

function geodesicSuite() {
  'use strict';
  var Graph = require("@arangodb/general-graph"),
    graph_name = "UnitTestsCollectionGraph",
    vertex = "UnitTestsCollectionVertex",
    edge = "UnitTestsCollectionEdge",
    graph,
    addVertex = function(key) {
      return graph[vertex].save({_key: String(key)});
    },
    addEdge = function(from, to) {
      return graph[edge].save(from._id, to._id, {});
    };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      try {
        // Drop the graph if it exsits
        if (Graph._exists(graph_name)) {
          Graph._drop(graph_name, true);
        }
        graph = Graph._create(graph_name,
          [Graph._relation(edge, vertex, vertex)]
        );
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
          Graph._drop(graph_name, true);
        }
      } catch (err) {
        console.error("[FAILED] tear-down failed:" + err);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test Geodesics
////////////////////////////////////////////////////////////////////////////////

    testGeodesics: function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4),
        v5 = addVertex(5),
        geodesics;

      addEdge(v1, v2);
      addEdge(v2, v3);
      addEdge(v2, v4);
      addEdge(v3, v4);
      addEdge(v3, v5);
      addEdge(v4, v5);

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
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4),
        v5 = addVertex(5),
        geodesics;

      addEdge(v1, v2);
      addEdge(v2, v3);
      addEdge(v2, v4);
      addEdge(v3, v4);
      addEdge(v3, v5);
      addEdge(v4, v5);

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
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4),
        v5 = addVertex(5),
        options,
        geodesics;

      addEdge(v1, v2);
      addEdge(v2, v3);
      addEdge(v2, v4);
      addEdge(v3, v4);
      addEdge(v3, v5);
      addEdge(v4, v5);

      options = { grouped: true, threshold: true };
      geodesics = graph.geodesics(options).sort();

      assertEqual(geodesics.length, 4);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Test Betweenness on Vertices
////////////////////////////////////////////////////////////////////////////////

    testBetweenness: function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4),
        v5 = addVertex(5);

      addEdge(v1, v2);
      addEdge(v2, v3);
      addEdge(v2, v4);
      addEdge(v3, v4);
      addEdge(v3, v5);
      addEdge(v4, v5);

      assertEqual(graph._absoluteBetweenness(v1), 0);
      assertEqual(graph._absoluteBetweenness(v2), 3);
      assertEqual(graph._absoluteBetweenness(v3), 1);
      assertEqual(graph._absoluteBetweenness(v4), 1);
      assertEqual(graph._absoluteBetweenness(v5), 0);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Normalized Measurements on Graphs
////////////////////////////////////////////////////////////////////////////////

function normalizedSuite() {
  'use strict';
  var Graph = require("@arangodb/general-graph"),
    graph_name = "UnitTestsCollectionGraph",
    vertex = "UnitTestsCollectionVertex",
    edge = "UnitTestsCollectionEdge",
    graph,
    addVertex = function(key) {
      return graph[vertex].save({_key: String(key)});
    },
    addEdge = function(from, to) {
      return graph[edge].save(from._id, to._id, {});
    };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      try {
        // Drop the graph if it exsits
        if (Graph._exists(graph_name)) {
          Graph._drop(graph_name, true);
        }
        graph = Graph._create(graph_name,
          [Graph._relation(edge, vertex, vertex)]
        );
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
          Graph._drop(graph_name, true);
        }
      } catch (err) {
        console.error("[FAILED] tear-down failed:" + err);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the normalized closeness of a graph
////////////////////////////////////////////////////////////////////////////////

    testCloseness: function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4),
        v5 = addVertex(5),
        closeness;

      addEdge(v1, v2);
      addEdge(v2, v3);
      addEdge(v2, v4);
      addEdge(v3, v4);
      addEdge(v3, v5);
      addEdge(v4, v5);

      closeness = graph._closeness();

      assertEqual(closeness[v1._id].toPrecision(1), '0.6');
      assertEqual(closeness[v2._id].toPrecision(1), '1');
      assertEqual(closeness[v3._id].toPrecision(1), '1');
      assertEqual(closeness[v4._id].toPrecision(1), '1');
      assertEqual(closeness[v5._id].toPrecision(1), '0.7');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the normalized betweenness of a graph
////////////////////////////////////////////////////////////////////////////////

    testBetweenness: function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4),
        v5 = addVertex(5),
        betweenness;

      addEdge(v1, v2);
      addEdge(v2, v3);
      addEdge(v2, v4);
      addEdge(v3, v4);
      addEdge(v3, v5);
      addEdge(v4, v5);

      betweenness = graph._betweenness();

      assertEqual(betweenness[v1._id].toPrecision(1), '0');
      assertEqual(betweenness[v2._id].toPrecision(1), '1');
      // assertEqual(betweenness[v3._id].toPrecision(1), '0.3');
      // assertEqual(betweenness[v4._id].toPrecision(1), '0.3');
      assertEqual(betweenness[v5._id].toPrecision(1), '0');
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the normalized eccentricity of a graph
////////////////////////////////////////////////////////////////////////////////

    testEccentricity: function () {
      var v1 = addVertex(1),
        v2 = addVertex(2),
        v3 = addVertex(3),
        v4 = addVertex(4),
        v5 = addVertex(5),
        eccentricity;

      addEdge(v1, v2);
      addEdge(v2, v3);
      addEdge(v2, v4);
      addEdge(v3, v4);
      addEdge(v3, v5);
      addEdge(v4, v5);

      eccentricity = graph._eccentricity();
      assertEqual(eccentricity[v1._id].toPrecision(1), '0.7');
      assertEqual(eccentricity[v2._id].toPrecision(1), '1');
      assertEqual(eccentricity[v3._id].toPrecision(1), '1');
      assertEqual(eccentricity[v4._id].toPrecision(1), '1');
      assertEqual(eccentricity[v5._id].toPrecision(1), '0.7');
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(measurementSuite);
// Not supported by general graph module yet.
// jsunity.run(geodesicSuite);
//
jsunity.run(normalizedSuite);

return jsunity.done();
