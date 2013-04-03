/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global runs, spyOn, waitsFor */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global ForceLayouter*/

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

  describe('Force Layouter', function () {

    var layouter,
    nodes,
    width,
    height,
    offset,
    linkDistance,
    gravity,
    standardConfig,
    edgeShaper,
    
    createNodeList = function(amount) {
      var i,
      nodes = [];
      for (i = 0; i < amount; i++) {
        nodes.push({"id": i});
      }
      return nodes;
    },
    timeOutTest = function() {
      var startTime;
      
      runs(function() {
        spyOn(layouter, 'stop').andCallThrough();
        startTime = (new Date()).getTime();
        layouter.start();
      });
        
      waitsFor(function() {
        return layouter.stop.calls.length > 0;
      }, "layouter should have been stopped", 2000);
        
      runs(function() {
        // List test if not time-outed
        var endTime = (new Date()).getTime(),
        duration = endTime - startTime;
        expect(duration).toBeInATimeCategory();
      });
    },
    positioningTest = function() {
      runs(function() {
        spyOn(layouter, 'stop').andCallThrough();
        layouter.start();
      });
        
      waitsFor(function() {
        return layouter.stop.calls.length > 0;
      }, "layouter should have been stopped", 2000);
        
      runs(function() {
        expect(nodes).toNotBeOffScreen();
      });
    },
    
    dummyNodeShaper = {
      "updateNodes": function() {
        var changeDist = 0;
        _.each(nodes, function(d) {
          changeDist += Math.abs(d.px - d.x) + Math.abs(d.py - d.y);
        });
        return changeDist;
      }
    };
    
    
    beforeEach(function() {
      width = 940;
      height = 640;
      offset = 10;
      linkDistance = 100;
      gravity = 0.5;
      
      standardConfig = {
        "nodes": [],
        "links": [],
        "width": width,
        "height": height,
        "gravity": gravity,
        "distance": linkDistance
      };
      
      this.addMatchers({
        toBeInATimeCategory: function() {
          var duration = this.actual;
          this.message = function(){
            layouter.stop();
            if (duration < 100) {
              return "terminates super fast (0.1 s)";
            }
            if (duration < 1000) {
              return "terminates fast (1 s)";
            }
            if (duration < 2000) {
              return "terminates quite fast (2 s)";
            }
            if (duration < 5000) {
              return "terminates in reasonable time (5 s)";
            }
            if (duration < 10000) {
              return "terminates in acceptable time (10 s)";
            }
            return "does not terminate in acceptable time";
          };
          return duration < 10000;
        },
        
        toBeCloseToNode: function (n2, threshold) {
          var n1 = this.actual,
            xdis = n1.x - n2.x,
            ydis = n1.y - n2.y,
            distance = Math.sqrt(xdis*xdis + ydis*ydis);
          this.message = function() {
            return "Node " + n1.id
              + " should be close to Node " + n2.id
              + " but distance is " + distance;
          };
          threshold = threshold || 100;
          return Math.abs(distance) < threshold;
        },
        
        toBeDistantToNode: function (n2, threshold) {
          var n1 = this.actual,
            xdis = n1.x - n2.x,
            ydis = n1.y - n2.y,
            distance = Math.sqrt(xdis*xdis + ydis*ydis);
          this.message = function() {
            if (distance - linkDistance < 0) {
              return "Node " + n1.id
                + " should be distant from Node " + n2.id
                + " but distance is to short (" + distance + ")";
            }
            return "Node " + n1.id
              + " should be distant from Node " + n2.id
              + " but distance is to long (" + distance + ")";
            
          };
          threshold = threshold || 100;
          return Math.abs(distance - linkDistance) < threshold;
        },
        
        toNotBeOffScreen: function () {
          var minx = Number.MAX_VALUE,
          miny = Number.MAX_VALUE,
          maxx = Number.MIN_VALUE,
          maxy = Number.MIN_VALUE,
          nodes = this.actual;
          _.each(nodes, function(node) {
            minx = Math.min(minx, node.x);
            miny = Math.min(miny, node.y);
            maxx = Math.max(maxx, node.x);
            maxy = Math.max(maxy, node.y);
          });
          this.message = function () {
            var msg = "Errors: \n";
            if (minx < offset) {
              msg += "Minimal x: " + minx + " < " + offset + ", ";
            }
            if (maxx >  width - offset) {
              msg += "Maximal x: " + maxx + " > " + (width - offset) + ", ";
            }
            if (miny > offset) {
              msg += "Minimal y: " + miny + " < " + offset + " ,";
            }
            if (maxy > height - offset) {
              msg += "Maximal y: " + maxy + " > " + (height - offset);
            }
            return msg;
          };
          return minx > offset
           && maxx < width - offset
           && miny > offset
           && maxy < height - offset;
        }
      });
      
    });
    
    it('should position the first node in the centre', function() {
      runs(function() {    
        standardConfig.nodes = createNodeList(1);
        nodes = standardConfig.nodes;
        edgeShaper = {"updateEdges": function(){}};
        layouter = new ForceLayouter(standardConfig);
        layouter.setCombinedUpdateFunction(dummyNodeShaper, edgeShaper);
        spyOn(layouter, 'stop').andCallThrough();
        layouter.start();
      });
      
      waitsFor(function() {
        return layouter.stop.calls.length > 0;
      }, "force should have been stopped", 10000);
        
      runs(function() {
        var center = {
          "id": "center",
          "x": width/2,
          "y": height/2
        };
        expect(nodes[0]).toBeCloseToNode(center);
      });
    });
    
    it('should position not linked nodes close to each other', function() {
      runs(function() {    
        nodes = createNodeList(4);
        standardConfig.nodes = nodes;
        edgeShaper = {"updateEdges": function(){}};
        layouter = new ForceLayouter(standardConfig);
        layouter.setCombinedUpdateFunction(dummyNodeShaper, edgeShaper);
        spyOn(layouter, 'stop').andCallThrough();
        layouter.start();
      });
      
      waitsFor(function() {
        return layouter.stop.calls.length > 0;
      }, "force should have been stopped", 10000);
        
      runs(function() {
        expect(nodes[0]).toBeCloseToNode(nodes[1]);
        expect(nodes[0]).toBeCloseToNode(nodes[2]);
        expect(nodes[0]).toBeCloseToNode(nodes[3]);
        expect(nodes[1]).toBeCloseToNode(nodes[2]);
        expect(nodes[1]).toBeCloseToNode(nodes[3]);
        expect(nodes[2]).toBeCloseToNode(nodes[3]);
      });
      
    });
    
    it('should keep distance between linked nodes', function() {
      runs(function() {    
        nodes = createNodeList(4);
        standardConfig.nodes = nodes;
        standardConfig.links = [
          {"source": nodes[0], "target": nodes[1]},
          {"source": nodes[0], "target": nodes[2]},
          {"source": nodes[0], "target": nodes[3]},
          {"source": nodes[1], "target": nodes[3]},
          {"source": nodes[2], "target": nodes[3]}
        ];
        edgeShaper = {"updateEdges": function(){}};
        layouter = new ForceLayouter(standardConfig);
        layouter.setCombinedUpdateFunction(dummyNodeShaper, edgeShaper);
        spyOn(layouter, 'stop').andCallThrough();
        layouter.start();
      });
      
      waitsFor(function() {
        return layouter.stop.calls.length > 0;
      }, "force should have been stopped", 10000);
        
      runs(function() {
        expect(nodes[0]).toBeDistantToNode(nodes[1]);
        expect(nodes[0]).toBeDistantToNode(nodes[2]);
        expect(nodes[0]).toBeDistantToNode(nodes[3]);
        expect(nodes[1]).toBeDistantToNode(nodes[3]);
        expect(nodes[2]).toBeDistantToNode(nodes[3]);
      });
      
    });
    
    it('should throw an error if nodes are not defined', function() {
      expect(function() {
        var tmp = new ForceLayouter({"links": []});
      }).toThrow("No nodes defined");
      expect(function() {
        var tmp = new ForceLayouter({"nodes": [],"links": []});
      }).not.toThrow("No nodes defined");
    });
    
    it('should throw an error if links are not defined', function() {
      expect(function() {
        var tmp = new ForceLayouter({"nodes": []});
      }).toThrow("No links defined");
      expect(function() {
        var tmp = new ForceLayouter({"nodes": [],"links": []});
      }).not.toThrow("No links defined");
    });
    
    
    describe('tested under normal conditions (50 nodes)', function() {
      beforeEach(function() {
        nodes = createNodeList(50);
        standardConfig.nodes = nodes;
        edgeShaper = {"updateEdges": function(){}};
        layouter = new ForceLayouter(standardConfig);
        layouter.setCombinedUpdateFunction(dummyNodeShaper, edgeShaper);
        
      });
      
      
      it('should not position a node offscreen', function() {
        positioningTest();
      });
      
      it('should terminate', function() {
        timeOutTest();
      });

    });
    
    describe('tested under heavy weight conditions (500 nodes)', function() {
      
      beforeEach(function() {
        nodes = createNodeList(500);
        standardConfig.nodes = nodes;
        edgeShaper = {"updateEdges": function(){}};
        layouter = new ForceLayouter(standardConfig);
        layouter.setCombinedUpdateFunction(dummyNodeShaper, edgeShaper);
        
      });
      
      
      it('should not position a node offscreen', function() {
        positioningTest();
      });
      
      
      it('should terminate', function() {
        timeOutTest();
      }); 
      
    });
    /*
    describe('tested under evil stress (5000 nodes)', function() {
      
      beforeEach(function() {
        nodes = createNodeList(5000);
        standardConfig.nodes = nodes;
        edgeShaper = {"updateEdges": function(){}};
        layouter = new ForceLayouter(standardConfig);
        layouter.setCombinedUpdateFunction(dummyNodeShaper, edgeShaper);
        
      });
      
      
      it('should not position a node offscreen', function() {
        positioningTest();
      });
      
      
      it('should terminate', function() {
        timeOutTest();
      });

      
    });
    
    */
    /*
    describe('tested by the devil himself (50000 nodes)', function() {
      
      beforeEach(function() {
        nodes = createNodeList(50000);
        standardConfig.nodes = nodes;
        edgeShaper = {"updateEdges": function(){}};
        layouter = new ForceLayouter(standardConfig);
        layouter.setCombinedUpdateFunction(dummyNodeShaper, edgeShaper);
        
      });
      
      
      it('should not position a node offscreen', function() {
        positioningTest();
      });
      
      
      it('should terminate', function() {
        timeOutTest();
      });
      
    });
    */
  });

}());