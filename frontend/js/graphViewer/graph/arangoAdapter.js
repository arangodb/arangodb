/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, d3, _, console*/
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
  
  insertNode = function(node) {
    initialY.getStart();
    node.x = initialX.getStart();
    node.y = initialY.getStart();
    nodes.push(node);
    node._outboundCounter = 0;
    node._inboundCounter = 0;
  },
  
  insertEdge = function(source, target) {
    edges.push({source: source, target: target});
    source._outboundCounter++;
    target._inboundCounter++;
  },
  api = arangodb.lastIndexOf("http://", 0) === 0
    ? arangodb + "/_api/cursor"
    : "http://" + arangodb + "/_api/cursor",
  
  sendQuery = function(query, onSuccess) {
    var data = {query: query};
    $.ajax({
      type: "POST",
      url: api,
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
  
  
  
  self.loadNodeFromTreeById = function(nodeId, callback) {
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
}