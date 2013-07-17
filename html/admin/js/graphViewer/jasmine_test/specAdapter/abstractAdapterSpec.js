/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine */
/*global runs, spyOn, waitsFor, waits */
/*global window, document, setTimeout */
/*global $, _, d3*/
/*global describeInterface*/
/*global AbstractAdapter*/

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

  describe('Abstract Adapter', function () {
    
    var nodes, edges,
    descendant,
    
    getCommunityNodes = function() {
      return _.filter(nodes, function(n) {
        return n._isCommunity;
      });
    },
    
    getCommunityNodesIds = function() {
      return _.pluck(getCommunityNodes(), "_id");
    },
    
    nodeWithID = function(id) {
      return $.grep(nodes, function(e){
        return e._id === id;
      })[0];
    },
    edgeWithSourceAndTargetId = function(sourceId, targetId) {
      return $.grep(edges, function(e){
        return e.source._id === sourceId
          && e.target._id === targetId;
      })[0];
    },
    existNode = function(id) {
      var node = nodeWithID(id);
      expect(node).toBeDefined();
      expect(node._id).toEqual(id);
    },
    
    notExistNode = function(id) {
      var node = nodeWithID(id);
      expect(node).toBeUndefined();
    },
    
    existEdge = function(source, target) {
      var edge = edgeWithSourceAndTargetId(source, target);
      expect(edge).toBeDefined();
      expect(edge.source._id).toEqual(source);
      expect(edge.target._id).toEqual(target);
    },
    
    notExistEdge = function(source, target) {
      var edge = edgeWithSourceAndTargetId(source, target);
      expect(edge).toBeUndefined();
    },
    
    existNodes = function(ids) {
      _.each(ids, existNode);
    },
    
    notExistNodes = function(ids) {
      _.each(ids, notExistNode);
    };
    
    
    beforeEach(function() {
      nodes = [];
      edges = [];
      descendant = {
        loadNode: function(){}
      };
    });
    
    describe('setup process', function() {
      
      it('should throw an error if nodes are not given', function() {
        expect(
          function() {
            var t = new AbstractAdapter();
          }
        ).toThrow("The nodes have to be given.");
      });
      
      it('should throw an error if edges are not given', function() {
        expect(
          function() {
            var t = new AbstractAdapter([]);
          }
        ).toThrow("The edges have to be given.");
      });
      
      it('should throw an error if no inheriting class is given', function() {
        expect(
          function() {
            var t = new AbstractAdapter([], []);
          }
        ).toThrow("An inheriting class has to be given.");
      });
      
      it('should not throw an error if setup correctly', function() {
        expect(
          function() {
            var t = new AbstractAdapter([], [], descendant);
          }
        ).not.toThrow();
      });
      
      it('should create a NodeReducer instance', function() {
        spyOn(window, "NodeReducer");
        var nodes = [],
          edges = [],
          t = new AbstractAdapter(nodes, edges, descendant);
        expect(window.NodeReducer).wasCalledWith(nodes, edges);
      });
      
      it('should send the nodeReducer the configuration if given', function() {
        spyOn(window, "NodeReducer");
        var nodes = [],
          edges = [],
          config = {
            prioList: ["foo", "bar", "baz"]
          },
          t = new AbstractAdapter(nodes, edges, descendant, config);
        expect(window.NodeReducer).wasCalledWith(nodes, edges, ["foo", "bar", "baz"]);        
      });
      
      it('should create a ModularityJoiner worker', function() {
        spyOn(window, "WebWorkerWrapper");
        var nodes = [],
          edges = [],
          t = new AbstractAdapter(nodes, edges, descendant);
        expect(window.WebWorkerWrapper).wasCalledWith(
          window.ModularityJoiner,
          jasmine.any(Function)
        );
      });
      
    });
    
    describe('checking the interface', function() {
      var testee;
      
      beforeEach(function() {
        testee = new AbstractAdapter([], [], descendant);
        this.addMatchers({
          toHaveFunction: function(func, argCounter) {
            var obj = this.actual;
            this.message = function(){
              return testee.constructor.name
                + " should react to function "
                + func
                + " with at least "
                + argCounter
                + " arguments.";
            };
            if (typeof(obj[func]) !== "function") {
              return false;
            }
            if (obj[func].length < argCounter) {
              return false;
            }
            return true;
          }
        });
      });
      
      it('should offer a function to set the width', function() {
        expect(testee).toHaveFunction("setWidth", 1);
      });
      
      it('should offer a function to set the height', function() {
        expect(testee).toHaveFunction("setHeight", 1);
      });
      
      it('should offer a function to insert a node', function() {
        expect(testee).toHaveFunction("insertNode", 1);
      });
      
      it('should offer a function to insert an edge', function() {
        expect(testee).toHaveFunction("insertEdge", 1);
      });
      
      it('should offer a function to remove a node', function() {
        expect(testee).toHaveFunction("removeNode", 1);
      });
      
      it('should offer a function to remove an edge', function() {
        expect(testee).toHaveFunction("removeEdge", 1);
      });
      
      it('should offer a function to remove edges for a given node', function() {
        expect(testee).toHaveFunction("removeEdgesForNode", 1);
      });
  
      it('should offer a function to expand a community', function() {
        expect(testee).toHaveFunction("expandCommunity", 1);
      });
  
      it('should offer a function to set the node limit', function() {
        expect(testee).toHaveFunction("setNodeLimit", 1);
      });
      
      it('should offer a function to set the child limit', function() {
        expect(testee).toHaveFunction("setChildLimit", 1);
      });

      it('should offer a function to check the amount of freshly inserted nodes', function() {
        expect(testee).toHaveFunction("checkSizeOfInserted", 1);
      });
      
      it('should offer a function to check the overall amount of nodes', function() {
        expect(testee).toHaveFunction("checkNodeLimit", 1);
      });
      
      it('should offer a function to change to a different configuration', function() {
        expect(testee).toHaveFunction("changeTo", 1);
      });
      
      it('should offer a function to get the current priority list', function() {
        expect(testee).toHaveFunction("getPrioList", 0);
      });
    });
    
    describe('checking nodes', function() {
      
      var adapter;
      
      beforeEach(function() {
        adapter = new AbstractAdapter(nodes, [], descendant);
      });
      
      it('should be possible to insert a node', function() {
        var newNode = {_id: 1};
        adapter.insertNode(newNode);
        existNode(1);
        expect(nodes.length).toEqual(1);
      });
      
      it('should not add the same node twice', function() {
        var newNode = {_id: 1};
        adapter.insertNode(newNode);
        adapter.insertNode(newNode);
        existNode(1);
        expect(nodes.length).toEqual(1);
      });
      
      it('should add x and y coordinates', function() {
        var newNode = {_id: 1};
        adapter.insertNode(newNode);
        this.addMatchers({
          toHaveCorrectCoordinates: function() {
            var list = this.actual,
              evil;
            _.each(list, function(n) {
              if (isNaN(n.x) || isNaN(n.y)) {
                evil = n;
              }
            });
            this.message = function() {
              return "Expected " + JSON.stringify(evil) + " to contain Numbers as X and Y.";
            };
            return evil === undefined;
          }
        });
        expect(nodes).toHaveCorrectCoordinates();
      });
      
      it('should set in- and outbound counters', function() {
        var newNode = adapter.insertNode({_id: 1});
        expect(newNode._outboundCounter).toEqual(0);
        expect(newNode._inboundCounter).toEqual(0);
      });
      
      it('should set the x coordinate close to the center', function() {
        var width = 200,
          newNode;
        adapter.setWidth(width);
        newNode = adapter.insertNode({_id: 1});
        expect(newNode.x).toBeGreaterThan(width / 4);
        expect(newNode.x).toBeLessThan(3 * width / 4);
      });
      
      it('should set the y coordinate close to the center', function() {
        var height = 200,
          newNode;
        adapter.setHeight(height);
        newNode = adapter.insertNode({_id: 1});
        expect(newNode.y).toBeGreaterThan(height / 4);
        expect(newNode.y).toBeLessThan(3 * height / 4);
      });
      
      it('should encapsulate all attributes in _data', function() {
        var data = {
            _id: 1,
            name: "Alice",
            age: 42
          },
          newNode = adapter.insertNode(data);
        expect(newNode).toBeDefined();
        expect(newNode._data).toEqual(data);
      });
      
      it('should be able to delete a node', function() {
        var toDelete = {_id: 1},
        nodeToDelete = adapter.insertNode(toDelete);
        adapter.removeNode(nodeToDelete);
        
        notExistNode(1);
        expect(nodes.length).toEqual(0);
      });
      
    });
    
    describe('checking edges', function() {
      
      var adapter,
      source,
      target,
      sourceid,
      targetid;
      
      beforeEach(function() {
        adapter = new AbstractAdapter(nodes, edges, descendant);
        source = adapter.insertNode({_id: 1});
        target = adapter.insertNode({_id: 2});
        sourceid = source._id;
        targetid = target._id;
      });
      
      it('should be able to insert an edge', function() {
        adapter.insertEdge({
          _id: "1-2",
          _from: sourceid,
          _to: targetid
        });
        existEdge(sourceid, targetid);
        expect(edges.length).toEqual(1);
      });
      
      it('should not insert the same edge twice', function() {
        var toInsert = {
          _id: "1-2",
          _from: sourceid,
          _to: targetid
        };
        adapter.insertEdge(toInsert);
        adapter.insertEdge(toInsert);
        existEdge(sourceid, targetid);
        expect(edges.length).toEqual(1);
      });
      
      it('should throw an error if an edge is inserted with illeagal source', function() {
        expect(
          function() {
            adapter.insertEdge({
              _id: "1-3",
              _from: 3,
              _to: targetid
            });
          }
        ).toThrow("Unable to insert Edge, source node not existing 3");
        
        notExistEdge(3, targetid);
        expect(edges.length).toEqual(0);
      });
      
      it('should throw an error if an edge is inserted with illeagal target', function() {
        expect(
          function() {
            adapter.insertEdge({
              _id: "1-3",
              _from: sourceid,
              _to: 3
            });
          }
        ).toThrow("Unable to insert Edge, target node not existing 3");
        notExistEdge(sourceid, 3);
        expect(edges.length).toEqual(0);
      });
      
      it('should change the in- and outbound counters accordingly', function() {
        var toInsert = {
          _id: "1-2",
          _from: sourceid,
          _to: targetid
        };
        adapter.insertEdge(toInsert);
        expect(source._outboundCounter).toEqual(1);
        expect(source._inboundCounter).toEqual(0);
        expect(target._outboundCounter).toEqual(0);
        expect(target._inboundCounter).toEqual(1);
      });
      
      it('should encapsulate all attributes in _data', function() {
        var data = {
          _id: "1-2",
          _from: sourceid,
          _to: targetid,
          label: "MyLabel",
          color: "black"
        },
          edge = adapter.insertEdge(data);
        expect(edge._data).toBeDefined();
        expect(edge._data).toEqual(data);
      });
      
      it('should be able to delete an edge', function() {
        var toDelete = {
          _id: "1-2",
          _from: sourceid,
          _to: targetid
        },
        edgeToDel = adapter.insertEdge(toDelete);
        adapter.removeEdge(edgeToDel);
        
        notExistEdge(sourceid, targetid);
        expect(edges.length).toEqual(0);
      });
      
      it('should be able to remove all edges of a node', function() {
        adapter.insertNode({_id: 3});
        adapter.insertEdge({
          _id: "3-1",
          _from: 3,
          _to: sourceid // This is nodes[0]
        });
        var edgeToKeep = adapter.insertEdge({
          _id: "2-3",
          _from: targetid, // This is nodes[1]
          _to: 3
        });
        
        adapter.removeEdgesForNode(source);
        
        existEdge(2, 3);
        notExistEdge(1, 2);
        notExistEdge(3, 1);
        expect(edges.length).toEqual(1);
        expect(edges[0]).toEqual(edgeToKeep);
      });
      
      it('should maintain in- and outboundcounter '
      + 'when removing all edges of a node', function() {
        var thirdNode = adapter.insertNode({_id: 3});
        adapter.insertEdge({
          _id: "3-1",
          _from: 3,
          _to: sourceid // This is nodes[0]
        });
        adapter.insertEdge({
          _id: "2-3",
          _from: targetid, // This is nodes[1]
          _to: 3
        });
        
        adapter.removeEdgesForNode(source);
        expect(nodes.length).toEqual(3);
        existNode(1);
        existNode(2);
        existNode(3);
        expect(source._inboundCounter).toEqual(0);
        expect(source._outboundCounter).toEqual(0);
        expect(target._inboundCounter).toEqual(0);
        expect(target._outboundCounter).toEqual(1);
        expect(thirdNode._inboundCounter).toEqual(1);
        expect(thirdNode._outboundCounter).toEqual(0);
      });
      
    });
    
    describe('checking node exploration', function() {
      
      var adapter,
        mockReducer,
        mockWrapper,
        workerCB;
      
      beforeEach(function() {
        mockWrapper = {};
        mockWrapper.call = function() {};
        mockReducer = {};
        mockReducer.getCommunity = function() {};
        mockReducer.bucketNodes = function() {};
        spyOn(window, "NodeReducer").andCallFake(function(v, e) {
          return {
            bucketNodes: function(toSort, numBuckets) {
              return mockReducer.bucketNodes(toSort, numBuckets);
            }
          };
        });
        spyOn(window, "WebWorkerWrapper").andCallFake(function(c, cb) {
          workerCB = cb;
          return {
            call: function() {
              mockWrapper.call.apply(
                mockWrapper,
                Array.prototype.slice.call(arguments)
              );
            }
          };
        });
        adapter = new AbstractAdapter(nodes, edges, descendant);
      });
      
      it('should expand a collapsed node', function() {
        var node = {
          _id: "0"
        },
        loaded = 0,
        loadedNodes = [];
        adapter.insertNode(node);
        spyOn(descendant, "loadNode").andCallFake(function(node) {
          loaded++;
          loadedNodes.push(node);
        });
        
        adapter.explore(node);
        
        expect(descendant.loadNode).wasCalled();
        
        expect(node._expanded).toBeTruthy();
        expect(loaded).toEqual(1);
        expect(loadedNodes.length).toEqual(1);
        expect(loadedNodes[0]).toEqual(node._id);
        
      });
      
      it('should collapse an expanded node', function() {
        var node = {
          _id: "0"
        };
        node = adapter.insertNode(node);
        node._expanded = true;
        spyOn(descendant, "loadNode");
        
        
        adapter.explore(node);
        
        expect(node._expanded).toBeFalsy();

        expect(descendant.loadNode).wasNotCalled();
      });
      
      it('should collapse a tree', function() {
        var root = {
          _id: "0"
        },
        c1 = {
          _id: "1"
        },
        c2 = {
          _id: "2"
        },
        e1 = {
          _id: "0-1",
          _from: "0",
          _to: "1"
        },
        e2 = {
          _id: "0-2",
          _from: "0",
          _to: "2"
        };
        
        root = adapter.insertNode(root);
        // Fake expansion of node
        root._expanded = true;
        
        adapter.insertNode(c1);
        adapter.insertNode(c2);
        
        adapter.insertEdge(e1);
        adapter.insertEdge(e2);

        spyOn(descendant, "loadNode");
        adapter.explore(root);
        
        expect(root._expanded).toBeFalsy();

        expect(descendant.loadNode).wasNotCalled();
        expect(nodes.length).toEqual(1);
        expect(edges.length).toEqual(0);
      });
      
      it('should not remove referenced nodes on collapsing ', function() {
        var root = {
          _id: "0"
        },
        c1 = {
          _id: "1"
        },
        c2 = {
          _id: "2"
        },
        c3 = {
          _id: "3"
        },
        e1 = {
          _id: "0-1",
          _from: "0",
          _to: "1"
        },
        e2 = {
          _id: "0-3",
          _from: "0",
          _to: "3"
        },
        e3 = {
          _id: "2-1",
          _from: "2",
          _to: "1"
        };
        
        root = adapter.insertNode(root);
        root._expanded = true;
        c1 = adapter.insertNode(c1);
        c2 = adapter.insertNode(c2);
        adapter.insertNode(c3);
        adapter.insertEdge(e1);
        adapter.insertEdge(e2);
        adapter.insertEdge(e3);
        
        spyOn(descendant, "loadNode");
        adapter.explore(root);
        
        expect(root._expanded).toBeFalsy();
        expect(descendant.loadNode).wasNotCalled();
        expect(nodes.length).toEqual(3);
        expect(edges.length).toEqual(1);
        
        expect(root._outboundCounter).toEqual(0);
        expect(c1._inboundCounter).toEqual(1);
        expect(c2._outboundCounter).toEqual(1);
      });
      
      describe('with community nodes', function() {
        
        it('should expand a community node properly', function() {
          // Hack for a CommunityNode
          var comm = {
            _id: "*community_1"
          };
          comm = adapter.insertNode(comm);
          comm._isCommunity = true;
          
          spyOn(adapter, "expandCommunity");
          adapter.explore(comm, function() {});
        
          expect(adapter.expandCommunity).toHaveBeenCalledWith(comm, jasmine.any(Function));
        });
        
        it('should remove a community if last pointer to it is collapsed', function() {
          
          runs(function() {
            var c0 = {
                _id: "0"
              },
              internal = {
               _id: "internal"
              },
              internal2 = {
               _id: "internal2"
              },
              c1 = {
                _id: "1"
              },
              c2 = {
                _id: "2"
              },
              e0 = {
                _id: "0-1",
                _from: "0",
                _to: "1"
              },
              e1 = {
                _id: "1-c",
                _from: "1",
                _to: "internal"
              },
              e2 = {
                _id: "c-2",
                _from: "internal",
                _to: "2"
              };
            
            c0 = adapter.insertNode(c0);
            c1 = adapter.insertNode(c1);
            c1._expanded = true;
            adapter.insertNode(internal);
            adapter.insertNode(c2);
            e0 = adapter.insertEdge(e0);
            adapter.insertEdge(e1);
            adapter.insertEdge(e2);
            
            spyOn(mockWrapper, "call").andCallFake(function(name) {
              workerCB({
                data: {
                  cmd: name,
                  result: ["internal", "internal2"]
                }
              });
            });
            
            adapter.setNodeLimit(3);
            
            adapter.explore(c1);
            
            expect(nodes.length).toEqual(2);
            expect(edges.length).toEqual(1);
            
            expect(nodes).toEqual([c0, c1]);
            expect(edges).toEqual([e0]);
          });
        
        });
        
        it('should not remove a community if a pointer to it still exists', function() {
          
          runs(function() {
            var c0 = {
                _id: "0"
              },
              c1 = {
                _id: "1"
              },
              internal = {
               _id: "internal"
              },
              internal2 = {
               _id: "internal2"
              },
              e0 = {
                _id: "0-1",
                _from: "0",
                _to: "1"
              },
              e1 = {
                _id: "0-c",
                _from: "0",
                _to: "internal"
              },
              e2 = {
                _id: "1-c",
                _from: "1",
                _to: "internal"
              },
              comm;
            c0 = adapter.insertNode(c0);
            c1 = adapter.insertNode(c1);
            c1._expanded = true;
            adapter.insertNode(internal);
            adapter.insertNode(internal2);
            e0 = adapter.insertEdge(e0);
            e1 = adapter.insertEdge(e1);
            adapter.insertEdge(e2);
            
            spyOn(mockWrapper, "call").andCallFake(function(name) {
              workerCB({
                data: {
                  cmd: name,
                  result: ["internal", "internal2"]
                }
              });
            });
            
            adapter.setNodeLimit(3);
            
            comm = getCommunityNodes()[0];
            
            adapter.explore(c1);

            expect(nodes).toEqual([c0, c1, comm]);
            expect(edges).toEqual([e0, e1]);
          });
        
        });
        
        it('should be possible to explore a node, collapse it and explore it again', function() {
          var id1 = "1",
            id2 = "2",
            id3 = "3",
            id4 = "4",
            id5 = "5",
            id6 = "6",
            ids = "start",
            n1 = {_id: id1},
            n2 = {_id: id2},
            n3 = {_id: id3},
            n4 = {_id: id4},
            n5 = {_id: id5},
            n6 = {_id: id6},
            ns = {_id: ids},
            intS,
            int1,
            int2,
            int3,
            int4,
            int5,
            int6,
            es1 = {
              _id: ids + "-" + id1,
              _from: ids,
              _to: id1
            },
            es2 = {
              _id: ids + "-" + id2,
              _from: ids,
              _to: id2
            },
            es3 = {
              _id: ids + "-" + id3,
              _from: ids,
              _to: id3
            },
            es4 = {
              _id: ids + "-" + id4,
              _from: ids,
              _to: id4
            },
            es5 = {
              _id: ids + "-" + id5,
              _from: ids,
              _to: id5
            },
            es6 = {
              _id: ids + "-" + id6,
              _from: ids,
              _to: id6
            };
          spyOn(mockReducer, "bucketNodes").andCallFake(function(list) {
            return [
              {
                reason: {
                  type: "similar"
                },
                nodes: [int1, int2, int3]
              },
              {
                reason: {
                  type: "similar"
                },
                nodes: [int4, int5, int6]
              }
            ];
          });
          spyOn(descendant, "loadNode").andCallFake(function() {
            var inserted = {};
            
            int1 = adapter.insertNode(n1);
            int2 = adapter.insertNode(n2);
            int3 = adapter.insertNode(n3);
            int4 = adapter.insertNode(n4);
            int5 = adapter.insertNode(n5);
            int6 = adapter.insertNode(n6);
            
            inserted.id1 = int1;
            inserted.id2 = int2;
            inserted.id3 = int3;
            inserted.id4 = int4;
            inserted.id5 = int5;
            inserted.id6 = int6;
            
            adapter.insertEdge(es1);
            adapter.insertEdge(es2);
            adapter.insertEdge(es3);
            adapter.insertEdge(es4);
            adapter.insertEdge(es5);
            adapter.insertEdge(es6);

            adapter.checkSizeOfInserted(inserted);
          });
          
          adapter.setChildLimit(2);
          expect(nodes).toEqual([]);
          
          intS = adapter.insertNode(ns);
          
          expect(nodes).toEqual([intS]);
          
          adapter.explore(intS);
          
          expect(nodes.length).toEqual(3);
          expect(edges.length).toEqual(6);
          expect(mockReducer.bucketNodes).wasCalledWith(
            [int1, int2, int3, int4, int5, int6], 2
          );
          
          expect(intS._expanded).toBeTruthy();
          
          adapter.explore(intS);
          
          
          expect(nodes.length).toEqual(1);
          expect(edges.length).toEqual(0);
          
          expect(intS._expanded).toBeFalsy();
          
          adapter.explore(intS);
          
          expect(intS._expanded).toBeTruthy();
          
          expect(nodes.length).toEqual(3);
          expect(edges.length).toEqual(6);
          expect(mockReducer.bucketNodes).wasCalledWith(
            [int1, int2, int3, int4, int5, int6], 2
          );
          
        });
        
        
      });
      
    });
    
    describe('checking communities', function() {
      
      var adapter,
        mockReducer,
        mockWrapper,
        workerCB;
      
      beforeEach(function() {
        mockWrapper = {};
        mockWrapper.call = function() {};
        mockReducer = {};
        mockReducer.getCommunity = function() {};
        mockReducer.bucketNodes = function() {};
        spyOn(window, "NodeReducer").andCallFake(function(v, e) {
          return {
            getCommunity: function(limit, focus) {
              if (focus !== undefined) {
                return mockReducer.getCommunity(limit, focus);
              }
              return mockReducer.getCommunity(limit);
            },
            bucketNodes: function(toSort, numBuckets) {
              return mockReducer.bucketNodes(toSort, numBuckets);
            }
          };
        });
        spyOn(window, "WebWorkerWrapper").andCallFake(function(c, cb) {
          workerCB = cb;
          return {
            call: function() {
              mockWrapper.call.apply(
                mockWrapper,
                Array.prototype.slice.call(arguments)
              );
            }
          };
        });
        adapter = new AbstractAdapter(nodes, edges, descendant);
      });
      
      it('should not take any action if no limit is set', function() {
        spyOn(mockWrapper, "call");
        adapter.insertNode({_id: 1});
        adapter.insertNode({_id: 2});
        adapter.insertNode({_id: 3});
        adapter.insertNode({_id: 4});
        adapter.insertNode({_id: 5});
        adapter.checkNodeLimit();
        expect(mockWrapper.call).wasNotCalled();
      });
      
      it('should take a given focus into account', function() {
        var n1, limit;
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          workerCB({
            data: {
              cmd: name,
              result: [2, 4]
            }
          });
        });
        limit = 2;
        adapter.setNodeLimit(limit);
        n1 = adapter.insertNode({_id: 1});
        adapter.insertNode({_id: 2});
        adapter.insertNode({_id: 3});
        adapter.insertNode({_id: 4});
        adapter.insertNode({_id: 5});
        adapter.checkNodeLimit(n1);
        expect(mockWrapper.call).toHaveBeenCalledWith("getCommunity", limit, n1._id);
      });
      
      it('should create a community if too many nodes are added', function() {
        var n1, n2, commId;
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          workerCB({
            data: {
              cmd: name,
              result: [1, 2]
            }
          });
        });
        adapter.setNodeLimit(2);
        n1 = adapter.insertNode({_id: 1});
        n2 = adapter.insertNode({_id: 2});
        
        adapter.insertNode({_id: 3});
        adapter.checkNodeLimit();
        expect(mockWrapper.call).wasCalledWith("getCommunity", 2);
        expect(getCommunityNodesIds().length).toEqual(1);
        notExistNode([1, 2]);
        existNode(3);
      });
      
      it('should create a community if limit is set to small', function() {
        var n1, n2, commId;
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          workerCB({
            data: {
              cmd: name,
              result: [1, 2]
            }
          });
        });
        n1 = adapter.insertNode({_id: 1});
        n2 = adapter.insertNode({_id: 2});
        adapter.insertNode({_id: 3});
        adapter.setNodeLimit(2);
        expect(mockWrapper.call).wasCalledWith("getCommunity", 2);
        expect(getCommunityNodesIds().length).toEqual(1);
        notExistNode([1, 2]);
        existNode(3);
      });
      
      it('should create proper attributes for the community', function() {
        var n1, n2, n3, n4, n5, com;
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          workerCB({
            data: {
              cmd: name,
              result: [1, 2, 3, 4, 5]
            }
          });
        });
        n1 = adapter.insertNode({_id: 1});
        n2 = adapter.insertNode({_id: 2});
        n3 = adapter.insertNode({_id: 3});
        n4 = adapter.insertNode({_id: 4});
        n5 = adapter.insertNode({_id: 5});
        adapter.setNodeLimit(2);
        com = getCommunityNodes()[0];
        expect(com.x).toBeDefined();
        expect(com.y).toBeDefined();
        expect(com._size).toEqual(5);
      });
      
      it('should not trigger getCommunity multiple times', function() {
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          setTimeout(function() {
            workerCB({
              data: {
                cmd: name,
                result: [1, 2]
              }
            });
          }, 200);
        });
        adapter.insertNode({_id: 1});
        adapter.insertNode({_id: 2});
        adapter.insertNode({_id: 3});
        adapter.insertNode({_id: 4});
        adapter.insertNode({_id: 5});
        adapter.insertNode({_id: 6});
        adapter.setNodeLimit(5);
        adapter.setNodeLimit(4);
        adapter.setNodeLimit(3);
        adapter.setNodeLimit(2);
        adapter.setNodeLimit(1);
        expect(mockWrapper.call).wasCalledWith("getCommunity", 5);
        expect(mockWrapper.call.calls.length).toEqual(1);
      });

      it('should be able to trigger getCommunity after it returns', function() {
        
        var send;
        
        runs(function() {
          send = false;
          spyOn(mockWrapper, "call").andCallFake(function(name) {
            if (!send) {
              setTimeout(function() {
                send = true;
                workerCB({
                  data: {
                    cmd: name,
                    result: [1, 2]
                  }
                });
              }, 200);
            }
            
          });
          adapter.insertNode({_id: 1});
          adapter.insertNode({_id: 2});
          adapter.insertNode({_id: 3});
          adapter.insertNode({_id: 4});
          adapter.insertNode({_id: 5});
          adapter.insertNode({_id: 6});
          adapter.setNodeLimit(5);
          adapter.setNodeLimit(4);
          expect(mockWrapper.call).wasCalledWith("getCommunity", 5);
          expect(mockWrapper.call.calls.length).toEqual(1);
        });
        
        waitsFor(function() {
          return send;
        });
        
        runs(function() {
          adapter.setNodeLimit(3);
          expect(mockWrapper.call).wasCalledWith("getCommunity", 3);
          expect(mockWrapper.call.calls.length).toEqual(2);
        });
        
      });
      
      it('should connect edges to communities', function() {
        var n1, n2, comm, e1, e2, e3;
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          workerCB({
            data: {
              cmd: name,
              result: [1, 2]
            }
          });
        });
        n1 = adapter.insertNode({_id: 1});
        n2 = adapter.insertNode({_id: 2});
        adapter.insertNode({_id: 3});
        e1 = adapter.insertEdge({
          _id: "1-2",
          _from: 1,
          _to: 2        
        });
        e2 = adapter.insertEdge({
          _id: "2-3",
          _from: 2,
          _to: 3        
        });
        e3 = adapter.insertEdge({
          _id: "3-1",
          _from: 3,
          _to: 1        
        });
        adapter.setNodeLimit(2);
        
        comm = getCommunityNodes()[0];
        notExistEdge(1, 2);
        expect(e2.source).toEqual(comm);
        expect(e3.target).toEqual(comm);
      });
      
      describe('if a community allready exists', function() {
        
        var n1, n2, n3, n4, 
        e1, e2, e3, comm,
        fakeData;
        
        beforeEach(function() {
          fakeData = [1, 2];
          spyOn(mockWrapper, "call").andCallFake(function(name) {
            workerCB({
              data: {
                cmd: name,
                result: fakeData
              }
            });
          });

          n1 = adapter.insertNode({_id: 1});
          n2 = adapter.insertNode({_id: 2});
          n3 = adapter.insertNode({_id: 3});
          n4 = adapter.insertNode({_id: 4});
          e1 = adapter.insertEdge({
            _id: "1-2",
            _from: 1,
            _to: 2        
          });
          e2 = adapter.insertEdge({
            _id: "2-3",
            _from: 2,
            _to: 3        
          });
          e3 = adapter.insertEdge({
            _id: "3-1",
            _from: 3,
            _to: 1        
          });
          
          adapter.setNodeLimit(3);
          comm = getCommunityNodes()[0];
        });
        
        it('should be able to expand the community', function() {
          adapter.setNodeLimit(10);
          adapter.expandCommunity(comm);
          
          expect(getCommunityNodes().length).toEqual(0);
          expect(nodes.length).toEqual(4);
          expect(edges.length).toEqual(3);
          existEdge(1, 2);
          existEdge(2, 3);
          existEdge(3, 1);
          existNodes([1, 2, 3, 4]);
        });
        
        it('should collapse another community if limit is to small', function() {
          fakeData = [3, 4];

          adapter.expandCommunity(comm);
          expect(getCommunityNodes().length).toEqual(1);
          var comm2 = getCommunityNodes()[0];
          expect(comm).not.toEqual(comm2);
          expect(mockWrapper.call).wasCalled();
          expect(nodes.length).toEqual(3);
          existNodes([1, 2]);
          notExistNodes([3, 4]);
        });
        
        it('should collapse another community if limit is further reduced', function() {
          fakeData = [3, 4];
          adapter.setNodeLimit(2);
          expect(getCommunityNodes().length).toEqual(2);
          var comm2 = getCommunityNodes()[1];
          expect(comm).not.toEqual(comm2);
          expect(mockWrapper.call).wasCalled();
          expect(nodes.length).toEqual(2);
          notExistNodes([1, 2, 3, 4]);
        });
        
        it('should be possible to insert an edge targeting a node in the community', function() {
          var e = adapter.insertEdge({
            _id: "4-2",
            _from: 4,
            _to: 2
          });
          expect(e.source).toEqual(n4);
          expect(e.target).toEqual(comm);
          expect(e._target).toEqual(n2);
          expect(comm.dissolve().edges.inbound).toEqual([e3, e]);
        });
        
        it('should be possible to insert an edge starting'
        + ' from a node in the community', function() {
          var e = adapter.insertEdge({
            _id: "2-4",
            _from: 2,
            _to: 4
          });
          expect(e.source).toEqual(comm);
          expect(e._source).toEqual(n2);
          expect(e.target).toEqual(n4);
          expect(comm.dissolve().edges.outbound).toEqual([e2, e]);
        });
        
      });
      
    });
    
    describe('checking information of modularity joiner', function() {
      
      var adapter,
      source,
      target,
      sourceid,
      targetid,
      mockWrapper,
      mockReducer,
      workerCB;
      
      beforeEach(function() {
        mockReducer = {};
        mockReducer.bucketNodes = function() {};
        spyOn(window, "NodeReducer").andCallFake(function(v, e) {
          return {
            bucketNodes: function(toSort, numBuckets) {
              return mockReducer.bucketNodes(toSort, numBuckets);
            }
          };
        });
        mockWrapper = {};
        mockWrapper.call = function() {};
        spyOn(window, "WebWorkerWrapper").andCallFake(function(c, cb) {
          workerCB = cb;
          return {
            call: function() {
              mockWrapper.call.apply(
                mockWrapper,
                Array.prototype.slice.call(arguments)
              );
            }
          };
        });
        adapter = new AbstractAdapter(nodes, edges, descendant);
        source = adapter.insertNode({_id: "1"});
        target = adapter.insertNode({_id: "2"});
        sourceid = source._id;
        targetid = target._id;
      });
      
      it('should be informed if an edge is inserted', function() {
        spyOn(mockWrapper, "call");
        adapter.insertEdge({
          _id: "1-2",
          _from: sourceid,
          _to: targetid
        });
        expect(mockWrapper.call).wasCalledWith("insertEdge", sourceid, targetid);
      });
      
      it('should be informed if an edge is removed', function() {
        var toDelete = {
          _id: "1-2",
          _from: sourceid,
          _to: targetid
        },
        edgeToDel = adapter.insertEdge(toDelete);
        spyOn(mockWrapper, "call");
        adapter.removeEdge(edgeToDel);
        
        expect(mockWrapper.call).wasCalledWith("deleteEdge", sourceid, targetid);
      });
      
      it('should be informed if all edges for a node are removed', function() {
        var toDelete1 = {
            _id: "1-2",
            _from: sourceid,
            _to: targetid
          },
          toDelete2 = {
            _id: "2-1",
            _from: targetid,
            _to: sourceid
          };
        adapter.insertEdge(toDelete1);
        adapter.insertEdge(toDelete2);
        spyOn(mockWrapper, "call");
        
        adapter.removeEdgesForNode(source);
        
        expect(mockWrapper.call).wasCalledWith("deleteEdge", sourceid, targetid);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", targetid, sourceid);
      });
      
      it('should remove all edges of a community if it is joined', function() {
        var n1, n2, n3, n4,
          e1, e2, e3, e4,
          eout, ein;
        n1 = {
          _id: "n1"
        };
        n2 = {
          _id: "n2"
        };
        n3 = {
          _id: "n3"
        };
        n4 = {
          _id: "n4"
        };
        e1 = {
          _id: "n1-n2",
          _from: n1._id,
          _to: n2._id
        };
        e2 = {
          _id: "n2-n3",
          _from: n2._id,
          _to: n3._id
        };
        e3 = {
          _id: "n3-n4",
          _from: n3._id,
          _to: n4._id
        };
        e4 = {
          _id: "n4-n1",
          _from: n4._id,
          _to: n1._id
        };
        eout = {
          _id: "n1-1",
          _from: n1._id,
          _to: source._id
        };
        ein = {
          _id: "2-n1",
          _from: target._id,
          _to: n1._id
        };
        adapter.insertNode(n1);
        adapter.insertNode(n2);
        adapter.insertNode(n3);
        adapter.insertNode(n4);
      
        adapter.insertEdge(e1);
        adapter.insertEdge(e2);
        adapter.insertEdge(e3);
        adapter.insertEdge(e4);
        adapter.insertEdge(eout);
        adapter.insertEdge(ein);
      
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          if (name === "getCommunity") {
            workerCB({
              data: {
                cmd: name,
                result: [n1._id, n2._id, n3._id, n4._id]
              }
            });
          }
        });
        adapter.setNodeLimit(3);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", n1._id, n2._id);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", n2._id, n3._id);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", n3._id, n4._id);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", n4._id, n1._id);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", n1._id, source._id);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", target._id, n1._id);
        // 1 time getCommunity, 7 times deleteEdge
        expect(mockWrapper.call.calls.length).toEqual(7);
      });
      
      it('should not be informed of edges connected to communities', function() {
        var n1, n2, n3,
          e1, e2;
        n1 = {
          _id: "n1"
        };
        n2 = {
          _id: "n2"
        };
        n3 = {
          _id: "n3"
        };
        e1 = {
          _id: "n1-n2",
          _from: n1._id,
          _to: n2._id
        };
        e2 = {
          _id: "n2-n3",
          _from: n2._id,
          _to: n3._id
        };

        adapter.insertNode(n1);
        adapter.insertNode(n2);
        adapter.insertNode(n3);
      
        adapter.insertEdge(e1);
        
      
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          if (name === "getCommunity") {
            workerCB({
              data: {
                cmd: name,
                result: [n1._id, n2._id]
              }
            });
          }
        });
        adapter.setNodeLimit(3);
                
        adapter.insertEdge(e2);
        expect(mockWrapper.call).not.wasCalledWith("insertEdge", n2._id, n3._id);
        
      });
      
      it('should be informed if a community is expanded', function() {
        var n1, n2, n3,
          e1, e2;
        n1 = {
          _id: "n1"
        };
        n2 = {
          _id: "n2"
        };
        n3 = {
          _id: "n3"
        };
        e1 = {
          _id: "n1-n2",
          _from: n1._id,
          _to: n2._id
        };
        e2 = {
          _id: "n2-n3",
          _from: n2._id,
          _to: n3._id
        };

        adapter.insertNode(n1);
        adapter.insertNode(n2);
        adapter.insertNode(n3);
      
        adapter.insertEdge(e1);
        adapter.insertEdge(e2);
      
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          if (name === "getCommunity") {
            workerCB({
              data: {
                cmd: name,
                result: [n1._id, n2._id, n3._id]
              }
            });
          }
        });
        adapter.setNodeLimit(3);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", n1._id, n2._id);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", n2._id, n3._id);
        adapter.setNodeLimit(100);
        adapter.expandCommunity(getCommunityNodes()[0]);
        expect(mockWrapper.call).wasCalledWith("insertEdge", n1._id, n2._id);
        expect(mockWrapper.call).wasCalledWith("insertEdge", n2._id, n3._id);
      });
      
      it('should be informed to remove edges if nodes are added to a bucket', function() {
        var centroid = {
          _id: "center"
        },
        s1 = {
          _id: "s_1"
        },
        s2 = {
          _id: "s_2"
        },
        s3 = {
          _id: "s_3"
        },
        s4 = {
          _id: "s_4"
        },
        s5 = {
          _id: "s_5"
        },
        s6 = {
          _id: "s_6"
        },
        s7 = {
          _id: "s_7"
        },
        s8 = {
          _id: "s_8"
        },
        ec1 = {
          _id: "c1",
          _from: "center",
          _to: "s_1"
        },
        ec2 = {
          _id: "c2",
          _from: "center",
          _to: "s_2"
        },
        ec3 = {
          _id: "c3",
          _from: "center",
          _to: "s_3"
        },
        ec4 = {
          _id: "c4",
          _from: "center",
          _to: "s_4"
        },
        ec5 = {
          _id: "c5",
          _from: "center",
          _to: "s_5"
        },
        ec6 = {
          _id: "c6",
          _from: "center",
          _to: "s_6"
        },
        ec7 = {
          _id: "c7",
          _from: "center",
          _to: "s_7"
        },
        ec8 = {
          _id: "c8",
          _from: "center",
          _to: "s_8"
        },
        inserted = [s1, s2, s3, s4, s5, s6, s7, s8],
        limit = 3;
        spyOn(mockReducer, "bucketNodes").andCallFake(function() {
          return [
            {
              reason: {
                type: "similar",
                example: s1
              },
              nodes: [s1, s2]
            },
            {
              reason: {
                type: "similar",
                example: s3
              },
              nodes: [s3, s4, s5, s6, s7]
            },
            {
              reason: {
                type: "similar",
                example: s8
              },
              nodes: [s8]
            }
          ];
        });
        adapter.setChildLimit(limit);
        
        adapter.insertNode(centroid);
        adapter.insertNode(s1);
        adapter.insertNode(s2);
        adapter.insertNode(s3);
        adapter.insertNode(s4);
        adapter.insertNode(s5);
        adapter.insertNode(s6);
        adapter.insertNode(s7);
        adapter.insertNode(s8);
        
        adapter.insertEdge(ec1);
        adapter.insertEdge(ec2);
        adapter.insertEdge(ec3);
        adapter.insertEdge(ec4);
        adapter.insertEdge(ec5);
        adapter.insertEdge(ec6);
        adapter.insertEdge(ec7);
        adapter.insertEdge(ec8);
        
        spyOn(mockWrapper, "call");
        
        adapter.checkSizeOfInserted(inserted);
        expect(mockReducer.bucketNodes).wasCalledWith(inserted, limit);
        
        expect(mockWrapper.call).toHaveBeenCalledWith("deleteEdge", "center", "s_1");
        expect(mockWrapper.call).toHaveBeenCalledWith("deleteEdge", "center", "s_2");
        expect(mockWrapper.call).toHaveBeenCalledWith("deleteEdge", "center", "s_3");
        expect(mockWrapper.call).toHaveBeenCalledWith("deleteEdge", "center", "s_4");
        expect(mockWrapper.call).toHaveBeenCalledWith("deleteEdge", "center", "s_5");
        expect(mockWrapper.call).toHaveBeenCalledWith("deleteEdge", "center", "s_6");
        expect(mockWrapper.call).toHaveBeenCalledWith("deleteEdge", "center", "s_7");
        expect(mockWrapper.call.calls.length).toEqual(7);

      });
      
      it('should not be informed about edges targeting at communities', function() {
        var centroid = {
          _id: "center"
        },
        s1 = {
          _id: "s_1"
        },
        s2 = {
          _id: "s_2"
        },
        ec1 = {
          _id: "c1",
          _from: "center",
          _to: "s_1"
        },
        ec2 = {
          _id: "c2",
          _from: "center",
          _to: "s_2"
        },
        comSource = {
          _id: "comsource",
          _from: "s_1",
          _to: "1"
        },
        comTarget = {
          _id: "comtarget",
          _from: "1",
          _to: "s_1"
        },
        inserted = [s1, s2],
        limit = 1;
        spyOn(mockReducer, "bucketNodes").andCallFake(function() {
          return [
            {
              reason: {
                type: "similar",
                example: s1
              },
              nodes: [s1, s2]
            }
          ];
        });
        adapter.setChildLimit(limit);
        
        adapter.insertNode(centroid);
        adapter.insertNode(s1);
        adapter.insertNode(s2);

        
        adapter.insertEdge(ec1);
        adapter.insertEdge(ec2);
        adapter.checkSizeOfInserted(inserted);
        expect(mockReducer.bucketNodes).wasCalledWith(inserted, limit);
        spyOn(mockWrapper, "call");
        
        adapter.insertEdge(comSource);
        adapter.insertEdge(comTarget);
        
        expect(mockWrapper.call).wasNotCalled();
      });
      
      it('should never be informed about edges concerning communities', function() {
        var fakeData, n1, n2, n3, n4, e1, e2, e3, comm;
        
        adapter.removeNode(source);
        adapter.removeNode(target);
        
        expect(nodes.length).toEqual(0);
        
        
        fakeData = [1, 2];
        spyOn(mockWrapper, "call").andCallFake(function(name) {
          if (name === "getCommunity") {
            workerCB({
              data: {
                cmd: name,
                result: fakeData
              }
            });
          }
        });
        
        n1 = adapter.insertNode({_id: 1});
        n2 = adapter.insertNode({_id: 2});
        n3 = adapter.insertNode({_id: 3});
        n4 = adapter.insertNode({_id: 4});
        e1 = adapter.insertEdge({
          _id: "1-2",
          _from: 1,
          _to: 2        
        });
        e2 = adapter.insertEdge({
          _id: "2-3",
          _from: 2,
          _to: 3        
        });
        e3 = adapter.insertEdge({
          _id: "3-1",
          _from: 3,
          _to: 1        
        });
        
        expect(mockWrapper.call).wasCalledWith("insertEdge", 1, 2);
        expect(mockWrapper.call).wasCalledWith("insertEdge", 2, 3);
        expect(mockWrapper.call).wasCalledWith("insertEdge", 3, 1);
        expect(mockWrapper.call.calls.length).toEqual(3);
        
        adapter.setNodeLimit(3);
        comm = getCommunityNodes()[0];
        
        expect(mockWrapper.call).wasCalledWith("getCommunity", 3);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", 1, 2);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", 2, 3);
        expect(mockWrapper.call).wasCalledWith("deleteEdge", 3, 1);
        expect(mockWrapper.call.calls.length).toEqual(7);
        
        
        fakeData = [3, 4];
        adapter.setNodeLimit(2);
        expect(mockWrapper.call).wasCalledWith("getCommunity", 2);
        expect(mockWrapper.call.calls.length).toEqual(8);
      });
      
    });
    
    describe('checking many child nodes', function() {
      
      var adapter, mockReducer;
      
      beforeEach(function() {
        mockReducer = {};
        mockReducer.getPrioList = function() {};
        mockReducer.changePrioList = function() {};
        mockReducer.bucketNodes = function() {};
        spyOn(window, "NodeReducer").andCallFake(function(v, e) {
          return {
            bucketNodes: function(toSort, numBuckets) {
              return mockReducer.bucketNodes(toSort, numBuckets);
            },
            changePrioList: function(list) {
              return mockReducer.changePrioList(list);
            },
            getPrioList: function() {
              return mockReducer.getPrioList();
            }
          };
        });
        adapter = new AbstractAdapter(nodes, edges, descendant);
      });
      
      it('should be able to change the reducer to a new prioList', function() {
        spyOn(mockReducer,"changePrioList");
        var list = ["a", "b", "c"],
          config = {
            prioList: list
          };
        adapter.changeTo(config);
        expect(mockReducer.changePrioList).wasCalledWith(list);
      });
      
      it('should be able to return the current prioList', function() {
        spyOn(mockReducer,"getPrioList");
        adapter.getPrioList();
        expect(mockReducer.getPrioList).wasCalled();
      });
      
      it('should not take any action if the limit is high enough', function() {
        adapter.setChildLimit(5);
        spyOn(mockReducer, "bucketNodes");
        
        
        var n1, n2, n3, n4, n5,
        inserted = {};
        n1 = adapter.insertNode({_id: 1 });
        n2 = adapter.insertNode({_id: 2 });
        n3 = adapter.insertNode({_id: 3 });
        n4 = adapter.insertNode({_id: 4 });
        n5 = adapter.insertNode({_id: 5 });
        _.each(nodes, function(n) {
          inserted[n._id] = n;
        });
        adapter.checkSizeOfInserted(inserted);
        expect(mockReducer.bucketNodes).wasNotCalled();
      });
      
      it('should bucket nodes if limit is to small', function() {
        var n1, n2, n3, n4, n5,
        inserted = [],
        limit = 2;
        spyOn(mockReducer, "bucketNodes").andCallFake(function() {
          return [
            {
              reason: {
                type: "similar",
                example: n1
              },
              nodes: [n1, n2]
            },
            {
              reason: {
                type: "similar",
                example: n3
              },
              nodes: [n3, n4, n5]
            }
          ];
        });
        adapter.setChildLimit(limit);
        n1 = adapter.insertNode({_id: "1" });
        n2 = adapter.insertNode({_id: "2" });
        n3 = adapter.insertNode({_id: "3" });
        n4 = adapter.insertNode({_id: "4" });
        n5 = adapter.insertNode({_id: "5" });
        _.each(nodes, function(n) {
          inserted.push(n);
        });
        adapter.checkSizeOfInserted(inserted);
        
        expect(mockReducer.bucketNodes).wasCalledWith(inserted, limit);
        
        expect(nodes.length).toEqual(2);
        expect(getCommunityNodes().length).toEqual(2);
        notExistNodes(["1", "2", "3", "4", "5"]);
      });
      
      it('should not display single nodes as buckets', function() {
        var n1, n2, n3, n4, n5,
        inserted = [],
        limit = 3;
        spyOn(mockReducer, "bucketNodes").andCallFake(function() {
          return [
            {
              reason: {
                type: "similar",
                example: n1
              },
              nodes: [n1]
            },
            {
              reason: {
                type: "similar",
                example: n3
              },
              nodes: [n3, n4, n5]
            },
            {
              reason: {
                type: "similar",
                example: n2
              },
              nodes: [n2]
            }
          ];
        });
        adapter.setChildLimit(limit);
        n1 = adapter.insertNode({_id: "1" });
        n2 = adapter.insertNode({_id: "2" });
        n3 = adapter.insertNode({_id: "3" });
        n4 = adapter.insertNode({_id: "4" });
        n5 = adapter.insertNode({_id: "5" });
        _.each(nodes, function(n) {
          inserted.push(n);
        });
        adapter.checkSizeOfInserted(inserted);
        
        expect(mockReducer.bucketNodes).wasCalledWith(inserted, limit);
        
        expect(nodes.length).toEqual(3);
        expect(getCommunityNodes().length).toEqual(1);
        notExistNodes(["3", "4", "5"]);
        existNodes(["1", "2"]);
      });
      
      it('should display the reason why a community has been joined', function() {
        var n1, n2, n3, n4, n5,
        com1, com2,
        inserted = [],
        limit = 3;
        spyOn(mockReducer, "bucketNodes").andCallFake(function() {
          return [
            {
              reason: {
                type: "similar",
                example: n1
              },
              nodes: [n1, n2]
            },
            {
              reason: {
                key: "type",
                value: "example"
              },
              nodes: [n3, n4, n5]
            }
          ];
        });
        adapter.setChildLimit(limit);
        n1 = adapter.insertNode({_id: "1" });
        n2 = adapter.insertNode({_id: "2" });
        n3 = adapter.insertNode({_id: "3" });
        n4 = adapter.insertNode({_id: "4" });
        n5 = adapter.insertNode({_id: "5" });
        _.each(nodes, function(n) {
          inserted.push(n);
        });
        adapter.checkSizeOfInserted(inserted);
        
        expect(mockReducer.bucketNodes).wasCalledWith(inserted, limit);
        
        expect(nodes.length).toEqual(2);
        expect(getCommunityNodes().length).toEqual(2);
        notExistNodes(["1", "2", "3", "4", "5"]);
        
        _.each(getCommunityNodes(), function(c) {
          if (c._size === 2) {
            com1 = c;
            return;
          }
          if (c._size === 3) {
            com2 = c;
            return;
          }
          // Should never be reached
          expect(true).toBeFalsy();
        });
        
        expect(com1._reason).toEqual({
          type: "similar",
          example: n1
        });
        expect(com2._reason).toEqual({
          key: "type",
          value: "example"
        });
      });
    });
    
  });
}());
