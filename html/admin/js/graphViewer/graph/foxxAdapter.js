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

function FoxxAdapter(nodes, edges, route, config) {
  "use strict";
  
  if (nodes === undefined) {
    throw "The nodes have to be given.";
  }
  if (edges === undefined) {
    throw "The edges have to be given.";
  }
  if (route === undefined) {
    throw "The route has to be given.";
  }

  var self = this,
    absAdapter = new AbstractAdapter(nodes, edges, this),
    routes = {},
    baseRoute = route,
    requestBase = {
      cache: false,
      contentType: "application/json",
      dataType: "json",
      processData: false,
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
    },
    
    fillRoutes = function () {
      routes.query = {
        get: function(id, cb) {
          var reqinfo = $.extend(requestBase, {
            type: "GET",
            url: baseRoute + "/query/" + id,
            success: cb
          });
          $.ajax(reqinfo);
        }
      };
      routes.nodes = {
        post: function(data, cb) {
          var reqinfo = $.extend(requestBase, {
            type: "POST",
            url: baseRoute + "/nodes",
            data: JSON.stringify(data),
            success: cb
          });
          $.ajax(reqinfo);
        },
        put: function(id, data, cb) {
          var reqinfo = $.extend(requestBase, {
            type: "PUT",
            url: baseRoute + "/nodes/" + id,
            data: JSON.stringify(data),
            success: cb
          });
          $.ajax(reqinfo);
        },
        del: function(id, cb) {
          var reqinfo = $.extend(requestBase, {
            type: "DELETE",
            url: baseRoute + "/nodes/" + id,
            success: cb
          });
          $.ajax(reqinfo);
        }
      };
      routes.edges = {
        post: function(data, cb) {
          var reqinfo = $.extend(requestBase, {
            type: "POST",
            url: baseRoute + "/edges",
            data: JSON.stringify(data),
            success: cb
          });
          $.ajax(reqinfo);
        },
        put: function(id, data, cb) {
          var reqinfo = $.extend(requestBase, {
            type: "PUT",
            url: baseRoute + "/edges/" + id,
            data: JSON.stringify(data),
            success: cb
          });
          $.ajax(reqinfo);
        },
        del: function(id, cb) {
          var reqinfo = $.extend(requestBase, {
            type: "DELETE",
            url: baseRoute + "/edges/" + id,
            success: cb
          });
          $.ajax(reqinfo);
        }
      };
      routes.forNode = {
        del: function(id, cb) {
          var reqinfo = $.extend(requestBase, {
            type: "DELETE",
            url: baseRoute + "/edges/forNode/" + id,
            success: cb
          });
          $.ajax(reqinfo);
        }
      };
    },
    
    sendGet = function (type, id, callback) {
      routes[type].get(id, callback);
    },
    
    sendPost = function (type, data, callback) {
      routes[type].post(data, callback);
    },
    
    sendDelete = function (type, id, callback) {
      routes[type].del(id, callback);
    },
    
    sendPut = function (type, id, data, callback) {
      routes[type].put(id, data, callback);
    },
    
    parseConfig = function(config) {
      /*
      if (config.host === undefined) {
        arangodb = "http://" + document.location.host;
      } else {
        arangodb = config.host;
      }
      */
      if (config.width !== undefined) {
        absAdapter.setWidth(config.width);
      }
      if (config.height !== undefined) {
        absAdapter.setHeight(config.height);
      }
    },

    parseResult = function (result, callback) {
      var inserted = {},
        first = result.first,
        oldLength = nodes.length;
      first = absAdapter.insertNode(first);
      _.each(result.nodes, function(n) {
        n = absAdapter.insertNode(n);
        if (oldLength < nodes.length) {
          inserted[n._id] = n;
          oldLength = nodes.length;
        }
      });
      _.each(result.edges, function(e) {
        absAdapter.insertEdge(e);
      });
      delete inserted[first._id];
      absAdapter.checkSizeOfInserted(inserted);
      absAdapter.checkNodeLimit(first);
      if (callback !== undefined && _.isFunction(callback)) {
        callback(first);
      }
    };

  config = config || {};
  
  parseConfig(config);
  fillRoutes();

  self.explore = absAdapter.explore;
  
  self.loadNode = function(nodeId, callback) {
    sendGet("query", nodeId, function(result) {
      parseResult(result, callback);
    });
  };

  self.requestCentralityChildren = function(nodeId, callback) {
    /*
    sendQuery(queries.childrenCentrality,{
      id: nodeId
    }, function(res) {
      callback(res[0]);
    });
    */
  };
  
  self.createEdge = function (edgeToAdd, callback) {
    var toSend = _.clone(edgeToAdd);
    toSend._from = edgeToAdd.source._id;
    toSend._to = edgeToAdd.target._id;
    delete toSend.source;
    delete toSend.target;
    sendPost("edges", toSend, function(data) {
      data._from = edgeToAdd.source._id;
      data._to = edgeToAdd.target._id;
      delete data.error;
      var edge = absAdapter.insertEdge(data);
      if (callback !== undefined && _.isFunction(callback)) {
        callback(edge);
      }
    });
  };
  
  self.deleteEdge = function (edgeToRemove, callback) {
    sendDelete("edges", edgeToRemove._id, function() {
      absAdapter.removeEdge(edgeToRemove);
      if (callback !== undefined && _.isFunction(callback)) {
        callback();
      }
    });
  };
  
  self.patchEdge = function (edgeToPatch, patchData, callback) {
    sendPut("edges", edgeToPatch._id, patchData, function(data) {
      edgeToPatch._data = $.extend(edgeToPatch._data, patchData);
      if (callback !== undefined && _.isFunction(callback)) {
        callback();
      }
    });
  };
  
  self.createNode = function (nodeToAdd, callback) {
    sendPost("nodes", nodeToAdd, function(data) {
      absAdapter.insertNode(data);
      if (callback !== undefined && _.isFunction(callback)) {
        callback(data);
      }
    });
  };
  
  self.deleteNode = function (nodeToRemove, callback) {
    sendDelete("nodes", nodeToRemove._id, function() {
      absAdapter.removeEdgesForNode(nodeToRemove);
      sendDelete("forNode", nodeToRemove._id, function() {});
      absAdapter.removeNode(nodeToRemove);
      if (callback !== undefined && _.isFunction(callback)) {
        callback();
      }
    });
  };
  
  self.patchNode = function (nodeToPatch, patchData, callback) {
    sendPut("nodes", nodeToPatch._id, patchData, function(data) {
      nodeToPatch._data = $.extend(nodeToPatch._data, patchData);
      if (callback !== undefined && _.isFunction(callback)) {
        callback(nodeToPatch);
      }
    });
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
  
}