/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, d3, _, console*/
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

function JSONAdapter(jsonPath, nodes, edges) {
  "use strict";
  
  var self = this,
  findNode = function(n) {
    var res = $.grep(nodes, function(e){
      return e.id === n.id;
    });
    if (res.length === 0) {
      return false;
    } 
    if (res.length === 1) {
      return res[0];
    }
    throw "Too many nodes with the same ID, should never happen";
  };
  
  
  self.loadNodeFromTree = function(nodeId, callback) {
    var json = jsonPath + nodeId + ".json";
    d3.json(json, function(error, node) {
      if (error !== undefined && error !== null) {
        console.log(error);
      }
      var n = findNode(node);
      if (!n) {
        nodes.insertNode(node);
        n = node;
      } else {
        n.children = node.children;
      }
      self.requestCentralityChildren(nodeId, function(c) {
        n._centrality = c;
      });
      _.each(n.children, function(c) {
        var check = findNode(c);
        if (check) {
          edges.insertEdge(n, check);
          self.requestCentralityChildren(check.id, function(c) {
            n._centrality = c;
          });
        } else {
          nodes.insertNode(c);
          edges.insertEdge(n, c);
          self.requestCentralityChildren(c.id, function(c) {
            n._centrality = c;
          });
        }
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
      if (callback) {
        callback(node.children.length);
      }
    });
  };
}