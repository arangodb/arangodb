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

function GharialAdapter(nodes, edges, viewer, config) {
  "use strict";

  if (nodes === undefined) {
    throw "The nodes have to be given.";
  }
  if (edges === undefined) {
    throw "The edges have to be given.";
  }
  if (viewer === undefined) {
    throw "A reference to the graph viewer has to be given.";
  }
  if (config === undefined) {
    throw "A configuration with graphName has to be given.";
  }
  if (config.graphName === undefined) {
    throw "The graphname has to be given.";
  }

  var self = this,
    absAdapter,
    absConfig = {},
    api = {},
    queries = {},
    nodeCollections,
    selectedNodeCol,
    edgeCollections,
    selectedEdgeCol,
    graphName,
    direction,

    getCollectionsFromGraph = function(name) {
      $.ajax({
        cache: false,
        type: 'GET',
        async: false,
        url: api.graph + "/" + name + "/edge",
        contentType: "application/json",
        success: function(data) {
          edgeCollections = data.collections;
          selectedEdgeCol = edgeCollections[0];
        }
      });
      $.ajax({
        cache: false,
        type: 'GET',
        async: false,
        url: api.graph + "/" + name + "/vertex",
        contentType: "application/json",
        success: function(data) {
          nodeCollections = data.collections;
          selectedNodeCol = nodeCollections[0];
        }
      });
    },

    setGraphName = function(name) {
      graphName = name;
      getCollectionsFromGraph(name);
      api.edges = api.graph + "/" + graphName + "/edge/";
      api.vertices = api.graph + "/" + graphName + "/vertex/";
      api.any = api.base + "simple/any";     
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
      api.graph = api.base + "gharial";

      if (config.graphName) {
        setGraphName(config.graphName);
      }
    },
  
    sendQuery = function(query, bindVars, onSuccess) {
      if (query !== queries.getAllGraphs) {
        bindVars.graph = graphName;
        if (query !== queries.connectedEdges
            && query !== queries.childrenCentrality) {
            bindVars.dir = direction;
        }
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
            throw "Undefined ERROR";
          }
        }
      });
    },
  
    getNRandom = function(n, callback) {
      var list = [],
        i = 0,
        rand,
        onSuccess = function(data) {
          list.push(data.document || {});
          if (list.length === n) {
            callback(list);
          }
        };
      for (i = 0; i < n; i++) {
        rand = Math.floor(Math.random() * nodeCollections.length);
        $.ajax({
          cache: false,
          type: 'PUT',
          url: api.any,
          data: JSON.stringify({
            collection: nodeCollections[rand]
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
      if (result.length === 0) {
        if (callback) {
          callback({
            errorCode: 404
          });
        }
        return;
      }
      var inserted = {},
        n = absAdapter.insertNode(result[0].vertex),
        oldLength = nodes.length;
        
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

    insertInitialCallback = function(callback) {
      return function (n) {
        if (n && n.errorCode) {
          callback(n);
          return;
        }
        callback(absAdapter.insertInitialNode(n));
      };
    };

   
  if (config.prioList) {
    absConfig.prioList = config.prioList;
  }
  absAdapter = new AbstractAdapter(nodes, edges, this, viewer, absConfig);
  
  parseConfig(config);
  
  queries.getAllGraphs = "FOR g IN _graphs"
    + " return g._key";
  queries.traversal = "RETURN GRAPH_TRAVERSAL("
      + "@graph, "
      + "@example, "
      + "@dir, {"
      + "strategy: \"depthfirst\","
      + "maxDepth: 1,"
      + "paths: true"
      + "})";
  queries.childrenCentrality = "RETURN LENGTH(GRAPH_EDGES(@graph, @id, {direction: any}))";
  queries.connectedEdges = "RETURN GRAPH_EDGES(@graph, @id)";
  
  self.explore = absAdapter.explore;
  
  self.loadNode = function(nodeId, callback) {
    self.loadNodeFromTreeById(nodeId, callback);
  };

  self.loadRandomNode = function(callback) {
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
    self.loadNode(nodeId, insertInitialCallback(callback));
  };
  
  self.loadNodeFromTreeById = function(nodeId, callback) {
    sendQuery(queries.traversal, {
      example: nodeId
    }, function(res) {
      parseResultOfTraversal(res, callback);
    });
  };
  
  self.loadNodeFromTreeByAttributeValue = function(attribute, value, callback) {
    var example = {};
    example[attribute] = value;
    sendQuery(queries.traversal, {
      example: example
    }, function(res) {
      parseResultOfTraversal(res, callback);
    });
  };  
  
  self.loadInitialNodeByAttributeValue = function(attribute, value, callback) {
    absAdapter.cleanUp();
    self.loadNodeFromTreeByAttributeValue(attribute, value, insertInitialCallback(callback));
  };
  
  self.requestCentralityChildren = function(nodeId, callback) {
    sendQuery(queries.childrenCentrality,{
      id: nodeId
    }, function(res) {
      callback(res[0]);
    });
  };
  
  self.createEdge = function (info, callback) { 
    var edgeToAdd = {};
    edgeToAdd._from = info.source._id;
    edgeToAdd._to = info.target._id;
    $.ajax({
      cache: false,
      type: "POST",
      url: api.edges + selectedEdgeCol,
      data: JSON.stringify(edgeToAdd),
      dataType: "json",
      contentType: "application/json",
      processData: false,
      success: function(data) {
        data._from = edgeToAdd._from;
        data._to = edgeToAdd._to;
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
      url: api.edges + edgeToRemove._id,
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
      url: api.edges + edgeToPatch._id,
      data: JSON.stringify(patchData),
      dataType: "json",
      contentType: "application/json",
      processData: false,
      success: function() {
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
      url: api.vertices + selectedNodeCol,
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
      url: api.vertices + nodeToRemove._id,
      dataType: "json",
      contentType: "application/json",
      processData: false,
      success: function() {
        absAdapter.removeEdgesForNode(nodeToRemove);
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
      url: api.vertices + nodeToPatch._id,
      data: JSON.stringify(patchData),
      dataType: "json",
      contentType: "application/json",
      processData: false,
      success: function() {
        nodeToPatch._data = $.extend(nodeToPatch._data, patchData);
        callback(nodeToPatch);
      },
      error: function(data) {
        throw data.statusText;
      }
    });
  };
  
  self.changeToGraph = function (name, dir) {
    absAdapter.cleanUp();
    setGraphName(name);
    if (dir !== undefined) {
      if (dir === true) {
        direction = "any";
      } else {
        direction = "outbound";
      }
    }
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


  self.getEdgeCollections = function() {
    return edgeCollections;
  };

  self.useEdgeCollection = function(name) {
    if (!_.contains(edgeCollections, name)) {
      throw "Collection " + name + " is not available in the graph.";
    }
    selectedEdgeCol = name;
  };

  self.getNodeCollections = function() {
    return nodeCollections;
  };

  self.useNodeCollection = function(name) {
    if (!_.contains(nodeCollections, name)) {
      throw "Collection " + name + " is not available in the graph.";
    }
    selectedNodeCol = name;
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
