/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine*/
/*global runs, waitsFor, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper, mocks*/
/*global EventDispatcher, EventDispatcherControls, NodeShaper, EdgeShaper*/

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

  describe('Event Dispatcher UI', function () {
    var svg, dispatcherUI, list, $list,
    nodeShaper, edgeShaper, layouter,
    nodes, edges, adapter,
    mousePointerbox,
    
    addSpies = function() {
      spyOn(layouter, "drag");
      spyOn(adapter, "createNode");
      spyOn(adapter, "patchNode");
      spyOn(adapter, "deleteNode");
      spyOn(adapter, "createEdge");
      spyOn(adapter, "patchEdge");
      spyOn(adapter, "deleteEdge");
      spyOn(adapter, "loadNode");
      spyOn(adapter, "expandCommunity");
    };



    beforeEach(function () {
      nodes = [{
        _id: 1,
        x: 3,
        y: 4,
        _data: {
          _id: 1,
          _rev: 1,
          _key: 1,
          name: "Alice"
        }
      },{
        _id: 2,
        x: 1,
        y: 2,
        _data: {
          _rev: 2,
          _key: 2,
          _id: 2,
          name: {
            first: "Bob",
            sir: "Bobbington"
          }
        }
      }];
      edges = [{
        source: nodes[0],
        target: nodes[1],
        _data: {
          _id: 12,
          _rev: 12,
          _key: 12,
          _from: 1,
          _to: 2,
          label: "oldLabel",
          nested: {
            from: "1",
            to: "2"
          }
        }
        
      }];
      adapter = mocks.adapter;
      layouter = mocks.layouter;
      addSpies();

      var expandConfig = {
          edges: edges,
          nodes: nodes,
          startCallback: function() {},
          adapter: adapter,
          reshapeNodes: function() {}
        },
      
        dragConfig = {
          layouter: layouter
        },
        
        nodeEditorConfig = {
          nodes: nodes,
          adapter: adapter
        },
      
        edgeEditorConfig = {
          edges: edges,
          adapter: adapter
        },
      
        completeConfig = {
          expand: expandConfig,
          drag: dragConfig,
          nodeEditor: nodeEditorConfig,
          edgeEditor: edgeEditorConfig
        };
      
      svg = document.createElement("svg");
      svg.id = "svg";
      document.body.appendChild(svg);
      nodeShaper = new NodeShaper(d3.select("svg"));
      edgeShaper = new EdgeShaper(d3.select("svg"));
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_event_list";
      $list = $(list);
      mousePointerbox = document.createElement("div");
      mousePointerbox.id = "mousepointer";
      mousePointerbox.className = "mousepointer";
      
      document.body.appendChild(mousePointerbox);
      
      nodeShaper.drawNodes(nodes);
      edgeShaper.drawEdges(edges);
      
      dispatcherUI = new EventDispatcherControls(
        list, mousePointerbox, nodeShaper, edgeShaper, completeConfig
      );
      spyOn(nodeShaper, "changeTo").andCallThrough();
      spyOn(edgeShaper, "changeTo").andCallThrough();
      
      
      this.addMatchers({
        toBeTag: function(name) {
          var item = this.actual;
          this.message = function() {
            return "Expected " + item.tagName.toLowerCase() + " to be " + name; 
          };
          return item.tagName.toLowerCase() === name;
        },
        
        toBeOfClass: function(name) {
          var item = this.actual;
          this.message = function() {
            return "Expected " + item.className + " to be " + name; 
          };
          return item.className === name;
        },
        toConformToToolbox: function() {
          var box = this.actual;
          _.each(box.children, function(div) {
            expect(div).toBeTag("div");
            expect(div).toBeOfClass("btn btn-group");
            expect(div.children.length).toEqual(2);
            _.each(div.children, function(btn) {
              expect(btn).toBeTag("button");
              expect(btn).toBeOfClass("btn btn-icon");
              expect(btn.children.length).toEqual(1);
              expect(btn.firstChild).toBeTag("i");
              expect(btn.firstChild.className).toMatch(/^icon-\S+ icon-white$/);
            });
          });          
          return true;
        }
      });
      
    });

    afterEach(function () {
      expect(list).toConformToToolbox();
      document.body.removeChild(list);
      document.body.removeChild(svg);
      document.body.removeChild(mousePointerbox);
    });

    it('should throw errors if not setup correctly', function() {
      expect(function() {
        var e = new EventDispatcherControls();
      }).toThrow("A list element has to be given.");
      expect(function() {
        var e = new EventDispatcherControls(list);
      }).toThrow("The cursor decoration box has to be given.");
      expect(function() {
        var e = new EventDispatcherControls(list, mousePointerbox);
      }).toThrow("The NodeShaper has to be given.");
      expect(function() {
        var e = new EventDispatcherControls(list, mousePointerbox, nodeShaper);
      }).toThrow("The EdgeShaper has to be given.");
    });
    
    it('should be able to add a new node control to the list', function() {
      runs(function() {
        dispatcherUI.addControlNewNode();
      
        expect($("#control_event_new_node", $list).length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_event_new_node");
      
        expect(nodeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true
          }
        });
        
        expect(edgeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true
          }
        });
        
        expect(mousePointerbox.className).toEqual("mousepointer icon-plus-sign");
      
        helper.simulateMouseEvent("click", "svg");
      
        expect($("#control_event_new_node_modal").length).toEqual(1);
      
        //$("#control_event_node_edit_name_value").val("Bob");
        
        expect($("#control_event_new_node_submit").text()).toEqual("Create");
        
        helper.simulateMouseEvent("click", "control_event_new_node_submit");
        
        expect(adapter.createNode).toHaveBeenCalledWith(
          {},
          jasmine.any(Function)
        );
        /*
        expect(adapter.createNode).toHaveBeenCalledWith(
          {
            name: "Bob"
          },
          jasmine.any(Function)
        );
        */
      });
      
        
      waitsFor(function() {
        return $("#control_event_node_edit_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
    });
    
    it('should be able to add a drag control to the list', function() {
      runs(function() {
        dispatcherUI.addControlDrag();
      
        expect($("#control_event_list #control_event_drag").length).toEqual(1);
        
        helper.simulateMouseEvent("click", "control_event_drag");
      
        expect(nodeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true,
            drag: jasmine.any(Function)
          }
        });
        
        expect(edgeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true
          }
        });
        
        expect(mousePointerbox.className).toEqual("mousepointer icon-move");
        
        helper.simulateDragEvent("1");
        
        expect(layouter.drag).toHaveBeenCalled();
      });

    });
    
    describe('the edit control', function() {
      
      var id = "control_event_edit",
        nodeId = "control_event_node_edit",
        edgeId = "control_event_edge_edit";
      
      beforeEach(function() {
        dispatcherUI.addControlEdit();
      });
      
      it('should be added', function() {
        expect($("#control_event_list #"+ id).length).toEqual(1);
      });
      
      it('should activate actions on nodes and edges if clicked', function() {
        helper.simulateMouseEvent("click", id);
      
        expect(nodeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true,
            click: jasmine.any(Function)
          }
        });
        
        expect(edgeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true,
            click: jasmine.any(Function)
          }
        });
      
        expect(mousePointerbox.className).toEqual("mousepointer icon-pencil");
      });
      
      it('should be possible to edit nodes', function() {
        
        runs(function() {
          helper.simulateMouseEvent("click", id);
          
          helper.simulateMouseEvent("click", "1");
        
          expect($("#" + nodeId + "_modal").length).toEqual(1);
        
          $("#" + nodeId + "_name_value").val("Bob");
        
          helper.simulateMouseEvent("click", nodeId + "_submit");
          expect(adapter.patchNode).toHaveBeenCalledWith(
            nodes[0],
            {
              name: "Bob"
            },
            jasmine.any(Function)
          );
        });
      
        waitsFor(function() {
          return $("#" + nodeId + "_modal").length === 0;
        }, 2000, "The modal dialog should disappear.");
        
      });
      
      it('should display nested attributes', function() {
        helper.simulateMouseEvent("click", id);
        
        helper.simulateMouseEvent("click", "2");
      
        expect($("#" + nodeId + "_modal").length).toEqual(1);
        
        expect($("#" + nodeId + "_name_value").val()).toEqual(JSON.stringify(nodes[1]._data.name));

      });
      
      it('should be possible to edit edges', function() {
        
        runs(function() {
          var nested = JSON.stringify(edges[0]._data.nested);
          helper.simulateMouseEvent("click", id);
          helper.simulateMouseEvent("click", "1-2");
      
          expect($("#" + edgeId + "_modal").length).toEqual(1);
      
          expect($("#" + edgeId + "_nested_value").val()).toEqual(nested);
      
          $("#" + edgeId + "_label_value").val("newLabel");
          
          helper.simulateMouseEvent("click", edgeId + "_submit");

          expect(adapter.patchEdge).toHaveBeenCalledWith(
            edges[0],
            {
              label: "newLabel",
              nested: nested
            },
            jasmine.any(Function)
          );
        });
        
        waitsFor(function() {
          return $("#" + edgeId + "_modal").length === 0;
        }, 2000, "The modal dialog should disappear.");
        
      });
      
    });
    
    it('should be able to add an expand control to the list', function() {
      runs(function() {
        dispatcherUI.addControlExpand();
      
        expect($("#control_event_list #control_event_expand").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_event_expand");
      
        expect(nodeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true,
            click: jasmine.any(Function)
          }
        });
      
        expect(edgeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true
          }
        });

        expect(mousePointerbox.className).toEqual("mousepointer icon-plus");

        helper.simulateMouseEvent("click", "1");
        
        expect(adapter.loadNode).toHaveBeenCalledWith(nodes[0]._id, jasmine.any(Function));
        
      });      
    });
    
    it('should be able to add a delete control to the list', function() {
      runs(function() {
        dispatcherUI.addControlDelete();
      
        expect($("#control_event_list #control_event_delete").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_event_delete");
      
        expect(edgeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true,
            click: jasmine.any(Function)
          }
        });
      
        expect(edgeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true,
            click: jasmine.any(Function)
          }
        });
      
        expect(mousePointerbox.className).toEqual("mousepointer icon-trash");
      
        helper.simulateMouseEvent("click", "1");
        
        expect(adapter.deleteNode).toHaveBeenCalledWith(
          nodes[0],
          jasmine.any(Function)
        );
        
        helper.simulateMouseEvent("click", "1-2");
        
        expect(adapter.deleteEdge).toHaveBeenCalledWith(
          edges[0],
          jasmine.any(Function)
        );
        
      });      
    });
    
    describe('the connect control', function() {
      
      it('should be added to the list', function() {
        runs(function() {
          dispatcherUI.addControlConnect();
      
          expect($("#control_event_list #control_event_connect").length).toEqual(1);
      
          helper.simulateMouseEvent("click", "control_event_connect");
      
          expect(nodeShaper.changeTo).toHaveBeenCalledWith({
            actions: {
              reset: true,
              mousedown: jasmine.any(Function),
              mouseup: jasmine.any(Function)
            }
          });
        
          expect(edgeShaper.changeTo).toHaveBeenCalledWith({
            actions: {
              reset: true
            }
          });
        
          expect(mousePointerbox.className).toEqual("mousepointer icon-resize-horizontal");
        
          helper.simulateMouseEvent("mousedown", "2");
        
          helper.simulateMouseEvent("mouseup", "1");
        
          expect(adapter.createEdge).toHaveBeenCalledWith(
            {source: nodes[1], target: nodes[0]},
            jasmine.any(Function)
          );
        
        });      
      });
      
      it('should draw a line from startNode following the cursor', function() {
        var line,
        cursorX,
        cursorY;
        
        spyOn(edgeShaper, "addAnEdgeFollowingTheCursor");
        
        dispatcherUI.addControlConnect();
        helper.simulateMouseEvent("click", "control_event_connect");
        helper.simulateMouseEvent("mousedown", "2");
        
        expect(edgeShaper.addAnEdgeFollowingTheCursor).toHaveBeenCalledWith(
          -$("svg").offset().left, -$("svg").offset().top
        );
      });
      
      it('the cursor-line should follow the cursor on mousemove over svg', function() {        
        dispatcherUI.addControlConnect();
        var x = 40,
          y= 50,
          line;
        
        helper.simulateMouseEvent("click", "control_event_connect");
        helper.simulateMouseEvent("mousedown", "2");
        
        helper.simulateMouseMoveEvent("svg", x, y);
        
        line = $("#connectionLine");
        //The Helper event triggers at (0,0) no matter where the node is.
        expect(line.attr("x1")).toEqual(String(- $("svg").offset().left));
        expect(line.attr("y1")).toEqual(String(- $("svg").offset().top));
        expect(line.attr("x2")).toEqual(String(x - $("svg").offset().left));
        expect(line.attr("y2")).toEqual(String(y - $("svg").offset().top));
      });
      
      it('the cursor-line should disappear on mouseup on svg', function() {
        spyOn(edgeShaper, "removeCursorFollowingEdge");
        dispatcherUI.addControlConnect();
        helper.simulateMouseEvent("click", "control_event_connect");
        helper.simulateMouseEvent("mousedown", "2");
        helper.simulateMouseEvent("mouseup", "1-2");
        expect(edgeShaper.removeCursorFollowingEdge).toHaveBeenCalled();
      });
      
      it('the cursor-line should disappear on mouseup on svg', function() {
        spyOn(edgeShaper, "removeCursorFollowingEdge");  
        dispatcherUI.addControlConnect();
        helper.simulateMouseEvent("click", "control_event_connect");
        helper.simulateMouseEvent("mousedown", "2");
        helper.simulateMouseEvent("mouseup", "1");
        expect(edgeShaper.removeCursorFollowingEdge).toHaveBeenCalled();
      });
      
    });
          
    it('should be able to add all controls to the list', function () {
      dispatcherUI.addAll();
      
      expect($("#control_event_list #control_event_drag").length).toEqual(1);
      expect($("#control_event_list #control_event_edit").length).toEqual(1);
      expect($("#control_event_list #control_event_expand").length).toEqual(1);
      expect($("#control_event_list #control_event_delete").length).toEqual(1);
      expect($("#control_event_list #control_event_connect").length).toEqual(1);
      expect($("#control_event_list #control_event_new_node").length).toEqual(1);
    });
    
    describe('checking the raw functions', function() {
      
      it('should offer drag rebinds', function() {
        expect(dispatcherUI.dragRebinds()).toEqual({
          nodes: {
            drag: jasmine.any(Function)
          }
        });
      });
      
      it('should offer new node rebinds', function() {
        expect(dispatcherUI.newNodeRebinds()).toEqual({
          svg: {
            click: jasmine.any(Function)
          }
        });
      });
      
      it('should offer connect nodes rebinds', function() {
        expect(dispatcherUI.connectNodesRebinds()).toEqual({
          nodes: {
            mouseup: jasmine.any(Function),
            mousedown: jasmine.any(Function)
          },
          svg: {
            mouseup: jasmine.any(Function)
          }
        });
      });
      
      it('should offer edit rebinds', function() {
        expect(dispatcherUI.editRebinds()).toEqual({
          nodes: {
            click: jasmine.any(Function)
          },
          edges: {
            click: jasmine.any(Function)
          }
        });
      });
      
      it('should offer expand rebinds', function() {
        expect(dispatcherUI.expandRebinds()).toEqual({
          nodes: {
            click: jasmine.any(Function)
          }
        });
      });
      
    });
  });

}());