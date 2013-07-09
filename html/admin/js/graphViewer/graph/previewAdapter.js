/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, d3, _, console, document, window*/
/*global AbstractAdapter*/
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

function PreviewAdapter(nodes, edges, config) {
  "use strict";
  
  if (nodes === undefined) {
    throw "The nodes have to be given.";
  }
  if (edges === undefined) {
    throw "The edges have to be given.";
  }

  var self = this,
    absAdapter = new AbstractAdapter(nodes, edges, this),
    
    parseConfig = function(config) {
      if (config.width !== undefined) {
        absAdapter.setWidth(config.width);
      }
      if (config.height !== undefined) {
        absAdapter.setHeight(config.height);
      }
    },

    parseResult = function (result, callback) {
      var inserted = {},
        first = result.first;
      first = absAdapter.insertNode(first);
      _.each(result.nodes, function(n) {
        n = absAdapter.insertNode(n);
        inserted[n._id] = n;
      });
      _.each(result.edges, function(e) {
        absAdapter.insertEdge(e);
      });
      delete inserted[first._id];
      absAdapter.checkSizeOfInserted(inserted);
      absAdapter.checkNodeLimit(first);
      if (callback !== undefined && _.isFunction(callback)) {
        callback(first);
      }
    };

  config = config || {};
  
  parseConfig(config);

  self.loadNode = function(nodeId, callback) {
    var ns = [],
      es = [],
      result = {},
      n1 = {
        _id: 1,
        label: "Node 1",
        image: "img/stored.png"
      },
      n2 = {
        _id: 2,
        label: "Node 2"
      },
      n3 = {
        _id: 3,
        label: "Node 3"
      },
      n4 = {
        _id: 4,
        label: "Node 4"
      },
      n5 = {
        _id: 5,
        label: "Node 5"
      },
      e12 = {
        _id: "1-2",
        _from: 1,
        _to: 2,
        label: "Edge 1"
      },
      e13 = {
        _id: "1-3",
        _from: 1,
        _to: 3,
        label: "Edge 2"
      },
      e14 = {
        _id: "1-4",
        _from: 1,
        _to: 4,
        label: "Edge 3"
      },
      e15 = {
        _id: "1-5",
        _from: 1,
        _to: 5,
        label: "Edge 4"
      },
      e23 = {
        _id: "2-3",
        _from: 2,
        _to: 3,
        label: "Edge 5"
      };
      
    ns.push(n1);
    ns.push(n2);
    ns.push(n3);
    ns.push(n4);
    ns.push(n5);
    
    es.push(e12);
    es.push(e13);
    es.push(e14);
    es.push(e15);
    es.push(e23);
    
    result.first = n1;
    result.nodes = ns;
    result.edges = es;
    
    parseResult(result, callback);
  };

  self.explore = absAdapter.explore;

  self.requestCentralityChildren = function(nodeId, callback) {};
  
  self.createEdge = function (edgeToAdd, callback) {
    window.alert("Server-side: createEdge was triggered.");
  };
  
  self.deleteEdge = function (edgeToRemove, callback) {
    window.alert("Server-side: deleteEdge was triggered.");
  };
  
  self.patchEdge = function (edgeToPatch, patchData, callback) {
    window.alert("Server-side: patchEdge was triggered.");
  };
  
  self.createNode = function (nodeToAdd, callback) {
    window.alert("Server-side: createNode was triggered.");
  };
  
  self.deleteNode = function (nodeToRemove, callback) {
    window.alert("Server-side: deleteNode was triggered.");
    window.alert("Server-side: onNodeDelete was triggered.");
  };
  
  self.patchNode = function (nodeToPatch, patchData, callback) {
    window.alert("Server-side: patchNode was triggered.");
  };
    
  self.setNodeLimit = function (pLimit, callback) {
    absAdapter.setNodeLimit(pLimit, callback);
  };
  
  self.setChildLimit = function (pLimit) {
    absAdapter.setChildLimit(pLimit);
  };
  
  self.expandCommunity = function (commNode, callback) {
    absAdapter.expandCommunity(commNode);
    if (callback !== undefined) {
      callback();
    }
  };
  
}