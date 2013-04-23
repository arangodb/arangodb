/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _*/
/*global EventDispatcher, ArangoAdapter, JSONAdapter */
/*global ForceLayouter, EdgeShaper, NodeShaper */
////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
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
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


function GraphViewer(svg, width, height,
  adapterConfig, config) {
  
  "use strict";
  // Check if all required inputs are given
  if (svg === undefined || svg.append === undefined) {
    throw "SVG has to be given and has to be selected using d3.select";
  }
  
  if (width === undefined || width <= 0) {
    throw "A width greater 0 has to be given";
  }
  
  if (height === undefined || height <= 0) {
    throw "A height greater 0 has to be given";
  }
  
  if (adapterConfig === undefined || adapterConfig.type === undefined) {
    throw "An adapter configuration has to be given";
  }
  
  var self = this,
    nodeContainer,
    edgeContainer,
    fixedSize,
    dispatcher,
    edges = [],
    nodes = [],

  parseLayouterConfig = function (config) {
    if (!config) {
      // Default
      config = {};
      config.nodes = nodes;
      config.links = edges;
      config.width = width;
      config.height = height;
      self.layouter = new ForceLayouter(config);
      return;
    }
    switch (config.type.toLowerCase()) {
      case "force":
        config.nodes = nodes;
        config.links = edges;
        config.width = width;
        config.height = height;
        self.layouter = new ForceLayouter(config);
        break;
      default:
        throw "Sorry unknown layout type.";
    }
  },
  parseConfig = function(config) {
    var esConf = config.edgeShaper || {},
      nsConf = config.nodeShaper || {},
      idFunc = nsConf.idfunc || undefined;
    
    parseLayouterConfig(config.layouter);
    edgeContainer = svg.append("svg:g");
    self.edgeShaper = new EdgeShaper(edgeContainer, esConf);
    nodeContainer = svg.append("svg:g");
    self.nodeShaper = new NodeShaper(nodeContainer, nsConf, idFunc);
    self.layouter.setCombinedUpdateFunction(self.nodeShaper, self.edgeShaper);
  };
  
  switch (adapterConfig.type.toLowerCase()) {
    case "arango":
      self.adapter = new ArangoAdapter(
        adapterConfig.host,
        nodes,
        edges,
        adapterConfig.nodeCollection,
        adapterConfig.edgeCollection,
        width,
        height
      );
      break;
    case "json":
      self.adapter = new JSONAdapter(
        adapterConfig.path,
        nodes,
        edges,
        width,
        height
      );
      break;
    default:
      throw "Sorry unknown adapter type.";
  }    
  
  parseConfig(config || {});
    
  self.start = function() {
    self.layouter.stop();
    self.edgeShaper.drawEdges(edges);
    self.nodeShaper.drawNodes(nodes);
    self.layouter.start();
  };
  
  self.loadGraph = function(nodeId) {
    nodes.length = 0;
    edges.length = 0;
    self.adapter.loadNodeFromTreeById(nodeId, function (node) {
      node._expanded = true;
      node.x = width / 2;
      node.y = height / 2;
      node.fixed = true;
      self.start();
    });
  };
  
  self.loadGraphWithAttributeValue = function(attribute, value) {
    nodes.length = 0;
    edges.length = 0;
    self.adapter.loadNodeFromTreeByAttributeValue(attribute, value, function (node) {
      node._expanded = true;
      node.x = width / 2;
      node.y = height / 2;
      node.fixed = true;
      self.start();
    });
  };
  
  self.dispatcherConfig = {
    expand: {
      edges: edges,
      nodes: nodes,
      startCallback: self.start,
      loadNode: self.adapter.loadNodeFromTreeById,
      reshapeNode: self.nodeShaper.reshapeNode
    },
    drag: {
      layouter: self.layouter
    },
    nodeEditor: {
      nodes: nodes,
      adapter: self.adapter
    },
    edgeEditor: {
      edges: edges,
      adapter: self.adapter
    }  
  };  
}