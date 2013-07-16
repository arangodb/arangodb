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
  
  var

  ////////////////////////////////////
  // Private variables              //
  ////////////////////////////////////   
    self = this,
    nodes = {},
  
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
    
    insertEdge = function(e) {
    
    },
  
    dissolve = function() {
    
    };
  
  ////////////////////////////////////
  // Setup                          //
  ////////////////////////////////////
  
  ////////////////////////////////////
  // Values required for shaping    //
  ////////////////////////////////////
  if (initial.length > 0) {
    this.x = initial[0].position.x;
    this.y = initial[0].position.y;
  } else {
    this.x = 0;
    this.y = 0;
  }
  this._size = 0;
  
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
  this.insertEdge = insertEdge;
  this.dissolve = dissolve;
  

  
  
}
