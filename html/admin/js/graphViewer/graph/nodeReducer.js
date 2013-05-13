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
          heap[sID].lID = lID;
        }
      });
   };
    
  self.getCommunity = function(limit, focus) {
    var dQ = {},
      a = {},
      heap = {};
    populateValues(dQ, a, heap);
    
    return [];
  };
}
