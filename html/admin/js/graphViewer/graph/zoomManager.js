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


function ZoomManager(width, height, svg, g, nodeShaper, edgeShaper, config, limitCallback) {
  "use strict";
  
  if (width === undefined || width < 0) {
    throw("A width has to be given.");
  }
  if (height === undefined || height < 0) {
    throw("A height has to be given.");
  }
  if (svg === undefined || svg.node === undefined || svg.node().tagName.toLowerCase() !== "svg") {
    throw("A svg has to be given.");
  }
  if (g === undefined || g.node === undefined || g.node().tagName.toLowerCase() !== "g") {
    throw("A group has to be given.");
  }
  if (
    nodeShaper === undefined
    || nodeShaper.activateLabel === undefined
    || nodeShaper.changeTo === undefined
    || nodeShaper.updateNodes === undefined
  ) {
    throw("The Node shaper has to be given.");
  }
  if (
    edgeShaper === undefined
    || edgeShaper.activateLabel === undefined
    || edgeShaper.updateEdges === undefined
  ) {
    throw("The Edge shaper has to be given.");
  }
  
  
  var self = this,
    fontSize,
    nodeRadius,
    labelToggle,
    currentZoom,
    currentTranslation,
    lastD3Translation,
    lastD3Scale,
    currentLimit,
    fisheye,
    currentDistortion,
    currentDistortionRadius,
    baseDist,
    baseDRadius,
    size =  width * height,
    zoom,
    slider,
    minZoom,
    limitCB = limitCallback || function() {},
    
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
    
    calcDistortionValues = function () {
      currentDistortion = baseDist / currentZoom - 0.99999999; // Always > 0
      currentDistortionRadius = baseDRadius / currentZoom;
      fisheye.distortion(currentDistortion);
      fisheye.radius(currentDistortionRadius);
    },
    
    reactToZoom = function(scale, transX, transY, fromButton) {
      if (fromButton) {
        if (scale !== null) {
          currentZoom = scale;
        }
      } else {
        currentZoom = scale;
      }
      if (transX !== null) {
        currentTranslation[0] += transX;
      }
      if (transY !== null) {
        currentTranslation[1] += transY;
      }
      currentLimit = calcNodeLimit();
      limitCB(currentLimit);
      nodeShaper.activateLabel(currentZoom >= labelToggle);
      edgeShaper.activateLabel(currentZoom >= labelToggle);
      calcDistortionValues();
      
      var transT = "translate(" + currentTranslation + ")",
      scaleT = " scale(" + currentZoom + ")";
      if (g._isCommunity) {
        g.attr("transform", transT);
      } else {
        g.attr("transform", transT + scaleT);
      }
      slider.slider("option", "value", currentZoom);
    },
    
    getScaleDelta = function(nextScale) {
      var diff = lastD3Scale - nextScale;
      lastD3Scale = nextScale;
      return diff;
    },
    
    getTranslationDelta = function(nextTrans) {
      var tmp = [];
      tmp[0] = nextTrans[0] - lastD3Translation[0];
      tmp[1] = nextTrans[1] - lastD3Translation[1];
      lastD3Translation[0] = nextTrans[0];
      lastD3Translation[1] = nextTrans[1];
      return tmp;
    },
    
    parseConfig = function (conf) {
      if (conf === undefined) {
        conf = {};
      }
      var fontMax = conf.maxFont || 16,
      fontMin = conf.minFont || 6,
      rMax = conf.maxRadius || 25,
      rMin = conf.minRadius || 4;
      baseDist = conf.focusZoom || 1;
      baseDRadius = conf.focusRadius || 100;
      minZoom = rMin/rMax;
      fontSize = fontMax;
      nodeRadius = rMax;
      
      labelToggle = fontMin / fontMax;
      currentZoom = 1;
      currentTranslation = [0, 0];
      lastD3Translation = [0, 0];
      calcDistortionValues();
      
      currentLimit = calcNodeLimit();
      
      zoom = d3.behavior.zoom()
        .scaleExtent([minZoom, 1])
        .on("zoom", function() {
          //  scaleDiff = getScaleDelta(d3.event.scale),
          var
            sEvent = d3.event.sourceEvent,
            scale = currentZoom,
            translation;
          if (sEvent.type === "mousewheel" || sEvent.type === "DOMMouseScroll") {
            if (sEvent.wheelDelta) {
              if (sEvent.wheelDelta > 0) {
                scale += 0.01;
                if (scale > 1) {
                  scale = 1;
                }
              } else {
                scale -= 0.01;
                if (scale < minZoom) {
                  scale = minZoom;
                }
              }
            } else {
              if (sEvent.detail > 0) {
                scale += 0.01;
                if (scale > 1) {
                  scale = 1;
                }
              } else {
                scale -= 0.01;
                if (scale < minZoom) {
                  scale = minZoom;
                }
              }
            }
            translation = [0, 0];
          } else {
            translation = getTranslationDelta(d3.event.translate);
          }
          
          reactToZoom(scale, translation[0], translation[1]);
       });
      
    },
    mouseMoveHandle = function() {
      var focus = d3.mouse(this);
      focus[0] -= currentTranslation[0];
      focus[0] /= currentZoom;
      focus[1] -= currentTranslation[1];
      focus[1] /= currentZoom;
      fisheye.focus(focus);
      nodeShaper.updateNodes();
      edgeShaper.updateEdges();
    };
    
    
  
  fisheye = d3.fisheye.circular();
  
  parseConfig(config);
  
  svg.call(zoom);
  
  nodeShaper.changeTo({
    distortion: fisheye
  });
  
  svg.on("mousemove", mouseMoveHandle);
  
  self.translation = function() {
    return null;
  };
  
  self.scaleFactor = function() {
    return currentZoom;
  };
  
  self.scaledMouse = function() {
    return null;
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
  
  self.getMinimalZoomFactor = function() {
    return minZoom;
  };
  
  self.registerSlider = function(s) {
    slider = s;
  };
  
  self.triggerScale = function(s) {
    reactToZoom(s, null, null, true);
  };
  
  self.triggerTranslation = function(x, y) {
    reactToZoom(null, x, y, true);
  };
 
}