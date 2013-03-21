/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global runs, spyOn, waitsFor */
/*global window, document, $, d3, _*/
/*global EventDispatcher, NodeShaper, EdgeShaper*/

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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


(function () {
  "use strict";

  describe('Event Dispatcher', function () {

    var dispatcher,
    nodes,
    edges,
    adapter,
    nodeShaper,
    edgeShaper,
    completeConfig,
    expandConfig,
    nodeEditorConfig,
    edgeEditorConfig,
    svg,
    simulateMouseEvent = function (type, objectId) {
      var evt = document.createEvent("MouseEvents"),
      testee = document.getElementById(objectId); 
      evt.initMouseEvent(type, true, true, window,
        0, 0, 0, 0, 0, false, false, false, false, 0, null);
      testee.dispatchEvent(evt);
    };
    
    beforeEach(function() {
      svg = document.createElement("svg");
      
      document.body.appendChild(svg);
      adapter = null;
      
      nodes = [];
      edges = [];
      
      expandConfig = {
        edges: edges,
        nodes: nodes,
        startCallback: function() {},
        loadNode: function() {},
        reshapeNode: function() {}
      };
      
      nodeEditorConfig = {
        nodes: nodes,
        adapter: adapter
      };
      
      edgeEditorConfig = {
        edges: edges,
        adapter: adapter
      };
      
      completeConfig = {
        expand: expandConfig,
        nodeEditor: nodeEditorConfig,
        edgeEditor: edgeEditorConfig
      };
      
      
      
      nodeShaper = new NodeShaper(d3.select("svg"),
            {
              "shape": NodeShaper.shapes.CIRCLE
            }
          );
      edgeShaper = new EdgeShaper(d3.select("svg"));
      dispatcher = new EventDispatcher(nodeShaper, edgeShaper, completeConfig);
    });
    
    afterEach(function() {
      document.body.removeChild(svg);
    });
    
    describe('set up process', function() {
      it('should throw an error if nodeShaper is not given', function() {
        expect(
          function() {
            var t = new EventDispatcher();
          }
        ).toThrow("NodeShaper has to be given.");
      });
      
      it('should throw an error if edgeShaper is not given', function() {
        expect(
          function() {
            var t = new EventDispatcher(nodeShaper);
          }
        ).toThrow("EdgeShaper has to be given.");
      });
      
      it('should not offer events if config is not given', function() {
        var t = new EventDispatcher(nodeShaper, edgeShaper);
        expect(t.events).toBeDefined();
        expect(_.keys(t.events).length).toEqual(0);
        // Check immutability
        expect(function() {t.events.blub = 0;}).toThrow();
      });
      
      it('should offer the expand event if config is correct', function() {
        var config = {expand: expandConfig},
        t = new EventDispatcher(nodeShaper, edgeShaper, config);
        
        expect(t.events).toBeDefined();
        expect(_.keys(t.events).length).toEqual(1);
        expect(t.events.EXPAND).toBeDefined();
        // Check immutability
        expect(function() {t.events.EXPAND = 0;}).toThrow();
      });
      
      it('should offer node editing events if config is correct', function() {
        var config = {nodeEditor: nodeEditorConfig},
        t = new EventDispatcher(nodeShaper, edgeShaper, config);
        expect(t.events).toBeDefined();
        expect(_.keys(t.events).length).toEqual(3);
        expect(t.events.CREATENODE).toBeDefined();
        expect(t.events.PATCHNODE).toBeDefined();
        expect(t.events.DELETENODE).toBeDefined();
      });
      
      it('should offer edge editing events if config is correct', function() {
        var config = {edgeEditor: edgeEditorConfig},
        t = new EventDispatcher(nodeShaper, edgeShaper, config);
        
        expect(t.events).toBeDefined();
        expect(_.keys(t.events).length).toEqual(3);
        expect(t.events.CREATEEDGE).toBeDefined();
        expect(t.events.PATCHEDGE).toBeDefined();
        expect(t.events.DELETEEDGE).toBeDefined();
      });
    });
    
    describe('checking objects to bind events to', function() {
      
      
      it('should be able to bind to any DOM-element', function() {
        var called = false,
        callback = function() {
          called = true;
        };
        
        runs(function() {
          var target = $("svg");
          dispatcher.bind(target, "click", callback);
          target.click();
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The click event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
        
      });
      
      it('should be able to bind to all nodes', function() {
        var called;
        
        runs(function() {
          var nodes = [{_id: 1}, {_id:2}],
          callback = function() {
            called++;
          };
          called = 0;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "click", callback);
          $("#1").click();
          $("#2").click();
        });
        
        waitsFor(function() {
          return called === 2;
        }, 1000, "The two click events should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
        
      });
      
      it('should be able to bind to all edges', function() {
        var called;
        
        
        runs(function() {
          var n1 = {_id: 1},
          n2 = {_id: 2},
          n3 = {_id: 3},
          edges = [
            {source: n1, target: n2},
            {source: n2, target: n3}
          ],
          callback = function() {
            called++;
          };
          called = 0;
          edgeShaper.drawEdges(edges);
          dispatcher.bind("edges", "click", callback);
          $("#1-2").click();
          $("#2-3").click();
        });
        
        waitsFor(function() {
          return called === 2;
        }, 1000, "The two click events should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
        
      });
      
    });
    
    describe('checking triggers for events', function() {
      
      it('should be able to bind to click', function() {
        var called;
        
        runs(function() {
          var nodes = [{_id: 1}],
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "click", callback);
          $("#1").trigger("click");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The click event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
      });
      
      it('should be able to bind to double click', function() {
        var called;
        
        runs(function() {
          var nodes = [{_id: 1}],
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "dblclick", callback);
          simulateMouseEvent("dblclick", "1");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The double click event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
      });
      
      it('should be able to bind to mousedown', function() {
        var called;
        
        runs(function() {
          var nodes = [{_id: 1}],
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "mousedown", callback);
          simulateMouseEvent("mousedown", "1");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The mousedown event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
      });
      
      it('should be able to bind to mouseup', function() {
        var called;
        
        runs(function() {
          var nodes = [{_id: 1}],
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "mouseup", callback);
          simulateMouseEvent("mouseup", "1");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The mouseup event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
      });
      
      
      it('should be able to bind to mousemove', function() {
        var called;
        
        runs(function() {
          var nodes = [{_id: 1}],
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "mousemove", callback);
          simulateMouseEvent("mousemove", "1");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The mousemove event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
      });
      
    });

    /*
    describe('checking different events', function() {
      
      it('should be able to bind the expand event', function() {
        
      });
      
    });
    */
  });

}());
