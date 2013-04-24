/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global runs, spyOn, waitsFor */
/*global window, eb, loadFixtures, document, $ */
/*global EventLibrary*/

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

  describe('Event Library', function () {

    var eventLib,
      nodeShaperDummy = {},
      edgeShaperDummy = {};
    
    beforeEach(function() {
      eventLib = new EventLibrary();
      nodeShaperDummy.reshapeNodes = function() {};
      edgeShaperDummy.reshapeEdges = function() {};
      
      spyOn(nodeShaperDummy, "reshapeNodes");
      spyOn(edgeShaperDummy, "reshapeEdges");
    });

    describe('Expand', function() {
      
      var loaded,
      reshaped,
      nodes,
      edges,
      loadedNodes,
      started,
      loadNodeCallback = function(node) {
        loaded++;
        loadedNodes.push(node);
      },
      reshapeNodesCallback = function() {
        reshaped++;
      },
      startCallback = function() {
        started++;
      },
      config,
      testee;
      
      beforeEach(function() {
        loaded = 0;
        reshaped = 0;
        started = 0;
      
        nodes = [];
        edges = [];
        loadedNodes = [];
        config = {
          edges: edges,
          nodes: nodes,
          startCallback: startCallback,
          loadNode: loadNodeCallback,
          reshapeNodes: reshapeNodesCallback
        };
      });
      
      it('should expand a collapsed node', function() {
        var node = {
          _id: 0,
          _outboundCounter: 0,
          _inboundCounter: 0
        };
        nodes.push(node);
        testee = eventLib.Expand(config);
        testee(node);
        
        expect(node._expanded).toBeTruthy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(1);
        expect(loadedNodes.length).toEqual(1);
        expect(loadedNodes[0]).toEqual(node._id);
        
      });
      
      it('should collapse an expanded node', function() {
        var node = {
          id: 0,
          _expanded: true,
          _outboundCounter: 0,
          _inboundCounter: 0
        };
        nodes.push(node);
        testee = eventLib.Expand(config);
        testee(node);
        
        expect(node._expanded).toBeFalsy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(0);
      });
      
      it('should collapse a tree', function() {
        var root = {
          id: 0,
          _expanded: true,
          _outboundCounter: 2,
          _inboundCounter: 0
        },
        c1 = {
          id: 1,
          _outboundCounter: 0,
          _inboundCounter: 1
        },
        c2 = {
          id: 2,
          _outboundCounter: 0,
          _inboundCounter: 1
        };
        
        nodes.push(root);
        nodes.push(c1);
        nodes.push(c2);
        edges.push({source: root, target: c1});
        edges.push({source: root, target: c2});
        
        testee = eventLib.Expand(config);
        testee(root);
        
        expect(root._expanded).toBeFalsy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(0);
        expect(nodes.length).toEqual(1);
        expect(edges.length).toEqual(0);
      });
      
      it('should not remove referenced nodes on collapsing ', function() {
        var root = {
          _id: 0,
          _expanded: true,
          _outboundCounter: 2,
          _inboundCounter: 0
        },
        c1 = {
          _id: 1,
          _outboundCounter: 0,
          _inboundCounter: 2
        },
        c2 = {
          _id: 2,
          _outboundCounter: 1,
          _inboundCounter: 0
        },
        c3 = {
          _id: 3,
          _outboundCounter: 0,
          _inboundCounter: 1
        };
        
        nodes.push(root);
        nodes.push(c1);
        nodes.push(c2);
        nodes.push(c3);
        edges.push({source: root, target: c1});
        edges.push({source: root, target: c3});
        edges.push({source: c2, target: c1});
        
        testee = eventLib.Expand(config);
        testee(root);
        
        expect(root._expanded).toBeFalsy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(0);
        expect(nodes.length).toEqual(3);
        expect(edges.length).toEqual(1);
        
        expect(root._outboundCounter).toEqual(0);
        expect(c1._inboundCounter).toEqual(1);
        expect(c2._outboundCounter).toEqual(1);
      });
      
      
      
      
      describe('setup process', function() {
        
        var testConfig = {};
              
        it('should throw an error if edges are not given', function() {          
          expect(
            function() {
              eventLib.Expand(testConfig);
            }
          ).toThrow("Edges have to be defined");
        });
        
        it('should throw an error if nodes are not given', function() {
          testConfig.edges = [];
          expect(
            function() {
              eventLib.Expand(testConfig);
            }
          ).toThrow("Nodes have to be defined");
        });
        
        it('should throw an error if start callback is not given', function() {
          testConfig.edges = [];
          testConfig.nodes = [];
          expect(
            function() {
              eventLib.Expand(testConfig);
            }
          ).toThrow("A callback to the Start-method has to be defined");
        });
        
        it('should throw an error if load node callback is not given', function() {
          testConfig.edges = [];
          testConfig.nodes = [];
          testConfig.startCallback = function(){};
          expect(
            function() {
              eventLib.Expand(testConfig);
            }
          ).toThrow("A callback to load a node has to be defined");
        });
        
        it('should throw an error if reshape node callback is not given', function() {
          testConfig.edges = [];
          testConfig.nodes = [];
          testConfig.startCallback = function(){};
          testConfig.loadNode = function(){};
          expect(
            function() {
              eventLib.Expand(testConfig);
            }
          ).toThrow("A callback to reshape nodes has to be defined");
        });
        
      });
      
    });
    
    describe('Drag', function() {
      
      describe('setup process', function() {
        
        it('should throw an error if layouter is not given', function() {
          var testConfig = {};
          
          expect(
            function() {
              eventLib.checkDragConfig(testConfig);
            }
          ).toThrow("A layouter has to be defined");
          
          expect(
            function() {
              eventLib.Drag(testConfig);
            }
          ).toThrow("A layouter has to be defined");
        });
        
        it('should throw an error if the layouter does not offer a drag function', function() {
          var testConfig = {
            layouter: {}
          };
          
          expect(
            function() {
              eventLib.checkDragConfig(testConfig);
            }
          ).toThrow("The layouter has to offer a drag function");
          
          expect(
            function() {
              eventLib.Drag(testConfig);
            }
          ).toThrow("The layouter has to offer a drag function");
          
          testConfig.layouter.drag = 42;
          
          expect(
            function() {
              eventLib.checkDragConfig(testConfig);
            }
          ).toThrow("The layouter has to offer a drag function");
          
          expect(
            function() {
              eventLib.Drag(testConfig);
            }
          ).toThrow("The layouter has to offer a drag function");
        });
        
      });
      
    });
    
    describe('Insert Node', function() {
      
      it('should create an event to add a node', function() {
                
        var adapterDummy = {},
        nodes = [],
        created = null,
        called = false,
        callbackCheck = function() {
          called = true;
        },
        nodeEditorConfig = {
          nodes: nodes,
          adapter: adapterDummy,
          shaper: nodeShaperDummy
        },
        testee;
        
        adapterDummy.createNode = function(nodeToCreate, callback) {
          created = nodeToCreate;
          nodes.push(created);
          callback(created);
        };
        
        runs(function() {
          testee = eventLib.InsertNode(nodeEditorConfig);
          testee(callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(created).toBeDefined();
          expect(nodeShaperDummy.reshapeNodes).toHaveBeenCalled();
        });
        
      });
      
    });
    
    describe('Patch Node', function() {
      
      it('should create an event to patch a node', function() {
        var adapterDummy = {},
        patched = {id: "1"},
        data = {hello: "world"},
        nodes = [patched],
        called = false,
        callbackCheck = function() {
          called = true;
        },
        nodeEditorConfig = {
          nodes: nodes,
          adapter: adapterDummy,
          shaper: nodeShaperDummy
        },
        testee;
        
        adapterDummy.patchNode = function(nodeToPatch, patchData, callback) {
          patched = nodeToPatch;
          $.extend(patched, patchData);
          callback();
        };

        
        runs(function() {
          testee = eventLib.PatchNode(nodeEditorConfig);
          testee(patched, data, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(patched).toBeDefined();
          expect(nodeShaperDummy.reshapeNodes).toHaveBeenCalled();
          expect(patched.id).toEqual("1");
          expect(patched.hello).toEqual("world");
        });
      });
      
    });
    
    describe('Delete Node', function() {
      
      it('should create an event to delete a node', function() {
        var adapterDummy = {},
        toDel = {id: "2"},
        nodes = [toDel],
        deleted = null,
        called = false,
        callbackCheck = function() {
          called = true;
        },
        nodeEditorConfig = {
          nodes: nodes,
          adapter: adapterDummy,
          shaper: nodeShaperDummy
        },
        testee;
        
        adapterDummy.deleteNode = function(nodeToDelete, callback) {
          deleted = nodeToDelete;
          nodes.pop();
          callback();
        };
        
        
        runs(function() {
          testee = eventLib.DeleteNode(nodeEditorConfig);
          testee(toDel, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(deleted).toEqual(toDel);
          expect(nodeShaperDummy.reshapeNodes).toHaveBeenCalled();
        });
        
      });
    
    });
    
    
    describe('Insert Edge', function() {
      
      it('should create an event to add an edge', function() {
        var adapterDummy = {},
        edges = [],
        called = false,
        created = null,
        source = {_id: 1},
        target = {_id: 2},
        callbackCheck = function() {
          called = true;
        },
        edgeEditorConfig = {
          edges: edges,
          adapter: adapterDummy,
          shaper: edgeShaperDummy
        },
        testee;
        
        adapterDummy.createEdge = function(edgeToCreate, callback) {
          created = edgeToCreate;
          edges.push(created);
          callback(created);
        };
        
        runs(function() {
          testee = eventLib.InsertEdge(edgeEditorConfig);
          testee(source, target, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(created).toBeDefined();
          expect(edgeShaperDummy.reshapeEdges).toHaveBeenCalled();
          expect(created.source).toEqual(source);
          expect(created.target).toEqual(target);
        });
        
      });
      
    });
    
    describe('Patch Edge', function() {
      
      it('should create an event to patch an edge', function() {
        var adapterDummy = {},
        source = {_id: 1},
        target = {_id: 2},
        patched = {source: source, target: target},
        data = {hello: "world"},
        edges = [patched],
        called = false,
        callbackCheck = function() {
          called = true;
        },
        edgeEditorConfig = {
          edges: edges,
          adapter: adapterDummy,
          shaper: edgeShaperDummy
        },
        testee;
        
        adapterDummy.patchEdge = function(edgeToPatch, patchData, callback) {
          patched = edgeToPatch;
          $.extend(patched, patchData);
          callback();
        };
        
        runs(function() {
          testee = eventLib.PatchEdge(edgeEditorConfig);
          testee(patched, data, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(patched).toBeDefined();
          expect(edgeShaperDummy.reshapeEdges).toHaveBeenCalled();
          expect(patched.source).toEqual(source);
          expect(patched.target).toEqual(target);
          expect(patched.hello).toEqual("world");
        });
        
      });
      
    });
    
    describe('Delete Edge', function() {
      
      it('should create an event to delete an edge', function() {
        var adapterDummy = {},
        source = {_id: 1},
        target = {_id: 2},
        toDel = {source: source, target: target},
        edges = [toDel],
        deleted = null,
        called = false,
        callbackCheck = function() {
          called = true;
        },
        edgeEditorConfig = {
          edges: edges,
          adapter: adapterDummy,
          shaper: edgeShaperDummy
        },
        testee;
        
        adapterDummy.deleteEdge = function(edgeToDelete, callback) {
          deleted = edgeToDelete;
          edges.pop();
          callback();
        };

        
        runs(function() {
          testee = eventLib.DeleteEdge(edgeEditorConfig);
          testee(toDel, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(deleted).toEqual(toDel);
          expect(edgeShaperDummy.reshapeEdges).toHaveBeenCalled();
        });
        
      });
      
    });
  });

}());
