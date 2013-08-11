/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect, jasmine */
/*global runs, spyOn, waitsFor, waits */
/*global window, eb, loadFixtures, document */
/*global $, _, d3*/
/*global describeInterface, describeIntegeration*/
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
            if (req.url.match(/_api\/cursor$/)) {
              req.success({result:
              [
                [{
                  vertex: node1,
                  path: {
                    edges: [],
                    vertices: [
                     node1
                    ]
                  }
            
                },{
                  vertex: node2,
                  path: {
                    edges: [
                      edge
                    ],
                    vertices: [
                      node1,
                      node2
                    ]
                  }
                }]
              ]});
            } else if (req.url.match(/_api\/edge/)) {
              req.success({_id: "1-2"});
            } else {
              req.success({_id: 1});
            }
            break;
          default:
            req.success();
        }
      });
      
      return new ArangoAdapter([], [], {
        nodeCollection: "",
        edgeCollection: "",
        prioList: ["foo", "bar", "baz"]
      });
    });

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
      
      it('should throw an error if no nodeCollection or graph is given', function() {
        expect(
          function() {
            var t = new ArangoAdapter([], [], {
              edgeCollection: ""
            });
          }
        ).toThrow("The nodeCollection or a graphname has to be given.");
      });
      
      it('should throw an error if no edgeCollection or graph is given', function() {
        expect(
          function() {
            var t = new ArangoAdapter([], [], {
              nodeCollection: ""
            });
          }
        ).toThrow("The edgeCollection or a graphname has to be given.");
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
        expect(window.NodeReducer).wasCalledWith();
      });
      
      it('should create the ModularityJoiner as a worker', function() {
        spyOn(window, "WebWorkerWrapper");
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
        expect(window.WebWorkerWrapper).wasCalledWith(
          window.ModularityJoiner,
          jasmine.any(Function)
        );
      });
    
      describe('setup correctly', function() {
        
        var traversalQuery,
          filterQuery,
          childrenQuery,
          loadGraph,
          requests,
          mockWrapper,
          workerCB;
        
        beforeEach(function() {  
          var self = this,
           host = window.location.protocol + "//" + window.location.host,
           apibase = host + "/_api/",
           apiCursor = apibase + 'cursor';
          self.fakeReducerBucketRequest = function() {};
          mockWrapper = {};
          mockWrapper.call = function() {};
          spyOn(window, "NodeReducer").andCallFake(function() {
            return {
              bucketNodes: function(toSort, numBuckets) {
                return self.fakeReducerBucketRequest(toSort, numBuckets);
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
          traversalQuery = function(id, nods, edgs, undirected) {
            var dir;
            if (undirected === true) {
              dir = "any";
            } else {
              dir = "outbound";
            }
            return JSON.stringify({
              query: "RETURN TRAVERSAL(@@nodes, @@edges, @id, @dir,"
                + " {strategy: \"depthfirst\",maxDepth: 1,paths: true})",
              bindVars: {
                id: id,
                "@nodes": nods,
                dir: dir,
                "@edges": edgs
              }
            });
          };
          filterQuery = function(v, nods, edgs, undirected) {
            var dir;
            if (undirected === true) {
              dir = "any";
            } else {
              dir = "outbound";
            }
            return JSON.stringify({
              query: "FOR n IN @@nodes FILTER n.id == @value"
                + " RETURN TRAVERSAL(@@nodes, @@edges, n._id, @dir,"
                + " {strategy: \"depthfirst\",maxDepth: 1,paths: true})",
              bindVars: {
                value: v,
                "@nodes": nods,
                dir: dir,
                "@edges": edgs 
              }
            });
          };
          childrenQuery = function(id, nods, edgs) {
            return JSON.stringify({
              query: "FOR u IN @@nodes FILTER u._id == @id"
               + " LET g = ( FOR l in @@edges FILTER l._from == u._id RETURN 1 )" 
               + " RETURN length(g)",
              bindVars: {
                id: id,
                "@nodes": nods,
                "@edges": edgs
              }
            });
          };          
          loadGraph = function(vars) {
            var nid = vars.id,
             ncol = vars["@nodes"],
             ecol = vars["@edges"],
             res = [],
             inner = [],
             first = {},
             node1 = readNode(ncol, nid);
            res.push(inner);
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
              del: function(id) {
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
              }
            };
          };
        });
        
        it('should offer lists of available collections', function() {
          var collections = [],
          sys1 = {id: "1", name: "_sys1", status: 3, type: 2},
          sys2 = {id: "2", name: "_sys2", status: 2, type: 2},
          doc1 = {id: "3", name: "doc1", status: 3, type: 2},
          doc2 = {id: "4", name: "doc2", status: 2, type: 2},
          doc3 = {id: "5", name: "doc3", status: 3, type: 2},
          edge1 = {id: "6", name: "edge1", status: 3, type: 3},
          edge2 = {id: "7", name: "edge2", status: 2, type: 3};

          collections.push(sys1);
          collections.push(sys2);
          collections.push(doc1);
          collections.push(doc2);
          collections.push(doc3);
          collections.push(edge1);
          collections.push(edge2);
          
          spyOn($, "ajax").andCallFake(function(request) {
            request.success({collections: collections});
          });
          
          adapter.getCollections(function(docs, edge) {
            expect(docs).toContain("doc1");
            expect(docs).toContain("doc2");
            expect(docs).toContain("doc3");
            
            expect(docs.length).toEqual(3);
            
            expect(edge).toContain("edge1");
            expect(edge).toContain("edge2");
            
            expect(edge.length).toEqual(2);
          });
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
              requests.cursor(traversalQuery(c0, nodesCollection, edgesCollection))
            );
          });
        });
        
        it('should map loadNode to loadByID', function() {
          spyOn(adapter, "loadNodeFromTreeById");
          adapter.loadNode("a", "b");
          expect(adapter.loadNodeFromTreeById).toHaveBeenCalledWith("a", "b");
        });
        
        it('should be able to load a tree node from ArangoDB'
        + ' by internal attribute and value', function() {
      
          var c0, c1, c2, c3, c4;
      
          runs(function() {
            spyOn($, "ajax").andCallFake(function(request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                vars.id = c0;
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
          }, 1000);
      
          runs(function() {
            existNodes([c0, c1, c2, c3, c4]);
            expect(nodes.length).toEqual(5);
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(filterQuery(0, nodesCollection, edgesCollection))
            );
          });
        });
        
        it('should callback with proper errorcode if no results are found', function() {
          var dummy = {
            cb: function() {}
          };
          spyOn(dummy, "cb");
          spyOn($, "ajax").andCallFake(function(request) {
            request.success({result: []});
          });
          adapter.loadNode("node", dummy.cb);
          expect(dummy.cb).wasCalledWith({
            errorCode: 404
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
              requests.cursor(childrenQuery(c0, nodesCollection, edgesCollection))
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
          }, 1000);
      
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
              requests.cursor(traversalQuery(c0, nodesCollection, edgesCollection))
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
      
            adapter.changeToCollections(altNodesCollection, altEdgesCollection);
      
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
              requests.cursor(traversalQuery(c0, altNodesCollection, altEdgesCollection))
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
   
        it('should be able to switch to different collections and change to directed', function() {
      
          runs(function() {
            
            spyOn($, "ajax");
            
            adapter.changeToCollections(altNodesCollection, altEdgesCollection, false);
            
            adapter.loadNode("42");
            
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(traversalQuery("42", altNodesCollection, altEdgesCollection, false))
            );
            
          });      
        });
        
        it('should be able to switch to different collections'
        + ' and change to undirected', function() {
      
          runs(function() {
            
            spyOn($, "ajax");
            
            adapter.changeToCollections(altNodesCollection, altEdgesCollection, true);
            
            adapter.loadNode("42");
            
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(traversalQuery("42", altNodesCollection, altEdgesCollection, true))
            );
            
          });      
        });
        
        it('should add at most the upper bound of children in one step', function() {
          var inNodeCol, callNodes;
          
          runs(function() {
            var addNNodes = function(n) {
                var i = 0,
                  res = [];
                for (i = 0; i < n; i++) {
                  res.push(insertNode(nodesCollection, i));
                }
                return res;
              },
              connectToAllButSelf = function(source, ns) {
                _.each(ns, function(target) {
                  if (source !== target) {
                    insertEdge(edgesCollection, source, target);
                  }
                });
              };
            
            inNodeCol = addNNodes(21);
            connectToAllButSelf(inNodeCol[0], inNodeCol);
            adapter.setChildLimit(5);
            
            spyOn($, "ajax").andCallFake(function(request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                request.success({result: loadGraph(vars)});
              }
            });
            spyOn(this, "fakeReducerBucketRequest").andCallFake(function(ns) {
              var i = 0,
                res = [],
                pos;
              callNodes = ns;
              for (i = 0; i < 5; i++) {
                pos = i*4;
                res.push({
                  reason: {
                    type: "similar",
                    example: ns[pos]
                  },
                  nodes: ns.slice(pos, pos + 4)
                });
              }
              return res;
            });
            
            callbackCheck = false;
            adapter.loadNodeFromTreeById(inNodeCol[0], checkCallbackFunction);
            
          });
          
          waitsFor(function() {
            return callbackCheck;
          }, 1000);
          
          runs(function() {
            var callNodesIds = _.map(callNodes, function(n) {
              return n._id;
            });
            expect(this.fakeReducerBucketRequest).toHaveBeenCalledWith(
              jasmine.any(Array),
              5
            );
            expect(callNodesIds).toEqual(inNodeCol.slice(1));
            expect(nodes.length).toEqual(6);
            expect(getCommunityNodes().length).toEqual(5);
          });
          
        });
        
        it('should not bucket existing nodes', function() {
          var lastCallWith, n0, n1, n2, n3, n4, n5, n6;
          
          runs(function() {
            var connectToAllButSelf = function(source, ns) {
                _.each(ns, function(target) {
                  if (source !== target) {
                    insertEdge(edgesCollection, source, target);
                  }
                });
              };
            
            n0 = insertNode(nodesCollection, 0);
            n1 = insertNode(nodesCollection, 1);
            n2 = insertNode(nodesCollection, 2);
            n3 = insertNode(nodesCollection, 3);
            n4 = insertNode(nodesCollection, 4);
            n5 = insertNode(nodesCollection, 5);
            n6 = insertNode(nodesCollection, 6);

            connectToAllButSelf(n0, [n1, n2, n3]);
            
            insertEdge(edgesCollection, n1, n0);
            insertEdge(edgesCollection, n1, n2);
            insertEdge(edgesCollection, n1, n4);
            insertEdge(edgesCollection, n1, n5);
            insertEdge(edgesCollection, n1, n6);
            
            adapter.setChildLimit(2);
            
            spyOn($, "ajax").andCallFake(function(request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                request.success({result: loadGraph(vars)});
              }
            });
            spyOn(this, "fakeReducerBucketRequest").andCallFake(function(ns) {
              lastCallWith = _.pluck(ns, "_id");
              return [
                {
                  reason: {
                    type: "similar",
                    example: ns[0]
                  },
                  nodes: [ns[0]]
                },
                {
                  reason: {
                    type: "similar",
                    example: ns[1]
                  },
                  nodes: [ns[1], ns[2]]
                }
              ];
            });
            
            callbackCheck = false;
            adapter.loadNodeFromTreeById(n0, checkCallbackFunction);
            
          });
          
          waitsFor(function() {
            return callbackCheck;
          }, 1000);
          
          runs(function() {
            expect(lastCallWith).toEqual([n1, n2, n3]);
            
            expect(getCommunityNodes().length).toEqual(1);
            callbackCheck = false;
            adapter.loadNodeFromTreeById(n1, checkCallbackFunction);
          });
          
          waitsFor(function() {
            return callbackCheck;
          }, 1000);
          
          runs(function() {
            expect(lastCallWith).toEqual([n4, n5, n6]);
            
            expect(getCommunityNodes().length).toEqual(2);
          });
          
        });
        
        it('should not replace single nodes by communities', function() {
          var inNodeCol, callNodes;
          
          runs(function() {
            var addNNodes = function(n) {
                var i = 0,
                  res = [];
                for (i = 0; i < n; i++) {
                  res.push(insertNode(nodesCollection, i));
                }
                return res;
              },
              connectToAllButSelf = function(source, ns) {
                _.each(ns, function(target) {
                  if (source !== target) {
                    insertEdge(edgesCollection, source, target);
                  }
                });
              };
            
            inNodeCol = addNNodes(7);
            connectToAllButSelf(inNodeCol[0], inNodeCol);
            adapter.setChildLimit(5);
            
            spyOn($, "ajax").andCallFake(function(request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                request.success({result: loadGraph(vars)});
              }
            });
            spyOn(this, "fakeReducerBucketRequest").andCallFake(function(ns) {
              var i = 0,
                res = [],
                pos;
              for (i = 0; i < 4; i++) {
                res.push({
                  reason: {
                    type: "similar",
                    example: ns[i]
                  },
                  nodes: [ns[i]]
                });
              }
              res.push({
                reason: {
                  type: "similar",
                  example: ns[4]
                },
                nodes: [ns[4], ns[5]]
              });
              return res;
            });
            
            callbackCheck = false;
            adapter.loadNodeFromTreeById(inNodeCol[0], checkCallbackFunction);
            
          });
          
          waitsFor(function() {
            return callbackCheck;
          }, 1000);
          
          runs(function() {
            var callNodesIds = _.map(callNodes, function(n) {
              return n._id;
            });
            expect(this.fakeReducerBucketRequest).toHaveBeenCalledWith(
              jasmine.any(Array),
              5
            );
            expect(nodes.length).toEqual(6);
            expect(getCommunityNodes().length).toEqual(1);
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
            }, 1000);
      
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
            }, 1000);
        
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
            }, 1000);
        
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
            }, 1000);
        
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
            }, 1000);
        
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
              spyOn(mockWrapper, "call").andCallFake(function(name) {
                workerCB({
                  data: {
                    cmd: name,
                    result: [c0]
                  }
                });
              });
              adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
              expect(mockWrapper.call).toHaveBeenCalledWith("getCommunity", 6, c1);
            });
          });
          

          describe('checking community nodes', function() {
            
            it('should not trigger the reducer if the limit is set large enough', function() {
              spyOn(mockWrapper, "call").andCallFake(function(name) {
                workerCB({
                  data: {
                    cmd: name,
                    result: [c0]
                  }
                });
              });

              adapter.setNodeLimit(10);
              expect(mockWrapper.call).not.toHaveBeenCalled();
            });
          
            it('should trigger the reducer if the limit is set too small', function() {
              spyOn(mockWrapper, "call").andCallFake(function(name) {
                workerCB({
                  data: {
                    cmd: name,
                    result: [c0]
                  }
                });
              });
              adapter.setNodeLimit(2);
              expect(mockWrapper.call).toHaveBeenCalledWith("getCommunity", 2);
            });
            
            it('should create a community node if limit is set too small', function() {
              var called;
              
              runs(function() {
                callbackCheck = false;
                spyOn(mockWrapper, "call").andCallFake(function(name) {
                  workerCB({
                    data: {
                      cmd: name,
                      result: [c0, c1, c2]
                    }
                  });
                });
                adapter.setNodeLimit(2, checkCallbackFunction);
              });
              
              waitsFor(function() {
                return callbackCheck;
              }, 1000);
              
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
                spyOn(mockWrapper, "call").andCallFake(function(name) {
                  workerCB({
                    data: {
                      cmd: name,
                      result: [c0, c1, c2, c3]
                    }
                  });
                });
                adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
              });
            
              waitsFor(function() {
                return callbackCheck;
              }, 1000);
      
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
                  spyOn(mockWrapper, "call").andCallFake(function(name) {
                    workerCB({
                      data: {
                        cmd: name,
                        result: [v1, v3, v4]
                      }
                    });
                  });
                  adapter.setNodeLimit(3);
                  adapter.changeToCollections(v, e);
                  adapter.loadNode(v0, counterCallback);
                  adapter.loadNode(v1, counterCallback);
                  
                });
                
                waitsFor(function() {
                  return called === 2;
                }, 1000);
                
                runs(function() {
                  adapter.loadNode(v2, counterCallback);
                  commNode = getCommunityNodes()[0];
                });
                
                waitsFor(function() {
                  return called === 3;
                }, 1000);
                
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
                }, 1000);
                
                runs(function() {
                  var commId = commNode._id;
                  existNodes([commId, v0, v2]);
                  expect(nodes.length).toEqual(3);
                  existEdge(v0, v2);
                  existEdge(v0, commId);
                  existEdge(v2, commId);
                  expect(edges.length).toEqual(4);
                  expect(commNode._expanded).toBeTruthy();
                  
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
                  spyOn(mockWrapper, "call").andCallFake(function(name) {
                    workerCB({
                      data: {
                        cmd: name,
                        result: [v1, v3, v4]
                      }
                    });
                  });
                  adapter.setNodeLimit(3);
                  
                  adapter.changeToCollections(v, e);
                  adapter.loadNode(v0, counterCallback);
                  adapter.loadNode(v1, counterCallback);
                  
                });
                
                waitsFor(function() {
                  return called === 2;
                }, 1000);
                
                runs(function() {
                  adapter.loadNode(v2, counterCallback);
                  commNode = getCommunityNodes()[0];
                });
                
                waitsFor(function() {
                  return called === 3;
                }, 1000);
                
                runs(function() {
                  adapter.setNodeLimit(20);
                  adapter.expandCommunity(commNode, counterCallback);
                });
                
                waitsFor(function() {
                  return called === 4;
                }, 1000);
                
                runs(function() {
                  var checkNodeWithInAndOut = function(id, inbound, outbound) {
                    var n = nodeWithID(id) || commNode.getNode(id);
                    expect(n._outboundCounter).toEqual(outbound);
                    expect(n._inboundCounter).toEqual(inbound);
                  };
                  checkNodeWithInAndOut(v0, 0, 2);
                  checkNodeWithInAndOut(v1, 1, 2);
                  checkNodeWithInAndOut(v2, 1, 2);
                  checkNodeWithInAndOut(v3, 2, 0);
                  checkNodeWithInAndOut(v4, 2, 0);
                  expect(commNode._outboundCounter).toEqual(0);
                  expect(commNode._inboundCounter).toEqual(3);
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
                  spyOn(mockWrapper, "call").andCallFake(function(name) {
                    workerCB({
                      data: {
                        cmd: name,
                        result: fakeResult
                      }
                    });
                  });
                  adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
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
                  expect(getCommunityNodes().length).toEqual(1);
                  existNodes([firstCommId, c1, c3, c4, c5, c6, c7]);
                  existEdge(firstCommId, c1);
                  existEdge(firstCommId, c3);
                  existEdge(firstCommId, c4);
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
                }, 1000);
                
                runs(function() {
                  var commId = getCommunityNodesIds()[0],
                  newCommId = getCommunityNodesIds()[1];
                  expect(getCommunityNodes().length).toEqual(2);
                  existNodes([commId, c3, c4, c5, c6, newCommId]);
                  notExistNodes([c0, c1, c2, c7]);
                  
                  existEdge(commId, c3);
                  existEdge(commId, c4);
                  
                  existEdge(commId, newCommId);
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
                }, 1000);
                
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
              }, 1000);  
        
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
              }, 1000);
        
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

              adapter.loadNodeFromTreeById(s0, checkCallbackFunction);
            });
        
            waitsFor(function() {
              return callbackCheck;
            }, 1000);
        
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
        
        
      });

  });
  
  
  
}());
