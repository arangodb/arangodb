/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global _*/
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
*   label: "key" \\ function(node)
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
    additionalShaping = function(edge) {
      return;
    },
    decorateShape = function (deco) {
      var tmp = additionalShaping;
      additionalShaping = function (edge) {
        tmp(edge);
        deco(edge);
      };
    },
    parseLabelFlag;
    
  self.parent = parent;
  
  if (flags !== undefined) {

    parseLabelFlag = function (label) {
      var labelDeco = additionalShaping;
      if (_.isFunction(label)) {
        additionalShaping = function (edge) {
          edge.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .text(function(d) { 
              return label(d);
            });
          labelDeco(edge);
        };
      } else {
        additionalShaping = function (edge) {
          edge.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .text(function(d) { 
              return d[label] || ""; // Which value should be used as label
            });
          labelDeco(edge);
        };
      }
    };
  
    parseLabelFlag(flags.label);
    
  }

  if (_.isFunction(idfunc)) {
    idFunction = idfunc;
  }
  
  self.drawEdges = function (edges) {
    var data, link;
    if (edges !== undefined) {
      data = self.parent
        .selectAll("line")
        .data(edges, idFunction);
      link = data
        .enter()
        .append("line")
        .attr("class", "link") // link is CSS class that might be edited
        .attr("id", idFunction);
      additionalShaping(link);
      data.exit().remove();
      return link;
    }
    link = self.parent
      .selectAll("line")
      .attr("class", "link") // node is CSS class that might be edited
      .attr("id",idFunction);
    additionalShaping(link);
  };
  
  self.updateEdges = function () {
    self.parent.selectAll("line.link")
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
  };
  
  self.on = function (identifier, callback) {
    decorateShape(function (edge) {
      edge.on(identifier, callback);
    });
    self.drawEdges();
  };
  
  
}



// Marker:
// .attr("marker-end", "url(#arrow)");