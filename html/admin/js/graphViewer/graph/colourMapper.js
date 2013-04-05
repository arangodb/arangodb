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
  colours = [],
  nextColour = 0;
  
  colours.push("navy");
  colours.push("green");
  colours.push("gold");
  colours.push("red");
  colours.push("saddlebrown");
  colours.push("skyblue");
  colours.push("olive");
  colours.push("deeppink");
  colours.push("orange");
  colours.push("silver");
  colours.push("blue");
  colours.push("yellowgreen");
  colours.push("firebrick");
  colours.push("rosybrown");
  colours.push("hotpink");
  colours.push("purple");
  colours.push("cyan");
  colours.push("teal");
  colours.push("peru");
  colours.push("maroon");
  
  this.getColour = function(value) {
    if (mapping[value] === undefined) {
      mapping[value] = colours[nextColour];
      nextColour++;
      if (nextColour === colours.length) {
        nextColour = 0;
      }
    }
    return mapping[value];
  };
  
  this.reset = function() {
    mapping = {};
    nextColour = 0;
  };
  
}