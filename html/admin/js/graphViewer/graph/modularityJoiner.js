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
    
    setHeapToMax = function(id) {
      var maxT,
        maxV = Number.NEGATIVE_INFINITY;
      _.each(dQ[id], function(v, t) {
        if (maxV < v) {
          maxV = v;
          maxT = t;
        }
      });
      if (maxV < 0) {
        delete heap[id];
        return;
      }
      heap[id] = maxT;
    },
  
    setHeapToMaxInList = function(l, id) {
      setHeapToMax(id);
    },
    
    isSetDQVal = function(i, j) {
      if (i < j) {
        return dQ[i] && dQ[i][j];
      }
      return dQ[j] && dQ[j][i];
    },
    
    // This does not check if everything exists,
    // do it before!
    getDQVal = function(i, j) {
      if (i < j) {
        return dQ[i][j];
      }
      return dQ[j][i];
    },
    
    setDQVal = function(i, j, v) {
      if (i < j) {
        dQ[i] = dQ[i] || {};
        dQ[i][j] = v;
        return;
      }
      dQ[j] = dQ[j] || {};
      dQ[j][i] = v;
    },
    
    delDQVal = function(i, j) {
      if (i < j) {
        if (!dQ[i]) {
          return;
        }
        delete dQ[i][j];
        if (_.isEmpty(dQ[i])) {
          delete dQ[i];
        }
        return;
      }
      if (i === j) {
        return;
      }
      delDQVal(j, i);
    },
    
    updateHeap = function(i, j) {
      var hv, val;
      if (i < j) {
        if (!isSetDQVal(i, j)) {
          setHeapToMax(i);
          return;
        }
        val = getDQVal(i, j);
        if (heap[i] === j) {
          setHeapToMax(i);
          return;
        }
        if (!isSetDQVal(i, heap[i])) {
          setHeapToMax(i);
          return;
        }
        hv = getDQVal(i, heap[i]);
        if (hv < val) {
          heap[i] = j;
        }
        return;
      }
      if (i === j) {
        return;
      }
      updateHeap(j, i);
    },
    
    updateDegrees = function(low, high) {
      a[low]._in += a[high]._in;
      a[low]._out += a[high]._out;
      delete a[high];
    }, 
    
    makeAdjacencyMatrix = function() {
      matrix = {};
      _.each(edges, function (e) {
        var s = e.source._id,
          t = e.target._id;
        matrix[s] = matrix[s] || {};
        matrix[s][t] = (matrix[s][t] || 0) + 1;
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
    
    makeInitialDQBKP = function() {
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
    
    notConnectedPenalty = function(s, t) {
      return a[s]._out * a[t]._in + a[s]._in * a[t]._out;
    },
    
    
    makeInitialDQ = function() {
      dQ = {};
      _.each(nodes, function(n1) {
        var s = n1._id;
        _.each(nodes, function(n2) {
          var t = n2._id,
            ast = 0,
            value;
          if (t <= s) {
            return;
          }
          if (matrix[s] && matrix[s][t]) {
            ast += matrix[s][t];
          }
          if (matrix[t] && matrix[t][s]) {
            ast += matrix[t][s];
          }
          if (ast === 0) {
            return;
          }
          value = ast * revM - notConnectedPenalty(s, t);
          if (value > 0) {
            setDQVal(s, t, value);
          }
          return;
        });
      });      
    },

    
    
    makeInitialHeap = function() {
      heap = {};
      _.each(dQ, setHeapToMaxInList);
      return heap;
    },
    
    updateDQAndHeapBKP = function (low, high) {
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
              if (dQ[k][high] === undefined) {
                console.log("K:", k, "High:", high, "dQ:", dQ[k]);
                console.log(dQ);
              }
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
    },
    

    // i < j && i != j != k
    updateDQAndHeapValue = function (i, j, k) {
      var val;
      if (isSetDQVal(k, i)) {
        val = getDQVal(k, i);
        if (isSetDQVal(k, j)) {
          val += getDQVal(k, j);
          setDQVal(k, i, val);
          delDQVal(k, j);
          updateHeap(k, i);
          updateHeap(k, j);
          return;
        }
        val -= notConnectedPenalty(k, j);
        if (val < 0) {
          delDQVal(k, i);
        }
        updateHeap(k, i);
        return;
      }
      if (isSetDQVal(k, j)) {
        val = getDQVal(k, j);        
        val -= notConnectedPenalty(k, i);
        if (val > 0) {
          setDQVal(k, i, val);
        }
        updateHeap(k, i);
        delDQVal(k, j);
        updateHeap(k, j);
      }
    },
    
    updateDQAndHeap = function (low, high) {
      _.each(dQ, function (list, s) {
        if (s === low || s === high) {
          _.each(list, function(v, t) {
            if (t === high) {
              delDQVal(low, high);
              updateHeap(low, high);
              return;
            }
            updateDQAndHeapValue(low, high, t);
          });
          return;
        }
        updateDQAndHeapValue(low, high, s);
      });
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
    updateDegrees(s, l);
  };
}
