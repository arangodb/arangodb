/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _, d3*/
/*global ColourMapper*/
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
    nodes = [],
    visibleLabels = true,
    noop = function (node) {
    
    },
    defaultDistortion = function(n) {
      return {
        x: n.x,
        y: n.y,
        z: 1
      };
    },
    distortion = defaultDistortion,
    addDistortion = function() {
      _.each(nodes, function(n) {
        n.position = distortion(n);
      });
    },
    colourMapper = new ColourMapper(),
    events,
    addUpdate,
    idFunction = function(d) {
      return d._id;
    },
    addColor = noop,
    addShape = noop,
    addLabel = noop,
    
    unbindEvents = function() {
      // Hard unbind the dragging
      self.parent
        .selectAll(".node")
        .on("mousedown.drag", null);
      events = {
        click: noop,
        dblclick: noop,
        drag: noop,
        mousedown: noop,
        mouseup: noop,
        mousemove: noop
      };
      addUpdate = noop;
    },
    
    addEvents = function (nodes) {
      _.each(events, function (func, type) {
        if (type === "drag") {
          nodes.call(func);
        } else {
          nodes.on(type, func);
        }
        
      });
    },
    
    addQue = function (g) {
      addShape(g);
      if (visibleLabels) {
        addLabel(g);
      }
      addColor(g);
      addEvents(g);
      addDistortion();
    },
    
    bindEvent = function (type, func) {
      if (type === "update") {
        addUpdate = func;
      } else if (events[type] === undefined) {
        throw "Sorry Unknown Event " + type + " cannot be bound.";
      } else {
        events[type] = func;
      }
    },
    
    updateNodes = function () {
      var nodes = self.parent.selectAll(".node");
      addDistortion();
      nodes.attr("transform", function(d) {
        return "translate(" + d.position.x + "," + d.position.y + ")scale(" + d.position.z + ")"; 
      });
      addUpdate(nodes);
    },
    
    shapeNodes = function (newNodes) {
      if (newNodes !== undefined) {
        nodes = newNodes;
      }
      var g = self.parent
        .selectAll(".node")
        .data(nodes, idFunction);
      // Append the group and class to all new    
      g.enter()
        .append("g")
        .attr("class", "node") // node is CSS class that might be edited
        .attr("id", idFunction);
      // Remove all old
      g.exit().remove();
      g.selectAll("* > *").remove();
      addQue(g);
      updateNodes();
    },

    parseShapeFlag = function (shape) {
      var radius, width, height;
      switch (shape.type) {
        case NodeShaper.shapes.NONE:
          addShape = noop;
          break;
        case NodeShaper.shapes.CIRCLE:
          radius = shape.radius || 25;
          addShape = function (node) {
            node.append("circle") // Display nodes as circles
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
              return d._data[label] !== undefined ? d._data[label] : "";
            });
        };
      }
    },
    
    parseActionFlag = function (actions) {
      if (actions.reset !== undefined && actions.reset) {
        unbindEvents();
      }
      _.each(actions, function(func, type) {
        if (type !== "reset") {
          bindEvent(type, func);
        }
      });
    },
    
    parseColorFlag = function (color) {
      switch (color.type) {
        case "single":
          addColor = function (g) {
            g.attr("stroke", color.stroke);
            g.attr("fill", color.fill);
          };
          break;
        case "expand":
          addColor = function (g) {
            g.attr("fill", function(n) {
              if (n._expanded) {
                return color.expanded;
              }
              return color.collapsed;
            });
            g.attr("stroke", function(n) {
              if (n._expanded) {
                return color.expanded;
              }
              return color.collapsed;
            });
          };
          break;
        case "attribute":
          addColor = function (g) {
             g.attr("fill", function(n) {
               return colourMapper.getColour(n._data[color.key]);
             });
             g.attr("stroke", function(n) {
               return colourMapper.getColour(n._data[color.key]);
             });
          };
          break; 
        default:
          throw "Sorry given colour-scheme not known";
      }
    },
    
    parseDistortionFlag = function (dist) {
      if (dist === "reset") {
        distortion = defaultDistortion;
      } else if (_.isFunction(dist)) {
        distortion = dist;
      } else {
        throw "Sorry distortion cannot be parsed.";
      }
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
      if (config.color !== undefined) {
        parseColorFlag(config.color);
      }
      if (config.distortion !== undefined) {
        parseDistortionFlag(config.distortion);
      }
      
    };
    
  self.parent = parent;
  
  unbindEvents();
  
  if (flags === undefined) {
    flags = {
      color: {
        type: "single",
        fill: "#FF8F35",
        stroke: "#8AA051"
      }
    };
  }
  
  if (flags.shape === undefined) {
   flags.shape = {
     type: NodeShaper.shapes.CIRCLE
   }; 
  }
  
  if (flags.color === undefined) {
    flags.color = {
      type: "single",
      fill: "#FF8F35",
      stroke: "#8AA051"
    }; 
  }
  
  if (flags.distortion === undefined) {
    flags.distortion = "reset";
  }
  
  parseConfig(flags);

  if (_.isFunction(idfunc)) {
    idFunction = idfunc;
  }
  
  
  /////////////////////////////////////////////////////////
  /// Public functions
  /////////////////////////////////////////////////////////
  
  self.changeTo = function(config) {
    parseConfig(config);
    shapeNodes();
  };
  
  self.drawNodes = function (nodes) {
    shapeNodes(nodes);
  };
  
  self.updateNodes = function () {
    updateNodes();
  };
  
  self.reshapeNodes = function() {
    shapeNodes();
  };
  
  self.activateLabel = function(toogle) {
    if (toogle) {
      visibleLabels = true;
    } else {
      visibleLabels = false;
    }
    shapeNodes();
  };
  
}

NodeShaper.shapes = Object.freeze({
  "NONE": 0,
  "CIRCLE": 1,
  "RECT": 2
});