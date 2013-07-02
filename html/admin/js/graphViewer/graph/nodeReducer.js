/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, d3, _, console, alert, ModularityJoiner*/
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
    isRunning = false,
    joiner = new ModularityJoiner(nodes, edges),
    
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
    

/*   
   dijkstra = function(sID) {
     var dist = {},
       leftOvers, next, neigh,
       filterNotNext = function(e) {
         return e !== next;
       },
       computeNeighbours = function(v) {
         if (_.contains(leftOvers, v)) {
           if (dist[v] > dist[next] + 1) {
             dist[v] = dist[next] + 1;
           }
         }
       };
       
     leftOvers = _.pluck(nodes, "_id");
     _.each(leftOvers, function(n) {
       dist[n] = Number.POSITIVE_INFINITY;
     });
     dist[sID] = 0;
     while(leftOvers.length > 0) {
       next = _.min(leftOvers, minDist(dist));
       if (next === Number.POSITIVE_INFINITY) {
         break;
       }
       leftOvers = leftOvers.filter(filterNotNext);
       neigh = neighbors(next);
       _.each(neigh, computeNeighbours);
     }
     return dist;
   },
   
   */
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
   };
   
  self.getCommunity = function(limit, focus) {
    if (isRunning === true) {
      throw "Still running.";
    }
    isRunning = true;
    var coms = {},
      res = [],
      dist = {},
      best,
      getBest = function (communities) {
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
    joiner.setup();
    best = joiner.getBest();
    while (best !== null) {
      joiner.joinCommunity(best);
      best = joiner.getBest();
    }
    coms = joiner.getCommunities();
    
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
    return getBest(coms);
  };
  
  self.bucketNodes = function(toSort, numBuckets) {
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
  
}
