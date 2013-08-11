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
*
* <image x="0" y="0" height="1140" width="1040" xlink:href="like_a_sir_original.svg"/>
*
* {
*   shape: {
*     type: NodeShaper.shapes.IMAGE,
*     width: value || function(node),
*     height: value || function(node),
*   },
*   actions: {
*     "click": function(node),
*     "drag": function(node)
*   },
*   update: function(node)
* }
*
*/
function NodeShaper(parent, flags, idfunc) {
  "use strict";

  var self = this,
    nodes = [],
    visibleLabels = true,
    splitLabel = function(label) {
      if (label === undefined) {
        return [""];
      }
      if (typeof label !== "string") {
        
        label = String(label);
      }
      var chunks = label.match(/[\w\W]{1,10}(\s|$)|\S+?(\s|$)/g);
      chunks[0] = $.trim(chunks[0]);
      chunks[1] = $.trim(chunks[1]);
      if (chunks.length > 2) {
        chunks.length = 2;
        chunks[1] += "...";
      }
      return chunks;
    },
    noop = function (node) {
    
    },
    start = noop,
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
        if (n._isCommunity) {
          n.addDistortion(distortion);
        }
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
    addLabelColor = function() {return "black";},

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
        mousemove: noop,
        mouseout: noop,
        mouseover: noop
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
      var community = g.filter(function(n) {
          return n._isCommunity;
        }),
        normal = g.filter(function(n) {
          return !n._isCommunity;
        });
      addShape(normal);
      community.each(function(c) {
        c.shapeNodes(d3.select(this), addShape, addQue, start, colourMapper);
      });
      if (visibleLabels) {
        addLabel(normal);
      }
      addColor(normal);
      addEvents(normal);
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
        .attr("class", function(d) {
          if (d._isCommunity) {
            return "node communitynode";
          }
          return "node";
        }) // node is CSS class that might be edited
        .attr("id", idFunction);
      // Remove all old
      g.exit().remove();
      g.selectAll("* > *").remove();
      addQue(g);
      updateNodes();
    },

    parseShapeFlag = function (shape) {
      var radius, width, height, translateX, translateY,
        fallback, source;
      switch (shape.type) {
        case NodeShaper.shapes.NONE:
          addShape = noop;
          break;
        case NodeShaper.shapes.CIRCLE:
          radius = shape.radius || 25;
          addShape = function (node, shift) {
            node
              .append("circle") // Display nodes as circles
              .attr("r", radius); // Set radius
            if (shift) {
              node.attr("cx", -shift)
                .attr("cy", -shift);
            }
          };
          break;
        case NodeShaper.shapes.RECT:
          width = shape.width || 90;
          height = shape.height || 36;
          if (_.isFunction(width)) {
            translateX = function(d) {
              return -(width(d) / 2);
            };
          } else {
            translateX = function(d) {
              return -(width / 2);
            };
          }
          if (_.isFunction(height)) {
            translateY = function(d) {
              return -(height(d) / 2);
            };
          } else {
            translateY = function() {
              return -(height / 2);
            };
          }
          addShape = function(node, shift) {
            shift = shift || 0;
            node.append("rect") // Display nodes as rectangles
              .attr("width", width) // Set width
              .attr("height", height) // Set height
              .attr("x", function(d) { return translateX(d) - shift;})
              .attr("y", function(d) { return translateY(d) - shift;})
              .attr("rx", "8")
              .attr("ry", "8");
          };
          break;
        case NodeShaper.shapes.IMAGE:
          width = shape.width || 32;
          height = shape.height || 32;
          fallback = shape.fallback || "";
          source = shape.source || fallback;
          if (_.isFunction(width)) {
            translateX = function(d) {
              return -(width(d) / 2);
            };
          } else {
            translateX = -(width / 2);
          }
          if (_.isFunction(height)) {
            translateY = function(d) {
              return -(height(d) / 2);
            };
          } else {
            translateY = -(height / 2);
          }
          addShape = function(node) {
            var img = node.append("image") // Display nodes as images
              .attr("width", width) // Set width
              .attr("height", height) // Set height
              .attr("x", translateX)
              .attr("y", translateY);
            if (_.isFunction(source)) {
              img.attr("xlink:href", source);
            } else {
              img.attr("xlink:href", function(d) {
                if (d._data[source]) {
                  return d._data[source];
                }
                return fallback;
              });
            }
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
          var textN = node.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .attr("fill", addLabelColor) // Force a black color
            .attr("stroke", "none"); // Make it readable
            textN.each(function(d) {
              var chunks = splitLabel(label(d));
              d3.select(this).append("tspan")
                .attr("x", "0")
                .attr("dy", "-4")
                .text(chunks[0]);
              if (chunks.length === 2) {
                d3.select(this).append("tspan")
                  .attr("x", "0")
                  .attr("dy", "16")
                  .text(chunks[1]);
              }
            });
        };
      } else {
        addLabel = function (node) {
          var textN = node.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .attr("fill", addLabelColor) // Force a black color
            .attr("stroke", "none"); // Make it readable
          textN.each(function(d) {
            var chunks = splitLabel(d._data[label]);
            d3.select(this).append("tspan")
              .attr("x", "0")
              .attr("dy", "-4")
              .text(chunks[0]);
            if (chunks.length === 2) {
              d3.select(this).append("tspan")
                .attr("x", "0")
                .attr("dy", "16")
                .text(chunks[1]);
            }
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
            g.attr("fill", color.fill);
          };
          addLabelColor = function (d) {
            return color.stroke;
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
          };
          addLabelColor = function (d) {
            return "white";
          };
          break;
        case "attribute":
          addColor = function (g) {
             g.attr("fill", function(n) {
               if (n._data === undefined) {
                 return colourMapper.getCommunityColour();
               }
               return colourMapper.getColour(n._data[color.key]);
             });
          };
          addLabelColor = function (n) {
            if (n._data === undefined) {
              return colourMapper.getForegroundCommunityColour();
            }
            return colourMapper.getForegroundColour(n._data[color.key]);
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
    flags = {};
  }
  
  if (flags.shape === undefined) {
   flags.shape = {
     type: NodeShaper.shapes.RECT
   }; 
  }
  
  if (flags.color === undefined) {
    flags.color = {
      type: "single",
      fill: "#333333",
      stroke: "white"
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
  
  self.getColourMapping = function() {
    return colourMapper.getList();
  }; 
  
  self.setColourMappingListener = function(callback) {
    colourMapper.setChangeListener(callback);
  };
  
  self.setGVStartFunction = function(func) {
    start = func;
  };
  
}

NodeShaper.shapes = Object.freeze({
  "NONE": 0,
  "CIRCLE": 1,
  "RECT": 2,
  "IMAGE": 3
});