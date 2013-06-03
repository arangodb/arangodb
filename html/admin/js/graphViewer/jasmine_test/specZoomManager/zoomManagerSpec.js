/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach, jasmine */
/*global describe, it, expect, spyOn */
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
    var domsvg,
      svg,
      g,
      nodeShaperMock,
      edgeShaperMock,
    
    simulateZoomOut = function () {
      helper.simulateScrollUpMouseEvent("outersvg");
    },
    
    simulateZoomIn = function () {
      helper.simulateScrollDownMouseEvent("outersvg");
    },

    lastNodeShaperCall = function() {
      return nodeShaperMock.activateLabel.mostRecentCall.args[0];
    },
    
    lastEdgeShaperCall = function() {
      return edgeShaperMock.activateLabel.mostRecentCall.args[0];
    },

    labelsAreInvisible = function () {
      return lastNodeShaperCall() === false;
    },
    
    labelsAreVisible = function () {
      return lastNodeShaperCall() === true;
    };

    beforeEach(function () {
      domsvg = document.createElement("svg");
      domsvg.id = "outersvg";
      document.body.appendChild(domsvg);
      svg = d3.select("svg");
      g = svg.append("g");
      g.attr("id", "svg");
      nodeShaperMock = {
        activateLabel: function() {},
        changeTo: function() {},
        updateNodes: function() {}
      };
      edgeShaperMock = {
        activateLabel: function() {},
        updateEdges: function() {}
      };
      spyOn(nodeShaperMock, "activateLabel");
      spyOn(nodeShaperMock, "changeTo");
      spyOn(nodeShaperMock, "updateNodes");
      
      spyOn(edgeShaperMock, "activateLabel");
      spyOn(edgeShaperMock, "updateEdges");
    });

    afterEach(function () {
      document.body.removeChild(domsvg);
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
      
      it('should throw an error if the svg is not given', function() {
        expect(function() {
          var s = new ZoomManager(10, 10);
        }).toThrow("A svg has to be given.");
      });
      
      it('should throw an error if the group is not given', function() {
        expect(function() {
          var s = new ZoomManager(10, 10, svg);
        }).toThrow("A group has to be given.");
      });
      
      it('should throw an error if the node shaper is not given', function() {
        expect(function() {
          var s = new ZoomManager(10, 10, svg, g);
        }).toThrow("The Node shaper has to be given.");
      });
      
      it('should throw an error if the edge shaper is not given', function() {
        expect(function() {
          var s = new ZoomManager(10, 10, svg, g, nodeShaperMock);
        }).toThrow("The Edge shaper has to be given.");
      });
      
      
      it('should not throw an error if mandatory information is given', function() {
        expect(function() {
          var s = new ZoomManager(10, 10, svg, g, nodeShaperMock, edgeShaperMock);
        }).not.toThrow();
      });
    });


    describe('setup with default values', function() {
    
      var w,
        h,
        manager;
    
      beforeEach(function() {
        w = 200;
        h = 200;
        manager = new ZoomManager(w, h, svg, g, nodeShaperMock, edgeShaperMock);
      });
    
      describe('the interface', function() {
      
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
      
      });
    
      describe('the zoom behaviour', function() {
     
        var fontMax,
          fontMin,
          radMax,
          radMin,
          nodeMax,
          nodeMaxNoLabel,
          nodeMinLabel,
          nodeMin,
          distRBase,
          minScale,
          toggleScale;
      
      
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
          radMin = 4;
          distRBase = 100;
          minScale = radMin / radMax;
          toggleScale = fontMin / fontMax;
          nodeMax = Math.floor(w * h / labelSize(fontMax));
          nodeMinLabel = Math.floor(w * h / labelSize(fontMin));
          nodeMaxNoLabel = Math.floor(w * h / circleSize((radMax - radMin) / 2 + radMin));
          nodeMin = Math.floor(w * h / circleSize(radMin));
        });
      
        it('should offer maximized values if no zoom happens', function() {
          expect(manager.getNodeLimit()).toEqual(nodeMax);
          expect(manager.getDistortion()).toBeCloseTo(0, 6);
          expect(manager.getDistortionRadius()).toEqual(distRBase);
        });
      
        it('should trigger the activateLabel function on each zoom-in event', function() {
          simulateZoomIn();
          expect(nodeShaperMock.activateLabel).toHaveBeenCalled();
          expect(edgeShaperMock.activateLabel).toHaveBeenCalled();
        });
        
        it('the zoom-out event should decrease the scale', function() {
          var oldSF = manager.scaleFactor();
          simulateZoomOut();
          expect(manager.scaleFactor()).toBeLessThan(oldSF);
        });
        
        it('the zoom-in event should increase the scale', function() {
          simulateZoomOut();
          simulateZoomOut();
          var oldSF = manager.scaleFactor();
          simulateZoomIn();
          expect(manager.scaleFactor()).toBeGreaterThan(oldSF);
        });
        
        it('should trigger the activateLabel function on each zoom-out event', function() {
          simulateZoomOut();
          expect(nodeShaperMock.activateLabel).toHaveBeenCalled();
          expect(edgeShaperMock.activateLabel).toHaveBeenCalled();
        });
      
        it('should not be possible to zoom in if max-zoom is reached', function() {
          var oldNL = manager.getNodeLimit(),
            oldD = manager.getDistortion(),
            oldDR = manager.getDistortionRadius(),
            oldSF = manager.scaleFactor();
          simulateZoomIn();
          expect(manager.getNodeLimit()).toEqual(oldNL);
          expect(manager.getDistortion()).toEqual(oldD);
          expect(manager.getDistortionRadius()).toEqual(oldDR);
          expect(manager.scaleFactor()).not.toBeLessThan(oldSF);
          
        });
      
        it('should be possible to zoom-out until labels are removed', function() {
          var oldNL,
            oldSF,
            oldD,
            oldDR,
            loopCounter = 0;
          do {
            oldNL = manager.getNodeLimit();
            oldD = manager.getDistortion();
            oldDR = manager.getDistortionRadius();
            oldSF = manager.scaleFactor();
            simulateZoomOut();
            expect(manager.getNodeLimit()).not.toBeLessThan(oldNL);
            expect(manager.getDistortion()).toBeGreaterThan(oldD);
            expect(manager.getDistortionRadius()).toBeGreaterThan(oldDR);
            expect(manager.scaleFactor()).toBeLessThan(oldSF);
            loopCounter++;
            if (loopCounter === 1000) {
              this.fail(new Error('The minimal font-size should have been reached'));
              break;
            }
          } while (labelsAreVisible());
          simulateZoomIn();
          expect(manager.getNodeLimit()).toBeCloseTo(nodeMinLabel, 6);
          //expect(manager.getDistortion()).toBeCloseTo(0, 6);
        });
      
        describe('with zoomlevel adjusted to minimal font-size', function() {
        
          beforeEach(function() {
            var loopCounter = 0;
            do {
              simulateZoomOut();
              loopCounter++;
              if (loopCounter === 1000) {
                this.fail(new Error('The minimal font-size should have been reached'));
                break;
              }
            } while (labelsAreVisible());
            simulateZoomIn();
          });
        
          it('should be able to zoom-in again', function() {
            var oldNL,
              oldD,
              oldDR,
              oldSF,
              loopCounter = 0;
            while (manager.scaleFactor() < 1) {
              oldNL = manager.getNodeLimit();
              oldD = manager.getDistortion();
              oldDR = manager.getDistortionRadius();
              oldSF = manager.scaleFactor();
              simulateZoomIn();
              expect(manager.getNodeLimit()).not.toBeGreaterThan(oldNL);
              expect(manager.getDistortion()).toBeLessThan(oldD);
              expect(manager.getDistortionRadius()).toBeLessThan(oldDR);
              expect(manager.scaleFactor()).toBeGreaterThan(oldSF);
              loopCounter++;
              if (loopCounter === 1000) {
                this.fail(new Error('The maximal font-size should have been reached'));
                break;
              }
            }
            expect(manager.scaleFactor()).toEqual(1);
            expect(manager.getNodeLimit()).toEqual(nodeMax);
            expect(manager.getDistortion()).toBeCloseTo(0, 6);
            expect(manager.getDistortionRadius()).toEqual(distRBase);
          });
        
          it('should remove the labels if further zoomed out', function() {
            simulateZoomOut();
            expect(lastNodeShaperCall()).toBeFalsy();
            expect(lastEdgeShaperCall()).toBeFalsy();
          });
        
          it('should significantly increase the node limit if further zoomed out', function() {
            simulateZoomOut();
            expect(manager.getNodeLimit()).toBeGreaterThan(nodeMaxNoLabel);
          });
        
          it('should be able to zoom-out until minimal node radius is reached', function() {
            var oldNL,
              oldD,
              oldDR,
              oldSF,
              loopCounter = 0;
            while (manager.scaleFactor() > minScale) {
              oldNL = manager.getNodeLimit();
              oldD = manager.getDistortion();
              oldDR = manager.getDistortionRadius();
              oldSF = manager.scaleFactor();
              simulateZoomOut();
              expect(manager.getNodeLimit()).not.toBeLessThan(oldNL);
              expect(manager.getDistortion()).toBeGreaterThan(oldD);
              expect(manager.getDistortionRadius()).toBeGreaterThan(oldDR);
              expect(manager.scaleFactor()).toBeLessThan(oldSF);
              loopCounter++;
              if (loopCounter === 1000) {
                this.fail(new Error('The minimal font-size should have been reached'));
                break;
              }
            }
            expect(manager.getNodeLimit()).toEqual(nodeMin);
            expect(manager.scaleFactor()).toEqual(minScale);
          });
        
        });
      
        describe('with zoomlevel adjusted to maximal zoom out', function() {
        
          beforeEach(function() {
            var loopCounter = 0;
            while (manager.scaleFactor() > minScale) {
              simulateZoomOut();
              loopCounter++;
              if (loopCounter === 2000) {
                this.fail(new Error('The minimal zoom level should have been reached'));
                break;
              }
            }
          });
        
          it('should not be able to further zoom out', function() {
            var oldNL = manager.getNodeLimit(),
              oldD = manager.getDistortion(),
              oldDR = manager.getDistortionRadius(),
              oldSF = manager.scaleFactor();
            simulateZoomOut();
            expect(manager.getNodeLimit()).toEqual(oldNL);
            expect(manager.getDistortion()).toEqual(oldD);
            expect(manager.getDistortionRadius()).toEqual(oldDR);
            expect(manager.scaleFactor()).toEqual(oldSF);
          });
        
          it('should be able to zoom-in again', function() {
            var oldNL,
              oldD,
              oldDR,
              oldSF,
              loopCounter = 0;
            while (labelsAreInvisible()) {
              oldNL = manager.getNodeLimit();
              oldD = manager.getDistortion();
              oldDR = manager.getDistortionRadius();
              oldSF = manager.scaleFactor();
              simulateZoomIn();
              expect(manager.getNodeLimit()).not.toBeGreaterThan(oldNL);
              expect(manager.getDistortion()).toBeLessThan(oldD);
              expect(manager.getDistortionRadius()).toBeLessThan(oldDR);
              expect(manager.scaleFactor()).toBeGreaterThan(oldSF);
              loopCounter++;
              if (loopCounter === 1000) {
                this.fail(new Error('The minimal font-size should have been reached'));
                break;
              }
            }
          });
        
        });
      });
      
      describe('the distortion behaviour', function() {
        
        it('should register fisheye distortion at the node shaper', function() {
          expect(nodeShaperMock.changeTo).toHaveBeenCalledWith({
            distortion: jasmine.any(Function)
          });
        });
        
        it('should update the nodes and edges on mouse move event', function() {
          helper.simulateMouseEvent("mousemove", "outersvg");
          expect(nodeShaperMock.updateNodes).toHaveBeenCalled();
          expect(edgeShaperMock.updateEdges).toHaveBeenCalled();
        });
        
      });
    });

    describe('testing user-defined values', function() {
    
    });

    it('if a nodelimit callback is defined it should be invoked on zoom-in', function() {
      var w = 200,
        h = 200,
        nl = -1 ,
        callback = function(n) {
          nl = n;
        },
        manager = new ZoomManager(w, h, svg, g, nodeShaperMock, edgeShaperMock, {}, callback);
        
      simulateZoomIn();
      expect(nl).toEqual(manager.getNodeLimit());
    });

    it('if a nodelimit callback is defined it should be invoked on zoom-out', function() {
      var w = 200,
        h = 200,
        nl = -1 ,
        callback = function(n) {
          nl = n;
        },
        manager = new ZoomManager(w, h, svg, g, nodeShaperMock, edgeShaperMock, {}, callback);
        
      simulateZoomOut();
      expect(nl).toEqual(manager.getNodeLimit());
    });
  });

}());