/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _*/
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
    backwardMatrix = null,
    self = this,
    m = 0,
    revM = 0,
    a = null,
    dQ = null,
    heap = null,
    comms = {},
 
    ////////////////////////////////////
    // Private functions              //
    //////////////////////////////////// 
    
    makeAdjacencyMatrix = function() {
      matrix = {};
      backwardMatrix = {};
      _.each(edges, function (e) {
        var s = e.source._id,
          t = e.target._id;
        matrix[s] = matrix[s] || {};
        matrix[s][t] = (matrix[s][t] || 0) + 1;
        backwardMatrix[t] = backwardMatrix[t] || {};
        backwardMatrix[t][s] = (backwardMatrix[t][s] || 0) + 1;
      });
      m = edges.length;
      revM = 1/m;
      return matrix;
    },
  
    makeInitialDegrees = function() {
      a = {};
      _.each(nodes, function (n) {
        a[n._id] = {
          _in: n._inboundCounter / m,
          _out: n._outboundCounter / m
        };
      });
      return a;
    },
  
    makeInitialDQOld = function() {
      dQ = {};
      _.each(matrix, function(ts, src) {
        _.each(ts, function(tar) {
          if (src < tar) {
            dQ[src] = dQ[src] || {};
            dQ[src][tar] = (dQ[src][tar] || 0) + revM - a[src]._in * a[tar]._out;
            return;
          } 
          if (tar < src) {
            dQ[tar] = dQ[tar] || {};
            dQ[tar][src] = (dQ[tar][src] || 0) + revM - a[src]._in * a[tar]._out;
            return;
          }
        });
      });
      return dQ;
    },
  
    makeInitialDQ = function() {
      dQ = {};
      _.each(nodes, function(n1) {
        var s = n1._id;
        _.each(nodes, function(n2) {
          var t = n2._id,
            ast;
          if (matrix[s] && matrix[s][t]) {
            ast = matrix[s][t];
          } else {
            ast = 0;
          }
          if (s < t) {
            dQ[s] = dQ[s] || {};
            dQ[s][t] = (dQ[s][t] || 0) + ast * revM - a[s]._in * a[t]._out;
            return;
          }
          if (t < s) {
            dQ[t] = dQ[t] || {};
            dQ[t][s] = (dQ[t][s] || 0) + ast * revM - a[s]._in * a[t]._out;
            return;
          }
        });
      });      
    },
  
    setHeapToMaxInList = function(list, id) {
      var maxT,
        maxV = Number.NEGATIVE_INFINITY;
      _.each(list, function(v, t) {
        if (maxV < v) {
          maxV = v;
          maxT = t;
        }
      });
      heap[id] = maxT;
    },
    
    
    makeInitialHeap = function() {
      heap = {};
      _.each(dQ, setHeapToMaxInList);
      return heap;
    },
    
    updateDQAndHeap = function (low, high) {
      _.each(dQ, function (list, s) {
        if (s < low) {
          list[low] += list[high];
          delete list[high];
          if (heap[s] === low || heap[s] === high) {
            setHeapToMaxInList(list, s);
          } else {
            if (dQ[s][heap[s]] < list[low]) {
              heap[s] = low;
            }
          }
          return;
        }
        if (s === low) {
          delete list[high];
          if (_.isEmpty(list)) {
            delete dQ[s];
            delete heap[s];
            return;
          }
          _.each(list, function(v, k) {
            if (k < high) {
              list[k] += dQ[k][high];
              delete dQ[k][high];
              return;
            }
            if (high < k) {
              list[k] += dQ[high][k];
              return;
            }
            return;
          });
          setHeapToMaxInList(list, s);
          return;
        }
        if (_.isEmpty(list)) {
          delete dQ[s];
          delete heap[s];
        }
        return;
      });
      delete dQ[high];
      delete heap[high];
    };
    

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
    if (bestV <= 0) {
      return null;
    }
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
    var s = comm.sID,
      l = comm.lID,
      q = comm.val;
    comms[s] = comms[s] || {nodes: [s], q: 0};
    if (comms[l]) {
      comms[s].nodes = comms[s].nodes.concat(comms[l].nodes);
      comms[s].q += comms[l].q;
      delete comms[l];
    } else {
      comms[s].nodes.push(l);
    }
    comms[s].q += q;
    updateDQAndHeap(s, l);
  };
}
