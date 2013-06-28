/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine */
/*global runs, spyOn, waitsFor, waits */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global describeInterface, describeIntegeration*/
/*global PreviewAdapter*/

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

  describe('Preview Adapter', function () {
    
    describeInterface(new PreviewAdapter([], []));
    /*
    describeIntegeration(function() {
      spyOn($, "ajax").andCallFake(function(req) {
        var node1 = {_id: 1},
          node2 = {_id: 2},
          edge = {_id: "1-2", _from: 1, _to: 2};
          
        switch(req.type) {
          case "DELETE":
            req.success();
            break;
          case "POST":
            if (req.url.match(/nodes$/)) {
              req.success({_id: 1});
            } else if (req.url.match(/edges/)) {
              req.success({_id: "1-2"});
            }
            break;
          case "GET":
            req.success({
              first: {_id: 1},
              nodes: {
                "1": {_id: 1},
                "2": {_id: 2}
              },
              edges: {
                "1-2": {_id: "1-2", _from: 1, _to: 2}
              }
            });
            break;
          default:
            req.success();
        }
      });
      
      return new FoxxAdapter([], [], "foxx/route");
    });
    */
    
    var adapter,
      nodes,
      edges;
    
      beforeEach(function() {
        nodes = [];
        edges = [];
      });
            
      it('should throw an error if no nodes are given', function() {
        expect(
          function() {
            var t = new PreviewAdapter();
          }
        ).toThrow("The nodes have to be given.");
      });
      
      it('should throw an error if no edges are given', function() {
        expect(
          function() {
            var t = new PreviewAdapter([]);
          }
        ).toThrow("The edges have to be given.");
      });
      
      it('should not throw an error if necessary info is given', function() {
        expect(
          function() {
            var t = new PreviewAdapter([], []);
          }
        ).not.toThrow();
      }); 

      it('should create a nodeReducer instance', function() {
        spyOn(window, "NodeReducer");
        var adapter = new PreviewAdapter(
          nodes,
          edges
        );
        expect(window.NodeReducer).wasCalledWith(nodes, edges);
      });
    
      describe('setup correctly', function() {
        
        beforeEach(function() {  
          var self = this;
          self.fakeReducerRequest = function() {};
          self.fakeReducerBucketRequest = function() {};
          spyOn(window, "NodeReducer").andCallFake(function(v, e) {
            return {
              getCommunity: function(limit, focus) {
                if (focus !== undefined) {
                  return self.fakeReducerRequest(limit, focus);
                }
                return self.fakeReducerRequest(limit);
              },
              bucketNodes: function(toSort, numBuckets) {
                return self.fakeReducerBucketRequest(toSort, numBuckets);
              }
            };
          });
          adapter = new PreviewAdapter(
            nodes,
            edges
          );
        });
        
        it('should be load a graph of 5 nodes and 5 edges', function() {
          
          var called, id;
          
          runs(function() {
            called = false;
            id = 1;
            var callback = function() {
              called = true;
            };
            adapter.loadNode(id, callback);
          });
          
          waitsFor(function() {
            return called;
          }, 1000);
          
          runs(function() {
            expect(nodes.length).toEqual(5);
            expect(edges.length).toEqual(5);
          });
        });

        it('nodes should offer a label', function() {
          
          var called, id;
          
          runs(function() {
            called = false;
            id = 1;
            var callback = function() {
              called = true;
            };
            adapter.loadNode(id, callback);
          });
          
          waitsFor(function() {
            return called;
          }, 1000);
          
          runs(function() {
            _.each(nodes, function(n) {
              expect(n._data.label).toBeDefined();
            });
          });
        });
        
        it('node one should offer an image', function() {
          
          var called, id;
          
          runs(function() {
            called = false;
            id = 1;
            var callback = function() {
              called = true;
            };
            adapter.loadNode(id, callback);
          });
          
          waitsFor(function() {
            return called;
          }, 1000);
          
          runs(function() {
            _.each(nodes, function(n) {
              if (n._id === 1) {
                expect(n._data.image).toBeDefined();
                expect(n._data.image).toEqual("img/stored.png");
              } else {
                expect(n._data.image).toBeUndefined();
              }
            });
          });
        });
        
        it('edges should offer a label', function() {
          
          var called, id;
          
          runs(function() {
            called = false;
            id = 1;
            var callback = function() {
              called = true;
            };
            adapter.loadNode(id, callback);
          });
          
          waitsFor(function() {
            return called;
          }, 1000);
          
          runs(function() {
            _.each(edges, function(e) {
              expect(e._data.label).toBeDefined();
            });
          });
        });

        it('should alert insertion of a node', function() {
          spyOn(window, "alert");
          var node = {_id: 1};
          adapter.createNode(node);
          expect(window.alert).wasCalledWith("Server-side: createNode was triggered.");
        });
        
        it('should alert change of a node', function() {
          spyOn(window, "alert");
          var toPatch = {_id: 1},
            data = {name: "Alice"};
          adapter.patchNode(toPatch, data);
          expect(window.alert).wasCalledWith("Server-side: patchNode was triggered.");
        });
        
        it('should alert deletion of a node', function() {
          spyOn(window, "alert");
          var node = {_id: 1};
          adapter.deleteNode(node);
          expect(window.alert).wasCalledWith("Server-side: deleteNode was triggered.");
          expect(window.alert).wasCalledWith("Server-side: onNodeDelete was triggered.");
        });
        
        it('should be able to insert an edge', function() {
          spyOn(window, "alert");
          var source = {_id: 1},
            target = {_id: 2},
            edge = {
              source: source,
              target: target,
              label: "Foxx"
            };
          adapter.createEdge(edge);
          expect(window.alert).wasCalledWith("Server-side: createEdge was triggered.");
        });
        
        it('should be able to change an edge', function() {
          spyOn(window, "alert");
          var source = {_id: 1},
            target = {_id: 2},
            edge = {
              _id: "1-2",
              source: source,
              target: target
            },
            patch = {
              label: "Foxx"
            };
          adapter.patchEdge(edge, patch);
          expect(window.alert).wasCalledWith("Server-side: patchEdge was triggered.");
        });
        
        it('should be able to delete an edge', function() {
          spyOn(window, "alert");
          var source = {_id: 1},
            target = {_id: 2},
            edge = {
              _id: "1-2",
              source: source,
              target: target
            };
          adapter.deleteEdge(edge);
          expect(window.alert).wasCalledWith("Server-side: deleteEdge was triggered.");
        });
                        
      });
      
  });
  
}());