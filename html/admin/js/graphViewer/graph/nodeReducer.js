/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _*/
// Will be injected by WebWorkers
/*global self*/
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

function NodeReducer(nodes, edges) {
  "use strict";
  // We are executed as a worker
  if (this.importScripts) {
    this.importScripts("js/lib/underscore.js");
  }
  
  
  if (nodes === undefined) {
    throw "Nodes have to be given.";
  }
  if (edges === undefined) {
    throw "Edges have to be given.";
  }

  var 
    matrix = null,
    m = 0,
    revM = 0,
    a = null,
    dQ = null,
    heap = null,
    comms = {},
    isRunning = false,
    
    ////////////////////////////////////
    // Private functions              //
    //////////////////////////////////// 
    
    /////////////////////////////////////
    // Functions for Modularity Join   //
    /////////////////////////////////////
    
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
    
    /////////////////////////////
    // Functions for Buckets   //
    /////////////////////////////
    
    neighbors = function(sID) {
      var neigh = [];
      _.each(edges, function(e) {
        if (e.source._id === sID) {
          neigh.push(e.target._id);
          return;
        }
        if (e.target._id === sID) {
          neigh.push(e.source._id);
          return;
        }
      });
      return neigh;
    },
    
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
      _.each(_.pluck(nodes, "_id"), function(n) {
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
    
   addNode = function(bucket, node) {
     bucket.push(node);
   },
   
   getSimilarityValue = function(bucket, node) {
     if (bucket.length === 0) {
       return 1;
     }     
     var comp = bucket[0],
       props = _.union(_.keys(comp), _.keys(node)),
       countMatch = 0,
       propCount = 0;
     _.each(props, function(key) {
       if (comp[key] !== undefined && node[key]!== undefined) {
         countMatch++;
         if (comp[key] === node[key]) {
           countMatch += 4;
         }
       }
     });
     propCount = props.length * 5;
     propCount++;
     countMatch++;
     return countMatch / propCount;
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
    if (nodes.length === 0 || edges.length === 0) {
      isRunning = false;
      throw "Load some nodes first.";
    }
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
  },
  
  bucketNodes = function(toSort, numBuckets) {
    var res = [],
    threshold = 0.5;
    if (toSort.length <= numBuckets) {
      res = _.map(toSort, function(n) {
        return [n];
      });
      return res;
    }
    _.each(toSort, function(n) {
      var i, shortest, sLength;
      shortest = 0;
      sLength = Number.POSITIVE_INFINITY;
      for (i = 0; i < numBuckets; i++) {
        res[i] = res[i] || [];
        if (getSimilarityValue(res[i], n) > threshold) {
          addNode(res[i], n);
          return;
        }
        if (sLength > res[i].length) {
          shortest = i;
          sLength = res[i].length;
        }
      }
      addNode(res[shortest], n);
    });
    return res;
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
  
  this.bucketNodes = bucketNodes;
  
}
