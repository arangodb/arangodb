/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _*/
/*global document, window*/

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

var helper = helper || {};

(function objectsHelper() {
  "use strict";
  
  helper.insertNSimpleNodes = function (nodes, n) {
    var i;
    for (i = 0; i < n; i++) {
      nodes.push({
        _id: i,
        _inboundCounter: 0,
        _outboundCounter: 0,
        position: {
          x: 1,
          y: 1,
          z: 1
        }
      });
    }
  };
  
  helper.createSimpleNodes = function (ids) {
    var nodes = [];
    _.each(ids, function(i) {
      nodes.push({
        _id: i,
        _inboundCounter: 0,
        _outboundCounter: 0,
        position: {
          x: 1,
          y: 1,
          z: 1
        }
      });
    });
    return nodes;
  };
  
  helper.insertSimpleNodes = function (nodes, ids) {
    _.each(ids, function(i) {
      nodes.push({
        _id: i,
        _inboundCounter: 0,
        _outboundCounter: 0,
        position: {
          x: 1,
          y: 1,
          z: 1
        }
      });
    });
  };
  
  helper.createSimpleEdge = function(nodes, s, t) {
    nodes[s]._outboundCounter++;
    nodes[t]._inboundCounter++;
    return {
      source: nodes[s],
      target: nodes[t]
    };
  };
  
  helper.insertClique = function(nodes, edges, toConnect) {
    var i, j;
    for (i = 0; i < toConnect.length - 1; i++) {
      for (j = i + 1; j < toConnect.length; j++) {
        edges.push(helper.createSimpleEdge(nodes, toConnect[i], toConnect[j]));
        edges.push(helper.createSimpleEdge(nodes, toConnect[j], toConnect[i]));
      }
    }
  };
  
}());