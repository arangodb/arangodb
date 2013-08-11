/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _, $, d3*/
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
*     type: EdgeShaper.shapes.ARROW
*   }
*   label: "key" \\ function(node)
*   actions: {
*     click: function(edge)
*   }
* }
*
*
*/

function EdgeShaper(parent, flags, idfunc) {
  "use strict";
  
  var self = this,
    edges = [],
    communityNodes = {},
    toplevelSVG,
    visibleLabels = true,
    followEdge = {},
    followEdgeG,
    idFunction = function(d) {
      return d._id;
    },
    noop = function (line, g) {
    
    },
    colourMapper = new ColourMapper(),
    events,
    addUpdate,
    addShape = noop,
    addLabel = noop,
    addColor = noop,
    
    unbindEvents = function() {
     events = {
       click: noop,
       dblclick: noop,
       mousedown: noop,
       mouseup: noop,
       mousemove: noop,
       mouseout: noop,
       mouseover: noop
     };
     addUpdate = noop;
    },
    
    
    getCorner = function(s, t) {
      return Math.atan2(t.y - s.y, t.x - s.x) * 180 / Math.PI;
    },
    
    getDistance = function(s, t) {
      var res = Math.sqrt(
        (t.y - s.y)
        * (t.y - s.y)
        + (t.x - s.x)
        * (t.x - s.x)
      );
      return res; 
    },
    
    addEvents = function (line, g) {
      _.each(events, function (func, type) {
        g.on(type, func);
      });
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
    
    calculateNodePositions = function (e) {
      var sp, tp, s, t;
      s = e.source;
      t = e.target;
      if (s._isCommunity) {
        communityNodes[s._id] = s;
        sp = s.getSourcePosition(e);
      } else {
        sp = s.position;
      }
      if (t._isCommunity) {
        communityNodes[t._id] = t;
        tp = t.getTargetPosition(e);
      } else {
        tp = t.position;
      }
      return {
        s: sp,
        t: tp
      };
    },
    
    addPosition = function (line, g) {
      communityNodes = {};
      g.attr("transform", function(d) {
        var p = calculateNodePositions(d);
        return "translate("
          + p.s.x + ", "
          + p.s.y + ")"
          + "rotate("
          + getCorner(p.s, p.t)
          + ")";
      });
      line.attr("x2", function(d) {
        var p = calculateNodePositions(d);
        return getDistance(p.s, p.t);
      });
    },
    
    addQue = function (line, g) {
      addShape(line, g);
      if (visibleLabels) {
        addLabel(line, g);
      }
      addColor(line, g);
      addEvents(line, g);
      addPosition(line, g);
    },
    
    shapeEdges = function (newEdges) {
      if (newEdges !== undefined) {
        edges = newEdges;
      }
      var line,
        g = self.parent
          .selectAll(".link")
          .data(edges, idFunction);
      // Append the group and class to all new    
      g.enter()
        .append("g")
        .attr("class", "link") // link is CSS class that might be edited
        .attr("id", idFunction);
      // Remove all old
      g.exit().remove();
      // Remove all elements that are still included.
      g.selectAll("* > *").remove();
      line = g.append("line");
      addQue(line, g);
      _.each(communityNodes, function(c) {
        c.shapeInnerEdges(d3.select(this), addQue);
      }); 
    },
    
    updateEdges = function () {
      var g = self.parent.selectAll(".link"),
        line = g.select("line");
      addPosition(line, g);
      addUpdate(g);
      _.each(communityNodes, function(c) {
        c.updateInnerEdges(d3.select(this), addPosition, addUpdate);
      });
    },
    
    parseShapeFlag = function (shape) {
      $("svg defs marker#arrow").remove();
      switch (shape.type) {
        case EdgeShaper.shapes.NONE:
          addShape = noop;
          break;
        case EdgeShaper.shapes.ARROW:
          addShape = function (line, g) {
            line.attr("marker-end", "url(#arrow)");
          };
          if (toplevelSVG.selectAll("defs")[0].length === 0) {
            toplevelSVG.append("defs");
          }
          toplevelSVG
            .select("defs")
            .append("marker")
            .attr("id", "arrow")
            .attr("refX", "22")
            .attr("refY", "5")
            .attr("markerUnits", "strokeWidth")
            .attr("markerHeight", "10")
            .attr("markerWidth", "10")
            .attr("orient", "auto")
            .append("path")
              .attr("d", "M 0 0 L 10 5 L 0 10 z");         
          break;
        default:
          throw "Sorry given Shape not known!";
      }
    },
    
    parseLabelFlag = function (label) {
      if (_.isFunction(label)) {
        addLabel = function (line, g) {
          g.append("text") // Append a label for the edge
            .attr("text-anchor", "middle") // Define text-anchor
            .text(label);
        };
      } else {
        addLabel = function (line, g) {
          g.append("text") // Append a label for the edge
            .attr("text-anchor", "middle") // Define text-anchor
            .text(function(d) { 
              // Which value should be used as label
              return d._data[label] !== undefined ? d._data[label] : "";
            });
        };
      }
      addUpdate = function (edges) {
        edges.select("text")
          .attr("transform", function(d) {
            var p = calculateNodePositions(d);
            return "translate("
              + getDistance(p.s, p.t) / 2
              + ", -3)";
          });
      };
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
      $("svg defs #gradientEdgeColor").remove();
      switch (color.type) {
        case "single":
          addColor = function (line, g) {
            line.attr("stroke", color.stroke);
          };
          break;
        case "gradient":
          if (toplevelSVG.selectAll("defs")[0].length === 0) {
            toplevelSVG.append("defs");
          }
          var gradient = toplevelSVG
            .select("defs")
            .append("linearGradient")
            .attr("id", "gradientEdgeColor");
          gradient.append("stop")
            .attr("offset", "0")
            .attr("stop-color", color.source);
          gradient.append("stop")
            .attr("offset", "0.4")
            .attr("stop-color", color.source);
          gradient.append("stop")
            .attr("offset", "0.6")
            .attr("stop-color", color.target);
          gradient.append("stop")
            .attr("offset", "1")
            .attr("stop-color", color.target);
          addColor = function (line, g) {
            line.attr("stroke", "url(#gradientEdgeColor)");
            line.attr("y2", "0.0000000000000001");
          };
          break;
        case "attribute":
          addColor = function (line, g) {
             g.attr("stroke", function(e) {
               return colourMapper.getColour(e._data[color.key]);
             });
          };
          break; 
        default:
          throw "Sorry given colour-scheme not known";
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
    };
    
  self.parent = parent;  
   
  unbindEvents();
  
  toplevelSVG = parent;
  while (toplevelSVG[0][0] && toplevelSVG[0][0].ownerSVGElement) {
    toplevelSVG = d3.select(toplevelSVG[0][0].ownerSVGElement);
  }
  
  if (flags === undefined) {
    flags = {
      color: {
        type: "single",
        stroke: "#686766"
      }
    };
  }
  
  if (flags.color === undefined) {
    flags.color = {
      type: "single",
      stroke: "#686766"
    };
  }

  parseConfig(flags);

  if (_.isFunction(idfunc)) {
    idFunction = idfunc;
  }
  
  followEdgeG = toplevelSVG.append("g");
  
  
  /////////////////////////////////////////////////////////
  /// Public functions
  /////////////////////////////////////////////////////////
  
  self.changeTo = function(config) {
    parseConfig(config);
    shapeEdges();
    updateEdges();
  };
  
  self.drawEdges = function (edges) {
    shapeEdges(edges);
    updateEdges();
  };
  
  self.updateEdges = function () {
    updateEdges();
  };
  
  self.reshapeEdges = function() {
    shapeEdges();
  };
  
  self.activateLabel = function(toogle) {
    if (toogle) {
      visibleLabels = true;
    } else {
      visibleLabels = false;
    }
    shapeEdges();
  };
  
  self.addAnEdgeFollowingTheCursor = function(x, y) {
    followEdge = followEdgeG.append("line");
    followEdge.attr("stroke", "black")
      .attr("id", "connectionLine")
      .attr("x1", x)
      .attr("y1", y)
      .attr("x2", x)
      .attr("y2", y);
    return function(x, y) {
      followEdge.attr("x2", x).attr("y2", y);
    };
  };
  
  self.removeCursorFollowingEdge = function() {
    if (followEdge.remove) {
      followEdge.remove();
      followEdge = {};
    }
  };
}

EdgeShaper.shapes = Object.freeze({
  "NONE": 0,
  "ARROW": 1
});
