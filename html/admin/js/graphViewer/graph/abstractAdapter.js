/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _ */
/*global NodeReducer */
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


function AbstractAdapter(nodes, edges) {
  "use strict";
  
  if (nodes === undefined) {
    throw "The nodes have to be given.";
  }
  if (edges === undefined) {
    throw "The edges have to be given.";
  }
  
  var self = this,
    initialX = {},
    initialY = {},
    cachedCommunities = {},
    joinedInCommunities = {},
    limit,
    reducer,
    childLimit,
    exports = {},

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
    
    checkSizeOfInserted = function (inserted) {
      var buckets;
      if (_.size(inserted) > childLimit) {
        buckets = reducer.bucketNodes(_.values(inserted), childLimit);
        _.each(buckets, function(b) {
          if (b.length > 1) {
            var ids = _.map(b, function(n) {
              return n._id;
            });
            collapseCommunity(ids);
          }
        });
      }
    },
    
    checkNodeLimit = function (focus) {
      if (limit < nodes.length) {
        var com = reducer.getCommunity(limit, focus);
        collapseCommunity(com);
      }
    },
    
    setNodeLimit = function (pLimit, callback) {
      limit = pLimit;
      if (limit < nodes.length) {
        var com = reducer.getCommunity(limit);
        collapseCommunity(com);
        if (callback !== undefined) {
          callback();
        }
      }
    },
    
    setChildLimit = function (pLimit) {
      childLimit = pLimit;
    };
  
  childLimit = Number.POSITIVE_INFINITY;
  
  reducer = new NodeReducer(nodes, edges);
  
  initialX.getStart = function() {return 0;};
  initialY.getStart = function() {return 0;};
  
  exports.setWidth = setWidth;
  exports.setHeight = setHeight;
  exports.insertNode = insertNode;
  exports.insertEdge = insertEdge;

  exports.removeNode = removeNode;
  exports.removeEdge = removeEdge;
  exports.removeEdgesForNode = removeEdgesForNode;  
  
  exports.expandCommunity = expandCommunity;
  
  exports.setNodeLimit = setNodeLimit;
  exports.setChildLimit = setChildLimit;
  
  exports.checkSizeOfInserted = checkSizeOfInserted;
  exports.checkNodeLimit = checkNodeLimit;
  
  return exports;
}