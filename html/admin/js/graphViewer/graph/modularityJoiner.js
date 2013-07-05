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

function ModularityJoiner(edges) {
  "use strict";

  if (edges === undefined) {
    throw "Edges have to be given.";
  }
  
  var matrix = null,
    backwardMatrix,
    degrees = null,
    m = 0,
    revM = 0,
    a = null,
    dQ = null,
    heap = null,
    isRunning = false,
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
      backwardMatrix = {};
      degrees = {};
      _.each(edges, function (e) {
        var s = e.source._id,
          t = e.target._id;
        matrix[s] = matrix[s] || {};
        matrix[s][t] = (matrix[s][t] || 0) + 1;
        backwardMatrix[t] = backwardMatrix[t] || {};
        backwardMatrix[t][s] = (backwardMatrix[t][s] || 0) + 1;
        degrees[s] = degrees[s] || {_in: 0, _out:0}; 
        degrees[t] = degrees[t] || {_in: 0, _out:0};
        degrees[s]._out++;
        degrees[t]._in++;
      });
      m = edges.length;
      revM = 1/m;
      return matrix;
    },
  
    makeInitialDegrees = function() {
      a = {};
      _.each(degrees, function (n, id) {
        a[id] = {
          _in: n._in / m,
          _out: n._out / m
        };
      });
      return a;
    },
    
    notConnectedPenalty = function(s, t) {
      return a[s]._out * a[t]._in + a[s]._in * a[t]._out;
    },
    
    neighbors = function(sID) {
      var outbound = _.keys(matrix[sID] || {}),
        inbound = _.keys(backwardMatrix[sID] || {});
      return _.union(outbound, inbound);
    },
    
    makeInitialDQ = function() {
      dQ = {};
      _.each(matrix, function(tars, s) {
        var bw = backwardMatrix[s] || {},
          keys = neighbors(s);
        _.each(keys, function(t) {
          var ast = (tars[t] || 0),
            value;
          ast += (bw[t] || 0);
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
    },

  ////////////////////////////////////
  // getters                        //
  ////////////////////////////////////
  
  getAdjacencyMatrix = function() {
    return matrix;
  },
  
  getHeap = function() {
    return heap;
  },
  
  getDQ = function() {
    return dQ;
  },
  
  getDegrees = function() {
    return a;
  },
  
  getCommunities = function() {
    return comms;
  },
  
  getBest = function() {
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
  },
  
  getBestCommunity = function (communities) {
    var bestQ = Number.NEGATIVE_INFINITY,
      bestC;
    _.each(communities, function (obj) {
      if (obj.q > bestQ) {
        bestQ = obj.q;
        bestC = obj.nodes;
      }
    });
    return bestC;
  },
  
  ////////////////////////////////////
  // setup                          //
  ////////////////////////////////////
  
  setup = function() {
    makeAdjacencyMatrix();
    makeInitialDegrees();
    makeInitialDQ();
    makeInitialHeap();
    comms = {};
  },
  
  
  ////////////////////////////////////
  // computation                    //
  ////////////////////////////////////
    
  joinCommunity = function(comm) {
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
  },
  
  //////////////////////////////////////////////
  // Evaluate value of community by distance  //
  //////////////////////////////////////////////
  
  floatDistStep = function(dist, depth, todo) {
    if (todo.length === 0) {
      return true;
    }
    var nextTodo = [];
    _.each(todo, function(t) {
      if (dist[t] !== Number.POSITIVE_INFINITY) {
        return;
      }
      dist[t] = depth;
      nextTodo = nextTodo.concat(neighbors(t));
    });
    return floatDistStep(dist, depth+1, nextTodo);
  },
 
  floatDist = function(sID) {
    var dist = {};
    _.each(matrix, function(u, n) {
      dist[n] = Number.POSITIVE_INFINITY;
    });
    dist[sID] = 0;
    if (floatDistStep(dist, 1, neighbors(sID))) {
      return dist;
    }
    throw "FAIL!";
  },
  
  minDist = function(dist) {
    return function(a) {
      return dist[a];
    };
  },
  
  ////////////////////////////////////
  // Get only the Best Community    //
  ////////////////////////////////////
   
  getCommunity = function(limit, focus) {
    if (isRunning) {
      throw "Still running.";
    }
    isRunning = true;
    var coms = {},
      res = [],
      dist = {},
      best,
      sortByDistance = function (a, b) {
        var d1 = dist[_.min(a,minDist(dist))],
          d2 = dist[_.min(b,minDist(dist))],
          val = d2 - d1;
        if (val === 0) {
          val = coms[b[b.length-1]].q - coms[a[a.length-1]].q;
        }
        return val;
      };
    setup();
    best = getBest();
    while (best !== null) {
      joinCommunity(best);
      best = getBest();
    }
    coms = getCommunities();
    
    if (focus !== undefined) {
      _.each(coms, function(obj, key) {
        if (_.contains(obj.nodes, focus._id)) {
          delete coms[key];
        }
      });
      
      res = _.pluck(_.values(coms), "nodes");
      dist = floatDist(focus._id);
      res.sort(sortByDistance);
      isRunning = false;
      return res[0];
    }
    isRunning = false;
    return getBestCommunity(coms);
  };
  
  ////////////////////////////////////
  // Public functions               //
  ////////////////////////////////////
  
  this.getAdjacencyMatrix = getAdjacencyMatrix;
 
  this.getHeap = getHeap;
 
  this.getDQ = getDQ;
 
  this.getDegrees = getDegrees;
 
  this.getCommunities = getCommunities;
 
  this.getBest = getBest;
  
  this.setup = setup;
  
  this.joinCommunity = joinCommunity;
  
  this.getCommunity = getCommunity;
  
}
