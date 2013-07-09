/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine*/
/*global runs, waitsFor, spyOn, waits */
/*global document, window, helper */
/*global $, _, d3*/
/*global GraphViewerWidget, mocks*/

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

  describe('Graph Viewer Widget', function () {
  
    var width, height;
  
    beforeEach(function() {
      var b = document.body;
      width = 200;
      height = 200;
      b.style.height = height + "px";
      b.style.width =  width + "px";
      while (b.firstChild) {
          b.removeChild(b.firstChild);
      }
      
      this.addMatchers({
        toBeTag: function(name) {
          var el = this.actual;
          this.message = function() {
            return "Expected " + el.tagName.toLowerCase() + " to be a " + name; 
          };
          return el.tagName.toLowerCase() === name;
        },
        
        toBeOfClass: function(name) {
          var el = this.actual;
          this.message = function() {
            return "Expected " + el.className + " to be " + name; 
          };
          return el.className === name;
        },
        
        toBeADropdownMenu: function() {
          var div = this.actual,
            btn = div.children[0],
            list = div.children[1],
            msg = "";
          this.message = function() {
            return "Expected " + msg;
          };
          expect(div).toBeOfClass("btn-group");
          expect(btn).toBeTag("button");
          expect(btn).toBeOfClass("btn btn-inverse btn-small dropdown-toggle");
          if (btn.getAttribute("data-toggle") !== "dropdown") {
            msg = "first elements data-toggle to be dropdown";
            return false;
          }
          if (btn.children[0].tagName.toLowerCase() !== "span"
            && btn.children[0].getAttribute("class") !== "caret") {
            msg = "first element to contain a caret";
            return false;
          }

          // Check the list
          if (list.getAttribute("class") !== "dropdown-menu") {
            msg = "list element to be of class dropdown-menu";
            return false;
          }
          return true;
        }
      });
    });
    
    afterEach(function() {
      var b = document.body;
      while (b.firstChild) {
          b.removeChild(b.firstChild);
      }
    });
  
    describe('setup process', function() {
      
      it('should append an svg to the body', function() {
        var b = document.body,
          ui = new GraphViewerWidget(),
          gsvg = $("#graphViewerSVG", $(b));
        expect(gsvg.length).toEqual(1);
        expect(gsvg.get(0)).toBeTag("svg");
      });
      
      it('should create a graphViewer with body dimensions and foxx adapter', function() {
        spyOn(window, "GraphViewer").andCallThrough();
        var b = document.body,
          ui = new GraphViewerWidget();
        expect(window.GraphViewer).wasCalledWith(
          jasmine.any(Object),
          b.offsetWidth,
          b.offsetHeight,
          {
          	type: "foxx",
          	route: ".",
            width: width,
            height: height
          },
          jasmine.any(Object)
        );
      });
      
      it('should try to load a starting node if one is given', function() {
        var mockObj = {
            loadNode: function() {},
            explore: function() {}
          },
          startNode = "nodes/123",
          ui;
        
        spyOn(window, "FoxxAdapter").andCallFake(function() {
          return mockObj;
        });
        spyOn(mockObj, "loadNode");
        ui = new GraphViewerWidget({}, startNode);
        expect(mockObj.loadNode).wasCalledWith(startNode, jasmine.any(Function));

      });
      
      it('should not try to load a starting node if none is given', function() {
        var mockObj = {
            loadNode: function() {},
            explore: function() {}
          },
          ui;
        spyOn(window, "FoxxAdapter").andCallFake(function() {
          return mockObj;
        });
        spyOn(mockObj, "loadNode");
        ui = new GraphViewerWidget({});
        expect(mockObj.loadNode).wasNotCalled();
      });
      
    });
  
    describe('testing mouse actions', function() {
      var disp;
      
      beforeEach(function() {
        var OrigDisp = window.EventDispatcher;
        spyOn(window, "EventDispatcher").andCallFake(function(ns, es, conf) {
          disp = new OrigDisp(ns, es, conf);
          spyOn(disp, "rebind");
          return disp;
        });
      });
      
      it('should be able to allow zoom', function() {
        spyOn(window, "ZoomManager");
        var config = {
            zoom: true
          },
          ui = new GraphViewerWidget(config);
        expect(window.ZoomManager).wasCalled();
      });
      
      it('should not configure zoom if it is undefined', function() {
        spyOn(window, "ZoomManager");
        var ui = new GraphViewerWidget();
        expect(window.ZoomManager).wasNotCalled();
      });
      
      it('should not configure zoom if it is forbidden', function() {
        spyOn(window, "ZoomManager");
        var config = {
            zoom: false
          },
          ui = new GraphViewerWidget(config);
        expect(window.ZoomManager).wasNotCalled();
      });
      
      it('should be able to bind drag initially', function() {
        var OrigLayouter = window.ForceLayouter,
          layouter,
          actions = {
            drag: true
          },
          config = {
            actions: actions
          },
          ui;
        spyOn(window, "ForceLayouter").andCallFake(function(c) {
          layouter = new OrigLayouter(c);
          spyOn(layouter, "drag");
          return layouter;
        });

        ui = new GraphViewerWidget(config);
        expect(disp.rebind).wasCalledWith("nodes", {drag: layouter.drag});
        expect(disp.rebind).wasCalledWith("edges", undefined);
        expect(disp.rebind).wasCalledWith("svg", undefined);
      });
      
      it('should be able to bind edit initially', function() {
        var actions = {
          edit: true
        },
        config = {
          actions: actions
        },
        ui = new GraphViewerWidget(config);
        expect(disp.rebind).wasCalledWith("nodes", {click: jasmine.any(Function)});
        expect(disp.rebind).wasCalledWith("edges", {click: jasmine.any(Function)});
        expect(disp.rebind).wasCalledWith("svg", undefined);
      });
      
      it('should be able to bind create initially', function() {
        var actions = {
          create: true
        },
        config = {
          actions: actions
        },
        ui = new GraphViewerWidget(config);
        expect(disp.rebind).wasCalledWith("nodes", {
          mousedown: jasmine.any(Function),
          mouseup: jasmine.any(Function)
        });
        expect(disp.rebind).wasCalledWith("edges", undefined);
        expect(disp.rebind).wasCalledWith("svg", {
          click: jasmine.any(Function),
          mouseup: jasmine.any(Function)
        });
      });
      
      it('should be able to bind remove initially', function() {
        var actions = {
          remove: true
        },
        config = {
          actions: actions
        },
        ui = new GraphViewerWidget(config);
        expect(disp.rebind).wasCalledWith("nodes", {
          click: jasmine.any(Function)
        });
        expect(disp.rebind).wasCalledWith("edges", {
          click: jasmine.any(Function)
        });
        expect(disp.rebind).wasCalledWith("svg", undefined);
      });
      
      it('should be able to bind expand initially', function() {
        var actions = {
          expand: true
        },
        config = {
          actions: actions
        },
        ui = new GraphViewerWidget(config);
        expect(disp.rebind).wasCalledWith("nodes", {
          click: jasmine.any(Function)
        });
        expect(disp.rebind).wasCalledWith("edges", undefined);
        expect(disp.rebind).wasCalledWith("svg", undefined);
      });
      
      
    });
    
    describe('testing toolbox', function() {
      var toolboxSelector = "#toolbox";
      
      beforeEach(function() {
        this.addMatchers({
          toConformToToolboxLayout: function() {
            var box = this.actual;
            expect(box).toBeTag("div");
            expect(box.id).toEqual("toolbox");
            expect(box).toBeOfClass("btn-group btn-group-vertical pull-left toolbox");
            _.each(box.children, function(group) {
              expect(group).toBeTag("div");
              expect(group).toBeOfClass("btn btn-group");
              expect(group.children.length).toEqual(2);
              // Correctness of buttons is checked in eventDispatcherUISpec.
            });
            return true;
          }
        });
      });
      
      it('should append the toolbox if any tool is added', function() {
        var toolboxConf = {
            expand: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config);
        expect($(toolboxSelector).length).toEqual(1);
      });
      
      it('should reduce the size of the svg', function() {
        var toolboxConf = {
            expand: true
          },
          config = {
            toolbox: toolboxConf
          },
          b,
          ui;

        spyOn(window, "GraphViewer").andCallThrough();
        b = document.body;
        ui = new GraphViewerWidget(config);
        expect(window.GraphViewer).wasCalledWith(
          jasmine.any(Object),
          b.offsetWidth - 43,
          b.offsetHeight,
          {
          	type: "foxx",
          	route: ".",
            width: width - 43,
            height: height
          },
          jasmine.any(Object)
        );
      });
      
      
      it('should create the additional mouse-icon box', function() {
        var toolboxConf = {
            expand: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config),
          pointerBox = $("#mousepointer");
        expect(pointerBox.length).toEqual(1);
        expect(pointerBox[0]).toBeTag("div");
        expect(pointerBox[0]).toBeOfClass("mousepointer");
      });
      
      it('should position the mouse-icon box next to the mouse pointer', function() {
        var toolboxConf = {
            expand: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config),
          x = 40,
          y = 50,
          pointerBox = $("#mousepointer");
          
        helper.simulateMouseMoveEvent("graphViewerSVG", x, y);
        expect(pointerBox.offset().left).toEqual(x + 7);
        expect(pointerBox.offset().top).toEqual(y + 12);
        
        x = 66;
        y = 33;
        
        helper.simulateMouseMoveEvent("graphViewerSVG", x, y);
        expect(pointerBox.offset().left).toEqual(x + 7);
        expect(pointerBox.offset().top).toEqual(y + 12);
      });
      
      it('should not add a toolbox if no buttons are defined', function() {
        var toolboxConf = {},
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config),
          pointerBox = $("#mousepointer");
        expect($(toolboxSelector).length).toEqual(0);
        expect(pointerBox.length).toEqual(0);
      });
      
      it('should be able to add the expand button', function() {
        var toolboxConf = {
            expand: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config);
          
        expect($(toolboxSelector + " #control_event_expand").length).toEqual(1);
        expect($(toolboxSelector)[0]).toConformToToolboxLayout();
      });
      
      it('should not add a toolbox if all buttons are defined false', function() {
        var toolboxConf = {
          expand: false
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config),
          pointerBox = $("#mousepointer");
        expect($(toolboxSelector).length).toEqual(0);
        expect(pointerBox.length).toEqual(0);
      });
      
      it('should be able to add the expand button', function() {
        var toolboxConf = {
            expand: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config);
          
        expect($(toolboxSelector + " #control_event_expand").length).toEqual(1);
        expect($(toolboxSelector)[0]).toConformToToolboxLayout();
      });
      
      it('should be able to add the drag button', function() {
        var toolboxConf = {
            drag: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config);
          
        expect($(toolboxSelector + " #control_event_drag").length).toEqual(1);
        expect($(toolboxSelector)[0]).toConformToToolboxLayout();
      });
      
      it('should be able to add the create buttons', function() {
        var toolboxConf = {
            create: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config);
        
        expect($(toolboxSelector + " #control_event_new_node").length).toEqual(1);  
        expect($(toolboxSelector + " #control_event_connect").length).toEqual(1);
        expect($(toolboxSelector)[0]).toConformToToolboxLayout();
      });
      
      it('should be able to add the edit button', function() {
        var toolboxConf = {
            edit: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config);
          
        expect($(toolboxSelector + " #control_event_edit").length).toEqual(1);
        expect($(toolboxSelector)[0]).toConformToToolboxLayout();
      });
      
      it('should be able to add the remove button', function() {
        var toolboxConf = {
            remove: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config);
          
        expect($(toolboxSelector + " #control_event_delete").length).toEqual(1);
        expect($(toolboxSelector)[0]).toConformToToolboxLayout();
      });
      
      it('should not add buttons configured as false', function() {
        var toolboxConf = {
            expand: false,
            drag: true
          },
          config = {
            toolbox: toolboxConf
          },
          ui = new GraphViewerWidget(config);
          
        expect($(toolboxSelector + " #control_event_expand").length).toEqual(0);
        expect($(toolboxSelector)[0]).toConformToToolboxLayout();
      });
      
    });
  
  });

}());