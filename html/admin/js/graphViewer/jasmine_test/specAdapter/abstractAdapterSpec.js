/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine */
/*global runs, spyOn, waitsFor, waits */
/*global window, eb, loadFixtures, document */
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
    
    getCommunityNodes = function() {
      return _.filter(nodes, function(n) {
        return String(n._id).match(/^\*community/);
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
      
      it('should not throw an error if setup correctly', function() {
        expect(
          function() {
            var t = new AbstractAdapter([], []);
          }
        ).not.toThrow();
      });
      
      it('should create a NodeReducer instance', function() {
        spyOn(window, "NodeReducer");
        var nodes = [],
          edges = [],
          t = new AbstractAdapter(nodes, edges);
        expect(window.NodeReducer).wasCalledWith(nodes, edges);
      });
      
      it('should create a ModularityJoiner worker', function() {
        spyOn(window, "WebWorkerWrapper");
        var nodes = [],
          edges = [],
          t = new AbstractAdapter(nodes, edges);
        expect(window.WebWorkerWrapper).wasCalledWith(
          window.ModularityJoiner,
          jasmine.any(Function)
        );
      });
      
    });
    
    describe('checking the interface', function() {
      var testee;
      
      beforeEach(function() {
        testee = new AbstractAdapter([], []);
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
    });
    
    describe('checking nodes', function() {
      
      var adapter;
      
      beforeEach(function() {
        adapter = new AbstractAdapter(nodes, []);
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
        adapter = new AbstractAdapter(nodes, edges);
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
        adapter = new AbstractAdapter(nodes, edges);
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
          existNodes([1,2,3,4]);
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
        
      });
      
    });
    
    describe('checking information of modularity joiner', function() {
      
      var adapter,
      source,
      target,
      sourceid,
      targetid,
      mockWrapper,
      workerCB;
      
      beforeEach(function() {
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
        adapter = new AbstractAdapter(nodes, edges);
        source = adapter.insertNode({_id: 1});
        target = adapter.insertNode({_id: 2});
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
          e1, e2, e3, e4;
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
        adapter.insertNode(n1);
        adapter.insertNode(n2);
        adapter.insertNode(n3);
        adapter.insertNode(n4);
      
        adapter.insertEdge(e1);
        adapter.insertEdge(e2);
        adapter.insertEdge(e3);
        adapter.insertEdge(e4);
      
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
      
    });
    
    
    
    describe('checking many child nodes', function() {
      
      var adapter, mockReducer;
      
      beforeEach(function() {
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
        adapter = new AbstractAdapter(nodes, edges);
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
            [n1, n2],
            [n3, n4, n5]
          ];
        });
        adapter.setChildLimit(limit);
        n1 = adapter.insertNode({_id: 1 });
        n2 = adapter.insertNode({_id: 2 });
        n3 = adapter.insertNode({_id: 3 });
        n4 = adapter.insertNode({_id: 4 });
        n5 = adapter.insertNode({_id: 5 });
        _.each(nodes, function(n) {
          inserted.push(n);
        });
        adapter.checkSizeOfInserted(inserted);
        
        expect(mockReducer.bucketNodes).wasCalledWith(inserted, limit);
        
        expect(nodes.length).toEqual(2);
        expect(getCommunityNodes().length).toEqual(2);
        notExistNodes([1, 2, 3, 4, 5]);
      });
      
      it('should not display single nodes as buckets', function() {
        var n1, n2, n3, n4, n5,
        inserted = [],
        limit = 3;
        spyOn(mockReducer, "bucketNodes").andCallFake(function() {
          return [
            [n1],
            [n3, n4, n5],
            [n2]
          ];
        });
        adapter.setChildLimit(limit);
        n1 = adapter.insertNode({_id: 1 });
        n2 = adapter.insertNode({_id: 2 });
        n3 = adapter.insertNode({_id: 3 });
        n4 = adapter.insertNode({_id: 4 });
        n5 = adapter.insertNode({_id: 5 });
        _.each(nodes, function(n) {
          inserted.push(n);
        });
        adapter.checkSizeOfInserted(inserted);
        
        expect(mockReducer.bucketNodes).wasCalledWith(inserted, limit);
        
        expect(nodes.length).toEqual(3);
        expect(getCommunityNodes().length).toEqual(1);
        notExistNodes([3, 4, 5]);
        existNodes([1, 2]);
      });
      
    });
    
  });
}());
