/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine */
/*global runs, spyOn, waitsFor */
/*global window, document, $, d3, _*/
/*global helper, mocks*/
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


(function () {
  "use strict";

  describe('Event Dispatcher', function () {

    var dispatcher,
      nodes,
      edges,
      adapter = mocks.adapter,
      layouter = mocks.layouter,
      nodeShaper,
      edgeShaper,
      completeConfig,
      expandConfig,
      dragConfig,
      nodeEditorConfig,
      edgeEditorConfig,
      svg,
      defaultPosition,
    
      bindSpies = function() {
        spyOn(layouter, "drag");
        spyOn(adapter, "createNode");
        spyOn(adapter, "patchNode");
        spyOn(adapter, "deleteNode");
        spyOn(adapter, "createEdge");
        spyOn(adapter, "patchEdge");
        spyOn(adapter, "deleteEdge");
        spyOn(adapter, "loadNode");
        spyOn(adapter, "expandCommunity");
        spyOn(adapter, "explore");
      };
    
    beforeEach(function() {
      svg = document.createElement("svg");
      svg.id = "svg";
      document.body.appendChild(svg);
      
      bindSpies();
      
      nodes = [];
      edges = [];
      
      defaultPosition = {
        x: 1,
        y: 1,
        z: 1
      };
      
      expandConfig = {
        startCallback: function() {},
        adapter: adapter,
        reshapeNodes: function() {}
      };
      
      dragConfig = {
        layouter: layouter
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
        drag: dragConfig,
        nodeEditor: nodeEditorConfig,
        edgeEditor: edgeEditorConfig
      };
      
      nodeShaper = new NodeShaper(d3.select("svg"));
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
        expect(_.keys(t.events).length).toEqual(5);
        expect(t.events.STARTCREATEEDGE).toBeDefined();
        expect(t.events.FINISHCREATEEDGE).toBeDefined();
        expect(t.events.CANCELCREATEEDGE).toBeDefined();
        expect(t.events.PATCHEDGE).toBeDefined();
        expect(t.events.DELETEEDGE).toBeDefined();
      });
      
      it('should offer the drag event if config is correct', function() {
        var config = {drag: dragConfig},
          t = new EventDispatcher(nodeShaper, edgeShaper, config);
        
        expect(t.events).toBeDefined();
        expect(_.keys(t.events).length).toEqual(1);
        expect(t.events.DRAG).toBeDefined();
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
          helper.simulateMouseEvent("click", "svg");
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
          nodes = helper.createSimpleNodes([1, 2]);
          var callback = function() {
            called++;
          };
          called = 0;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "click", callback);
          helper.simulateMouseEvent("click", "1");
          helper.simulateMouseEvent("click", "2");
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
          nodes = helper.createSimpleNodes([1, 2, 3]);
          var n1 = nodes[0],
            n2 = nodes[1],
            n3 = nodes[2],
            edges = [
              {_id: "1-2", source: n1, target: n2},
              {_id: "2-3", source: n2, target: n3}
            ],
            callback = function() {
              called++;
            };
          called = 0;
          edgeShaper.drawEdges(edges);
          dispatcher.bind("edges", "click", callback);
          helper.simulateMouseEvent("click", "1-2");
          helper.simulateMouseEvent("click", "2-3");
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
          var nodes = helper.createSimpleNodes([1]),
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "click", callback);
          helper.simulateMouseEvent("click", "1");
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
          var nodes = helper.createSimpleNodes([1]),
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "dblclick", callback);
          helper.simulateMouseEvent("dblclick", "1");
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
          var nodes = helper.createSimpleNodes([1]),
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "mousedown", callback);
          helper.simulateMouseEvent("mousedown", "1");
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
          var nodes = helper.createSimpleNodes([1]),
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "mouseup", callback);
          helper.simulateMouseEvent("mouseup", "1");
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
          var nodes = helper.createSimpleNodes([1]),
          callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "mousemove", callback);
          helper.simulateMouseEvent("mousemove", "1");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The mousemove event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
      });
      
      it('should be able to bind to drag', function() {
        var called;
        
        runs(function() {
          nodes = helper.createSimpleNodes([1]);
          var callback = function() {
            called = true;
          };
          called = false;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "drag", callback);
          helper.simulateDragEvent("1");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The drag event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(true).toBeTruthy();
        });
      });
      
    });

    describe('checking different events', function() {
      
      it('should be able to bind the expand event', function() {
        runs(function() {
          nodes = helper.createSimpleNodes([1]);
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "click", dispatcher.events.EXPAND);
          helper.simulateMouseEvent("click", "1");
        });
        
        waitsFor(function() {
          return adapter.explore.wasCalled;
        }, 1000, "The explore function should have been called.");
        
        runs(function() {
          // Just do be displayed in the list
          expect(true).toBeTruthy();
        });
      });
      
      it('should be able to bind the drag event', function() {
        runs(function() {
          nodes = helper.createSimpleNodes([1]);
          nodeShaper.drawNodes(nodes);
          
          dispatcher.bind("nodes", "drag", dispatcher.events.DRAG);
          helper.simulateDragEvent("1");
        });
        
        waitsFor(function() {
          return layouter.drag.wasCalled;
        }, 1000, "The drag event should have been triggered.");
        
        runs(function() {
          expect(true).toBeTruthy();
        });
        
      });
      
      it('should be able to bind the create node event', function() {
        runs(function() {
          dispatcher.bind($("svg"), "click", dispatcher.events.CREATENODE(
            function() {
              // Never reached as the spy stops propagation
              return 0;
            }
          ));
          
          helper.simulateMouseEvent("click", "svg");
        });
        
        waitsFor(function() {
          return adapter.createNode.wasCalled;
        }, 1000, "The event should have been triggered.");
        
        runs(function() {
          expect(true).toBeTruthy();
        });
      });
      
      it('should be able to bind the patch node event', function() {
        
        runs(function() {
          nodes = helper.createSimpleNodes([1]);          
          dispatcher.bind($("svg"), "click", dispatcher.events.PATCHNODE(
            nodes[0],
            function() {
              return {
                name: "Alice"
              };
            },
            function() {
              // Never reached as the spy stops propagation
              return 0;
            }
          ));
          
          helper.simulateMouseEvent("click", "svg");
        });
        
        waitsFor(function() {
          return adapter.patchNode.wasCalled;
        }, 1000, "The event should have been triggered.");
        
        runs(function() {
          expect(adapter.patchNode).toHaveBeenCalledWith(
            nodes[0],
            { name: "Alice" },
            jasmine.any(Function));
        });
      });
      
      it('should be able to bind the delete node event', function() {
        runs(function() {
          nodes = helper.createSimpleNodes([1]);
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "click", dispatcher.events.DELETENODE(
            function(node) {
              // Never reached as the spy stops propagation
              return 0;
            }
          ));
          
          helper.simulateMouseEvent("click", "1");
        });
        
        waitsFor(function() {
          return adapter.deleteNode.wasCalled;
        }, 1000, "The event should have been triggered.");
        
        runs(function() {
          expect(adapter.deleteNode).toHaveBeenCalledWith(
            nodes[0],
            jasmine.any(Function));
        });
      });
      
      it('should be able to bind the events to create an edge', function() {
        nodes = helper.createSimpleNodes([1, 2, 3]);
        edges = [{_id: "1-3", source: nodes[0], target: nodes[2]}];
        nodeShaper.drawNodes(nodes);
        edgeShaper.drawEdges(edges);
        
        var started = 0,
          canceled = 0;
        
        runs(function() {
          dispatcher.bind("nodes", "mousedown", dispatcher.events.STARTCREATEEDGE(
            function() {
              started++;
            }
          ));
          dispatcher.bind("nodes", "mouseup", dispatcher.events.FINISHCREATEEDGE(
            function() {
              // Never reached as the spy stops propagation
              return 0;
            }
          ));
          dispatcher.bind($("svg"), "mouseup", dispatcher.events.CANCELCREATEEDGE(
            function() {
              return canceled++;
            }
          ));
          helper.simulateMouseEvent("mousedown", "1");
          helper.simulateMouseEvent("mouseup", "svg");
          
          helper.simulateMouseEvent("mousedown", "1");
          helper.simulateMouseEvent("mouseup", "1-3");
          
          helper.simulateMouseEvent("mousedown", "1");
          helper.simulateMouseEvent("mouseup", "1");
          
          helper.simulateMouseEvent("mousedown", "1");
          helper.simulateMouseEvent("mouseup", "2");
          
        });
        
        waitsFor(function() {
          return adapter.createEdge.wasCalled;
        }, 1000, "The event should have been triggered.");
        
        runs(function() {
          expect(started).toEqual(4);
          expect(canceled).toEqual(3);
          expect(adapter.createEdge.calls.length).toEqual(1);
          expect(adapter.createEdge).toHaveBeenCalledWith({
            source: nodes[0],
            target: nodes[1]
          }, jasmine.any(Function));
        });
      });
      
      it('should be able to bind the patch edge event', function() {
        
        runs(function() {
          nodes = helper.createSimpleNodes([1, 2]);
          edges = [{source: nodes[0], target: nodes[1]}];       
          dispatcher.bind($("svg"), "click", dispatcher.events.PATCHEDGE(
            edges[0],
            function() {
              return {
                name: "Alice"
              };
            },
            function() {
              // Never reached as the spy stops propagation
              return 0;
            }
          ));
          
          helper.simulateMouseEvent("click", "svg");
        });
        
        waitsFor(function() {
          return adapter.patchEdge.wasCalled;
        }, 1000, "The event should have been triggered.");
        
        runs(function() {
          expect(adapter.patchEdge).toHaveBeenCalledWith(
            edges[0],
            { name: "Alice" },
            jasmine.any(Function));
        });
      });
      
      it('should be able to bind the delete edge event', function() {
        runs(function() {
          nodes = helper.createSimpleNodes([1, 2]);
          edges = [{_id: "1-2", source: nodes[0], target: nodes[1]}]; 
          edgeShaper.drawEdges(edges);
          dispatcher.bind("edges", "click", dispatcher.events.DELETEEDGE(
            function(edge) {
              // Never reached as the spy stops propagation
              return 0;
            }
          ));
          
          helper.simulateMouseEvent("click", "1-2");
        });
        
        waitsFor(function() {
          return adapter.deleteEdge.wasCalled;
        }, 1000, "The event should have been triggered.");
        
        runs(function() {
          expect(adapter.deleteEdge).toHaveBeenCalledWith(
            edges[0],
            jasmine.any(Function));
        });
      });      
      
    });
    
    describe('checking overwriting of events', function() {
      
      it('should be able to overwrite the event of any DOM-element', function() {
        var falseCalled = false,
        called = false,
        falseCallback = function() { 
          falseCalled = true;
        },
        callback = function() {
          called = true;
        };
        
        runs(function() {
          var target = $("svg");
          dispatcher.bind(target, "click", falseCallback);
          dispatcher.bind(target, "click", callback);
          helper.simulateMouseEvent("click", "svg");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The click event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(falseCalled).toBeFalsy();
          expect(called).toBeTruthy();
        });
      });
      
      it('should be able to overwrite the event of all nodes', function() {
        var nodes = helper.createSimpleNodes([1]),
        falseCalled = false,
        called = false,
        falseCallback = function() {
          falseCalled = true;
        },
        callback = function() {
          called = true;
        };
        
        runs(function() {
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "click", falseCallback);
          dispatcher.bind("nodes", "click", callback);
          helper.simulateMouseEvent("click", "1");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The click event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(falseCalled).toBeFalsy();
          expect(called).toBeTruthy();
        });
      });
      
      it('should be able to overwrite the event of all edges', function() {
        nodes = helper.createSimpleNodes([1, 2]);
        var n1 = nodes[0],
          n2 = nodes[1],
          edges = [
            {_id: "1-2", source: n1, target: n2}
          ],
          falseCalled = false,
          called = false,
          falseCallback = function() { 
            falseCalled = true;
          },
          callback = function() {
            called = true;
          };
        
        runs(function() {
          edgeShaper.drawEdges(edges);
          dispatcher.bind("edges", "click", falseCallback);
          dispatcher.bind("edges", "click", callback);
          helper.simulateMouseEvent("click", "1-2");
        });
        
        waitsFor(function() {
          return called;
        }, 1000, "The click event should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(falseCalled).toBeFalsy();
          expect(called).toBeTruthy();
        });
      });
      
    }); 
    
    describe('checking binding to SVG', function() {
      
      it('should be able to permanently bind many functions to events', function() {
        var functions = {
          a: function() {},
          b: function() {},
          c: function() {},
          d: function() {}
        };
        spyOn(functions, "a");
        spyOn(functions, "b");
        spyOn(functions, "c");
        spyOn(functions, "d");
        
        dispatcher.fixSVG("click", functions.a);
        dispatcher.fixSVG("click", functions.b);
        dispatcher.fixSVG("click", functions.c);
        dispatcher.fixSVG("click", functions.d);
        
        helper.simulateMouseEvent("click", "svg");
        
        expect(functions.a).wasCalled();
        expect(functions.b).wasCalled();
        expect(functions.c).wasCalled();
        expect(functions.d).wasCalled();
      });
      
      it('should be able to bind temporary functions to events', function() {
        var functions = {
          a: function() {}
        };
        spyOn(functions, "a");
        
        dispatcher.bind("svg", "click", functions.a);
        
        helper.simulateMouseEvent("click", "svg");
        
        expect(functions.a).wasCalled();
      });
      
      it('should be able to overwrite temporary functions on events', function() {
        var functions = {
          a: function() {},
          b: function() {}
        };
        spyOn(functions, "a");
        spyOn(functions, "b");
        
        dispatcher.bind("svg", "click", functions.a);
        
        dispatcher.bind("svg", "click", functions.b);
        
        helper.simulateMouseEvent("click", "svg");
        
        expect(functions.b).wasCalled();
        expect(functions.a).wasNotCalled();
      });
      
      it('should not overwrite permanent functions', function() {
        var functions = {
          a: function() {},
          b: function() {}
        };
        spyOn(functions, "a");
        spyOn(functions, "b");
        
        dispatcher.fixSVG("click", functions.a);
        
        dispatcher.bind("svg", "click", functions.b);
        
        helper.simulateMouseEvent("click", "svg");
        
        expect(functions.b).wasCalled();
        expect(functions.a).wasCalled();
      });
      
      it('binding a permanent function should not effect temporary function', function() {
        var functions = {
          a: function() {},
          b: function() {}
        };
        spyOn(functions, "a");
        spyOn(functions, "b");
        
        dispatcher.bind("svg", "click", functions.b);
        dispatcher.fixSVG("click", functions.a);
        
        helper.simulateMouseEvent("click", "svg");
        
        expect(functions.b).wasCalled();
        expect(functions.a).wasCalled();
      });
      
      it('should be able to bind only the given events and'
      + ' unbind other temporary events', function() {
        var functions = {
          a: function() {},
          b: function() {},
          c: function() {},
          d: function() {}
        };
        spyOn(functions, "a");
        spyOn(functions, "b");
        spyOn(functions, "c");
        spyOn(functions, "d");
        
        dispatcher.bind("svg", "mouseup", functions.a);
        dispatcher.bind("svg", "mousedown", functions.b);
        dispatcher.bind("svg", "click", functions.c);
        dispatcher.rebind("svg", {click: functions.d});
        
        helper.simulateMouseEvent("click", "svg");
        helper.simulateMouseEvent("mouseup", "svg");
        helper.simulateMouseEvent("mousedown", "svg");
        
        expect(functions.a).wasNotCalled();
        expect(functions.b).wasNotCalled();
        expect(functions.c).wasNotCalled();
        expect(functions.d).wasCalled();
      });
      
      it('should not remove permanent events on rebind', function() {
        var functions = {
          a: function() {},
          b: function() {}
        };
        spyOn(functions, "a");
        spyOn(functions, "b");
        
        dispatcher.fixSVG("mousemove", functions.a);
        dispatcher.rebind("svg", {click: functions.b});
        
        helper.simulateMouseEvent("mousemove", "svg");
        helper.simulateMouseEvent("click", "svg");
        
        expect(functions.a).wasCalled();
        expect(functions.b).wasCalled();
      });
      
    });
    
    describe('checking rebinding of events', function() {
      
      it('should be able to bind only the given events and unbind all other for nodes', function() {
        var falseCalls, called;
        
        runs(function() {
          var nodes = helper.createSimpleNodes([1, 2]),
            callback = function() {
              called++;
            },
            falseCallback = function() {
              falseCalls++;
            };
          falseCalls = 0;
          called = 0;
          nodeShaper.drawNodes(nodes);
          dispatcher.bind("nodes", "click", falseCallback);
          dispatcher.bind("nodes", "mousedown", falseCallback);
          dispatcher.bind("nodes", "mouseup", falseCallback);
          dispatcher.rebind("nodes", {
            click: callback
          });
          helper.simulateMouseEvent("mousedown", "2");
          helper.simulateMouseEvent("mouseup", "2");
          helper.simulateMouseEvent("click", "1");
          helper.simulateMouseEvent("click", "2");
        });
        
        waitsFor(function() {
          return called === 2;
        }, 1000, "The two click events should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(falseCalls).toEqual(0);
        });
        
      });
      
      it('should be able to bind only the given events and unbind all other for edges', function() {
        var falseCalls, called;
        
        runs(function() {
          nodes = helper.createSimpleNodes([1, 2, 3]);
          var n1 = nodes[0],
            n2 = nodes[1],
            n3 = nodes[2],
            edges = [
              {_id: "1-2", source: n1, target: n2},
              {_id: "2-3", source: n2, target: n3}
            ],
            callback = function() {
              called++;
            },
            falseCallback = function() {
              falseCalls++;
            };
          falseCalls = 0;
          called = 0;
          edgeShaper.drawEdges(edges);
          dispatcher.bind("edges", "click", falseCallback);
          dispatcher.bind("edges", "mousedown", falseCallback);
          dispatcher.bind("edges", "mouseup", falseCallback);
          dispatcher.rebind("edges", {
            click: callback
          });
          helper.simulateMouseEvent("mousedown", "1-2");
          helper.simulateMouseEvent("mouseup", "1-2");
          helper.simulateMouseEvent("click", "1-2");
          helper.simulateMouseEvent("click", "2-3");
        });
        
        waitsFor(function() {
          return called === 2;
        }, 1000, "The two click events should have been triggered.");
        
        runs(function() {
          // Just display that everything had worked
          expect(falseCalls).toEqual(0);
        });
        
      });
      
      
    });
  });

}());
