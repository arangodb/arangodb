/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, d3, _, console, alert*/
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
  
  if (nodes === undefined) {
    throw "Nodes have to be given.";
  }
  if (edges === undefined) {
    throw "Edges have to be given.";
  }
  
  var self = this,
    getDegree = function(id) {
      return $.grep(nodes, function(e){
        return e._id === id;
      })[0];
    },
    
    // Will Overwrite dQ, a and heap!
    populateValues = function(dQ, a, heap) {
      var m = edges.length,
        twoM = 2 * m,
        cFact = 1 / twoM;
      _.each(nodes, function(n) {
        a[n._id] = (n._outboundCounter + n._inboundCounter) / twoM;
      });
      
      _.each(edges, function(e) {
        var sID, lID;
        if (e.source._id < e.target._id) {
          sID = e.source._id;
          lID = e.target._id;
        } else {
          sID = e.target._id;
          lID = e.source._id;
        }
        if (dQ[sID] === undefined) {
          dQ[sID] = {};
          heap[sID] = {};
          heap[sID].val = -1;
        }
        dQ[sID][lID] = cFact - a[sID] * a[lID];
        if (heap[sID].val < dQ[sID][lID]) {
          heap[sID].val = dQ[sID][lID];
          heap[sID].sID = sID;
          heap[sID].lID = lID;
        }
      });
   },
   
   getLargestOnHeap = function(heap) {
     return _.max(heap, function(e) {
       return e.val;
     });
   },
   
   joinCommunities = function(sID, lID, coms, heap, val) {
     coms[sID] = coms[sID] || {com: [sID], q: 0};
     coms[lID] = coms[lID] || {com: [lID], q: 0};
     var old = coms[sID].com,
      newC = coms[lID].com;
     coms[lID].com = old.concat(newC);
     coms[lID].q += coms[sID].q + val;
     delete coms[sID];
   },
   

   getDQValue = function(dQ, a, b) {
     if (a < b) {
       if (dQ[a] !== undefined) {
         return dQ[a][b];
       }
       return undefined;
     }
     if (dQ[b] !== undefined) {
       return dQ[b][a];
     }
     return undefined;
   },
   
   setDQValue = function(dQ, heap, a, b, val) {
     if (a < b) {
       if(dQ[a] === undefined) {
         dQ[a] = {};
         heap[a] = {};
         heap[a].val = -1;
       }
      dQ[a][b] = val;
      if (heap[a].val < val) {
        heap[a].val = val;
        heap[a].sID = a;
        heap[a].lID = b;
      }
     } else {
       if(dQ[b] === undefined) {
         dQ[b] = {};
         heap[b] = {};
         heap[b].val = -1;
       }
      dQ[b][a] = val;
      if (heap[b].val < val) {
        heap[b].val = val;
        heap[b].sID = b;
        heap[b].lID = a;
      }
     }
   },
   
   delDQValue = function(dQ, a, b) {
     if (a < b) {
       delete dQ[a][b];
     } else {
       delete dQ[b][a];
     }
   },
   
   updateValues = function(largest, dQ, a, heap) {
     var lID = largest.lID,
       sID = largest.sID;
     _.each(nodes, function (n) {
       var id = n._id,
         c1, c2;
       if (id === sID || id === lID) {
         return null;
       }
       c1 = getDQValue(dQ, id, sID);
       c2 = getDQValue(dQ, id, lID);
       if (c1 !== undefined) {
         if (c2 !== undefined) {
           setDQValue(dQ, heap, id, lID, c1 + c2);
         } else {
           setDQValue(dQ, heap, id, lID, c1 - 2 * a[lID] * a[id]);
         }
         delDQValue(dQ, id, sID);
       } else {
         if (c2 !== undefined) {
           setDQValue(dQ, heap, id, lID, c2- 2 * a[sID] * a[id]);
         }
       }
     });
     delete dQ[sID];
     delete heap[sID];
     a[lID] += a[sID];
     delete a[sID];
   },
   
   sortBySize = function(a, b) {
     return b.length - a.length;
   },
   
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
   
   minDist = function(dist) {
     return function(a) {
       return dist[a];
     };
   },
   
   dijkstra = function(sID) {
     var dist = {},
       toDo, next, neigh,
       filterNotNext = function(e) {
         return e !== next;
       },
       computeNeighbours = function(v) {
         if (_.contains(toDo, v)) {
           if (dist[v] > dist[next] + 1) {
             dist[v] = dist[next] + 1;
           }
         }
       };
       
     toDo = _.pluck(nodes, "_id");
     _.each(toDo, function(n) {
       dist[n] = Number.POSITIVE_INFINITY;
     });
     dist[sID] = 0;
     while(toDo.length > 0) {
       next = _.min(toDo, minDist(dist));
       if (next === Number.POSITIVE_INFINITY) {
         break;
       }
       toDo = toDo.filter(filterNotNext);
       neigh = neighbors(next);
       _.each(neigh, computeNeighbours);
     }
     return dist;
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
   
   communityDetectionStep = function(dQ, a, heap, coms) {
     //console.log("COMS: " + JSON.stringify(coms));
     //console.log("HEAP: " + JSON.stringify(heap));
     var l = getLargestOnHeap(heap);
     if (l === Number.NEGATIVE_INFINITY || l.val < 0) {
       return false;
     }
     joinCommunities(l.sID, l.lID, coms, heap, l.val);
     updateValues(l, dQ, a, heap);
     return true;
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
           countMatch++;
         }
       }
     });
     propCount = props.length * 2;
     propCount++;
     countMatch++;
     return countMatch / propCount;
   };
    
  self.getCommunity = function(limit, focus) {
    var dQ = {},
      a = {},
      heap = {},
      coms = {},
      res = [],
      dist = {},
      dist2 = {},
      detectSteps = true,
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
      throw "Load some nodes first.";
    }
    populateValues(dQ, a, heap);
    while (detectSteps) {
      detectSteps = communityDetectionStep(dQ, a, heap, coms);
    }
    res = _.pluck(_.values(coms), "com");
    if (focus !== undefined) {
      dist = floatDist(focus._id);
      res = res.filter(function(e) {return !_.contains(e, focus._id);});
      res = res.filter(function(e) {return e.length > 1;});
      res.sort(sortByDistance);
    } else {
      res = res.filter(function(e) {return e.length > 1;});
    }
    return res[0];
  };
  
  self.bucketNodes = function(toSort, numBuckets) {
    var res = [],
    threshold = 0.3;
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
  
}
