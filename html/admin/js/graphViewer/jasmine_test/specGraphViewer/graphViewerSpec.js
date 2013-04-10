/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global waitsFor, runs, waits */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
/*global GraphViewer*/

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


describe("Graph Viewer", function() {
  "use strict";
  var viewer,
  waittime = 100,
  svg,
  docSVG,
  
  clickOnNode = function(id) {
    helper.simulateMouseEvent("click", id);
  };
  
  beforeEach(function() {
    docSVG = document.createElement("svg");
    document.body.appendChild(docSVG);
    svg = d3.select("svg");
  });
  
  
  afterEach(function() {
    document.body.removeChild(docSVG);
  });
  
  describe('set up process', function() {
    
    var adapterConfig,
    layouterConfig;
    
    beforeEach(function() {
      adapterConfig = {type: "json", path: "../test_data/"};
      layouterConfig = {type: "force"};
    });
    
    it('should throw an error if the svg is not given or incorrect', function() {
      expect(
        function() {
          var t = new GraphViewer();
        }
      ).toThrow("SVG has to be given and has to be selected using d3.select");
      expect(
        function() {
          var t = new GraphViewer(docSVG);
        }
      ).toThrow("SVG has to be given and has to be selected using d3.select");
    });
    
    it('should throw an error if the width is not given or incorrect', function() {
      expect(
        function() {
          var t = new GraphViewer(svg);
        }
      ).toThrow("A width greater 0 has to be given");
      expect(
        function() {
          var t = new GraphViewer(svg, -10);
        }
      ).toThrow("A width greater 0 has to be given");
    });
    
    it('should throw an error if the height is not given or incorrect', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10);
        }
      ).toThrow("A height greater 0 has to be given");
      expect(
        function() {
          var t = new GraphViewer(svg, 10, -10);
        }
      ).toThrow("A height greater 0 has to be given");
    });
    
    
    it('should not throw an error if everything is given', function() {
      expect(
        function() {
          var t = new GraphViewer(svg, 10, 10);
        }
      ).not.toThrow();
    });
    
  });
  
  describe('set up with jsonAdapter, forceLayout, click Expand and default shapers', function() {
    
    var viewer;
    
    beforeEach(function() {
      var aconf = {type: "json", path: "../test_data/"},
      lconf = {type: "force"},
      evconf = {
        expand: {
          target: "nodes",
          type: "click",
          callback: function(){}
        }
      },
      config = {
        adapter: aconf,
        layouter: lconf,
        events: evconf
      };
      viewer = new GraphViewer(svg, 10, 10, config);
      
      this.addMatchers({
        toBeDisplayed: function() {
          var nodes = this.actual,
          nonDisplayed = [];
          this.message = function(){
            var msg = "Nodes: [";
            _.each(nonDisplayed, function(n) {
              msg += n + " ";
            });
            msg += "] are not displayed.";
            return msg;
          };
          _.each(nodes, function(n) {
            if ($("svg #" + n)[0] === undefined) {
              nonDisplayed.push(n);
            }
          });
          return nonDisplayed.length === 0;
        },
        toNotBeDisplayed: function() {
          var nodes = this.actual,
          displayed = [];
          this.message = function() {
            var msg = "Nodes: [";
            _.each(displayed, function(n) {
              msg += n + " ";
            });
            msg += "] are still displayed.";
            return msg;
          };
          _.each(nodes, function(n) {
            if ($("svg #" + n)[0] !== undefined) {
              displayed.push(n);
            }
          });
          return displayed.length === 0;
        },
        toBeConnectedTo: function(target) {
          var source = this.actual;
          this.message = function() {
            return source + " -> " + target + " edge, does not exist";
          };
          return $("svg #" + source + "-" + target)[0] !== undefined;
        },
        toNotBeConnectedTo: function(target) {
          var source = this.actual;
          this.message = function() {
            return source + " -> " + target + " edge, does still exist";
          };
          return $("svg #" + source + "-" + target)[0] === undefined;
        }
      });
      
    });
    
    it("should be able to load a root node", function() {
      runs (function() {
        viewer.loadGraph(0);
      });
    
      // Give it a second to load
      // Unfortunately there is no handle to check for changes
      waits(waittime);
    
      runs (function() {
        expect([0, 1, 2, 3, 4]).toBeDisplayed();
      });
    });
    
    describe('when a graph has been loaded', function() {
      beforeEach(function() {
      
        runs (function() {
          viewer.loadGraph(0);
        });
    
        waits(waittime);
      });
      
      /*
      it('should be able to rebind the events', function() {
        var called = false;
        
        runs(function() {
          var newEventConfig = {custom: [
            {
              target: "nodes",
              type: "click",
              func: function() {
                called = true;
              }
            }
          ]};
          viewer.rebind(newEventConfig);
          clickOnNode(1);
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The click event should have been triggered.");
        
        runs(function() {
          expect(called).toBeTruthy();
        });
        
      });
      */
      it("should be able to expand a node", function() {
    
        runs (function() {
          clickOnNode(1);
        });
    
        waits(waittime);
        
        runs (function() {
          expect([0, 1, 2, 3, 4, 5, 6, 7]).toBeDisplayed();
        });

      });
      
      it("should be able to collapse the root", function() {
    
        runs (function() {
          clickOnNode(0);
        });
    
        waits(waittime);
    
        runs (function() {
          // Load 1 Nodes: Root
          expect([0]).toBeDisplayed();
          expect([1, 2, 3, 4]).toNotBeDisplayed();
        });

      });
      
      it("should be able to load a different graph", function() {
        runs (function() {
          viewer.loadGraph(42);
        });
    
        waits(waittime);
    
        runs (function() {
          expect([42, 43, 44, 45]).toBeDisplayed();
          expect([0, 1, 2, 3, 4]).toNotBeDisplayed();
        });
      
      });
      
      describe("when a user rapidly expand nodes", function() {
        
        beforeEach(function() {
          runs (function() {
            clickOnNode(1);
            clickOnNode(2);
            clickOnNode(3);
            clickOnNode(4);
          });
    
          waits(waittime);
          
          it("the graph should still be correct", function() {
            expect([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12]).toBeDisplayed();
          });
        });
      });
      
      describe("when a user rapidly expand and collapses nodes", function() {
      
        beforeEach(function() {
          runs (function() {
            clickOnNode(1);
            clickOnNode(2);
            clickOnNode(3);
            clickOnNode(4);
          });
        
          // Wait a gentle second for all nodes to expand properly
          waits(waittime);

          runs(function() {
            clickOnNode(1);
            clickOnNode(4);
          });
        
          waits(waittime);
        });
        
        it("the graph should still be correct", function() {
          expect([0, 1, 2, 3, 4, 8, 9]).toBeDisplayed();
          expect([5, 6, 7, 12]).toNotBeDisplayed();
        });
      });
      
      describe("when an undirected circle has been loaded", function() {
        
        beforeEach(function() {

          runs (function() {
            clickOnNode(2);
            clickOnNode(3);
          });
          
          waits(waittime);
          
        });
        
        it("the basis should be correct", function() {
          expect([0, 1, 2, 3, 4, 8, 9]).toBeDisplayed();
        });
        
        it("should be able to collapse one node "
         + "without removing the double referenced one", function() {
      
          runs (function() {
            clickOnNode(2);
          });
      
          waits(waittime);
      
          runs (function() {
            expect([2, 3, 8]).toBeDisplayed();
            expect(2).toNotBeConnectedTo(8);
            expect(3).toBeConnectedTo(8);
          });
        });
        
        
        it("should be able to collapse the other node "
         + "without removing the double referenced one", function() {
      
          runs (function() {
            clickOnNode(3);
          });
      
          waits(waittime);
      
          runs (function() {
            expect([2, 3, 8]).toBeDisplayed();
            expect(3).toNotBeConnectedTo(8);
            expect(2).toBeConnectedTo(8);
          });
        });
        
        it("should be able to collapse the both nodes "
         + "and remove the double referenced one", function() {
      
          runs (function() {
            clickOnNode(3);
            clickOnNode(2);
          });
      
          waits(waittime);
      
          runs (function() {
            expect([2, 3]).toBeDisplayed();
            expect([8]).toNotBeDisplayed();
            expect(3).toNotBeConnectedTo(8);
            expect(2).toNotBeConnectedTo(8);
          });
        });
        
      });
      
      describe("when a complex graph has been loaded", function() {
        
        beforeEach(function() {
          runs(function() {
            clickOnNode(1);
            clickOnNode(4);
            clickOnNode(2);
            clickOnNode(3);
          });
          waits(waittime);
        });
        
        it("should be able to collapse a node "
        + "referencing a node connected to a subgraph", function() {
          
          runs(function() {         
            clickOnNode(1);
          });
          
          waits(waittime);
          
          runs(function() {
            
            expect([0, 1, 2, 3, 4, 5, 8, 9, 12]).toBeDisplayed();
            expect([6, 7, 10, 11]).toNotBeDisplayed();
            
            expect(0).toBeConnectedTo(1);
            expect(0).toBeConnectedTo(2);
            expect(0).toBeConnectedTo(3);
            expect(0).toBeConnectedTo(4); 
            expect(1).toNotBeConnectedTo(5);
            
            expect(2).toBeConnectedTo(8);
            
            expect(3).toBeConnectedTo(8);
            expect(3).toBeConnectedTo(9);
            
            expect(4).toBeConnectedTo(5);
            expect(4).toBeConnectedTo(12);
          });
        });
      });
      
    });
  });
});