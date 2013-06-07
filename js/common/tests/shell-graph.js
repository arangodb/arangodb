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

var arangodb = require("org/arangodb");
var console = require("console");

var ArangoCollection = arangodb.ArangoCollection;
var print = arangodb.print;

// -----------------------------------------------------------------------------
// --SECTION--                                                      graph module
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Graph Creation
////////////////////////////////////////////////////////////////////////////////

function GraphCreationSuite() {

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Graph Creation
////////////////////////////////////////////////////////////////////////////////

    testCreation : function () {
      var Graph = require("org/arangodb/graph").Graph,
        graph_name = "UnitTestsCollectionGraph",
        vertex = "UnitTestsCollectionVertex",
        edge = "UnitTestsCollectionEdge",
        graph = null;

      graph = new Graph(graph_name, vertex, edge);

      assertEqual(graph_name, graph._properties._key);
      assertTrue(graph._vertices.type() == ArangoCollection.TYPE_DOCUMENT);
      assertTrue(graph._edges.type() == ArangoCollection.TYPE_EDGE);

      graph.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Find Graph
////////////////////////////////////////////////////////////////////////////////

    testFindGraph : function () {
      var Graph = require("org/arangodb/graph").Graph,
        graph_name = "UnitTestsCollectionGraph",
        vertex = "UnitTestsCollectionVertex",
        edge = "UnitTestsCollectionEdge",
        graph1 = null,
        graph2 = null;

      graph1 = new Graph(graph_name, vertex, edge);
      graph2 = new Graph(graph_name);

      assertEqual(graph1._properties.name, graph2._properties.name);
      assertEqual(graph1._vertices._id, graph2._vertices._id);
      assertEqual(graph1._edges._id, graph2._edges._id);

      graph1.drop();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Find Graph
////////////////////////////////////////////////////////////////////////////////

    testCreateGraph : function () {
      var Graph = require("org/arangodb/graph").Graph,
        graph_name = "UnitTestsCollectionGraph",
        vertex = "UnitTestsCollectionVertex",
        edge = "UnitTestsCollectionEdge",
        one = null,
        two = null,
        graph = null;

      graph = new Graph(graph_name, vertex, edge);
      one = graph.addVertex("one");
      two = graph.addVertex("two");
      graph.addEdge(one, two);
      graph.drop();
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Graph Basics
////////////////////////////////////////////////////////////////////////////////

function GraphBasicsSuite() {
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
          graph = new Graph(graph_name);
          print("FOUND: ");
          printObject(graph);
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
/// @brief create a vertex without data
////////////////////////////////////////////////////////////////////////////////

    testCreateVertexWithoutData : function () {
      var v = graph.addVertex("name1");

      assertEqual("name1", v.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vertex
////////////////////////////////////////////////////////////////////////////////

    testCreateVertex : function () {
      var v = graph.addVertex("name1", { age : 23 });
      assertEqual("name1", v.getId());
      assertEqual(23, v.getProperty("age"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get a vertex
////////////////////////////////////////////////////////////////////////////////

    testGetVertex : function () {
      var v1 = graph.addVertex("find_me", { age : 23 }),
        vid,
        v2;

      vid = v1.getId();
      v2 = graph.getVertex(vid);
      assertEqual(23, v2.getProperty("age"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief change a property
////////////////////////////////////////////////////////////////////////////////

    testChangeProperty : function () {
      var v = graph.addVertex("name2", { age : 32 });

      assertEqual("name2", v.getId());
      assertEqual(32, v.getProperty("age"));

      v.setProperty("age", 23);

      assertEqual("name2", v.getId());
      assertEqual(23, v.getProperty("age"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief change a property
////////////////////////////////////////////////////////////////////////////////

    testAddEdgeWithoutInfo : function () {
      var v1,
        v2,
        edge;

      v1 = graph.addVertex("vertex1");
      v2 = graph.addVertex("vertex2");

      edge = graph.addEdge(v1, v2);

      assertEqual(edge._properties._key, edge.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief change a property
////////////////////////////////////////////////////////////////////////////////

    testAddEdge : function () {
      var v1,
        v2,
        edge;

      v1 = graph.addVertex("vertex1");
      v2 = graph.addVertex("vertex2");

      edge = graph.addEdge(v1,
        v2,
        "edge1",
        "label",
        { testProperty: "testValue" });

      assertEqual("edge1", edge.getId());
      assertEqual("label", edge.getLabel());
      assertEqual("testValue", edge.getProperty("testProperty"));
    },

    testAddEdgeWithLabelSetViaData : function () {
      var v1,
        v2,
        edge;

      v1 = graph.addVertex("vertex1");
      v2 = graph.addVertex("vertex2");

      edge = graph.addEdge(v1, v2, null, null, {"$label": "test"});

      assertEqual("test", edge.getLabel());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief change a property
////////////////////////////////////////////////////////////////////////////////

    testGetEdges : function () {
      var v1,
        v2,
        edge1,
        edge2;

      v1 = graph.addVertex("vertex1");
      v2 = graph.addVertex("vertex2");

      edge1 = graph.addEdge(v1,
        v2,
        "edge1",
        "label",
        { testProperty: "testValue" });

      edge2 = graph.getEdges().next();
      assertEqual(true, graph.getEdges().hasNext());
      assertEqual(edge1.getId(), edge2.getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove an edge
////////////////////////////////////////////////////////////////////////////////

    testRemoveEdges : function () {
      var v1,
        v2,
        edge;

      v1 = graph.addVertex("vertex1");
      v2 = graph.addVertex("vertex2");

      edge = graph.addEdge(v1,
        v2,
        "edge1",
        "label",
        { testProperty: "testValue" });

      graph.removeEdge(edge);
      assertEqual(false, graph.getEdges().hasNext());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief remove a vertex
////////////////////////////////////////////////////////////////////////////////

    testRemoveVertex : function () {
      var v1,
        v1_id,
        v2,
        edge;

      v1 = graph.addVertex("vertex1");
      v1_id = v1.getId();
      v2 = graph.addVertex("vertex2");

      edge = graph.addEdge(v1,
        v2,
        "edge1",
        "label",
        { testProperty: "testValue" });

      graph.removeVertex(v1);

      assertEqual(null, graph.getVertex(v1_id));
      assertEqual(false, graph.getEdges().hasNext());
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Vertex
////////////////////////////////////////////////////////////////////////////////

function VertexSuite() {
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
          graph = new Graph(graph_name);
          print("FOUND: ");
          printObject(graph);
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
/// @brief add edges
////////////////////////////////////////////////////////////////////////////////

    testAddEdges : function () {
      var v1,
        v2,
        v3,
        edge1,
        edge2;

      v1 = graph.addVertex();
      v2 = graph.addVertex();
      v3 = graph.addVertex();

      edge1 = v1.addInEdge(v2);
      edge2 = v1.addOutEdge(v3);

      assertEqual(v1.getId(), edge1.getInVertex().getId());
      assertEqual(v2.getId(), edge1.getOutVertex().getId());
      assertEqual(v3.getId(), edge2.getInVertex().getId());
      assertEqual(v1.getId(), edge2.getOutVertex().getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get edges
////////////////////////////////////////////////////////////////////////////////

    testGetEdges : function () {
      var v1,
        v2,
        edge;

      v1 = graph.addVertex();
      v2 = graph.addVertex();

      edge = graph.addEdge(v1, v2);
      
      assertEqual(edge.getId(), v1.getOutEdges()[0].getId());
      assertEqual(edge.getId(), v2.getInEdges()[0].getId());
      assertEqual(0, v1.getInEdges().length);
      assertEqual(0, v2.getOutEdges().length);
      assertEqual(edge.getId(), v1.edges()[0].getId());
      assertEqual(edge.getId(), v2.edges()[0].getId());
      assertEqual(1, v1.getEdges().length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get edges with labels
////////////////////////////////////////////////////////////////////////////////

    testGetEdgesWithLabels : function () {
      var v1,
        v2,
        edge1,
        edge2;

      v1 = graph.addVertex();
      v2 = graph.addVertex();

      edge1 = graph.addEdge(v1, v2, null, "label_1");
      edge2 = graph.addEdge(v1, v2, null, "label_2");

      assertEqual(edge2.getId(), v1.getOutEdges("label_2")[0].getId());
      assertEqual(1, v2.getInEdges("label_2").length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief properties
////////////////////////////////////////////////////////////////////////////////

    testProperties : function () {
      var v1;

      v1 = graph.addVertex();

      v1.setProperty("myProperty", "myValue");
      assertEqual("myValue", v1.getProperty("myProperty"));
      assertEqual("myProperty", v1.getPropertyKeys()[0]);
      assertEqual(1, v1.getPropertyKeys().length);
      assertEqual({myProperty: "myValue"}, v1.properties());
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Edges
////////////////////////////////////////////////////////////////////////////////

function EdgeSuite() {
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
          graph = new Graph(graph_name);
          print("FOUND: ");
          printObject(graph);
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
/// @brief get Vertices
////////////////////////////////////////////////////////////////////////////////

    testGetVertices : function () {
      var v1,
        v2,
        edge;

      v1 = graph.addVertex();
      v2 = graph.addVertex();
      edge = graph.addEdge(v1, v2);

      assertEqual(v1.getId(), edge.getOutVertex().getId());
      assertEqual(v2.getId(), edge.getInVertex().getId());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief get Vertices
////////////////////////////////////////////////////////////////////////////////

    testGetLabel : function () {
      var v1,
        v2,
        edge;

      v1 = graph.addVertex();
      v2 = graph.addVertex();
      edge = graph.addEdge(v1, v2, null, "my_label");

      assertEqual("my_label", edge.getLabel());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief Properties
////////////////////////////////////////////////////////////////////////////////

    testProperties : function () {
      var v1,
        v2,
        edge,
        properties;

      v1 = graph.addVertex();
      v2 = graph.addVertex();
      properties = { myProperty: "myValue"};
      edge = graph.addEdge(v1, v2, null, "my_label", properties);

      assertEqual(properties, edge.properties());
      assertEqual("myValue", edge.getProperty("myProperty"));
      edge.setProperty("foo", "bar");
      assertEqual("bar", edge.getProperty("foo"));
      assertEqual(["foo", "myProperty"], edge.getPropertyKeys());
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(GraphCreationSuite);
jsunity.run(GraphBasicsSuite);
jsunity.run(VertexSuite);
jsunity.run(EdgeSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
