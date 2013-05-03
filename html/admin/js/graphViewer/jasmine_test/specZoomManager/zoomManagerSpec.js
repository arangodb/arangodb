/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach, jasmine */
/*global describe, it, expect */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global ZoomManager*/

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


(function () {
  "use strict";

  describe('Zoom Manager', function () {
    var svg;

    beforeEach(function () {
      svg = document.createElement("svg");
      document.body.appendChild(svg);
    });

    afterEach(function () {
      document.body.removeChild(svg);
    });

    describe('setup process', function() {
      
      it('should throw an error if no width is given', function() {
        expect(function() {
          var s = new ZoomManager();
        }).toThrow("A width has to be given.");
      });
      
      it('should throw an error if no height is given', function() {
        expect(function() {
          var s = new ZoomManager(10);
        }).toThrow("A height has to be given.");
      });
      
      it('should not throw an error if mandatory information is given', function() {
        expect(function() {
          var s = new ZoomManager(10, 10);
        }).not.toThrow();
      });
    });

  });

  describe('setup with default values', function() {
    
    var w,
      h,
      manager;
    
    beforeEach(function() {
      w = 200;
      h = 200;
      manager = new ZoomManager(w, h);
    });
    
    describe('the interface', function() {
      
      it('should offer a function handle for zoom', function() {
        expect(manager.zoomHandle).toBeDefined();
        expect(manager.zoomHandle).toEqual(jasmine.any(Function));
      });
      
      it('should offer a function to get the current scale factor', function() {
        expect(manager.scaleFactor).toBeDefined();
        expect(manager.scaleFactor).toEqual(jasmine.any(Function));
      });
      
      it('should offer a function to get the current translation', function() {
        expect(manager.translation).toBeDefined();
        expect(manager.translation).toEqual(jasmine.any(Function));
      });
      
      it('should offer a function to get scaled mouse position', function() {
        expect(manager.scaledMouse).toBeDefined();
        expect(manager.scaledMouse).toEqual(jasmine.any(Function));
      });
      
      it('should offer a function to get the distortion', function() {
        expect(manager.getDistortion).toBeDefined();
        expect(manager.getDistortion).toEqual(jasmine.any(Function));
      });
      
      it('should offer a function to get the distortion radius', function() {
        expect(manager.getDistortionRadius).toBeDefined();
        expect(manager.getDistortionRadius).toEqual(jasmine.any(Function));
      });
      
      it('should offer a function to get the node limit', function() {
        expect(manager.getNodeLimit).toBeDefined();
        expect(manager.getNodeLimit).toEqual(jasmine.any(Function));
      });
      
      // Old interface might be unused!
      
      it('should offer a function to get the current font-size', function() {
        expect(manager.getFontSize).toBeDefined();
        expect(manager.getFontSize).toEqual(jasmine.any(Function));
      });
    
      it('should offer a function to get the current node-radius', function() {
        expect(manager.getRadius).toBeDefined();
        expect(manager.getRadius).toEqual(jasmine.any(Function));
      });
    
      it('should offer a function for zoom-in', function() {
        expect(manager.zoomIn).toBeDefined();
        expect(manager.zoomIn).toEqual(jasmine.any(Function));
      });
    
      it('should offer a function for zoom-out', function() {
        expect(manager.zoomOut).toBeDefined();
        expect(manager.zoomOut).toEqual(jasmine.any(Function));
      });
    });
    
    describe('default values', function() {
     
      var fontMax,
        fontMin,
        radMax,
        radMin,
        nodeMax,
        nodeMaxNoLabel,
        nodeMinLabel,
        nodeMin;
      
      
      beforeEach(function() {
        var labelSize = function (font) {
          return 60 * font * font;
        },
          circleSize = function (radius) {
            return 4 * radius * radius * Math.PI;
          };
        fontMax = 16;
        fontMin = 6;
        radMax = 25;
        radMin = 1;
        nodeMax = Math.floor(w * h / labelSize(fontMax));
        nodeMinLabel = Math.floor(w * h / labelSize(fontMin));
        nodeMaxNoLabel = Math.floor(w * h / circleSize((radMax - radMin) / 2 + radMin));
        nodeMin = Math.floor(w * h / circleSize(radMin));
      });
      
      it('should offer maximized values if no zoom happens', function() {
        expect(manager.getFontSize()).toEqual(fontMax);
        expect(manager.getRadius()).toEqual(radMax);
        expect(manager.getNodeLimit()).toEqual(nodeMax);
        expect(manager.getDistortion()).toBeCloseTo(0, 6);
      });
      
      it('should not be possible to zoom in if max-zoom is reached', function() {
        var oldFS = manager.getFontSize(),
          oldR = manager.getRadius(),
          oldNL = manager.getNodeLimit(),
          oldD = manager.getDistortion();
        manager.zoomIn();
        expect(manager.getFontSize()).toEqual(oldFS);
        expect(manager.getRadius()).toEqual(oldR);
        expect(manager.getNodeLimit()).toEqual(oldNL);
        expect(manager.getDistortion()).toEqual(oldD);
      });
      
      it('should be possible to zoom-out until minimal font-size is reached', function() {
        var oldFS,
          oldR,
          oldNL,
          oldD,
          loopCounter = 0;
        while (manager.getFontSize() > fontMin && manager.getFontSize() !== null) {
          oldFS = manager.getFontSize();
          oldR = manager.getRadius();
          oldNL = manager.getNodeLimit();
          oldD = manager.getDistortion();
          manager.zoomOut();
          expect(manager.getFontSize()).toBeLessThan(oldFS);
          expect(manager.getRadius()).toBeLessThan(oldR);
          expect(manager.getNodeLimit()).not.toBeLessThan(oldNL);
          expect(manager.getDistortion()).toBeGreaterThan(oldD);
          loopCounter++;
          if (loopCounter === 1000) {
            this.fail(new Error('The minimal font-size should have been reached'));
            break;
          }
        }
        if (manager.getFontSize() === null) {
          manager.zoomIn();
        }
        expect(manager.getFontSize()).toBeCloseTo(fontMin, 6);
        expect(manager.getRadius()).toBeCloseTo((radMax-radMin) / 2 + radMin, 6);
        expect(manager.getNodeLimit()).toBeCloseTo(nodeMinLabel, 6);
        //expect(manager.getDistortion()).toBeCloseTo(0, 6);
      });
      
      describe('with zoomlevel adjusted to minimal font-size', function() {
        
        beforeEach(function() {
          var loopCounter = 0;
          while (manager.getFontSize() > fontMin && manager.getFontSize() !== null) {
            manager.zoomOut();
            loopCounter++;
            if (loopCounter === 1000) {
              this.fail(new Error('The minimal font-size should have been reached'));
              break;
            }
          }
          if (manager.getFontSize() === null) {
            manager.zoomIn();
          }
        });
        
        it('should be able to zoom-in again', function() {
          var oldFS,
            oldR,
            oldNL,
            oldD,
            loopCounter = 0;
          while (manager.getFontSize() < fontMax) {
            oldFS = manager.getFontSize();
            oldR = manager.getRadius();
            oldNL = manager.getNodeLimit();
            oldD = manager.getDistortion();
            manager.zoomIn();
            expect(manager.getFontSize()).toBeGreaterThan(oldFS);
            expect(manager.getRadius()).toBeGreaterThan(oldR);
            expect(manager.getNodeLimit()).not.toBeGreaterThan(oldNL);
            expect(manager.getDistortion()).toBeLessThan(oldD);
            loopCounter++;
            if (loopCounter === 1000) {
              this.fail(new Error('The maximal font-size should have been reached'));
              break;
            }
          }
          expect(manager.getFontSize()).toEqual(fontMax);
          expect(manager.getRadius()).toEqual(radMax);
          expect(manager.getNodeLimit()).toEqual(nodeMax);
          expect(manager.getDistortion()).toBeCloseTo(0, 6);
        });
        
        it('should return null for font-size if further zoomed out', function() {
          manager.zoomOut();
          expect(manager.getFontSize()).toEqual(null);
        });
        
        it('should significantly increase the node limit if further zoomed out', function() {
          manager.zoomOut();
          expect(manager.getNodeLimit()).toBeGreaterThan(nodeMaxNoLabel);
        });
        
        it('should be able to zoom-out until minimal node radius is reached', function() {
          var oldR,
            oldNL,
            oldD,
            loopCounter = 0;
          while (manager.getRadius() > radMin) {
            oldR = manager.getRadius();
            oldNL = manager.getNodeLimit();
            oldD = manager.getDistortion();
            manager.zoomOut();
            expect(manager.getFontSize()).toEqual(null);
            expect(manager.getRadius()).toBeLessThan(oldR);
            expect(manager.getNodeLimit()).not.toBeLessThan(oldNL);
            expect(manager.getDistortion()).toBeGreaterThan(oldD);
            loopCounter++;
            if (loopCounter === 1000) {
              this.fail(new Error('The minimal font-size should have been reached'));
              break;
            }
          }
          expect(manager.getRadius()).toEqual(radMin);
          expect(manager.getNodeLimit()).toEqual(nodeMin);
          //expect(manager.getDistortion()).toBeCloseTo(0, 6);
        });
        
      });
      
      describe('with zoomlevel adjusted to maximal zoom out', function() {
        
        beforeEach(function() {
          var loopCounter = 0;
          while (manager.getRadius() > radMin) {
            manager.zoomOut();
            loopCounter++;
            if (loopCounter === 2000) {
              this.fail(new Error('The minimal zoom level should have been reached'));
              break;
            }
          }
        });
        
        it('should not be able to further zoom out', function() {
          var oldR = manager.getRadius(),
            oldNL = manager.getNodeLimit(),
            oldD = manager.getDistortion();
          manager.zoomOut();
          expect(manager.getRadius()).toEqual(oldR);
          expect(manager.getNodeLimit()).toEqual(oldNL);
          expect(manager.getDistortion()).toEqual(oldD);
        });
        
        it('should be able to zoom-in again', function() {
          var oldR,
            oldNL,
            oldD,
            loopCounter = 0;
          while (manager.getFontSize() === null) {
            oldR = manager.getRadius();
            oldNL = manager.getNodeLimit();
            oldD = manager.getDistortion();
            manager.zoomIn();
            expect(manager.getRadius()).toBeGreaterThan(oldR);
            expect(manager.getNodeLimit()).not.toBeGreaterThan(oldNL);
            expect(manager.getDistortion()).toBeLessThan(oldD);
            loopCounter++;
            if (loopCounter === 1000) {
              this.fail(new Error('The minimal font-size should have been reached'));
              break;
            }
          }
          expect(manager.getFontSize()).toBeCloseTo(fontMin, 6);
          expect(manager.getRadius()).toBeCloseTo((radMax-radMin) / 2 + radMin, 6);
          expect(manager.getNodeLimit()).toBeCloseTo(nodeMinLabel, 6);
          //expect(manager.getDistortion()).toBeCloseTo(0, 6);
        });
        
      });
    });
  });

  describe('testing user-defined values', function() {
    
  });

}());