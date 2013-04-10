/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine*/
/*global runs, waitsFor, spyOn, waits */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper, mocks*/
/*global GraphViewerUI*/

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

  describe('Graph Viewer UI', function () {
    
    var div,
    ui,
    adapterConfig;
    
    beforeEach(function() {
      div = document.createElement("div");
      div.id = "contentDiv";
      adapterConfig = {type: "json", path: "../test_data/"};
      document.body.appendChild(div);
      ui = new GraphViewerUI(div, adapterConfig);
      this.addMatchers({
        toBeADropdownMenu: function() {
          var div = this.actual,
            btn = div.children[0],
            list = div.children[1],
            msg = "";
          this.message = function() {
            return msg;
          };
          if (div.className !== "dropdown") {
            msg = "div class to be dropdown";
            return false;
          }
          // Check the toggle button
          if (btn === undefined || btn.tagName.toLowerCase() !== "a") {
            msg = "first element has to be a link";
            return false;
          }
          if (btn.className !== "dropdown-toggle") {
            msg = "first elements class to be dropdown-toggle";
            return false;
          }
          if (btn.role !== "button") {
            msg = "first elements role to be button";
            return false;
          }
          if (btn["data-toggle"] !== "dropdown") {
            msg = "first elements data-toggle to be dropdown";
            return false;
          }
          if (btn["data-target"] !== "#") {
            msg = "first elements data-target to be a link";
            return false;
          }
          if (btn.children[0].tagName.toLowerCase() !== "b"
            && btn.children[0].className !== "caret") {
            msg = "first element to contain a caret";
            return false;
          }

          // Check the list
          if (list.className !== "dropdown-menu") {
            msg = "list element to be of class dropdown-menu";
            return false;
          }
          if (list.role !== "menu") {
            msg = "list elements role to be menu";
            return false;
          }
          if (list["aria-labelledby"] !== btn.id) {
            msg = "list elements aria-labelledby to be same as button id";
            return false;
          }
          return true;
        }
      });
    });
    
    afterEach(function() {
      document.body.removeChild(div);
    });
    
    it('should throw an error if no container element is given', function() {
      expect(
        function() {
          var t = new GraphViewerUI();
        }
      ).toThrow("A parent element has to be given.");
    });
    
    it('should throw an error if the container element has no id', function() {
      expect(
        function() {
          var t = new GraphViewerUI(document.createElement("div"));
        }
      ).toThrow("The parent element needs an unique id.");
    });
    
    it('should throw an error if the adapter config is not given', function() {
      expect(
        function() {
          var t = new GraphViewerUI(div);
        }
      ).toThrow("An adapter configuration has to be given");
    });
    
    it('should append a svg to the given parent', function() {
      expect($("#contentDiv svg").length).toEqual(1);
    });
    
    describe('checking the toolbox', function() {
      var toolboxSelector = "#contentDiv #toolbox";
      
      it('should append the toolbox', function() {
        expect($(toolboxSelector).length).toEqual(1);
      });
      
      it('should contain the objects from eventDispatcher', function() {
        expect($(toolboxSelector + " #control_drag").length).toEqual(1);
        expect($(toolboxSelector + " #control_edit").length).toEqual(1);
        expect($(toolboxSelector + " #control_expand").length).toEqual(1);
        expect($(toolboxSelector + " #control_delete").length).toEqual(1);
        expect($(toolboxSelector + " #control_connect").length).toEqual(1);
      });
    });
    
    describe('checking the menubar', function() {
      
      it('should append the menubar', function() {
        expect($("#contentDiv #menubar").length).toEqual(1);
      });
      
      it('should contain a field to load a node', function() {
        expect($("#contentDiv #menubar #nodeid").length).toEqual(1);
        expect($("#contentDiv #menubar #loadnode").length).toEqual(1);
        var field = $("#contentDiv #menubar #nodeid")[0],
          btn = $("#contentDiv #menubar #loadnode")[0];
        expect(field.tagName.toLowerCase()).toEqual("input");
        expect(field.className).toEqual("searchInput");
        expect(btn.tagName.toLowerCase()).toEqual("img");
        expect(btn.width).toEqual(16);
        expect(btn.height).toEqual(16);
        expect(btn.src).toEqual("img/enter_icon");
      });
      
      it('should contain a menu for the node shapes', function() {
        var menuSelector = "#contentDiv #menubar #nodeshapermenu";
        expect($(menuSelector).length).toEqual(1);
        expect($(menuSelector)[0]).toBeADropdownMenu();
        expect($(menuSelector +  " #control_none").length).toEqual(1);
        expect($(menuSelector +  " #control_circle").length).toEqual(1);
        expect($(menuSelector +  " #control_rect").length).toEqual(1);
        expect($(menuSelector +  " #control_label").length).toEqual(1);
        expect($(menuSelector +  " #control_singlecolour").length).toEqual(1);
        expect($(menuSelector +  " #control_attributecolour").length).toEqual(1);
        expect($(menuSelector +  " #control_expandcolour").length).toEqual(1);
      });
      
      it('should contain a menu for the edge shapes', function() {
        var menuSelector = "#contentDiv #menubar #edgeshapermenu";
        expect($(menuSelector).length).toEqual(1);
        expect($(menuSelector)[0]).toBeADropdownMenu();
        expect($(menuSelector + " #control_none").length).toEqual(1);
        expect($(menuSelector + " #control_arrow").length).toEqual(1);
        expect($(menuSelector + " #control_label").length).toEqual(1);
        expect($(menuSelector + " #control_singlecolour").length).toEqual(1);
        expect($(menuSelector + " #control_attributecolour").length).toEqual(1);
        expect($(menuSelector + " #control_gradientcolour").length).toEqual(1);
      });
      
      it('should contain a menu for the adapter', function() {
        var menuSelector = "#contentDiv #menubar #adaptermenu";
        expect($(menuSelector).length).toEqual(1);
        expect($(menuSelector)[0]).toBeADropdownMenu();
        expect(false).toBeTruthy();
      });
      
    });
    
    
    describe('set up with jsonAdapter and click Expand rest default', function() {
    
      var waittime = 100,
    
      clickOnNode = function(id) {
        helper.simulateMouseEvent("click", id);
      };
    
      beforeEach(function() {

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
      
        runs (function() {
          $("#contentDiv #menubar #nodeid").attr("value", "0");
          helper.simulateMouseEvent("click", "loadnode");
          helper.simulateMouseEvent("click", "control_expand");
        });

        waits(waittime);
      
      });
    
      it('should have loaded the graph', function() {
        expect([0, 1, 2, 3, 4]).toBeDisplayed();
      });
    
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
          $("#contentDiv #menubar #nodeid").attr("value", "42");
          helper.simulateMouseEvent("click", "loadnode");
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

}());