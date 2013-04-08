/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect*/
/*global runs, waitsFor, spyOn */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global helper*/
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
    var svg, dispatcher, dispatcherUI, list,
    nodeShaper, edgeShaper, layouter,
    nodes, edges, adapter;

    beforeEach(function () {
      nodes = [{
        _id: 1
      },{
        _id: 2
      }];
      edges = [{
        source: nodes[0],
        target: nodes[1]
      }];
      adapter = {
      
      };
      var expandConfig = {
          edges: edges,
          nodes: nodes,
          startCallback: function() {},
          loadNode: function() {},
          reshapeNode: function() {}
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
          nodeEditor: nodeEditorConfig,
          edgeEditor: edgeEditorConfig
        };
      
      layouter = {
        drag: function() {return 0;}
      };
      svg = document.createElement("svg");
      document.body.appendChild(svg);
      nodeShaper = new NodeShaper(d3.select("svg"));
      edgeShaper = new EdgeShaper(d3.select("svg"));
      list = document.createElement("ul");
      document.body.appendChild(list);
      list.id = "control_list";
      nodeShaper.drawNodes(nodes);
      edgeShaper.drawEdges(edges);
      
      dispatcherUI = new EventDispatcherControls(list, nodeShaper, edgeShaper, completeConfig);
      spyOn(nodeShaper, "changeTo").andCallThrough();
      spyOn(edgeShaper, "changeTo").andCallThrough();
      
    });

    afterEach(function () {
      document.body.removeChild(list);
    });

    it('should throw errors if not setup correctly', function() {
      expect(function() {
        var e = new EventDispatcherControls();
      }).toThrow("A list element has to be given.");
      expect(function() {
        var e = new EventDispatcherControls(list);
      }).toThrow("The NodeShaper has to be given.");
      expect(function() {
        var e = new EventDispatcherControls(list, nodeShaper);
      }).toThrow("The EdgeShaper has to be given.");
    });
    
    it('should be able to add a drag control to the list', function() {
      runs(function() {
        dispatcherUI.addControlDrag();
      
        expect($("#control_list #control_drag").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_drag");
      
        expect(nodeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true,
            drag: layouter.drag
          }
        });
        
        expect(edgeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true
          }
        });
      });      
    });
    
    it('should be able to add an edit control to the list', function() {
      runs(function() {
        dispatcherUI.addControlEdit();
      
        expect($("#control_list #control_edit").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_edit");
      
        helper.simulateMouseEvent("click", "1");
      
        expect($("#control_node_edit_modal").length).toEqual(1);
      
        // Todo check node edit dialog
        helper.simulateMouseEvent("click", "control_node_edit_submit");
        // Todo check adapter call
        
      });
      
      waitsFor(function() {
        return $("#control_node_edit_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
      
      runs(function() {
        helper.simulateMouseEvent("click", "1-2");
      
        expect($("#control_edge_edit_modal").length).toEqual(1);
      
        // Todo check node edit dialog
        helper.simulateMouseEvent("click", "control_edge_edit_submit");
        // Todo check adapter call
      });
      
      waitsFor(function() {
        return $("#control_edge_edit_modal").length === 0;
      }, 2000, "The modal dialog should disappear.");
           
    });
    
    it('should be able to add an expand control to the list', function() {
      runs(function() {
        dispatcherUI.addControlExpand();
      
        expect($("#control_list #control_expand").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_expand");
      
        expect(edgeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true
          }
        });

        helper.simulateMouseEvent("click", "1");
        
        //Todo Check for expand event
        
      });      
    });
    
    it('should be able to add a delete control to the list', function() {
      runs(function() {
        dispatcherUI.addControlDelete();
      
        expect($("#control_list #control_delete").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_delete");
      
        helper.simulateMouseEvent("click", "1");
        
        // Todo check for del event
        
        helper.simulateMouseEvent("click", "1-2");
        
        // Todo check for del event
      });      
    });
    
    it('should be able to add a connect control to the list', function() {
      runs(function() {
        dispatcherUI.addControlConnect();
      
        expect($("#control_list #control_connect").length).toEqual(1);
      
        helper.simulateMouseEvent("click", "control_connect");
      
        expect(nodeShaper.changeTo).toHaveBeenCalledWith({
          actions: {
            reset: true
          }
        });
        
        helper.simulateMouseEvent("mousedown", "2");
        
        helper.simulateMouseEvent("mouseup", "1");
        
        // Todo Adapter Check
        
      });      
    });
            
    it('should be able to add all controls to the list', function () {
      dispatcherUI.addAll();
      
      expect($("#control_list #control_drag").length).toEqual(1);
      expect($("#control_list #control_edit").length).toEqual(1);
      expect($("#control_list #control_expand").length).toEqual(1);
      expect($("#control_list #control_delete").length).toEqual(1);
      expect($("#control_list #control_connect").length).toEqual(1);
    });
  });

}());