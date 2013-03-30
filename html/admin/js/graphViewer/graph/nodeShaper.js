/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global alert*/
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////




/*
* flags example format:
* {
*   shape: {
*     type: NodeShaper.shapes.CIRCLE,
*     radius: value || function(node)
*   },
*   label: "key" || function(node),
*   actions: {
*     "click": function(node),
*     "drag": function(node)
*   },
*   update: function(node)
* }
*
* {
*   shape: {
*     type: NodeShaper.shapes.RECT,
*     width: value || function(node),
*     height: value || function(node),
*   },
*   label: "key" || function(node),
*   actions: {
*     "click": function(node),
*     "drag": function(node)
*   },
*   update: function(node)
* }
*/
function NodeShaper(parent, flags, idfunc) {
  "use strict";
  
  var self = this,  
    noop = function (node) {
    
    },
    events = {
      click: noop,
      drag: noop,
      mousedown: noop,
      mouseup: noop
    },
    userDefinedUpdate = function(){},
    idFunction = function(d) {
      return d._id;
    },
    addShape = noop,
    reloadShape = noop,
    addLabel = noop,
    reloadLabel = noop,
    
    addEvents = function (nodes) {
      _.each(events, function (func, type) {
        if (type === "drag") {
          nodes.call(func);
        } else if (type === "update") {
          alert("buhu");
        } else {
          nodes.on(type, func);
        }
        
      });
    },
    
    bindEvent = function (type, func) {
      if (events[type] === undefined) {
        throw "Sorry Unknown Event " + type + " cannot be bound.";
      }
      events[type] = func;
    },
    
    shapeNodes = function (nodes) {
      if (nodes !== undefined) {
        var data, handle;
        data = self.parent
          .selectAll(".node")
          .data(nodes, idFunction);
        handle = data
          .enter()
          .append("g")
          .attr("class", "node") // node is CSS class that might be edited
          .attr("id", idFunction);
        addShape(handle);
        addLabel(handle);
        addEvents(handle);
        data.exit().remove();
        return handle;
      }
    },
    
    reshapeNodes = function () {
      var handle;
      handle = self.parent
        .selectAll(".node");
      $(".node").empty();
      addShape(handle);
      addLabel(handle);
      addEvents(handle);
    },
    
    updateNodes = function () {
      var nodes = self.parent.selectAll(".node");
      nodes.attr("transform", function(d) {
        return "translate(" + d.x + "," + d.y + ")"; 
      });
      userDefinedUpdate(nodes);
    },
    
    parseShapeFlag = function (shape) {
      var radius, width, height;
      switch (shape.type) {
        case NodeShaper.shapes.CIRCLE:
          radius = shape.radius || 12;
          addShape = function (node) {
            node.append("circle") // Display nodes as circles
              .attr("r", radius); // Set radius
          };
          reloadShape = function (node) {
            node.select("circle") // Just Change the internal circle
              .attr("r", radius); // Set radius
          };
          break;
        case NodeShaper.shapes.RECT:
          width = shape.width || 20;
          height = shape.height || 10;
          addShape = function(node) {
            node.append("rect") // Display nodes as rectangles
              .attr("width", width) // Set width
              .attr("height", height); // Set height
          };
          reloadShape = function(node) {
            node.select("rect") // Just Change the internal rectangle
              .attr("width", width) // Set width
              .attr("height", height); // Set height
          };
          break;
        case undefined:
          break;
        default:
          throw "Sorry given Shape not known!";
      }
    },
    
    parseLabelFlag = function (label) {
      if (_.isFunction(label)) {
        addLabel = function (node) {
          node.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .text(label);
        };
      } else {
        addLabel = function (node) {
          node.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .text(function(d) { 
              return d[label] !== undefined ? d[label] : ""; // Which value should be used as label
            });
        };
      }
    },
    
    parseActionFlag = function (actions) {
      _.each(actions, function(func, type) {
        bindEvent(type, func);
      });
    },
    
    parseConfig = function(config) {
      if (config.shape !== undefined) {
        parseShapeFlag(config.shape);
      }
      if (config.label !== undefined) {
        parseLabelFlag(config.label);
      }
      if (config.actions !== undefined) {
        parseActionFlag(config.actions);
      }
    };
    
    
  self.parent = parent;
  
  if (flags !== undefined) {
    parseConfig(flags);
  } 

  if (_.isFunction(idfunc)) {
    idFunction = idfunc;
  }
  
  
  /////////////////////////////////////////////////////////
  /// Public functions
  /////////////////////////////////////////////////////////
  
  self.changeTo = function(config) {
    parseConfig(config);
    reshapeNodes();
  };
  
  self.drawNodes = function (nodes) {
    return shapeNodes(nodes);
  };
  
  self.updateNodes = function () {
    updateNodes();
  };
  
}

NodeShaper.shapes = Object.freeze({
  "CIRCLE": 1,
  "RECT": 2
});