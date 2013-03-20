/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
/*global runs, spyOn, waitsFor */
/*global window, eb, loadFixtures, document, jQuery */
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


(function () {
  "use strict";

  describe('Event Library', function () {

    var eventLib;
    
    beforeEach(function() {
      eventLib = new EventLibrary();
    });

    describe('Expand', function() {
      
      var loaded,
      reshaped,
      nodes,
      edges,
      loadedNodes,
      reshapedNodes,
      started,
      loadNodeCallback = function(node) {
        loaded++;
        loadedNodes.push(node);
      },
      reshapeNodeCallback = function(node) {
        reshaped++;
        reshapedNodes.push(node);
      },
      startCallback = function() {
        started++;
      },
      testee;
      
      beforeEach(function() {
        loaded = 0;
        reshaped = 0;
        started = 0;
      
        nodes = [];
        edges = [];
        loadedNodes = [];
        reshapedNodes = [];
      });
      
      it('should expand a collapsed node', function() {
        var node = {
          _id: 0,
          _outboundCounter: 0,
          _inboundCounter: 0
        };
        nodes.push(node);
        testee = eventLib.Expand(
          edges,
          nodes,
          startCallback,
          loadNodeCallback,
          reshapeNodeCallback
        );
        testee(node);
        
        expect(node._expanded).toBeTruthy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(1);
        expect(reshapedNodes.length).toEqual(1);
        expect(reshapedNodes[0]).toEqual(node);
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
        testee = eventLib.Expand(
          edges,
          nodes,
          startCallback,
          loadNodeCallback,
          reshapeNodeCallback
        );
        testee(node);
        
        expect(node._expanded).toBeFalsy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(0);
        expect(reshapedNodes.length).toEqual(1);
        expect(reshapedNodes[0]).toEqual(node);
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
        
        testee = eventLib.Expand(
          edges,
          nodes,
          startCallback,
          loadNodeCallback,
          reshapeNodeCallback
        );
        testee(root);
        
        expect(root._expanded).toBeFalsy();
        expect(started).toEqual(1);
        expect(reshaped).toEqual(1);
        expect(loaded).toEqual(0);
        expect(nodes.length).toEqual(1);
        expect(edges.length).toEqual(0);
        expect(reshapedNodes.length).toEqual(1);
        expect(reshapedNodes[0]).toEqual(root);
      });
      
      describe('setup process', function() {
                
        it('should throw an error if edges are not given', function() {          
          expect(
            function() {
              eventLib.Expand();
            }
          ).toThrow("Edges have to be defined");
        });
        
        it('should throw an error if nodes are not given', function() {
          expect(
            function() {
              eventLib.Expand([]);
            }
          ).toThrow("Nodes have to be defined");
        });
        
        it('should throw an error if start callback is not given', function() {
          expect(
            function() {
              eventLib.Expand([], []);
            }
          ).toThrow("A callback to the Start-method has to be defined");
        });
        
        it('should throw an error if load node callback is not given', function() {
          expect(
            function() {
              eventLib.Expand([], [], function(){});
            }
          ).toThrow("A callback to load a node has to be defined");
        });
        
        it('should throw an error if reshape node callback is not given', function() {
          expect(
            function() {
              eventLib.Expand([], [], function(){}, function(){});
            }
          ).toThrow("A callback to reshape a node has to be defined");
        });
        
      });
      
    });
    
    describe('Insert Node', function() {
      
      it('should create an event to add a node', function() {
        var adapterDummy = {},
        nodeShaperDummy = {},
        nodes = [],
        created = null,
        reshaped = null,
        called = false,
        callbackCheck = function() {
          called = true;
        },
        testee;
        
        adapterDummy.createNode = function(nodeToCreate, callback) {
          created = nodeToCreate;
          nodes.push(created);
          callback(created);
        };
        
        nodeShaperDummy.drawNodes = function(nodes) {
          reshaped = nodes;
        };
        
        runs(function() {
          testee = eventLib.InsertNode(nodes, adapterDummy, nodeShaperDummy);
          testee(callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(created).toBeDefined();
          expect(created._inboundCounter).toEqual(0);
          expect(created._outboundCounter).toEqual(0);
          expect(reshaped[0]).toEqual(created);
        });
        
      });
      
    });
    
    describe('Patch Node', function() {
      
      it('should create an event to patch a node', function() {
        var adapterDummy = {},
        nodeShaperDummy = {},
        patched = {id: "1"},
        data = {hello: "world"},
        nodes = [patched],
        reshaped = null,
        called = false,
        callbackCheck = function() {
          called = true;
        },
        testee;
        
        adapterDummy.patchNode = function(nodeToPatch, patchData, callback) {
          patched = nodeToPatch;
          jQuery.extend(patched, patchData);
          callback();
        };
        
        nodeShaperDummy.drawNodes = function(nodes) {
          reshaped = nodes;
        };
        
        runs(function() {
          testee = eventLib.PatchNode(nodes, adapterDummy, nodeShaperDummy);
          testee(patched, data, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(patched).toBeDefined();
          expect(reshaped[0]).toEqual(patched);
          expect(patched.id).toEqual("1");
          expect(patched.hello).toEqual("world");
        });
      });
      
    });
    
    describe('Delete Node', function() {
      
      it('should create an event to delete a node', function() {
        var adapterDummy = {},
        nodeShaperDummy = {},
        toDel = {id: "2"},
        nodes = [toDel],
        deleted = null,
        reshaped = null,
        called = false,
        callbackCheck = function() {
          called = true;
        },
        testee;
        
        adapterDummy.deleteNode = function(nodeToDelete, callback) {
          deleted = nodeToDelete;
          nodes.pop();
          callback();
        };
        
        nodeShaperDummy.drawNodes = function(nodes) {
          reshaped = nodes;
        };
        
        runs(function() {
          testee = eventLib.DeleteNode(nodes, adapterDummy, nodeShaperDummy);
          testee(toDel, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(deleted).toEqual(toDel);
        });
        
      });
    
    });
    
    
    describe('Insert Edge', function() {
      
      it('should create an event to add an edge', function() {
        var adapterDummy = {},
        edgeShaperDummy = {},
        edges = [],
        reshaped = null,
        called = false,
        created = null,
        source = {_id: 1},
        target = {_id: 2},
        callbackCheck = function() {
          called = true;
        },
        testee;
        
        adapterDummy.createEdge = function(edgeToCreate, callback) {
          created = edgeToCreate;
          edges.push(created);
          callback(created);
        };
        
        edgeShaperDummy.drawEdges = function(edges) {
          reshaped = edges;
        };
        
        runs(function() {
          testee = eventLib.InsertEdge(edges, adapterDummy, edgeShaperDummy);
          testee(source, target, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(created).toBeDefined();
          expect(reshaped[0]).toEqual(created);
          expect(created.source).toEqual(source);
          expect(created.target).toEqual(target);
        });
        
      });
      
    });
    
    describe('Patch Edge', function() {
      
      it('should create an event to patch an edge', function() {
        var adapterDummy = {},
        edgeShaperDummy = {},
        source = {_id: 1},
        target = {_id: 2},
        patched = {source: source, target: target},
        data = {hello: "world"},
        edges = [patched],
        reshaped = null,
        called = false,
        callbackCheck = function() {
          called = true;
        },
        testee;
        
        adapterDummy.patchEdge = function(edgeToPatch, patchData, callback) {
          patched = edgeToPatch;
          jQuery.extend(patched, patchData);
          callback();
        };
        
        edgeShaperDummy.drawEdges = function(edges) {
          reshaped = edges;
        };
        
        runs(function() {
          testee = eventLib.PatchEdge(edges, adapterDummy, edgeShaperDummy);
          testee(patched, data, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(patched).toBeDefined();
          expect(reshaped[0]).toEqual(patched);
          expect(patched.source).toEqual(source);
          expect(patched.target).toEqual(target);
          expect(patched.hello).toEqual("world");
        });
        
      });
      
    });
    
    describe('Delete Edge', function() {
      
      it('should create an event to delete an edge', function() {
        var adapterDummy = {},
        edgeShaperDummy = {},
        source = {_id: 1},
        target = {_id: 2},
        toDel = {source: source, target: target},
        edges = [toDel],
        deleted = null,
        reshaped = null,
        called = false,
        callbackCheck = function() {
          called = true;
        },
        testee;
        
        adapterDummy.deleteEdge = function(edgeToDelete, callback) {
          deleted = edgeToDelete;
          edges.pop();
          callback();
        };
        
        edgeShaperDummy.drawEdges = function(edges) {
          reshaped = edges;
        };
        
        runs(function() {
          testee = eventLib.DeleteEdge(edges, adapterDummy, edgeShaperDummy);
          testee(toDel, callbackCheck);
        });
        
        waitsFor(function() {
          return called;
        });
        
        runs(function() {
          expect(deleted).toEqual(toDel);
        });
        
      });
      
    });
  });

}());
