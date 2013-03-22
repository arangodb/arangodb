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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////




/*
* flags example format:
* {
*   shape: NodeShaper.shapes.CIRCLE,
*   label: "key" || function(node),
*   radius: value || function(node),
*   width: value || function(node),
*   height: value || function(node),
*   onclick: function(node),
*   ondrag: function(node),
*   onupdate: function(nodes)
* }
*
*
*/
function NodeShaper(parent, flags, idfunc) {
  "use strict";
  
  var self = this,
    radius,
    width,
    height,
    parseShapeFlag,
    parseLabelFlag,
    userDefinedUpdate = function(){},
    idFunction = function(d) {
      return d._id;
    },
    additionalShaping = function(node) {
      return;
    },
    decorateShape = function (deco) {
      var tmp = additionalShaping;
      additionalShaping = function (node) {
        tmp(node);
        deco(node);
      };
    },
    reshaping = function(node) {
      return;
    },
    decorateReshape = function (deco) {
      var tmp = reshaping;
      reshaping = function (node) {
        tmp(node);
        deco(node);
      };
    };
    
  self.parent = parent;
  
  if (flags !== undefined) {
    parseShapeFlag = function (shape) {
      switch (shape) {
        case NodeShaper.shapes.CIRCLE:
          radius = flags.size || 12;
          decorateShape(function(node) {
            node.append("circle") // Display nodes as circles
              .attr("r", radius); // Set radius
            });
          decorateReshape(function(node) {
            node.select("circle") // Just Change the internal circle
              .attr("r", radius); // Set radius
          });
          break;
        case NodeShaper.shapes.RECT:
          width = flags.width || 20;
          height = flags.height || 10;
          decorateShape(function(node) {
            node.append("rect") // Display nodes as rectangles
              .attr("width", width) // Set width
              .attr("height", height); // Set height
            });
            decorateReshape(function(node) {
              node.select("rect") // Just Change the internal rectangle
                .attr("width", width) // Set width
                .attr("height", height); // Set height
            });
          break;
        case undefined:
          break;
        default:
          throw "Sorry given Shape not known!";
      }
    };
    parseLabelFlag = function (label) {
      var labelDeco = additionalShaping;
      if (_.isFunction(label)) {
        decorateShape(function (node) {
          node.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .text(function(d) { 
              return label(d);
            });
        });
      } else {
        decorateShape(function (node) {
          node.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .text(function(d) { 
              return d[label] !== undefined ? d[label] : ""; // Which value should be used as label
            });
        });
      }
    };
  
    parseShapeFlag(flags.shape);
    parseLabelFlag(flags.label);

    if (flags.ondrag) {
      decorateShape(function (node) {
        node.call(flags.ondrag);
      });
    }
  } 

  if (_.isFunction(idfunc)) {
    idFunction = idfunc;
  }
  
  self.drawNodes = function (nodes) {
    var data, node;
    if (nodes !== undefined) {
      data = self.parent
        .selectAll(".node")
        .data(nodes, idFunction);
      node = data
        .enter()
        .append("g")
        .attr("class", "node") // node is CSS class that might be edited
        .attr("id",idFunction);
      additionalShaping(node);
      data.exit().remove();
      return node;
    }
    node = 
    self.parent
      .selectAll(".node")
      .attr("class", "node") // node is CSS class that might be edited
      .attr("id",idFunction);
    additionalShaping(node);
  };
  
  self.updateNodes = function () {
    var nodes = self.parent.selectAll(".node");
    nodes.attr("transform", function(d) {
      return "translate(" + d.x + "," + d.y + ")"; 
    });
    userDefinedUpdate(nodes);
  };
  
  self.reshapeNode = function (node) {
    var nodes  = d3.selectAll(".node").filter(
      function(d) {
        return d._id === node._id;
      });
    reshaping(nodes);
  };
  
  self.on = function (identifier, callback) {
    if (identifier === "update") {
      userDefinedUpdate = callback;
    } else {
      decorateShape(function (node) {
        node.on(identifier, callback);
      });
    }
    self.drawNodes();
  };
}

NodeShaper.shapes = Object.freeze({
  "CIRCLE": 1,
  "RECT": 2
});