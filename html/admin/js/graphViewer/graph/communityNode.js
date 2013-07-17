/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _ */
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



function CommunityNode(initial) {
  "use strict";
  
  initial = initial || [];
  
  var

  ////////////////////////////////////
  // Private variables              //
  ////////////////////////////////////   
    self = this,
    nodes = {},
    internal = {},
    inbound = {},
    outbound = {},
  ////////////////////////////////////
  // Private functions              //
  //////////////////////////////////// 
  
    toArray = function(obj) {
      var res = [];
      _.each(obj, function(v) {
        res.push(v);
      });
      return res;
    },
  
    hasNode = function(id) {
      return !!nodes[id];
    },
  
    getNodes = function() {
      return toArray(nodes);
    },
    
    getNode = function(id) {
      return nodes[id];
    },
  
    insertNode = function(n) {
      nodes[n._id] = n;
      self._size++;
    },
    
    insertInboundEdge = function(e) {
      e._target = e.target;
      e.target = self;
      if (outbound[e._id]) {
        delete outbound[e._id];
        self._outboundCounter--;
        internal[e._id] = e;
        return true;
      }
      inbound[e._id] = e;
      self._inboundCounter++;
      return false;
    },
    
    insertOutboundEdge = function(e) {
      e._source = e.source;
      e.source = self;
      if (inbound[e._id]) {
        delete inbound[e._id];
        self._inboundCounter--;
        internal[e._id] = e;
        return true;
      }
      self._outboundCounter++;
      outbound[e._id] = e;
      return false;
    },
  
    dissolve = function() {
      return {
        nodes: toArray(nodes),
        edges: {
          both: toArray(internal),
          inbound: toArray(inbound),
          outbound: toArray(outbound)
        }
      };
    };
  
  ////////////////////////////////////
  // Setup                          //
  ////////////////////////////////////
  
  ////////////////////////////////////
  // Values required for shaping    //
  ////////////////////////////////////
  this._id = "*community_" + Math.floor(Math.random()* 1000000);
  if (initial.length > 0) {
    this.x = initial[0].x;
    this.y = initial[0].y;
  } else {
    this.x = 0;
    this.y = 0;
  }
  this._size = 0;
  this._inboundCounter = 0;
  this._outboundCounter = 0;
  
  // Easy check for the other classes,
  // no need for a regex on the _id any more.
  this._isCommunity = true;
  
  _.each(initial, function(n) {
    insertNode(n);
  });
  
  ////////////////////////////////////
  // Public functions               //
  ////////////////////////////////////
  
  this.hasNode = hasNode;
  this.getNodes = getNodes;
  this.getNode = getNode;
  this.insertNode = insertNode;
  this.insertInboundEdge = insertInboundEdge;
  this.insertOutboundEdge = insertOutboundEdge;
  this.dissolve = dissolve;
}
