/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");
const examples = require('@arangodb/graph-examples/example-graph');
const gm = require('@arangodb/general-graph');

let api = "/_admin/aardvark/graphs-v2";

////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////

function cleanup() {

  try {
    gm._drop('knows_graph', true);
  } catch (e) {
  }

}

function setupKnowsGraph() {
  cleanup();
  const graph = examples.loadGraph('knows_graph');
  assertTrue(graph !== null);
  assertNotUndefined(db._collection('persons'));
  assertNotUndefined(db._collection('knows'));
  return graph;
}

////////////////////////////////////////////////////////////////////////////////
// Error handling tests
////////////////////////////////////////////////////////////////////////////////
function errorHandlingSuite() {
  return {
    setUp: cleanup,
    tearDown: cleanup,

    test_returns_error_for_non_existent_graph: function() {
      let cmd = api + "/nonexistent_graph";
      let doc = arango.GET_RAW(cmd);
      
      require("internal").print("=== NON-EXISTENT GRAPH ERROR TEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      require("internal").print("Response Body:", JSON.stringify(doc.parsedBody, null, 2));
      
      assertEqual(doc.code, 400); // Fixed: API returns 400, not 404
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 400);
    },

    test_returns_error_for_invalid_path: function() {
      let cmd = api + "/knows_graph/invalid";
      let doc = arango.GET_RAW(cmd);
      
      require("internal").print("=== INVALID PATH ERROR TEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      require("internal").print("Response Body:", JSON.stringify(doc.parsedBody, null, 2));
      
      assertEqual(doc.code, 404);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 404);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
// Basic functionality tests
////////////////////////////////////////////////////////////////////////////////
function basicFunctionalitySuite() {
  return {
    setUp: setupKnowsGraph,
    tearDown: cleanup,

    test_basic_graph_visualization_request: function() {
      let cmd = api + "/knows_graph";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== BASIC REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      require("internal").print("Response Body:", JSON.stringify(doc.parsedBody, null, 2));

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      
      // Basic structure checks
      assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
      assertTrue(doc.parsedBody.hasOwnProperty('edges'));
      assertTrue(doc.parsedBody.hasOwnProperty('settings'));
      assertTrue(Array.isArray(doc.parsedBody.nodes));
      assertTrue(Array.isArray(doc.parsedBody.edges));
      
      // Exact count checks

      // Some vertices have less connections than others, Charlie and Dave are not connected with everyone
      const startVertex = doc.parsedBody.settings.startVertex._key;
      require("internal").print("Start:", startVertex, "Edges: ", doc.parsedBody.edges.length);

      const expectedEdges =  ["charlie", "dave"].indexOf(startVertex) !== -1 ? 4 : 5;
      assertEqual(doc.parsedBody.nodes.length, 5);
      assertEqual(doc.parsedBody.edges.length, expectedEdges, `Expected 4 edges, got ${doc.parsedBody.edges.map(e => e.id)}`);
      
      // Verify all expected node IDs are present
      let nodeIds = doc.parsedBody.nodes.map(n => n.id).sort();
      assertEqual(nodeIds, ["persons/alice", "persons/bob", "persons/charlie", "persons/dave", "persons/eve"]);
      
      // Check node structure for first node
      let sampleNode = doc.parsedBody.nodes[0];
      assertTrue(sampleNode.hasOwnProperty('id'));
      assertTrue(sampleNode.hasOwnProperty('label'));
      assertTrue(sampleNode.hasOwnProperty('size'));
      assertTrue(sampleNode.hasOwnProperty('value'));
      assertTrue(sampleNode.hasOwnProperty('sizeCategory'));
      assertTrue(sampleNode.hasOwnProperty('shape'));
      assertTrue(sampleNode.hasOwnProperty('color'));
      assertTrue(sampleNode.hasOwnProperty('font'));
      assertTrue(sampleNode.hasOwnProperty('title'));
      assertTrue(sampleNode.hasOwnProperty('colorAttributeFound'));
      assertTrue(sampleNode.hasOwnProperty('sortColor'));
      
      assertEqual(sampleNode.size, 20);
      assertEqual(sampleNode.value, 20);
      assertEqual(sampleNode.sizeCategory, "");
      assertEqual(sampleNode.shape, "dot");
      assertEqual(sampleNode.color, "#48BB78");
      assertEqual(sampleNode.colorAttributeFound, false);
      assertEqual(sampleNode.sortColor, "#48BB78");
      
      // Check edge structure
      let sampleEdge = doc.parsedBody.edges[0];
      assertTrue(sampleEdge.hasOwnProperty('id'));
      assertTrue(sampleEdge.hasOwnProperty('source'));
      assertTrue(sampleEdge.hasOwnProperty('from'));
      assertTrue(sampleEdge.hasOwnProperty('label'));
      assertTrue(sampleEdge.hasOwnProperty('target'));
      assertTrue(sampleEdge.hasOwnProperty('to'));
      assertTrue(sampleEdge.hasOwnProperty('color'));
      assertTrue(sampleEdge.hasOwnProperty('font'));
      assertTrue(sampleEdge.hasOwnProperty('length'));
      assertTrue(sampleEdge.hasOwnProperty('size'));
      assertTrue(sampleEdge.hasOwnProperty('colorAttributeFound'));
      assertTrue(sampleEdge.hasOwnProperty('sortColor'));
      
      assertEqual(sampleEdge.label, "");
      assertEqual(sampleEdge.color, "#1D2A12");
      assertEqual(sampleEdge.length, 500);
      assertEqual(sampleEdge.size, 1);
      assertEqual(sampleEdge.colorAttributeFound, false);
      assertEqual(sampleEdge.sortColor, "#1D2A12");
    },

    test_graph_with_depth_parameter: function() {
      let cmd = api + "/knows_graph?depth=1";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== DEPTH=1 REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      require("internal").print("Response Body Keys:", Object.keys(doc.parsedBody));
      require("internal").print("Nodes Count:", doc.parsedBody.nodes ? doc.parsedBody.nodes.length : 'N/A');
      require("internal").print("Edges Count:", doc.parsedBody.edges ? doc.parsedBody.edges.length : 'N/A');

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
      assertTrue(doc.parsedBody.hasOwnProperty('edges'));
      assertTrue(doc.parsedBody.hasOwnProperty('settings'));
      
      // With depth=1, we expect fewer results than default

      const startVertex = doc.parsedBody.settings.startVertex._key;
      require("internal").print("Start:", startVertex, "Nodes: ", doc.parsedBody.nodes.length);

      // Some vertices have less connections than others, Charlie and Dave are not connected with everyone
      let expectedNodes =  3;
      let expectedEdges =  2;
      switch (startVertex) {
        case "bob":
          expectedNodes = 5;
          expectedEdges = 4;
          break;
        case "dave":
        case "charlie":
          expectedNodes = 2;
          expectedEdges = 1;
          break;
      }
      assertEqual(doc.parsedBody.nodes.length, expectedNodes, `Expected ${expectedNodes} nodes, got ${doc.parsedBody.nodes.length} for ${startVertex}`);
      assertEqual(doc.parsedBody.edges.length, expectedEdges, `Expected ${expectedEdges} edges, got ${doc.parsedBody.edges.length} for ${startVertex}`);
    },

    test_graph_with_limit_parameter: function() {
      let cmd = api + "/knows_graph?limit=2";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== LIMIT=2 REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      require("internal").print("Nodes Count:", doc.parsedBody.nodes ? doc.parsedBody.nodes.length : 'N/A');
      require("internal").print("Edges Count:", doc.parsedBody.edges ? doc.parsedBody.edges.length : 'N/A');

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
      assertTrue(doc.parsedBody.hasOwnProperty('edges'));
      
      // With limit=2, we expect limited results
      assertEqual(doc.parsedBody.nodes.length, 2);
      assertEqual(doc.parsedBody.edges.length, 1);
    },

    test_graph_with_nodeStart_parameter: function() {
      // First get a valid node ID
      let basicCmd = api + "/knows_graph";
      let basicDoc = arango.GET_RAW(basicCmd);
      
      if (basicDoc.parsedBody.nodes && basicDoc.parsedBody.nodes.length > 0) {
        let firstNodeId = basicDoc.parsedBody.nodes[0].id;
        let cmd = api + "/knows_graph?nodeStart=" + encodeURIComponent(firstNodeId);
        let doc = arango.GET_RAW(cmd);

        require("internal").print("=== NODE START REQUEST ===");
        require("internal").print("URL:", cmd);
        require("internal").print("Starting Node:", firstNodeId);
        require("internal").print("Response Code:", doc.code);
        require("internal").print("Nodes Count:", doc.parsedBody.nodes ? doc.parsedBody.nodes.length : 'N/A');

        assertEqual(doc.code, 200);
        assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
        assertTrue(doc.parsedBody.hasOwnProperty('edges'));
        assertEqual(doc.parsedBody.nodes.length, 5);
        
        // The starting node should be marked with special properties
        let startingNode = doc.parsedBody.nodes.find(n => n.id === firstNodeId);
        assertTrue(startingNode.hasOwnProperty('borderWidth'));
        assertTrue(startingNode.hasOwnProperty('shadow'));
        assertTrue(startingNode.hasOwnProperty('shapeProperties'));
        assertEqual(startingNode.borderWidth, 4);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
// Configuration parameter tests
////////////////////////////////////////////////////////////////////////////////
function configurationParametersSuite() {
  return {
    setUp: setupKnowsGraph,
    tearDown: cleanup,

    test_node_label_configuration: function() {
      let cmd = api + "/knows_graph?nodeLabel=name";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== NODE LABEL REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      if (doc.parsedBody.nodes && doc.parsedBody.nodes.length > 0) {
        require("internal").print("Sample Node:", JSON.stringify(doc.parsedBody.nodes[0], null, 2));
      }

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
      
      // Check that node labels follow the expected format
      let sampleNode = doc.parsedBody.nodes[0];
      assertTrue(sampleNode.label.startsWith("name: "));
      assertTrue(sampleNode.title.startsWith("name: "));
    },

    test_node_color_configuration: function() {
      let cmd = api + "/knows_graph?nodeColor=ff0000&nodeColorByCollection=true";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== NODE COLOR REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      if (doc.parsedBody.nodes && doc.parsedBody.nodes.length > 0) {
        require("internal").print("Sample Node Color Info:", {
          color: doc.parsedBody.nodes[0].color,
          group: doc.parsedBody.nodes[0].group
        });
      }

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
      
      // When nodeColorByCollection=true, color should be empty and group should be set
      let sampleNode = doc.parsedBody.nodes[0];
      assertEqual(sampleNode.color, "");
      assertEqual(sampleNode.group, "persons");
    },

    test_node_size_configuration: function() {
      let cmd = api + "/knows_graph?nodeSizeByEdges=true";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== NODE SIZE BY EDGES REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      if (doc.parsedBody.nodes && doc.parsedBody.nodes.length > 0) {
        require("internal").print("Sample Node Size Info:", {
          size: doc.parsedBody.nodes[0].size,
          value: doc.parsedBody.nodes[0].value,
          nodeEdgesCount: doc.parsedBody.nodes[0].nodeEdgesCount
        });
      }

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
      
      // When nodeSizeByEdges=true, nodes should have nodeEdgesCount property
      let sampleNode = doc.parsedBody.nodes[0];
      assertTrue(sampleNode.hasOwnProperty('nodeEdgesCount'));
      assertTrue(typeof sampleNode.nodeEdgesCount === 'number');
      assertEqual(sampleNode.value, sampleNode.nodeEdgesCount);
    },

    test_edge_label_configuration: function() {
      let cmd = api + "/knows_graph?edgeLabel=vertex&edgeLabelByCollection=true";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== EDGE LABEL REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      if (doc.parsedBody.edges && doc.parsedBody.edges.length > 0) {
        require("internal").print("Sample Edge:", JSON.stringify(doc.parsedBody.edges[0], null, 2));
      }

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('edges'));
      
      // When edgeLabelByCollection=true, edge labels should include collection name
      let sampleEdge = doc.parsedBody.edges[0];
      assertTrue(sampleEdge.label.includes(" - knows"));
    },

    test_edge_color_configuration: function() {
      let cmd = api + "/knows_graph?edgeColor=00ff00&edgeColorByCollection=true";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== EDGE COLOR REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      if (doc.parsedBody.edges && doc.parsedBody.edges.length > 0) {
        require("internal").print("Sample Edge Color:", doc.parsedBody.edges[0].color);
      }

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('edges'));
      
      // When edgeColorByCollection=true, colors should be from the predefined palette
      let sampleEdge = doc.parsedBody.edges[0];
      assertEqual(sampleEdge.color, "rgba(166, 109, 161, 1)"); // First color from jans palette
    },

    test_layout_configuration: function() {
      let layouts = ['barnesHut', 'hierarchical', 'forceAtlas2'];
      
      layouts.forEach(function(layout) {
        let cmd = api + "/knows_graph?layout=" + layout;
        let doc = arango.GET_RAW(cmd);

        require("internal").print("=== LAYOUT REQUEST: " + layout + " ===");
        require("internal").print("URL:", cmd);
        require("internal").print("Response Code:", doc.code);
        if (doc.parsedBody.settings) {
          require("internal").print("Layout Config:", doc.parsedBody.settings.configlayout);
        }

        assertEqual(doc.code, 200);
        assertTrue(doc.parsedBody.hasOwnProperty('settings'));
        assertEqual(doc.parsedBody.settings.configlayout, layout);
      });
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
// Advanced functionality tests
////////////////////////////////////////////////////////////////////////////////
function advancedFunctionalitySuite() {
  return {
    setUp: setupKnowsGraph,
    tearDown: cleanup,

    test_all_mode: function() {
      let cmd = api + "/knows_graph?mode=all&limit=10";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== ALL MODE REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Response Code:", doc.code);
      require("internal").print("Nodes Count:", doc.parsedBody.nodes ? doc.parsedBody.nodes.length : 'N/A');
      require("internal").print("Edges Count:", doc.parsedBody.edges ? doc.parsedBody.edges.length : 'N/A');

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
      assertTrue(doc.parsedBody.hasOwnProperty('edges'));
      
      // In all mode, we get all vertices and edges up to the limit
      assertEqual(doc.parsedBody.nodes.length, 5);
      assertEqual(doc.parsedBody.edges.length, 5); // All edges in the graph
    },

    test_multiple_start_nodes: function() {
      // Get some node IDs first
      let basicCmd = api + "/knows_graph";
      let basicDoc = arango.GET_RAW(basicCmd);
      
      if (basicDoc.parsedBody.nodes && basicDoc.parsedBody.nodes.length >= 2) {
        let nodeIds = basicDoc.parsedBody.nodes.slice(0, 2).map(n => n.id).join(' ');
        let cmd = api + "/knows_graph?nodeStart=" + encodeURIComponent(nodeIds);
        let doc = arango.GET_RAW(cmd);

        require("internal").print("=== MULTIPLE START NODES REQUEST ===");
        require("internal").print("URL:", cmd);
        require("internal").print("Start Nodes:", nodeIds);
        require("internal").print("Response Code:", doc.code);
        require("internal").print("Nodes Count:", doc.parsedBody.nodes ? doc.parsedBody.nodes.length : 'N/A');

        assertEqual(doc.code, 200);
        assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
        assertEqual(doc.parsedBody.nodes.length, 5);
        
        // Both starting nodes should be marked
        let startNodeIds = nodeIds.split(' ');
        let markedNodes = doc.parsedBody.nodes.filter(n => n.hasOwnProperty('borderWidth'));
        assertEqual(markedNodes.length, 2);
      }
    },

    test_custom_query: function() {
      let customQuery = "FOR v, e, p IN 1..1 ANY 'persons/alice' GRAPH 'knows_graph' RETURN p";
      let cmd = api + "/knows_graph?query=" + encodeURIComponent(customQuery);
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== CUSTOM QUERY REQUEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Custom Query:", customQuery);
      require("internal").print("Response Code:", doc.code);
      if (doc.code !== 200) {
        require("internal").print("Error:", doc.parsedBody);
      } else {
        require("internal").print("Nodes Count:", doc.parsedBody.nodes ? doc.parsedBody.nodes.length : 'N/A');
      }

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
      assertEqual(doc.parsedBody.nodes.length, 3); // alice + connected nodes
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
// Settings and metadata tests
////////////////////////////////////////////////////////////////////////////////
function settingsAndMetadataSuite() {
  return {
    setUp: setupKnowsGraph,
    tearDown: cleanup,

    test_settings_structure: function() {
      let cmd = api + "/knows_graph";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== SETTINGS STRUCTURE TEST ===");
      require("internal").print("Settings Keys:", Object.keys(doc.parsedBody.settings || {}));
      require("internal").print("Settings Content:", JSON.stringify(doc.parsedBody.settings, null, 2));

      assertEqual(doc.code, 200);
      assertTrue(doc.parsedBody.hasOwnProperty('settings'));
      
      let settings = doc.parsedBody.settings;
      
      // Check exact required keys
      let expectedKeys = [
        'nodeColorAttributeMessage',
        'nodeSizeAttributeMessage', 
        'edgeColorAttributeMessage',
        'layout',
        'vertexCollections',
        'edgesCollections',
        'startVertex',
        'nodesColorAttributes',
        'edgesColorAttributes',
        'nodesSizeValues',
        'nodesSizeMinMax',
        'connectionsCounts',
        'connectionsMinMax'
      ];
      
      expectedKeys.forEach(key => {
        assertTrue(settings.hasOwnProperty(key), `Missing key: ${key}`);
      });
      
      assertTrue(Array.isArray(settings.vertexCollections));
      assertTrue(Array.isArray(settings.edgesCollections));
      assertTrue(Array.isArray(settings.nodesColorAttributes));
      assertTrue(Array.isArray(settings.edgesColorAttributes));
      assertTrue(Array.isArray(settings.nodesSizeValues));
      assertTrue(Array.isArray(settings.connectionsCounts));
      
      // Check specific values
      assertEqual(settings.nodeColorAttributeMessage, "");
      assertEqual(settings.nodeSizeAttributeMessage, "");
      assertEqual(settings.edgeColorAttributeMessage, "");
      
      // Check collections structure
      assertEqual(settings.vertexCollections.length, 1);
      assertEqual(settings.vertexCollections[0].name, "persons");
      assertEqual(settings.edgesCollections.length, 1);
      assertEqual(settings.edgesCollections[0].name, "knows");
      
      // Check startVertex structure
      assertTrue(settings.startVertex.hasOwnProperty('_key'));
      assertTrue(settings.startVertex.hasOwnProperty('_id'));
      assertTrue(settings.startVertex.hasOwnProperty('_rev'));
      assertTrue(settings.startVertex.hasOwnProperty('name'));
      assertTrue(settings.startVertex._id.startsWith('persons/'));
    },

    test_node_structure: function() {
      let cmd = api + "/knows_graph";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== NODE STRUCTURE TEST ===");
      if (doc.parsedBody.nodes && doc.parsedBody.nodes.length > 0) {
        let sampleNode = doc.parsedBody.nodes[0];
        require("internal").print("Sample Node Keys:", Object.keys(sampleNode));
        require("internal").print("Sample Node:", JSON.stringify(sampleNode, null, 2));
        
        // Check all required node fields
        let requiredFields = ['id', 'label', 'size', 'value', 'sizeCategory', 'shape', 'color', 'font', 'title', 'colorAttributeFound', 'sortColor'];
        requiredFields.forEach(field => {
          assertTrue(sampleNode.hasOwnProperty(field), `Missing node field: ${field}`);
        });
        
        // Check font structure
        assertTrue(sampleNode.font.hasOwnProperty('multi'));
        assertTrue(sampleNode.font.hasOwnProperty('strokeWidth'));
        assertTrue(sampleNode.font.hasOwnProperty('strokeColor'));
        assertTrue(sampleNode.font.hasOwnProperty('vadjust'));
        assertEqual(sampleNode.font.multi, "html");
        assertEqual(sampleNode.font.strokeWidth, 2);
        assertEqual(sampleNode.font.strokeColor, "#ffffff");
        assertEqual(sampleNode.font.vadjust, -7);
      }

      assertEqual(doc.code, 200);
    },

    test_edge_structure: function() {
      let cmd = api + "/knows_graph";
      let doc = arango.GET_RAW(cmd);

      require("internal").print("=== EDGE STRUCTURE TEST ===");
      if (doc.parsedBody.edges && doc.parsedBody.edges.length > 0) {
        let sampleEdge = doc.parsedBody.edges[0];
        require("internal").print("Sample Edge Keys:", Object.keys(sampleEdge));
        require("internal").print("Sample Edge:", JSON.stringify(sampleEdge, null, 2));
        
        // Check all required edge fields
        let requiredFields = ['id', 'source', 'from', 'label', 'target', 'to', 'color', 'font', 'length', 'size', 'colorAttributeFound', 'sortColor'];
        requiredFields.forEach(field => {
          assertTrue(sampleEdge.hasOwnProperty(field), `Missing edge field: ${field}`);
        });
        
        // Check font structure
        assertTrue(sampleEdge.font.hasOwnProperty('strokeWidth'));
        assertTrue(sampleEdge.font.hasOwnProperty('strokeColor'));
        assertTrue(sampleEdge.font.hasOwnProperty('align'));
        assertEqual(sampleEdge.font.strokeWidth, 2);
        assertEqual(sampleEdge.font.strokeColor, "#ffffff");
        assertEqual(sampleEdge.font.align, "top");
        
        // Check that source equals from and target equals to
        assertEqual(sampleEdge.source, sampleEdge.from);
        assertEqual(sampleEdge.target, sampleEdge.to);
      }

      assertEqual(doc.code, 200);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
// Performance test suite
////////////////////////////////////////////////////////////////////////////////
function performanceTestSuite() {
  const LARGE_STRING = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum. Sed ut perspiciatis unde omnis iste natus error sit voluptatem accusantium doloremque laudantium, totam rem aperiam, eaque ipsa quae ab illo inventore veritatis et quasi architecto beatae vitae dicta sunt explicabo. Nemo enim ipsam voluptatem quia voluptas sit aspernatur aut odit aut fugit, sed quia consequuntur magni dolores eos qui ratione voluptatem sequi nesciunt. Neque porro quisquam est, qui dolorem ipsum quia dolor sit amet, consectetur, adipisci velit, sed quia non numquam eius modi tempora incidunt ut labore et dolore magnam aliquam quaerat voluptatem. Ut enim ad minima veniam, quis nostrum exercitationem ullam corporis suscipit laboriosam, nisi ut aliquid ex ea commodi consequat. Quis autem vel eum iure reprehenderit qui in ea voluptate velit esse quam nihil molestiae consequatur, vel illum qui dolorem eum fugiat quo voluptas nulla pariatur. At vero eos et accusamus et iusto odio dignissimos ducimus qui blanditiis praesentium voluptatum deleniti atque corrupti quos dolores et quas molestias excepturi sint occaecati cupiditate non provident, similique sunt in culpa qui officia deserunt mollitia animi, id est laborum et dolorum fuga.";

  function cleanupPerformanceTest() {
    try {
      gm._drop('performance_graph');
      db._graphs.remove('performance_graph');
    } catch (e) {
    }
    try {
      db._drop('perf_vertices');
    } catch (e) {
    }
    try {
      db._drop('perf_edges');
    } catch (e) {
    }
  }

  function setupPerformanceGraph() {
    cleanupPerformanceTest();
    
    // Create collections
    const vertices = db._create('perf_vertices');
    const edges = db._createEdgeCollection('perf_edges');
    
    // Create graph
    gm._create('performance_graph', [
      gm._relation('perf_edges', 'perf_vertices', 'perf_vertices')
    ],[], {numberOfShards: 5});
    
    // Prepare all vertices for batch insert
    const allVertices = [];
    const rootKey = 'root';
    
    // Add root vertex
    allVertices.push({
      _key: rootKey,
      name: 'Root Node',
      attr1: LARGE_STRING,
      attr2: LARGE_STRING,
      attr3: LARGE_STRING,
      attr4: LARGE_STRING,
      attr5: LARGE_STRING
    });
    
    // Add depth 1 vertices (20 children of root)
    const depth1Keys = [];
    for (let i = 0; i < 20; i++) {
      const key = `depth1_${i}`;
      depth1Keys.push(key);
      allVertices.push({
        _key: key,
        name: `Depth 1 Node ${i}`,
        attr1: LARGE_STRING,
        attr2: LARGE_STRING,
        attr3: LARGE_STRING,
        attr4: LARGE_STRING,
        attr5: LARGE_STRING
      });
    }
    
    // Add depth 2 vertices (20 children for each depth 1 node)
    for (let i = 0; i < 20; i++) {
      for (let j = 0; j < 20; j++) {
        const key = `depth2_${i}_${j}`;
        allVertices.push({
          _key: key,
          name: `Depth 2 Node ${i}-${j}`,
          attr1: LARGE_STRING,
          attr2: LARGE_STRING,
          attr3: LARGE_STRING,
          attr4: LARGE_STRING,
          attr5: LARGE_STRING
        });
      }
    }
    
    // Batch insert all vertices
    vertices.insert(allVertices);
    
    // Prepare all edges for batch insert
    const allEdges = [];
    
    // Add edges from root to depth 1 nodes
    for (let i = 0; i < 20; i++) {
      const key = depth1Keys[i];
      allEdges.push({
        _from: `perf_vertices/${rootKey}`,
        _to: `perf_vertices/${key}`,
        relation: `root_to_${key}`,
        edge_attr1: LARGE_STRING,
        edge_attr2: LARGE_STRING,
        edge_attr3: LARGE_STRING,
        edge_attr4: LARGE_STRING,
        edge_attr5: LARGE_STRING
      });
    }
    
    // Add edges from depth 1 to depth 2 nodes
    for (let i = 0; i < 20; i++) {
      const parentKey = depth1Keys[i];
      for (let j = 0; j < 20; j++) {
        const key = `depth2_${i}_${j}`;
        allEdges.push({
          _from: `perf_vertices/${parentKey}`,
          _to: `perf_vertices/${key}`,
          relation: `${parentKey}_to_${key}`,
          edge_attr1: LARGE_STRING,
          edge_attr2: LARGE_STRING,
          edge_attr3: LARGE_STRING,
          edge_attr4: LARGE_STRING,
          edge_attr5: LARGE_STRING
        });
      }
    }
    
    // Batch insert all edges
    edges.insert(allEdges);
    
    require("internal").print("=== PERFORMANCE GRAPH CREATED ===");
    require("internal").print("Total vertices:", vertices.count());
    require("internal").print("Total edges:", edges.count());
    require("internal").print("Root vertex size (approx):", JSON.stringify(vertices.document(rootKey)).length, "bytes");
  }

  return {
    setUp: setupPerformanceGraph,
    tearDown: cleanupPerformanceTest,

    test_large_tree_performance: function() {
      const startTime = Date.now();
      
      let cmd = api + "/performance_graph?nodeStart=perf_vertices/root";
      let doc = arango.GET_RAW(cmd);

      const endTime = Date.now();
      const executionTime = endTime - startTime;

      require("internal").print("=== LARGE TREE PERFORMANCE TEST ===");
      require("internal").print("URL:", cmd);
      require("internal").print("Execution time:", executionTime, "ms");
      require("internal").print("Response Code:", doc.code);
      require("internal").print("Response size (approx):", JSON.stringify(doc.parsedBody).length, "bytes");
      require("internal").print("Nodes Count:", doc.parsedBody.nodes ? doc.parsedBody.nodes.length : 'N/A');
      require("internal").print("Edges Count:", doc.parsedBody.edges ? doc.parsedBody.edges.length : 'N/A');

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      
      // Basic structure checks
      assertTrue(doc.parsedBody.hasOwnProperty('nodes'));
      assertTrue(doc.parsedBody.hasOwnProperty('edges'));
      assertTrue(doc.parsedBody.hasOwnProperty('settings'));
      assertTrue(Array.isArray(doc.parsedBody.nodes));
      assertTrue(Array.isArray(doc.parsedBody.edges));
      
      // Assert exact counts for the tree structure:
      // 1 root + 20 depth1 + 400 depth2 = 421 vertices
      // 20 root->depth1 + 400 depth1->depth2 = 420 edges
      assertEqual(doc.parsedBody.nodes.length, 421, "Expected 421 nodes (1 root + 20 depth1 + 400 depth2)");
      assertEqual(doc.parsedBody.edges.length, 420, "Expected 420 edges (20 root->depth1 + 400 depth1->depth2)");
      
      // Verify root node is properly marked as start vertex
      let rootNode = doc.parsedBody.nodes.find(n => n.id === 'perf_vertices/root');
      assertTrue(rootNode !== undefined, "Root node should be present");
      assertTrue(rootNode.hasOwnProperty('borderWidth'), "Root node should have borderWidth");
      assertTrue(rootNode.hasOwnProperty('shadow'), "Root node should have shadow");
      assertEqual(rootNode.borderWidth, 4, "Root node should have border width of 4");
      
      // Verify node structure contains our large attributes (indirectly through label/title)
      assertTrue(rootNode.hasOwnProperty('label'));
      assertTrue(rootNode.hasOwnProperty('title'));
      
      // Verify we have the expected vertex collections in settings
      assertTrue(doc.parsedBody.settings.hasOwnProperty('vertexCollections'));
      assertEqual(doc.parsedBody.settings.vertexCollections.length, 1);
      assertEqual(doc.parsedBody.settings.vertexCollections[0].name, "perf_vertices");
      
      // Verify we have the expected edge collections in settings
      assertTrue(doc.parsedBody.settings.hasOwnProperty('edgesCollections'));
      assertEqual(doc.parsedBody.settings.edgesCollections.length, 1);
      assertEqual(doc.parsedBody.settings.edgesCollections[0].name, "perf_edges");
      
      // Verify start vertex is correctly set
      assertEqual(doc.parsedBody.settings.startVertex._id, "perf_vertices/root");
      assertEqual(doc.parsedBody.settings.startVertex._key, "root");
      
      require("internal").print("=== PERFORMANCE TEST COMPLETED SUCCESSFULLY ===");
      require("internal").print("All", doc.parsedBody.nodes.length, "vertices and", doc.parsedBody.edges.length, "edges returned correctly");
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
// Run all test suites
////////////////////////////////////////////////////////////////////////////////
jsunity.run(errorHandlingSuite);
jsunity.run(basicFunctionalitySuite);
jsunity.run(configurationParametersSuite);
jsunity.run(advancedFunctionalitySuite);
jsunity.run(settingsAndMetadataSuite);
jsunity.run(performanceTestSuite);

return jsunity.done(); 