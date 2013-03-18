/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, d3, _, console, jQuery*/
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

function ArangoAdapter(arangodb, nodes, edges, nodeCollection, edgeCollection, width, height) {
  "use strict";
  
  var self = this,
  initialX = {},
  initialY = {},
  api = {},
  
  findNode = function(id) {
    var res = $.grep(nodes, function(e){
      return e._id === id;
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
    throw "Too many nodes with the same ID, should never happen";
  },
  
  insertNode = function(node) {
    var n = findNode(node._id);
    if (n) {
      return n;
    }
    initialY.getStart();
    node.x = initialX.getStart();
    node.y = initialY.getStart();
    nodes.push(node);
    node._outboundCounter = 0;
    node._inboundCounter = 0;
  },
  
  insertEdge = function(edge) {
    var source,
    target,
    e = findEdge(edge._id);
    if (e) {
      return e;
    }
    source = findNode(edge._from);
    target = findNode(edge._to);
    if (!source) {
      throw "Unable to insert Edge, source node not existing " + edge._from;
    }
    if (!target) {
      throw "Unable to insert Edge, target node not existing " + edge._to;
    }
    edge.source = source;
    edge.target = target;
    edges.push(edge);
    source._outboundCounter++;
    target._inboundCounter++;
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
  
  
  sendQuery = function(query, onSuccess) {
    var data = {query: query};
    $.ajax({
      type: "POST",
      url: api.cursor,
      data: JSON.stringify(data),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        onSuccess(data.result);
      },
      error: function(data) {
        try {
          console.log(data.statusText);
          var temp = JSON.parse(data);
          console.log(temp);
          throw "[" + temp.errorNum + "] " + temp.errorMessage;
        }
        catch (e) {
          console.log(e);
          throw "Undefined ERROR";
        }
      }
    });
  },
  
  parseResultOfTraversal = function (result, callback) {
    result = result[0];
    _.each(result, function(visited) {
      var node = insertNode(visited.vertex),
      path = visited.path;
      _.each(path.vertices, function(connectedNode) {
        insertNode(connectedNode);
      });
      _.each(path.edges, function(edge) {
        insertEdge(edge);
      });
    });
    if (callback) {
      callback(result[0].vertex);
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
  };
  
  api.base = arangodb.lastIndexOf("http://", 0) === 0
    ? arangodb + "/_api/"
    : "http://" + arangodb + "/_api/";
  api.cursor = api.base + "cursor";
  api.collection = api.base + "collection/";
  api.document = api.base + "document/";
  api.node = api.base + "document?collection=" + nodeCollection; 
  api.edge = api.base + "edge?collection=" + edgeCollection; 
  
  
  
  initialX.range = width / 2;
  initialX.start = width / 4;
  initialX.getStart = function () {
    return this.start + Math.random() * this.range;
  };
  
  initialY.range = height / 2;
  initialY.start = height / 4;
  initialY.getStart = function () {
    return this.start + Math.random() * this.range;
  };
  
  self.oldLoadNodeFromTreeById = function(nodeId, callback) {
    var loadNodeQuery =
        "FOR n IN " + nodeCollection
      + " FILTER n._id == " + JSON.stringify(nodeId)
      + " LET links = ("
      + "  FOR l IN " + edgeCollection
      + "  FILTER n._id == l._from"
      + "   FOR t IN " + nodeCollection
      + "   FILTER t._id == l._to"
      + "   RETURN t._id"
      + " )"
      + " RETURN MERGE(n, {\"children\" : links})";
    sendQuery(loadNodeQuery, function(res) {
      parseResultOfQuery(res, callback);
    });
  };
  
  self.loadNodeFromTreeById = function(nodeId, callback) {
    var traversal = "RETURN TRAVERSAL("
    + nodeCollection + ", "
    + edgeCollection + ", "
    + "\"" + nodeId + "\", "
    + "\"outbound\", {"
    + "startegy: \"depthfirst\","
    + "maxDepth: 1,"
    + "paths: true"
    + "})";
    sendQuery(traversal, function(res) {
      parseResultOfTraversal(res, callback);
    });
  };
  
  self.loadNodeFromTreeByAttributeValue = function(attribute, value, callback) {
    var loadNodeQuery =
        "FOR n IN " + nodeCollection
      + "FILTER n." + attribute + " == \"" + value + "\""
      + "LET links = ("
      + "  FOR l IN " + edgeCollection
      + "  FILTER n._id == l._from"
      + "   FOR t IN " + nodeCollection
      + "   FILTER t._id == l._to"
      + "   RETURN t._id"
      + ")"
      + "RETURN MERGE(n, {\"children\" : links})";

    sendQuery(loadNodeQuery, function(res) {
      parseResultOfQuery(res, callback);
    });
  };  
  
  self.requestCentralityChildren = function(nodeId, callback) {
    var requestChildren = "for u in " + nodeCollection
      + " filter u._id == " + JSON.stringify(nodeId)
      + " let g = ("
      + " for l in " + edgeCollection
      + " filter l._from == u._id"
      + " return 1 )"
      + " return length(g)";
    
    sendQuery(requestChildren, function(res) {
      callback(res[0]);
    });
  };
  
  self.createEdge = function (edgeToAdd, callback) { 
    $.ajax({
      cache: false,
      type: "POST",
      url: api.edge + "&from=" + edgeToAdd.source._id + "&to=" + edgeToAdd.target._id,
      data: JSON.stringify({}),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        data._from = edgeToAdd.source._id;
        data._to = edgeToAdd.target._id;
        insertEdge(data);
        callback(data);
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
      processData: false,
      success: function() {
        callback();
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
      contentType: "application/json",
      processData: false,
      success: function(data) {
        edgeToPatch = jQuery.extend(edgeToPatch, patchData);
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
    var semaphore = 1,
    semaphoreCallback = function() {
      semaphore--;
      if (semaphore === 0) {
        if (callback) {
          callback();
        }
      }
    };
    $.ajax({
      cache: false,
      type: "DELETE",
      url: api.document + nodeToRemove._id,
      contentType: "application/json",
      processData: false,
      success: function() {
        var edges = removeOutboundEdgesFromNode(nodeToRemove);    
        _.each(edges, function (e) {
          semaphore++;
          self.deleteEdge(e, semaphoreCallback);
        });
        removeNode(nodeToRemove);
        semaphoreCallback();
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
      contentType: "application/json",
      processData: false,
      success: function(data) {
        nodeToPatch = jQuery.extend(nodeToPatch, patchData);
        callback();
      },
      error: function(data) {
        throw data.statusText;
      }
    });
  };
  
  
}