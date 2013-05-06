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


function ZoomManager(width, height, g, nodeShaper, edgeShaper, config) {
  "use strict";
  
  if (width === undefined || width < 0) {
    throw("A width has to be given.");
  }
  if (height === undefined || height < 0) {
    throw("A height has to be given.");
  }
  if (g === undefined || g.node === undefined || g.node().tagName !== "G") {
    throw("A group has to be given.");
  }
  if (nodeShaper === undefined || nodeShaper.activate === undefined) {
    throw("The Node shaper has to be given.");
  }
  if (edgeShaper === undefined || edgeShaper.activate === undefined) {
    throw("The Edge shaper has to be given.");
  }
  
  
  var self = this,
    fontSize,
    nodeRadius,
    labelToggle,
    currentZoom,
    currentLimit,
    currentDistortion,
    currentDistortionRadius,
    size =  width * height,
    zoom,
    
    calcNodeLimit = function () {
      var div, reqSize;
      if (currentZoom >= labelToggle) {
        reqSize = fontSize * currentZoom;
        reqSize *= reqSize;
        div = 60 * reqSize;
      } else {
        reqSize = nodeRadius * currentZoom;
        reqSize *= reqSize;
        div = 4 * Math.PI * reqSize;
      }
      return Math.floor(size / div);
    },
    parseConfig = function (conf) {
      if (conf === undefined) {
        conf = {};
      }
      var 
      fontMax = conf.maxFont || 16,
      fontMin = conf.minFont || 6,
      rMax = conf.maxRadius || 25,
      rMin = conf.minRadius || 1;
      
      fontSize = fontMax;
      nodeRadius = rMax;
      
      labelToggle = 0;
      currentDistortion = 0;
      currentDistortionRadius = 100;
      currentLimit = calcNodeLimit();
      currentZoom = 1;
      
      zoom = d3.behavior.zoom()
       .scaleExtent([rMin/rMax, 1])
       .on("zoom", function() {
         // TODO: Still to be implemented
         currentZoom = d3.event.scale;
         currentLimit = calcNodeLimit();
         
         //curTrans = $.extend({}, d3.event.translate);
         /*
         //curTrans[0] /= curZoom;
         //curTrans[1] /= curZoom;
           //console.log("here", d3.event.translate, d3.event.scale);
             g.attr("transform",
                 "translate(" + d3.event.translate + ")"
                 + " scale(" + d3.event.scale + ")");
             if (d3.event.scale < stopLabel) {
               test.remove();
             }
             /*
             fisheye
             .distortion(1/d3.event.scale * fe_dist - 1);
             */
             //.radius(1/d3.event.scale * fe_radius);
         
       });
      
    };
  
  parseConfig(config);
  
  g.call(zoom);

  
  self.translation = function() {
    return null;
  };
  
  self.scaleFactor = function() {
    return currentZoom;
  };
  
  self.scaledMouse = function() {
    return null;
  };
  
  self.mouseMoveHandle = function() {
    // TODO
    var focus = d3.mouse(this);
    focus[0] += curTrans[0];
    focus[1] += curTrans[1];
    fisheye.focus(focus);

    node.each(function(d) { d.fisheye = fisheye(d); })
        .attr("cx", function(d) { return d.fisheye.x; })
        .attr("cy", function(d) { return d.fisheye.y; })
        .attr("r", function(d) { return d.fisheye.z * 25; });

    link.attr("x1", function(d) { return d.source.fisheye.x; })
        .attr("y1", function(d) { return d.source.fisheye.y; })
        .attr("x2", function(d) { return d.target.fisheye.x; })
        .attr("y2", function(d) { return d.target.fisheye.y; });
  };
 
  self.getDistortion = function() {
    return currentDistortion;
  };
  
  self.getDistortionRadius = function() {
    return currentDistortionRadius;
  };
  
  self.getNodeLimit = function() {
    return currentLimit;
  };
  
 
}