/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine*/
/*global runs, waitsFor, spyOn, waits */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper, mocks, JSONAdapter:true, uiMatchers*/
/*global GraphViewerUI, NodeShaper, EdgeShaper*/

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
    adapterConfig,
    adapterMockCall;
    
    beforeEach(function() {
      //Mock for jsonAdapter
      window.communicationMock(spyOn);
      var Tmp = JSONAdapter;
      JSONAdapter = function (jsonPath, nodes, edges, width, height) {
        var r = new Tmp(jsonPath, nodes, edges, width, height);
        r.loadInitialNodeByAttributeValue = function(attribute, value, callback) {
          adapterMockCall = {
            attribute: attribute,
            value: value
          };
        };
        r.getCollections = function(callback) {
          callback(["nodes"], ["edges"]);
        };
        return r;
      };
      //Mock for ZoomManager
      if (window.ZoomManager === undefined) {
        window.ZoomManager = {};
      }
      spyOn(window, "ZoomManager");
      div = document.createElement("div");
      div.id = "contentDiv";
      div.style.width = "200px";
      div.style.height = "200px";
      adapterConfig = {type: "json", path: "../test_data/"};
      document.body.appendChild(div);
      ui = new GraphViewerUI(div, adapterConfig);
      uiMatchers.define(this);
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
    
    it('should automatically start the ZoomManager', function() {
      expect(window.ZoomManager).toHaveBeenCalledWith(
        140,
        200,
        jasmine.any(Object),
        jasmine.any(Object),
        jasmine.any(NodeShaper),
        jasmine.any(EdgeShaper),
        {},
        jasmine.any(Function)
      );
    });
    
    describe('checking the toolbox', function() {
      var toolboxSelector = "#contentDiv #toolbox";
            
      it('should append the toolbox', function() {
        expect($(toolboxSelector).length).toEqual(1);
      });
      
      it('should contain the objects from eventDispatcher', function() {
        expect($(toolboxSelector + " #control_event_drag").length).toEqual(1);
        expect($(toolboxSelector + " #control_event_edit").length).toEqual(1);
        expect($(toolboxSelector + " #control_event_expand").length).toEqual(1);
        expect($(toolboxSelector + " #control_event_delete").length).toEqual(1);
        expect($(toolboxSelector + " #control_event_connect").length).toEqual(1);
      });
      
      it('should have the correct layout', function() {
        expect($(toolboxSelector)[0]).toConformToToolboxLayout();
      });
      
      /* Archive
      it('should create the additional mouse-icon box', function() {
        var pointerBox = $("#contentDiv #mousepointer");
        expect(pointerBox.length).toEqual(1);
        expect(pointerBox[0]).toBeTag("div");
        expect(pointerBox[0]).toBeOfClass("mousepointer");
      });
      
      it('should position the mouse-icon box next to the mouse pointer', function() {
        var x = 40,
          y = 50,
          pointerBox = $("#contentDiv #mousepointer");
          
        helper.simulateMouseMoveEvent("graphViewerSVG", x, y);
        expect(pointerBox.offset().left).toEqual(x + 7);
        expect(pointerBox.offset().top).toEqual(y + 12);
        
        x = 66;
        y = 33;
        
        helper.simulateMouseMoveEvent("graphViewerSVG", x, y);
        expect(pointerBox.offset().left).toEqual(x + 7);
        expect(pointerBox.offset().top).toEqual(y + 12);
      });
      */
    });
    
    describe('checking the menubar', function() {
      
      it('should append the menubar', function() {
        expect($("#contentDiv #menubar").length).toEqual(1);
      });
      
      it('should contain a field to load a node by attribute', function() {
        var barSelector = "#contentDiv #menubar",
          attrfield = $(barSelector + " #attribute")[0],
          valfield = $(barSelector + " #value")[0],
          btn = $(barSelector + " #loadnode")[0];
        expect($(barSelector + " #attribute").length).toEqual(1);
        expect($(barSelector + " #value").length).toEqual(1);
        expect($(barSelector + " #loadnode").length).toEqual(1);
        expect(attrfield).toBeTag("input");
        expect(attrfield.type).toEqual("text");
        expect(attrfield.className).toEqual("input-mini searchByAttribute");
        expect(attrfield.placeholder).toEqual("key");
        expect(valfield).toBeTag("input");
        expect(valfield.type).toEqual("text");
        expect(valfield.className).toEqual("searchInput");
        expect(valfield.placeholder).toEqual("value");
        expect(btn).toBeTag("img");
        expect(btn.width).toEqual(16);
        expect(btn.height).toEqual(16);
        expect(btn.className).toEqual("searchSubmit");
      });
      
      it('should contain a position for the layout buttons', function() {
        expect($("#contentDiv #menubar #modifiers").length).toEqual(1);
        expect($("#contentDiv #menubar #modifiers")[0].className).toEqual("pull-right");
      });
      
      /*
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
      */
      /*
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
      */
      /*
      it('should contain a menu for the adapter', function() {
        var menuSelector = "#contentDiv #menubar #adaptermenu";
        expect($(menuSelector).length).toEqual(1);
        expect($(menuSelector)[0]).toBeADropdownMenu();
        expect($(menuSelector +  " #control_collections").length).toEqual(1);
      });
      */
      /*
      it('should contain a menu for the layouter', function() {
        var menuSelector = "#contentDiv #menubar #layoutermenu";
        expect($(menuSelector).length).toEqual(1);
        expect($(menuSelector)[0]).toBeADropdownMenu();
        expect($(menuSelector + " #control_gravity").length).toEqual(1);
        expect($(menuSelector + " #control_distance").length).toEqual(1);
        expect($(menuSelector + " #control_charge").length).toEqual(1);
      });
      */
      
      it('should contain a general configure menu', function() {
        var menuSelector = "#contentDiv #menubar #configuremenu";
        expect($(menuSelector).length).toEqual(1);
        expect($(menuSelector)[0]).toBeADropdownMenu();
        expect($("> button", menuSelector).text()).toEqual("Configure ");
        expect($(menuSelector +  " #control_adapter_collections").length).toEqual(1);
        expect($(menuSelector +  " #control_node_labelandcolour").length).toEqual(1);
      });
      
      it('should have the same layout as the web interface', function() {
        var header = div.children[0],
          transHeader = header.firstChild,
          searchField = transHeader.children[0],
          
          content = div.children[1];
        expect(header).toBeTag("ul");
        expect(header.id).toEqual("menubar");
        expect(header.className).toEqual("thumbnails2");
        expect(transHeader).toBeTag("div");
        expect(transHeader.id).toEqual("transparentHeader");
        
        expect(searchField).toBeTag("div");
        expect(searchField.id).toEqual("transparentPlaceholder");
        expect(searchField.className).toEqual("pull-left");
        expect(searchField.children[0].id).toEqual("attribute");
        expect(searchField.children[1]).toBeTag("span");
        expect(searchField.children[1].textContent).toEqual("==");
        expect(searchField.children[2].id).toEqual("value");
        expect(searchField.children[3].id).toEqual("loadnode");
      });
    });
    
    describe('checking the node colour mapping list', function() {
      
      var map;
      
      beforeEach(function() {
        map = $("#contentDiv #node_colour_list");
      });
      
      it('should append the list', function() {
        expect(map.length).toEqual(1);
      });
      
      it('should be positioned in the top-right corner of the svg', function() {
        expect(map.css("position")).toEqual("absolute");
        var leftPos = $("#contentDiv svg").position().left,
        topPos = $("#contentDiv svg").position().top;
        leftPos += $("#contentDiv svg").width();
        if (leftPos === Math.round(leftPos)) {
          expect(map.css("left")).toEqual(leftPos + "px");
        } else {
          expect(map.css("left")).toEqual(leftPos.toFixed(1) + "px");
        }
        if (topPos === Math.round(topPos)) {
          expect(map.css("top")).toEqual(topPos + "px");
        } else {
          expect(map.css("top")).toEqual(topPos.toFixed(1) + "px");
        }
      });
      
    });
    
    describe('checking to load a graph', function() {
      
      var waittime = 200;
      
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
          }
        });
      });
      
      it('should load the graph by _id', function() {
        
        runs(function() {
          $("#contentDiv #menubar #value").attr("value", "0");
          helper.simulateMouseEvent("click", "loadnode");
        });
        
        waits(waittime);
        
        runs(function() {
          expect([0, 1, 2, 3, 4]).toBeDisplayed();
        });
        
      });
      
      it('should load the graph on pressing enter', function() {
        
        runs(function() {
          $("#contentDiv #menubar #value").attr("value", "0");
          var e = $.Event("keypress");
          e.keyCode = 13;
          $("#value").trigger(e);
        });
        
        waits(waittime);
        
        runs(function() {
          expect([0, 1, 2, 3, 4]).toBeDisplayed();
        });
        
      });
      
      it('should load the graph by attribute and value', function() {
        
        runs(function() {
          adapterMockCall = {};
          $("#contentDiv #menubar #attribute").attr("value", "name");
          $("#contentDiv #menubar #value").attr("value", "0");
          helper.simulateMouseEvent("click", "loadnode");
          expect(adapterMockCall).toEqual({
            attribute: "name",
            value: "0"
          });
        });
        
      });
      
    });
    
    describe('set up with jsonAdapter and click Expand rest default', function() {
      // This waittime is rather optimistic, on a slow machine this has to be increased
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
          $("#contentDiv #menubar #value").attr("value", "0");
          helper.simulateMouseEvent("click", "loadnode");
          helper.simulateMouseEvent("click", "control_event_expand");
        });

        waits(waittime);
      
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
          $("#contentDiv #menubar #value").attr("value", "42");
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