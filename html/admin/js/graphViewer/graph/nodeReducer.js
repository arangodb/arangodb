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
   
   joinCommunities = function(sID, lID, coms) {
     var old = coms[sID] || [sID],
       newC = coms[lID] || [lID];
     coms[lID] = old.concat(newC);
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
       var id = n._id;
       if (id == sID || id == lID) {
         return null;
       }
       var c1 = getDQValue(dQ, id, sID),
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
   
   communityDetectionStep = function(dQ, a, heap, coms) {
     var l = getLargestOnHeap(heap);
     if (l.val < 0) {
       return false;
     }
     joinCommunities(l.sID, l.lID, coms);
     updateValues(l, dQ, a, heap);
     return true;
   };
   
   
    
  self.getCommunity = function(limit, focus) {
    var dQ = {},
      a = {},
      heap = {},
      q = 0,
      coms = {},
      res = [];
    if (nodes.length === 0 || edges.length === 0) {
      throw "Load some nodes first.";
    }
    populateValues(dQ, a, heap);
    while (communityDetectionStep(dQ, a, heap, coms)) {
    }
    res = _.values(coms);
    res.sort(function(a, b) {
      return b.length - a.length;
    });
    if (_.contains(res[0], focus)) {
      return res[1];
    }
    return res[0];
  };
}
