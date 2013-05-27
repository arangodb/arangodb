/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, d3, _, console, document*/
/*global NodeReducer*/
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

function ArangoAdapter(nodes, edges, config) {
  "use strict";
  
  if (nodes === undefined) {
    throw "The nodes have to be given.";
  }
  if (edges === undefined) {
    throw "The edges have to be given.";
  }
  if (config === undefined) {
    throw "A configuration with node- and edgeCollection has to be given.";
  }
  if (config.nodeCollection === undefined) {
    throw "The nodeCollection has to be given.";
  }
  if (config.edgeCollection === undefined) {
    throw "The edgeCollection has to be given.";
  }
  
  var self = this,
    initialX = {},
    initialY = {},
    api = {},
    queries = {},
    cachedCommunities = {},
    joinedInCommunities = {},
    nodeCollection,
    edgeCollection,
    limit,
    childLimit,
    reducer,
    arangodb,
    width,
    height,
    direction,
    
    setWidth = function(w) {
      initialX.range = w / 2;
      initialX.start = w / 4;
      initialX.getStart = function () {
        return this.start + Math.random() * this.range;
      };
    },
    
    setHeight = function(h) {
      initialY.range = h / 2;
      initialY.start = h / 4;
      initialY.getStart = function () {
        return this.start + Math.random() * this.range;
      };
    },
    
    parseConfig = function(config) {
      initialX.getStart = function() {return 0;};
      initialY.getStart = function() {return 0;};
      nodeCollection = config.nodeCollection;
      edgeCollection = config.edgeCollection;
      if (config.host === undefined) {
        arangodb = "http://" + document.location.host;
      } else {
        arangodb = config.host;
      }
      if (config.width !== undefined) {
        setWidth(config.width);
      }
      if (config.height !== undefined) {
        setHeight(config.height);
      }
      if (config.undirected !== undefined) {
        if (config.undirected === true) {
          direction = "any";
        } else {
          direction = "outbound";
        }
      } else {
        direction = "outbound";
      }
    },
  
    findNode = function(id) {
      var intId = joinedInCommunities[id] || id,
        res = $.grep(nodes, function(e){
          return e._id === intId;
        });
      if (res.length === 0) {
        return false;
      } 
      if (res.length === 1) {
        return res[0];
      }
      throw "Too many nodes with the same ID, should never happen";
    },
  
    findEdge = function(id) {
      var res = $.grep(edges, function(e){
        return e._id === id;
      });
      if (res.length === 0) {
        return false;
      } 
      if (res.length === 1) {
        return res[0];
      }
      throw "Too many edges with the same ID, should never happen";
    },
  
    insertNode = function(data) {
      var node = {
        _data: data,
        _id: data._id
      },
        n = findNode(node._id);
      if (n) {
        return n;
      }
      node.x = initialX.getStart();
      node.y = initialY.getStart();
      nodes.push(node);
      node._outboundCounter = 0;
      node._inboundCounter = 0;
      return node;
    },
  
    insertEdge = function(data) {
      var source,
        target,
        edge = {
          _data: data,
          _id: data._id
        },
        e = findEdge(edge._id),
        edgeToPush;
      if (e) {
        return e;
      }
      source = findNode(data._from);
      target = findNode(data._to);
      if (!source) {
        throw "Unable to insert Edge, source node not existing " + edge._from;
      }
      if (!target) {
        throw "Unable to insert Edge, target node not existing " + edge._to;
      }
      edge.source = source;
      edge.target = target;
      edges.push(edge);
      
      
      if (cachedCommunities[source._id] !== undefined) {
        edgeToPush = {};
        edgeToPush.type = "s";
        edgeToPush.id = edge._id;
        edgeToPush.source = $.grep(cachedCommunities[source._id].nodes, function(e){
          return e._id === data._from;
        })[0];
        edgeToPush.source._outboundCounter++;
        cachedCommunities[source._id].edges.push(edgeToPush);
      } else {
        source._outboundCounter++;
      }
      if (cachedCommunities[target._id] !== undefined) {
        edgeToPush = {};
        edgeToPush.type = "t";
        edgeToPush.id = edge._id;
        edgeToPush.target = $.grep(cachedCommunities[target._id].nodes, function(e){
          return e._id === data._to;
        })[0];
        edgeToPush.target._inboundCounter++;
        cachedCommunities[target._id].edges.push(edgeToPush);
      } else {
        target._inboundCounter++;
      }
      return edge;
    },
  
    removeNode = function (node) {
      var i;
      for ( i = 0; i < nodes.length; i++ ) {
        if ( nodes[i] === node ) {
          nodes.splice( i, 1 );
          return;
        }
      }
    },
  
    removeEdge = function (edge) {
      var i;
      for ( i = 0; i < edges.length; i++ ) {
        if ( edges[i] === edge ) {
          edges.splice( i, 1 );
          return;
        }
      }
    },
  
    removeEdgesForNode = function (node) {
      var i;
      for (i = 0; i < edges.length; i++ ) {
        if (edges[i].source === node) {
          node._outboundCounter--;
          edges[i].target._inboundCounter--;
          edges.splice( i, 1 );
          i--;
        } else if (edges[i].target === node) {
          node._inboundCounter--;
          edges[i].source._outboundCounter--;
          edges.splice( i, 1 );
          i--;
        }
      }
    },
  
    combineCommunityEdges = function (nodes, commNode) {
      var i, j, s, t,
        cachedCommEdges = cachedCommunities[commNode._id].edges,
        edgeToPush;
      for (i = 0; i < edges.length; i++ ) {
        edgeToPush = {};
        // s and t keep old values yay!
        s = edges[i].source;
        t = edges[i].target;
        for (j = 0; j < nodes.length; j++) {
          if (s === nodes[j]) {
            if (edgeToPush.type !== undefined) {
              edges[i].target = edgeToPush.target;
              delete edgeToPush.target;
              edgeToPush.type = "b";
              edgeToPush.edge = edges[i];
              edges.splice( i, 1 );
              i--;
              break;
            }
            edges[i].source = commNode;
            edgeToPush.type = "s";
            edgeToPush.id = edges[i]._id;
            edgeToPush.source = s;
          }
          if (t === nodes[j]) {
            if (edgeToPush.type !== undefined) {
              edges[i].source = edgeToPush.source;
              delete edgeToPush.source;
              edgeToPush.type = "b";
              edgeToPush.edge = edges[i];
              edges.splice( i, 1 );
              i--;
              break;
            }
            edges[i].target = commNode;
            edgeToPush.type = "t";
            edgeToPush.id = edges[i]._id;
            edgeToPush.target = t;
          }
        }
        if (edgeToPush.type !== undefined) {
          cachedCommEdges.push(edgeToPush);
        }
      }
    },
  
    // Helper function to easily remove all outbound edges for one node
    removeOutboundEdgesFromNode = function ( node ) {
      if (node._outboundCounter > 0) {
        var removed = [],
        i;
        for ( i = 0; i < edges.length; i++ ) {
          if ( edges[i].source === node ) {
            removed.push(edges[i]);
            node._outboundCounter--;
            edges[i].target._inboundCounter--;
            edges.splice( i, 1 );
            if (node._outboundCounter === 0) {
              break;
            }
            i--;
          }
        }
        return removed;
      }
    },
  
  
    sendQuery = function(query, bindVars, onSuccess) {
      if (query !== queries.connectedEdges) {
        bindVars["@nodes"] = nodeCollection;
      }
      if (query !== queries.childrenCentrality) {
        bindVars.dir = direction;
      }
      bindVars["@edges"] = edgeCollection;
      var data = {
        query: query,
        bindVars: bindVars
      };
      $.ajax({
        type: "POST",
        url: api.cursor,
        data: JSON.stringify(data),
        contentType: "application/json",
        dataType: "json",
        processData: false,
        success: function(data) {
          onSuccess(data.result);
        },
        error: function(data) {
          try {
            console.log(data.statusText);
            throw "[" + data.errorNum + "] " + data.errorMessage;
          }
          catch (e) {
            console.log(e);
            throw "Undefined ERROR";
          }
        }
      });
    },
    
    collapseCommunity = function (community) {
      var commId = "*community_" + Math.floor(Math.random()* 1000000),
        commNode = {
          _id: commId,
          edges: []
        },
        nodesToRemove = _.map(community, function(id) {
          return findNode(id);
        });
      commNode.x = nodesToRemove[0].x;
      commNode.y = nodesToRemove[0].y;
      cachedCommunities[commId] = {};
      cachedCommunities[commId].nodes = nodesToRemove;
      cachedCommunities[commId].edges = [];
      
      combineCommunityEdges(nodesToRemove, commNode);
      _.each(nodesToRemove, function(n) {
        joinedInCommunities[n._id] = commId;
        removeNode(n);
      });
      nodes.push(commNode);
    },
  
    expandCommunity = function (commNode) {
      var commId = commNode._id,
        nodesToAdd = cachedCommunities[commId].nodes,
        edgesToChange = cachedCommunities[commId].edges,
        com;
      removeNode(commNode);
      if (limit < nodes.length + nodesToAdd.length) {
        com = reducer.getCommunity(limit);
        collapseCommunity(com);
      }
      _.each(nodesToAdd, function(n) {
        delete joinedInCommunities[n._id];
        nodes.push(n);
      });
      _.each(edgesToChange, function(e) {
        var edge;
        switch(e.type) {
          case "t":
            edge = findEdge(e.id);
            edge.target = e.target;
            break;
          case "s":
            edge = findEdge(e.id);
            edge.source = e.source;
            break;
          case "b":
            edges.push(e.edge);
            break;
        }
      });
      delete cachedCommunities[commId];      
    },
  
    parseResultOfTraversal = function (result, callback) {
      result = result[0];
      var inserted = {},
        n = insertNode(result[0].vertex),
        com, buckets; 
      _.each(result, function(visited) {
        var node = insertNode(visited.vertex),
          path = visited.path;
        inserted[node._id] = node;
        _.each(path.vertices, function(connectedNode) {
          var ins = insertNode(connectedNode);
          inserted[ins._id] = ins;
        });
        _.each(path.edges, function(edge) {
          insertEdge(edge);
        });
      });      
      delete inserted[n._id];
      if (_.size(inserted) > childLimit) {
        buckets = reducer.bucketNodes(_.values(inserted), childLimit);
        _.each(buckets, function(b) {
          var ids = _.map(b, function(n) {
            return n._id;
          });
          collapseCommunity(ids);
        });
      }
      if (limit < nodes.length) {
        com = reducer.getCommunity(limit, n);
        collapseCommunity(com);
      }
      if (callback) {
        callback(n);
      }
    },
  
    parseResultOfQuery = function (result, callback) {
      _.each(result, function (node) {
        var n = findNode(node._id);
        if (!n) {
          insertNode(node);
          n = node;
        } else {
          n.children = node.children;
        }
        self.requestCentralityChildren(node._id, function(c) {
          n._centrality = c;
        });
        _.each(n.children, function(id) {
          var check = findNode(id),
          newnode;
          if (check) {
            insertEdge(n, check);
            self.requestCentralityChildren(id, function(c) {
              n._centrality = c;
            });
          } else {
            newnode = {_id: id};
            insertNode(newnode);
            insertEdge(n, newnode);
            self.requestCentralityChildren(id, function(c) {
              newnode._centrality = c;
            });
          }
        });
        if (callback) {
          callback(n);
        }
      });
    },
  
    permanentlyRemoveEdgesOfNode = function (nodeId) {
       sendQuery(queries.connectedEdges, {
         id: nodeId
       }, function(res) {
         _.each(res, self.deleteEdge);
       });
    };
    
  parseConfig(config);
  
  api.base = arangodb.lastIndexOf("http://", 0) === 0
    ? arangodb + "/_api/"
    : "http://" + arangodb + "/_api/";
  api.cursor = api.base + "cursor";
  api.collection = api.base + "collection/";
  api.document = api.base + "document/";
  api.node = api.base + "document?collection=" + nodeCollection; 
  api.edge = api.base + "edge?collection=" + edgeCollection; 
  
  queries.nodeById = "FOR n IN @@nodes"
    + " FILTER n._id == @id"
    + " LET links = ("
    + "  FOR l IN @@edges"
    + "  FILTER n._id == l._from"
    + "   FOR t IN @@nodes"
    + "   FILTER t._id == l._to"
    + "   RETURN t._id"
    + " )"
    + " RETURN MERGE(n, {\"children\" : links})";
  queries.traversalById = "RETURN TRAVERSAL("
    + "@@nodes, "
    + "@@edges, "
    + "@id, "
    + "@dir, {"
    + "strategy: \"depthfirst\","
    + "maxDepth: 1,"
    + "paths: true"
    + "})";
  queries.traversalByAttribute = function(attr) {
    return "FOR n IN @@nodes"
      + " FILTER n." + attr
      + " == @value"
      + " RETURN TRAVERSAL("
      + "@@nodes, "
      + "@@edges, "
      + "n._id, "
      + "@dir, {"
      + "strategy: \"depthfirst\","
      + "maxDepth: 1,"
      + "paths: true"
      + "})";
  };
  queries.childrenCentrality = "FOR u IN @@nodes"
    + " FILTER u._id == @id"
    + " LET g = ("
    + " FOR l in @@edges"
    + " FILTER l._from == u._id"
    + " RETURN 1 )"
    + " RETURN length(g)";
  queries.connectedEdges = "FOR e IN @@edges"
   + " FILTER e._to == @id"
   + " || e._from == @id"
   + " RETURN e";
  
  childLimit = Number.POSITIVE_INFINITY;
  
  reducer = new NodeReducer(nodes, edges);
  
  self.oldLoadNodeFromTreeById = function(nodeId, callback) {
    sendQuery(queries.nodeById, {
      id: nodeId
    }, function(res) {
      parseResultOfQuery(res, callback);
    });
  };
  
  self.loadNode = function(nodeId, callback) {
    self.loadNodeFromTreeById(nodeId, callback);
  };
  
  self.loadNodeFromTreeById = function(nodeId, callback) {
    sendQuery(queries.traversalById, {
      id: nodeId
    }, function(res) {
      parseResultOfTraversal(res, callback);
    });
  };
  
  self.loadNodeFromTreeByAttributeValue = function(attribute, value, callback) {
    sendQuery(queries.traversalByAttribute(attribute), {
      value: value
    }, function(res) {
      parseResultOfTraversal(res, callback);
    });
  };  
  
  self.requestCentralityChildren = function(nodeId, callback) {
    sendQuery(queries.childrenCentrality,{
      id: nodeId
    }, function(res) {
      callback(res[0]);
    });
  };
  
  self.createEdge = function (edgeToAdd, callback) { 
    $.ajax({
      cache: false,
      type: "POST",
      url: api.edge + "&from=" + edgeToAdd.source._id + "&to=" + edgeToAdd.target._id,
      data: JSON.stringify({}),
      dataType: "json",
      contentType: "application/json",
      processData: false,
      success: function(data) {
        data._from = edgeToAdd.source._id;
        data._to = edgeToAdd.target._id;
        delete data.error;
        var edge = insertEdge(data);
        callback(edge);
      },
      error: function(data) {
        throw data.statusText;
      }
    });
  };
  
  self.deleteEdge = function (edgeToRemove, callback) {
    $.ajax({
      cache: false,
      type: "DELETE",
      url: api.document + edgeToRemove._id,
      contentType: "application/json",
      dataType: "json",
      processData: false,
      success: function() {
        removeEdge(edgeToRemove);
        if (callback !== undefined && _.isFunction(callback)) {
          callback();
        }
      },
      error: function(data) {
        throw data.statusText;
      }
    });
    
  };
  
  self.patchEdge = function (edgeToPatch, patchData, callback) {
    $.ajax({
      cache: false,
      type: "PUT",
      url: api.document + edgeToPatch._id,
      data: JSON.stringify(patchData),
      dataType: "json",
      contentType: "application/json",
      processData: false,
      success: function(data) {
        edgeToPatch._data = $.extend(edgeToPatch._data, patchData);
        callback();
      },
      error: function(data) {
        throw data.statusText;
      }
    });
  };
  
  self.createNode = function (nodeToAdd, callback) {
    $.ajax({
      cache: false,
      type: "POST",
      url: api.node,
      data: JSON.stringify(nodeToAdd),
      dataType: "json",
      contentType: "application/json",
      processData: false,
      success: function(data) {
        insertNode(data);
        callback(data);
      },
      error: function(data) {
        throw data.statusText;
      }
    });
  };
  
  self.deleteNode = function (nodeToRemove, callback) {
    $.ajax({
      cache: false,
      type: "DELETE",
      url: api.document + nodeToRemove._id,
      dataType: "json",
      contentType: "application/json",
      processData: false,
      success: function() {
        removeEdgesForNode(nodeToRemove);
        permanentlyRemoveEdgesOfNode(nodeToRemove._id);    
        removeNode(nodeToRemove);
        if (callback !== undefined && _.isFunction(callback)) {
          callback();
        }
      },
      error: function(data) {
        throw data.statusText;
      }
    });
  };
  
  self.patchNode = function (nodeToPatch, patchData, callback) {
    $.ajax({
      cache: false,
      type: "PUT",
      url: api.document + nodeToPatch._id,
      data: JSON.stringify(patchData),
      dataType: "json",
      contentType: "application/json",
      processData: false,
      success: function(data) {
        nodeToPatch._data = $.extend(nodeToPatch._data, patchData);
        callback(nodeToPatch);
      },
      error: function(data) {
        throw data.statusText;
      }
    });
  };
  
  self.changeTo = function (nodesCol, edgesCol, dir) {
    nodeCollection = nodesCol;
    edgeCollection = edgesCol;
    if (dir !== undefined) {
      if (dir === true) {
        direction = "any";
      } else {
        direction = "outbound";
      }
    }
    api.node = api.base + "document?collection=" + nodeCollection; 
    api.edge = api.base + "edge?collection=" + edgeCollection;
  };
  
  self.setNodeLimit = function (pLimit, callback) {
    limit = pLimit;
    if (limit < nodes.length) {
      var com = reducer.getCommunity(limit);
      collapseCommunity(com);
      if (callback !== undefined) {
        callback();
      }
    }
  };
  
  self.setChildLimit = function (pLimit) {
    childLimit = pLimit;
  };
  
  self.expandCommunity = function (commNode, callback) {
    expandCommunity(commNode);
    if (callback !== undefined) {
      callback();
    }
  };
  
  self.getCollections = function(callback) {
    if (callback && callback.length >= 2) {
      $.ajax({
        cache: false,
        type: "GET",
        url: api.collection,
        contentType: "application/json",
        dataType: "json",
        processData: false,
        success: function(data) {
          var cols = data.collections,
            docs = [],
            edgeCols = [];
          _.each(cols, function(c) {
            if (!c.name.match(/^_/)) {
              if (c.type === 3) {
                edgeCols.push(c.name);
              } else if (c.type === 2){
                docs.push(c.name);
              }
            }
          });
          callback(docs, edgeCols);
        },
        error: function(data) {
          throw data.statusText;
        }
      });
    }
  };
}