/* jshint unused: false */
/* global beforeEach, afterEach */
/* global describe, it, expect, jasmine */
/* global runs, spyOn, waitsFor, waits */
/* global window, eb, loadFixtures, document */
/* global $, _, d3*/
/* global describeInterface, describeIntegeration*/
/* global GharialAdapter*/

// //////////////////////////////////////////////////////////////////////////////
// / @brief Graph functionality
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Michael Hackstein
// / @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

(function () {
  'use strict';

  describe('Gharial Adapter', function () {
    describeInterface(new GharialAdapter([], [], {}, {
      graphName: 'myGraph'
    }));

    describeIntegeration(function () {
      var adapter,
        ecol = 'myEdges',
        vcol = 'myVertices';
      spyOn($, 'ajax').andCallFake(function (req) {
        var node1 = {_id: 1},
          node2 = {_id: 2},
          edge = {_id: '1-2', _from: 1, _to: 2},
          urlParts = req.url.split('/');

        switch (req.type) {
          case 'GET':
            if (urlParts.length === 4) {
              if (urlParts[3] === 'edge') {
                req.success({
                  collections: [ecol]
                });
              } else if (urlParts[3] === 'vertex') {
                req.success({
                  collections: [vcol]
                });
              } else {
                req.success();
              }
            } else {
              req.success();
            }
            break;
          case 'DELETE':
            req.success();
            break;
          case 'POST':
            if (req.url.match(/_api\/cursor$/)) {
              req.success({
                result: [
                  [
                    [{
                      vertex: node1,
                      path: {
                        edges: [],
                        vertices: [
                          node1
                        ]
                      }

                    }, {
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
                  ]
                ]
              });
            } else if (req.url.match(/_api\/gharial\/myGraph\/edge/)) {
              req.success({
                error: false,
                edge: {
                  _id: '1-2'
                }
              });
            } else {
              req.success({
                vertex: {
                  _id: 1
                },
                error: false
              });
            }
            break;
          default:
            req.success();
        }
      });
      adapter = new GharialAdapter([], [], {}, {
        graphName: 'myGraph',
        prioList: ['foo', 'bar', 'baz']
      });

      return adapter;
    });

    var adapter,
      nodes,
      edges,
      viewer,
      arangodb = 'http://localhost:8529',
      nodesCollection,
      altNodesCollection,
      edgesCollection,
      altEdgesCollection,
      mockCollection,
      graphName,
      altGraphName,
      callbackCheck,
      checkCallbackFunction = function () {
        callbackCheck = true;
      },

      getCommunityNodes = function () {
        return _.filter(nodes, function (n) {
          return n._isCommunity;
        });
      },

      getCommunityNodesIds = function () {
        return _.pluck(getCommunityNodes(), '_id');
      },

      nodeWithID = function (id) {
        return $.grep(nodes, function (e) {
          return e._id === id;
        })[0];
      },
      edgeWithSourceAndTargetId = function (sourceId, targetId) {
        return $.grep(edges, function (e) {
          return e.source._id === sourceId
            && e.target._id === targetId;
        })[0];
      },
      existNode = function (id) {
        var node = nodeWithID(id);
        expect(node).toBeDefined();
        expect(node._id).toEqual(id);
      },

      notExistNode = function (id) {
        var node = nodeWithID(id);
        expect(node).toBeUndefined();
      },

      existEdge = function (source, target) {
        var edge = edgeWithSourceAndTargetId(source, target);
        expect(edge).toBeDefined();
        expect(edge.source._id).toEqual(source);
        expect(edge.target._id).toEqual(target);
      },

      notExistEdge = function (source, target) {
        var edge = edgeWithSourceAndTargetId(source, target);
        expect(edge).toBeUndefined();
      },

      existNodes = function (ids) {
        _.each(ids, existNode);
      },

      notExistNodes = function (ids) {
        _.each(ids, notExistNode);
      },

      nodeCollectionForGraph,
      edgeCollectionForGraph,

      insertEdge = function (collectionID, from, to, cont) {
        var key = String(Math.floor(Math.random() * 100000)),
          id = collectionID + '/' + key;
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
        var key = String(Math.floor(Math.random() * 100000)),
          id = collectionID + '/' + key;
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
      readNode = function (collectionID, example) {
        if (typeof example === 'string') {
          example = {_id: example};
        }
        return _.findWhere(mockCollection[collectionID], example);
      },
      constructPath = function (colNodes, colEdges, from, to) {
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

    beforeEach(function () {
      nodeCollectionForGraph = function (graph) {
        if (graph === graphName) {
          return nodesCollection;
        }
        return altNodesCollection;
      };

      edgeCollectionForGraph = function (graph) {
        if (graph === graphName) {
          return edgesCollection;
        }
        return altEdgesCollection;
      };

      nodes = [];
      edges = [];
      mockCollection = {};
      viewer = {
        cleanUp: function () {
          return undefined;
        }
      };
      nodesCollection = 'TestNodes321';
      edgesCollection = 'TestEdges321';
      altNodesCollection = 'TestNodes654';
      altEdgesCollection = 'TestEdges654';

      graphName = 'TestGraph';
      altGraphName = 'TestGraph123';

      this.addMatchers({
        toHaveCorrectCoordinates: function () {
          var list = this.actual,
            evil;
          _.each(list, function (n) {
            if (isNaN(n.x) || isNaN(n.y)) {
              evil = n;
            }
          });
          this.message = function () {
            return 'Expected ' + JSON.stringify(evil) + ' to contain Numbers as X and Y.';
          };
          return evil === undefined;
        }
      });
    });

    afterEach(function () {
      expect(nodes).toHaveCorrectCoordinates();
    });

    it('should throw an error if no nodes are given', function () {
      expect(
        function () {
          var t = new GharialAdapter();
          t.fail();
        }
      ).toThrow('The nodes have to be given.');
    });

    it('should throw an error if no edges are given', function () {
      expect(
        function () {
          var t = new GharialAdapter([]);
          t.fail();
        }
      ).toThrow('The edges have to be given.');
    });

    it('should throw an error if a reference to the graph viewer is not given', function () {
      expect(
        function () {
          var t = new GharialAdapter([], []);
          t.fail();
        }
      ).toThrow('A reference to the graph viewer has to be given.');
    });

    it('should throw an error if no config is given', function () {
      expect(
        function () {
          var t = new GharialAdapter([], [], viewer);
          return t;
        }
      ).toThrow('A configuration with graphName has to be given.');
    });

    it('should throw an error if the config does not contain graphName', function () {
      expect(
        function () {
          var t = new GharialAdapter([], [], viewer, {});
          return t;
        }
      ).toThrow('The graphname has to be given.');
    });

    it('should not throw an error if everything is given', function () {
      expect(
        function () {
          var t = new GharialAdapter([], [], viewer, {
            graphName: ''
          });
          return t;
        }
      ).not.toThrow();
    });

    it('should automatically determine the host if not given', function () {
      adapter = new GharialAdapter(
        nodes,
        edges,
        viewer,
        {
          graphName: graphName,
          width: 100,
          height: 40
        }
      );
      var args;
      spyOn($, 'ajax');
      adapter.createNode({}, function () {
        throw 'This should be a spy';
      });
      args = $.ajax.mostRecentCall.args[0];
      expect(args.url).not.toContain('http');
    });

    it('should create a nodeReducer instance', function () {
      spyOn(window, 'NodeReducer');
      adapter = new GharialAdapter(
        nodes,
        edges,
        viewer,
        {
          graphName: graphName,
          width: 100,
          height: 40
        }
      );
      expect(window.NodeReducer).wasCalledWith();
    });

    it('should create the ModularityJoiner as a worker', function () {
      spyOn(window, 'WebWorkerWrapper');
      adapter = new GharialAdapter(
        nodes,
        edges,
        viewer,
        {
          graphName: graphName,
          width: 100,
          height: 40
        }
      );
      expect(window.WebWorkerWrapper).wasCalledWith(
        window.ModularityJoiner,
        jasmine.any(Function)
      );
    });

    describe('setup correctly', function () {
      var traversalQuery,
        filterQuery,
        childrenQuery,
        loadGraph,
        requests,
        ajaxResponse,
        mockWrapper,
        workerCB;

      beforeEach(function () {
        var self = this,
          apibase = '_api/',
          apiCursor = apibase + 'cursor';
        ajaxResponse = function () {
          return undefined;
        };
        self.fakeReducerBucketRequest = function () {
          return undefined;
        };
        mockWrapper = {};
        mockWrapper.call = function () {
          return undefined;
        };
        spyOn(window, 'NodeReducer').andCallFake(function () {
          return {
            bucketNodes: function (toSort, numBuckets) {
              return self.fakeReducerBucketRequest(toSort, numBuckets);
            }
          };
        });
        spyOn(window, 'WebWorkerWrapper').andCallFake(function (c, cb) {
          workerCB = cb;
          return {
            call: function () {
              mockWrapper.call.apply(
                mockWrapper,
                Array.prototype.slice.call(arguments)
              );
            }
          };
        });

        traversalQuery = function (id, graph, undirected) {
          var dir;
          if (undirected === true) {
            dir = 'any';
          } else {
            dir = 'outbound';
          }
          return JSON.stringify({
            query: 'RETURN GRAPH_TRAVERSAL(@graph, @example, @dir,'
              + ' {strategy: "depthfirst",maxDepth: 1,paths: true})',
            bindVars: {
              example: id,
              graph: graph,
              dir: dir
            }
          });
        };
        filterQuery = function (v, graph, undirected) {
          var dir, example = {};
          example.id = v;
          if (undirected === true) {
            dir = 'any';
          } else {
            dir = 'outbound';
          }
          return JSON.stringify({
            query: 'RETURN GRAPH_TRAVERSAL(@graph, @example, @dir,'
              + ' {strategy: "depthfirst",maxDepth: 1,paths: true})',
            bindVars: {
              example: example,
              graph: graph,
              dir: dir
            }
          });
        };
        childrenQuery = function (id, graph) {
          return JSON.stringify({
            query: 'RETURN LENGTH(GRAPH_EDGES(@graph, @id, {direction: any}))',
            bindVars: {
              id: id,
              graph: graph
            }
          });
        };
        loadGraph = function (vars) {
          var example = vars.example,
            graph = vars.graph,
            ncol = nodeCollectionForGraph(graph),
            ecol = edgeCollectionForGraph(graph),
            res = [],
            inner = [],
            first = {},
            node1 = readNode(ncol, example),
            nid = node1._id;
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
            _.each(mockCollection[ecol][nid], function (val, key) {
              inner.push(constructPath(ncol, ecol, nid, key));
            });
          }
          return [res];
        };
        spyOn($, 'ajax').andCallFake(function (req) {
          var urlParts = req.url.split('/'),
            didAnswer = false;
          if (req.type === 'GET') {
            if (urlParts.length === 4) {
              if (urlParts[3] === 'edge') {
                didAnswer = true;
                req.success({
                  collections: [edgeCollectionForGraph(urlParts[2])]
                });
              } else if (urlParts[3] === 'vertex') {
                didAnswer = true;
                req.success({
                  collections: [nodeCollectionForGraph(urlParts[2])]
                });
              }
            }
          }
          if (!didAnswer) {
            ajaxResponse(req);
          }
        });

        adapter = new GharialAdapter(
          nodes,
          edges,
          viewer,
          {
            graphName: graphName,
            width: 100,
            height: 40
          }
        );

        $.ajax.reset();

        requests = {};
        requests.cursor = function (data) {
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
        requests.node = function (graph, col) {
          var read = apibase + 'gharial/' + graph + '/vertex/' + col,
            write = apibase + 'gharial/' + graph + '/vertex/',
            base = {
              cache: false,
              dataType: 'json',
              contentType: 'application/json',
              processData: false,
              success: jasmine.any(Function),
              error: jasmine.any(Function)
          };
          return {
            create: function (data) {
              return $.extend(base, {url: read, type: 'POST', data: JSON.stringify(data)});
            },
            patch: function (id, data) {
              return $.extend(base, {url: write + id, type: 'PUT', data: JSON.stringify(data)});
            },
            del: function (id) {
              return $.extend(base, {url: write + id, type: 'DELETE'});
            }
          };
        };
        requests.edge = function (graph, col) {
          var create = apibase + 'gharial/' + graph + '/edge/' + col,
            write = apibase + 'gharial/' + graph + '/edge/',
            base = {
              cache: false,
              dataType: 'json',
              contentType: 'application/json',
              processData: false,
              success: jasmine.any(Function),
              error: jasmine.any(Function)
          };
          return {
            create: function (from, to, data) {
              data._from = from;
              data._to = to;
              return $.extend(base, {
                url: create,
                type: 'POST',
                data: JSON.stringify(data)
              });
            },
            patch: function (id, data) {
              return $.extend(base, {url: write + id, type: 'PUT', data: JSON.stringify(data)});
            },
            del: function (id) {
              return $.extend(base, {url: write + id, type: 'DELETE'});
            }
          };
        };
      });

      it('should determine to store edges in lonely edge collection', function () {
        expect(adapter.getSelectedEdgeCollection()).toEqual(edgeCollectionForGraph(graphName));
      });

      it('should determine to store nodes in lonely node collection', function () {
        expect(adapter.getSelectedNodeCollection()).toEqual(nodeCollectionForGraph(graphName));
      });

      it('should know the graph name', function () {
        expect(adapter.getGraphName()).toEqual(graphName);
      });

      it('the default direction should be outbound', function () {
        expect(adapter.getDirection()).toEqual('outbound');
      });

      it('should be able to load a tree node from '
        + 'ArangoDB by internal _id attribute', function () {
          var c0, c1, c2, c3, c4;

          runs(function () {
            ajaxResponse = function (request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                request.success({result: loadGraph(vars)});
              }
            };

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

          waitsFor(function () {
            return callbackCheck;
          }, 1000);

          runs(function () {
            existNodes([c0, c1, c2, c3, c4]);
            expect(nodes.length).toEqual(5);
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(traversalQuery(c0, graphName))
            );
          });
        });

      it('should map loadNode to loadByID', function () {
        spyOn(adapter, 'loadNodeFromTreeById');
        adapter.loadNode('a', 'b');
        expect(adapter.loadNodeFromTreeById).toHaveBeenCalledWith('a', 'b');
      });

      it('should be able to load a tree node from ArangoDB'
        + ' by internal attribute and value', function () {
          var c0, c1, c2, c3, c4;

          runs(function () {
            ajaxResponse = function (request) {
              var vars = JSON.parse(request.data).bindVars;
              if (vars !== undefined) {
                vars.id = c0;
                request.success({result: loadGraph(vars)});
              }
            };

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
            adapter.loadNodeFromTreeByAttributeValue('id', 0, checkCallbackFunction);
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1000);

          runs(function () {
            existNodes([c0, c1, c2, c3, c4]);
            expect(nodes.length).toEqual(5);
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(filterQuery(0, graphName))
            );
          });
        });

      it('should callback with proper errorcode if no results are found', function () {
        var dummy = {
          cb: function () {
            throw 'Should be a spy';
          }
        };
        spyOn(dummy, 'cb');
        ajaxResponse = function (request) {
          request.success({result: []});
        };
        adapter.loadNode('node', dummy.cb);
        expect(dummy.cb).wasCalledWith({
          errorCode: 404
        });
      });

      it('should be able to request the number of children centrality', function () {
        var c0,
          children;
        runs(function () {
          c0 = insertNode(nodesCollection, 0);
          ajaxResponse = function (request) {
            request.success({result: [4]});
          };

          callbackCheck = false;
          adapter.requestCentralityChildren(c0, function (count) {
            callbackCheck = true;
            children = count;
          });
        });

        waitsFor(function () {
          return callbackCheck;
        });

        runs(function () {
          expect(children).toEqual(4);
          expect($.ajax).toHaveBeenCalledWith(
            requests.cursor(childrenQuery(c0, graphName))
          );
        });
      });

      it('should encapsulate all attributes of nodes and edges in _data', function () {
        var c0, c1, e1_2;

        runs(function () {
          ajaxResponse = function (request) {
            var vars = JSON.parse(request.data).bindVars;
            if (vars !== undefined) {
              request.success({result: loadGraph(vars)});
            }
          };

          c0 = insertNode(nodesCollection, 0, {name: 'Alice', age: 42});
          c1 = insertNode(nodesCollection, 1, {name: 'Bob', age: 1337});
          e1_2 = insertEdge(edgesCollection, c0, c1, {label: 'knows'});

          callbackCheck = false;
          adapter.loadNodeFromTreeById(c0, checkCallbackFunction);
        });

        waitsFor(function () {
          return callbackCheck;
        }, 1000);

        runs(function () {
          expect(nodes[0]._data).toEqual({
            _id: c0,
            _key: jasmine.any(String),
            _rev: jasmine.any(String),
            id: 0,
            name: 'Alice',
            age: 42
          });
          expect(nodes[1]._data).toEqual({
            _id: c1,
            _key: jasmine.any(String),
            _rev: jasmine.any(String),
            id: 1,
            name: 'Bob',
            age: 1337
          });
          expect(edges[0]._data).toEqual({
            _id: e1_2,
            _from: c0,
            _to: c1,
            _key: jasmine.any(String),
            _rev: jasmine.any(String),
            label: 'knows'
          });
          expect($.ajax).toHaveBeenCalledWith(
            requests.cursor(traversalQuery(c0, graphName))
          );
        });
      });

      it('should be able to switch to a different graph', function () {
        var c0, c1, insertedId;

        runs(function () {
          ajaxResponse = function (request) {
            var vars = JSON.parse(request.data).bindVars;
            if (vars !== undefined) {
              request.success({result: loadGraph(vars)});
            } else {
              request.success({
                error: false,
                vertex: {
                  _id: 'TestNodes654/myNewNode',
                  _key: 'myNewNode',
                  _rev: '1234'
                }
              });
            }
          };

          c0 = insertNode(altNodesCollection, 0);
          c1 = insertNode(altNodesCollection, 1);
          insertEdge(altEdgesCollection, c0, c1);

          adapter.changeToGraph(altGraphName);

          callbackCheck = false;
          adapter.loadNode(c0, checkCallbackFunction);
        });

        waitsFor(function () {
          return callbackCheck;
        }, 1000);

        runs(function () {
          existNodes([c0, c1]);
          expect(nodes.length).toEqual(2);
          expect($.ajax).toHaveBeenCalledWith(
            requests.cursor(traversalQuery(c0, altGraphName))
          );

          callbackCheck = false;
          adapter.createNode({}, function (node) {
            insertedId = node._id;
            callbackCheck = true;
          });
        });

        waitsFor(function () {
          return callbackCheck;
        }, 1500);

        runs(function () {
          existNode(insertedId);
          expect($.ajax).toHaveBeenCalledWith(
            requests.node(altGraphName, altNodesCollection).create({})
          );
        });
      });

      it('should be able to switch to a different graph and change to directed', function () {
        runs(function () {
          adapter.changeToGraph(altGraphName, false);

          adapter.loadNode('42');

          expect($.ajax).toHaveBeenCalledWith(
            requests.cursor(traversalQuery('42', altGraphName, false))
          );
        });
      });

      it('should be able to switch to a different graph'
        + ' and change to undirected', function () {
          runs(function () {
            adapter.changeToGraph(altGraphName, true);

            adapter.loadNode('42');

            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(traversalQuery('42', altGraphName, true))
            );
          });
        });

      it('should add at most the upper bound of children in one step', function () {
        var inNodeCol, callNodes;

        runs(function () {
          var addNNodes = function (n) {
              var i = 0,
                res = [];
              for (i = 0; i < n; i++) {
                res.push(insertNode(nodesCollection, i));
              }
              return res;
            },
            connectToAllButSelf = function (source, ns) {
              _.each(ns, function (target) {
                if (source !== target) {
                  insertEdge(edgesCollection, source, target);
                }
              });
          };

          inNodeCol = addNNodes(21);
          connectToAllButSelf(inNodeCol[0], inNodeCol);
          adapter.setChildLimit(5);

          ajaxResponse = function (request) {
            var vars = JSON.parse(request.data).bindVars;
            if (vars !== undefined) {
              request.success({result: loadGraph(vars)});
            }
          };

          spyOn(this, 'fakeReducerBucketRequest').andCallFake(function (ns) {
            var i = 0,
              res = [],
              pos;
            callNodes = ns;
            for (i = 0; i < 5; i++) {
              pos = i * 4;
              res.push({
                reason: {
                  type: 'similar',
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

        waitsFor(function () {
          return callbackCheck;
        }, 1000);

        runs(function () {
          var callNodesIds = _.map(callNodes, function (n) {
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

      it('should not bucket existing nodes', function () {
        var lastCallWith, n0, n1, n2, n3, n4, n5, n6;

        runs(function () {
          var connectToAllButSelf = function (source, ns) {
            _.each(ns, function (target) {
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

          ajaxResponse = function (request) {
            var vars = JSON.parse(request.data).bindVars;
            if (vars !== undefined) {
              request.success({result: loadGraph(vars)});
            }
          };

          spyOn(this, 'fakeReducerBucketRequest').andCallFake(function (ns) {
            lastCallWith = _.pluck(ns, '_id');
            return [
              {
                reason: {
                  type: 'similar',
                  example: ns[0]
                },
                nodes: [ns[0]]
              },
              {
                reason: {
                  type: 'similar',
                  example: ns[1]
                },
                nodes: [ns[1], ns[2]]
              }
            ];
          });

          callbackCheck = false;
          adapter.loadNodeFromTreeById(n0, checkCallbackFunction);
        });

        waitsFor(function () {
          return callbackCheck;
        }, 1000);

        runs(function () {
          expect(lastCallWith).toEqual([n1, n2, n3]);

          expect(getCommunityNodes().length).toEqual(1);
          callbackCheck = false;
          adapter.loadNodeFromTreeById(n1, checkCallbackFunction);
        });

        waitsFor(function () {
          return callbackCheck;
        }, 1000);

        runs(function () {
          expect(lastCallWith).toEqual([n4, n5, n6]);

          expect(getCommunityNodes().length).toEqual(2);
        });
      });

      it('should not replace single nodes by communities', function () {
        var inNodeCol;

        runs(function () {
          var addNNodes = function (n) {
              var i = 0,
                res = [];
              for (i = 0; i < n; i++) {
                res.push(insertNode(nodesCollection, i));
              }
              return res;
            },
            connectToAllButSelf = function (source, ns) {
              _.each(ns, function (target) {
                if (source !== target) {
                  insertEdge(edgesCollection, source, target);
                }
              });
          };

          inNodeCol = addNNodes(7);
          connectToAllButSelf(inNodeCol[0], inNodeCol);
          adapter.setChildLimit(5);

          ajaxResponse = function (request) {
            var vars = JSON.parse(request.data).bindVars;
            if (vars !== undefined) {
              request.success({result: loadGraph(vars)});
            }
          };

          spyOn(this, 'fakeReducerBucketRequest').andCallFake(function (ns) {
            var i = 0,
              res = [];
            for (i = 0; i < 4; i++) {
              res.push({
                reason: {
                  type: 'similar',
                  example: ns[i]
                },
                nodes: [ns[i]]
              });
            }
            res.push({
              reason: {
                type: 'similar',
                example: ns[4]
              },
              nodes: [ns[4], ns[5]]
            });
            return res;
          });

          callbackCheck = false;
          adapter.loadNodeFromTreeById(inNodeCol[0], checkCallbackFunction);
        });

        waitsFor(function () {
          return callbackCheck;
        }, 1000);

        runs(function () {
          expect(this.fakeReducerBucketRequest).toHaveBeenCalledWith(
            jasmine.any(Array),
            5
          );
          expect(nodes.length).toEqual(6);
          expect(getCommunityNodes().length).toEqual(1);
        });
      });

      describe('that has already loaded one graph', function () {
        var c0, c1, c2, c3, c4, c5, c6, c7,
          fakeResult, spyHook;

        beforeEach(function () {
          runs(function () {
            ajaxResponse = function (request) {
              if (spyHook !== undefined) {
                if (!spyHook(request)) {
                  return;
                }
              }
              if (request.url.indexOf('cursor', request.url.length - 'cursor'.length) !== -1) {
                var vars = JSON.parse(request.data).bindVars;
                if (vars !== undefined) {
                  request.success({result: loadGraph(vars)});
                }
              } else {
                request.success(fakeResult);
              }
            };

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
              toBeStoredPermanently: function () {
                var id = this.actual,
                  res = false;
                $.ajax({
                  type: 'GET',
                  url: arangodb + '/_api/document/' + id,
                  contentType: 'application/json',
                  processData: false,
                  async: false,
                  success: function () {
                    res = true;
                  },
                  error: function (data) {
                    try {
                      var temp = JSON.parse(data);
                      throw '[' + temp.errorNum + '] ' + temp.errorMessage;
                    } catch (e) {
                      throw 'Undefined ERROR';
                    }
                  }
                });
                return res;
              },

              toNotBeStoredPermanently: function () {
                var id = this.actual,
                  res = false;
                $.ajax({
                  type: 'GET',
                  url: arangodb + '/_api/document/' + id,
                  contentType: 'application/json',
                  processData: false,
                  async: false,
                  success: function () {
                    return undefined;
                  },
                  error: function (data) {
                    if (data.status === 404) {
                      res = true;
                    }
                  }
                });
                return res;
              },

              toHavePermanentAttributeWithValue: function (attribute, value) {
                var id = this.actual,
                  res = false;
                $.ajax({
                  type: 'GET',
                  url: arangodb + '/_api/document/' + id,
                  contentType: 'application/json',
                  processData: false,
                  async: false,
                  success: function (data) {
                    if (data[attribute] === value) {
                      res = true;
                    }
                  },
                  error: function () {
                    return undefined;
                  }
                });
                return res;
              }
            });
          });

          waitsFor(function () {
            return callbackCheck;
          });

          runs(function () {
            callbackCheck = false;
          });
        });

        it('should be able to add nodes from another query', function () {
          runs(function () {
            adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1000);

          runs(function () {
            existNodes([c0, c1, c2, c3, c4, c5, c6, c7]);
            expect(nodes.length).toEqual(8);
            expect($.ajax).toHaveBeenCalledWith(
              requests.cursor(traversalQuery(c1, graphName))
            );
          });
        });

        it('should be able to change a value of one node permanently', function () {
          var toPatch;

          runs(function () {
            fakeResult = {
              error: false,
              vertex: {
                hello: 'world'
              }
            };
            toPatch = nodeWithID(c0);
            adapter.patchNode(toPatch, {hello: 'world'}, checkCallbackFunction);
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1000);

          runs(function () {
            expect(toPatch._data.hello).toEqual('world');
            expect($.ajax).toHaveBeenCalledWith(
              requests.node(graphName, nodesCollection).patch(c0, fakeResult.vertex)
            );
          });
        });

        it('should be able to change a value of one edge permanently', function () {
          var toPatch;

          runs(function () {
            fakeResult = {
              error: false,
              edge: {hello: 'world'}
            };
            toPatch = edgeWithSourceAndTargetId(c0, c1);
            adapter.patchEdge(toPatch, {hello: 'world'}, checkCallbackFunction);
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1000);

          runs(function () {
            expect(toPatch._data.hello).toEqual('world');
            expect($.ajax).toHaveBeenCalledWith(
              requests.edge(graphName, edgesCollection).patch(toPatch._id, fakeResult.edge)
            );
          });
        });

        it('should be able to remove an edge permanently', function () {
          var toDelete;

          runs(function () {
            fakeResult = '';
            toDelete = edgeWithSourceAndTargetId(c0, c4);
            existEdge(c0, c4);
            adapter.deleteEdge(toDelete, checkCallbackFunction);
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1000);

          runs(function () {
            expect($.ajax).toHaveBeenCalledWith(
              requests.edge(graphName, edgesCollection).del(toDelete._id)
            );
            notExistEdge(c0, c4);
          });
        });

        it('should be able to add a node permanently', function () {
          var insertedId;

          runs(function () {
            fakeResult = {
              error: false,
              vertex: {
                _id: 'TestNodes123/MyNode',
                _rev: '1234',
                _key: 'MyNode'
              }
            };
            adapter.createNode({}, function (node) {
              insertedId = node._id;
              callbackCheck = true;
            });
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1000);

          runs(function () {
            expect($.ajax).toHaveBeenCalledWith(
              requests.node(graphName, nodesCollection).create({})
            );

            existNode(insertedId);
          });
        });

        it('should trigger the reducer if too many nodes are added', function () {
          runs(function () {
            adapter.setNodeLimit(6);
            spyOn(mockWrapper, 'call').andCallFake(function (name) {
              workerCB({
                data: {
                  cmd: name,
                  result: [c0]
                }
              });
            });
            adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
            expect(mockWrapper.call).toHaveBeenCalledWith('getCommunity', 6, c1);
          });
        });

        describe('checking community nodes', function () {
          it('should not trigger the reducer if the limit is set large enough', function () {
            spyOn(mockWrapper, 'call').andCallFake(function (name) {
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

          it('should trigger the reducer if the limit is set too small', function () {
            spyOn(mockWrapper, 'call').andCallFake(function (name) {
              workerCB({
                data: {
                  cmd: name,
                  result: [c0]
                }
              });
            });
            adapter.setNodeLimit(2);
            expect(mockWrapper.call).toHaveBeenCalledWith('getCommunity', 2);
          });

          it('should create a community node if limit is set too small', function () {
            runs(function () {
              callbackCheck = false;
              spyOn(mockWrapper, 'call').andCallFake(function (name) {
                workerCB({
                  data: {
                    cmd: name,
                    result: [c0, c1, c2]
                  }
                });
              });
              adapter.setNodeLimit(2, checkCallbackFunction);
            });

            waitsFor(function () {
              return callbackCheck;
            }, 1000);

            runs(function () {
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

          it('should create a community node if too many nodes are added', function () {
            runs(function () {
              adapter.setNodeLimit(6);
              spyOn(mockWrapper, 'call').andCallFake(function (name) {
                workerCB({
                  data: {
                    cmd: name,
                    result: [c0, c1, c2, c3]
                  }
                });
              });
              adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
            });

            waitsFor(function () {
              return callbackCheck;
            }, 1000);

            runs(function () {
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

          describe('expanding after a while', function () {
            it('should connect edges of internal nodes accordingly', function () {
              var commNode, called, counterCallback,
                v0, v1, v2, v3, v4;

              runs(function () {
                var v = 'vertices',
                  e = 'edges',
                  g = 'graph';
                nodes.length = 0;
                edges.length = 0;
                v0 = insertNode(v, 0);
                v1 = insertNode(v, 1);
                v2 = insertNode(v, 2);
                v3 = insertNode(v, 3);
                v4 = insertNode(v, 4);
                insertEdge(e, v0, v1);
                insertEdge(e, v0, v2);
                insertEdge(e, v1, v3);
                insertEdge(e, v1, v4);
                insertEdge(e, v2, v3);
                insertEdge(e, v2, v4);
                nodeCollectionForGraph = function () {
                  return v;
                };
                edgeCollectionForGraph = function () {
                  return e;
                };
                called = 0;
                counterCallback = function () {
                  called++;
                };
                spyOn(mockWrapper, 'call').andCallFake(function (name) {
                  workerCB({
                    data: {
                      cmd: name,
                      result: [v1, v3, v4]
                    }
                  });
                });
                adapter.setNodeLimit(3);
                adapter.changeToGraph(g);
                adapter.loadNode(v0, counterCallback);
                adapter.loadNode(v1, counterCallback);
              });

              waitsFor(function () {
                return called === 2;
              }, 1000);

              runs(function () {
                adapter.loadNode(v2, counterCallback);
                commNode = getCommunityNodes()[0];
              });

              waitsFor(function () {
                return called === 3;
              }, 1000);

              runs(function () {
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

              waitsFor(function () {
                return called === 4;
              }, 1000);

              runs(function () {
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

            it('set inbound and outboundcounter correctly', function () {
              var commNode, called, counterCallback,
                v0, v1, v2, v3, v4;

              runs(function () {
                var v = 'vertices',
                  e = 'edges',
                  g = 'graph';
                nodes.length = 0;
                edges.length = 0;
                v0 = insertNode(v, 0);
                v1 = insertNode(v, 1);
                v2 = insertNode(v, 2);
                v3 = insertNode(v, 3);
                v4 = insertNode(v, 4);
                insertEdge(e, v0, v1);
                insertEdge(e, v0, v2);
                insertEdge(e, v1, v3);
                insertEdge(e, v1, v4);
                insertEdge(e, v2, v3);
                insertEdge(e, v2, v4);
                called = 0;
                counterCallback = function () {
                  called++;
                };
                nodeCollectionForGraph = function () {
                  return v;
                };
                edgeCollectionForGraph = function () {
                  return e;
                };
                spyOn(mockWrapper, 'call').andCallFake(function (name) {
                  workerCB({
                    data: {
                      cmd: name,
                      result: [v1, v3, v4]
                    }
                  });
                });
                adapter.setNodeLimit(3);

                adapter.changeToGraph(g);
                adapter.loadNode(v0, counterCallback);
                adapter.loadNode(v1, counterCallback);
              });

              waitsFor(function () {
                return called === 2;
              }, 1000);

              runs(function () {
                adapter.loadNode(v2, counterCallback);
                commNode = getCommunityNodes()[0];
              });

              waitsFor(function () {
                return called === 3;
              }, 1000);

              runs(function () {
                adapter.setNodeLimit(20);
                adapter.expandCommunity(commNode, counterCallback);
              });

              waitsFor(function () {
                return called === 4;
              }, 1000);

              runs(function () {
                var checkNodeWithInAndOut = function (id, inbound, outbound) {
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

          describe('that displays a community node already', function () {
            var firstCommId;

            beforeEach(function () {
              runs(function () {
                callbackCheck = false;
                adapter.setNodeLimit(7);
                fakeResult = [c0, c2];
                spyOn(mockWrapper, 'call').andCallFake(function (name) {
                  workerCB({
                    data: {
                      cmd: name,
                      result: fakeResult
                    }
                  });
                });
                adapter.loadNodeFromTreeById(c1, checkCallbackFunction);
              });

              waitsFor(function () {
                return callbackCheck;
              });

              runs(function () {
                firstCommId = getCommunityNodesIds()[0];
              });
            });

            it('should expand a community if enough space is available', function () {
              runs(function () {
                adapter.setNodeLimit(10);
                callbackCheck = false;
                adapter.expandCommunity(nodeWithID(firstCommId), checkCallbackFunction);
              });

              waitsFor(function () {
                return callbackCheck;
              });

              runs(function () {
                expect(getCommunityNodes().length).toEqual(1);
                existNodes([firstCommId, c1, c3, c4, c5, c6, c7]);
                existEdge(firstCommId, c1);
                existEdge(firstCommId, c3);
                existEdge(firstCommId, c4);
              });
            });

            it('should expand a community and join another '
              + 'one if not enough space is available', function () {
                runs(function () {
                  fakeResult = [c1, c7];
                  callbackCheck = false;
                  adapter.expandCommunity(nodeWithID(firstCommId), checkCallbackFunction);
                });

                waitsFor(function () {
                  return callbackCheck;
                }, 1000);

                runs(function () {
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

            it('should join another community if space is further reduced', function () {
              runs(function () {
                fakeResult = [c1, c7];
                callbackCheck = false;
                adapter.setNodeLimit(6, checkCallbackFunction);
              });

              waitsFor(function () {
                return callbackCheck;
              });

              runs(function () {
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

            it('should connect edges to internal nodes', function () {
              runs(function () {
                insertEdge(edgesCollection, c3, c0);

                adapter.setNodeLimit(20);
                callbackCheck = false;
                adapter.loadNode(c3, checkCallbackFunction);
              });

              waitsFor(function () {
                return callbackCheck;
              }, 1000);

              runs(function () {
                existEdge(c3, firstCommId);
              });
            });
          });
        });

        describe('that has loaded several queries', function () {
          var c8, c9, e2_8;

          beforeEach(function () {
            runs(function () {
              c8 = insertNode(nodesCollection, 8);
              c9 = insertNode(nodesCollection, 9);

              e2_8 = insertEdge(edgesCollection, c2, c8);
              insertEdge(edgesCollection, c3, c8);
              insertEdge(edgesCollection, c3, c9);

              callbackCheck = false;
              adapter.loadNodeFromTreeById(c2, checkCallbackFunction);
            });

            waitsFor(function () {
              return callbackCheck;
            });

            runs(function () {
              callbackCheck = false;
            });
          });

          it('should not add a node to the list twice', function () {
            runs(function () {
              adapter.loadNodeFromTreeById(c3, checkCallbackFunction);
            });

            waitsFor(function () {
              return callbackCheck;
            }, 1000);

            runs(function () {
              existNodes([c0, c1, c2, c3, c4, c8, c9]);
              expect(nodes.length).toEqual(7);
            });
          });

          it('should be able to add an edge permanently', function () {
            var insertedId,
              source,
              target,
              insertedEdge;

            runs(function () {
              source = nodeWithID(c0);
              target = nodeWithID(c8);
              fakeResult = {
                error: false,
                edge: {
                  _id: edgesCollection + '/123',
                  _key: '123',
                  _rev: '123',
                  _from: source._id,
                  _to: target._id
                }
              };
              var edgeInfo = {source: source, target: target};
              adapter.createEdge(edgeInfo, function (edge) {
                insertedId = edge._id;
                callbackCheck = true;
                insertedEdge = edge;
              });
            });

            waitsFor(function () {
              return callbackCheck;
            }, 1000);

            runs(function () {
              expect($.ajax).toHaveBeenCalledWith(
                requests.edge(graphName, edgesCollection).create(source._id, target._id, {})
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

          it('should be able to remove a node and all connected edges permanently', function () {
            var toDelete;
            runs(function () {
              spyHook = function (request) {
                if (request.data !== undefined) {
                  request.success({result: [
                      {_id: e2_8}
                  ]});
                  return false;
                }
                return true;
              };
              fakeResult = '';
              toDelete = nodeWithID(c2);
              adapter.deleteNode(toDelete, checkCallbackFunction);
            });

            waits(2000);

            runs(function () {
              expect($.ajax).toHaveBeenCalledWith(
                requests.node(graphName, nodesCollection).del(toDelete._id)
              );
              notExistNode(c2);
              expect($.ajax).not.toHaveBeenCalledWith(
                requests.edge(graphName, edgesCollection).del(e2_8)
              );
              notExistEdge(c2, c8);
            });
          });
        });
      });
    });

    describe('setup with a multi-collection graph', function () {
      var vc1, vc2, vc3, ec1, ec2, ec3, gn,
        requests, ajaxResponse, vertexCollections, edgeCollections;

      beforeEach(function () {
        var apibase = '_api/';
        requests = {};
        requests.node = function (graph, col) {
          var read = apibase + 'gharial/' + graph + '/vertex/' + col,
            write = apibase + 'gharial/' + graph + '/vertex/',
            base = {
              cache: false,
              dataType: 'json',
              contentType: 'application/json',
              processData: false,
              success: jasmine.any(Function),
              error: jasmine.any(Function)
          };
          return {
            create: function (data) {
              return $.extend(base, {url: read, type: 'POST', data: JSON.stringify(data)});
            },
            patch: function (id, data) {
              return $.extend(base, {url: write + id, type: 'PUT', data: JSON.stringify(data)});
            },
            del: function (id) {
              return $.extend(base, {url: write + id, type: 'DELETE'});
            }
          };
        };
        requests.edge = function (graph, col) {
          var create = apibase + 'gharial/' + graph + '/edge/' + col,
            write = apibase + 'gharial/' + graph + '/edge/',
            base = {
              cache: false,
              dataType: 'json',
              contentType: 'application/json',
              processData: false,
              success: jasmine.any(Function),
              error: jasmine.any(Function)
          };
          return {
            create: function (from, to, data) {
              data._from = from;
              data._to = to;
              return $.extend(base, {
                url: create,
                type: 'POST',
                data: JSON.stringify(data)
              });
            },
            patch: function (id, data) {
              return $.extend(base, {url: write + id, type: 'PUT', data: JSON.stringify(data)});
            },
            del: function (id) {
              return $.extend(base, {url: write + id, type: 'DELETE'});
            }
          };
        };

        gn = 'multiGraph';
        vc1 = 'vertex1';
        vc2 = 'vertex2';
        vc3 = 'vertex3';
        ec1 = 'edge1';
        ec2 = 'edge2';
        ec3 = 'edge3';
        ajaxResponse = function () {
          return undefined;
        };
        vertexCollections = [vc1, vc2, vc3];
        edgeCollections = [ec1, ec2, ec3];

        spyOn($, 'ajax').andCallFake(function (req) {
          var urlParts = req.url.split('/'),
            didAnswer = false;
          if (req.type === 'GET') {
            if (urlParts.length === 4) {
              if (urlParts[3] === 'edge') {
                didAnswer = true;
                req.success({
                  collections: edgeCollections
                });
              } else if (urlParts[3] === 'vertex') {
                didAnswer = true;
                req.success({
                  collections: vertexCollections
                });
              }
            }
          }
          if (!didAnswer) {
            ajaxResponse(req);
          }
        });

        adapter = new GharialAdapter([{
          _id: 's'
        },
          {
            _id: 't'
          }], [], {}, {
          graphName: gn
        });
      });

      describe('vertex collections', function () {
        it('should offer the available list', function () {
          var list = adapter.getNodeCollections();
          expect(list).toEqual(vertexCollections);
        });

        it('should use the first as default', function () {
          runs(function () {
            callbackCheck = false;
            ajaxResponse = function (req) {
              if (req.type === 'POST') {
                req.success({
                  code: 200,
                  error: false,
                  vertex: {
                    _id: '123'
                  }
                });
              }
            };
            adapter.createNode({}, function () {
              callbackCheck = true;
            });
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1500);

          runs(function () {
            expect($.ajax).toHaveBeenCalledWith(
              requests.node(gn, vertexCollections[0]).create({})
            );
          });
        });

        it('should switch between collections', function () {
          runs(function () {
            callbackCheck = false;
            adapter.useNodeCollection(vc3);
            ajaxResponse = function (req) {
              if (req.type === 'POST') {
                req.success({
                  code: 200,
                  error: false,
                  vertex: {
                    _id: '123'
                  }
                });
              }
            };
            adapter.createNode({}, function () {
              callbackCheck = true;
            });
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1500);

          runs(function () {
            expect($.ajax).toHaveBeenCalledWith(
              requests.node(gn, vc3).create({})
            );
          });
        });

        it('should throw an error for unknwon collections', function () {
          expect(function () {
            adapter.useNodeCollection('unknown');
          }).toThrow('Collection unknown is not available in the graph.');
        });
      });

      describe('edge collections', function () {
        it('should offer the available list', function () {
          var list = adapter.getEdgeCollections();
          expect(list).toEqual(edgeCollections);
        });

        it('should use the first as default', function () {
          var edgeInfo;

          runs(function () {
            callbackCheck = false;
            edgeInfo = {source: {_id: 's'}, target: {_id: 't'}};
            ajaxResponse = function (req) {
              if (req.type === 'POST') {
                var body = JSON.parse(req.data);
                req.success({
                  code: 200,
                  error: false,
                  edge: {
                    _id: '123',
                    _from: body._from,
                    _to: body._to
                  }
                });
              }
            };
            adapter.createEdge(edgeInfo, function () {
              callbackCheck = true;
            });
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1500);

          runs(function () {
            expect($.ajax).toHaveBeenCalledWith(
              requests.edge(gn, edgeCollections[0]).create('s', 't', {})
            );
          });
        });

        it('should switch between collections', function () {
          var edgeInfo;

          runs(function () {
            callbackCheck = false;
            adapter.useEdgeCollection(ec3);
            edgeInfo = {source: {_id: 's'}, target: {_id: 't'}};
            ajaxResponse = function (req) {
              if (req.type === 'POST') {
                var body = JSON.parse(req.data);
                req.success({
                  code: 200,
                  error: false,
                  edge: {
                    _id: '123',
                    _from: body._from,
                    _to: body._to
                  }
                });
              }
            };
            adapter.createEdge(edgeInfo, function () {
              callbackCheck = true;
            });
          });

          waitsFor(function () {
            return callbackCheck;
          }, 1500);

          runs(function () {
            expect($.ajax).toHaveBeenCalledWith(
              requests.edge(gn, ec3).create('s', 't', {})
            );
          });
        });

        it('should throw an error for unknwon collections', function () {
          expect(function () {
            adapter.useEdgeCollection('unknown');
          }).toThrow('Collection unknown is not available in the graph.');
        });
      });
    });
  });
}());
