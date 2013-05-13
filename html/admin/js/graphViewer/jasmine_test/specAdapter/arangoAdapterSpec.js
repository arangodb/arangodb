/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine */
/*global runs, spyOn, waitsFor, waits */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global describeInterface*/
/*global ArangoAdapter*/

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

  describe('Arango Adapter', function () {
    /*
    describeInterface(new ArangoAdapter([], [], {
      nodeCollection: "",
      edgeCollection: ""
    }));
    */
    var adapter,
      nodes,
      edges,
      arangodb = "http://localhost:8529",
      nodesCollection,
      altNodesCollection,
      edgesCollection,
      altEdgesCollection,
      mockCollection,
      callbackCheck,
      checkCallbackFunction = function() {
        callbackCheck = true;
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
      },
            
      insertEdge = function (collectionID, from, to, cont) {
        var key = Math.floor(Math.random()*100000),
          id = collectionID + "/" + key;
        cont = cont || {};
        mockCollection[collectionID] = mockCollection[collectionID] || {};
        mockCollection[collectionID][from] = mockCollection[collectionID][from] || {};
        cont._id = id;
        cont._key = key;
        cont._rev = key;
        cont._from = from;
        cont._to = to;
        mockCollection[collectionID][from][to] = cont;
        return id;
      },
      insertNode = function (collectionID, nodeId, cont) {
        var key = Math.floor(Math.random()*100000),
          id = collectionID + "/" + key;
        cont = cont || {};
        mockCollection[collectionID] = mockCollection[collectionID] || {};
        cont.id = nodeId;
        cont._id = id;
        cont._key = key;
        cont._rev = key;
        mockCollection[collectionID][id] = cont;
        return id;
      },
      readEdge = function (collectionID, from, to) {
        return mockCollection[collectionID][from._id][to._id];
      },
      readNode = function (collectionID, id) {
        return mockCollection[collectionID][id];
      },
      constructPath = function(colNodes, colEdges, from, to) {
        var obj = {},
          src = readNode(colNodes, from),
          tar = readNode(colNodes, to);
        obj.vertex = tar;
        obj.path = {
          edges: [
            readEdge(colEdges, src, tar)
          ],
          vertices: [
            src,
            tar
          ]
        };
        return obj;
      };
    
      beforeEach(function() {
        nodes = [];
        edges = [];
        mockCollection = {};
        nodesCollection = "TestNodes321";
        edgesCollection = "TestEdges321";
        altNodesCollection = "TestNodes654";
        altEdgesCollection = "TestEdges654";
        
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
      });
      
      afterEach(function() {
        expect(nodes).toHaveCorrectCoordinates();
      });
      
      it('should throw an error if no nodes are given', function() {
        expect(
          function() {
            var t = new ArangoAdapter();
          }
        ).toThrow("The nodes have to be given.");
      });
      
      it('should throw an error if no edges are given', function() {
        expect(
          function() {
            var t = new ArangoAdapter([]);
          }
        ).toThrow("The edges have to be given.");
      });
      
      it('should throw an error if no nodeCollection is given', function() {
        expect(
          function() {
            var t = new ArangoAdapter([], [], {
              edgeCollection: ""
            });
          }
        ).toThrow("The nodeCollection has to be given.");
      });
      
      it('should throw an error if no edgeCollection is given', function() {
        expect(
          function() {
            var t = new ArangoAdapter([], [], {
              nodeCollection: ""
            });
          }
        ).toThrow("The edgeCollection has to be given.");
      });
    
      it('should not throw an error if everything is given', function() {
        expect(
          function() {
            var t = new ArangoAdapter([], [], {
              nodeCollection: "",
              edgeCollection: ""
            });
          }
        ).not.toThrow();
      });
    
      it('should automatically determine the host of not given', function() {
      
      });
    
      describe('setup correctly', function() {
        
        var travQ,
          filterQ,
          apiCursor,
          host,
          travVars,
          filterVars;
        
        beforeEach(function() {      
          adapter = new ArangoAdapter(
            nodes,
            edges,
            {
              nodeCollection: nodesCollection,
              edgeCollection: edgesCollection,
              width: 100,
              height: 40
            }
          );
          travQ = '{"query":"RETURN TRAVERSAL(@@nodes, @@edges, @id, \\"outbound\\", {strategy: \\"depthfirst\\",maxDepth: 1,paths: true})"';
          filterQ = '{"query":"FOR n IN @@nodes  FILTER n.id == @value RETURN TRAVERSAL(@@nodes, @@edges, n._id, \\"outbound\\", {strategy: \\"depthfirst\\",maxDepth: 1,paths: true})"';
          host = window.location.protocol + "//" + window.location.host;
          apiCursor = host + '/_api/cursor';
          travVars = function(id, nods, edgs) {
            return '"bindVars":{"id":"' + id + '","@nodes":"' + nods + '","@edges":"' + edgs + '"}}';
          };
          filterVars = function(v, nods, edgs) {
            return '"bindVars":{"value":' + v + ',"@nodes":"' + nods + '","@edges":"' + edgs + '"}}';
          };
        });
    
        it('should be able to load a tree node from ' 
           + 'ArangoDB by internal _id attribute', function() {
      
          var c0, c1, c2, c3, c4;
      
          runs(function() {
            spyOn($, "ajax").andCallFake(function(request) {
              var res = [];
              var inner = [];
              res.push(inner);
              var first = {};
              var node1 = readNode(nodesCollection, c0);
              first.vertex = node1;
              first.path = {
                edges: [],
                vertices: [
                 node1
                ]
              };
              inner.push(first);
              inner.push(constructPath(nodesCollection, edgesCollection, c0, c1));
              inner.push(constructPath(nodesCollection, edgesCollection, c0, c2));
              inner.push(constructPath(nodesCollection, edgesCollection, c0, c3));
              inner.push(constructPath(nodesCollection, edgesCollection, c0, c4));
              request.success({result: res});
            });
            
            c0 = insertNode(nodesCollection, 0);
            c1 = insertNode(nodesCollection, 1);
            c2 = insertNode(nodesCollection, 2);
            c3 = insertNode(nodesCollection, 3);
            c4 = insertNode(nodesCollection, 4);
        
            insertEdge(edgesCollection, c0, c1);
            insertEdge(edgesCollection, c0, c2);
            insertEdge(edgesCollection, c0, c3);
            insertEdge(edgesCollection, c0, c4);
        
            callbackCheck = false;
            adapter.loadNodeFromTreeById(c0, checkCallbackFunction);
          });
      
          waitsFor(function() {
            return callbackCheck;
          });
      
          runs(function() {
            existNodes([c0, c1, c2, c3, c4]);
            expect(nodes.length).toEqual(5);
            expect($.ajax).toHaveBeenCalledWith({
              type: 'POST',
              url: apiCursor,
              data: travQ + ',' + travVars(c0, nodesCollection, edgesCollection),
              contentType: 'application/json',
              dataType: 'json',
              success: jasmine.any(Function),
              error: jasmine.any(Function),
              processData: false
            });
          });
        });
        
        
        it('should be able to load a tree node from ArangoDB'
          + ' by internal attribute and value', function() {
      
          var c0, c1, c2, c3, c4;
      
          runs(function() {
            spyOn($, "ajax").andCallFake(function(request) {
              var res = [];
              var inner = [];
              res.push(inner);
              var first = {};
              var node1 = readNode(nodesCollection, c0);
              first.vertex = node1;
              first.path = {
                edges: [],
                vertices: [
                 node1
                ]
              };
              inner.push(first);
              inner.push(constructPath(nodesCollection, edgesCollection, c0, c1));
              inner.push(constructPath(nodesCollection, edgesCollection, c0, c2));
              inner.push(constructPath(nodesCollection, edgesCollection, c0, c3));
              inner.push(constructPath(nodesCollection, edgesCollection, c0, c4));
              request.success({result: res});
            });
            
            
            c0 = insertNode(nodesCollection, 0);
            c1 = insertNode(nodesCollection, 1);
            c2 = insertNode(nodesCollection, 2);
            c3 = insertNode(nodesCollection, 3);
            c4 = insertNode(nodesCollection, 4);
        
            insertEdge(edgesCollection, c0, c1);
            insertEdge(edgesCollection, c0, c2);
            insertEdge(edgesCollection, c0, c3);
            insertEdge(edgesCollection, c0, c4);
        
            callbackCheck = false;
            adapter.loadNodeFromTreeByAttributeValue("id", 0, checkCallbackFunction);
          });
      
          waitsFor(function() {
            return callbackCheck;
          });
      
          runs(function() {
            existNodes([c0, c1, c2, c3, c4]);
            expect(nodes.length).toEqual(5);
            expect($.ajax).toHaveBeenCalledWith({
              type: 'POST',
              url: apiCursor,
              data: filterQ + ',' + filterVars("0", nodesCollection, edgesCollection),
              contentType: 'application/json',
              dataType: 'json',
              processData: false,
              success: jasmine.any(Function),
              error: jasmine.any(Function)
              
            });
          });
        });
        /*
        it('should be able to request the number of children centrality', function() {
          var c0, c1 ,c2 ,c3 ,c4,
          children;
          runs(function() {
            c0 = insertNode(nodesCollection, 0);
            c1 = insertNode(nodesCollection, 1);
            c2 = insertNode(nodesCollection, 2);
            c3 = insertNode(nodesCollection, 3);
            c4 = insertNode(nodesCollection, 4);
        
            insertEdge(edgesCollection, c0, c1);
            insertEdge(edgesCollection, c0, c2);
            insertEdge(edgesCollection, c0, c3);
            insertEdge(edgesCollection, c0, c4);
        
            callbackCheck = false;
            adapter.requestCentralityChildren(c0, function(count) {
              callbackCheck = true;
              children = count;
            });
          });
      
          waitsFor(function() {
            return callbackCheck;
          });
      
          runs(function() {
            expect(children).toEqual(4);
          });
        });
   
        it('should encapsulate all attributes of nodes and edges in _data', function() {
          var c0, c1, e1_2;
      
          runs(function() {
            c0 = insertNode(nodesCollection, 0, {name: "Alice", age: 42});
            c1 = insertNode(nodesCollection, 1, {name: "Bob", age: 1337});
            e1_2 = insertEdge(edgesCollection, c0, c1, {label: "knows"});
        
            callbackCheck = false;
            adapter.loadNodeFromTreeById(c0, checkCallbackFunction);
          });
      
          waitsFor(function() {
            return callbackCheck;
          });
      
          runs(function() {
            expect(nodes[0]._data).toEqual({
              _id: c0,
              _key: jasmine.any(String),
              _rev: jasmine.any(String),
              id: 0,
              name: "Alice",
              age: 42
            });
            expect(nodes[1]._data).toEqual({
              _id: c1,
              _key: jasmine.any(String),
              _rev: jasmine.any(String),
              id: 1,
              name: "Bob",
              age: 1337
            });
            expect(edges[0]._data).toEqual({
              _id: e1_2,
              _from: c0,
              _to: c1,
              _key: jasmine.any(String),
              _rev: jasmine.any(String),
              label: "knows"
            });
          });
       
      
        });
   
        it('should be able to switch to different collections', function() {
          var c0, c1, e1_2, insertedId;
      
          runs(function() {
            c0 = insertNode(altNodesCollection, 0);
            c1 = insertNode(altNodesCollection, 1);
            e1_2 = insertEdge(altEdgesCollection, c0, c1);
      
            adapter.changeTo(altNodesCollection, altEdgesCollection);
      
            callbackCheck = false;
            adapter.loadNodeFromTreeById(c0, checkCallbackFunction);
          });
    
          waitsFor(function() {
            return callbackCheck;
          });
    
          runs(function() {
            existNodes([c0, c1]);
            expect(nodes.length).toEqual(2);
        
            callbackCheck = false;
            adapter.createNode({}, function(node) {
              insertedId = node._id;
              callbackCheck = true;
            });
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
              }
            });
        
          });
      
          waitsFor(function() {
            return callbackCheck;
          });
      
          runs(function() {
            expect(insertedId).toBeStoredPermanently();
            existNode(insertedId);
          });
      
        });
   
    
        describe('that has already loaded one graph', function() {
          var c0, c1, c2, c3, c4, c5, c6, c7;
      
      
          beforeEach(function() {
      
            runs(function() {
              
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
              adapter.loadNodeFromTreeById(c0, checkCallbackFunction);
          
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
              adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
            });
      
            waitsFor(function() {
              return callbackCheck;
            });
      
            runs(function() {
              existNodes([c0, c1, c2, c3, c4, c5, c6, c7]);
              expect(nodes.length).toEqual(8);
            });
          });
      
          it('should be able to change a value of one node permanently', function() {
            var toPatch;
        
            runs(function() {
              toPatch = nodeWithID(c0);
              adapter.patchNode(toPatch, {hello: "world"}, checkCallbackFunction);
            });
        
            waitsFor(function() {
              return callbackCheck;
            });
        
            runs(function() {
              expect(toPatch._data.hello).toEqual("world");
              expect(toPatch._id).toHavePermanentAttributeWithValue("hello", "world");
            });
        
          });
      
          it('should be able to change a value of one edge permanently', function() {
            var toPatch;
        
            runs(function() {
              toPatch = edgeWithSourceAndTargetId(c0, c1);
              adapter.patchEdge(toPatch, {hello: "world"}, checkCallbackFunction);
            });
        
            waitsFor(function() {
              return callbackCheck;
            });
        
            runs(function() {
              expect(toPatch._data.hello).toEqual("world");
              expect(toPatch._id).toHavePermanentAttributeWithValue("hello", "world");
            });
          });
      
          it('should be able to remove an edge permanently', function() {
        
            var toDelete;
        
            runs(function() {
              toDelete = edgeWithSourceAndTargetId(c0, c4);
              adapter.deleteEdge(toDelete, checkCallbackFunction);
              existEdge(c0, c4);
            });
        
            waitsFor(function() {
              return callbackCheck;
            });
        
            runs(function() {
              expect(toDelete._id).toNotBeStoredPermanently();
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
              expect(insertedId).toBeStoredPermanently();
              existNode(insertedId);
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
                adapter.loadNodeFromTreeById(c2, checkCallbackFunction);
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
                adapter.loadNodeFromTreeById(c3, checkCallbackFunction);
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
                expect(insertedId).toBeStoredPermanently();
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
                toDelete = nodeWithID(c2);
                adapter.deleteNode(toDelete, checkCallbackFunction);
              });
        
              waits(2000);
        
              runs(function() {
                expect(toDelete._id).toNotBeStoredPermanently();
                notExistNode(c2);
                expect(e2_8).toNotBeStoredPermanently();
                notExistEdge(c2, c8);
              });
            });
      
          });
      
        });
   
        describe('displaying only parts of the graph', function() {
    
          it('should be able to remove a node and all '
          + 'connected edges including not visible ones', function() {
            var s0, s1, t0, toDel,
            s0_toDel, s1_toDel, toDel_t0;
        
            runs(function() {
          
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
                }
              });
          
              callbackCheck = false;
              s0 = insertNode(nodesCollection, 0);
              s1 = insertNode(nodesCollection, 1);
              t0 = insertNode(nodesCollection, 2);
              toDel = insertNode(nodesCollection, 3);
          
              s0_toDel = insertEdge(edgesCollection, s0, toDel);
              s1_toDel = insertEdge(edgesCollection, s1, toDel);
              toDel_t0 = insertEdge(edgesCollection, toDel, t0);
          
              adapter.loadNodeFromTreeById(s0, checkCallbackFunction);
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
          
              expect(toDel).toNotBeStoredPermanently();
              expect(s0).toBeStoredPermanently();
              expect(s1).toBeStoredPermanently();
              expect(t0).toBeStoredPermanently();
          
          
              expect(s0_toDel).toNotBeStoredPermanently();
              expect(s1_toDel).toNotBeStoredPermanently();
              expect(toDel_t0).toNotBeStoredPermanently();
          
              // Check if counter is set correctly
              expect(nodeWithID(s0)._outboundCounter).toEqual(0);
            });
        
          });
    
        });
        
        */
      });
   
  });
  
  
  
}());