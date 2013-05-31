/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _, d3*/
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


/*
* example config:
* {
*   nodes: nodes,
*   links: links,
*   
*   (optional)
*   width: width,
*   height: height,
*   distance: distance, 
*   gravity: gravity,
*   onUpdate: callback
* }
*/

function ForceLayouter(config) {
  "use strict";
  var self = this,
    force = d3.layout.force(),
    distance = config.distance || 240, // 80
    gravity = config.gravity || 0.01, // 0.08
    charge = config.charge || -1000, // -240
    onUpdate = config.onUpdate || function () {},
    width = config.width || 940,
    height = config.height || 640,
    parseConfig = function(config) {
      if (config.distance) {
        force.linkDistance(config.distance);
      }
      if (config.gravity) {
        force.gravity(config.gravity);
      }
      if (config.charge) {
        force.charge(config.charge);
      }
    };
   
  if (config.nodes === undefined) {
    throw "No nodes defined";
  }
  if (config.links === undefined) {
    throw "No links defined";
  }
  // Set up the force
  force.nodes(config.nodes); // Set nodes
  force.links(config.links); // Set edges
  force.size([width, height]); // Set width and height
  force.linkDistance(distance); // Set distance between nodes
  force.gravity(gravity); // Set gravity
  force.charge(charge); // Set charge
  force.on("tick", function(){}); // Bind tick function
    
  self.start = function() {
    force.start(); // Start Force computation
  };
  
  self.stop = function() {
    force.stop(); // Stop Force computation
  };
  
  self.drag = force.drag;
  
  self.setCombinedUpdateFunction = function(nodeShaper, edgeShaper, additional) {
    if (additional !== undefined) {
      onUpdate = function() {
        if (force.alpha() < 0.1) {
          nodeShaper.updateNodes();
          edgeShaper.updateEdges();
          additional();
          if (force.alpha() < 0.05) {
            self.stop();
          }
        }
      };
      force.on("tick", onUpdate);
    } else {
      onUpdate = function() {
        if (force.alpha() < 0.1) {
          nodeShaper.updateNodes();
          edgeShaper.updateEdges();
          if (force.alpha() < 0.05) {
            self.stop();
          }
        }
      };
      force.on("tick", onUpdate);
    }
  };
  
  self.changeTo = function(config) {
    parseConfig(config);
  };
}