/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true */
/*global _*/
/* global eventLibrary */

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

// configs:
//  expand: {
//    edges,
//    nodes,
//    startCallback,
//    loadNode,
//    reshapeNodes
//  }
//  
//  nodeEditor: {
//    nodes,
//    adapter,
//    shaper
//  }
//
//  edgeEditor: {
//    edges,
//    adapter,
//    shaper
//  }
//
//
//
function EventLibrary() {
  "use strict";
  
  var self = this;
  
  this.checkExpandConfig = function(config) {
    if (config.startCallback === undefined) {
      throw "A callback to the Start-method has to be defined";
    }
    if (config.adapter === undefined || config.adapter.explore === undefined) {
      throw "An adapter to load data has to be defined";
    }
    if (config.reshapeNodes === undefined) {
      throw "A callback to reshape nodes has to be defined";
    }
    return true;
  };
  
  this.Expand = function (config) {
    self.checkExpandConfig(config);
    var 
      startCallback = config.startCallback,
      explore = config.adapter.explore,
      reshapeNodes = config.reshapeNodes;
    return function(n) {
      explore(n, startCallback);
      reshapeNodes();
      startCallback();
    };
  };
  
  this.checkDragConfig = function (config) {
    if (config.layouter === undefined) {
      throw "A layouter has to be defined";
    }
    if (config.layouter.drag === undefined || !_.isFunction(config.layouter.drag)) {
      throw "The layouter has to offer a drag function";
    }
    return true;
  };
  
  this.Drag = function (config) {
    self.checkDragConfig(config);
    return config.layouter.drag;
  };
  
  this.checkNodeEditorConfig = function (config) {
    if (config.adapter === undefined) {
      throw "An adapter has to be defined";
    }
    if (config.shaper === undefined) {
      throw "A Node Shaper has to be defined";
    }
    return true;
  };
  
  this.checkEdgeEditorConfig = function (config) {
    if (config.adapter === undefined) {
      throw "An adapter has to be defined";
    }
    if (config.shaper === undefined) {
      throw "An Edge Shaper has to be defined";
    }
    return true;
  };
  
  this.InsertNode = function (config) {
    self.checkNodeEditorConfig(config);
    var adapter = config.adapter,
    nodeShaper = config.shaper;
    
    return function(callback) {
      adapter.createNode({}, function(newNode) {
        nodeShaper.reshapeNodes();
        callback(newNode);
      });
    };
  };
  
  this.PatchNode = function (config) {
    self.checkNodeEditorConfig(config);
    var adapter = config.adapter,
    nodeShaper = config.shaper;
    
    return function(nodeToPatch, patchData, callback) {
      adapter.patchNode(nodeToPatch, patchData, function(patchedNode) {
        nodeShaper.reshapeNodes();
        callback(patchedNode);
      });
    };
  };
  
  this.DeleteNode = function (config) {
    self.checkNodeEditorConfig(config);
    var adapter = config.adapter,
    nodeShaper = config.shaper;
    
    return function(nodeToDelete, callback) {
      adapter.deleteNode(nodeToDelete, function() {
        nodeShaper.reshapeNodes();
        callback();
      });
    };
  };
  
  this.InsertEdge = function (config) {
    self.checkEdgeEditorConfig(config);
    var adapter = config.adapter,
    edgeShaper = config.shaper;
    return function(source, target, callback) {
      adapter.createEdge({source: source, target: target}, function(newEdge) {
        edgeShaper.reshapeEdges();
        callback(newEdge);
      });
    };
  };
  
  this.PatchEdge = function (config) {
    self.checkEdgeEditorConfig(config);
    var adapter = config.adapter,
    edgeShaper = config.shaper;
    return function(edgeToPatch, patchData, callback) {
      adapter.patchEdge(edgeToPatch, patchData, function(patchedEdge) {
        edgeShaper.reshapeEdges();
        callback(patchedEdge);
      });
    };
  };
  
  this.DeleteEdge = function (config) {
    self.checkEdgeEditorConfig(config);
    var adapter = config.adapter,
    edgeShaper = config.shaper;
    return function(edgeToDelete, callback) {
      adapter.deleteEdge(edgeToDelete, function() {
        edgeShaper.reshapeEdges();
        callback();
      });
    };
  };
  
}