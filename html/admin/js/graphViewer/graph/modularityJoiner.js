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

function ModularityJoiner() {
  "use strict";
  
  var 
  // Copy of underscore.js. importScripts doesn't work
    breaker = {},
    nativeForEach = Array.prototype.forEach,
    nativeKeys = Object.keys,
    nativeIsArray = Array.isArray,
    toString = Object.prototype.toString,
    nativeIndexOf = Array.prototype.indexOf,
    nativeMap = Array.prototype.map,
    nativeSome = Array.prototype.some,
    _ = {
      isArray: nativeIsArray || function(obj) {
        return toString.call(obj) === '[object Array]';
      },
      isFunction: function(obj) {
        return typeof obj === 'function';
      },
      isString: function(obj) {
        return toString.call(obj) === '[object String]';
      },
      each: function(obj, iterator, context) {
        if (obj === null || obj === undefined) {
          return;
        }
        var i, l, key;
        if (nativeForEach && obj.forEach === nativeForEach) {
          obj.forEach(iterator, context);
        } else if (obj.length === +obj.length) {
          for (i = 0, l = obj.length; i < l; i++) {
            if (iterator.call(context, obj[i], i, obj) === breaker) {
              return;
            }
          }
        } else {
          for (key in obj) {
            if (obj.hasOwnProperty(key)) {
              if (iterator.call(context, obj[key], key, obj) === breaker) {
                return;
              }
            }
          }
        }
      },
      keys: nativeKeys || function(obj) {
        if (obj !== Object(obj)) {
          throw new TypeError('Invalid object');
        }
        var keys = [], key;
        for (key in obj) {
          if (obj.hasOwnProperty(key)) {
            keys[keys.length] = key;
          }
        }
        return keys;
      },
      min: function(obj, iterator, context) {
        if (!iterator && _.isArray(obj) && obj[0] === +obj[0] && obj.length < 65535) {
          return Math.min.apply(Math, obj);
        }
        if (!iterator && _.isEmpty(obj)) {
          return Infinity;
        }
        var result = {computed : Infinity, value: Infinity};
        _.each(obj, function(value, index, list) {
          var computed = iterator ? iterator.call(context, value, index, list) : value;
          if (computed < result.computed) {
            result = {value : value, computed : computed};
          }
        });
        return result.value;
      },
      map: function(obj, iterator, context) {
        var results = [];
        if (obj === null) {
          return results;
        }
        if (nativeMap && obj.map === nativeMap) {
          return obj.map(iterator, context);
        }
        _.each(obj, function(value, index, list) {
          results[results.length] = iterator.call(context, value, index, list);
        });
        return results;
      },
      pluck: function(obj, key) {
        return _.map(obj, function(value){ return value[key]; });
      },
      uniq: function(array, isSorted, iterator, context) {
        if (_.isFunction(isSorted)) {
          context = iterator;
          iterator = isSorted;
          isSorted = false;
        }
        var initial = iterator ? _.map(array, iterator, context) : array,
          results = [],
          seen = [];
        _.each(initial, function(value, index) {
          if (isSorted) {
            if (!index || seen[seen.length - 1] !== value) {
              seen.push(value);
              results.push(array[index]);
            }
          } else if (!_.contains(seen, value)) {
            seen.push(value);
            results.push(array[index]);
          }
        });
        return results;
      },
      union: function() {
        return _.uniq(Array.prototype.concat.apply(Array.prototype, arguments));
      },
      isEmpty: function(obj) {
        var key;
        if (obj === null) {
          return true;
        }
        if (_.isArray(obj) || _.isString(obj)) {
          return obj.length === 0;
        }
        for (key in obj) {
          if (obj.hasOwnProperty(key)) {
            return false;
          }
        }
        return true;
      },
      any: function(obj, iterator, context) {
        iterator =  iterator || _.identity;
        var result = false;
        if (obj === null) {
          return result;
        }
        if (nativeSome && obj.some === nativeSome) {
          return obj.some(iterator, context);
        }
        _.each(obj, function(value, index, list) {
          if (result) {
            return breaker;
          }
          result = iterator.call(context, value, index, list);
          return breaker;
        });
        return !!result;
      },
      contains: function(obj, target) {
        if (obj === null) {
          return false;
        }
        if (nativeIndexOf && obj.indexOf === nativeIndexOf) {
          return obj.indexOf(target) !== -1;
        }
        return _.any(obj, function(value) {
          return value === target;
        });
      },
      values: function(obj) {
        var values = [], key;
        for (key in obj) {
          if (obj.hasOwnProperty(key)) {
            values.push(obj[key]);
          }
        }
        return values;
      }
    },
    matrix = {},
    backwardMatrix = {},
    degrees = {},
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

    insertEdge = function(s, t) {
      matrix[s] = matrix[s] || {};
      matrix[s][t] = (matrix[s][t] || 0) + 1;
      backwardMatrix[t] = backwardMatrix[t] || {};
      backwardMatrix[t][s] = (backwardMatrix[t][s] || 0) + 1;
      degrees[s] = degrees[s] || {_in: 0, _out:0}; 
      degrees[t] = degrees[t] || {_in: 0, _out:0};
      degrees[s]._out++;
      degrees[t]._in++;
      m++;
      revM = Math.pow(m, -1);
    },
  
    deleteEdge = function(s, t) {
      if (matrix[s]) {
        matrix[s][t]--;
        if (matrix[s][t] === 0) {
          delete matrix[s][t];
        }
        backwardMatrix[t][s]--;
        if (backwardMatrix[t][s] === 0) {
          delete backwardMatrix[t][s];
        }
        degrees[s]._out--;
        degrees[t]._in--;
        m--;
        if (m > 0) {
          revM = Math.pow(m, -1);
        } else {
          revM = 0;
        }
        if (_.isEmpty(matrix[s])) {
          delete matrix[s];
        }
        if (_.isEmpty(backwardMatrix[t])) {
          delete backwardMatrix[t];
        }
        if (degrees[s]._in === 0 && degrees[s]._out === 0) {
          delete degrees[s];
        }
        if (degrees[t]._in === 0 && degrees[t]._out === 0) {
          delete degrees[t];
        }
      } else {
        /*
        self.postMessage({
          cmd: "debug",
          result: "Source not stored",
        });
        */
      }
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
        if (_.contains(obj.nodes, focus)) {
          delete coms[key];
        }
      });
      
      res = _.pluck(_.values(coms), "nodes");
      dist = floatDist(focus);
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
  
  this.insertEdge = insertEdge;
  
  this.deleteEdge = deleteEdge;
  
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
