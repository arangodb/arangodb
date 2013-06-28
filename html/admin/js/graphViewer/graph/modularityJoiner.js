/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
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

function ModularityJoiner(nodes, edges) {
  "use strict";
  
  if (nodes === undefined) {
    throw "Nodes have to be given.";
  }
  if (edges === undefined) {
    throw "Edges have to be given.";
  }
  
  var matrix = null,
  self = this,
  m = 0,
  revM = 0,
  a = null,
  dQ = null;
  
  
  ////////////////////////////////////
  // Public functions               //
  ////////////////////////////////////
  
  this.makeAdjacencyMatrix = function() {
    matrix = {};
    _.each(edges, function (e) {
      matrix[e.source._id] = matrix[e.source._id] || [];
      matrix[e.source._id].push(e.target._id);
    });
    m = edges.length;
    revM = 1/m;
    return matrix;
  };
  
  this.makeInitialDegrees = function() {
    if (!matrix) {
      self.makeAdjacencyMatrix();
    }
    a = {};
    _.each(nodes, function (n) {
      a[n._id] = {
        in: n._inboundCounter / m,
        out: n._outboundCounter / m
      }
    });
    return a;
  };
  
  this.makeInitialDQ = function() {
    if (!matrix) {
      self.makeAdjacencyMatrix();
    }
    if (!a) {
      self.makeInitialDegrees();
    }
    dQ = {};
    _.each(matrix, function(ts, src) {
      _.each(ts, function(tar) {
        if (src < tar) {
          dQ[src] = dQ[src] || {};
          dQ[src][tar] = (dQ[src][tar] || 0) + revM - a[src].in * a[tar].out;
          return;
        } 
        if (tar < src) {
          dQ[tar] = dQ[tar] || {};
          dQ[tar][src] = (dQ[tar][src] || 0) + revM - a[src].in * a[tar].out;
          return;
        }
      });
    });
    return dQ;
  };
}
