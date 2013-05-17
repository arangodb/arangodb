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
    
    describeInterface(new ArangoAdapter([], [], {
      nodeCollection: "",
      edgeCollection: ""
    }));
    
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
        var key = String(Math.floor(Math.random()*100000)),
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
        var key = String(Math.floor(Math.random()*100000)),
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
        var adapter = new ArangoAdapter(
          nodes,
          edges,
          {
            nodeCollection: nodesCollection,
            edgeCollection: edgesCollection,
            width: 100,
            height: 40
          }
        ),
          args,
          host;
        spyOn($, "ajax");
        adapter.createNode({}, function() {});
        args = $.ajax.mostRecentCall.args[0];
        host = window.location.protocol + "//" + window.location.host;
        expect(args.url).toContain(host);
      });
      
      it('should create a nodeReducer instance', function() {
        spyOn(window, "NodeReducer");
        var adapter = new ArangoAdapter(
          nodes,
          edges,
          {
            nodeCollection: nodesCollection,
            edgeCollection: edgesCollection,
            width: 100,
            height: 40
          }
        );
        expect(window.NodeReducer).wasCalledWith(nodes, edges);
      });
    
      describe('setup correctly', function() {
        
        var travQ,
          filterQ,
          childQ,
          travVars,
          filterVars,
          loadGraph,
          requests;
        
        beforeEach(function() {  
          var self = this;
          self.fakeReducerRequest = function() {};
          spyOn(window, "NodeReducer").andCallFake(function(v, e) {
            return {
              getCommunity: function(limit, focus) {
                if (focus !== undefined) {
                  self.fakeReducerRequest(limit, focus);
                } else {
                  self.fakeReducerRequest(limit);
                }
              }
            };
          });
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
          childQ = '{"query":"FOR u IN @@nodes FILTER u._id == @id LET g = ( FOR l in @@edges FILTER l._from == u._id RETURN 1 ) RETURN length(g)"';
          var host = window.location.protocol + "//" + window.location.host,
           apibase = host + "/_api/",
           apiCursor = apibase + 'cursor';
          travVars = function(id, nods, edgs) {
            return '"bindVars":{"id":"' + id + '","@nodes":"' + nods + '","@edges":"' + edgs + '"}}';
          };
          filterVars = function(v, nods, edgs) {
            return '"bindVars":{"value":' + v + ',"@nodes":"' + nods + '","@edges":"' + edgs + '"}}';
          };
          
          loadGraph = function(vars) {
            var nid = vars["id"];
            var ncol = vars["@nodes"];
            var ecol = vars["@edges"];
            var res = [];
            var inner = [];
            res.push(inner);
            var first = {};
            var node1 = readNode(ncol, nid);
            first.vertex = node1;
            first.path = {
              edges: [],
              vertices: [
               node1
              ]
            };
            inner.push(first);
            if (mockCollection[ecol][nid] !== undefined) {
              _.each(mockCollection[ecol][nid], function(val, key) {
                inner.push(constructPath(ncol, ecol, nid, key));
              });
            }
            return res;
          };
          
          
          requests = {};
          requests.cursor = function(data) {
            return {
              type: 'POST',
              url: apiCursor,
              data: data,
              contentType: 'application/json',
              dataType: 'json',
              success: jasmine.any(Function),
              error: jasmine.any(Function),
              processData: false
            };
          };
          requests.node = function(col) {
            var read = apibase + "document?collection=" + col,
              write = apibase + "document/",
              base = {
                cache: false,
                dataType: "json",
                contentType: "application/json",
                processData: false,
                success: jasmine.any(Function),
                error: jasmine.any(Function)
              };
            return {
              create: function(data) {
                return $.extend(base, {url: read, type: "POST", data: JSON.stringify(data)});
              },
              patch: function(id, data) {
                return $.extend(base, {url: write + id, type: "PUT", data: JSON.stringify(data)});
              },
              delete: function(id) {
                return $.extend(base, {url: write + id, type: "DELETE"});
              }
            };
          };
          requests.edge = function(col) {
            var create = apibase + "edge?collection=" + col,
              base = {
                cache: false,
                dataType: "json",
                contentType: "application/json",
                processData: false,
                success: jasmine.any(Function),
                error: jasmine.any(Function)
              };
            return {
              create: function(from, to, data) {
                return $.extend(base, {
                  url: create + "&from=" + from + "&to=" + to,
                  type: "POST",
                  data: JSON.stringify(data)
                });
              },
            };
          };
        });
    
        it('should be able to load a tree node from ' 
           + 'ArangoDB by internal _id attribute', function() {
      
          var c0, c1, c2, c3, c4;
      
          runs(function() {
            spyOn($, "ajax").andCallFake(function(request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                request.success({result: loadGraph(vars)});
              }
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
          }, 1000);
      
          runs(function() {
            existNodes([c0, c1, c2, c3, c4]);
            expect(nodes.length).toEqual(5);
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(travQ + ',' + travVars(c0, nodesCollection, edgesCollection))
            );
          });
        });
        
        it('should be able to load a tree node from ArangoDB'
          + ' by internal attribute and value', function() {
      
          var c0, c1, c2, c3, c4;
      
          runs(function() {
            spyOn($, "ajax").andCallFake(function(request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                vars["id"] = c0;
                request.success({result: loadGraph(vars)});
              }
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
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(filterQ + ',' + filterVars("0", nodesCollection, edgesCollection))
            );
          });
        });
        
        it('should be able to request the number of children centrality', function() {
          var c0,
          children;
          runs(function() {
            c0 = insertNode(nodesCollection, 0);
            spyOn($, "ajax").andCallFake(function(request) {
              request.success({result: [4]});
            });
        
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
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(childQ + ',' + travVars(c0, nodesCollection, edgesCollection))
            );
          });
        });
        
        it('should encapsulate all attributes of nodes and edges in _data', function() {
          var c0, c1, e1_2;
      
          runs(function() {
            
            spyOn($, "ajax").andCallFake(function(request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                request.success({result: loadGraph(vars)});
              }
            });
            
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
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(travQ + ',' + travVars(c0, nodesCollection, edgesCollection))
            );
          });
       
      
        });
        
        it('should be able to switch to different collections', function() {
          var c0, c1, e1_2, insertedId;
      
          runs(function() {
            
            spyOn($, "ajax").andCallFake(function(request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                request.success({result: loadGraph(vars)});
              } else {
                request.success({result: {}});
              }
            });
            
            c0 = insertNode(altNodesCollection, 0);
            c1 = insertNode(altNodesCollection, 1);
            e1_2 = insertEdge(altEdgesCollection, c0, c1);
      
            adapter.changeTo(altNodesCollection, altEdgesCollection);
      
            callbackCheck = false;
            adapter.loadNodeFromTreeById(c0, checkCallbackFunction);
          });
    
          waitsFor(function() {
            return callbackCheck;
          }, 1000);
    
          runs(function() {
            existNodes([c0, c1]);
            expect(nodes.length).toEqual(2);
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(travQ + ',' + travVars(c0, altNodesCollection, altEdgesCollection))
            );
            
            callbackCheck = false;
            adapter.createNode({}, function(node) {
              insertedId = node._id;
              callbackCheck = true;
            });
          });
      
          waitsFor(function() {
            return callbackCheck;
          }, 1000);
      
          runs(function() {
            existNode(insertedId);
            expect($.ajax).toHaveBeenCalledWith(
              requests.node(altNodesCollection).create({})
            );
          });
      
        });
   
        
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
              expect($.ajax).toHaveBeenCalledWith(
                requests.cursor(travQ + ',' + travVars(c1, nodesCollection, edgesCollection))
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
                requests.node(edgesCollection).delete(toDelete._id)
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
              spyOn(this, "fakeReducerRequest");
              adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
              expect(this.fakeReducerRequest).toHaveBeenCalledWith(6, nodeWithID(c1));
            });
          });
          
          
          it('should render a reduced set of nodes if too many nodes are added', function() {
            runs(function() {
              adapter.setNodeLimit(6);
              spyOn(this, "fakeReducerRequest").andCallFake(function() {
                return [0, 1, 2, 3];
              });
              adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
            });
            
            waitsFor(function() {
              return callbackCheck;
            });
      
            runs(function() {
              notExistNodes([c0, c1, c2, c3]);
              existNodes([c4, c5, c6, c7]);
              expect(nodes.length).toEqual(5);
              
            });
            
          });
          
          it('should not trigger the reducer if the limit is set large enough', function() {
            spyOn(this, "fakeReducerRequest");
            adapter.setNodeLimit(10);
            expect(this.fakeReducerRequest).not.toHaveBeenCalled();
          });
          
          
          it('should trigger the reducer if the limit is set too small', function() {
            spyOn(this, "fakeReducerRequest");
            adapter.setNodeLimit(2);
            expect(this.fakeReducerRequest).toHaveBeenCalledWith(2);
          });
          
          it('should reshape all objects if limit is set too small', function() {
            var called = false;
            var callback = function() {
              called = true;
            };
            spyOn(this, "fakeReducerRequest").andCallFake(function() {
              return [0, 1, 2];
            });
            adapter.setNodeLimit(2, callback);
            
            notExistNodes([c0, c1, c2]);
            existNodes([c3]);
            expect(nodes.length).toEqual(2);
            
            expect(called).toBeTruthy();
            
            
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
                }
                fakeResult = "";
                toDelete = nodeWithID(c2);
                adapter.deleteNode(toDelete, checkCallbackFunction);
              });
        
              waits(2000);
        
              runs(function() {
                expect($.ajax).toHaveBeenCalledWith(
                  requests.node(nodesCollection).delete(toDelete._id)
                );
                notExistNode(c2);
                expect($.ajax).toHaveBeenCalledWith(
                  requests.node(edgesCollection).delete(e2_8)
                );
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
          
              callbackCheck = false;
              s0 = insertNode(nodesCollection, 0);
              s1 = insertNode(nodesCollection, 1);
              t0 = insertNode(nodesCollection, 2);
              toDel = insertNode(nodesCollection, 3);
          
              s0_toDel = insertEdge(edgesCollection, s0, toDel);
              s1_toDel = insertEdge(edgesCollection, s1, toDel);
              toDel_t0 = insertEdge(edgesCollection, toDel, t0);
              
              var loaded = false;
              var fakeResult = "";
              
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
              
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(nodesCollection).delete(toDel)
              );
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(edgesCollection).delete(s0_toDel)
              );
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(edgesCollection).delete(s1_toDel)
              );
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(edgesCollection).delete(toDel_t0)
              );
              
              // Check if counter is set correctly
              expect(nodeWithID(s0)._outboundCounter).toEqual(0);
            });
        
          });
    
        });
        
        
      });
      
  });
  
  
  
}());