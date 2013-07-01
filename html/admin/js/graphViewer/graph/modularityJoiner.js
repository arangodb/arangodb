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
    dQ = null,
    heap = null,
    comms = {},
    
    makeAdjacencyMatrix = function() {
      matrix = {};
      _.each(edges, function (e) {
        matrix[e.source._id] = matrix[e.source._id] || [];
        matrix[e.source._id].push(e.target._id);
      });
      m = edges.length;
      revM = 1/m;
      return matrix;
    },
  
    makeInitialDegrees = function() {
      a = {};
      _.each(nodes, function (n) {
        a[n._id] = {
          in: n._inboundCounter / m,
          out: n._outboundCounter / m
        }
      });
      return a;
    },
  
    makeInitialDQ = function() {
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
    },
  
    makeInitialHeap = function() {
      heap = {};
      _.each(dQ, function(l, s) {
        var maxT,
          maxV = Number.NEGATIVE_INFINITY;
        _.each(l, function(v, t) {
          if (maxV < v) {
            maxV = v;
            maxT = t;
          }
        });
        heap[s] = maxT;
      });
      return heap;
    };
    
  ////////////////////////////////////
  // Private functions              //
  ////////////////////////////////////
  
  
  ////////////////////////////////////
  // Public functions               //
  ////////////////////////////////////
  
  ////////////////////////////////////
  // getters                        //
  ////////////////////////////////////
  
  this.getAdjacencyMatrix = function() {
    return matrix;
  };
  
  this.getHeap = function() {
    return heap;
  };
  
  this.getDQ = function() {
    return dQ;
  };
  
  this.getDegrees = function() {
    return a;
  };
  
  this.getCommunities = function() {
    return comms;
  };
  
  this.getBest = function() {
    var bestL, bestS, bestV = Number.NEGATIVE_INFINITY;
    _.each(heap, function(lID, sID) {
      if (bestV < dQ[sID][lID]) {
        bestL = lID;
        bestS = sID;
        bestV = dQ[sID][lID];
      }
    });
    return {
      sID: bestS,
      lID: bestL,
      val: bestV
    };
  };
  
  
  ////////////////////////////////////
  // setup                          //
  ////////////////////////////////////
  
  this.setup = function() {
    makeAdjacencyMatrix();
    makeInitialDegrees();
    makeInitialDQ();
    makeInitialHeap();
  };
  
  
  ////////////////////////////////////
  // computation                    //
  ////////////////////////////////////
  
  
  
    
  this.joinCommunity = function(comm) {
    
  };
}
