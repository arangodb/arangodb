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
    AvocadoCollection, AvocadoEdgesCollection */

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
/// @brief test suite: Dijkstra (Comparison with Data from Neo4j)
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
/// @brief compare with Neo4j on a network of size 500
////////////////////////////////////////////////////////////////////////////////

    testComparisonWithNeo4j500 : function () {
      var base_path = "js/common/test-data/random500/",
        v1,
        v2,
        e,
        pathes,
        results = [],
        neo4j_results = [],
        single_result = {},
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
          pathes = v1.pathTo(v2);
          single_result = {
            'from'   : v1.getId(),
            'to'     : v2.getId(),
            'path_length' : pathes[0].length,
            'number_of_pathes' : pathes.length
          };
          results.push(single_result);
        }
      });

      v1 = "";
      v2 = "";
      Helper.process(base_path + "neo4j-dijkstra.csv", function (row) {
        counter += 1;
        if (!(v1 === row[0] && v2 === row[row.length - 1])) {
          v1 = row[0];
          v2 = row[row.length - 1];

          single_result = {
            'from'   : v1,
            'to'     : v2,
            'path_length' : row.length
          };
          if (neo4j_results.length > 0) {
            neo4j_results[neo4j_results.length - 1].number_of_pathes = counter;
          }
          counter = 0;
          neo4j_results.push(single_result);
        }
      });
      neo4j_results[0].number_of_pathes += 1;
      neo4j_results[neo4j_results.length - 1].number_of_pathes = counter + 1;

      for (counter = 0; counter < neo4j_results.length; counter += 1) {
        assertEqual(results[counter].from,
          neo4j_results[counter].from);

        assertEqual(results[counter].to,
          neo4j_results[counter].to);

        assertEqual(results[counter].path_length,
          neo4j_results[counter].path_length);

        assertEqual(results[counter].number_of_pathes,
                  neo4j_results[counter].number_of_pathes);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Measurements (Comparison with Gephi)
////////////////////////////////////////////////////////////////////////////////

function centralitySuite() {
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
/// @brief compare with Gephi on a generated network
////////////////////////////////////////////////////////////////////////////////

    testComparisonWithGephi : function () {
      var base_path = "js/common/test-data/gephi/",
        console = require("console"),
        number_of_edges = 0;

      Helper.process(base_path + "vertices.csv", function (row) {
        var vertex = graph.addVertex(row[0], {
          degree       : row[1],
          eccentricity : row[2],
          closeness    : row[3],
          betweenness  : row[4]
        });
      });

      Helper.process(base_path + "edges.csv", function (row) {
        v1 = graph.getVertex(row[0]);
        v2 = graph.getVertex(row[1]);
        e = graph.addEdge(v1, v2);
        number_of_edges += 1;
      });

      graph._vertices.toArray().forEach(function (raw_vertex) {
        var vertex = graph.getVertex(raw_vertex._id);

        assertEqual(vertex.getProperty("degree"), vertex.degree());
        assertEqual(parseFloat(vertex.getProperty("eccentricity")),
                    vertex.measurement("eccentricity"));
        assertEqual(parseFloat(vertex.getProperty("betweenness")),
                    vertex.measurement("betweenness"));
        assertEqual(parseFloat(vertex.getProperty("closeness")),
                    vertex.measurement("closeness") / number_of_edges);
      });
    }
  };
}




////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(dijkstraSuite);
jsunity.run(centralitySuite);

return jsunity.done();
