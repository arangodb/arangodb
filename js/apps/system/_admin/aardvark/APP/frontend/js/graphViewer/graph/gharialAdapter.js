/*global $, _, console*/
/*global AbstractAdapter, arangoHelper*/
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
        // currently there is a bug with outbound-direction graphs.
        // any should be default at the moment
        direction = "any";
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

    getNRandom = function(n, collection) {
      var data = {
        query: queries.randomVertices,
        bindVars: {
          "@collection" : collection,
          limit: n
        }
      };
      var result;

      $.ajax({
        type: "POST",
        url: api.cursor,
        data: JSON.stringify(data),
        contentType: "application/json",
        dataType: "json",
        processData: false,
        async: false,
        success: function(data) {
          result = data.result;
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
      return result;
    },

    parseResultOfTraversal = function (result, callback) {
      if (result.length === 0 || result[0].vertex === null) {
        if (callback) {
          callback({ errorCode: 404});
        }
      }

      var inserted = {},
          // This is start vertex
        n = absAdapter.insertNode(result[0].vertex),
        oldLength = nodes.length;
      result.shift();

      _.each(result, function(visited) {
        if (visited.vertex !== null) {
          var node = absAdapter.insertNode(visited.vertex);
          if (oldLength < nodes.length) {
            inserted[node._id] = node;
            oldLength = nodes.length;
          }
          if (visited.edge !== null) {
            absAdapter.insertEdge(visited.edge);
          }
        }
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
  queries.traversal = function (dir) {
    return "FOR vertex, edge IN 0..1 " + dir + " @example GRAPH @graph RETURN {vertex, edge}";
  };
  queries.oneNodeByAttributeValue = function (bindVars, collections, attribute, value) {
    bindVars.attr = attribute;
    bindVars.value = value;
    if (collections.length === 1) {
      bindVars["@collection"] = collections[0];
      return "FOR node IN @@collection FILTER node[@attr] == @value LIMIT 1";
    }
    var query = "FOR node IN UNION(";
    var i = 0;
    for (i = 0; i < collections.length; ++i) {
      query += "(FOR node IN @@collection" + i + " FILTER node[@attr] == @value LIMIT 1 RETURN node)";
      bindVars["@collection" + i] = collections[i];
    }
    query += ") LIMIT 1";
    return query;
  };
  queries.traversalAttributeValue = function (dir, bindVars, collections, attribute, value) {
    return queries.oneNodeByAttributeValue(bindVars, collections, attribute, value) +
           " FOR vertex, edge IN 0..1 " + dir + " node GRAPH @graph RETURN {vertex, edge}";
  };
  queries.childrenCentrality = "RETURN SUM(FOR v IN ANY @id GRAPH @graph RETURN 1)";
  queries.connectedEdges = "FOR v, e IN ANY @id GRAPH @graph RETURN e";
  queries.randomVertices = "FOR x IN @@collection SORT RAND() LIMIT @limit RETURN x";

  self.explore = absAdapter.explore;

  self.loadNode = function(nodeId, callback) {
    self.loadNodeFromTreeById(nodeId, callback);
  };

  //origin nodes to display, real display may be more (depending on their relations)
  self.NODES_TO_DISPLAY = 19;
  self.TOTAL_NODES = 0;

  self.definedNodes = [];
  self.randomNodes = [];

  self.loadRandomNode = function(callback, size) {
    var collections = _.shuffle(self.getNodeCollections()), i;
    for (i = 0; i < collections.length; ++i) {

      if (size !== undefined) {
        if (size === 'all') {
          self.NODES_TO_DISPLAY = self.TOTAL_NODES;
        }
        else {
          self.NODES_TO_DISPLAY = parseInt(size, 10) - 1;
        }

        if (self.NODES_TO_DISPLAY >= self.TOTAL_NODES) {
          $('.infoField').hide();
        }
        else {
          $('.infoField').show();
        }
      }

      var list = getNRandom(self.NODES_TO_DISPLAY, collections[i]);
      if (list.length > 0) {
        _.each(list, function(node) {
          self.randomNodes.push(node);
        });
        self.loadInitialNode(list[0]._id, callback);
        return;
      }
    }

    // no vertex found
    callback({errorCode: 404});
  };

  self.loadInitialNode = function(nodeId, callback) {
    absAdapter.cleanUp();
    self.loadNode(nodeId, insertInitialCallback(callback));
  };

  self.getRandomNodes = function () {
    var nodeArray = [];
    var nodes = [];

    if (self.definedNodes.length > 0) {
      _.each(self.definedNodes, function(node) {
        nodes.push(node);
      });
    }
    if (self.randomNodes.length > 0) {
      _.each(self.randomNodes, function(node) {
        nodes.push(node);
      });
    }

    var counter = 0;
    _.each(nodes, function(node) {
      if (counter < self.NODES_TO_DISPLAY) {
        nodeArray.push({
          vertex: node,
          path: {
            edges: [],
            vertices: [node]
          }
        });
        counter++;
      }
    });

  return nodeArray;
  };

  self.loadNodeFromTreeById = function(nodeId, callback) {
    sendQuery(queries.traversal(direction), {
      example: nodeId
    }, function(res) {

      var nodes = [];
      nodes = self.getRandomNodes();

      if (nodes.length > 0) {
        _.each(nodes, function(node) {
          sendQuery(queries.traversal(direction), {
            example: node.vertex._id
          }, function(res2) {
            _.each(res2, function(obj) {
              res.push(obj);
            });
            parseResultOfTraversal(res, callback);
          });
        });
      }
      else {
        sendQuery(queries.traversal(direction), {
          example: nodeId
        }, function(res) {
          parseResultOfTraversal(res, callback);
        });
      }
    });

  };

  self.loadNodeFromTreeByAttributeValue = function(attribute, value, callback) {
    var bindVars = {};
    var q = queries.traversalAttributeValue(direction, bindVars, nodeCollections, attribute, value);
    sendQuery(q, bindVars, function(res) {
      parseResultOfTraversal(res, callback);
    });
  };

  self.getNodeExampleFromTreeByAttributeValue = function(attribute, value, callback) {
    var bindVars;

    var q = queries.travesalAttributeValue(direction, bindVars, nodeCollections, attribute, value);
    sendQuery(q, bindVars, function(res) {
      if (res.length === 0) {
        arangoHelper.arangoError("Graph error", "no nodes found");
        throw "No suitable nodes have been found.";
      }
      else {
        _.each(res, function(node) {
          if (node.vertex[attribute] === value) {
            var nodeToAdd = {};
            nodeToAdd._key = node.vertex._key;
            nodeToAdd._id = node.vertex._id;
            nodeToAdd._rev = node.vertex._rev;
            absAdapter.insertNode(nodeToAdd);
            callback(nodeToAdd);
          }

        });
      }
    });

  };

  self.loadAdditionalNodeByAttributeValue = function(attribute, value, callback) {
    self.getNodeExampleFromTreeByAttributeValue(attribute, value, callback);
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
        if (data.error === false) {
          var toInsert = data.edge, edge;
          toInsert._from = edgeToAdd._from;
          toInsert._to = edgeToAdd._to;
          edge = absAdapter.insertEdge(toInsert);
          callback(edge);
        }
      },
      error: function(data) {
        var text = "";
        try {
          text = JSON.parse(data.responseText).errorMessage + ' (' + JSON.parse(data.responseText).errorNum + ')';
          arangoHelper.arangoError(data.statusText, text);
        }
        catch (e) {
          throw data.statusText;
        }
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
        var text = "";
        try {
          text = JSON.parse(data.responseText).errorMessage + ' (' + JSON.parse(data.responseText).errorNum + ')';
          arangoHelper.arangoError(data.statusText, text);
        }
        catch (e) {
          throw data.statusText;
        }
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
        var text = "";
        try {
          text = JSON.parse(data.responseText).errorMessage + ' (' + JSON.parse(data.responseText).errorNum + ')';
          arangoHelper.arangoError(data.statusText, text);
        }
        catch (e) {
          throw data.statusText;
        }
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
          nodeToAdd._key = data.vertex._key;
          nodeToAdd._id = data.vertex._id;
          nodeToAdd._rev = data.vertex._rev;
          absAdapter.insertNode(nodeToAdd);
          callback(nodeToAdd);
        }
      },
      error: function(data) {
        var text = "";
        try {
          text = JSON.parse(data.responseText).errorMessage + ' (' + JSON.parse(data.responseText).errorNum + ')';
          arangoHelper.arangoError(data.statusText, text);
        }
        catch (e) {
          throw data.statusText;
        }
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
        var text = "";
        try {
          text = JSON.parse(data.responseText).errorMessage + ' (' + JSON.parse(data.responseText).errorNum + ')';
          arangoHelper.arangoError(data.statusText, text);
        }
        catch (e) {
          throw data.statusText;
        }
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
      var ret = [ ];
      var collections = _.shuffle(self.getNodeCollections()), i;
      for (i = 0; i < collections.length; ++i) {
        var l = getNRandom(10, collections[i]);

        //count vertices of graph
        $.ajax({
          cache: false,
          type: 'GET',
          async: false,
          url: arangoHelper.databaseUrl("/_api/collection/" + encodeURIComponent(collections[i]) + "/count"),
          contentType: "application/json",
          success: function(data) {
            self.TOTAL_NODES = self.TOTAL_NODES + data.count;
          }
        });

        if (l.length > 0) {
          ret = ret.concat(_.flatten(
           _.map(l, function(o) {
             return _.keys(o);
           })
          ));
        }
      }
          
      ret = _.sortBy(
        _.uniq(ret), function(e) {
          return e.toLowerCase();
        }
      );

      callback(ret);
    }
  };


  self.getEdgeCollections = function() {
    return edgeCollections;
  };

  self.getSelectedEdgeCollection = function() {
    return selectedEdgeCol;
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

  self.getSelectedNodeCollection = function() {
    return selectedNodeCol;
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
