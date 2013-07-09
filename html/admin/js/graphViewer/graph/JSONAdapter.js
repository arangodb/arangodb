/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, d3, _, console, alert*/
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

function JSONAdapter(jsonPath, nodes, edges, width, height) {
  "use strict";
  
  var self = this,
  initialX = {},
  initialY = {},
  findNode = function(n) {    
    var res = $.grep(nodes, function(e){
      return e._id === n._id;
    });
    if (res.length === 0) {
      return false;
    } 
    if (res.length === 1) {
      return res[0];
    }
    throw "Too many nodes with the same ID, should never happen";
  },
  insertNode = function(data) {
    var node = {
      _data: data,
      _id: data._id,
      children: data.children
    };
    delete data.children;
    initialY.getStart();
    node.x = initialX.getStart();
    node.y = initialY.getStart();
    nodes.push(node);
    node._outboundCounter = 0;
    node._inboundCounter = 0;
    return node;
  },
  
  insertEdge = function(source, target) {
    edges.push({source: source, target: target});
    source._outboundCounter++;
    target._inboundCounter++;
  };
  
  initialX.range = width / 2;
  initialX.start = width / 4;
  initialX.getStart = function () {
    return this.start + Math.random() * this.range;
  };
  
  initialY.range = height / 2;
  initialY.start = height / 4;
  initialY.getStart = function () {
    return this.start + Math.random() * this.range;
  };
  
  self.loadNode = function(nodeId, callback) {
    self.loadNodeFromTreeById(nodeId, callback);
  };
  
  self.loadNodeFromTreeById = function(nodeId, callback) {
    var json = jsonPath + nodeId + ".json";
    d3.json(json, function(error, node) {
      if (error !== undefined && error !== null) {
        console.log(error);
      }
      var n = findNode(node);
      if (!n) {
        n = insertNode(node);
      } else {
        n.children = node.children;
      }
      self.requestCentralityChildren(nodeId, function(c) {
        n._centrality = c;
      });
      _.each(n.children, function(c) {
        var check = findNode(c);
        if (!check) {
          check = insertNode(c);
        }
        insertEdge(n, check);
        self.requestCentralityChildren(check._id, function(c) {
          n._centrality = c;
        });
      });
      if (callback) {
        callback(n);
      }
    });
  };  
  
  self.requestCentralityChildren = function(nodeId, callback) {
    var json = jsonPath + nodeId + ".json";
    d3.json(json, function(error, node) {
      if (error !== undefined && error !== null) {
        console.log(error);
      }
      if (callback !== undefined) {
        if (node.children !== undefined) {
          callback(node.children.length);
        } else {
          callback(0);
        }
      }
    });
  };
  
  self.loadNodeFromTreeByAttributeValue = function(attribute, value, callback) {
    throw "Sorry this adapter is read-only";
  };
  
  self.createEdge = function(edgeToCreate, callback){
      throw "Sorry this adapter is read-only";
  };
  
  self.deleteEdge = function(edgeToDelete, callback){
      throw "Sorry this adapter is read-only";
  };
  
  self.patchEdge = function(edgeToPatch, patchData, callback){
      throw "Sorry this adapter is read-only";
  };
  
  self.createNode = function(nodeToCreate, callback){
      throw "Sorry this adapter is read-only";
  };
  
  self.deleteNode = function(nodeToDelete, callback){
      throw "Sorry this adapter is read-only";
  };
  
  self.patchNode = function(nodeToPatch, patchData, callback){
      throw "Sorry this adapter is read-only";
  };
  
  self.setNodeLimit = function (limit, callback) {
  
  };
  
  self.setChildLimit = function (limit) {
  
  };
  
  self.expandCommunity = function (commNode, callback) {
  
  };
  
  self.explore = function (node, callback) {
  
  };
  
}