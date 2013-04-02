/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _, $*/
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
    idFunction = function(d) {
      return d.source._id + "-" + d.target._id;
    },
    noop = function (edge) {
    
    },
    events = {
      click: noop,
      dblclick: noop,
      mousedown: noop,
      mouseup: noop,
      mousemove: noop
    },
    addUpdate = noop,
    addShape = noop,
    addLabel = noop,
    
    addEvents = function (edges) {
      _.each(events, function (func, type) {
        if (type === "update") {
          addUpdate = func;
        } else {
          edges.on(type, func);
        }
        
      });
    },
    
    bindEvent = function (type, func) {
      if (type === "update") {
        addUpdate = func;
      } else if (events[type] === undefined) {
        throw "Sorry Unknown Event " + type + " cannot be bound.";
      }
      events[type] = func;
    },
    
    shapeEdges = function (nodes) {
      if (nodes !== undefined) {
        var data, handle;
        data = self.parent
          .selectAll(".link")
          .data(nodes, idFunction);
        handle = data
          .enter()
          .append("line")
          .attr("class", "link") // link is CSS class that might be edited
          .attr("id", idFunction);
        addShape(handle);
        addLabel(handle);
        addEvents(handle);
        data.exit().remove();
        return handle;
      }
    },
    
    reshapeEdges = function () {
      var handle;
      handle = self.parent
        .selectAll(".link");
      $(".link").empty();
      addShape(handle);
      addLabel(handle);
      addEvents(handle);
    },
    
    reshapeEdge = function (edge) {
      var handle = self.parent
        .selectAll(".link")
        .filter(function (e) {
          return e._id === edge._id;
        });
      $("#" + edge._id.replace(/([ #;&,.+*~\':"!^$[\]()=>|\/])/g,'\\$1')).empty();
      addShape(handle);
      addLabel(handle);
      addEvents(handle);
    },
    
    updateEdges = function () {
      var edges = self.parent.selectAll(".link")
        // Set source x coordinate for edge.
        .attr("x1", function(d) {
            return d.source.x; 
        })
        // Set source y coordinate for edge.
        .attr("y1", function(d) {
            return d.source.y; 
        })
        // Set target x coordinate for edge.
        .attr("x2", function(d) {
            return d.target.x; 
        })
        // Set target y coordinate for edge.
        .attr("y2", function(d) {
            return d.target.y;
        });
        edges.attr("transform", function(d) {
          return "translate(" + d.x + "," + d.y + ")"; 
        });
      addUpdate(edges);
    },
    
    parseShapeFlag = function (shape) {
      switch (shape.type) {
        default:
          throw "Sorry given Shape not known!";
      }
    },
    
    parseLabelFlag = function (label) {
      if (_.isFunction(label)) {
        addLabel = function (edge) {
          edge.append("text") // Append a label for the edge
            .attr("text-anchor", "middle") // Define text-anchor
            .text(label);
        };
      } else {
        addLabel = function (edge) {
          edge.append("text") // Append a label for the edge
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
    reshapeEdges();
  };
  
  self.drawEdges = function (edges) {
    return shapeEdges(edges);
  };
  
  self.updateEdges = function () {
    updateEdges();
  };
  
  self.reshapeEdge = function(edge) {
    reshapeEdge(edge);
  };
  
  self.reshapeEdges = function() {
    reshapeEdges();
  }; 
}

EdgeShaper.shapes = Object.freeze({
  "NONE": 0,
  "ARROW": 1
});

// Marker:
// .attr("marker-end", "url(#arrow)");