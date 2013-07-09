/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine */
/*global runs, spyOn, waitsFor, waits */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global describeInterface, describeIntegeration*/
/*global FoxxAdapter*/

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

  describe('Foxx Adapter', function () {
    
    describeInterface(new FoxxAdapter([], [], "foxx/route"));
    
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
            var t = new FoxxAdapter();
          }
        ).toThrow("The nodes have to be given.");
      });
      
      it('should throw an error if no edges are given', function() {
        expect(
          function() {
            var t = new FoxxAdapter([]);
          }
        ).toThrow("The edges have to be given.");
      });
      
      it('should throw an error if no route is given', function() {
        expect(
          function() {
            var t = new FoxxAdapter([], []);
          }
        ).toThrow("The route has to be given.");
      }); 
      
      it('should not throw an error if necessary info is given', function() {
        expect(
          function() {
            var t = new FoxxAdapter([], [], "foxx/route");
          }
        ).not.toThrow();
      }); 
      /*    
      it('should automatically determine the host of relative route is given', function() {
        var route = "foxx/route",
          args,
          host;
        adapter = new FoxxAdapter(
          nodes,
          edges,
          route
        );
        spyOn($, "ajax");
        adapter.createNode({}, function() {});
        args = $.ajax.mostRecentCall.args[0];
        host = window.location.protocol + "//" + window.location.host + "/" + route;
        expect(args.url).toContain(host);
      });
      */
      it('should create a nodeReducer instance', function() {
        spyOn(window, "NodeReducer");
        var adapter = new FoxxAdapter(
          nodes,
          edges,
          "foxx/route"
        );
        expect(window.NodeReducer).wasCalledWith(nodes, edges);
      });
    
      describe('setup correctly', function() {
        
        var edgeRoute,
          nodeRoute,
          queryRoute,
          loadGraph,
          requests;
        
        beforeEach(function() {  
          var self = this,
            route = "foxx/route",
            /*host = window.location.protocol + "//"
              + window.location.host + "/"
              + route;*/
            host = route;
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
          adapter = new FoxxAdapter(
            nodes,
            edges,
            route
          );
          edgeRoute = host + "/edges";
          nodeRoute = host + "/nodes";
          queryRoute = host + "/query";      
          
          requests = {};
          requests.query = function(id) {
            return {
              type: 'GET',
              cache: false,
              url: queryRoute + "/" + id,
              contentType: 'application/json',
              dataType: 'json',
              success: jasmine.any(Function),
              error: jasmine.any(Function),
              processData: false
            };
          };
          requests.node = function() {
            var base = {
              cache: false,
              dataType: "json",
              contentType: "application/json",
              processData: false,
              success: jasmine.any(Function),
              error: jasmine.any(Function)
            };
            return {
              create: function(data) {
                return $.extend(base, {url: nodeRoute, type: "POST", data: JSON.stringify(data)});
              },
              patch: function(id, data) {
                return $.extend(base, {
                  url: nodeRoute + "/" + id,
                  type: "PUT",
                  data: JSON.stringify(data)
                });
              },
              del: function(id) {
                return $.extend(base, {url: nodeRoute + "/" + id, type: "DELETE"});
              }
            };
          };
          requests.edge = function() {
            var base = {
              cache: false,
              dataType: "json",
              contentType: "application/json",
              processData: false,
              success: jasmine.any(Function),
              error: jasmine.any(Function)
            };
            return {
              create: function(from, to, data) {
                data = $.extend(data, {_from: from, _to: to});
                return $.extend(base, {
                  url: edgeRoute,
                  type: "POST",
                  data: JSON.stringify(data)
                });
              },
              patch: function(id, data) {
                return $.extend(base, {
                  url: edgeRoute + "/" + id,
                  type: "PUT",
                  data: JSON.stringify(data)
                });
              },
              del: function(id) {
                return $.extend(base, {url: edgeRoute + "/" + id, type: "DELETE"});
              },
              delForNode: function(id) {
                return $.extend(base, {url: edgeRoute + "/forNode/" + id, type: "DELETE"});
              }
            };
          };
        });
        
        it('should be able to load a graph', function() {
          
          var called, id;
          
          runs(function() {
            called = false;
            id = 1;
            spyOn($, "ajax").andCallFake(function(req) {
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
            });
            var callback = function() {
              called = true;
            };
            adapter.loadNode(id, callback);
          });
          
          waitsFor(function() {
            return called;
          }, 1000);
          
          runs(function() {
            expect(nodes.length).toEqual(2);
            expect(edges.length).toEqual(1);
            expect($.ajax).toHaveBeenCalledWith(
              requests.query(id)
            );
          });
        });

        it('should be able to insert a node', function() {
          spyOn($, "ajax");
          var node = {_id: 1};
          adapter.createNode(node);
          expect($.ajax).wasCalledWith(requests.node().create(node));
        });
        
        it('should be able to change a node', function() {
          spyOn($, "ajax");
          var toPatch = {_id: 1},
            data = {name: "Alice"};
          adapter.patchNode(toPatch, data);
          expect($.ajax).wasCalledWith(requests.node().patch(toPatch._id, data));
        });
        
        it('should be able to delete a node', function() {
          //Checks first ajax Call and omits propagation
          spyOn($, "ajax");
          var node = {_id: 1};
          adapter.deleteNode(node);
          expect($.ajax).wasCalledWith(requests.node().del(node._id));
        });
        
        it('should delete adjacent edges on node delete', function() {
          // Checks the second (and last) ajax Call.
          spyOn($, "ajax").andCallFake(function(req) {
            req.success();
          });
          var node = {_id: 1};
          adapter.deleteNode(node);
          expect($.ajax).wasCalledWith(requests.edge().delForNode(node._id));
        });
        
        it('should be able to insert an edge', function() {
          spyOn($, "ajax");
          var source = {_id: 1},
            target = {_id: 2},
            edge = {
              source: source,
              target: target,
              label: "Foxx"
            };
          adapter.createEdge(edge);
          expect($.ajax).wasCalledWith(requests.edge().create(
            source._id, target._id, {label: "Foxx"})
          );
        });
        
        it('should be able to change an edge', function() {
          spyOn($, "ajax");
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
          expect($.ajax).wasCalledWith(requests.edge().patch(edge._id, patch));
        });
        
        it('should be able to delete an edge', function() {
          spyOn($, "ajax");
          var source = {_id: 1},
            target = {_id: 2},
            edge = {
              _id: "1-2",
              source: source,
              target: target
            };
          adapter.deleteEdge(edge);
          expect($.ajax).wasCalledWith(requests.edge().del(edge._id));
        });
        

        it('should not bucket existing nodes', function() {
          var lastCallWith, n0, n1, n2, n3, n4, n5, n6, isFirstCall,
          callbackCheck, checkCallbackFunction;
          
          runs(function() {
            checkCallbackFunction = function() {
              callbackCheck = true;
            };
            callbackCheck = false;
            isFirstCall = true;
            spyOn($, "ajax").andCallFake(function(req) {
              if (isFirstCall) {
                isFirstCall = false;
                req.success({
                  first: {_id: "0"},
                  nodes: {
                    "0": {_id: "0"},
                    "1": {_id: "1"},
                    "2": {_id: "2"},
                    "3": {_id: "3"}
                  },
                  edges: {
                    "0-1": {_id: "0-1", _from: "0", _to: "1"},
                    "0-2": {_id: "0-2", _from: "0", _to: "2"},
                    "0-3": {_id: "0-3", _from: "0", _to: "3"}
                  }
                });
              } else {
                req.success({
                  first: {_id: "1"},
                  nodes: {
                    "0": {_id: "0"},
                    "1": {_id: "1"},
                    "2": {_id: "2"},
                    "4": {_id: "4"},
                    "5": {_id: "5"},
                    "6": {_id: "6"}
                  },
                  edges: {
                    "1-0": {_id: "1-0", _from: "1", _to: "0"},
                    "1-2": {_id: "1-2", _from: "1", _to: "2"},
                    "1-4": {_id: "1-4", _from: "1", _to: "4"},
                    "1-5": {_id: "1-5", _from: "1", _to: "5"},
                    "1-6": {_id: "1-6", _from: "1", _to: "6"}
                  }
                });
              }
            });
            adapter.setChildLimit(2);

            spyOn(this, "fakeReducerBucketRequest").andCallFake(function(ns) {
              lastCallWith = _.pluck(ns, "_id");
              return [[ns[0]], [ns[1], ns[2]]];
            });
            
            callbackCheck = false;
            adapter.loadNode(n0, checkCallbackFunction);
            
          });
          
          waitsFor(function() {
            return callbackCheck;
          }, 1000);
          
          runs(function() {
            expect(lastCallWith).toEqual(["1", "2", "3"]);
            
            callbackCheck = false;
            adapter.loadNode(n1, checkCallbackFunction);
          });
          
          waitsFor(function() {
            return callbackCheck;
          }, 1000);
          
          runs(function() {
            expect(lastCallWith).toEqual(["4", "5", "6"]);
            
          });
          
        });

        describe('that has already loaded a graph', function() {
          
        });
        
        
        /*
        describe('that has already loaded one graph', function() {
          var c0, c1, c2, c3, c4, c5, c6, c7,
            fakeResult, spyHook;
      
      
          beforeEach(function() {
      
            runs(function() {
              spyOn($, "ajax").andCallFake(function(request) {
                if (spyHook !== undefined) {
                  if(!spyHook(request)) {
                    return;
                  }
                }
                if (request.url.indexOf("cursor", request.url.length - "cursor".length) !== -1) {
                  var vars = JSON.parse(request.data).bindVars;
                  if (vars !== undefined) {
                    request.success({result: loadGraph(vars)});
                  }
                } else {
                  request.success(fakeResult);
                }
                
              });
              c0 = insertNode(nodesCollection, 0);
              c1 = insertNode(nodesCollection, 1);
              c2 = insertNode(nodesCollection, 2);
              c3 = insertNode(nodesCollection, 3);
              c4 = insertNode(nodesCollection, 4);
              c5 = insertNode(nodesCollection, 5);
              c6 = insertNode(nodesCollection, 6);
              c7 = insertNode(nodesCollection, 7);
          
              insertEdge(edgesCollection, c0, c1);
              insertEdge(edgesCollection, c0, c2);
              insertEdge(edgesCollection, c0, c3);
              insertEdge(edgesCollection, c0, c4);
              insertEdge(edgesCollection, c1, c5);
              insertEdge(edgesCollection, c1, c6);
              insertEdge(edgesCollection, c1, c7);
          
              callbackCheck = false;
              adapter.loadNode(c0, checkCallbackFunction);
          
              this.addMatchers({
                toBeStoredPermanently: function() {
                  var id = this.actual,
                  res = false;
                  $.ajax({
                    type: "GET",
                    url: arangodb + "/_api/document/" + id,
                    contentType: "application/json",
                    processData: false,
                    async: false,
                    success: function(data) {
                      res = true;
                    },
                    error: function(data) {
                      try {
                        var temp = JSON.parse(data);
                        throw "[" + temp.errorNum + "] " + temp.errorMessage;
                      }
                      catch (e) {
                        throw "Undefined ERROR";
                      }
                    }
                  });
                  return res;
                },
            
                toNotBeStoredPermanently: function() {
                  var id = this.actual,
                  res = false;
                  $.ajax({
                    type: "GET",
                    url: arangodb + "/_api/document/" + id,
                    contentType: "application/json",
                    processData: false,
                    async: false,
                    success: function(data) {

                    },
                    error: function(data) {
                      if (data.status === 404) {
                        res = true;
                      }
                  
                    }
                  });
                  return res;
                },
            
                toHavePermanentAttributeWithValue: function(attribute, value) {
                  var id = this.actual,
                  res = false;
                  $.ajax({
                    type: "GET",
                    url: arangodb + "/_api/document/" + id,
                    contentType: "application/json",
                    processData: false,
                    async: false,
                    success: function(data) {
                      if (data[attribute] === value) {
                        res = true;
                      }
                    },
                    error: function(data) {                  
                    }
                  });
                  return res;
                }
              });
            });
      
            waitsFor(function() {
              return callbackCheck;
            });
        
            runs(function() {
              callbackCheck = false;
            });     
          });
          
          it('should be able to add nodes from another query', function() {
        
            runs(function() {
              adapter.loadNode(c1, checkCallbackFunction);
            });
      
            waitsFor(function() {
              return callbackCheck;
            });
      
            runs(function() {
              existNodes([c0, c1, c2, c3, c4, c5, c6, c7]);
              expect(nodes.length).toEqual(8);
              expect($.ajax).toHaveBeenCalledWith(
                requests.cursor(traversalQuery(c1, nodesCollection, edgesCollection))
              );
            });
          });
          
          it('should be able to change a value of one node permanently', function() {
            var toPatch;
        
            runs(function() {
              fakeResult = {hello: "world"};
              toPatch = nodeWithID(c0);
              adapter.patchNode(toPatch, {hello: "world"}, checkCallbackFunction);
            });
        
            waitsFor(function() {
              return callbackCheck;
            });
        
            runs(function() {
              expect(toPatch._data.hello).toEqual("world");
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(nodesCollection).patch(c0, fakeResult)
              );
              
            });
        
          });
          
          it('should be able to change a value of one edge permanently', function() {
            var toPatch;
        
            runs(function() {
              fakeResult = {hello: "world"};
              toPatch = edgeWithSourceAndTargetId(c0, c1);
              adapter.patchEdge(toPatch, {hello: "world"}, checkCallbackFunction);
            });
        
            waitsFor(function() {
              return callbackCheck;
            });
        
            runs(function() {
              expect(toPatch._data.hello).toEqual("world");
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(edgesCollection).patch(toPatch._id, fakeResult)
              );
            });
          });
          
          it('should be able to remove an edge permanently', function() {
        
            var toDelete;
        
            runs(function() {
              fakeResult = "";
              toDelete = edgeWithSourceAndTargetId(c0, c4);
              existEdge(c0, c4);
              adapter.deleteEdge(toDelete, checkCallbackFunction);
            });
        
            waitsFor(function() {
              return callbackCheck;
            });
        
            runs(function() {
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(edgesCollection).del(toDelete._id)
              );
              notExistEdge(c0, c4);
            });
        
          });
          
          it('should be able to add a node permanently', function() {
        
            var insertedId;
        
            runs(function() {
              adapter.createNode({}, function(node) {
                insertedId = node._id;
                callbackCheck = true;
              });
            });
        
            waitsFor(function() {
              return callbackCheck;
            });
        
            runs(function() {
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(nodesCollection).create({})
              );
              
              existNode(insertedId);
            });
          });
          
          it('should trigger the reducer if too many nodes are added', function() {
        
            runs(function() {
              adapter.setNodeLimit(6);
              spyOn(this, "fakeReducerRequest").andCallFake(function() {
                return [c0];
              });
              adapter.loadNode(c1, checkCallbackFunction);
              expect(this.fakeReducerRequest).toHaveBeenCalledWith(6, nodeWithID(c1));
            });
          });
          

          describe('checking community nodes', function() {
            
            it('should not trigger the reducer if the limit is set large enough', function() {
              spyOn(this, "fakeReducerRequest").andCallFake(function() {
                return [c0];
              });
              adapter.setNodeLimit(10);
              expect(this.fakeReducerRequest).not.toHaveBeenCalled();
            });
          
            it('should trigger the reducer if the limit is set too small', function() {
              spyOn(this, "fakeReducerRequest").andCallFake(function() {
                return [c0];
              });
              adapter.setNodeLimit(2);
              expect(this.fakeReducerRequest).toHaveBeenCalledWith(2);
            });
            
            it('should create a community node if limit is set too small', function() {
              var called;
              
              runs(function() {
                callbackCheck = false;
                spyOn(this, "fakeReducerRequest").andCallFake(function() {
                  return [c0, c1, c2];
                });
                adapter.setNodeLimit(2, checkCallbackFunction);
              });
              
              waitsFor(function() {
                return callbackCheck;
              });
              
              runs(function() {
                var commId = getCommunityNodesIds()[0];
                notExistNodes([c0, c1, c2]);
                existNode(commId);
                existNodes([c3, c4]);
                expect(nodes.length).toEqual(3);
                existEdge(commId, c3);
                existEdge(commId, c4);
                expect(edges.length).toEqual(2);
              });              
            });
            
            it('should create a community node if too many nodes are added', function() {
              runs(function() {
                adapter.setNodeLimit(6);
                spyOn(this, "fakeReducerRequest").andCallFake(function() {
                  return [c0, c1, c2, c3];
                });
                adapter.loadNode(c1, checkCallbackFunction);
              });
            
              waitsFor(function() {
                return callbackCheck;
              });
      
              runs(function() {
                var commId = getCommunityNodesIds()[0];
                notExistNodes([c0, c1, c2, c3]);
                existNode(commId);
                existNodes([c4, c5, c6, c7]);
                expect(nodes.length).toEqual(5);
              
                existEdge(commId, c4);
                existEdge(commId, c5);
                existEdge(commId, c6);
                existEdge(commId, c7);
                expect(edges.length).toEqual(4);
              });
            
            });
            
            describe('expanding after a while', function() {
              
              it('should connect edges of internal nodes accordingly', function() {
                
                var commNode, called, counterCallback,
                v0, v1, v2, v3, v4,
                e0_1, e0_2, e1_3, e1_4, e2_3, e2_4;
                
                runs(function() {
                  var v = "vertices",
                    e = "edges";
                  nodes.length = 0;
                  edges.length = 0;
                  v0 = insertNode(v, 0);
                  v1 = insertNode(v, 1);
                  v2 = insertNode(v, 2);
                  v3 = insertNode(v, 3);
                  v4 = insertNode(v, 4);
                  e0_1 = insertEdge(e, v0, v1);
                  e0_2 = insertEdge(e, v0, v2);
                  e1_3 = insertEdge(e, v1, v3);
                  e1_4 = insertEdge(e, v1, v4);
                  e2_3 = insertEdge(e, v2, v3);
                  e2_4 = insertEdge(e, v2, v4);
                  called = 0;
                  counterCallback = function() {
                    called++;
                  };
                  spyOn(this, "fakeReducerRequest").andCallFake(function() {
                    return [v1, v3, v4];
                  });
                  adapter.setNodeLimit(3);
                  
                  adapter.changeTo(v, e);
                  adapter.loadNode(v0, counterCallback);
                  adapter.loadNode(v1, counterCallback);
                  
                });
                
                waitsFor(function() {
                  return called === 2;
                });
                
                runs(function() {
                  adapter.loadNode(v2, counterCallback);
                  commNode = getCommunityNodes()[0];
                });
                
                waitsFor(function() {
                  return called === 3;
                });
                
                runs(function() {
                  var commId = commNode._id;
                  // Check start condition
                  existNodes([commId, v0, v2]);
                  expect(nodes.length).toEqual(3);
                  
                  existEdge(v0, v2);
                  existEdge(v0, commId);
                  existEdge(v2, commId);
                  expect(edges.length).toEqual(4);
                  
                  adapter.setNodeLimit(20);
                  adapter.expandCommunity(commNode, counterCallback);
                });
                
                waitsFor(function() {
                  return called === 4;
                });
                
                runs(function() {
                  existNodes([v0, v1, v2, v3, v4]);
                  expect(nodes.length).toEqual(5);
                  
                  existEdge(v0, v1);
                  existEdge(v0, v2);
                  existEdge(v1, v3);
                  existEdge(v1, v4);
                  existEdge(v2, v3);
                  existEdge(v2, v4);
                  expect(edges.length).toEqual(6);
                  
                });
              });
              
              it('set inbound and outboundcounter correctly', function() {
                
                var commNode, called, counterCallback,
                v0, v1, v2, v3, v4,
                e0_1, e0_2, e1_3, e1_4, e2_3, e2_4;
                
                runs(function() {
                  var v = "vertices",
                    e = "edges";
                  nodes.length = 0;
                  edges.length = 0;
                  v0 = insertNode(v, 0);
                  v1 = insertNode(v, 1);
                  v2 = insertNode(v, 2);
                  v3 = insertNode(v, 3);
                  v4 = insertNode(v, 4);
                  e0_1 = insertEdge(e, v0, v1);
                  e0_2 = insertEdge(e, v0, v2);
                  e1_3 = insertEdge(e, v1, v3);
                  e1_4 = insertEdge(e, v1, v4);
                  e2_3 = insertEdge(e, v2, v3);
                  e2_4 = insertEdge(e, v2, v4);
                  called = 0;
                  counterCallback = function() {
                    called++;
                  };
                  spyOn(this, "fakeReducerRequest").andCallFake(function() {
                    return [v1, v3, v4];
                  });
                  adapter.setNodeLimit(3);
                  
                  adapter.changeTo(v, e);
                  adapter.loadNode(v0, counterCallback);
                  adapter.loadNode(v1, counterCallback);
                  
                });
                
                waitsFor(function() {
                  return called === 2;
                });
                
                runs(function() {
                  adapter.loadNode(v2, counterCallback);
                  commNode = getCommunityNodes()[0];
                });
                
                waitsFor(function() {
                  return called === 3;
                });
                
                runs(function() {
                  adapter.setNodeLimit(20);
                  adapter.expandCommunity(commNode, counterCallback);
                });
                
                waitsFor(function() {
                  return called === 4;
                });
                
                runs(function() {
                  var checkNodeWithInAndOut = function(id, inbound, outbound) {
                    var n = nodeWithID(id);
                    expect(n._outboundCounter).toEqual(outbound);
                    expect(n._inboundCounter).toEqual(inbound);
                  };
                  checkNodeWithInAndOut(v0, 0, 2);
                  checkNodeWithInAndOut(v1, 1, 2);
                  checkNodeWithInAndOut(v2, 1, 2);
                  checkNodeWithInAndOut(v3, 2, 0);
                  checkNodeWithInAndOut(v4, 2, 0);            
                });
              });
              
            });
            
            describe('that displays a community node already', function() {
              
              var firstCommId,
              fakeResult;
              
              beforeEach(function() {
                runs(function() {
                  callbackCheck = false;
                  adapter.setNodeLimit(7);
                  fakeResult = [c0, c2];
                  spyOn(this, "fakeReducerRequest").andCallFake(function() {
                    return fakeResult;
                  });
                  adapter.loadNode(c1, checkCallbackFunction);
                });
            
                waitsFor(function() {
                  return callbackCheck;
                });
      
                runs(function() {
                  firstCommId = getCommunityNodesIds()[0];
                });
              });
              
              it('should expand a community if enough space is available', function() {
                runs(function() {
                  adapter.setNodeLimit(10);
                  callbackCheck = false;
                  adapter.expandCommunity(nodeWithID(firstCommId), checkCallbackFunction);
                });
                
                waitsFor(function() {
                  return callbackCheck;
                });
                
                runs(function() {
                  expect(getCommunityNodes().length).toEqual(0);
                  existNodes([c0, c1, c2, c3, c4, c5, c6, c7]);
                  existEdge(c0, c1);
                  existEdge(c0, c2);
                  existEdge(c0, c3);
                  existEdge(c0, c4);
                });
                
              });
              
              it('should expand a community and join another '
              + 'one if not enough space is available', function() {
                runs(function() {
                  fakeResult = [c1, c7];
                  callbackCheck = false;
                  adapter.expandCommunity(nodeWithID(firstCommId), checkCallbackFunction);
                });
                
                waitsFor(function() {
                  return callbackCheck;
                });
                
                runs(function() {
                  var newCommId = getCommunityNodesIds()[0];
                  expect(getCommunityNodes().length).toEqual(1);
                  existNodes([c0, c2, c3, c4, c5, c6, newCommId]);
                  notExistNodes([c1, c7]);
                  
                  existEdge(c0, c2);
                  existEdge(c0, c3);
                  existEdge(c0, c4);
                  
                  existEdge(c0, newCommId);
                  existEdge(newCommId, c5);
                  existEdge(newCommId, c6);
                });
              });
              
              it('should join another community if space is further reduced', function() {
                runs(function() {
                  fakeResult = [c1, c7];
                  callbackCheck = false;
                  adapter.setNodeLimit(6, checkCallbackFunction);
                });
                
                waitsFor(function() {
                  return callbackCheck;
                });
                
                runs(function() {
                  expect(getCommunityNodes().length).toEqual(2);
                  var ids = getCommunityNodesIds(),
                    newCommId;
                  
                  if (firstCommId === ids[0]) {
                    newCommId = ids[1];
                  } else {
                    newCommId = ids[0];
                  }
                  
                  existNodes([c3, c4, c5, c6, firstCommId, newCommId]);
                  notExistNodes([c0, c1, c2, c7]);
                  
                  existEdge(firstCommId, c3);
                  existEdge(firstCommId, c4);
                  existEdge(firstCommId, newCommId);
                  existEdge(newCommId, c5);
                  existEdge(newCommId, c6);
                });
              });
              
              it('should connect edges to internal nodes', function() {
                
                runs(function() {
                  insertEdge(edgesCollection, c3, c0);
                  
                  adapter.setNodeLimit(20);
                  callbackCheck = false;
                  adapter.loadNode(c3, checkCallbackFunction);
                });
                
                waitsFor(function() {
                  return callbackCheck;
                });
                
                runs(function() {
                  existEdge(c3, firstCommId);
                });
                
              });
              
            });
            
          });
          
          describe('that has loaded several queries', function() {
            var c8, c9, e2_8;
      
            beforeEach(function() {
      
              runs(function() {
                c8 = insertNode(nodesCollection, 8);
                c9 = insertNode(nodesCollection, 9);
          
                e2_8 = insertEdge(edgesCollection, c2, c8);
                insertEdge(edgesCollection, c3, c8);
                insertEdge(edgesCollection, c3, c9);
          
                callbackCheck = false;
                adapter.loadNode(c2, checkCallbackFunction);
              });
      
              waitsFor(function() {
                return callbackCheck;
              });
      
              runs(function() {
                callbackCheck = false;
              });
      
            });
            
            it('should not add a node to the list twice', function() {
      
              runs(function() {
                adapter.loadNode(c3, checkCallbackFunction);
              });
      
              waitsFor(function() {
                return callbackCheck;
              });  
        
              runs(function() {
                existNodes([c0, c1, c2, c3, c4, c8, c9]);
                expect(nodes.length).toEqual(7);
              });
            });
            
            it('should be able to add an edge permanently', function() {
              var insertedId,
                source,
                target,
                insertedEdge;
        
        
              runs(function() {
                source = nodeWithID(c0);
                target = nodeWithID(c8);
                fakeResult = {
                  _id: edgesCollection + "/123",
                  _key: "123",
                  _rev: "123",
                  _from: source._id,
                  _to: target._id
                };
                adapter.createEdge({source: source, target: target}, function(edge) {
                  insertedId = edge._id;
                  callbackCheck = true;
                  insertedEdge = edge;
                });
              });
        
              waitsFor(function() {
                return callbackCheck;
              });
        
              runs(function() {
                expect($.ajax).toHaveBeenCalledWith(
                  requests.edge(edgesCollection).create(source._id, target._id, {})
                );
                existEdge(source._id, target._id);
                expect(insertedEdge).toEqual({
                  source: source,
                  target: target,
                  _id: insertedId,
                  _data: {
                    _id: insertedId,
                    _from: source._id,
                    _to: target._id,
                    _rev: jasmine.any(String),
                    _key: jasmine.any(String)
                  }
                });
              });
        
            });
            
            it('should be able to remove a node and all connected edges permanently', function() {
              var toDelete;
              runs(function() {
                spyHook = function(request) {
                  if (request.data !== undefined) {
                    request.success({result: [
                     {_id: e2_8}
                    ]});
                    return false;
                  }
                  return true;
                };
                fakeResult = "";
                toDelete = nodeWithID(c2);
                adapter.deleteNode(toDelete, checkCallbackFunction);
              });
        
              waits(2000);
        
              runs(function() {
                expect($.ajax).toHaveBeenCalledWith(
                  requests.node(nodesCollection).del(toDelete._id)
                );
                notExistNode(c2);
                expect($.ajax).toHaveBeenCalledWith(
                  requests.node(edgesCollection).del(e2_8)
                );
                notExistEdge(c2, c8);
              });
            });
            
          });
          
        });
        */
        
        /*
        describe('displaying only parts of the graph', function() {
    
          it('should be able to remove a node and all '
          + 'connected edges including not visible ones', function() {
            var s0, s1, t0, toDel,
            s0_toDel, s1_toDel, toDel_t0;
        
            runs(function() {
          
              callbackCheck = false;
              s0 = insertNode(nodesCollection, 0);
              s1 = insertNode(nodesCollection, 1);
              t0 = insertNode(nodesCollection, 2);
              toDel = insertNode(nodesCollection, 3);
          
              s0_toDel = insertEdge(edgesCollection, s0, toDel);
              s1_toDel = insertEdge(edgesCollection, s1, toDel);
              toDel_t0 = insertEdge(edgesCollection, toDel, t0);
              
              var loaded = false,
                fakeResult = "";
              
              spyOn($, "ajax").andCallFake(function(request) {                
                if (request.url.indexOf("cursor", request.url.length - "cursor".length) !== -1) {
                  if (!loaded) {
                    var vars = JSON.parse(request.data).bindVars;
                    if (vars !== undefined) {
                      loaded = true;
                      request.success({result: loadGraph(vars)});
                    }
                  } else {
                    request.success({result: [
                     {
                       _id: s0_toDel
                     },{
                       _id: s1_toDel
                     },{
                       _id: toDel_t0
                     }      
                    ]});    
                  }
                } else {
                  request.success(fakeResult);
                }
              });

              adapter.loadNode(s0, checkCallbackFunction);
            });
        
            waitsFor(function() {
              return callbackCheck;
            });
        
            runs(function() {
              callbackCheck = false;
              adapter.deleteNode(nodeWithID(toDel), checkCallbackFunction);
            });
        
            // Wait 2 seconds as no handle for the deletion of edges exists.
            waits(2000);
        
            runs(function() {
              notExistNodes([toDel, s1, t0]);
              existNode(s0);
          
              notExistEdge(s0, toDel);
              notExistEdge(s1, toDel);
              notExistEdge(toDel, t0);
              
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(nodesCollection).del(toDel)
              );
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(edgesCollection).del(s0_toDel)
              );
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(edgesCollection).del(s1_toDel)
              );
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(edgesCollection).del(toDel_t0)
              );
              
              // Check if counter is set correctly
              expect(nodeWithID(s0)._outboundCounter).toEqual(0);
            });
        
          });
    
        });
        */
        
      });
      
  });
  
}());