/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, d3, _, console, document*/
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
  if (config.graph === undefined) {
    if (config.nodeCollection === undefined) {
      throw "The nodeCollection or a graphname has to be given.";
    }
    if (config.edgeCollection === undefined) {
      throw "The edgeCollection or a graphname has to be given.";
    }
  }

  var self = this,
    absAdapter,
    absConfig = {},
    api = {},
    queries = {},
    nodeCollection,
    edgeCollection,
    graphName,
    direction,

    setGraphName = function(name) {
      graphName = name;
    },

    setNodeCollection = function(name) {
      nodeCollection = name;
      api.node = api.base + "document?collection=" + nodeCollection;
    },

    setEdgeCollection = function(name) {
      edgeCollection = name;
      api.edge = api.base + "edge?collection=" + edgeCollection;
    },

    getCollectionsFromGraph = function(name) {
      $.ajax({
        cache: false,
        type: 'GET',
        async: false,
        url: api.graph + "/" + name,
        contentType: "application/json",
        success: function(data) {
          setNodeCollection(data.graph.vertices);
          setEdgeCollection(data.graph.edges);
        }
      });
    },

    parseConfig = function(config) {
      var arangodb = config.baseUrl || "";
      if (config.width !== undefined) {
        absAdapter.setWidth(config.width);
      }
      if (config.height !== undefined) {
        absAdapter.setHeight(config.height);
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
      api.base = arangodb + "_api/";
      api.cursor = api.base + "cursor";
      api.graph = api.base + "graph";
      api.collection = api.base + "collection/";
      api.document = api.base + "document/";
      api.any = api.base + "simple/any";     
      if (config.graph) {
        getCollectionsFromGraph(config.graph);
        setGraphName(config.graph);
      } else {
        setNodeCollection(config.nodeCollection);
        setEdgeCollection(config.edgeCollection);
        setGraphName(undefined);
      }
    },
  
    sendQuery = function(query, bindVars, onSuccess) {
      if (query !== queries.getAllGraphs) {
        if (query !== queries.connectedEdges) {
          bindVars["@nodes"] = nodeCollection;
        }
        if (query !== queries.childrenCentrality) {
          bindVars.dir = direction;
        }
        bindVars["@edges"] = edgeCollection;
      }
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
  
    getNRandom = function(n, callback) {
      var list = [],
        i = 0,
        onSuccess = function(data) {
          list.push(data.document || {});
          if (list.length === n) {
            callback(list);
          }
        };
      for (i = 0; i < n; i++) {
        $.ajax({
          cache: false,
          type: 'PUT',
          url: api.any,
          data: JSON.stringify({
            collection: nodeCollection
          }),
          contentType: "application/json",
          success: onSuccess
        });
      }
    },
    
    parseResultOfTraversal = function (result, callback) {
      if (result.length === 0) {
        if (callback) {
          callback({
            errorCode: 404
          });
        }
        return;
      }
      result = result[0];
      var inserted = {},
        n = absAdapter.insertNode(result[0].vertex),
        oldLength = nodes.length,
        com, buckets;
        
      _.each(result, function(visited) {
        var node = absAdapter.insertNode(visited.vertex),
          path = visited.path;
        if (oldLength < nodes.length) {
          inserted[node._id] = node;
          oldLength = nodes.length;
        }
        _.each(path.vertices, function(connectedNode) {
          var ins = absAdapter.insertNode(connectedNode);
          if (oldLength < nodes.length) {
            inserted[ins._id] = ins;
            oldLength = nodes.length;
          }
        });
        _.each(path.edges, function(edge) {
          absAdapter.insertEdge(edge);
        });
      });      
      delete inserted[n._id];
      absAdapter.checkSizeOfInserted(inserted);
      absAdapter.checkNodeLimit(n);
      if (callback) {
        callback(n);
      }
    },
    /* Archive
    parseResultOfQuery = function (result, callback) {
      _.each(result, function (node) {
        var n = findNode(node._id);
        if (!n) {
          absAdapter.insertNode(node);
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
            absAdapter.insertEdge(n, check);
            self.requestCentralityChildren(id, function(c) {
              n._centrality = c;
            });
          } else {
            newnode = {_id: id};
            absAdapter.insertNode(newnode);
            absAdapter.insertEdge(n, newnode);
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
    */
    permanentlyRemoveEdgesOfNode = function (nodeId) {
       sendQuery(queries.connectedEdges, {
         id: nodeId
       }, function(res) {
         _.each(res, self.deleteEdge);
       });
    };
   
   
  if (config.prioList) {
    absConfig.prioList = config.prioList;
  }
  absAdapter = new AbstractAdapter(nodes, edges, this, absConfig);
  
  parseConfig(config);
  
  queries.getAllGraphs = "FOR g IN _graphs"
    + " return g._key";
  queries.randomDocuments = "FOR u IN @@nodes"
    + " sort rand()"
    + " limit 10"
    + " return u";
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
   /* Archive
  self.oldLoadNodeFromTreeById = function(nodeId, callback) {
    sendQuery(queries.nodeById, {
      id: nodeId
    }, function(res) {
      parseResultOfQuery(res, callback);
    });
  };
  */
  
  self.explore = absAdapter.explore;
  
  self.loadNode = function(nodeId, callback) {
    self.loadNodeFromTreeById(nodeId, callback);
  };

  self.loadRandomNode = function(callback) {
    var self = this;
    getNRandom(1, function(list) {
      var r = list[0];
      if (r._id) {
        self.loadInitialNode(r._id, callback);
        return; 
      }
      return;
    });
  };
  
  self.loadInitialNode = function(nodeId, callback) {
    absAdapter.cleanUp();
    var cb = function(n) {
      callback(absAdapter.insertInitialNode(n));
    };
    self.loadNode(nodeId, cb);
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
  
  self.loadInitialNodeByAttributeValue = function(attribute, value, callback) {
    absAdapter.cleanUp();
    var cb = function(n) {
      callback(absAdapter.insertInitialNode(n));
    };
    self.loadNodeFromTreeByAttributeValue(attribute, value, cb);
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
        var edge = absAdapter.insertEdge(data);
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
        absAdapter.removeEdge(edgeToRemove);
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
        if (data.error === false) {
          nodeToAdd._key = data._key;
          nodeToAdd._id = data._id;
          nodeToAdd._rev = data._rev;
          absAdapter.insertNode(nodeToAdd);
          callback(nodeToAdd);
        }
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
        absAdapter.removeEdgesForNode(nodeToRemove);
        permanentlyRemoveEdgesOfNode(nodeToRemove._id);
        absAdapter.removeNode(nodeToRemove);
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
  
  self.changeToCollections = function (nodesCol, edgesCol, dir) {
    absAdapter.cleanUp();
    setNodeCollection(nodesCol);
    setEdgeCollection(edgesCol);
    if (dir !== undefined) {
      if (dir === true) {
        direction = "any";
      } else {
        direction = "outbound";
      }
    }

    setGraphName(undefined);
  };
  
  self.changeToGraph = function (name, dir) {
    absAdapter.cleanUp();
    getCollectionsFromGraph(name);
    if (dir !== undefined) {
      if (dir === true) {
        direction = "any";
      } else {
        direction = "outbound";
      }
    }
    setGraphName(name);
  };
  
  self.setNodeLimit = function (pLimit, callback) {
    absAdapter.setNodeLimit(pLimit, callback);
  };
  
  self.setChildLimit = function (pLimit) {
    absAdapter.setChildLimit(pLimit);
  };
  
  self.expandCommunity = function (commNode, callback) {
    absAdapter.expandCommunity(commNode);
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
  
  self.getGraphs = function(callback) {
    if (callback && callback.length >= 1) {      
      sendQuery(
        queries.getAllGraphs,
        {}, 
        callback
      );
    }
  };
  
  self.getAttributeExamples = function(callback) {
    if (callback && callback.length >= 1) {
      getNRandom(10, function(l) {
        var ret = _.sortBy(
          _.uniq(
            _.flatten(
              _.map(l, function(o) {
                return _.keys(o);
              })
            )
          ), function(e) {
            return e.toLowerCase();
          }
        );
        callback(ret);
      });
    }
  };

  self.getNodeCollection = function () {
    return nodeCollection;
  };

  self.getEdgeCollection = function () {
    return edgeCollection;
  };

  self.getDirection = function () {
    return direction;
  };

  self.getGraphName = function () {
    return graphName;
  };
  
  self.setWidth = absAdapter.setWidth;
  self.changeTo = absAdapter.changeTo;
  self.getPrioList = absAdapter.getPrioList;
}
