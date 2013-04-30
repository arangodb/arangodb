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


function ZoomManager(width, height, config) {
  "use strict";
  
  if (width === undefined || width < 0) {
    throw("A width has to be given.");
  }
  if (height === undefined || height < 0) {
    throw("A height has to be given.");
  }
  
  var self = this,
    fontMax,
    fontMin,
    rMax,
    rMin,
    rMid,
    
    radiusStep,
    fontStep,
    distortionStep,
    
    currentZoom,
    currentFont,
    currentRadius,
    currentLimit,
    currentDistortion,
    size =  width * height,
    calcNodeLimit = function () {
      var div;
      if (currentFont !== null) {
        div = 60 * currentFont * currentFont;
      } else {
        div = 4 * currentRadius * currentRadius * Math.PI;
      }
      return Math.floor(size / div);
    },
    parseConfig = function (conf) {
      fontMax = 16;
      fontMin = 6;
      rMax = 25;
      rMin = 1;
      rMid = (rMax - rMin) / 2 + rMin;
      
      fontStep = (fontMax - fontMin) / 100;
      radiusStep = (rMax - rMin) / 200;
      distortionStep = 0.001; // TODO!
      
      currentFont = fontMax;
      currentRadius = rMax;
      currentDistortion = 0;
      currentLimit = calcNodeLimit();
      currentZoom = 0;
    },
    adjustValues = function (out) {
      if (out) {
        currentZoom++;
        if (currentZoom > 100) {
          currentFont = null;
          if (currentZoom === 200) {
            currentRadius = rMin;
          } else {
            currentRadius -= radiusStep;
          }
        } else {
          
          if (currentZoom === 100) {
            currentFont = fontMin;
            currentRadius = rMid;
          } else {
            currentRadius -= radiusStep;
            currentFont -= fontStep;
          }
        }
        currentDistortion += distortionStep;
      } else {
        currentZoom--;
        if (currentZoom < 100) {
          if (currentZoom === 0) {
            currentFont = fontMax;
            currentRadius = rMax;
          } else {
            currentFont += fontStep;
            currentRadius += radiusStep;
          }
        } else {
          if (currentZoom === 100) {
            currentFont = fontMin;
            currentRadius = rMid;
          } else {
            currentRadius += radiusStep;
            currentFont = null;
          }
        }
        currentDistortion -= distortionStep;
      }
      currentLimit = calcNodeLimit();
    };
  
  parseConfig(config);
  
  self.getFontSize = function() {
    return currentFont;
  };
  
  self.getRadius = function() {
    return currentRadius;
  };
  
  self.getDistortion = function() {
    return currentDistortion;
  };
  
  self.getNodeLimit = function() {
    return currentLimit;
  };
  
  self.zoomIn = function() {
    if (currentZoom === 0) {
      return;
    }
    adjustValues(false);
  };
  
  self.zoomOut = function() {
    if (currentZoom === 200) {
      return;
    }
    adjustValues(true);
  };
 
}