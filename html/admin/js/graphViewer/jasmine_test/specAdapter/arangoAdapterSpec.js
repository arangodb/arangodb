/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global beforeEach, afterEach */
/*global describe, it, expect */
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
    
    describeInterface(new ArangoAdapter("", [], [], "", "", 1, 1));
    
    var adapter,
      nodes,
      edges,
      arangodb,
      nodesCollection,
      nodesCollId,
      edgesCollection,
      edgesCollId,
      c0,
      c1,
      c2,
      c3,
      c4,
      c5,
      c6,
      c7,
      c8,
      c9,
      c10,
      c11,
      c12,
      c42,
      c43,
      c44,
      c45,
      delTriple1,
      delTriple2,
      delTriple3,
      e1_5,
      e2_8,
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
      
      createCollection = function(name, type, callback) {
        if (type !== "Document" && type !== "Edge") {
          throw "Unsupported type";
        }
        var data = {
          "name": name,
          "type": type,
          "journalSize": 1048576
        };
        $.ajax({
          cache: false,
          type: "POST",
          url: arangodb + "/_api/collection",
          data: JSON.stringify(data),
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            callback(data.id);
          },
          error: function(data) {
            throw data.statusText;
          }
        });
      },
      
      dropCollection = function(id) {
        $.ajax({
          cache: false,
          type: 'DELETE',
          url: arangodb + "/_api/collection/" + id,
          async: false,
          success: function (data) {
            
          },
          error: function (data) {
            throw data.statusText;
          }
        });
      },
      insertEdge = function (collectionID, from, to) {
        var id;
        $.ajax({
          cache: false,
          type: "POST",
          async: false,
          url: arangodb + "/_api/edge?collection=" + collectionID + "&from=" + from + "&to=" + to,
          data: JSON.stringify({}),
          contentType: "application/json",
          processData: false,
          success: function(data) {
            id = data._id;
          },
          error: function(data) {
            throw data.statusText;
          }
        });
        return id;
      },
      insertNode = function (collectionID, nodeId) {
        var id;
        $.ajax({
          cache: false,
          type: "POST",
          async: false,
          url: arangodb + "/_api/document?collection=" + collectionID,
          data: JSON.stringify({id: nodeId}),
          contentType: "application/json",
          processData: false,
          success: function(data) {
            id = data._id;
          },
          error: function(data) {
            throw data.statusText;
          }
        });
        return id;
      },
      
      deleteArangoContent = function() {
        try {
          dropCollection(nodesCollection);
          dropCollection(edgesCollection);
        }catch(e){
          
        }
      },
      
      setupArangoContent = function() {
        nodesCollection = "TestNodes321";
        edgesCollection = "TestEdges321";
        
        deleteArangoContent();
        
        createCollection(nodesCollection, "Document", function(id) {nodesCollId = id;});
        createCollection(edgesCollection, "Edge", function(id) {edgesCollId = id;});
      };
    
    beforeEach(function() {
      
      arangodb = "http://localhost:8529";
      setupArangoContent();
      nodes = [];
      edges = [];
      adapter = new ArangoAdapter(
        arangodb,
        nodes,
        edges,
        nodesCollection,
        edgesCollection,
        100,
        40);
    });
    
    afterEach(function() {
      
    });
    
    it('should be able to load a tree node from ArangoDB by internal _id attribute', function() {
      
      var c0,
      c1,
      c2,
      c3,
      c4;
      
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
        adapter.loadNodeFromTreeById(c0, checkCallbackFunction);
      });
      
      waitsFor(function() {
        return callbackCheck;
      });
      
      runs(function() {
        existNodes([c0, c1, c2, c3, c4]);
        expect(nodes.length).toEqual(5);
      });
    });
    
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
          expect(toPatch.hello).toEqual("world");
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
          expect(toPatch.hello).toEqual("world");
          expect(toPatch._id).toHavePermanentAttributeWithValue("hello", "world");
        });
      });
      
      it('should be able to remove an edge permanently', function() {
        
        var toDelete;
        
        runs(function() {
          toDelete = edgeWithSourceAndTargetId(c0, c4);
          adapter.deleteEdge(toDelete, checkCallbackFunction);
        });
        
        waitsFor(function() {
          return callbackCheck;
        });
        
        runs(function() {
          expect(toDelete._id).toNotBeStoredPermanently();
          notExistEdge(c1, c5);
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
        var c8, c9;
      
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
          target;
        
        
          runs(function() {
            source = nodeWithID(c0);
            target = nodeWithID(c8);
            adapter.createEdge({source: source, target: target}, function(edge) {
              insertedId = edge._id;
              callbackCheck = true;
            });
          });
        
          waitsFor(function() {
            return callbackCheck;
          });
        
          runs(function() {
            expect(insertedId).toBeStoredPermanently();
            existEdge(source._id, target._id);
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
   
  });
  
  
  
}());