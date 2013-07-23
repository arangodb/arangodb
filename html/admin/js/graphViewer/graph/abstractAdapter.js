/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
/*global $, _ */
/*global console */
/*global NodeReducer, ModularityJoiner, WebWorkerWrapper, CommunityNode*/
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


function AbstractAdapter(nodes, edges, descendant, config) {
  "use strict";
  
  if (nodes === undefined) {
    throw "The nodes have to be given.";
  }
  if (edges === undefined) {
    throw "The edges have to be given.";
  }
  if (descendant === undefined) {
    throw "An inheriting class has to be given.";
  }
  config = config || {};
  
  var self = this,
    isRunning = false,
    initialX = {},
    initialY = {},
    cachedCommunities = {},
    joinedInCommunities = {},
    limit,
    reducer,
    joiner,
    childLimit,

    changeTo = function (config) {
      if (config.prioList !== undefined) {
        reducer.changePrioList(config.prioList || []);
      }
    },

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
    
    insertInitialNode = function(data) {
      var n = insertNode(data);
      n.x = initialX.start * 2;
      n.y = initialY.start * 2;
      n.fixed = true;
      return n;
    },
    
    cleanUp = function() {
      nodes.length = 0;
      edges.length = 0;
      joinedInCommunities = {};
      cachedCommunities = {};
    },
    
    insertEdge = function(data) {
      var source,
        target,
        informJoiner = true,
        edge = {
          _data: data,
          _id: data._id
        },
        e = findEdge(edge._id),
        edgeToPush,
        com;
      if (e) {
        return e;
      }
      source = findNode(data._from);
      target = findNode(data._to);
      if (!source) {
        throw "Unable to insert Edge, source node not existing " + data._from;
      }
      if (!target) {
        throw "Unable to insert Edge, target node not existing " + data._to;
      }
      edge.source = source;
      if (edge.source._isCommunity) {
        com = cachedCommunities[edge.source._id];
        edge.source = com.getNode(data._from);
        edge.source._outboundCounter++;
        com.insertOutboundEdge(edge);
        informJoiner = false;
      } else {
        source._outboundCounter++;
      }
      edge.target = target;
      if (edge.target._isCommunity) {
        com = cachedCommunities[edge.target._id];
        edge.target = com.getNode(data._to);
        edge.target._inboundCounter++;
        com.insertInboundEdge(edge);
        informJoiner = false;
      } else {
        target._inboundCounter++;
      }
      edges.push(edge);
      if (informJoiner) {
        joiner.call("insertEdge", source._id, target._id);
      }
      
      
      /* Archive
      if (cachedCommunities[source._id] !== undefined) {
        edgeToPush = {};
        edgeToPush.type = "s";
        edgeToPush._id = edge._id;
        edgeToPush.source = $.grep(cachedCommunities[source._id].nodes, function(e){
          return e._id === data._from;
        })[0];
        edgeToPush.source._outboundCounter++;
        cachedCommunities[source._id].edges.push(edgeToPush);
        informJoiner = false;
      } else {
        source._outboundCounter++;
      }
      if (cachedCommunities[target._id] !== undefined) {
        edgeToPush = {};
        edgeToPush.type = "t";
        edgeToPush._id = edge._id;
        edgeToPush.target = $.grep(cachedCommunities[target._id].nodes, function(e){
          return e._id === data._to;
        })[0];
        edgeToPush.target._inboundCounter++;
        cachedCommunities[target._id].edges.push(edgeToPush);
        informJoiner = false;
      } else {
        target._inboundCounter++;
      }
      if (informJoiner) {
        joiner.call("insertEdge", source._id, target._id);
      }
      */
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
    
    removeEdgeWithIndex = function (index, notInformJoiner) {
      var e = edges[index],
        s = e.source._id,
        t = e.target._id;
      edges.splice(index, 1);
      if (!notInformJoiner) {
        joiner.call("deleteEdge",s , t);
      }
    },
  
    removeEdge = function (edge, notInformJoiner) {
      var i;
      for ( i = 0; i < edges.length; i++ ) {
        if ( edges[i] === edge ) {
          removeEdgeWithIndex(i, notInformJoiner);
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
          removeEdgeWithIndex(i);
          i--;
        } else if (edges[i].target === node) {
          node._inboundCounter--;
          edges[i].source._outboundCounter--;
          removeEdgeWithIndex(i);
          i--;
        }
      }
    },
    /* Archive
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
              edges.splice(i, 1);
              i--;
              break;
            }
            edges[i].source = commNode;
            edgeToPush.type = "s";
            edgeToPush._id = edges[i]._id;
            edgeToPush.source = s;
            
            if (!/^\*community/.test(t._id)) {
              joiner.call("deleteEdge", s._id, t._id);
            }            
          }
          if (t === nodes[j]) {
            if (edgeToPush.type !== undefined) {
              edges[i].source = edgeToPush.source;
              delete edgeToPush.source;
              edgeToPush.type = "b";
              edgeToPush.edge = edges[i];
              edges.splice(i, 1);
              i--;
              break;
            }
            edges[i].target = commNode;
            edgeToPush.type = "t";
            edgeToPush._id = edges[i]._id;
            edgeToPush.target = t;
            if (!/^\*community/.test(s._id)) {
              joiner.call("deleteEdge", s._id, t._id);
            }            
          }
        }
        if (edgeToPush.type !== undefined) {
          cachedCommEdges.push(edgeToPush);
        }
      }
    },
    */
    combineCommunityEdges = function (nodes, commNode) {
      var i, j, s, t, shouldRemove;
      for (i = 0; i < edges.length; i++) {
        // s and t keep old values yay!
        s = edges[i].source;
        t = edges[i].target;
        for (j = 0; j < nodes.length; j++) {
          shouldRemove = false;
          if (s === nodes[j]) {
            shouldRemove = commNode.insertOutboundEdge(edges[i]);
            if (!t._isCommunity) {
              joiner.call("deleteEdge", s._id, t._id);
            } 
            s = edges[i].source;           
          }
          if (t === nodes[j]) {
            shouldRemove = commNode.insertInboundEdge(edges[i]);
            if (!s._isCommunity) {
              joiner.call("deleteEdge", s._id, t._id);
            }
            t = edges[i].target;          
          }
          if (shouldRemove) {
            edges.splice(i, 1);
            i--;
          }
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
            removeEdgeWithIndex(i, edges[i].target._isCommunity);
            if (node._outboundCounter === 0) {
              break;
            }
            i--;
          }
        }
        return removed;
      }
    },
    /* Archive
    collapseCommunity = function (community, reason) {
      if (!community || community.length === 0) {
        return;
      }
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
      commNode._size = community.length;
      if (reason) {
        commNode._reason = reason;
      }
      cachedCommunities[commId] = {};
      cachedCommunities[commId].nodes = nodesToRemove;
      cachedCommunities[commId].edges = [];
      
      combineCommunityEdges(nodesToRemove, commNode);
      _.each(nodesToRemove, function(n) {
        joinedInCommunities[n._id] = commId;
        removeNode(n);
      });
      nodes.push(commNode);
      isRunning = false;
    },
    */
    
    collapseCommunity = function (community, reason) {
      if (!community || community.length === 0) {
        return;
      }
      var 
        nodesToRemove = _.map(community, function(id) {
          return findNode(id);
        }),
        commNode = new CommunityNode(self, nodesToRemove),
        commId = commNode._id;
      if (reason) {
        commNode._reason = reason;
      }
      cachedCommunities[commId] = commNode;
      
      combineCommunityEdges(nodesToRemove, commNode);
      _.each(nodesToRemove, function(n) {
        joinedInCommunities[n._id] = commId;
        removeNode(n);
      });
      nodes.push(commNode);
      isRunning = false;
    },
    
    joinerCb = function (d) {
      var data = d.data;
      if (data.error) {
        console.log(data.cmd);
        console.log(data.error);
        return;
      }
      switch (data.cmd) {
        case "debug": 
          //console.log(data.result);
          break;
        case "getCommunity":
          collapseCommunity(data.result);
          break;
        default:
      }
    },
    
    requestCollapse = function (focus) {
      if (isRunning) {
        return;
      }
      isRunning = true;
      if (focus) {
        joiner.call("getCommunity", limit, focus._id);
      } else {
        joiner.call("getCommunity", limit);
      }
    },
  
    checkNodeLimit = function (focus) {
      var curRendered = nodes.length,
        commToColapse,
        bestComVal = -Infinity;
      _.each(cachedCommunities, function(c) {
        if (c._expanded === true) {
          if (bestComVal < c._size && c !== focus) {
            commToColapse = c;
            bestComVal = c._size;
          }
          curRendered += c._size;
        }
      });
      if (limit < curRendered) {
        if (commToColapse) {
          commToColapse.collapse();
        } else {
          requestCollapse(focus);
        }
      }
    },
    /* Archive
    expandCommunity = function (commNode) {
      var commId = commNode._id,
        nodesToAdd = cachedCommunities[commId].nodes,
        edgesToChange = cachedCommunities[commId].edges,
        com;
      removeNode(commNode);
      if (limit < nodes.length + nodesToAdd.length) {
        requestCollapse();
      }
      _.each(nodesToAdd, function(n) {
        delete joinedInCommunities[n._id];
        nodes.push(n);
      });
      _.each(edgesToChange, function(e) {
        var edge;
        switch(e.type) {
          case "t":
            edge = findEdge(e._id);
            edge.target = e.target;
            if (!/^\*community/.test(edge.source._id)) {
              joiner.call("insertEdge", edge.source._id, edge.target._id);
            }
            break;
          case "s":
            edge = findEdge(e._id);
            edge.source = e.source;
            if (!/^\*community/.test(edge.target._id)) {
              joiner.call("insertEdge", edge.source._id, edge.target._id);
            }
            break;
          case "b":
            edges.push(e.edge);
            joiner.call("insertEdge", e.edge.source._id, e.edge.target._id);
            break;
        }
        
      });
      delete cachedCommunities[commId];      
    },
    */
    
    dissolveCommunity = function (commNode) {
      var dissolveInfo = commNode.getDissolveInfo(),
        nodesToAdd = dissolveInfo.nodes,
        internalEdges = dissolveInfo.edges.both,
        inboundEdges = dissolveInfo.edges.inbound,
        outboundEdges = dissolveInfo.edges.outbound;
      removeNode(commNode);
      if (limit < nodes.length + nodesToAdd.length) {
        requestCollapse();
      }
      _.each(nodesToAdd, function(n) {
        delete joinedInCommunities[n._id];
        nodes.push(n);
      });
      _.each(inboundEdges, function(edge) {
        edge.target = edge._target;
        delete edge._target;
        if (!edge.source._isCommunity) {
          joiner.call("insertEdge", edge.source._id, edge.target._id);
        }
      });
      _.each(outboundEdges, function(edge) {
        edge.source = edge._source;
        delete edge._source;
        if (!edge.target._isCommunity) {
          joiner.call("insertEdge", edge.source._id, edge.target._id);
        }
      });
      _.each(internalEdges, function(edge) {
        edge.source = edge._source;
        delete edge._source;
        edge.target = edge._target;
        delete edge._target;
        edges.push(edge);
        joiner.call("insertEdge", edge.source._id, edge.target._id);
      });
      delete cachedCommunities[commNode._id];      
    },
    
    expandCommunity = function(commNode) {
      commNode.expand();
      checkNodeLimit(commNode);
    },
    
    checkSizeOfInserted = function (inserted) {
      if (_.size(inserted) > childLimit) {
        var buckets = reducer.bucketNodes(_.values(inserted), childLimit);
        _.each(buckets, function(b) {
          if (b.nodes.length > 1) {
            var ids = _.map(b.nodes, function(n) {
              return n._id;
            });
            collapseCommunity(ids, b.reason);
          }
        });
      }
    },
    
    setNodeLimit = function (pLimit, callback) {
      limit = pLimit;
      checkNodeLimit();
      if (callback !== undefined) {
        callback();
      }
    },
    
    setChildLimit = function (pLimit) {
      childLimit = pLimit;
    },
        
    handleRemovedEdge,
    
    collapseNodeInCommunity = function(node, commNode) {
      node._expanded = false;
      var removedEdges = commNode.removeOutboundEdgesFromNode(node);
      _.each(removedEdges, function(e) {
        handleRemovedEdge(e);
        removeEdge(e, true);
      });
    },
    
    collapseNode = function(node) {
      node._expanded = false;
      var removedEdges = removeOutboundEdgesFromNode(node);
      _.each(removedEdges, handleRemovedEdge);
    },
    
    collapseExploreCommunity = function(commNode) {
      var disInfo = commNode.getDissolveInfo();
      removeNode(commNode);
      _.each(disInfo.nodes, function (n) {
        delete joinedInCommunities[n._id];
      });
      _.each(disInfo.edges.outbound, function(e) {
        handleRemovedEdge(e);
        removeEdge(e, true);
      });
      delete cachedCommunities[commNode._id];
    },
    
    expandNode = function(n, startCallback) {
      if (n._isCommunity) {
        console.log("Adapter Explore!");
        self.expandCommunity(n, startCallback);
      } else {
        n._expanded = true;
        descendant.loadNode(n._id, startCallback);
      }
    },
    
    explore = function (node, startCallback) {
      if (!node._expanded) {
        expandNode(node, startCallback);
      } else {
        collapseNode(node);
      }
      
    };
  
  handleRemovedEdge = function (e) {
    var n = e.target, t;
    if (n._isCommunity) {
      t = e._target;
      n.removeInboundEdge(e);
      t._inboundCounter--;
      if (t._inboundCounter === 0) {
        collapseNodeInCommunity(t, n);
        n.removeNode(t);
        delete joinedInCommunities[t._id];
      }
      if (n._inboundCounter === 0) {
        collapseExploreCommunity(n);
      }
      return;
    }
    n._inboundCounter--;
    if (n._inboundCounter === 0) {
      collapseNode(n);
      removeNode(n);
    }
  };
  
  childLimit = Number.POSITIVE_INFINITY;
  
  if (config.prioList) {
    reducer = new NodeReducer(nodes, edges, config.prioList);
  } else {
    reducer = new NodeReducer(nodes, edges);
  }
  joiner = new WebWorkerWrapper(ModularityJoiner, joinerCb);
  
  initialX.getStart = function() {return 0;};
  initialY.getStart = function() {return 0;};
  
  this.cleanUp = cleanUp;
  
  this.setWidth = setWidth;
  this.setHeight = setHeight;
  this.insertNode = insertNode;
  this.insertInitialNode = insertInitialNode;
  this.insertEdge = insertEdge;

  this.removeNode = removeNode;
  this.removeEdge = removeEdge;
  this.removeEdgesForNode = removeEdgesForNode;  
  
  this.expandCommunity = expandCommunity;
  
  this.setNodeLimit = setNodeLimit;
  this.setChildLimit = setChildLimit;
  
  this.checkSizeOfInserted = checkSizeOfInserted;
  this.checkNodeLimit = checkNodeLimit;
  
  this.explore = explore;
  
  this.changeTo = changeTo;
  
  this.getPrioList = reducer.getPrioList;
  
  this.dissolveCommunity = dissolveCommunity;
}