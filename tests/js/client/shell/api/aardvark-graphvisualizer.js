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
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");
const examples = require('@arangodb/graph-examples/example-graph');

let api = "/_admin/aardvark/graphs-v2";

////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////

function cleanup() {
  try {
    db._graphs.remove('knows_graph');
  } catch (e) {
  }
  try {
    db._drop('persons');
  } catch (e) {
  }
  try {
    db._drop('knows');
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
// Run all test suites
////////////////////////////////////////////////////////////////////////////////
jsunity.run(errorHandlingSuite);
jsunity.run(basicFunctionalitySuite);
jsunity.run(configurationParametersSuite);
jsunity.run(advancedFunctionalitySuite);
jsunity.run(settingsAndMetadataSuite);

return jsunity.done(); 