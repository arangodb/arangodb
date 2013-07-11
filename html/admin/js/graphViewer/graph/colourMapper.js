/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _, $, d3*/
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

function ColourMapper() {
  "use strict";
  
  var mapCreated = false,
  mapping = {},
  reverseMapping = {},
  colours = [],
  listener,
  self = this,
  nextColour = 0;
  
  colours.push({back: "navy", front: "white"});
  colours.push({back: "green", front: "white"});
  colours.push({back: "gold", front: "black"});
  colours.push({back: "red", front: "black"});
  colours.push({back: "saddlebrown", front: "white"});
  colours.push({back: "skyblue", front: "black"});
  colours.push({back: "olive", front: "black"});
  colours.push({back: "deeppink", front: "black"});
  colours.push({back: "orange", front: "black"});
  colours.push({back: "silver", front: "black"});
  colours.push({back: "blue", front: "white"});
  colours.push({back: "yellowgreen", front: "black"});
  colours.push({back: "firebrick", front: "black"});
  colours.push({back: "rosybrown", front: "black"});
  colours.push({back: "hotpink", front: "black"});
  colours.push({back: "purple", front: "white"});
  colours.push({back: "cyan", front: "black"});
  colours.push({back: "teal", front: "black"});
  colours.push({back: "peru", front: "black"});
  colours.push({back: "maroon", front: "white"});
  
  this.getColour = function(value) {
    if (mapping[value] === undefined) {
      mapping[value] = colours[nextColour];
      if (reverseMapping[colours[nextColour].back] === undefined) {
        reverseMapping[colours[nextColour].back] = {
          front: colours[nextColour].front,
          list: []
        };
      }
      reverseMapping[colours[nextColour].back].list.push(value);
      nextColour++;
      if (nextColour === colours.length) {
        nextColour = 0;
      }
    }
    if (listener !== undefined) {
      listener(self.getList());
    }
    return mapping[value].back;
  };

  this.getCommunityColour = function() {
    return "#333333";
  };

  this.getForegroundColour = function(value) {
    if (mapping[value] === undefined) {
      mapping[value] = colours[nextColour];
      if (reverseMapping[colours[nextColour].back] === undefined) {
        reverseMapping[colours[nextColour].back] = {
          front: colours[nextColour].front,
          list: []
        };
      }
      reverseMapping[colours[nextColour].back].list.push(value);
      nextColour++;
      if (nextColour === colours.length) {
        nextColour = 0;
      }
    }
    if (listener !== undefined) {
      listener(self.getList());
    }
    return mapping[value].front;
  };
  
  this.getForegroundCommunityColour = function() {
    return "white";
  };
  

  
  this.reset = function() {
    mapping = {};
    reverseMapping = {};
    nextColour = 0;
    if (listener !== undefined) {
      listener(self.getList());
    }
  };
  
  this.getList = function() {
    return reverseMapping;
  };
  
  this.setChangeListener = function(callback) {
    listener = callback;
  }; 
  
  this.reset();
}