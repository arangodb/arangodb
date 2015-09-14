/*global $, d3, _, console, alert*/
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

function JSONAdapter(jsonPath, nodes, edges, viewer, width, height) {
  "use strict";

  var self = this,
  initialX = {},
  initialY = {},
  absAdapter = new AbstractAdapter(nodes, edges, this, viewer),
  findNode = function(n) {
    var res = $.grep(nodes, function(e){
      return e._id === n._id;
    });
    if (res.length === 0) {
      return false;
    }
    if (res.length === 1) {
      return res[0];
    }
    throw "Too many nodes with the same ID, should never happen";
  },
  insertNode = function(data) {
    var node = {
      _data: data,
      _id: data._id,
      children: data.children
    };
    delete data.children;
    initialY.getStart();
    node.x = initialX.getStart();
    node.y = initialY.getStart();
    nodes.push(node);
    node._outboundCounter = 0;
    node._inboundCounter = 0;
    return node;
  },

  insertEdge = function(source, target) {
    edges.push({source: source, target: target});
    source._outboundCounter++;
    target._inboundCounter++;
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

  self.loadNode = function(nodeId, callback) {
    self.loadNodeFromTreeById(nodeId, callback);
  };

  self.loadInitialNode = function(nodeId, callback) {
    var json = jsonPath + nodeId + ".json";
    absAdapter.cleanUp();
    d3.json(json, function(error, node) {
      if (error !== undefined && error !== null) {
        console.log(error);
      }
      var n = absAdapter.insertInitialNode(node);
      self.requestCentralityChildren(nodeId, function(c) {
        n._centrality = c;
      });
      _.each(node.children, function(c) {
        var t = absAdapter.insertNode(c),
          e = {
            _from: n._id,
            _to: t._id,
            _id: n._id + "-" + t._id
          };
        absAdapter.insertEdge(e);
        self.requestCentralityChildren(t._id, function(c) {
          t._centrality = c;
        });
        delete t._data.children;
      });
      delete n._data.children;
      if (callback) {
        callback(n);
      }
    });
  };

  self.loadNodeFromTreeById = function(nodeId, callback) {
    var json = jsonPath + nodeId + ".json";
    d3.json(json, function(error, node) {
      if (error !== undefined && error !== null) {
        console.log(error);
      }
      var n = absAdapter.insertNode(node);
      self.requestCentralityChildren(nodeId, function(c) {
        n._centrality = c;
      });
      _.each(node.children, function(c) {
        var check = absAdapter.insertNode(c),
        e = {
          _from: n._id,
          _to: check._id,
          _id: n._id + "-" + check._id
        };
        absAdapter.insertEdge(e);
        self.requestCentralityChildren(check._id, function(c) {
          n._centrality = c;
        });
        delete check._data.children;
      });
      delete n._data.children;
      if (callback) {
        callback(n);
      }
    });
  };

  self.requestCentralityChildren = function(nodeId, callback) {
    var json = jsonPath + nodeId + ".json";
    d3.json(json, function(error, node) {
      if (error !== undefined && error !== null) {
        console.log(error);
      }
      if (callback !== undefined) {
        if (node.children !== undefined) {
          callback(node.children.length);
        } else {
          callback(0);
        }
      }
    });
  };

  self.loadNodeFromTreeByAttributeValue = function(attribute, value, callback) {
    throw "Sorry this adapter is read-only";
  };

  self.loadInitialNodeByAttributeValue = function(attribute, value, callback) {
    throw "Sorry this adapter is read-only";
  };

  self.createEdge = function(edgeToCreate, callback){
      throw "Sorry this adapter is read-only";
  };

  self.deleteEdge = function(edgeToDelete, callback){
      throw "Sorry this adapter is read-only";
  };

  self.patchEdge = function(edgeToPatch, patchData, callback){
      throw "Sorry this adapter is read-only";
  };

  self.createNode = function(nodeToCreate, callback){
      throw "Sorry this adapter is read-only";
  };

  self.deleteNode = function(nodeToDelete, callback){
      throw "Sorry this adapter is read-only";
  };

  self.patchNode = function(nodeToPatch, patchData, callback){
      throw "Sorry this adapter is read-only";
  };

  self.setNodeLimit = function (limit, callback) {

  };

  self.setChildLimit = function (limit) {

  };

  self.expandCommunity = function (commNode, callback) {

  };

  self.setWidth = function() {
  };

  self.explore = absAdapter.explore;

}

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


function AbstractAdapter(nodes, edges, descendant, viewer, config) {
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
  if (viewer === undefined) {
    throw "A reference to the graph viewer has to be given.";
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

    insertNode = function(data, x, y) {
      var node = {
        _data: data,
        _id: data._id
      },
        n = findNode(node._id);
      if (n) {
        return n;
      }
      node.x = x || initialX.getStart();
      node.y = y || initialY.getStart();
      node.weight = 1;
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
      viewer.cleanUp();
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
      if (joinedInCommunities[node._id]) {
        cachedCommunities[joinedInCommunities[node._id]].collapseNode(node);
      }
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
    reducer = new NodeReducer(config.prioList);
  } else {
    reducer = new NodeReducer();
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

function ArangoAdapter(nodes, edges, viewer, config) {

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
          if (query !== queries.childrenCentrality) {
            bindVars.dir = direction;
          }
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


    insertInitialCallback = function(callback) {
      return function (n) {
        if (n && n.errorCode) {
          callback(n);
          return;
        }
        callback(absAdapter.insertInitialNode(n));
      };
    },


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
  absAdapter = new AbstractAdapter(nodes, edges, this, viewer, absConfig);

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
    self.loadNode(nodeId, insertInitialCallback(callback));
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
    self.loadNodeFromTreeByAttributeValue(attribute, value, insertInitialCallback(callback));
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

/*global _, $, d3*/
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

function ColourMapper() {
  "use strict";

  var mapping = {},
    reverseMapping = {},
    colours = [],
    listener,
    self = this,
    nextColour = 0;

  colours.push({back: "navy", front: "white"});
  colours.push({back: "green", front: "white"});
  colours.push({back: "gold", front: "black"});
  colours.push({back: "red", front: "black"});
  colours.push({back: "saddlebrown", front: "white"});
  colours.push({back: "skyblue", front: "black"});
  colours.push({back: "olive", front: "black"});
  colours.push({back: "deeppink", front: "black"});
  colours.push({back: "orange", front: "black"});
  colours.push({back: "silver", front: "black"});
  colours.push({back: "blue", front: "white"});
  colours.push({back: "yellowgreen", front: "black"});
  colours.push({back: "firebrick", front: "black"});
  colours.push({back: "rosybrown", front: "black"});
  colours.push({back: "hotpink", front: "black"});
  colours.push({back: "purple", front: "white"});
  colours.push({back: "cyan", front: "black"});
  colours.push({back: "teal", front: "black"});
  colours.push({back: "peru", front: "black"});
  colours.push({back: "maroon", front: "white"});

  this.getColour = function(value) {
    if (mapping[value] === undefined) {
      mapping[value] = colours[nextColour];
      if (reverseMapping[colours[nextColour].back] === undefined) {
        reverseMapping[colours[nextColour].back] = {
          front: colours[nextColour].front,
          list: []
        };
      }
      reverseMapping[colours[nextColour].back].list.push(value);
      nextColour++;
      if (nextColour === colours.length) {
        nextColour = 0;
      }
    }
    if (listener !== undefined) {
      listener(self.getList());
    }
    return mapping[value].back;
  };

  this.getCommunityColour = function() {
    return "#333333";
  };

  this.getForegroundColour = function(value) {
    if (mapping[value] === undefined) {
      mapping[value] = colours[nextColour];
      if (reverseMapping[colours[nextColour].back] === undefined) {
        reverseMapping[colours[nextColour].back] = {
          front: colours[nextColour].front,
          list: []
        };
      }
      reverseMapping[colours[nextColour].back].list.push(value);
      nextColour++;
      if (nextColour === colours.length) {
        nextColour = 0;
      }
    }
    if (listener !== undefined) {
      listener(self.getList());
    }
    return mapping[value].front;
  };

  this.getForegroundCommunityColour = function() {
    return "white";
  };



  this.reset = function() {
    mapping = {};
    reverseMapping = {};
    nextColour = 0;
    if (listener !== undefined) {
      listener(self.getList());
    }
  };

  this.getList = function() {
    return reverseMapping;
  };

  this.setChangeListener = function(callback) {
    listener = callback;
  };

  this.reset();
}

/*global _, document, ForceLayouter, DomObserverFactory*/
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



function CommunityNode(parent, initial) {
  "use strict";

  if (_.isUndefined(parent)
    || !_.isFunction(parent.dissolveCommunity)
    || !_.isFunction(parent.checkNodeLimit)) {
    throw "A parent element has to be given.";
  }

  initial = initial || [];

  var

  ////////////////////////////////////
  // Private variables              //
  ////////////////////////////////////
    self = this,
    bBox,
    bBoxBorder,
    bBoxTitle,
    nodes = {},
    observer,
    nodeArray = [],
    intEdgeArray = [],
    internal = {},
    inbound = {},
    outbound = {},
    outReferences = {},
    layouter,
  ////////////////////////////////////
  // Private functions              //
  ////////////////////////////////////

    getDistance = function(def) {
      if (self._expanded) {
        return 2 * def * Math.sqrt(nodeArray.length);
      }
      return def;
    },

    getCharge = function(def) {
      if (self._expanded) {
        return 4 * def * Math.sqrt(nodeArray.length);
      }
      return def;
    },

    compPosi = function(p) {
      var d = self.position,
        x = p.x * d.z + d.x,
        y = p.y * d.z + d.y,
        z = p.z * d.z;
      return {
        x: x,
        y: y,
        z: z
      };
    },

    getSourcePosition = function(e) {
      if (self._expanded) {
        return compPosi(e._source.position);
      }
      return self.position;
    },


    getTargetPosition = function(e) {
      if (self._expanded) {
        return compPosi(e._target.position);
      }
      return self.position;
    },

    updateBoundingBox = function() {
      var boundingBox = document.getElementById(self._id).getBBox();
      bBox.attr("transform", "translate(" + (boundingBox.x - 5) + "," + (boundingBox.y - 25) + ")");
      bBoxBorder.attr("width", boundingBox.width + 10)
        .attr("height", boundingBox.height + 30);
      bBoxTitle.attr("width", boundingBox.width + 10);
    },

    getObserver = function() {
      if (!observer) {
        var factory = new DomObserverFactory();
        observer = factory.createObserver(function(e){
          if (_.any(e, function(obj) {
            return obj.attributeName === "transform";
          })) {
            updateBoundingBox();
            observer.disconnect();
          }
        });
      }
      return observer;
    },

    updateNodeArray = function() {
      layouter.stop();
      nodeArray.length = 0;
      _.each(nodes, function(v) {
        nodeArray.push(v);
      });
      layouter.start();
    },

    updateEdgeArray = function() {
      layouter.stop();
      intEdgeArray.length = 0;
      _.each(internal, function(e) {
        intEdgeArray.push(e);
      });
      layouter.start();
    },

    toArray = function(obj) {
      var res = [];
      _.each(obj, function(v) {
        res.push(v);
      });
      return res;
    },

    hasNode = function(id) {
      return !!nodes[id];
    },

    getNodes = function() {
      return nodeArray;
    },

    getNode = function(id) {
      return nodes[id];
    },

    insertNode = function(n) {
      nodes[n._id] = n;
      updateNodeArray();
      self._size++;
    },

    insertInitialNodes = function(ns) {
      _.each(ns, function(n) {
        nodes[n._id] = n;
        self._size++;
      });
      updateNodeArray();
    },

    removeNode = function(n) {
      var id = n._id || n;
      delete nodes[id];
      updateNodeArray();
      self._size--;
    },

    removeInboundEdge = function(e) {
      var id;
      if (!_.has(e, "_id")) {
        id = e;
        e = internal[id] || inbound[id];
      } else {
        id = e._id;
      }
      e.target = e._target;
      delete e._target;
      if (internal[id]) {
        delete internal[id];
        self._outboundCounter++;
        outbound[id] = e;
        updateEdgeArray();
        return;
      }
      delete inbound[id];
      self._inboundCounter--;
      return;
    },

    removeOutboundEdge = function(e) {
      var id;
      if (!_.has(e, "_id")) {
        id = e;
        e = internal[id] || outbound[id];
      } else {
        id = e._id;
      }
      e.source = e._source;
      delete e._source;
      delete outReferences[e.source._id][id];
      if (internal[id]) {
        delete internal[id];
        self._inboundCounter++;
        inbound[id] = e;
        updateEdgeArray();
        return;
      }
      delete outbound[id];
      self._outboundCounter--;
      return;
    },

    removeOutboundEdgesFromNode = function(n) {
      var id = n._id || n,
        res = [];
      _.each(outReferences[id], function(e) {
        removeOutboundEdge(e);
        res.push(e);
      });
      delete outReferences[id];
      return res;
    },

    insertInboundEdge = function(e) {
      e._target = e.target;
      e.target = self;
      if (outbound[e._id]) {
        delete outbound[e._id];
        self._outboundCounter--;
        internal[e._id] = e;
        updateEdgeArray();
        return true;
      }
      inbound[e._id] = e;
      self._inboundCounter++;
      return false;
    },

    insertOutboundEdge = function(e) {
      var sId = e.source._id;
      e._source = e.source;
      e.source = self;
      outReferences[sId] = outReferences[sId] || {};
      outReferences[sId][e._id] = e;
      if (inbound[e._id]) {
        delete inbound[e._id];
        self._inboundCounter--;
        internal[e._id] = e;
        updateEdgeArray();
        return true;
      }
      self._outboundCounter++;
      outbound[e._id] = e;
      return false;
    },

    getDissolveInfo = function() {
      return {
        nodes: nodeArray,
        edges: {
          both: intEdgeArray,
          inbound: toArray(inbound),
          outbound: toArray(outbound)
        }
      };
    },

    expand = function() {
      this._expanded = true;
    },

    dissolve = function() {
      parent.dissolveCommunity(self);
    },

    collapse = function() {
      this._expanded = false;
    },

    addCollapsedLabel = function(g, colourMapper) {
      var width = g.select("rect").attr("width"),
        textN = g.append("text") // Append a label for the node
          .attr("text-anchor", "middle") // Define text-anchor
          .attr("fill", colourMapper.getForegroundCommunityColour())
          .attr("stroke", "none"); // Make it readable
      width *= 2;
      width /= 3;
      if (self._reason && self._reason.key) {
        textN.append("tspan")
          .attr("x", "0")
          .attr("dy", "-4")
          .text(self._reason.key + ":");
        textN.append("tspan")
          .attr("x", "0")
          .attr("dy", "16")
          .text(self._reason.value);
      }
      textN.append("tspan")
        .attr("x", width)
        .attr("y", "0")
        .attr("fill", colourMapper.getCommunityColour())
        .text(self._size);
    },

    addCollapsedShape = function(g, shapeFunc, start, colourMapper) {
      var inner = g.append("g")
        .attr("stroke", colourMapper.getForegroundCommunityColour())
        .attr("fill", colourMapper.getCommunityColour());
      shapeFunc(inner, 9);
      shapeFunc(inner, 6);
      shapeFunc(inner, 3);
      shapeFunc(inner);
      inner.on("click", function() {
        self.expand();
        parent.checkNodeLimit(self);
        start();
      });
      addCollapsedLabel(inner, colourMapper);
    },

    addNodeShapes = function(g, shapeQue) {
      var interior = g.selectAll(".node")
      .data(nodeArray, function(d) {
        return d._id;
      });
      interior.enter()
        .append("g")
        .attr("class", "node")
        .attr("id", function(d) {
          return d._id;
        });
      // Remove all old
      interior.exit().remove();
      interior.selectAll("* > *").remove();
      shapeQue(interior);
    },

    addBoundingBox = function(g, start) {
      bBox = g.append("g");
      bBoxBorder = bBox.append("rect")
        .attr("rx", "8")
        .attr("ry", "8")
        .attr("fill", "none")
        .attr("stroke", "black");
      bBoxTitle = bBox.append("rect")
        .attr("rx", "8")
        .attr("ry", "8")
        .attr("height", "20")
        .attr("fill", "#686766")
        .attr("stroke", "none");
      bBox.append("image")
        .attr("id", self._id + "_dissolve")
        .attr("xlink:href", "img/icon_delete.png")
        .attr("width", "16")
        .attr("height", "16")
        .attr("x", "5")
        .attr("y", "2")
        .attr("style", "cursor:pointer")
        .on("click", function() {
          self.dissolve();
          start();
        });
      bBox.append("image")
        .attr("id", self._id + "_collapse")
        .attr("xlink:href", "img/gv_collapse.png")
        .attr("width", "16")
        .attr("height", "16")
        .attr("x", "25")
        .attr("y", "2")
        .attr("style", "cursor:pointer")
        .on("click", function() {
          self.collapse();
          start();
        });
      var title = bBox.append("text")
        .attr("x", "45")
        .attr("y", "15")
        .attr("fill", "white")
        .attr("stroke", "none")
        .attr("text-anchor", "left");
      if (self._reason) {
        title.text(self._reason.text);
      }
      getObserver().observe(document.getElementById(self._id), {
        subtree:true,
        attributes:true
      });
    },

    addDistortion = function(distFunc) {
      if (self._expanded) {
        var oldFocus = distFunc.focus(),
          newFocus = [
            oldFocus[0] - self.position.x,
            oldFocus[1] - self.position.y
          ];
        distFunc.focus(newFocus);
        _.each(nodeArray, function(n) {
          n.position = distFunc(n);
          n.position.x /= self.position.z;
          n.position.y /= self.position.z;
          n.position.z /= self.position.z;
        });
        distFunc.focus(oldFocus);
      }
    },

    shapeAll = function(g, shapeFunc, shapeQue, start, colourMapper) {
      // First unbind all click events that are proably still bound
      g.on("click", null);
      if (self._expanded) {
        addBoundingBox(g, start);
        addNodeShapes(g, shapeQue, start, colourMapper);
        return;
      }
      addCollapsedShape(g, shapeFunc, start, colourMapper);
    },

    updateEdges = function(g, addPosition, addUpdate) {
      if (self._expanded) {
        var interior = g.selectAll(".link"),
          line = interior.select("line");
        addPosition(line, interior);
        addUpdate(interior);
      }
    },

    shapeEdges = function(g, addQue) {
      var idFunction = function(d) {
          return d._id;
        },
	line,
	interior;
      if (self._expanded) {
        interior = g
          .selectAll(".link")
          .data(intEdgeArray, idFunction);
        // Append the group and class to all new
        interior.enter()
          .append("g")
          .attr("class", "link") // link is CSS class that might be edited
          .attr("id", idFunction);
        // Remove all old
        interior.exit().remove();
        // Remove all elements that are still included.
        interior.selectAll("* > *").remove();
        line = interior.append("line");
        addQue(line, interior);
      }
    },

    collapseNode = function(n) {
      removeOutboundEdgesFromNode(n);
    };

  ////////////////////////////////////
  // Setup                          //
  ////////////////////////////////////

  layouter = new ForceLayouter({
    distance: 100,
    gravity: 0.1,
    charge: -500,
    width: 1,
    height: 1,
    nodes: nodeArray,
    links: intEdgeArray
  });

  ////////////////////////////////////
  // Values required for shaping    //
  ////////////////////////////////////
  this._id = "*community_" + Math.floor(Math.random()* 1000000);
  if (initial.length > 0) {
    this.x = initial[0].x;
    this.y = initial[0].y;
  } else {
    this.x = 0;
    this.y = 0;
  }
  this._size = 0;
  this._inboundCounter = 0;
  this._outboundCounter = 0;
  this._expanded = false;
  // Easy check for the other classes,
  // no need for a regex on the _id any more.
  this._isCommunity = true;

  insertInitialNodes(initial);

  ////////////////////////////////////
  // Public functions               //
  ////////////////////////////////////

  this.hasNode = hasNode;
  this.getNodes = getNodes;
  this.getNode = getNode;
  this.getDistance = getDistance;
  this.getCharge = getCharge;


  this.insertNode = insertNode;
  this.insertInboundEdge = insertInboundEdge;
  this.insertOutboundEdge = insertOutboundEdge;

  this.removeNode = removeNode;
  this.removeInboundEdge = removeInboundEdge;
  this.removeOutboundEdge = removeOutboundEdge;
  this.removeOutboundEdgesFromNode = removeOutboundEdgesFromNode;


  this.collapseNode = collapseNode;

  this.dissolve = dissolve;
  this.getDissolveInfo = getDissolveInfo;

  this.collapse = collapse;
  this.expand = expand;

  this.shapeNodes = shapeAll;
  this.shapeInnerEdges = shapeEdges;
  this.updateInnerEdges = updateEdges;


  this.addDistortion = addDistortion;

  this.getSourcePosition = getSourcePosition;

  this.getTargetPosition = getTargetPosition;
}

/*global window */

//////////////////////////////////
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


function DomObserverFactory() {
  "use strict";

  var Constructor = window.WebKitMutationObserver || window.MutationObserver;

  this.createObserver = function(callback) {
    if (!Constructor) {
      throw "Observer not supported";
    }
    return new Constructor(callback);
  };
}

/*global _, $, d3*/
/*global ColourMapper, ContextMenu*/
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

/*
* config example format:
* {
*   shape: {
*     type: EdgeShaper.shapes.ARROW
*   }
*   label: "key" \\ function(edge)
*   actions: {
*     click: function(edge)
*   }
* }
*
*
*/

function EdgeShaper(parent, config, idfunc) {
  "use strict";

  var self = this,
    edges = [],
    communityNodes = {},
    contextMenu = new ContextMenu("gv_edge_cm"),
    findFirstValue = function(list, data) {
      if (_.isArray(list)) {
        return data[_.find(list, function(val) {
          return data[val];
        })];
      }
      return data[list];
    },
    splitLabel = function(label) {
      if (label === undefined) {
        return [""];
      }
      if (typeof label !== "string") {
        label = String(label);
      }
      var chunks = label.match(/[\w\W]{1,10}(\s|$)|\S+?(\s|$)/g);
      chunks[0] = $.trim(chunks[0]);
      chunks[1] = $.trim(chunks[1]);
      if (chunks[0].length > 12) {
        chunks[0] = $.trim(label.substring(0,10)) + "-";
        chunks[1] = $.trim(label.substring(10));
        if (chunks[1].length > 12) {
          chunks[1] = chunks[1].split(/\W/)[0];
          if (chunks[1].length > 12) {
            chunks[1] = chunks[1].substring(0,10) + "...";
          }
        }
        chunks.length = 2;
      }
      if (chunks.length > 2) {
        chunks.length = 2;
        chunks[1] += "...";
      }
      return chunks;
    },
    toplevelSVG,
    visibleLabels = true,
    followEdge = {},
    followEdgeG,
    idFunction = function(d) {
      return d._id;
    },
    noop = function (line, g) {

    },
    colourMapper = new ColourMapper(),
    resetColourMap = function() {
      colourMapper.reset();
    },
    events,
    addUpdate = noop,
    addShape = noop,
    addLabel = noop,
    addColor = noop,

    unbindEvents = function() {
     events = {
       click: noop,
       dblclick: noop,
       mousedown: noop,
       mouseup: noop,
       mousemove: noop,
       mouseout: noop,
       mouseover: noop
     };
    },


    getCorner = function(s, t) {
      return Math.atan2(t.y - s.y, t.x - s.x) * 180 / Math.PI;
    },

    getDistance = function(s, t) {
      var res = Math.sqrt(
        (t.y - s.y)
        * (t.y - s.y)
        + (t.x - s.x)
        * (t.x - s.x)
      ),
      m;
      if (s.x === t.x) {
        res -= t.z * 18;
      } else {
        m = Math.abs((t.y - s.y) / (t.x - s.x));
        if (m < 0.4) {
          res -= Math.abs((res * t.z * 45) / (t.x - s.x));
        } else {
          res -= Math.abs((res * t.z * 18) / (t.y - s.y));
        }
      }
      return res;
    },

    addEvents = function (line, g) {
      _.each(events, function (func, type) {
        g.on(type, func);
      });
    },

    bindEvent = function (type, func) {
      if (type === "update") {
        addUpdate = func;
      } else if (events[type] === undefined) {
        throw "Sorry Unknown Event " + type + " cannot be bound.";
      } else {
        events[type] = func;
      }
    },

    calculateNodePositions = function (e) {
      var sp, tp, s, t;
      s = e.source;
      t = e.target;
      if (s._isCommunity) {
        communityNodes[s._id] = s;
        sp = s.getSourcePosition(e);
      } else {
        sp = s.position;
      }
      if (t._isCommunity) {
        communityNodes[t._id] = t;
        tp = t.getTargetPosition(e);
      } else {
        tp = t.position;
      }
      return {
        s: sp,
        t: tp
      };
    },

    addPosition = function (line, g) {
      communityNodes = {};
      g.attr("transform", function(d) {
        var p = calculateNodePositions(d);
        return "translate("
          + p.s.x + ", "
          + p.s.y + ")"
          + "rotate("
          + getCorner(p.s, p.t)
          + ")";
      });
      line.attr("x2", function(d) {
        var p = calculateNodePositions(d);
        return getDistance(p.s, p.t);
      });
    },

    addQue = function (line, g) {
      addShape(line, g);
      if (visibleLabels) {
        addLabel(line, g);
      }
      addColor(line, g);
      addEvents(line, g);
      addPosition(line, g);
    },

    shapeEdges = function (newEdges) {
      if (newEdges !== undefined) {
        edges = newEdges;
      }
      var line,
        g = self.parent
          .selectAll(".link")
          .data(edges, idFunction);
      // Append the group and class to all new
      g.enter()
        .append("g")
        .attr("class", "link") // link is CSS class that might be edited
        .attr("id", idFunction);
      // Remove all old
      g.exit().remove();
      // Remove all elements that are still included.
      g.selectAll("* > *").remove();
      line = g.append("line");
      addQue(line, g);
      _.each(communityNodes, function(c) {
        c.shapeInnerEdges(d3.select(this), addQue);
      });
      contextMenu.bindMenu($(".link"));
    },

    updateEdges = function () {
      var g = self.parent.selectAll(".link"),
        line = g.select("line");
      addPosition(line, g);
      addUpdate(g);
      _.each(communityNodes, function(c) {
        c.updateInnerEdges(d3.select(this), addPosition, addUpdate);
      });
    },

    parseShapeFlag = function (shape) {
      $("svg defs marker#arrow").remove();
      switch (shape.type) {
        case EdgeShaper.shapes.NONE:
          addShape = noop;
          break;
        case EdgeShaper.shapes.ARROW:
          addShape = function (line, g) {
            line.attr("marker-end", "url(#arrow)");
          };
          if (toplevelSVG.selectAll("defs")[0].length === 0) {
            toplevelSVG.append("defs");
          }
          toplevelSVG
            .select("defs")
            .append("marker")
            .attr("id", "arrow")
            .attr("refX", "10")
            .attr("refY", "5")
            .attr("markerUnits", "strokeWidth")
            .attr("markerHeight", "10")
            .attr("markerWidth", "10")
            .attr("orient", "auto")
            .append("path")
              .attr("d", "M 0 0 L 10 5 L 0 10 z");
          break;
        default:
          throw "Sorry given Shape not known!";
      }
    },

    parseLabelFlag = function (label) {
      if (_.isFunction(label)) {
        addLabel = function (line, g) {
          g.append("text") // Append a label for the edge
            .attr("text-anchor", "middle") // Define text-anchor
            .text(label);
        };
      } else {
        addLabel = function (line, g) {
          g.append("text") // Append a label for the edge
            .attr("text-anchor", "middle") // Define text-anchor
            .text(function(d) {
              // Which value should be used as label
              var chunks = splitLabel(findFirstValue(label, d._data));
              return chunks[0] || "";
            });
        };
      }
      addUpdate = function (edges) {
        edges.select("text")
          .attr("transform", function(d) {
            var p = calculateNodePositions(d);
            return "translate("
              + getDistance(p.s, p.t) / 2
              + ", -3)";
          });
      };
    },

    parseActionFlag = function (actions) {
      if (actions.reset !== undefined && actions.reset) {
        unbindEvents();
      }
      _.each(actions, function(func, type) {
        if (type !== "reset") {
          bindEvent(type, func);
        }
      });
    },

    parseColorFlag = function (color) {
      $("svg defs #gradientEdgeColor").remove();
      resetColourMap();
      switch (color.type) {
        case "single":
          addColor = function (line, g) {
            line.attr("stroke", color.stroke);
          };
          break;
        case "gradient":
          if (toplevelSVG.selectAll("defs")[0].length === 0) {
            toplevelSVG.append("defs");
          }
          var gradient = toplevelSVG
            .select("defs")
            .append("linearGradient")
            .attr("id", "gradientEdgeColor");
          gradient.append("stop")
            .attr("offset", "0")
            .attr("stop-color", color.source);
          gradient.append("stop")
            .attr("offset", "0.4")
            .attr("stop-color", color.source);
          gradient.append("stop")
            .attr("offset", "0.6")
            .attr("stop-color", color.target);
          gradient.append("stop")
            .attr("offset", "1")
            .attr("stop-color", color.target);
          addColor = function (line, g) {
            line.attr("stroke", "url(#gradientEdgeColor)");
            line.attr("y2", "0.0000000000000001");
          };
          break;
        case "attribute":
          addColor = function (line, g) {
             g.attr("stroke", function(e) {
               return colourMapper.getColour(e._data[color.key]);
             });
          };
          break;
        default:
          throw "Sorry given colour-scheme not known";
      }
    },

    parseConfig = function(config) {
      if (config.shape !== undefined) {
        parseShapeFlag(config.shape);
      }
      if (config.label !== undefined) {
        parseLabelFlag(config.label);
        self.label = config.label;
      }
      if (config.actions !== undefined) {
        parseActionFlag(config.actions);
      }
      if (config.color !== undefined) {
        parseColorFlag(config.color);
      }
    };

  self.parent = parent;

  unbindEvents();

  toplevelSVG = parent;
  while (toplevelSVG[0][0] && toplevelSVG[0][0].ownerSVGElement) {
    toplevelSVG = d3.select(toplevelSVG[0][0].ownerSVGElement);
  }

  if (config === undefined) {
    config = {
      color: {
        type: "single",
        stroke: "#686766"
      }
    };
  }

  if (config.color === undefined) {
    config.color = {
      type: "single",
      stroke: "#686766"
    };
  }

  parseConfig(config);

  if (_.isFunction(idfunc)) {
    idFunction = idfunc;
  }

  followEdgeG = toplevelSVG.append("g");


  /////////////////////////////////////////////////////////
  /// Public functions
  /////////////////////////////////////////////////////////

  self.changeTo = function(config) {
    parseConfig(config);
    shapeEdges();
    updateEdges();
  };

  self.drawEdges = function (edges) {
    shapeEdges(edges);
    updateEdges();
  };

  self.updateEdges = function () {
    updateEdges();
  };

  self.reshapeEdges = function() {
    shapeEdges();
  };

  self.activateLabel = function(toogle) {
    if (toogle) {
      visibleLabels = true;
    } else {
      visibleLabels = false;
    }
    shapeEdges();
  };

  self.addAnEdgeFollowingTheCursor = function(x, y) {
    followEdge = followEdgeG.append("line");
    followEdge.attr("stroke", "black")
      .attr("id", "connectionLine")
      .attr("x1", x)
      .attr("y1", y)
      .attr("x2", x)
      .attr("y2", y);
    return function(x, y) {
      followEdge.attr("x2", x).attr("y2", y);
    };
  };

  self.removeCursorFollowingEdge = function() {
    if (followEdge.remove) {
      followEdge.remove();
      followEdge = {};
    }
  };

  self.addMenuEntry = function(name, func) {
    contextMenu.addEntry(name, func);
  };

  self.getLabel = function() {
    return self.label || "";
  };

  self.resetColourMap = resetColourMap;
}

EdgeShaper.shapes = Object.freeze({
  "NONE": 0,
  "ARROW": 1
});

/*global _, $, window, d3*/
/*global EventLibrary*/
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
function EventDispatcher(nodeShaper, edgeShaper, config) {
  "use strict";

  var eventlib,
    svgBase,
    svgTemp,
    svgObj,
    self = this,
    parseNodeEditorConfig = function(config) {
      if (config.shaper === undefined) {
        config.shaper = nodeShaper;
      }
      if (eventlib.checkNodeEditorConfig(config)) {
        var insert = new eventlib.InsertNode(config),
          patch = new eventlib.PatchNode(config),
          del = new eventlib.DeleteNode(config);

        self.events.CREATENODE = function(getNewData, callback, x, y) {
          var data;
          if (!_.isFunction(getNewData)) {
            data = getNewData;
          } else {
            data = getNewData();
          }
          return function() {
            insert(data, callback, x, y);
          };
        };
        self.events.PATCHNODE = function(node, getNewData, callback) {
          if (!_.isFunction(getNewData)) {
            throw "Please give a function to extract the new node data";
          }
          return function() {
            patch(node, getNewData(), callback);
          };
        };

        self.events.DELETENODE = function(callback) {
          return function(node) {
            del(node, callback);
          };
        };
      }
    },

    parseEdgeEditorConfig = function(config) {
      if (config.shaper === undefined) {
        config.shaper = edgeShaper;
      }
      if (eventlib.checkEdgeEditorConfig(config)) {
        var insert = new eventlib.InsertEdge(config),
          patch = new eventlib.PatchEdge(config),
          del = new eventlib.DeleteEdge(config),
          edgeStart = null,
          didInsert = false;

        self.events.STARTCREATEEDGE = function(callback) {
          return function(node) {
            var e = d3.event || window.event;
            edgeStart = node;
            didInsert = false;
            if (callback !== undefined) {
              callback(node, e);
            }
            // Necessary to omit dragging of the graph
            e.stopPropagation();
          };
        };

        self.events.CANCELCREATEEDGE = function(callback) {
          return function() {
            edgeStart = null;
            if (callback !== undefined && !didInsert) {
              callback();
            }
          };
        };

        self.events.FINISHCREATEEDGE = function(callback) {
          return function(node) {
            if (edgeStart !== null && node !== edgeStart) {
              insert(edgeStart, node, callback);
              didInsert = true;
            }
          };
        };

        self.events.PATCHEDGE = function(edge, getNewData, callback) {
          if (!_.isFunction(getNewData)) {
            throw "Please give a function to extract the new node data";
          }
          return function() {
            patch(edge, getNewData(), callback);
          };
        };

        self.events.DELETEEDGE = function(callback) {
          return function(edge) {
            del(edge, callback);
          };
        };
      }
    },

    bindSVGEvents = function() {
      svgObj = svgObj || $("svg");
      svgObj.unbind();
      _.each(svgBase, function(fs, ev) {
        svgObj.bind(ev, function(trigger) {
          _.each(fs, function(f) {
            f(trigger);
          });
          if (!! svgTemp[ev]) {
            svgTemp[ev](trigger);
          }
        });
      });
    };

  if (nodeShaper === undefined) {
    throw "NodeShaper has to be given.";
  }

  if (edgeShaper === undefined) {
    throw "EdgeShaper has to be given.";
  }

  eventlib = new EventLibrary();

  svgBase = {
    click: [],
    dblclick: [],
    mousedown: [],
    mouseup: [],
    mousemove: [],
    mouseout: [],
    mouseover: []
  };
  svgTemp = {};

  self.events = {};

  if (config !== undefined) {
    if (config.expand !== undefined) {
      if (eventlib.checkExpandConfig(config.expand)) {
        self.events.EXPAND = new eventlib.Expand(config.expand);
        nodeShaper.setGVStartFunction(function() {
         config.expand.reshapeNodes();
         config.expand.startCallback();
        });
      }
    }
    if (config.drag !== undefined) {
      if (eventlib.checkDragConfig(config.drag)) {
        self.events.DRAG = eventlib.Drag(config.drag);
      }
    }
    if (config.nodeEditor !== undefined) {
      parseNodeEditorConfig(config.nodeEditor);
    }
    if (config.edgeEditor !== undefined) {
      parseEdgeEditorConfig(config.edgeEditor);
    }
  }
  Object.freeze(self.events);

  //Check for expand config
  self.bind = function (object, event, func) {
    if (func === undefined || !_.isFunction(func)) {
      throw "You have to give a function that should be bound as a third argument";
    }
    var actions = {};
    switch (object) {
      case "nodes":
        actions[event] = func;
        nodeShaper.changeTo({
          actions: actions
        });
        break;
      case "edges":
        actions[event] = func;
        edgeShaper.changeTo({
          actions: actions
        });
        break;
      case "svg":
        svgTemp[event] = func;
        bindSVGEvents();
        break;
      default:
        if (object.bind !== undefined) {
          object.unbind(event);
          object.bind(event, func);
        } else {
          throw "Sorry cannot bind to object. Please give either "
          + "\"nodes\", \"edges\" or a jQuery-selected DOM-Element";
        }
    }
  };

  self.rebind = function (object, actions) {
    actions = actions || {};
    actions.reset = true;
    switch (object) {
      case "nodes":
        nodeShaper.changeTo({
          actions: actions
        });
        break;
      case "edges":
        edgeShaper.changeTo({
          actions: actions
        });
        break;
      case "svg":
        svgTemp = {};
        _.each(actions, function(fs, ev) {
          if (ev !== "reset") {
            svgTemp[ev] = fs;
          }
        });
        bindSVGEvents();
        break;
      default:
          throw "Sorry cannot rebind to object. Please give either "
          + "\"nodes\", \"edges\" or \"svg\"";
    }
  };

  self.fixSVG = function(event, action) {
    if (svgBase[event] === undefined) {
      throw "Sorry unkown event";
    }
    svgBase[event].push(action);
    bindSVGEvents();
  };
  /*
  self.unbind = function () {
    throw "Not implemented";
  };
  */
  Object.freeze(self.events);
}

/*global _*/

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

// configs:
//  expand: {
//    startCallback,
//    loadNode,
//    reshapeNodes
//  }
//
//  nodeEditor: {
//    nodes,
//    adapter,
//    shaper
//  }
//
//  edgeEditor: {
//    edges,
//    adapter,
//    shaper
//  }
//
//
//
function EventLibrary() {
  "use strict";

  var self = this;

  this.checkExpandConfig = function(config) {
    if (config.startCallback === undefined) {
      throw "A callback to the Start-method has to be defined";
    }
    if (config.adapter === undefined || config.adapter.explore === undefined) {
      throw "An adapter to load data has to be defined";
    }
    if (config.reshapeNodes === undefined) {
      throw "A callback to reshape nodes has to be defined";
    }
    return true;
  };

  this.Expand = function (config) {
    self.checkExpandConfig(config);
    var
      startCallback = config.startCallback,
      explore = config.adapter.explore,
      reshapeNodes = config.reshapeNodes;
    return function(n) {
      explore(n, startCallback);
      reshapeNodes();
      startCallback();
    };
  };

  this.checkDragConfig = function (config) {
    if (config.layouter === undefined) {
      throw "A layouter has to be defined";
    }
    if (config.layouter.drag === undefined || !_.isFunction(config.layouter.drag)) {
      throw "The layouter has to offer a drag function";
    }
    return true;
  };

  this.Drag = function (config) {
    self.checkDragConfig(config);
    return config.layouter.drag;
  };

  this.checkNodeEditorConfig = function (config) {
    if (config.adapter === undefined) {
      throw "An adapter has to be defined";
    }
    if (config.shaper === undefined) {
      throw "A node shaper has to be defined";
    }
    return true;
  };

  this.checkEdgeEditorConfig = function (config) {
    if (config.adapter === undefined) {
      throw "An adapter has to be defined";
    }
    if (config.shaper === undefined) {
      throw "An edge Shaper has to be defined";
    }
    return true;
  };

  this.InsertNode = function (config) {
    self.checkNodeEditorConfig(config);
    var adapter = config.adapter,
      nodeShaper = config.shaper;

    return function(data, callback, x, y) {
      var cb, d;
      if (_.isFunction(data) && !callback) {
        cb = data;
        d = {};
      } else {
        cb = callback;
        d = data;
      }
      adapter.createNode(d, function(newNode) {
        nodeShaper.reshapeNodes();
        cb(newNode);
      }, x, y);
    };
  };

  this.PatchNode = function (config) {
    self.checkNodeEditorConfig(config);
    var adapter = config.adapter,
    nodeShaper = config.shaper;

    return function(nodeToPatch, patchData, callback) {
      adapter.patchNode(nodeToPatch, patchData, function(patchedNode) {
        nodeShaper.reshapeNodes();
        callback(patchedNode);
      });
    };
  };

  this.DeleteNode = function (config) {
    self.checkNodeEditorConfig(config);
    var adapter = config.adapter,
    nodeShaper = config.shaper;

    return function(nodeToDelete, callback) {
      adapter.deleteNode(nodeToDelete, function() {
        nodeShaper.reshapeNodes();
        callback();
      });
    };
  };

  this.SelectNodeCollection = function(config) {
    self.checkNodeEditorConfig(config);
    var adapter = config.adapter;
    if (!_.isFunction(adapter.useNodeCollection)) {
      throw "The adapter has to support collection changes";
    }
    return function(name, callback) {
      adapter.useNodeCollection(name);
      callback();
    };
  };

  this.InsertEdge = function (config) {
    self.checkEdgeEditorConfig(config);
    var adapter = config.adapter,
    edgeShaper = config.shaper;
    return function(source, target, callback) {
      adapter.createEdge({source: source, target: target}, function(newEdge) {
        edgeShaper.reshapeEdges();
        callback(newEdge);
      });
    };
  };

  this.PatchEdge = function (config) {
    self.checkEdgeEditorConfig(config);
    var adapter = config.adapter,
    edgeShaper = config.shaper;
    return function(edgeToPatch, patchData, callback) {
      adapter.patchEdge(edgeToPatch, patchData, function(patchedEdge) {
        edgeShaper.reshapeEdges();
        callback(patchedEdge);
      });
    };
  };

  this.DeleteEdge = function (config) {
    self.checkEdgeEditorConfig(config);
    var adapter = config.adapter,
    edgeShaper = config.shaper;
    return function(edgeToDelete, callback) {
      adapter.deleteEdge(edgeToDelete, function() {
        edgeShaper.reshapeEdges();
        callback();
      });
    };
  };

}

/*global _, d3*/
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


/*
* example config:
* {
*   nodes: nodes,
*   links: links,
*
*   (optional)
*   width: width,
*   height: height,
*   distance: distance,
*   gravity: gravity,
*   onUpdate: callback
* }
*/

function ForceLayouter(config) {
  "use strict";
  var self = this,
    force = d3.layout.force(),
    /*
    distance = config.distance || 240, // 80
    gravity = config.gravity || 0.01, // 0.08
    charge = config.charge || -1000, // -240
    */
    defaultCharge = config.charge || -600,
    defaultDistance = config.distance || 80,
    gravity = config.gravity || 0.01,
    distance = function(d) {
      var res = 0;
      if (d.source._isCommunity) {
        res += d.source.getDistance(defaultDistance);
      } else {
        res += defaultDistance;
      }
      if (d.target._isCommunity) {
        res += d.target.getDistance(defaultDistance);
      } else {
        res += defaultDistance;
      }
      return res;
    },
    charge = function(d) {
      if (d._isCommunity) {
        return d.getCharge(defaultCharge);
      }
      return defaultCharge;
    },

    onUpdate = config.onUpdate || function () {},
    width = config.width || 880,
    height = config.height || 680,
    parseConfig = function(config) {
      if (config.distance) {
        defaultDistance = config.distance;
      }
      if (config.gravity) {
        force.gravity(config.gravity);
      }
      if (config.charge) {
        defaultCharge = config.charge;
      }
    };

  if (config.nodes === undefined) {
    throw "No nodes defined";
  }
  if (config.links === undefined) {
    throw "No links defined";
  }
  // Set up the force
  force.nodes(config.nodes); // Set nodes
  force.links(config.links); // Set edges
  force.size([width, height]); // Set width and height
  force.linkDistance(distance); // Set distance between nodes
  force.gravity(gravity); // Set gravity
  force.charge(charge); // Set charge
  force.on("tick", function(){}); // Bind tick function

  self.start = function() {
    force.start(); // Start Force computation
  };

  self.stop = function() {
    force.stop(); // Stop Force computation
  };

  self.drag = force.drag;

  self.setCombinedUpdateFunction = function(nodeShaper, edgeShaper, additional) {
    if (additional !== undefined) {
      onUpdate = function() {
        if (force.alpha() < 0.1) {
          nodeShaper.updateNodes();
          edgeShaper.updateEdges();
          additional();
          if (force.alpha() < 0.05) {
            self.stop();
          }
        }
      };
      force.on("tick", onUpdate);
    } else {
      onUpdate = function() {
        if (force.alpha() < 0.1) {
          nodeShaper.updateNodes();
          edgeShaper.updateEdges();
          if (force.alpha() < 0.05) {
            self.stop();
          }
        }
      };
      force.on("tick", onUpdate);
    }
  };

  self.changeTo = function(config) {
    parseConfig(config);
  };

  self.changeWidth = function(w) {
    width = w;
    force.size([width, height]); // Set width and height
  };
}

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

function FoxxAdapter(nodes, edges, route, viewer, config) {
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
  if (viewer === undefined) {
    throw "A reference to the graph viewer has to be given.";
  }

  config = config || {};

  var self = this,
    absConfig = {},
    absAdapter,
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

  if (config.prioList) {
    absConfig.prioList = config.prioList;
  }
  absAdapter = new AbstractAdapter(nodes, edges, this, viewer, absConfig);

  parseConfig(config);
  fillRoutes();

  self.explore = absAdapter.explore;

  self.loadNode = function(nodeId, callback) {
    sendGet("query", nodeId, function(result) {
      parseResult(result, callback);
    });
  };

  self.loadInitialNode = function(nodeId, callback) {
    absAdapter.cleanUp();
    var cb = function(n) {
      callback(absAdapter.insertInitialNode(n));
    };
    self.loadNode(nodeId, cb);
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

  self.setWidth = absAdapter.setWidth;
  self.changeTo = absAdapter.changeTo;
  self.getPrioList = absAdapter.getPrioList;
}

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
          && query !== queries.childrenCentrality
          && query !== queries.randomVertices) {
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
      if (result.length === 0
        || result[0].length === 0
        || result[0][0].length === 0) {
        if (callback) {
          callback({
            errorCode: 404
          });
        }
        return;
      }
      result = result[0][0];
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
  queries.randomVertices = "FOR x IN @@collection SORT RAND() LIMIT @limit RETURN x";

  self.explore = absAdapter.explore;

  self.loadNode = function(nodeId, callback) {
    self.loadNodeFromTreeById(nodeId, callback);
  };

  self.loadRandomNode = function(callback) {
    var collections = _.shuffle(self.getNodeCollections()), i;
    for (i = 0; i < collections.length; ++i) {
      var list = getNRandom(1, collections[i]);
      
      if (list.length > 0) {
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
        if (data.error === false) {
          var toInsert = data.edge, edge;
          toInsert._from = edgeToAdd._from;
          toInsert._to = edgeToAdd._to;
          edge = absAdapter.insertEdge(toInsert);
          callback(edge);
        }
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
          nodeToAdd._key = data.vertex._key;
          nodeToAdd._id = data.vertex._id;
          nodeToAdd._rev = data.vertex._rev;
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
      var ret = [ ];
      var collections = _.shuffle(self.getNodeCollections()), i;
      for (i = 0; i < collections.length; ++i) {
        var l = getNRandom(10, collections[i]);
      
        if (l.length > 0) {
          ret = ret.concat(_.flatten(
           _.map(l, function(o) {
             return _.keys(o);
           })
          ));
        }
      }
          
      var ret = _.sortBy(
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

/*global _*/
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

function ModularityJoiner() {
  "use strict";

  var
  // Copy of underscore.js. importScripts doesn't work
    breaker = {},
    nativeForEach = Array.prototype.forEach,
    nativeKeys = Object.keys,
    nativeIsArray = Array.isArray,
    toString = Object.prototype.toString,
    nativeIndexOf = Array.prototype.indexOf,
    nativeMap = Array.prototype.map,
    nativeSome = Array.prototype.some,
    _ = {
      isArray: nativeIsArray || function(obj) {
        return toString.call(obj) === '[object Array]';
      },
      isFunction: function(obj) {
        return typeof obj === 'function';
      },
      isString: function(obj) {
        return toString.call(obj) === '[object String]';
      },
      each: function(obj, iterator, context) {
        if (obj === null || obj === undefined) {
          return;
        }
        var i, l, key;
        if (nativeForEach && obj.forEach === nativeForEach) {
          obj.forEach(iterator, context);
        } else if (obj.length === +obj.length) {
          for (i = 0, l = obj.length; i < l; i++) {
            if (iterator.call(context, obj[i], i, obj) === breaker) {
              return;
            }
          }
        } else {
          for (key in obj) {
            if (obj.hasOwnProperty(key)) {
              if (iterator.call(context, obj[key], key, obj) === breaker) {
                return;
              }
            }
          }
        }
      },
      keys: nativeKeys || function(obj) {
        if (typeof obj !== "object" || Array.isArray(obj)) {
          throw new TypeError('Invalid object');
        }
        var keys = [], key;
        for (key in obj) {
          if (obj.hasOwnProperty(key)) {
            keys[keys.length] = key;
          }
        }
        return keys;
      },
      min: function(obj, iterator, context) {
        if (!iterator && _.isArray(obj) && obj[0] === +obj[0] && obj.length < 65535) {
          return Math.min.apply(Math, obj);
        }
        if (!iterator && _.isEmpty(obj)) {
          return Infinity;
        }
        var result = {computed : Infinity, value: Infinity};
        _.each(obj, function(value, index, list) {
          var computed = iterator ? iterator.call(context, value, index, list) : value;
          if (computed < result.computed) {
            result = {value : value, computed : computed};
          }
        });
        return result.value;
      },
      map: function(obj, iterator, context) {
        var results = [];
        if (obj === null) {
          return results;
        }
        if (nativeMap && obj.map === nativeMap) {
          return obj.map(iterator, context);
        }
        _.each(obj, function(value, index, list) {
          results[results.length] = iterator.call(context, value, index, list);
        });
        return results;
      },
      pluck: function(obj, key) {
        return _.map(obj, function(value){ return value[key]; });
      },
      uniq: function(array, isSorted, iterator, context) {
        if (_.isFunction(isSorted)) {
          context = iterator;
          iterator = isSorted;
          isSorted = false;
        }
        var initial = iterator ? _.map(array, iterator, context) : array,
          results = [],
          seen = [];
        _.each(initial, function(value, index) {
          if (isSorted) {
            if (!index || seen[seen.length - 1] !== value) {
              seen.push(value);
              results.push(array[index]);
            }
          } else if (!_.contains(seen, value)) {
            seen.push(value);
            results.push(array[index]);
          }
        });
        return results;
      },
      union: function() {
        return _.uniq(Array.prototype.concat.apply(Array.prototype, arguments));
      },
      isEmpty: function(obj) {
        var key;
        if (obj === null) {
          return true;
        }
        if (_.isArray(obj) || _.isString(obj)) {
          return obj.length === 0;
        }
        for (key in obj) {
          if (obj.hasOwnProperty(key)) {
            return false;
          }
        }
        return true;
      },
      any: function(obj, iterator, context) {
        iterator =  iterator || _.identity;
        var result = false;
        if (obj === null) {
          return result;
        }
        if (nativeSome && obj.some === nativeSome) {
          return obj.some(iterator, context);
        }
        _.each(obj, function(value, index, list) {
          if (result) {
            return breaker;
          }
          result = iterator.call(context, value, index, list);
          return breaker;
        });
        return !!result;
      },
      contains: function(obj, target) {
        if (obj === null) {
          return false;
        }
        if (nativeIndexOf && obj.indexOf === nativeIndexOf) {
          return obj.indexOf(target) !== -1;
        }
        return _.any(obj, function(value) {
          return value === target;
        });
      },
      values: function(obj) {
        var values = [], key;
        for (key in obj) {
          if (obj.hasOwnProperty(key)) {
            values.push(obj[key]);
          }
        }
        return values;
      }
    },
    matrix = {},
    backwardMatrix = {},
    degrees = {},
    m = 0,
    revM = 0,
    a = null,
    dQ = null,
    heap = null,
    isRunning = false,
    comms = {},

    ////////////////////////////////////
    // Private functions              //
    ////////////////////////////////////

    setHeapToMax = function(id) {
      var maxT,
        maxV = Number.NEGATIVE_INFINITY;
      _.each(dQ[id], function(v, t) {
        if (maxV < v) {
          maxV = v;
          maxT = t;
        }
      });
      if (maxV < 0) {
        delete heap[id];
        return;
      }
      heap[id] = maxT;
    },

    setHeapToMaxInList = function(l, id) {
      setHeapToMax(id);
    },

    isSetDQVal = function(i, j) {
      if (i < j) {
        return dQ[i] && dQ[i][j];
      }
      return dQ[j] && dQ[j][i];
    },

    // This does not check if everything exists,
    // do it before!
    getDQVal = function(i, j) {
      if (i < j) {
        return dQ[i][j];
      }
      return dQ[j][i];
    },

    setDQVal = function(i, j, v) {
      if (i < j) {
        dQ[i] = dQ[i] || {};
        dQ[i][j] = v;
        return;
      }
      dQ[j] = dQ[j] || {};
      dQ[j][i] = v;
    },

    delDQVal = function(i, j) {
      if (i < j) {
        if (!dQ[i]) {
          return;
        }
        delete dQ[i][j];
        if (_.isEmpty(dQ[i])) {
          delete dQ[i];
        }
        return;
      }
      if (i === j) {
        return;
      }
      delDQVal(j, i);
    },

    updateHeap = function(i, j) {
      var hv, val;
      if (i < j) {
        if (!isSetDQVal(i, j)) {
          setHeapToMax(i);
          return;
        }
        val = getDQVal(i, j);
        if (heap[i] === j) {
          setHeapToMax(i);
          return;
        }
        if (!isSetDQVal(i, heap[i])) {
          setHeapToMax(i);
          return;
        }
        hv = getDQVal(i, heap[i]);
        if (hv < val) {
          heap[i] = j;
        }
        return;
      }
      if (i === j) {
        return;
      }
      updateHeap(j, i);
    },

    updateDegrees = function(low, high) {
      a[low]._in += a[high]._in;
      a[low]._out += a[high]._out;
      delete a[high];
    },

    insertEdge = function(s, t) {
      matrix[s] = matrix[s] || {};
      matrix[s][t] = (matrix[s][t] || 0) + 1;
      backwardMatrix[t] = backwardMatrix[t] || {};
      backwardMatrix[t][s] = (backwardMatrix[t][s] || 0) + 1;
      degrees[s] = degrees[s] || {_in: 0, _out:0};
      degrees[t] = degrees[t] || {_in: 0, _out:0};
      degrees[s]._out++;
      degrees[t]._in++;
      m++;
      revM = Math.pow(m, -1);
    },

    deleteEdge = function(s, t) {
      if (matrix[s]) {
        matrix[s][t]--;
        if (matrix[s][t] === 0) {
          delete matrix[s][t];
        }
        backwardMatrix[t][s]--;
        if (backwardMatrix[t][s] === 0) {
          delete backwardMatrix[t][s];
        }
        degrees[s]._out--;
        degrees[t]._in--;
        m--;
        if (m > 0) {
          revM = Math.pow(m, -1);
        } else {
          revM = 0;
        }
        if (_.isEmpty(matrix[s])) {
          delete matrix[s];
        }
        if (_.isEmpty(backwardMatrix[t])) {
          delete backwardMatrix[t];
        }
        if (degrees[s]._in === 0 && degrees[s]._out === 0) {
          delete degrees[s];
        }
        if (degrees[t]._in === 0 && degrees[t]._out === 0) {
          delete degrees[t];
        }
      }
    },

    makeInitialDegrees = function() {
      a = {};
      _.each(degrees, function (n, id) {
        a[id] = {
          _in: n._in / m,
          _out: n._out / m
        };
      });
      return a;
    },

    notConnectedPenalty = function(s, t) {
      return a[s]._out * a[t]._in + a[s]._in * a[t]._out;
    },

    neighbors = function(sID) {
      var outbound = _.keys(matrix[sID] || {}),
        inbound = _.keys(backwardMatrix[sID] || {});
      return _.union(outbound, inbound);
    },

    makeInitialDQ = function() {
      dQ = {};
      _.each(matrix, function(tars, s) {
        var bw = backwardMatrix[s] || {},
          keys = neighbors(s);
        _.each(keys, function(t) {
          var ast = (tars[t] || 0),
            value;
          ast += (bw[t] || 0);
          value = ast * revM - notConnectedPenalty(s, t);
          if (value > 0) {
            setDQVal(s, t, value);
          }
          return;
        });
      });

    },

    makeInitialHeap = function() {
      heap = {};
      _.each(dQ, setHeapToMaxInList);
      return heap;
    },

    // i < j && i != j != k
    updateDQAndHeapValue = function (i, j, k) {
      var val;
      if (isSetDQVal(k, i)) {
        val = getDQVal(k, i);
        if (isSetDQVal(k, j)) {
          val += getDQVal(k, j);
          setDQVal(k, i, val);
          delDQVal(k, j);
          updateHeap(k, i);
          updateHeap(k, j);
          return;
        }
        val -= notConnectedPenalty(k, j);
        if (val < 0) {
          delDQVal(k, i);
        }
        updateHeap(k, i);
        return;
      }
      if (isSetDQVal(k, j)) {
        val = getDQVal(k, j);
        val -= notConnectedPenalty(k, i);
        if (val > 0) {
          setDQVal(k, i, val);
        }
        updateHeap(k, i);
        delDQVal(k, j);
        updateHeap(k, j);
      }
    },

    updateDQAndHeap = function (low, high) {
      _.each(dQ, function (list, s) {
        if (s === low || s === high) {
          _.each(list, function(v, t) {
            if (t === high) {
              delDQVal(low, high);
              updateHeap(low, high);
              return;
            }
            updateDQAndHeapValue(low, high, t);
          });
          return;
        }
        updateDQAndHeapValue(low, high, s);
      });
    },

  ////////////////////////////////////
  // getters                        //
  ////////////////////////////////////

  getAdjacencyMatrix = function() {
    return matrix;
  },

  getHeap = function() {
    return heap;
  },

  getDQ = function() {
    return dQ;
  },

  getDegrees = function() {
    return a;
  },

  getCommunities = function() {
    return comms;
  },

  getBest = function() {
    var bestL, bestS, bestV = Number.NEGATIVE_INFINITY;
    _.each(heap, function(lID, sID) {
      if (bestV < dQ[sID][lID]) {
        bestL = lID;
        bestS = sID;
        bestV = dQ[sID][lID];
      }
    });
    if (bestV <= 0) {
      return null;
    }
    return {
      sID: bestS,
      lID: bestL,
      val: bestV
    };
  },

  getBestCommunity = function (communities) {
    var bestQ = Number.NEGATIVE_INFINITY,
      bestC;
    _.each(communities, function (obj) {
      if (obj.q > bestQ) {
        bestQ = obj.q;
        bestC = obj.nodes;
      }
    });
    return bestC;
  },

  ////////////////////////////////////
  // setup                          //
  ////////////////////////////////////

  setup = function() {
    makeInitialDegrees();
    makeInitialDQ();
    makeInitialHeap();
    comms = {};
  },


  ////////////////////////////////////
  // computation                    //
  ////////////////////////////////////

  joinCommunity = function(comm) {
    var s = comm.sID,
      l = comm.lID,
      q = comm.val;

    comms[s] = comms[s] || {nodes: [s], q: 0};
    if (comms[l]) {
      comms[s].nodes = comms[s].nodes.concat(comms[l].nodes);
      comms[s].q += comms[l].q;
      delete comms[l];
    } else {
      comms[s].nodes.push(l);
    }
    comms[s].q += q;
    updateDQAndHeap(s, l);
    updateDegrees(s, l);
  },

  //////////////////////////////////////////////
  // Evaluate value of community by distance  //
  //////////////////////////////////////////////

  floatDistStep = function(dist, depth, todo) {
    if (todo.length === 0) {
      return true;
    }
    var nextTodo = [];
    _.each(todo, function(t) {
      if (dist[t] !== Number.POSITIVE_INFINITY) {
        return;
      }
      dist[t] = depth;
      nextTodo = nextTodo.concat(neighbors(t));
    });
    return floatDistStep(dist, depth+1, nextTodo);
  },

  floatDist = function(sID) {
    var dist = {};
    _.each(matrix, function(u, n) {
      dist[n] = Number.POSITIVE_INFINITY;
    });
    dist[sID] = 0;
    if (floatDistStep(dist, 1, neighbors(sID))) {
      return dist;
    }
    throw "FAIL!";
  },

  minDist = function(dist) {
    return function(a) {
      return dist[a];
    };
  },

  ////////////////////////////////////
  // Get only the Best Community    //
  ////////////////////////////////////

  getCommunity = function(limit, focus) {
    var coms = {},
      res = [],
      dist = {},
      best,
      sortByDistance = function (a, b) {
        var d1 = dist[_.min(a,minDist(dist))],
          d2 = dist[_.min(b,minDist(dist))],
          val = d2 - d1;
        if (val === 0) {
          val = coms[b[b.length-1]].q - coms[a[a.length-1]].q;
        }
        return val;
      };
    setup();
    best = getBest();
    while (best !== null) {
      joinCommunity(best);
      best = getBest();
    }
    coms = getCommunities();
    if (focus !== undefined) {
      _.each(coms, function(obj, key) {
        if (_.contains(obj.nodes, focus)) {
          delete coms[key];
        }
      });

      res = _.pluck(_.values(coms), "nodes");
      dist = floatDist(focus);
      res.sort(sortByDistance);
      return res[0];
    }
    return getBestCommunity(coms);
  };

  ////////////////////////////////////
  // Public functions               //
  ////////////////////////////////////

  this.insertEdge = insertEdge;

  this.deleteEdge = deleteEdge;

  this.getAdjacencyMatrix = getAdjacencyMatrix;

  this.getHeap = getHeap;

  this.getDQ = getDQ;

  this.getDegrees = getDegrees;

  this.getCommunities = getCommunities;

  this.getBest = getBest;

  this.setup = setup;

  this.joinCommunity = joinCommunity;

  this.getCommunity = getCommunity;
}

/*global _*/
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

function NodeReducer(prioList) {
  "use strict";

  prioList = prioList || [];

  var

    ////////////////////////////////////
    // Private functions              //
    ////////////////////////////////////

    /////////////////////////////
    // Functions for Buckets   //
    /////////////////////////////

   addNode = function(bucket, node) {
     bucket.push(node);
   },

   getSimilarityValue = function(bucketContainer, node) {
     if (!bucketContainer.reason.example) {
       bucketContainer.reason.example = node;
       return 1;
     }
     var data = node._data || {},
       comp = bucketContainer.reason.example._data || {},
       props = _.union(_.keys(comp), _.keys(data)),
       countMatch = 0,
       propCount = 0;
     _.each(props, function(key) {
       if (comp[key] !== undefined && data[key] !== undefined) {
         countMatch++;
         if (comp[key] === data[key]) {
           countMatch += 4;
         }
       }
     });
     propCount = props.length * 5;
     propCount++;
     countMatch++;
     return countMatch / propCount;
   },

   getPrioList = function() {
     return prioList;
   },

   changePrioList = function (list) {
     prioList = list;
   },

   bucketByPrioList = function (toSort, numBuckets) {
     var res = {},
       resArray = [];
     _.each(toSort, function(n) {
       var d = n._data,
         sortTo = {},
         key,
         resKey,
         i = 0;
       for (i = 0; i < prioList.length; i++) {
         key = prioList[i];
         if (d[key] !== undefined) {
           resKey = d[key];
           res[key] = res[key] || {};
           res[key][resKey] = res[key][resKey] || [];
           res[key][resKey].push(n);
           return;
         }
       }
       resKey = "default";
       res[resKey] = res[resKey] || [];
       res[resKey].push(n);
     });
     _.each(res, function(list, key) {
       _.each(list, function(list, value) {
         var reason = {
           key: key,
           value: value,
           text: key + ": " + value
         };
         resArray.push({
           reason: reason,
           nodes: list
         });
       });
     });
     return resArray;
   },

  bucketNodes = function(toSort, numBuckets) {

    var res = [],
    threshold = 0.5;
    if (toSort.length <= numBuckets) {
      res = _.map(toSort, function(n) {
        return {
          reason: {
            type: "single",
            text: "One Node"
          },
          nodes: [n]
        };
      });
      return res;
    }
    if (!_.isEmpty(prioList)) {
      return bucketByPrioList(toSort, numBuckets);
    }
    _.each(toSort, function(n) {
      var i, shortest, sLength;
      shortest = 0;
      sLength = Number.POSITIVE_INFINITY;
      for (i = 0; i < numBuckets; i++) {
        res[i] = res[i] || {
          reason: {
            type: "similar",
            text: "Similar Nodes"
          },
          nodes: []
        };
        if (getSimilarityValue(res[i], n) > threshold) {
          addNode(res[i].nodes, n);
          return;
        }
        if (sLength > res[i].nodes.length) {
          shortest = i;
          sLength = res[i].nodes.length;
        }
      }
      addNode(res[shortest].nodes, n);
    });
    return res;
  };

  ////////////////////////////////////
  // Public functions               //
  ////////////////////////////////////

  this.bucketNodes = bucketNodes;

  this.changePrioList = changePrioList;

  this.getPrioList = getPrioList;

}

/*global $, _, d3*/
/*global ColourMapper, ContextMenu*/
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




/*
* flags example format:
* {
*   shape: {
*     type: NodeShaper.shapes.CIRCLE,
*     radius: value || function(node)
*   },
*   label: "key" || function(node),
*   actions: {
*     "click": function(node),
*     "drag": function(node)
*   },
*   update: function(node)
* }
*
* {
*   shape: {
*     type: NodeShaper.shapes.RECT,
*     width: value || function(node),
*     height: value || function(node),
*   },
*   label: "key" || function(node),
*   actions: {
*     "click": function(node),
*     "drag": function(node)
*   },
*   update: function(node)
* }
*
* <image x="0" y="0" height="1140" width="1040" xlink:href="like_a_sir_original.svg"/>
*
* {
*   shape: {
*     type: NodeShaper.shapes.IMAGE,
*     width: value || function(node),
*     height: value || function(node),
*   },
*   actions: {
*     "click": function(node),
*     "drag": function(node)
*   },
*   update: function(node)
* }
*
*/
function NodeShaper(parent, flags, idfunc) {
  "use strict";

  var self = this,
    nodes = [],
    visibleLabels = true,
    contextMenu = new ContextMenu("gv_node_cm"),
    findFirstValue = function(list, data) {
      if (_.isArray(list)) {
        return data[_.find(list, function(val) {
          return data[val];
        })];
      }
      return data[list];
    },
    splitLabel = function(label) {
      if (label === undefined) {
        return [""];
      }
      if (typeof label !== "string") {
        label = String(label);
      }
      var chunks = label.match(/[\w\W]{1,10}(\s|$)|\S+?(\s|$)/g);
      chunks[0] = $.trim(chunks[0]);
      chunks[1] = $.trim(chunks[1]);
      if (chunks[0].length > 12) {
        chunks[0] = $.trim(label.substring(0,10)) + "-";
        chunks[1] = $.trim(label.substring(10));
        if (chunks[1].length > 12) {
          chunks[1] = chunks[1].split(/\W/)[0];
          if (chunks[1].length > 12) {
            chunks[1] = chunks[1].substring(0,10) + "...";
          }
        }
        chunks.length = 2;
      }
      if (chunks.length > 2) {
        chunks.length = 2;
        chunks[1] += "...";
      }
      return chunks;
    },
    noop = function (node) {

    },
    start = noop,
    defaultDistortion = function(n) {
      return {
        x: n.x,
        y: n.y,
        z: 1
      };
    },
    distortion = defaultDistortion,
    addDistortion = function() {
      _.each(nodes, function(n) {
        n.position = distortion(n);
        if (n._isCommunity) {
          n.addDistortion(distortion);
        }
      });
    },
    colourMapper = new ColourMapper(),
    resetColourMap = function() {
      colourMapper.reset();
    },
    events,
    addUpdate,
    idFunction = function(d) {
      return d._id;
    },
    addColor = noop,
    addShape = noop,
    addLabel = noop,
    addLabelColor = function() {return "black";},

    unbindEvents = function() {
      // Hard unbind the dragging
      self.parent
        .selectAll(".node")
        .on("mousedown.drag", null);
      events = {
        click: noop,
        dblclick: noop,
        drag: noop,
        mousedown: noop,
        mouseup: noop,
        mousemove: noop,
        mouseout: noop,
        mouseover: noop
      };
      addUpdate = noop;
    },

    addEvents = function (nodes) {
      _.each(events, function (func, type) {
        if (type === "drag") {
          nodes.call(func);
        } else {
          nodes.on(type, func);
        }

      });
    },

    addQue = function (g) {
      var community = g.filter(function(n) {
          return n._isCommunity;
        }),
        normal = g.filter(function(n) {
          return !n._isCommunity;
        });
      addShape(normal);
      community.each(function(c) {
        c.shapeNodes(d3.select(this), addShape, addQue, start, colourMapper);
      });
      if (visibleLabels) {
        addLabel(normal);
      }
      addColor(normal);
      addEvents(normal);
      addDistortion();
    },

    bindEvent = function (type, func) {
      if (type === "update") {
        addUpdate = func;
      } else if (events[type] === undefined) {
        throw "Sorry Unknown Event " + type + " cannot be bound.";
      } else {
        events[type] = func;
      }
    },

    updateNodes = function () {
      var nodes = self.parent.selectAll(".node");
      addDistortion();
      nodes.attr("transform", function(d) {
        return "translate(" + d.position.x + "," + d.position.y + ")scale(" + d.position.z + ")";
      });
      addUpdate(nodes);
    },

    shapeNodes = function (newNodes) {
      if (newNodes !== undefined) {
        nodes = newNodes;
      }
      var g = self.parent
        .selectAll(".node")
        .data(nodes, idFunction);
      // Append the group and class to all new
      g.enter()
        .append("g")
        .attr("class", function(d) {
          if (d._isCommunity) {
            return "node communitynode";
          }
          return "node";
        }) // node is CSS class that might be edited
        .attr("id", idFunction);
      // Remove all old
      g.exit().remove();
      g.selectAll("* > *").remove();
      addQue(g);
      updateNodes();
      contextMenu.bindMenu($(".node"));
    },

    parseShapeFlag = function (shape) {
      var radius, width, height, translateX, translateY,
        fallback, source;
      switch (shape.type) {
        case NodeShaper.shapes.NONE:
          addShape = noop;
          break;
        case NodeShaper.shapes.CIRCLE:
          radius = shape.radius || 25;
          addShape = function (node, shift) {
            node
              .append("circle") // Display nodes as circles
              .attr("r", radius); // Set radius
            if (shift) {
              node.attr("cx", -shift)
                .attr("cy", -shift);
            }
          };
          break;
        case NodeShaper.shapes.RECT:
          width = shape.width || 90;
          height = shape.height || 36;
          if (_.isFunction(width)) {
            translateX = function(d) {
              return -(width(d) / 2);
            };
          } else {
            translateX = function(d) {
              return -(width / 2);
            };
          }
          if (_.isFunction(height)) {
            translateY = function(d) {
              return -(height(d) / 2);
            };
          } else {
            translateY = function() {
              return -(height / 2);
            };
          }
          addShape = function(node, shift) {
            shift = shift || 0;
            node.append("rect") // Display nodes as rectangles
              .attr("width", width) // Set width
              .attr("height", height) // Set height
              .attr("x", function(d) { return translateX(d) - shift;})
              .attr("y", function(d) { return translateY(d) - shift;})
              .attr("rx", "8")
              .attr("ry", "8");
          };
          break;
        case NodeShaper.shapes.IMAGE:
          width = shape.width || 32;
          height = shape.height || 32;
          fallback = shape.fallback || "";
          source = shape.source || fallback;
          if (_.isFunction(width)) {
            translateX = function(d) {
              return -(width(d) / 2);
            };
          } else {
            translateX = -(width / 2);
          }
          if (_.isFunction(height)) {
            translateY = function(d) {
              return -(height(d) / 2);
            };
          } else {
            translateY = -(height / 2);
          }
          addShape = function(node) {
            var img = node.append("image") // Display nodes as images
              .attr("width", width) // Set width
              .attr("height", height) // Set height
              .attr("x", translateX)
              .attr("y", translateY);
            if (_.isFunction(source)) {
              img.attr("xlink:href", source);
            } else {
              img.attr("xlink:href", function(d) {
                if (d._data[source]) {
                  return d._data[source];
                }
                return fallback;
              });
            }
          };
          break;
        case undefined:
          break;
        default:
          throw "Sorry given Shape not known!";
      }
    },

    parseLabelFlag = function (label) {
      if (_.isFunction(label)) {
        addLabel = function (node) {
          var textN = node.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .attr("fill", addLabelColor) // Force a black color
            .attr("stroke", "none"); // Make it readable
            textN.each(function(d) {
              var chunks = splitLabel(label(d));
              d3.select(this).append("tspan")
                .attr("x", "0")
                .attr("dy", "-4")
                .text(chunks[0]);
              if (chunks.length === 2) {
                d3.select(this).append("tspan")
                  .attr("x", "0")
                  .attr("dy", "16")
                  .text(chunks[1]);
              }
            });
        };
      } else {
        addLabel = function (node) {
          var textN = node.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .attr("fill", addLabelColor) // Force a black color
            .attr("stroke", "none"); // Make it readable
          textN.each(function(d) {
            var chunks = splitLabel(findFirstValue(label, d._data));
            d3.select(this).append("tspan")
              .attr("x", "0")
              .attr("dy", "-4")
              .text(chunks[0]);
            if (chunks.length === 2) {
              d3.select(this).append("tspan")
                .attr("x", "0")
                .attr("dy", "16")
                .text(chunks[1]);
            }
          });
        };
      }
    },

    parseActionFlag = function (actions) {
      if (actions.reset !== undefined && actions.reset) {
        unbindEvents();
      }
      _.each(actions, function(func, type) {
        if (type !== "reset") {
          bindEvent(type, func);
        }
      });
    },

    parseColorFlag = function (color) {
      resetColourMap();
      switch (color.type) {
        case "single":
          addColor = function (g) {
            g.attr("fill", color.fill);
          };
          addLabelColor = function (d) {
            return color.stroke;
          };
          break;
        case "expand":
          addColor = function (g) {
            g.attr("fill", function(n) {
              if (n._expanded) {
                return color.expanded;
              }
              return color.collapsed;
            });
          };
          addLabelColor = function (d) {
            return "white";
          };
          break;
        case "attribute":
          addColor = function (g) {
             g.attr("fill", function(n) {
               if (n._data === undefined) {
                 return colourMapper.getCommunityColour();
               }
               return colourMapper.getColour(findFirstValue(color.key, n._data));
             }).attr("opacity", function(n) {
               if (!n._expanded) {
                 return "0.8";
               }
               return "1";
             });
          };
          addLabelColor = function (n) {
            if (n._data === undefined) {
              return colourMapper.getForegroundCommunityColour();
            }
            return colourMapper.getForegroundColour(findFirstValue(color.key, n._data));
          };
          break;
        default:
          throw "Sorry given colour-scheme not known";
      }
    },

    parseDistortionFlag = function (dist) {
      if (dist === "reset") {
        distortion = defaultDistortion;
      } else if (_.isFunction(dist)) {
        distortion = dist;
      } else {
        throw "Sorry distortion cannot be parsed.";
      }
    },

    parseConfig = function(config) {
      if (config.shape !== undefined) {
        parseShapeFlag(config.shape);
      }
      if (config.label !== undefined) {
        parseLabelFlag(config.label);
        self.label = config.label;
      }
      if (config.actions !== undefined) {
        parseActionFlag(config.actions);
      }
      if (config.color !== undefined) {
        parseColorFlag(config.color);
        self.color = config.color;
      }
      if (config.distortion !== undefined) {
        parseDistortionFlag(config.distortion);
      }
    };

  self.parent = parent;

  unbindEvents();

  if (flags === undefined) {
    flags = {};
  }

  if (flags.shape === undefined) {
   flags.shape = {
     type: NodeShaper.shapes.RECT
   };
  }

  if (flags.color === undefined) {
    flags.color = {
      type: "single",
      fill: "#333333",
      stroke: "white"
    };
  }

  if (flags.distortion === undefined) {
    flags.distortion = "reset";
  }

  parseConfig(flags);

  if (_.isFunction(idfunc)) {
    idFunction = idfunc;
  }


  /////////////////////////////////////////////////////////
  /// Public functions
  /////////////////////////////////////////////////////////

  self.changeTo = function(config) {
    parseConfig(config);
    shapeNodes();
  };

  self.drawNodes = function (nodes) {
    shapeNodes(nodes);
  };

  self.updateNodes = function () {
    updateNodes();
  };

  self.reshapeNodes = function() {
    shapeNodes();
  };

  self.activateLabel = function(toogle) {
    if (toogle) {
      visibleLabels = true;
    } else {
      visibleLabels = false;
    }
    shapeNodes();
  };

  self.getColourMapping = function() {
    return colourMapper.getList();
  };

  self.setColourMappingListener = function(callback) {
    colourMapper.setChangeListener(callback);
  };

  self.setGVStartFunction = function(func) {
    start = func;
  };

  self.getLabel = function() {
    return self.label || "";
  };

  self.getColor = function() {
    return self.color.key || "";
  };

  self.addMenuEntry = function(name, func) {
    contextMenu.addEntry(name, func);
  };

  self.resetColourMap = resetColourMap;
}

NodeShaper.shapes = Object.freeze({
  "NONE": 0,
  "CIRCLE": 1,
  "RECT": 2,
  "IMAGE": 3
});

/*global $, d3, _, console, document, window*/
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

function PreviewAdapter(nodes, edges, viewer, config) {
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

  var self = this,
    absAdapter = new AbstractAdapter(nodes, edges, this, viewer),

    parseConfig = function(config) {
      if (config.width !== undefined) {
        absAdapter.setWidth(config.width);
      }
      if (config.height !== undefined) {
        absAdapter.setHeight(config.height);
      }
    },

    parseResult = function (result, callback) {
      var inserted = {},
        first = result.first;
      first = absAdapter.insertNode(first);
      _.each(result.nodes, function(n) {
        n = absAdapter.insertNode(n);
        inserted[n._id] = n;
      });
      _.each(result.edges, function(e) {
        absAdapter.insertEdge(e);
      });
      delete inserted[first._id];
      if (callback !== undefined && _.isFunction(callback)) {
        callback(first);
      }
    };

  config = config || {};

  parseConfig(config);

  self.loadInitialNode = function(nodeId, callback) {
    absAdapter.cleanUp();
    var cb = function(n) {
      callback(absAdapter.insertInitialNode(n));
    };
    self.loadNode(nodeId, cb);
  };

  self.loadNode = function(nodeId, callback) {
    var ns = [],
      es = [],
      result = {},
      n1 = {
        _id: 1,
        label: "Node 1",
        image: "img/stored.png"
      },
      n2 = {
        _id: 2,
        label: "Node 2"
      },
      n3 = {
        _id: 3,
        label: "Node 3"
      },
      n4 = {
        _id: 4,
        label: "Node 4"
      },
      n5 = {
        _id: 5,
        label: "Node 5"
      },
      e12 = {
        _id: "1-2",
        _from: 1,
        _to: 2,
        label: "Edge 1"
      },
      e13 = {
        _id: "1-3",
        _from: 1,
        _to: 3,
        label: "Edge 2"
      },
      e14 = {
        _id: "1-4",
        _from: 1,
        _to: 4,
        label: "Edge 3"
      },
      e15 = {
        _id: "1-5",
        _from: 1,
        _to: 5,
        label: "Edge 4"
      },
      e23 = {
        _id: "2-3",
        _from: 2,
        _to: 3,
        label: "Edge 5"
      };

    ns.push(n1);
    ns.push(n2);
    ns.push(n3);
    ns.push(n4);
    ns.push(n5);

    es.push(e12);
    es.push(e13);
    es.push(e14);
    es.push(e15);
    es.push(e23);

    result.first = n1;
    result.nodes = ns;
    result.edges = es;

    parseResult(result, callback);
  };

  self.explore = absAdapter.explore;

  self.requestCentralityChildren = function(nodeId, callback) {};

  self.createEdge = function (edgeToAdd, callback) {
    window.alert("Server-side: createEdge was triggered.");
  };

  self.deleteEdge = function (edgeToRemove, callback) {
    window.alert("Server-side: deleteEdge was triggered.");
  };

  self.patchEdge = function (edgeToPatch, patchData, callback) {
    window.alert("Server-side: patchEdge was triggered.");
  };

  self.createNode = function (nodeToAdd, callback) {
    window.alert("Server-side: createNode was triggered.");
  };

  self.deleteNode = function (nodeToRemove, callback) {
    window.alert("Server-side: deleteNode was triggered.");
    window.alert("Server-side: onNodeDelete was triggered.");
  };

  self.patchNode = function (nodeToPatch, patchData, callback) {
    window.alert("Server-side: patchNode was triggered.");
  };

  self.setNodeLimit = function (pLimit, callback) {
    absAdapter.setNodeLimit(pLimit, callback);
  };

  self.setChildLimit = function (pLimit) {
    absAdapter.setChildLimit(pLimit);
  };

  self.setWidth = absAdapter.setWidth;

  self.expandCommunity = function (commNode, callback) {
    absAdapter.expandCommunity(commNode);
    if (callback !== undefined) {
      callback();
    }
  };

}

/*global window, _*/
// These values are injected
/*global w:true, Construct, self*/
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


function WebWorkerWrapper(Class, callback) {
  "use strict";

  if (Class === undefined) {
    throw "A class has to be given.";
  }

  if (callback === undefined) {
    throw "A callback has to be given.";
  }
  var args = Array.prototype.slice.call(arguments),
    exports = {},
    createInBlobContext = function() {
      var onmessage = function(e) {
        switch(e.data.cmd) {
          case "construct":
            try {
              w = new (Function.prototype.bind.apply(
                Construct, [null].concat(e.data.args)
              ))();
              if (w) {
                self.postMessage({
                  cmd: "construct",
                  result: true
                });
              } else {
                self.postMessage({
                  cmd: "construct",
                  result: false
                });
              }
            } catch (err) {
              self.postMessage({
                cmd: "construct",
                result: false,
                error: err.message || err
            });
            }
            break;
          default:
            var msg = {
              cmd: e.data.cmd
            },
            res;
            if (w && typeof w[e.data.cmd] === "function") {
              try {
                res = w[e.data.cmd].apply(w, e.data.args);
                if (res) {
                  msg.result = res;
                }
                self.postMessage(msg);
              } catch (err1) {
                msg.error = err1.message || err1;
                self.postMessage(msg);
              }
            } else {
              msg.error = "Method not known";
              self.postMessage(msg);
            }
        }
      },

      BlobObject = function(c) {
        var code = "var w, Construct = "
          + c.toString()
          + ";self.onmessage = "
          + onmessage.toString();
        return new window.Blob(code.split());
      },
      worker,
      url = window.webkitURL || window.URL,
      blobPointer = new BlobObject(Class);
    worker = new window.Worker(url.createObjectURL(blobPointer));
    worker.onmessage = callback;
    return worker;
  },
  Wrap = function() {
    return Class.apply(this, args);
  },
  worker;

  try {
    worker = createInBlobContext();
    exports.call = function(cmd) {
      var args = Array.prototype.slice.call(arguments);
      args.shift();
      worker.postMessage({
        cmd: cmd,
        args: args
      });
    };

    args.shift();
    args.shift();
    args.unshift("construct");
    exports.call.apply(this, args);
    return exports;
  } catch (e) {
    args.shift();
    args.shift();
    Wrap.prototype = Class.prototype;
    try {
      worker = new Wrap();
    } catch (err) {
      callback({
        data: {
          cmd: "construct",
          error: err
        }
      });
      return;
    }
    exports.call = function(cmd) {
      var args = Array.prototype.slice.call(arguments),
        resp = {
          data: {
            cmd: cmd
          }
        };
      if (!_.isFunction(worker[cmd])) {
        resp.data.error = "Method not known";
        callback(resp);
        return;
      }
      args.shift();
      try {
        resp.data.result = worker[cmd].apply(worker, args);
        callback(resp);
      } catch (e) {
        resp.data.error = e;
        callback(resp);
      }
    };
    callback({
      data: {
        cmd: "construct",
        result: true
      }
    });
    return exports;
  }
}

/*global $, _, d3*/
/*global ColourMapper*/
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


function ZoomManager(width, height, svg, g, nodeShaper, edgeShaper, config, limitCallback) {
  "use strict";

  if (width === undefined || width < 0) {
    throw("A width has to be given.");
  }
  if (height === undefined || height < 0) {
    throw("A height has to be given.");
  }
  if (svg === undefined || svg.node === undefined || svg.node().tagName.toLowerCase() !== "svg") {
    throw("A svg has to be given.");
  }
  if (g === undefined || g.node === undefined || g.node().tagName.toLowerCase() !== "g") {
    throw("A group has to be given.");
  }
  if (
    nodeShaper === undefined
    || nodeShaper.activateLabel === undefined
    || nodeShaper.changeTo === undefined
    || nodeShaper.updateNodes === undefined
  ) {
    throw("The Node shaper has to be given.");
  }
  if (
    edgeShaper === undefined
    || edgeShaper.activateLabel === undefined
    || edgeShaper.updateEdges === undefined
  ) {
    throw("The Edge shaper has to be given.");
  }


  var self = this,
    fontSize,
    nodeRadius,
    labelToggle,
    currentZoom,
    currentTranslation,
    lastD3Translation,
    lastD3Scale,
    currentLimit,
    fisheye,
    currentDistortion,
    currentDistortionRadius,
    baseDist,
    baseDRadius,
    size =  width * height,
    zoom,
    slider,
    minZoom,
    limitCB = limitCallback || function() {},

    calcNodeLimit = function () {
      var div, reqSize;
      if (currentZoom >= labelToggle) {
        reqSize = fontSize * currentZoom;
        reqSize *= reqSize;
        div = 60 * reqSize;
      } else {
        reqSize = nodeRadius * currentZoom;
        reqSize *= reqSize;
        div = 4 * Math.PI * reqSize;
      }
      return Math.floor(size / div);
    },

    calcDistortionValues = function () {
      currentDistortion = baseDist / currentZoom - 0.99999999; // Always > 0
      currentDistortionRadius = baseDRadius / currentZoom;
      fisheye.distortion(currentDistortion);
      fisheye.radius(currentDistortionRadius);
    },

    reactToZoom = function(scale, transX, transY, fromButton) {
      if (fromButton) {
        if (scale !== null) {
          currentZoom = scale;
        }
      } else {
        currentZoom = scale;
      }
      if (transX !== null) {
        currentTranslation[0] += transX;
      }
      if (transY !== null) {
        currentTranslation[1] += transY;
      }
      currentLimit = calcNodeLimit();
      limitCB(currentLimit);
      nodeShaper.activateLabel(currentZoom >= labelToggle);
      edgeShaper.activateLabel(currentZoom >= labelToggle);
      calcDistortionValues();

      var transT = "translate(" + currentTranslation + ")",
      scaleT = " scale(" + currentZoom + ")";
      if (g._isCommunity) {
        g.attr("transform", transT);
      } else {
        g.attr("transform", transT + scaleT);
      }
      if (slider) {
         slider.slider("option", "value", currentZoom);
      }
    },

    getScaleDelta = function(nextScale) {
      var diff = lastD3Scale - nextScale;
      lastD3Scale = nextScale;
      return diff;
    },

    getTranslationDelta = function(nextTrans) {
      var tmp = [];
      tmp[0] = nextTrans[0] - lastD3Translation[0];
      tmp[1] = nextTrans[1] - lastD3Translation[1];
      lastD3Translation[0] = nextTrans[0];
      lastD3Translation[1] = nextTrans[1];
      return tmp;
    },

    parseConfig = function (conf) {
      if (conf === undefined) {
        conf = {};
      }
      var fontMax = conf.maxFont || 16,
      fontMin = conf.minFont || 6,
      rMax = conf.maxRadius || 25,
      rMin = conf.minRadius || 4;
      baseDist = conf.focusZoom || 1;
      baseDRadius = conf.focusRadius || 100;
      minZoom = rMin/rMax;
      fontSize = fontMax;
      nodeRadius = rMax;

      labelToggle = fontMin / fontMax;
      currentZoom = 1;
      currentTranslation = [0, 0];
      lastD3Translation = [0, 0];
      calcDistortionValues();

      currentLimit = calcNodeLimit();

      zoom = d3.behavior.zoom()
        .scaleExtent([minZoom, 1])
        .on("zoom", function() {
          //  scaleDiff = getScaleDelta(d3.event.scale),
          var
            sEvent = d3.event.sourceEvent,
            scale = currentZoom,
            translation;
          if (sEvent.type === "mousewheel" || sEvent.type === "DOMMouseScroll") {
            if (sEvent.wheelDelta) {
              if (sEvent.wheelDelta > 0) {
                scale += 0.01;
                if (scale > 1) {
                  scale = 1;
                }
              } else {
                scale -= 0.01;
                if (scale < minZoom) {
                  scale = minZoom;
                }
              }
            } else {
              if (sEvent.detail > 0) {
                scale += 0.01;
                if (scale > 1) {
                  scale = 1;
                }
              } else {
                scale -= 0.01;
                if (scale < minZoom) {
                  scale = minZoom;
                }
              }
            }
            translation = [0, 0];
          } else {
            translation = getTranslationDelta(d3.event.translate);
          }

          reactToZoom(scale, translation[0], translation[1]);
       });

    },
    mouseMoveHandle = function() {
      var focus = d3.mouse(this);
      focus[0] -= currentTranslation[0];
      focus[0] /= currentZoom;
      focus[1] -= currentTranslation[1];
      focus[1] /= currentZoom;
      fisheye.focus(focus);
      nodeShaper.updateNodes();
      edgeShaper.updateEdges();
    };



  fisheye = d3.fisheye.circular();

  parseConfig(config);

  svg.call(zoom);

  nodeShaper.changeTo({
    distortion: fisheye
  });

  svg.on("mousemove", mouseMoveHandle);

  self.translation = function() {
    return null;
  };

  self.scaleFactor = function() {
    return currentZoom;
  };

  self.scaledMouse = function() {
    return null;
  };

  self.getDistortion = function() {
    return currentDistortion;
  };

  self.getDistortionRadius = function() {
    return currentDistortionRadius;
  };

  self.getNodeLimit = function() {
    return currentLimit;
  };

  self.getMinimalZoomFactor = function() {
    return minZoom;
  };

  self.registerSlider = function(s) {
    slider = s;
  };

  self.triggerScale = function(s) {
    reactToZoom(s, null, null, true);
  };

  self.triggerTranslation = function(x, y) {
    reactToZoom(null, x, y, true);
  };

  self.changeWidth = function(w) {
    size =  width * height;
  };

}

/*global $, _, d3*/
/*global document*/
/*global modalDialogHelper, uiComponentsHelper*/
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
function ArangoAdapterControls(list, adapter) {
  "use strict";

  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (adapter === undefined) {
    throw "The ArangoAdapter has to be given.";
  }
  this.addControlChangeCollections = function(callback) {
    var prefix = "control_adapter_collections",
      idprefix = prefix + "_";

    adapter.getCollections(function(nodeCols, edgeCols) {
      adapter.getGraphs(function(graphs) {
        uiComponentsHelper.createButton(list, "Collections", prefix, function() {
          modalDialogHelper.createModalDialog("Switch Collections",
            idprefix, [{
              type: "decission",
              id: "collections",
              group: "loadtype",
              text: "Select existing collections",
              isDefault: (adapter.getGraphName() === undefined),
              interior: [
                {
                  type: "list",
                  id: "node_collection",
                  text: "Vertex collection",
                  objects: nodeCols,
                  selected: adapter.getNodeCollection()
                },{
                  type: "list",
                  id: "edge_collection",
                  text: "Edge collection",
                  objects: edgeCols,
                  selected: adapter.getEdgeCollection()
                }
              ]
            },{
              type: "decission",
              id: "graphs",
              group: "loadtype",
              text: "Select existing graph",
              isDefault: (adapter.getGraphName() !== undefined),
              interior: [
                {
                  type: "list",
                  id: "graph",
                  objects: graphs,
                  selected: adapter.getGraphName()
                }
              ]
            },{
              type: "checkbox",
              text: "Start with random vertex",
              id: "random",
              selected: true
            },{
              type: "checkbox",
              id: "undirected",
              selected: (adapter.getDirection() === "any")
            }], function () {
              var nodes = $("#" + idprefix + "node_collection")
                .children("option")
                .filter(":selected")
                .text(),
                edges = $("#" + idprefix + "edge_collection")
                  .children("option")
                  .filter(":selected")
                  .text(),
                graph = $("#" + idprefix + "graph")
                  .children("option")
                  .filter(":selected")
                  .text(),
                undirected = !!$("#" + idprefix + "undirected").prop("checked"),
                random = !!$("#" + idprefix + "random").prop("checked"),
                selected = $("input[type='radio'][name='loadtype']:checked").prop("id");
              if (selected === idprefix + "collections") {
                adapter.changeToCollections(nodes, edges, undirected);
              } else {
                adapter.changeToGraph(graph, undirected);
              }
              if (random) {
                adapter.loadRandomNode(callback);
                return;
              }
              if (_.isFunction(callback)) {
                callback();
              }
            }
          );
        });
      });
    });
  };

  this.addControlChangePriority = function() {
    var prefix = "control_adapter_priority",
      idprefix = prefix + "_",
      prioList = adapter.getPrioList(),
      label = "Group vertices";

      uiComponentsHelper.createButton(list, label, prefix, function() {
        modalDialogHelper.createModalChangeDialog(label,
          idprefix, [{
            type: "extendable",
            id: "attribute",
            objects: adapter.getPrioList()
          }], function () {
            var list = $("input[id^=" + idprefix + "attribute_]"),
              prios = [];
            list.each(function(i, t) {
              var val = $(t).val();
              if (val !== "") {
                prios.push(val);
              }
            });
            adapter.changeTo({
              prioList: prios
            });
          }
        );
      });
    /*
    adapter.getCollections(function(nodeCols, edgeCols) {
      uiComponentsHelper.createButton(list, "Collections", prefix, function() {
        modalDialogHelper.createModalDialog("Switch Collections",
          idprefix, [{
            type: "list",
            id: "nodecollection",
            objects: nodeCols
          },{
            type: "list",
            id: "edgecollection",
            objects: edgeCols
          },{
            type: "checkbox",
            id: "undirected"
          }], function () {
            var  = $("#" + idprefix + "nodecollection").attr("value"),
              edges = $("#" + idprefix + "edgecollection").attr("value"),
              undirected = !!$("#" + idprefix + "undirected").attr("checked");
            adapter.changeTo(nodes, edges, undirected);

          }
        );
      });
    });
    */
  };

  this.addAll = function() {
    this.addControlChangeCollections();
    this.addControlChangePriority();
  };
}

/*global $, _, d3*/
/*global document*/
/*global EdgeShaper, modalDialogHelper, uiComponentsHelper*/
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
function ContextMenu(id) {
  "use strict";

  if (id === undefined) {
    throw("An id has to be given.");
  }
  var div,
      ul,
      jqId = "#" + id,
      menu,

    addEntry = function(label, callback) {
      var item, inner;
      item = document.createElement("div");
      item.className = "context-menu-item";
      inner = document.createElement("div");
      inner.className = "context-menu-item-inner";
      inner.appendChild(document.createTextNode(label));
      inner.onclick = function() {
        callback(d3.select(menu.target).data()[0]);
      };
      item.appendChild(inner);
      div.appendChild(item);
    },

    bindMenu = function($objects) {
      menu = $.contextMenu.create(jqId, {
        shadow: false
      });
      $objects.each(function() {
        $(this).bind('contextmenu', function(e){
          menu.show(this,e);
          return false;
        });
      });
    },

    divFactory = function() {
      div = document.getElementById(id);
      if (div) {
        div.parentElement.removeChild(div);
      }
      div = document.createElement("div");
      div.className = "context-menu context-menu-theme-osx";
      div.id = id;
      document.body.appendChild(div);
      return div;
    };

  divFactory();

  this.addEntry = addEntry;
  this.bindMenu = bindMenu;
}


/*global $, _, d3*/
/*global document*/
/*global EdgeShaper, modalDialogHelper, uiComponentsHelper*/
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
function EdgeShaperControls(list, shaper) {
  "use strict";

  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (shaper === undefined) {
    throw "The EdgeShaper has to be given.";
  }
  var self = this;

  this.addControlOpticShapeNone = function() {
    var prefix = "control_edge_none",
    idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "None", prefix, function() {
      shaper.changeTo({
        shape: {
          type: EdgeShaper.shapes.NONE
        }
      });
    });
  };

  this.addControlOpticShapeArrow = function() {
    var prefix = "control_edge_arrow",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Arrow", prefix, function() {
      shaper.changeTo({
        shape: {
          type: EdgeShaper.shapes.ARROW
        }
      });
    });
  };



  this.addControlOpticLabel = function() {
    var prefix = "control_edge_label",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Label", prefix, function() {
      modalDialogHelper.createModalDialog("Switch Label Attribute",
        idprefix, [{
          type: "text",
          id: "key",
          text: "Edge label attribute",
          value: shaper.getLabel()
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          shaper.changeTo({
            label: key
          });
        }
      );
    });
  };

  this.addControlOpticLabelList = function() {
    var prefix = "control_edge_label",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Label", prefix, function() {
      modalDialogHelper.createModalDialog("Change Label Attribute",
        idprefix, [{
          type: "extendable",
          id: "label",
          text: "Edge label attribute",
          objects: shaper.getLabel()
        }], function () {
          var lblList = $("input[id^=" + idprefix + "label_]"),
            labels = [];
          lblList.each(function(i, t) {
            var val = $(t).val();
            if (val !== "") {
              labels.push(val);
            }
          });
          shaper.changeTo({
            label: labels
          });
        }
      );
    });
  };




  this.addControlOpticSingleColour = function() {
    var prefix = "control_edge_singlecolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Single Colour", prefix, function() {
      modalDialogHelper.createModalDialog("Switch to Colour",
        idprefix, [{
          type: "text",
          id: "stroke"
        }], function () {
          var stroke = $("#" + idprefix + "stroke").attr("value");
          shaper.changeTo({
            color: {
              type: "single",
              stroke: stroke
            }
          });
        }
      );
    });
  };

  this.addControlOpticAttributeColour = function() {
    var prefix = "control_edge_attributecolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Colour by Attribute", prefix, function() {
      modalDialogHelper.createModalDialog("Display colour by attribute",
        idprefix, [{
          type: "text",
          id: "key"
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          shaper.changeTo({
            color: {
              type: "attribute",
              key: key
            }
          });
        }
      );
    });
  };

  this.addControlOpticGradientColour = function() {
    var prefix = "control_edge_gradientcolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Gradient Colour", prefix, function() {
      modalDialogHelper.createModalDialog("Change colours for gradient",
        idprefix, [{
          type: "text",
          id: "source"
        },{
          type: "text",
          id: "target"
        }], function () {
          var source = $("#" + idprefix + "source").attr("value"),
          target = $("#" + idprefix + "target").attr("value");
          shaper.changeTo({
            color: {
              type: "gradient",
              source: source,
              target: target
            }
          });
        }
      );
    });
  };

  this.addAllOptics = function () {
    self.addControlOpticShapeNone();
    self.addControlOpticShapeArrow();
    self.addControlOpticLabel();
    self.addControlOpticSingleColour();
    self.addControlOpticAttributeColour();
    self.addControlOpticGradientColour();
  };

  this.addAllActions = function () {

  };

  this.addAll = function () {
    self.addAllOptics();
    self.addAllActions();
  };

}

/*global $, _, d3*/
/*global document, window, prompt*/
/*global modalDialogHelper, uiComponentsHelper */
/*global EventDispatcher*/
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

function EventDispatcherControls(list, nodeShaper, edgeShaper, start, dispatcherConfig) {
  "use strict";

  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (nodeShaper === undefined) {
    throw "The NodeShaper has to be given.";
  }
  if (edgeShaper === undefined) {
    throw "The EdgeShaper has to be given.";
  }
  if (start === undefined) {
    throw "The Start callback has to be given.";
  }

  var self = this,
    /*
    icons = {
      expand: "expand",
      add: "add",
      trash: "trash",
      drag: "drag",
      edge: "connect",
      edit: "edit",
      view: "view"
    },
    */
    icons = {
      expand: {
        icon: "expand",
        title: "SPOT"
      },
      add: {
        icon: "plus-square-o",
        title: "NODE"
      },
      trash: {
        icon: "trash-o",
        title: "TRASH"
      },
      drag: {
        icon: "arrows",
        title: "DRAG"
      },
      edge: {
        icon: "link",
        title: "EDGE"
      },
      edit: {
        icon: "pencil",
        title: "EDIT"
      },
      view: {
        icon: "search",
        title: "VIEW"
      }
    },
    dispatcher = new EventDispatcher(nodeShaper, edgeShaper, dispatcherConfig),
    adapter = dispatcherConfig.edgeEditor.adapter,
    askForCollection = (!!adapter
      && _.isFunction(adapter.useNodeCollection)
      && _.isFunction(adapter.useEdgeCollection)),


    appendToList = function(button) {
      list.appendChild(button);
    },
    createIcon = function(icon, title, callback) {
      var btn = uiComponentsHelper.createIconButton(
        icon,
        "control_event_" + title,
        callback
      );
      appendToList(btn);
    },
    rebindNodes = function(actions) {
      dispatcher.rebind("nodes", actions);
    },
    rebindEdges = function(actions) {
      dispatcher.rebind("edges", actions);
    },
    rebindSVG = function(actions) {
      dispatcher.rebind("svg", actions);
    },
    getCursorPosition = function (ev) {
      var e = ev || window.event,
        res = {};
      res.x = e.clientX;
      res.y = e.clientY;
      res.x += document.body.scrollLeft;
      res.y += document.body.scrollTop;
      return res;
    },
    getCursorPositionInSVG = function (ev) {
      var pos = getCursorPosition(ev),
          off = $('svg#graphViewerSVG').offset(),
          svg, bBox, bCR;
      svg = d3.select("svg#graphViewerSVG").node();
      // Normal case. SVG has no clipped view box.
      bCR = svg.getBoundingClientRect();
      if ($("svg#graphViewerSVG").height() <= bCR.height ) {
        return {
          x: pos.x - off.left,
          y: pos.y - off.top
        };
      }
      // Firefox case. SVG has a clipped view box.
      bBox = svg.getBBox();
      return {
        x: pos.x - (bCR.left - bBox.x),
        y: pos.y - (bCR.top - bBox.y)
      };
    },
    callbacks = {
      nodes: {},
      edges: {},
      svg: {}
    },

  /*******************************************
  * Create callbacks wenn clicking on objects
  *
  *******************************************/

    createNewNodeCB = function() {
      var prefix = "control_event_new_node",
        idprefix = prefix + "_",
        createCallback = function(ev) {
          var pos = getCursorPositionInSVG(ev);
          modalDialogHelper.createModalCreateDialog(
            "Create New Node",
            idprefix,
            {},
            function(data) {
              dispatcher.events.CREATENODE(data, function(node) {
                $("#" + idprefix + "modal").modal('hide');
                nodeShaper.reshapeNodes();
                start();
              }, pos.x, pos.y)();
            }
          );
        };
      callbacks.nodes.newNode = createCallback;
    },
    createViewCBs = function() {
      var prefix = "control_event_view",
        idprefix = prefix + "_",
        nodeCallback = function(n) {
          modalDialogHelper.createModalViewDialog(
            "View Node " + n._id,
            "control_event_node_view_",
            n._data,
            function() {
              modalDialogHelper.createModalEditDialog(
                "Edit Node " + n._id,
                "control_event_node_edit_",
                n._data,
                function(newData) {
                  dispatcher.events.PATCHNODE(n, newData, function() {
                    $("#control_event_node_edit_modal").modal('hide');
                  })();
                }
              );
            }
          );
        },
        edgeCallback = function(e) {
          modalDialogHelper.createModalViewDialog(
            "View Edge " + e._id,
            "control_event_edge_view_",
            e._data,
            function() {
              modalDialogHelper.createModalEditDialog(
                "Edit Edge " + e._id,
                "control_event_edge_edit_",
                e._data,
                function(newData) {
                  dispatcher.events.PATCHEDGE(e, newData, function() {
                    $("#control_event_edge_edit_modal").modal('hide');
                  })();
                }
              );
            }
          );
        };

      callbacks.nodes.view = nodeCallback;
      callbacks.edges.view = edgeCallback;
    },
    createConnectCBs = function() {
      var prefix = "control_event_connect",
        idprefix = prefix + "_",
        nodesDown = dispatcher.events.STARTCREATEEDGE(function(startNode, ev) {
          var pos = getCursorPositionInSVG(ev),
             moveCB = edgeShaper.addAnEdgeFollowingTheCursor(pos.x, pos.y);
          dispatcher.bind("svg", "mousemove", function(ev) {
            var pos = getCursorPositionInSVG(ev);
            moveCB(pos.x, pos.y);
          });
        }),
        nodesUp = dispatcher.events.FINISHCREATEEDGE(function(edge){
          edgeShaper.removeCursorFollowingEdge();
          dispatcher.bind("svg", "mousemove", function(){
            return undefined;
          });
          start();
        }),
        svgUp = function() {
          dispatcher.events.CANCELCREATEEDGE();
          edgeShaper.removeCursorFollowingEdge();
          dispatcher.bind("svg", "mousemove", function(){
            return undefined;
          });
        };
      callbacks.nodes.startEdge = nodesDown;
      callbacks.nodes.endEdge = nodesUp;
      callbacks.svg.cancelEdge = svgUp;
    },

    createEditsCBs = function() {
      var nodeCallback = function(n) {
          modalDialogHelper.createModalEditDialog(
            "Edit Node " + n._id,
            "control_event_node_edit_",
            n._data,
            function(newData) {
              dispatcher.events.PATCHNODE(n, newData, function() {
                $("#control_event_node_edit_modal").modal('hide');
              })();
            }
          );
        },
        edgeCallback = function(e) {
          modalDialogHelper.createModalEditDialog(
            "Edit Edge " + e._id,
            "control_event_edge_edit_",
            e._data,
            function(newData) {
              dispatcher.events.PATCHEDGE(e, newData, function() {
                $("#control_event_edge_edit_modal").modal('hide');
              })();
            }
          );
        };
      callbacks.nodes.edit = nodeCallback;
      callbacks.edges.edit = edgeCallback;
    },

    createDeleteCBs = function() {
      var nodeCallback = function(n) {
          modalDialogHelper.createModalDeleteDialog(
            "Delete Node " + n._id,
            "control_event_node_delete_",
            n,
            function(n) {
              dispatcher.events.DELETENODE(function() {
                $("#control_event_node_delete_modal").modal('hide');
                nodeShaper.reshapeNodes();
                edgeShaper.reshapeEdges();
                start();
              })(n);
            }
          );
        },
        edgeCallback = function(e) {
          modalDialogHelper.createModalDeleteDialog(
            "Delete Edge " + e._id,
            "control_event_edge_delete_",
            e,
            function(e) {
              dispatcher.events.DELETEEDGE(function() {
                $("#control_event_edge_delete_modal").modal('hide');
                nodeShaper.reshapeNodes();
                edgeShaper.reshapeEdges();
                start();
              })(e);
            }
          );
        };
      callbacks.nodes.del = nodeCallback;
      callbacks.edges.del = edgeCallback;
    },

    createSpotCB = function() {
     callbacks.nodes.spot = dispatcher.events.EXPAND;
    };

  createNewNodeCB();
  createViewCBs();
  createConnectCBs();
  createEditsCBs();
  createDeleteCBs();
  createSpotCB();

  /*******************************************
  * Raw rebind objects
  *
  *******************************************/
  this.dragRebinds = function() {
    return {
      nodes: {
        drag: dispatcher.events.DRAG
      }
    };
  };

  this.newNodeRebinds = function() {
    return {
      svg: {
        click: callbacks.nodes.newNode
      }
    };
  };

  this.viewRebinds = function() {
      return {
        nodes: {
          click: callbacks.nodes.view
        },
        edges: {
          click: callbacks.edges.view
        }
      };
  };

  this.connectNodesRebinds = function() {
    return {
      nodes: {
        mousedown: callbacks.nodes.startEdge,
        mouseup: callbacks.nodes.endEdge
      },
      svg: {
        mouseup: callbacks.svg.cancelEdge
      }
    };
  };

  this.editRebinds = function() {
      return {
        nodes: {
          click: callbacks.nodes.edit
        },
        edges: {
          click: callbacks.edges.edit
        }
      };
  };

  this.expandRebinds = function() {
    return {
      nodes: {
        click: callbacks.nodes.spot
      }
    };
  };

  this.deleteRebinds = function() {
    return {
      nodes: {
        click: callbacks.nodes.del
      },
      edges: {
        click: callbacks.edges.del
      }
    };
  };

  this.rebindAll = function(obj) {
    rebindNodes(obj.nodes);
    rebindEdges(obj.edges);
    rebindSVG(obj.svg);
  };

  /*******************************************
  * Inject controls into right-click menus
  *
  *******************************************/

  nodeShaper.addMenuEntry("View", callbacks.nodes.view);
  nodeShaper.addMenuEntry("Edit", callbacks.nodes.edit);
  nodeShaper.addMenuEntry("Spot", callbacks.nodes.spot);
  nodeShaper.addMenuEntry("Trash", callbacks.nodes.del);

  edgeShaper.addMenuEntry("View", callbacks.edges.view);
  edgeShaper.addMenuEntry("Edit", callbacks.edges.edit);
  edgeShaper.addMenuEntry("Trash", callbacks.edges.del);


  /*******************************************
  * Functions to add controls
  *
  *******************************************/


  this.addControlNewNode = function() {
    var icon = icons.add,
      idprefix = "select_node_collection",
      callback = function() {
        if (askForCollection && adapter.getNodeCollections().length > 1) {
          modalDialogHelper.createModalDialog("Select Vertex Collection",
            idprefix, [{
              type: "list",
              id: "vertex",
              objects: adapter.getNodeCollections(),
              text: "Select collection",
              selected: adapter.getSelectedNodeCollection()
            }], function () {
              var nodeCollection = $("#" + idprefix + "vertex")
                  .children("option")
                  .filter(":selected")
                  .text();
              adapter.useNodeCollection(nodeCollection);
            },
            "Select"
          );
        }
        self.rebindAll(self.newNodeRebinds());
      };
    createIcon(icon, "new_node", callback);
  };

  this.addControlView = function() {
    var icon = icons.view,
      callback = function() {
        self.rebindAll(self.viewRebinds());
      };
    createIcon(icon, "view", callback);
  };

  this.addControlDrag = function() {
    var icon = icons.drag,
      callback = function() {
        self.rebindAll(self.dragRebinds());
      };
    createIcon(icon, "drag", callback);
  };

  this.addControlEdit = function() {
    var icon = icons.edit,
      callback = function() {
        self.rebindAll(self.editRebinds());
      };
    createIcon(icon, "edit", callback);
  };

  this.addControlExpand = function() {
    var icon = icons.expand,
      callback = function() {
        self.rebindAll(self.expandRebinds());
      };
    createIcon(icon, "expand", callback);
  };

  this.addControlDelete = function() {
    var icon = icons.trash,
      callback = function() {
        self.rebindAll(self.deleteRebinds());
      };
    createIcon(icon, "delete", callback);
  };

  this.addControlConnect = function() {
    var icon = icons.edge,
      idprefix = "select_edge_collection",
      callback = function() {
        if (askForCollection && adapter.getEdgeCollections().length > 1) {
          modalDialogHelper.createModalDialog("Select Edge Collection",
            idprefix, [{
              type: "list",
              id: "edge",
              objects: adapter.getEdgeCollections(),
              text: "Select collection",
              selected: adapter.getSelectedEdgeCollection()
            }], function () {
              var edgeCollection = $("#" + idprefix + "edge")
                  .children("option")
                  .filter(":selected")
                  .text();
              adapter.useEdgeCollection(edgeCollection);
            },
            "Select"
          );
        }
        self.rebindAll(self.connectNodesRebinds());
      };
    createIcon(icon, "connect", callback);
  };

  this.addAll = function () {
    self.addControlDrag();
    self.addControlView();
    self.addControlEdit();
    self.addControlExpand();
    self.addControlDelete();
    self.addControlConnect();
    self.addControlNewNode();
  };
}

/*global $, _, d3*/
/*global document*/
/*global modalDialogHelper, uiComponentsHelper*/
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
function GharialAdapterControls(list, adapter) {
  "use strict";

  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (adapter === undefined) {
    throw "The GharialAdapter has to be given.";
  }
  this.addControlChangeGraph = function(callback) {
    var prefix = "control_adapter_graph",
      idprefix = prefix + "_";

    adapter.getGraphs(function(graphs) {
      uiComponentsHelper.createButton(list, "Graph", prefix, function() {
        modalDialogHelper.createModalDialog("Switch Graph",
          idprefix, [{
            type: "list",
            id: "graph",
            objects: graphs,
            text: "Select graph",
            selected: adapter.getGraphName()
          },{
            type: "checkbox",
            text: "Start with random vertex",
            id: "random",
            selected: true
          },{
            type: "checkbox",
            id: "undirected",
            selected: (adapter.getDirection() === "any")
          }], function () {
            var graph = $("#" + idprefix + "graph")
                .children("option")
                .filter(":selected")
                .text(),
              undirected = !!$("#" + idprefix + "undirected").prop("checked"),
              random = !!$("#" + idprefix + "random").prop("checked");
            adapter.changeToGraph(graph, undirected);
            if (random) {
              adapter.loadRandomNode(callback);
              return;
            }
            if (_.isFunction(callback)) {
              callback();
            }
          }
        );
      });
    });
  };

  this.addControlChangePriority = function() {
    var prefix = "control_adapter_priority",
      idprefix = prefix + "_",
      label = "Group vertices";

    uiComponentsHelper.createButton(list, label, prefix, function() {
      modalDialogHelper.createModalChangeDialog(label,
        idprefix, [{
          type: "extendable",
          id: "attribute",
          objects: adapter.getPrioList()
        }], function () {
          var attrList = $("input[id^=" + idprefix + "attribute_]"),
            prios = [];
          _.each(attrList, function(t) {
            var val = $(t).val();
            if (val !== "") {
              prios.push(val);
            }
          });
          adapter.changeTo({
            prioList: prios
          });
        }
      );
    });
  };

  this.addAll = function() {
    this.addControlChangeGraph();
    this.addControlChangePriority();
  };
}

/*global document, $, _ */
/*global d3, window*/
/*global GraphViewer, EventDispatcherControls, EventDispatcher, NodeShaper */
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

/*******************************************************************************
* Assume the widget is imported via an iframe.
* Hence we append everything directly to the body
* and make use of all available space.
*******************************************************************************/

function GraphViewerPreview(container, viewerConfig) {
  "use strict";

  /*******************************************************************************
  * Internal variables and functions
  *******************************************************************************/

  var svg,
    width,
    height,
    viewer,
    createTB,
    adapterConfig,
    dispatcherUI,
    //mousePointerBox = document.createElement("div"),


    createSVG = function() {
      return d3.select(container)
        .append("svg")
        .attr("id", "graphViewerSVG")
        .attr("width",width)
        .attr("height",height)
        .attr("class", "graph-viewer")
        .attr("style", "width:" + width + "px;height:" + height + ";");
    },

    shouldCreateToolbox = function(config) {
      var counter = 0;
      _.each(config, function(v, k) {
        if (v === false) {
          delete config[k];
        } else {
          counter++;
        }
      });
      return counter > 0;
    },

    addRebindsToList = function(list, rebinds) {
      _.each(rebinds, function(acts, obj) {
        list[obj] = list[obj] || {};
        _.each(acts, function(func, trigger) {
          list[obj][trigger] = func;
        });
      });
    },

    parseActions = function(config) {
      if (!config) {
        return;
      }
      var allActions = {};
      if (config.drag) {
        addRebindsToList(allActions, dispatcherUI.dragRebinds());
      }
      if (config.create) {
        addRebindsToList(allActions, dispatcherUI.newNodeRebinds());
        addRebindsToList(allActions, dispatcherUI.connectNodesRebinds());
      }
      if (config.remove) {
        addRebindsToList(allActions, dispatcherUI.deleteRebinds());
      }
      if (config.expand) {
        addRebindsToList(allActions, dispatcherUI.expandRebinds());
      }
      if (config.edit) {
        addRebindsToList(allActions, dispatcherUI.editRebinds());
      }
      dispatcherUI.rebindAll(allActions);
    },

    createToolbox = function(config) {
      var toolbox = document.createElement("div");
      dispatcherUI = new EventDispatcherControls(
        toolbox,
        viewer.nodeShaper,
        viewer.edgeShaper,
        viewer.start,
        viewer.dispatcherConfig
      );
      toolbox.id = "toolbox";
      toolbox.className = "btn-group btn-group-vertical pull-left toolbox";
      container.appendChild(toolbox);
      /*
      mousePointerBox.id = "mousepointer";
      mousePointerBox.className = "mousepointer";
      container.appendChild(mousePointerBox);
      */
      _.each(config, function(v, k) {
        switch(k) {
          case "expand":
            dispatcherUI.addControlExpand();
            break;
          case "create":
            dispatcherUI.addControlNewNode();
            dispatcherUI.addControlConnect();
            break;
          case "drag":
            dispatcherUI.addControlDrag();
            break;
          case "edit":
            dispatcherUI.addControlEdit();
            break;
          case "remove":
            dispatcherUI.addControlDelete();
            break;
        }
      });
    },

    createDispatcherOnly = function(config) {
      var toolbox = document.createElement("div");
      dispatcherUI = new EventDispatcherControls(
        toolbox,
//        mousePointerBox,
        viewer.nodeShaper,
        viewer.edgeShaper,
        viewer.start,
        viewer.dispatcherConfig
      );
    },

    changeConfigToPreviewGraph = function() {
      if (viewerConfig) {
        // Fix nodeShaper:
        if (viewerConfig.nodeShaper) {
          if (viewerConfig.nodeShaper.label) {
            viewerConfig.nodeShaper.label = "label";
          }
          if (
            viewerConfig.nodeShaper.shape
            && viewerConfig.nodeShaper.shape.type === NodeShaper.shapes.IMAGE
            && viewerConfig.nodeShaper.shape.source
          ) {
            viewerConfig.nodeShaper.shape.source = "image";
          }
        }
        // Fix nodeShaper:
        if (viewerConfig.edgeShaper) {
          if (viewerConfig.edgeShaper.label) {
            viewerConfig.edgeShaper.label = "label";
          }
        }
      }
    },

    createViewer = function() {
      changeConfigToPreviewGraph();
      return new GraphViewer(svg, width, height, adapterConfig, viewerConfig);
    };


  /*******************************************************************************
  * Execution start
  *******************************************************************************/

  width = container.offsetWidth;
  height = container.offsetHeight;
  adapterConfig = {
    type: "preview"
  };

  viewerConfig = viewerConfig || {};
  createTB = shouldCreateToolbox(viewerConfig.toolbox);
  if (createTB) {
    width -= 43;
  }
  svg = createSVG();
  viewer = createViewer();
  if (createTB) {
    createToolbox(viewerConfig.toolbox);
  } else {
    createDispatcherOnly();
  }
  viewer.loadGraph("1");
  parseActions(viewerConfig.actions);

}

/*global document, $, _ */
/*global EventDispatcherControls, NodeShaperControls, EdgeShaperControls */
/*global LayouterControls, GharialAdapterControls*/
/*global GraphViewer, d3, window*/
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

function GraphViewerUI(container, adapterConfig, optWidth, optHeight, viewerConfig, startNode) {
  "use strict";

  if (container === undefined) {
    throw "A parent element has to be given.";
  }
  if (!container.id) {
    throw "The parent element needs an unique id.";
  }
  if (adapterConfig === undefined) {
    throw "An adapter configuration has to be given";
  }

  var graphViewer,
    width = (optWidth || container.offsetWidth - 81),
    height = optHeight || container.offsetHeight,
    menubar = document.createElement("ul"),
    background = document.createElement("div"),
    colourList,
    nodeShaperUI,
    edgeShaperUI,
    adapterUI,
    slider,
    searchAttrField,
    searchAttrExampleList,
    //mousePointerBox = document.createElement("div"),
    svg,

    makeFilterDiv = function() {
      var
        div = document.createElement("div"),
        innerDiv = document.createElement("div"),
        queryLine = document.createElement("div"),
        searchAttrDiv = document.createElement("div"),
        searchAttrExampleToggle = document.createElement("button"),
        searchAttrExampleCaret = document.createElement("span"),
        searchValueField = document.createElement("input"),
        searchStart = document.createElement("img"),
        equalsField = document.createElement("span"),

        showSpinner = function() {
          $(background).css("cursor", "progress");
        },

        hideSpinner = function() {
          $(background).css("cursor", "");
        },

        alertError = function(msg) {
          window.alert(msg);
        },

        resultCB = function(res) {
          hideSpinner();
          if (res && res.errorCode && res.errorCode === 404) {
            alertError("Sorry could not find a matching node.");
            return;
          }
          return;
        },

        searchFunction = function() {
          showSpinner();
          if (searchAttrField.value === ""
            || searchAttrField.value === undefined) {
            graphViewer.loadGraph(
              searchValueField.value,
              resultCB
            );
          } else {
            graphViewer.loadGraphWithAttributeValue(
              searchAttrField.value,
              searchValueField.value,
              resultCB
            );
          }
        };

      div.id = "filterDropdown";
      div.className = "headerDropdown";
      innerDiv.className = "dropdownInner";
      queryLine.className = "queryline";

      searchAttrField = document.createElement("input");
      searchAttrExampleList = document.createElement("ul");

      searchAttrDiv.className = "pull-left input-append searchByAttribute";
      searchAttrField.id = "attribute";
      //searchAttrField.className = "input";
      searchAttrField.type = "text";
      searchAttrField.placeholder = "Attribute name";
      searchAttrExampleToggle.id = "attribute_example_toggle";
      searchAttrExampleToggle.className = "button-neutral gv_example_toggle";
      searchAttrExampleCaret.className = "caret gv_caret";
      searchAttrExampleList.className = "gv-dropdown-menu";
      searchValueField.id = "value";
      searchValueField.className = "searchInput gv_searchInput";
      //searchValueField.className = "filterValue";
      searchValueField.type = "text";
      searchValueField.placeholder = "Attribute value";
      searchStart.id = "loadnode";
      searchStart.className = "gv-search-submit-icon";
      equalsField.className = "searchEqualsLabel";
      equalsField.appendChild(document.createTextNode("=="));

      innerDiv.appendChild(queryLine);
      queryLine.appendChild(searchAttrDiv);

      searchAttrDiv.appendChild(searchAttrField);
      searchAttrDiv.appendChild(searchAttrExampleToggle);
      searchAttrDiv.appendChild(searchAttrExampleList);
      searchAttrExampleToggle.appendChild(searchAttrExampleCaret);
      queryLine.appendChild(equalsField);
      queryLine.appendChild(searchValueField);
      queryLine.appendChild(searchStart);

      searchStart.onclick = searchFunction;
      $(searchValueField).keypress(function(e) {
        if (e.keyCode === 13 || e.which === 13) {
          searchFunction();
          return false;
        }
      });

      searchAttrExampleToggle.onclick = function() {
        $(searchAttrExampleList).slideToggle(200);
      };

      div.appendChild(innerDiv);
      return div;
    },

    makeConfigureDiv = function () {
      var div, innerDiv, nodeList, nodeHeader, colList, colHeader,
          edgeList, edgeHeader;
      div = document.createElement("div");
      div.id = "configureDropdown";
      div.className = "headerDropdown";
      innerDiv = document.createElement("div");
      innerDiv.className = "dropdownInner";
      nodeList = document.createElement("ul");
      nodeHeader = document.createElement("li");
      nodeHeader.className = "nav-header";
      nodeHeader.appendChild(document.createTextNode("Vertices"));
      edgeList = document.createElement("ul");
      edgeHeader = document.createElement("li");
      edgeHeader.className = "nav-header";
      edgeHeader.appendChild(document.createTextNode("Edges"));
      colList = document.createElement("ul");
      colHeader = document.createElement("li");
      colHeader.className = "nav-header";
      colHeader.appendChild(document.createTextNode("Connection"));
      nodeList.appendChild(nodeHeader);
      edgeList.appendChild(edgeHeader);
      colList.appendChild(colHeader);
      innerDiv.appendChild(nodeList);
      innerDiv.appendChild(edgeList);
      innerDiv.appendChild(colList);
      div.appendChild(innerDiv);
      return {
        configure: div,
        nodes: nodeList,
        edges: edgeList,
        col: colList
      };
    },

    makeConfigure = function (div, idConf, idFilter) {
      var ul, liConf, aConf, spanConf, liFilter, aFilter, spanFilter, lists;
      div.className = "headerButtonBar";
      ul = document.createElement("ul");
      ul.className = "headerButtonList";

      div.appendChild(ul);

      liConf = document.createElement("li");
      liConf.className = "enabled";
      aConf = document.createElement("a");
      aConf.id = idConf;
      aConf.className = "headerButton";
      spanConf = document.createElement("span");
      spanConf.className = "icon_arangodb_settings2";
      $(spanConf).attr("title", "Configure");

      ul.appendChild(liConf);
      liConf.appendChild(aConf);
      aConf.appendChild(spanConf);

      liFilter = document.createElement("li");
      liFilter.className = "enabled";
      aFilter = document.createElement("a");
      aFilter.id = idFilter;
      aFilter.className = "headerButton";
      spanFilter = document.createElement("span");
      spanFilter.className = "icon_arangodb_filter";
      $(spanFilter).attr("title", "Filter");

      ul.appendChild(liFilter);
      liFilter.appendChild(aFilter);
      aFilter.appendChild(spanFilter);
      lists = makeConfigureDiv();
      lists.filter = makeFilterDiv();
      aConf.onclick = function () {
        $('#filterdropdown').removeClass('activated');
        $('#configuredropdown').toggleClass('activated');
        $(lists.configure).slideToggle(200);
        $(lists.filter).hide();
      };

      aFilter.onclick = function () {
        $('#configuredropdown').removeClass('activated');
        $('#filterdropdown').toggleClass('activated');
        $(lists.filter).slideToggle(200);
        $(lists.configure).hide();
      };

      return lists;
    },

    createSVG = function () {
      return d3.select("#" + container.id + " #background")
        .append("svg")
        .attr("id", "graphViewerSVG")
        .attr("width",width)
        .attr("height",height)
        .attr("class", "graph-viewer")
        .style("width", width + "px")
        .style("height", height + "px");
    },

    createZoomUIWidget = function() {
      var zoomUI = document.createElement("div"),
        zoomButtons = document.createElement("div"),
        btnTop = document.createElement("button"),
        btnLeft = document.createElement("button"),
        btnRight = document.createElement("button"),
        btnBottom = document.createElement("button");
      zoomUI.className = "gv_zoom_widget";
      zoomButtons.className = "gv_zoom_buttons_bg";

      btnTop.className = "btn btn-icon btn-zoom btn-zoom-top gv-zoom-btn pan-top";
      btnLeft.className = "btn btn-icon btn-zoom btn-zoom-left gv-zoom-btn pan-left";
      btnRight.className = "btn btn-icon btn-zoom btn-zoom-right gv-zoom-btn pan-right";
      btnBottom.className = "btn btn-icon btn-zoom btn-zoom-bottom gv-zoom-btn pan-bottom";
      btnTop.onclick = function() {
        graphViewer.zoomManager.triggerTranslation(0, -10);
      };
      btnLeft.onclick = function() {
        graphViewer.zoomManager.triggerTranslation(-10, 0);
      };
      btnRight.onclick = function() {
        graphViewer.zoomManager.triggerTranslation(10, 0);
      };
      btnBottom.onclick = function() {
        graphViewer.zoomManager.triggerTranslation(0, 10);
      };

      zoomButtons.appendChild(btnTop);
      zoomButtons.appendChild(btnLeft);
      zoomButtons.appendChild(btnRight);
      zoomButtons.appendChild(btnBottom);

      slider = document.createElement("div");
      slider.id = "gv_zoom_slider";
      slider.className = "gv_zoom_slider";

      background.appendChild(zoomUI);
      background.insertBefore(zoomUI, svg[0][0]);

      zoomUI.appendChild(zoomButtons);
      zoomUI.appendChild(slider);
      $( "#gv_zoom_slider" ).slider({
        orientation: "vertical",
        min: graphViewer.zoomManager.getMinimalZoomFactor(),
        max: 1,
        value: 1,
        step: 0.01,
        slide: function( event, ui ) {
          graphViewer.zoomManager.triggerScale(ui.value);
        }
      });
      graphViewer.zoomManager.registerSlider($("#gv_zoom_slider"));
    },

    createToolbox = function() {
      var toolbox = document.createElement("div"),
        dispatcherUI = new EventDispatcherControls(
          toolbox,
          graphViewer.nodeShaper,
          graphViewer.edgeShaper,
          graphViewer.start,
          graphViewer.dispatcherConfig
        );
      toolbox.id = "toolbox";
      toolbox.className = "btn-group btn-group-vertical toolbox";
      background.insertBefore(toolbox, svg[0][0]);
      dispatcherUI.addAll();
      // Default selection
      $("#control_event_expand").click();
    },

    updateAttributeExamples = function() {
      searchAttrExampleList.innerHTML = "";
      var throbber = document.createElement("li"),
        throbberImg = document.createElement("img");
      throbber.appendChild(throbberImg);
      throbberImg.className = "gv-throbber";
      searchAttrExampleList.appendChild(throbber);
      graphViewer.adapter.getAttributeExamples(function(res) {
        searchAttrExampleList.innerHTML = "";
        _.each(res, function(r) {
          var entry = document.createElement("li"),
            link = document.createElement("a"),
            lbl = document.createElement("label");
          entry.appendChild(link);
          link.appendChild(lbl);
          lbl.appendChild(document.createTextNode(r));
          lbl.className = "gv_dropdown_label";
          searchAttrExampleList.appendChild(entry);
          entry.onclick = function() {
            searchAttrField.value = r;
            $(searchAttrExampleList).slideToggle(200);
          };
        });
      });
    },

    createMenu = function() {
      var transparentHeader = document.createElement("div"),
        buttons = document.createElement("div"),
        title = document.createElement("a"),
        configureLists = makeConfigure(
          buttons,
          "configuredropdown",
          "filterdropdown"
        );

      nodeShaperUI = new NodeShaperControls(
        configureLists.nodes,
        graphViewer.nodeShaper
      );
      edgeShaperUI = new EdgeShaperControls(
        configureLists.edges,
        graphViewer.edgeShaper
      );
      adapterUI = new GharialAdapterControls(
        configureLists.col,
        graphViewer.adapter
      );

      menubar.id = "menubar";

      transparentHeader.className = "headerBar";

      buttons.id = "modifiers";

      title.appendChild(document.createTextNode("Graph Viewer"));
      title.className = "arangoHeader";

      /*
      nodeShaperDropDown.id = "nodeshapermenu";
      edgeShaperDropDown.id = "edgeshapermenu";
      layouterDropDown.id = "layoutermenu";
      adapterDropDown.id = "adaptermenu";
      */

      menubar.appendChild(transparentHeader);
      menubar.appendChild(configureLists.configure);
      menubar.appendChild(configureLists.filter);
      transparentHeader.appendChild(buttons);
      transparentHeader.appendChild(title);

      adapterUI.addControlChangeGraph(function() {
        updateAttributeExamples();
        graphViewer.start();
      });
      adapterUI.addControlChangePriority();
      // nodeShaperUI.addControlOpticLabelAndColour(graphViewer.adapter);
      nodeShaperUI.addControlOpticLabelAndColourList(graphViewer.adapter);
      edgeShaperUI.addControlOpticLabelList();

      /*
      buttons.appendChild(nodeShaperDropDown);
      buttons.appendChild(edgeShaperDropDown);
      buttons.appendChild(layouterDropDown);
      buttons.appendChild(adapterDropDown);

      nodeShaperUI.addAll();
      edgeShaperUI.addAll();
      layouterUI.addAll();
      adapterUI.addAll();
      */
      updateAttributeExamples();
    },

    createColourList = function() {
      colourList = nodeShaperUI.createColourMappingList();
      colourList.className = "gv-colour-list";
      background.insertBefore(colourList, svg[0][0]);
    };
  container.appendChild(menubar);
  container.appendChild(background);
  background.className = "contentDiv gv-background ";
  background.id = "background";

  viewerConfig = viewerConfig || {};
  viewerConfig.zoom = true;


  svg = createSVG();
  graphViewer = new GraphViewer(svg, width, height, adapterConfig, viewerConfig);

  createToolbox();
  createZoomUIWidget();
  createMenu();
  createColourList();

  if (startNode) {
    if (typeof startNode === "string") {
      graphViewer.loadGraph(startNode);
    } else {
      graphViewer.loadGraphWithRandomStart(function(node) {
        if (node && node.errorCode) {
          window.alert("Sorry your graph seems to be empty");
        }
      });
    }
  }

  this.changeWidth = function(w) {
    graphViewer.changeWidth(w);
    var reducedW = w - 75;
    svg.attr("width", reducedW)
      .style("width", reducedW + "px");
  };
}

/*global document, $, _ */
/*global d3, window*/
/*global GraphViewer, EventDispatcherControls, EventDispatcher */
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

/*******************************************************************************
* Assume the widget is imported via an iframe.
* Hence we append everything directly to the body
* and make use of all available space.
*******************************************************************************/

function GraphViewerWidget(viewerConfig, startNode) {
  "use strict";

  /*******************************************************************************
  * Internal variables and functions
  *******************************************************************************/

  var svg,
    container,
    width,
    height,
    viewer,
    createTB,
    adapterConfig,
    dispatcherUI,
    //mousePointerBox = document.createElement("div"),


    createSVG = function() {
      return d3.select(container)
        .append("svg")
        .attr("id", "graphViewerSVG")
        .attr("width",width)
        .attr("height",height)
        .attr("class", "graph-viewer")
        .attr("style", "width:" + width + "px;height:" + height + "px;");
    },

    shouldCreateToolbox = function(config) {
      var counter = 0;
      _.each(config, function(v, k) {
        if (v === false) {
          delete config[k];
        } else {
          counter++;
        }
      });
      return counter > 0;
    },

    addRebindsToList = function(list, rebinds) {
      _.each(rebinds, function(acts, obj) {
        list[obj] = list[obj] || {};
        _.each(acts, function(func, trigger) {
          list[obj][trigger] = func;
        });
      });
    },

    parseActions = function(config) {
      if (!config) {
        return;
      }
      var allActions = {};
      if (config.drag) {
        addRebindsToList(allActions, dispatcherUI.dragRebinds());
      }
      if (config.create) {
        addRebindsToList(allActions, dispatcherUI.newNodeRebinds());
        addRebindsToList(allActions, dispatcherUI.connectNodesRebinds());
      }
      if (config.remove) {
        addRebindsToList(allActions, dispatcherUI.deleteRebinds());
      }
      if (config.expand) {
        addRebindsToList(allActions, dispatcherUI.expandRebinds());
      }
      if (config.edit) {
        addRebindsToList(allActions, dispatcherUI.editRebinds());
      }
      dispatcherUI.rebindAll(allActions);
    },

    createToolbox = function(config) {
      var toolbox = document.createElement("div");
      dispatcherUI = new EventDispatcherControls(
        toolbox,
        viewer.nodeShaper,
        viewer.edgeShaper,
        viewer.start,
        viewer.dispatcherConfig
      );
      toolbox.id = "toolbox";
      toolbox.className = "btn-group btn-group-vertical pull-left toolbox";
      container.appendChild(toolbox);
      /*
      mousePointerBox.id = "mousepointer";
      mousePointerBox.className = "mousepointer";
      container.appendChild(mousePointerBox);
      */
      _.each(config, function(v, k) {
        switch(k) {
          case "expand":
            dispatcherUI.addControlExpand();
            break;
          case "create":
            dispatcherUI.addControlNewNode();
            dispatcherUI.addControlConnect();
            break;
          case "drag":
            dispatcherUI.addControlDrag();
            break;
          case "edit":
            dispatcherUI.addControlEdit();
            break;
          case "remove":
            dispatcherUI.addControlDelete();
            break;
        }
      });
    },

    createDispatcherOnly = function(config) {
      var toolbox = document.createElement("div");
      dispatcherUI = new EventDispatcherControls(
        toolbox,
//        mousePointerBox,
        viewer.nodeShaper,
        viewer.edgeShaper,
        viewer.start,
        viewer.dispatcherConfig
      );
    },

    createViewer = function() {
      return new GraphViewer(svg, width, height, adapterConfig, viewerConfig);
    };


  /*******************************************************************************
  * Execution start
  *******************************************************************************/

  container = document.body;
  width = container.offsetWidth;
  height = container.offsetHeight;
  adapterConfig = {
    type: "foxx",
    route: "."
  };

  viewerConfig = viewerConfig || {};
  createTB = shouldCreateToolbox(viewerConfig.toolbox);
  if (createTB) {
    width -= 43;
  }
  svg = createSVG();
  viewer = createViewer();
  if (createTB) {
    createToolbox(viewerConfig.toolbox);
  } else {
    createDispatcherOnly();
  }
  if (startNode) {
    viewer.loadGraph(startNode);
  }
  parseActions(viewerConfig.actions);

}

/*global $, _, d3*/
/*global document*/
/*global EdgeShaper, modalDialogHelper, uiComponentsHelper*/
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
function LayouterControls(list, layouter) {
  "use strict";

  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (layouter === undefined) {
    throw "The Layouter has to be given.";
  }
  var self = this;

  this.addControlGravity = function() {
    var prefix = "control_layout_gravity",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Gravity", prefix, function() {
      modalDialogHelper.createModalDialog("Switch Gravity Strength",
        idprefix, [{
          type: "text",
          id: "value"
        }], function () {
          var value = $("#" + idprefix + "value").attr("value");
          layouter.changeTo({
            gravity: value
          });
        }
      );
    });
  };

  this.addControlCharge = function() {
    var prefix = "control_layout_charge",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Charge", prefix, function() {
      modalDialogHelper.createModalDialog("Switch Charge Strength",
        idprefix, [{
          type: "text",
          id: "value"
        }], function () {
          var value = $("#" + idprefix + "value").attr("value");
          layouter.changeTo({
            charge: value
          });
        }
      );
    });
  };

  this.addControlDistance = function() {
    var prefix = "control_layout_distance",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Distance", prefix, function() {
      modalDialogHelper.createModalDialog("Switch Distance Strength",
        idprefix, [{
          type: "text",
          id: "value"
        }], function () {
          var value = $("#" + idprefix + "value").attr("value");
          layouter.changeTo({
            distance: value
          });
        }
      );
    });
  };


  this.addAll = function () {
    self.addControlDistance();
    self.addControlGravity();
    self.addControlCharge();
  };

}

/*global document, $, _ */

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

var modalDialogHelper = modalDialogHelper || {};

(function dialogHelper() {
  "use strict";

  var
    bindSubmit = function(button) {
      $(document).bind("keypress.key13", function(e) {
        if (e.which && e.which === 13) {
          $(button).click();
        }
      });
    },

    unbindSubmit = function() {
      $(document).unbind("keypress.key13");
    },

    createDialogWithObject = function (title, buttonTitle, idprefix, object, callback) {
      var tableToJSON,
        callbackCapsule = function() {
          callback(tableToJSON);
        },
        table = modalDialogHelper.modalDivTemplate(title, buttonTitle, idprefix, callbackCapsule),
        firstRow = document.createElement("tr"),
        firstCell = document.createElement("th"),
        secondCell = document.createElement("th"),
        thirdCell = document.createElement("th"),
        addRow = document.createElement("button"),
        newCounter = 1,
        insertRow;

      tableToJSON = function() {
        var result = {};
        _.each($("#" + idprefix + "table tr:not(#first_row)"), function(tr) {

          var key = $(".keyCell input", tr).val(),
            value = $(".valueCell input", tr).val();
          result[key] = value;
        });
        return result;
      };

      table.appendChild(firstRow);
      firstRow.id = "first_row";
      firstRow.appendChild(firstCell);
      firstCell.className = "keyCell";

      firstRow.appendChild(secondCell);
      secondCell.className = "valueCell";

      firstRow.appendChild(thirdCell);

      thirdCell.className = "actionCell";
      thirdCell.appendChild(addRow);

      addRow.id = idprefix + "new";
      addRow.className = "graphViewer-icon-button gv-icon-small add";

      insertRow = function(value, key) {
        var internalRegex = /^_(id|rev|key|from|to)/,
          tr = document.createElement("tr"),
          actTh = document.createElement("th"),
          keyTh = document.createElement("th"),
          valueTh = document.createElement("th"),
          deleteInput,
          keyInput,
          valueInput;
        if (internalRegex.test(key)) {
          return;
        }
        table.appendChild(tr);

        tr.appendChild(keyTh);
        keyTh.className = "keyCell";
        keyInput = document.createElement("input");
        keyInput.type = "text";
        keyInput.id = idprefix + key + "_key";
        keyInput.value = key;
        keyTh.appendChild(keyInput);


        tr.appendChild(valueTh);
        valueTh.className = "valueCell";
        valueInput = document.createElement("input");
        valueInput.type = "text";
        valueInput.id = idprefix + key + "_value";
        if ("object" === typeof value) {
          valueInput.value = JSON.stringify(value);
        } else {
          valueInput.value = value;
        }

        valueTh.appendChild(valueInput);


        tr.appendChild(actTh);
        actTh.className = "actionCell";
        deleteInput = document.createElement("button");
        deleteInput.id = idprefix + key + "_delete";
        deleteInput.className = "graphViewer-icon-button gv-icon-small delete";

        actTh.appendChild(deleteInput);

        deleteInput.onclick = function() {
          table.removeChild(tr);
        };

      };

      addRow.onclick = function() {
        insertRow("", "new_" + newCounter);
        newCounter++;
      };

      _.each(object, insertRow);
      $("#" + idprefix + "modal").modal('show');
    },

    createViewWithObject = function (title, buttonTitle, idprefix, object, callback) {
      var table = modalDialogHelper.modalDivTemplate(title, buttonTitle, idprefix, callback),
        firstRow = document.createElement("tr"),
        firstCell = document.createElement("th"),
        pre = document.createElement("pre");
      table.appendChild(firstRow);
      firstRow.appendChild(firstCell);
      firstCell.appendChild(pre);
      pre.className = "gv-object-view";
      pre.innerHTML = JSON.stringify(object, null, 2);
      $("#" + idprefix + "modal").modal('show');
    },

    createTextInput = function(id, value) {
      var input = document.createElement("input");
        input.type = "text";
        input.id = id;
        input.value = value;
        return input;
    },

    createCheckboxInput = function(id, selected) {
      var input = document.createElement("input");
      input.type = "checkbox";
      input.id = id;
      input.checked = selected;
      return input;
    },

    createListInput = function(id, list, selected) {
      var input = document.createElement("select");
      input.id = id;
      _.each(
        _.sortBy(list, function(e) {
          return e.toLowerCase();
        }), function(entry) {
        var option = document.createElement("option");
        option.value = entry;
        option.selected = (entry === selected);
        option.appendChild(
          document.createTextNode(entry)
        );
        input.appendChild(option);
      });
      return input;
    },

    displayDecissionRowsOfGroup = function(group) {
      var options = $(".decission_" + group),
      selected = $("input[type='radio'][name='" + group + "']:checked").attr("id");
      options.each(function() {
        if ($(this).attr("decider") === selected) {
          $(this).css("display", "");
        } else {
          $(this).css("display", "none");
        }
      });
    },

    insertModalRow,

    insertDecissionInput = function(idPre, idPost, group,
      text, isDefault, interior, contentTh, table) {
      var input = document.createElement("input"),
        id = idPre + idPost,
        lbl = document.createElement("label"),
        tBody = document.createElement("tbody");
      input.id = id;
      input.type = "radio";
      input.name = group;
      input.className = "gv-radio-button";
      lbl.className = "radio";
      contentTh.appendChild(lbl);
      lbl.appendChild(input);
      lbl.appendChild(document.createTextNode(text));
      table.appendChild(tBody);
      $(tBody).toggleClass("decission_" + group, true);
      $(tBody).attr("decider", id);
      _.each(interior, function(o) {
        insertModalRow(tBody, idPre, o);
      });
      if (isDefault) {
        input.checked = true;
      } else {
        input.checked = false;
      }
      lbl.onclick = function(e) {
        displayDecissionRowsOfGroup(group);
        e.stopPropagation();
      };
      displayDecissionRowsOfGroup(group);
    },

    insertExtendableInput = function(idPre, idPost, list, contentTh, table, tr) {
      var rows = [],
        i,
        id = idPre + idPost,
        lastId = 1,
        buttonTh = document.createElement("th"),
        addLineButton = document.createElement("button"),
        input = document.createElement("input"),
        addNewLine = function(content) {
          lastId++;
          var innerTr = document.createElement("tr"),
            innerLabelTh = document.createElement("th"),
            innerContentTh = document.createElement("th"),
            innerButtonTh = document.createElement("th"),
            innerInput = document.createElement("input"),
            removeRow = document.createElement("button"),
            lastItem;
          innerInput.type = "text";
          innerInput.id = id + "_" + lastId;
          innerInput.value = content || "";
          if (rows.length === 0) {
            lastItem = $(tr);
          } else {
            lastItem = $(rows[rows.length - 1]);
          }
          lastItem.after(innerTr);
          innerTr.appendChild(innerLabelTh);
          innerLabelTh.className = "collectionTh capitalize";
          innerLabelTh.appendChild(document.createTextNode(idPost + " " + lastId + ":"));
          innerTr.appendChild(innerContentTh);
          innerContentTh.className = "collectionTh";
          innerContentTh.appendChild(innerInput);
          removeRow.id = id + "_" + lastId + "_remove";
          removeRow.className = "graphViewer-icon-button gv-icon-small delete";
          removeRow.onclick = function() {
            table.removeChild(innerTr);
            rows.splice(rows.indexOf(innerTr), 1 );
          };
          innerButtonTh.appendChild(removeRow);
          innerTr.appendChild(innerButtonTh);
          rows.push(innerTr);
        };
      input.type = "text";
      input.id = id + "_1";
      contentTh.appendChild(input);
      buttonTh.appendChild(addLineButton);
      tr.appendChild(buttonTh);
      addLineButton.onclick = function() {
        addNewLine();
      };
      addLineButton.id = id + "_addLine";
      addLineButton.className = "graphViewer-icon-button gv-icon-small add";
      if (typeof list === "string" && list.length > 0) {
        list = [list];
      }
      if (list.length > 0) {
        input.value = list[0];
      }
      for (i = 1; i < list.length; i++) {
        addNewLine(list[i]);
      }
    },

    modalContent = function(title, idprefix) {
      // Create needed Elements

      var div = document.createElement("div"),
        headerDiv = document.createElement("div"),
        buttonDismiss = document.createElement("button"),
        header = document.createElement("a"),
        bodyDiv = document.createElement("div"),
        bodyTable = document.createElement("table");

      // Set Classnames and attributes.
      div.id = idprefix + "modal";
      div.className = "modal hide fade createModalDialog";
      div.setAttribute("tabindex", "-1");
      div.setAttribute("role", "dialog");
      div.setAttribute("aria-labelledby", "myModalLabel");
      div.setAttribute("aria-hidden", true);
      div.style.display = "none";
      div.onhidden = function() {
        unbindSubmit();
        document.body.removeChild(div);
      };

      headerDiv.className = "modal-header";
      header.className = "arangoHeader";
      buttonDismiss.id = idprefix + "modal_dismiss";
      buttonDismiss.className = "close";
      buttonDismiss.dataDismiss = "modal";
      buttonDismiss.ariaHidden = "true";
      buttonDismiss.appendChild(document.createTextNode("×"));

      header.appendChild(document.createTextNode(title));

      bodyDiv.className = "modal-body";

      bodyTable.id = idprefix + "table";

      // Append in correct ordering
      div.appendChild(headerDiv);
      div.appendChild(bodyDiv);

      headerDiv.appendChild(buttonDismiss);
      headerDiv.appendChild(header);

      bodyDiv.appendChild(bodyTable);

      document.body.appendChild(div);

      buttonDismiss.onclick = function() {
        unbindSubmit();
        $("#" + idprefix + "modal").modal('hide');
      };

      return {
        div: div,
        bodyTable: bodyTable
      };
    };

  insertModalRow = function(table, idprefix, o) {
    var tr = document.createElement("tr"),
      labelTh = document.createElement("th"),
      contentTh = document.createElement("th");
    table.appendChild(tr);
    tr.appendChild(labelTh);
    labelTh.className = "collectionTh";
    if (o.text) {
      labelTh.appendChild(document.createTextNode(o.text + ":"));
    } else {
      labelTh.className += " capitalize";
      if (o.type && o.type === "extenadable") {
        labelTh.appendChild(document.createTextNode(o.id + ":"));
      } else {
        labelTh.appendChild(document.createTextNode(o.id + ":"));
      }
    }
    tr.appendChild(contentTh);
    contentTh.className = "collectionTh";
    switch(o.type) {
      case "text":
        contentTh.appendChild(createTextInput(idprefix + o.id, o.value || ""));
        break;
      case "checkbox":
        contentTh.appendChild(createCheckboxInput(idprefix + o.id, o.selected || false));
        break;
      case "list":
        contentTh.appendChild(createListInput(idprefix + o.id, o.objects, o.selected || undefined));
        break;
      case "extendable":
        insertExtendableInput(idprefix, o.id, o.objects, contentTh, table, tr);
        break;
      case "decission":
        insertDecissionInput(idprefix, o.id, o.group, o.text,
          o.isDefault, o.interior, contentTh, table);
        labelTh.innerHTML = "";
        break;
      default:
        //Sorry unknown
        table.removeChild(tr);
        break;
    }
    return tr;
  };


  modalDialogHelper.modalDivTemplate = function (title, buttonTitle, idprefix, callback) {

    buttonTitle = buttonTitle || "Switch";

    var footerDiv = document.createElement("div"),
      buttonCancel = document.createElement("button"),
      buttonSubmit = document.createElement("button"),
      content = modalContent(title, idprefix);

    footerDiv.className = "modal-footer";

    buttonCancel.id = idprefix + "cancel";
    buttonCancel.className = "button-close btn-margin";
    buttonCancel.appendChild(document.createTextNode("Close"));

    buttonSubmit.id = idprefix + "submit";
    buttonSubmit.className = "button-success";
    buttonSubmit.style.marginRight = "8px";
    buttonSubmit.appendChild(document.createTextNode(buttonTitle));

    content.div.appendChild(footerDiv);
    footerDiv.appendChild(buttonSubmit);
    footerDiv.appendChild(buttonCancel);

    // Add click events
    buttonCancel.onclick = function() {
      unbindSubmit();
      $("#" + idprefix + "modal").modal('hide');
    };
    buttonSubmit.onclick = function() {
      unbindSubmit();
      callback();
      $("#" + idprefix + "modal").modal('hide');
    };
    bindSubmit(buttonSubmit);
    // Return the table which has to be filled somewhere else
    return content.bodyTable;
  };

  modalDialogHelper.createModalDialog = function(title, idprefix, objects, callback, buttonTitle) {
    var table =  modalDialogHelper.modalDivTemplate(title, buttonTitle, idprefix, callback);
    _.each(objects, function(o) {
      insertModalRow(table, idprefix, o);
    });
    $("#" + idprefix + "modal").modal('show');
  };

  modalDialogHelper.createModalChangeDialog = function(title, idprefix, objects, callback) {
    var table =  modalDialogHelper.modalDivTemplate(title, "Change", idprefix, callback);
    _.each(objects, function(o) {
      insertModalRow(table, idprefix, o);
    });
    $("#" + idprefix + "modal").modal('show');
  };

  modalDialogHelper.createModalEditDialog = function(title, idprefix, object, callback) {
    createDialogWithObject(title, "Save", idprefix, object, callback);
  };

  modalDialogHelper.createModalCreateDialog = function(title, idprefix, object, callback) {
    createDialogWithObject(title, "Create", idprefix, object, callback);
  };

  modalDialogHelper.createModalViewDialog = function(title, idprefix, object, callback) {
    createViewWithObject(title, "Edit", idprefix, object, callback);
  };

  modalDialogHelper.createModalDeleteDialog = function(title, idprefix, object, callback) {
    var footerDiv = document.createElement("div"),
      buttonCancel = document.createElement("button"),
      buttonSubmit = document.createElement("button"),
      content = modalContent(title, idprefix);

    footerDiv.className = "modal-footer";

    buttonCancel.id = idprefix + "cancel";
    buttonCancel.className = "button-close btn-margin";
    buttonCancel.appendChild(document.createTextNode("Close"));

    buttonSubmit.id = idprefix + "submit";
    buttonSubmit.className = "button-danger";
    buttonSubmit.style.marginRight = "8px";
    buttonSubmit.appendChild(document.createTextNode("Delete"));

    content.div.appendChild(footerDiv);
    footerDiv.appendChild(buttonSubmit);
    footerDiv.appendChild(buttonCancel);

    // Add click events
    buttonCancel.onclick = function() {
      unbindSubmit();
      $("#" + idprefix + "modal").modal('hide');
    };
    buttonSubmit.onclick = function() {
      unbindSubmit();
      callback(object);
      $("#" + idprefix + "modal").modal('hide');
    };
    bindSubmit(buttonSubmit);
    $("#" + idprefix + "modal").modal('show');
  };

}());

/*global $, _, d3*/
/*global document*/
/*global NodeShaper, modalDialogHelper, uiComponentsHelper*/
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
function NodeShaperControls(list, shaper) {
  "use strict";

  if (list === undefined) {
    throw "A list element has to be given.";
  }
  if (shaper === undefined) {
    throw "The NodeShaper has to be given.";
  }
  var self = this,
    colourDiv,

    fillColourDiv = function(mapping) {
      while (colourDiv.hasChildNodes()) {
          colourDiv.removeChild(colourDiv.lastChild);
      }
      var list = document.createElement("ul");
      colourDiv.appendChild(list);
      _.each(mapping, function(obj, col) {
        var ul = document.createElement("ul"),
          els = obj.list,
          fore = obj.front;
        ul.style.backgroundColor = col;
        ul.style.color = fore;
        _.each(els, function(e) {
          var li = document.createElement("li");
          li.appendChild(document.createTextNode(e));
          ul.appendChild(li);
        });
        list.appendChild(ul);
      });
    };

  this.addControlOpticShapeNone = function() {
    uiComponentsHelper.createButton(list, "None", "control_node_none", function() {
      shaper.changeTo({
        shape: {
          type: NodeShaper.shapes.NONE
        }
      });
    });
  };

  this.addControlOpticShapeCircle = function() {
    var prefix = "control_node_circle",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Circle", prefix, function() {
      modalDialogHelper.createModalDialog("Switch to Circle",
        idprefix, [{
          type: "text",
          id: "radius"
        }], function () {
          var r = $("#" + idprefix + "radius").attr("value");
          shaper.changeTo({
            shape: {
              type: NodeShaper.shapes.CIRCLE,
              radius: r
            }
          });
        }
      );
    });
  };

  this.addControlOpticShapeRect = function() {
    var prefix = "control_node_rect",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Rectangle", prefix, function() {
      modalDialogHelper.createModalDialog("Switch to Rectangle",
        "control_node_rect_", [{
          type: "text",
          id: "width"
        },{
          type: "text",
          id: "height"
        }], function () {
          var w = $("#" + idprefix + "width").attr("value"),
          h = $("#" + idprefix + "height").attr("value");
          shaper.changeTo({
            shape: {
              type: NodeShaper.shapes.RECT,
              width: w,
              height: h
            }
          });
        }
      );
    });
  };

  this.addControlOpticLabel = function() {
    var prefix = "control_node_label",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Label", prefix, function() {
      modalDialogHelper.createModalChangeDialog("Change label attribute",
        idprefix, [{
          type: "text",
          id: "key"
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          shaper.changeTo({
            label: key
          });
        }
      );
    });
  };

  //////////////////////////////////////////////////////////////////
  //  Colour Buttons
  //////////////////////////////////////////////////////////////////

  this.addControlOpticSingleColour = function() {
    var prefix = "control_node_singlecolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Single Colour", prefix, function() {
      modalDialogHelper.createModalDialog("Switch to Colour",
        idprefix, [{
          type: "text",
          id: "fill"
        },{
          type: "text",
          id: "stroke"
        }], function () {
          var fill = $("#" + idprefix + "fill").attr("value"),
          stroke = $("#" + idprefix + "stroke").attr("value");
          shaper.changeTo({
            color: {
              type: "single",
              fill: fill,
              stroke: stroke
            }
          });
        }
      );
    });
  };

  this.addControlOpticAttributeColour = function() {
    var prefix = "control_node_attributecolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Colour by Attribute", prefix, function() {
      modalDialogHelper.createModalDialog("Display colour by attribute",
        idprefix, [{
          type: "text",
          id: "key"
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          shaper.changeTo({
            color: {
              type: "attribute",
              key: key
            }
          });
        }
      );
    });
  };

  this.addControlOpticExpandColour = function() {
    var prefix = "control_node_expandcolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Expansion Colour", prefix, function() {
      modalDialogHelper.createModalDialog("Display colours for expansion",
        idprefix, [{
          type: "text",
          id: "expanded"
        },{
          type: "text",
          id: "collapsed"
        }], function () {
          var expanded = $("#" + idprefix + "expanded").attr("value"),
          collapsed = $("#" + idprefix + "collapsed").attr("value");
          shaper.changeTo({
            color: {
              type: "expand",
              expanded: expanded,
              collapsed: collapsed
            }
          });
        }
      );
    });
  };

  //////////////////////////////////////////////////////////////////
  //  Mixed Buttons
  //////////////////////////////////////////////////////////////////

  this.addControlOpticLabelAndColour = function(adapter) {
    var prefix = "control_node_labelandcolour",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Label", prefix, function() {
      modalDialogHelper.createModalChangeDialog("Change label attribute",
        idprefix, [{
          type: "text",
          id: "label-attribute",
          text: "Vertex label attribute",
          value: shaper.getLabel() || ""
        },{
          type: "decission",
          id: "samecolour",
          group: "colour",
          text: "Use this attribute for coloring, too",
          isDefault: (shaper.getLabel() === shaper.getColor())
        },{
          type: "decission",
          id: "othercolour",
          group: "colour",
          text: "Use different attribute for coloring",
          isDefault: (shaper.getLabel() !== shaper.getColor()),
          interior: [
            {
              type: "text",
              id: "colour-attribute",
              text: "Color attribute",
              value: shaper.getColor() || ""
            }
          ]
        }], function () {
          var key = $("#" + idprefix + "label-attribute").attr("value"),
            colourkey = $("#" + idprefix + "colour-attribute").attr("value"),
            selected = $("input[type='radio'][name='colour']:checked").attr("id");
          if (selected === idprefix + "samecolour") {
            colourkey = key;
          }
          shaper.changeTo({
            label: key,
            color: {
              type: "attribute",
              key: colourkey
            }
          });
          if (colourDiv === undefined) {
            colourDiv = self.createColourMappingList();
          }

        }
      );
    });
  };

  this.addControlOpticLabelAndColourList = function(adapter) {
    var prefix = "control_node_labelandcolourlist",
      idprefix = prefix + "_";
    uiComponentsHelper.createButton(list, "Label", prefix, function() {
      modalDialogHelper.createModalChangeDialog("Change label attribute",
        idprefix, [{
          type: "extendable",
          id: "label",
          text: "Vertex label attribute",
          objects: shaper.getLabel()
        },{
          type: "decission",
          id: "samecolour",
          group: "colour",
          text: "Use this attribute for coloring, too",
          isDefault: (shaper.getLabel() === shaper.getColor())
        },{
          type: "decission",
          id: "othercolour",
          group: "colour",
          text: "Use different attribute for coloring",
          isDefault: (shaper.getLabel() !== shaper.getColor()),
          interior: [
            {
              type: "extendable",
              id: "colour",
              text: "Color attribute",
              objects: shaper.getColor() || ""
            }
          ]
        }], function () {
          var lblList = $("input[id^=" + idprefix + "label_]"),
            colList = $("input[id^=" + idprefix + "colour_]"),
            selected = $("input[type='radio'][name='colour']:checked").attr("id"),
            labels = [], colours = [];
          lblList.each(function(i, t) {
            var val = $(t).val();
            if (val !== "") {
              labels.push(val);
            }
          });
          colList.each(function(i, t) {
            var val = $(t).val();
            if (val !== "") {
              colours.push(val);
            }
          });
          if (selected === idprefix + "samecolour") {
            colours = labels;
          }
          shaper.changeTo({
            label: labels,
            color: {
              type: "attribute",
              key: colours
            }
          });
          if (colourDiv === undefined) {
            colourDiv = self.createColourMappingList();
          }
        }
      );
    });
  };

  //////////////////////////////////////////////////////////////////
  //  Multiple Buttons
  //////////////////////////////////////////////////////////////////

  this.addAllOptics = function () {
    self.addControlOpticShapeNone();
    self.addControlOpticShapeCircle();
    self.addControlOpticShapeRect();
    self.addControlOpticLabel();
    self.addControlOpticSingleColour();
    self.addControlOpticAttributeColour();
    self.addControlOpticExpandColour();
  };

  this.addAllActions = function () {

  };

  this.addAll = function () {
    self.addAllOptics();
    self.addAllActions();
  };

  //////////////////////////////////////////////////////////////////
  //  Colour Mapping List
  //////////////////////////////////////////////////////////////////

  this.createColourMappingList = function() {
    if (colourDiv !== undefined) {
      return colourDiv;
    }
    colourDiv = document.createElement("div");
    colourDiv.id = "node_colour_list";
    fillColourDiv(shaper.getColourMapping());
    shaper.setColourMappingListener(fillColourDiv);
    return colourDiv;
  };
}

/*global document, $, _ */

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

var uiComponentsHelper = uiComponentsHelper || {};

(function componentsHelper() {
  "use strict";

  uiComponentsHelper.createButton = function(list, title, prefix, callback) {
    var li = document.createElement("li"),
      button = document.createElement("button");
    li.className = "graph_control " + prefix;
    li.id = prefix;
    li.appendChild(button);
    button.className = "button-primary gv_dropdown_entry";
    button.appendChild(document.createTextNode(title));
    list.appendChild(li);
    button.id = prefix + "_button";
    button.onclick = callback;
  };

  uiComponentsHelper.createIconButton = function(iconInfo, prefix, callback) {
    var button = document.createElement("div"),
        icon = document.createElement("h6"),
        label = document.createElement("h6");
    button.className = "gv_action_button";
    button.id = prefix;
    button.onclick = function() {
      $(".gv_action_button").each(function(i, btn) {
        $(btn).toggleClass("active", false);
      });
      $(button).toggleClass("active", true);
      callback();
    };
    icon.className = "fa gv_icon_icon fa-" + iconInfo.icon;
    label.className = "gv_button_title";
    button.appendChild(icon);
    button.appendChild(label);
    label.appendChild(document.createTextNode(iconInfo.title));
    return button;
  };

}());

/*global _, $*/
/*global ArangoAdapter, JSONAdapter, FoxxAdapter, PreviewAdapter, GharialAdapter*/
/*global ForceLayouter, EdgeShaper, NodeShaper, ZoomManager */
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

function GraphViewer(svg, width, height, adapterConfig, config) {
  "use strict";

  // Make the html aware of xmlns:xlink
  $("html").attr("xmlns:xlink", "http://www.w3.org/1999/xlink");

  // Check if all required inputs are given
  if (svg === undefined || svg.append === undefined) {
    throw "SVG has to be given and has to be selected using d3.select";
  }

  if (width === undefined || width <= 0) {
    throw "A width greater 0 has to be given";
  }

  if (height === undefined || height <= 0) {
    throw "A height greater 0 has to be given";
  }

  if (adapterConfig === undefined || adapterConfig.type === undefined) {
    throw "An adapter configuration has to be given";
  }

  // Globally disable the right-click menu
  /*
  svg[0][0].oncontextmenu = function() {
    return false;
  };
  */
  var self = this,
    adapter,
    nodeShaper,
    edgeShaper,
    layouter,
    zoomManager,
    graphContainer,
    nodeContainer,
    edgeContainer,
    fixedSize,
    edges = [],
    nodes = [],

  parseLayouterConfig = function (config) {
    if (!config) {
      // Default
      config = {};
      config.nodes = nodes;
      config.links = edges;
      config.width = width;
      config.height = height;
      layouter = new ForceLayouter(config);
      return;
    }
    switch (config.type.toLowerCase()) {
      case "force":
        config.nodes = nodes;
        config.links = edges;
        config.width = width;
        config.height = height;
        layouter = new ForceLayouter(config);
        break;
      default:
        throw "Sorry unknown layout type.";
    }
  },

  nodeLimitCallBack = function(limit) {
    adapter.setNodeLimit(limit, self.start);
  },

  parseZoomConfig = function(config) {
    if (config) {
      zoomManager = new ZoomManager(width, height, svg,
        graphContainer, nodeShaper, edgeShaper,
        {}, nodeLimitCallBack);
    }
  },

  parseConfig = function(config) {
    var esConf = config.edgeShaper || {},
      nsConf = config.nodeShaper || {},
      idFunc = nsConf.idfunc || undefined,
      zConf = config.zoom || false;
    esConf.shape = esConf.shape || {
      type: EdgeShaper.shapes.ARROW
    };
    parseLayouterConfig(config.layouter);
    edgeContainer = graphContainer.append("g");
    edgeShaper = new EdgeShaper(edgeContainer, esConf);
    nodeContainer = graphContainer.append("g");
    nodeShaper = new NodeShaper(nodeContainer, nsConf, idFunc);
    layouter.setCombinedUpdateFunction(nodeShaper, edgeShaper);
    parseZoomConfig(zConf);
  };

  switch (adapterConfig.type.toLowerCase()) {
    case "arango":
      adapterConfig.width = width;
      adapterConfig.height = height;
      adapter = new ArangoAdapter(
        nodes,
        edges,
        this,
        adapterConfig
      );
      adapter.setChildLimit(10);
      break;
    case "gharial":
      adapterConfig.width = width;
      adapterConfig.height = height;
      adapter = new GharialAdapter(
        nodes,
        edges,
        this,
        adapterConfig
      );
      adapter.setChildLimit(10);
      break;
    case "foxx":
      adapterConfig.width = width;
      adapterConfig.height = height;
      adapter = new FoxxAdapter(
        nodes,
        edges,
        adapterConfig.route,
        this,
        adapterConfig
      );
      break;
    case "json":
      adapter = new JSONAdapter(
        adapterConfig.path,
        nodes,
        edges,
        this,
        width,
        height
      );
      break;
    case "preview":
      adapterConfig.width = width;
      adapterConfig.height = height;
      adapter = new PreviewAdapter(
        nodes,
        edges,
        this,
        adapterConfig
      );
      break;
    default:
      throw "Sorry unknown adapter type.";
  }

  graphContainer = svg.append("g");

  parseConfig(config || {});

  this.start = function() {
    layouter.stop();
    nodeShaper.drawNodes(nodes);
    edgeShaper.drawEdges(edges);
    layouter.start();
  };

  this.loadGraph = function(nodeId, callback) {
//    loadNode
//  loadInitialNode
    adapter.loadInitialNode(nodeId, function (node) {
      if (node.errorCode) {
        callback(node);
        return;
      }
      node._expanded = true;
      self.start();
      if (_.isFunction(callback)) {
        callback();
      }
    });
  };

  this.loadGraphWithRandomStart = function(callback) {
    adapter.loadRandomNode(function (node) {
      if (node.errorCode && node.errorCode === 404) {
        callback(node);
        return;
      }
      node._expanded = true;
      self.start();
      if (_.isFunction(callback)) {
        callback();
      }
    });
  };

  this.loadGraphWithAttributeValue = function(attribute, value, callback) {
    adapter.loadInitialNodeByAttributeValue(attribute, value, function (node) {
      if (node.errorCode) {
        callback(node);
        return;
      }
      node._expanded = true;
      self.start();
      if (_.isFunction(callback)) {
        callback();
      }
    });
  };

  this.cleanUp = function() {
    nodeShaper.resetColourMap();
    edgeShaper.resetColourMap();
  };

  this.changeWidth = function(w) {
    layouter.changeWidth(w);
    zoomManager.changeWidth(w);
    adapter.setWidth(w);
  };

  this.dispatcherConfig = {
    expand: {
      edges: edges,
      nodes: nodes,
      startCallback: self.start,
      adapter: adapter,
      reshapeNodes: nodeShaper.reshapeNodes
    },
    drag: {
      layouter: layouter
    },
    nodeEditor: {
      nodes: nodes,
      adapter: adapter
    },
    edgeEditor: {
      edges: edges,
      adapter: adapter
    }
  };
  this.adapter = adapter;
  this.nodeShaper = nodeShaper;
  this.edgeShaper = edgeShaper;
  this.layouter = layouter;
  this.zoomManager = zoomManager;
}

/*global window, Backbone, $, arangoHelper */
(function() {
  'use strict';
  window.arangoCollectionModel = Backbone.Model.extend({
    initialize: function () {
    },

    idAttribute: "name",

    urlRoot: "/_api/collection",
    defaults: {
      id: "",
      name: "",
      status: "",
      type: "",
      isSystem: false,
      picture: ""
    },

    getProperties: function () {
      var data2;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/collection/" + encodeURIComponent(this.get("id")) + "/properties",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          data2 = data;
        },
        error: function(data) {
          data2 = data;
        }
      });
      return data2;
    },
    getFigures: function () {
      var data2;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/collection/" + this.get("id") + "/figures",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          data2 = data;
        },
        error: function(data) {
          data2 = data;
        }
      });
      return data2;
    },
    getRevision: function () {
      var data2;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/collection/" + this.get("id") + "/revision",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          data2 = data;
        },
        error: function(data) {
          data2 = data;
        }
      });
      return data2;
    },

    getIndex: function () {
      var data2;
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/index/?collection=" + this.get("id"),
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          data2 = data;
        },
        error: function(data) {
          data2 = data;
        }
      });
      return data2;
    },

  createIndex: function (postParameter) {
      var returnVal = false;

      $.ajax({
          cache: false,
          type: "POST",
          url: "/_api/index?collection="+ this.get("id"),
          data: JSON.stringify(postParameter),
          contentType: "application/json",
          processData: false,
          async: false,
          success: function() {
            returnVal = true;
          },
          error: function(data) {
            returnVal = data;
          }
      });
      return returnVal;
  },

      deleteIndex: function (id) {
          var returnval = false;
          $.ajax({
              cache: false,
              type: 'DELETE',
              url: "/_api/index/"+ this.get("name") +"/"+encodeURIComponent(id),
              async: false,
              success: function () {
                returnval = true;
              },
              error: function () {
                returnval = false;
              }
          });
          return returnval;
      },

    truncateCollection: function () {
      $.ajax({
        async: false,
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/truncate",
        success: function () {
          arangoHelper.arangoNotification('Collection truncated');
        },
        error: function () {
          arangoHelper.arangoError('Collection error');
        }
      });
    },

    loadCollection: function () {
      var self = this;
      $.ajax({
        async: false,
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/load",
        success: function () {
          self.set("status", "loaded");
          arangoHelper.arangoNotification('Collection loaded');
        },
        error: function () {
          arangoHelper.arangoError('Collection error');
        }
      });
    },

    unloadCollection: function () {
      var self = this;
      $.ajax({
        async:false,
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/unload",
        success: function () {
          self.set("status", "unloaded");
          arangoHelper.arangoNotification('Collection unloaded');
        },
        error: function () {
          arangoHelper.arangoError('Collection error');
        }
      });
    },

    renameCollection: function (name) {
      var self = this,
        result = false;
      $.ajax({
        cache: false,
        type: "PUT",
        async: false, // sequential calls!
        url: "/_api/collection/" + this.get("id") + "/rename",
        data: '{"name":"' + name + '"}',
        contentType: "application/json",
        processData: false,
        success: function() {
          self.set("name", name);
          result = true;
        },
        error: function(data) {
          try {
            var parsed = JSON.parse(data.responseText);
            result = parsed.errorMessage;
          }
          catch (e) {
            result = false;
          }
        }
      });
      return result;
    },

    changeCollection: function (wfs, journalSize) {
      var result = false;

      $.ajax({
        cache: false,
        type: "PUT",
        async: false, // sequential calls!
        url: "/_api/collection/" + this.get("id") + "/properties",
        data: '{"waitForSync":' + wfs + ',"journalSize":' + JSON.stringify(journalSize) + '}',
        contentType: "application/json",
        processData: false,
        success: function() {
          result = true;
        },
        error: function(data) {
          try {
            var parsed = JSON.parse(data.responseText);
            result = parsed.errorMessage;
          }
          catch (e) {
            result = false;
          }
        }
      });
      return result;
    }

  });
}());

/*global window, Backbone */

window.DatabaseModel = Backbone.Model.extend({

  idAttribute: "name",

  initialize: function () {
    'use strict';
  },

  isNew: function() {
    'use strict';
    return false;
  },
  sync: function(method, model, options) {
    'use strict';
    if (method === "update") {
      method = "create";
    }
    return Backbone.sync(method, model, options);
  },

  url: "/_api/database",

  defaults: {
  }


});

/*global window, Backbone, arangoHelper, _ */

window.arangoDocumentModel = Backbone.Model.extend({
  initialize: function () {
    'use strict';
  },
  urlRoot: "/_api/document",
  defaults: {
    _id: "",
    _rev: "",
    _key: ""
  },
  getSorted: function () {
    'use strict';
    var self = this;
    var keys = Object.keys(self.attributes).sort(function (l, r) {
      var l1 = arangoHelper.isSystemAttribute(l);
      var r1 = arangoHelper.isSystemAttribute(r);

      if (l1 !== r1) {
        if (l1) {
          return -1;
        }
        return 1;
      }

      return l < r ? -1 : 1;
    });

    var sorted = {};
    _.each(keys, function (k) {
      sorted[k] = self.attributes[k];
    });
    return sorted;
  }
});

/*global window, Backbone */
(function () {
  'use strict';
  window.ArangoQuery = Backbone.Model.extend({

    urlRoot: "/_api/user",

    defaults: {
      name: "",
      type: "custom",
      value: ""
    }

  });
}());

/*global Backbone, window */

window.Replication = Backbone.Model.extend({
  defaults: {
    state: {},
    server: {}
  },

  initialize: function () {
  }

});

// obsolete file

/*global window, Backbone */

window.Statistics = Backbone.Model.extend({
  defaults: {
  },

  url: function() {
    'use strict';
    return "/_admin/statistics";
  }
});

/*global window, Backbone */

window.StatisticsDescription = Backbone.Model.extend({
  defaults: {
    "figures" : "",
    "groups" : ""
  },
  url: function() {
    'use strict';

    return "/_admin/statistics-description";
  }

});

/*jshint strict: false */
/*global Backbone, $, window */
window.Users = Backbone.Model.extend({
  defaults: {
    user: "",
    active: false,
    extra: {}
  },

  idAttribute : "user",

  parse : function (d) {
    this.isNotNew = true;
    return d;
  },

  isNew: function () {
    return !this.isNotNew;
  },

  url: function () {
    if (this.isNew()) {
      return "/_api/user";
    }
    if (this.get("user") !== "") {
      return "/_api/user/" + this.get("user");
    }
    return "/_api/user";
  },

  checkPassword: function(passwd) {
    var result = false;

    $.ajax({
      cache: false,
      type: "POST",
      async: false, // sequential calls!
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({ passwd: passwd }),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        result = data.result;
      }
    });
    return result;
  },

  setPassword: function(passwd) {
    $.ajax({
      cache: false,
      type: "PATCH",
      async: false, // sequential calls!
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({ passwd: passwd }),
      contentType: "application/json",
      processData: false
    });
  },

  setExtras: function(name, img) {
    $.ajax({
      cache: false,
      type: "PATCH",
      async: false, // sequential calls!
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({"extra": {"name":name, "img":img}}),
      contentType: "application/json",
      processData: false
    });
  }

});

/*global Backbone, window */

(function() {
  "use strict";

  window.CurrentDatabase = Backbone.Model.extend({
    url: "/_api/database/current",

    parse: function(data) {
      return data.result;
    }
  });
}());

/*jshint browser: true */
/*global Backbone, $, arango */
(function() {
  "use strict";

  var sendRequest = function (foxx, callback, type, part, body) {
    var req = {
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(data);
      },
      error: function(err) {
        callback(err);
      }
    };
    req.type = type;
    if (part && part !== "") {
      req.url = "/_admin/aardvark/foxxes/" + part + "?mount=" + foxx.encodedMount();
    } else {
      req.url = "/_admin/aardvark/foxxes?mount=" + foxx.encodedMount();
    }
    if (body !== undefined) {
      req.data = JSON.stringify(body);
    }
    $.ajax(req);
  };

  window.Foxx = Backbone.Model.extend({
    idAttribute: 'mount',

    defaults: {
      "author": "Unknown Author",
      "name": "",
      "version": "Unknown Version",
      "description": "No description",
      "license": "Unknown License",
      "contributors": [],
      "git": "",
      "system": false,
      "development": false
    },

    isNew: function() {
      return false;
    },

    encodedMount: function() {
      return encodeURIComponent(this.get("mount"));
    },

    destroy: function(callback) {
      sendRequest(this, callback, "DELETE");
    },

    getConfiguration: function(callback) {
      sendRequest(this, callback, "GET", "config");
    },

    setConfiguration: function(data, callback) {
      sendRequest(this, callback, "PATCH", "config", data);
    },

    toggleDevelopment: function(activate, callback) {
      $.ajax({
        type: "PATCH",
        url: "/_admin/aardvark/foxxes/devel?mount=" + this.encodedMount(),
        data: JSON.stringify(activate),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          this.set("development", activate);
          callback(data);
        }.bind(this),
        error: function(err) {
          callback(err);
        }
      });
    },

    setup: function(callback) {
      sendRequest(this, callback, "PATCH", "setup");
    },

    teardown: function(callback) {
      sendRequest(this, callback, "PATCH", "teardown");
    },

    isSystem: function() {
      return this.get("system");
    },

    isDevelopment: function() {
      return this.get("development");
    },

    download: function() {
      window.open(
        "/_db/" + arango.getDatabaseName() + "/_admin/aardvark/foxxes/download/zip?mount=" + this.encodedMount()
      );
    }

  });
}());

/*global window, Backbone, $ */
(function() {
  "use strict";

  window.Graph = Backbone.Model.extend({

    idAttribute: "_key",

    urlRoot: "/_api/gharial",

    isNew: function() {
      return !this.get("_id");
    },

    parse: function(raw) {
      return raw.graph || raw;
    },

    addEdgeDefinition: function(edgeDefinition) {
      $.ajax(
        {
          async: false,
          type: "POST",
          url: this.urlRoot + "/" + this.get("_key") + "/edge",
          data: JSON.stringify(edgeDefinition)
        }
      );
    },

    deleteEdgeDefinition: function(edgeCollection) {
      $.ajax(
        {
          async: false,
          type: "DELETE",
          url: this.urlRoot + "/" + this.get("_key") + "/edge/" + edgeCollection
        }
      );
    },

    modifyEdgeDefinition: function(edgeDefinition) {
      $.ajax(
        {
          async: false,
          type: "PUT",
          url: this.urlRoot + "/" + this.get("_key") + "/edge/" + edgeDefinition.collection,
          data: JSON.stringify(edgeDefinition)
        }
      );
    },

    addVertexCollection: function(vertexCollectionName) {
      $.ajax(
        {
          async: false,
          type: "POST",
          url: this.urlRoot + "/" + this.get("_key") + "/vertex",
          data: JSON.stringify({collection: vertexCollectionName})
        }
      );
    },

    deleteVertexCollection: function(vertexCollectionName) {
      $.ajax(
        {
          async: false,
          type: "DELETE",
          url: this.urlRoot + "/" + this.get("_key") + "/vertex/" + vertexCollectionName
        }
      );
    },


    defaults: {
      name: "",
      edgeDefinitions: [],
      orphanCollections: []
    }
  });
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.newArangoLog = Backbone.Model.extend({
    defaults: {
      lid: "",
      level: "",
      timestamp: "",
      text: "",
      totalAmount: ""
    },

    getLogStatus: function() {
      switch (this.get("level")) {
        case 1:
          return "Error";
        case 2:
          return "Warning";
        case 3:
          return  "Info";
        case 4:
          return "Debug";
        default:
          return "Unknown";
      }
    }
  });
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.Notification = Backbone.Model.extend({
    defaults: {
      "title"    : "",
      "date"     : 0,
      "content"  : "",
      "priority" : "",
      "tags"     : "",
      "seen"     : false
    }

  });
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.queryManagementModel = Backbone.Model.extend({

    defaults: {
      id: "",
      query: "",
      started: "",
      runTime: ""
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, window, $, _ */
(function () {

  "use strict";

  window.PaginatedCollection = Backbone.Collection.extend({
    page: 0,
    pagesize: 10,
    totalAmount: 0,

    getPage: function() {
      return this.page + 1;
    },

    setPage: function(counter) {
      if (counter >= this.getLastPageNumber()) {
        this.page = this.getLastPageNumber()-1;
        return;
      }
      if (counter < 1) {
        this.page = 0;
        return;
      }
      this.page = counter - 1;

    },

    getLastPageNumber: function() {
      return Math.max(Math.ceil(this.totalAmount / this.pagesize), 1);
    },

    getOffset: function() {
      return this.page * this.pagesize;
    },

    getPageSize: function() {
      return this.pagesize;
    },

    setPageSize: function(newPagesize) {
      if (newPagesize === "all") {
        this.pagesize = 'all';
      }
      else {
        try {
          newPagesize = parseInt(newPagesize, 10);
          this.pagesize = newPagesize;
        }
        catch (ignore) {
        }
      }
    },

    setToFirst: function() {
      this.page = 0;
    },

    setToLast: function() {
      this.setPage(this.getLastPageNumber());
    },

    setToPrev: function() {
      this.setPage(this.getPage() - 1);

    },

    setToNext: function() {
      this.setPage(this.getPage() + 1);
    },

    setTotal: function(total) {
      this.totalAmount = total;
    },

    getTotal: function() {
      return this.totalAmount;
    },

    setTotalMinusOne: function() {
      this.totalAmount--;
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, window, arangoCollectionModel, $, arangoHelper, data, _ */
(function() {
  "use strict";

  window.arangoCollections = Backbone.Collection.extend({
      url: '/_api/collection',

      model: arangoCollectionModel,

      searchOptions : {
        searchPhrase: null,
        includeSystem: false,
        includeDocument: true,
        includeEdge: true,
        includeLoaded: true,
        includeUnloaded: true,
        sortBy: 'name',
        sortOrder: 1
      },

      translateStatus : function (status) {
        switch (status) {
          case 0:
            return 'corrupted';
          case 1:
            return 'new born collection';
          case 2:
            return 'unloaded';
          case 3:
            return 'loaded';
          case 4:
            return 'in the process of being unloaded';
          case 5:
            return 'deleted';
          case 6:
            return 'loading';
          default:
            return;
        }
      },

      translateTypePicture : function (type) {
        var returnString = "img/icon_";
        switch (type) {
          case 'document':
            returnString += "document.png";
            break;
          case 'edge':
            returnString += "node.png";
            break;
          case 'unknown':
            returnString += "unknown.png";
            break;
          default:
            returnString += "arango.png";
        }
        return returnString;
      },

      parse: function(response)  {
        var self = this;
        _.each(response.collections, function(val) {
          val.isSystem = arangoHelper.isSystemCollection(val);
          val.type = arangoHelper.collectionType(val);
          val.status = self.translateStatus(val.status);
          val.picture = self.translateTypePicture(val.type);
        });
        return response.collections;
      },

      getPosition : function (name) {
        var list = this.getFiltered(this.searchOptions), i;
        var prev = null;
        var next = null;

        for (i = 0; i < list.length; ++i) {
          if (list[i].get('name') === name) {
            if (i > 0) {
              prev = list[i - 1];
            }
            if (i < list.length - 1) {
              next = list[i + 1];
            }
          }
        }

        return { prev: prev, next: next };
      },

      getFiltered : function (options) {
        var result = [ ];
        var searchPhrases = [ ];

        if (options.searchPhrase !== null) {
          var searchPhrase = options.searchPhrase.toLowerCase();
          // kick out whitespace
          searchPhrase = searchPhrase.replace(/\s+/g, ' ').replace(/(^\s+|\s+$)/g, '');
          searchPhrases = searchPhrase.split(' ');
        }

        this.models.forEach(function (model) {
          // search for name(s) entered
          if (searchPhrases.length > 0) {
            var lowerName = model.get('name').toLowerCase(), i;
            // all phrases must match!
            for (i = 0; i < searchPhrases.length; ++i) {
              if (lowerName.indexOf(searchPhrases[i]) === -1) {
                // search phrase entered but current collection does not match?
                return;
              }
            }
          }

          if (options.includeSystem === false && model.get('isSystem')) {
            // system collection?
            return;
          }
          if (options.includeEdge === false && model.get('type') === 'edge') {
            return;
          }
          if (options.includeDocument === false && model.get('type') === 'document') {
            return;
          }
          if (options.includeLoaded === false && model.get('status') === 'loaded') {
            return;
          }
          if (options.includeUnloaded === false && model.get('status') === 'unloaded') {
            return;
          }

          result.push(model);
        });

        result.sort(function (l, r) {
          var lValue, rValue;
          if (options.sortBy === 'type') {
            // we'll use first type, then name as the sort criteria
            // this is because when sorting by type, we need a 2nd criterion (type is not unique)
            lValue = l.get('type') + ' ' + l.get('name').toLowerCase();
            rValue = r.get('type') + ' ' + r.get('name').toLowerCase();
          }
          else {
            lValue = l.get('name').toLowerCase();
            rValue = r.get('name').toLowerCase();
          }
          if (lValue !== rValue) {
            return options.sortOrder * (lValue < rValue ? -1 : 1);
          }
          return 0;
        });

        return result;
      },

      newCollection: function (collName, wfs, isSystem, journalSize, collType, shards, keys) {
        var returnobj = {};
        var data = {};
        data.name = collName;
        data.waitForSync = wfs;
        if (journalSize > 0) {
          data.journalSize = journalSize;
        }
        data.isSystem = isSystem;
        data.type = parseInt(collType, 10);
        if (shards) {
          data.numberOfShards = shards;
          data.shardKeys = keys;
        }
        returnobj.status = false;
        $.ajax({
          cache: false,
          type: "POST",
          url: "/_api/collection",
          data: JSON.stringify(data),
          contentType: "application/json",
          processData: false,
          async: false,
          success: function(data) {
            returnobj.status = true;
            returnobj.data = data;
          },
          error: function(data) {
            returnobj.status = false;
            returnobj.errorMessage = JSON.parse(data.responseText).errorMessage;
          }
        });
        return returnobj;
      }
  });
}());

/*jshint browser: true */
/*global window, Backbone, $, window, _*/

(function() {
  'use strict';

  window.ArangoDatabase = Backbone.Collection.extend({

    model: window.DatabaseModel,

    sortOptions: {
      desc: false
    },

    url: "/_api/database",

    comparator: function(item, item2) {
      var a = item.get('name').toLowerCase();
      var b = item2.get('name').toLowerCase();
      if (this.sortOptions.desc === true) {
        return a < b ? 1 : a > b ? -1 : 0;
      }
      return a > b ? 1 : a < b ? -1 : 0;
    },

    parse: function(response) {
      if (!response) {
        return;
      }
      return _.map(response.result, function(v) {
        return {name:v};
      });
    },

    initialize: function() {
      var self = this;
      this.fetch().done(function() {
        self.sort();
      });
    },

    setSortingDesc: function(yesno) {
      this.sortOptions.desc = yesno;
    },

    getDatabases: function() {
      var self = this;
      this.fetch().done(function() {
        self.sort();
      });
      return this.models;
    },

    getDatabasesForUser: function() {
      var returnVal;
      $.ajax({
        type: "GET",
        cache: false,
        url: this.url + "/user",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          returnVal = data.result;
        },
        error: function() {
          returnVal = [];
        }
      });
      return returnVal.sort();
    },

    createDatabaseURL: function(name, protocol, port) {
      var loc = window.location;
      var hash = window.location.hash;
      if (protocol) {
        if (protocol === "SSL" || protocol === "https:") {
          protocol = "https:";
        } else {
          protocol = "http:";
        }
      } else {
        protocol = loc.protocol;
      }
      port = port || loc.port;

      var url = protocol
        + "//"
        + window.location.hostname
        + ":"
        + port
        + "/_db/"
        + encodeURIComponent(name)
        + "/_admin/aardvark/standalone.html";
      if (hash) {
        var base = hash.split("/")[0];
        if (base.indexOf("#collection") === 0) {
          base = "#collections";
        }
        if (base.indexOf("#application") === 0) {
          base = "#applications";
        }
        url += base;
      }
      return url;
    },

    getCurrentDatabase: function() {
      var returnVal;
      $.ajax({
        type: "GET",
        cache: false,
        url: this.url + "/current",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          if (data.code === 200) {
            returnVal = data.result.name;
            return;
          }
          returnVal = data;
        },
        error: function(data) {
          returnVal = data;
        }
      });
      return returnVal;
    },

    hasSystemAccess: function() {
      var list = this.getDatabasesForUser();
      return _.contains(list, "_system");
    }
  });
}());

/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global require, exports, Backbone, window, arangoDocument, arangoDocumentModel, $, arangoHelper */

window.arangoDocument = Backbone.Collection.extend({
  url: '/_api/document/',
  model: arangoDocumentModel,
  collectionInfo: {},
  deleteEdge: function (colid, docid) {
    var returnval = false;
    try {
      $.ajax({
        cache: false,
        type: 'DELETE',
        async: false,
        contentType: "application/json",
        url: "/_api/edge/" + colid + "/" + docid,
        success: function () {
          returnval = true;
        },
        error: function () {
          returnval = false;
        }
      });
    }
    catch (e) {
          returnval = false;
    }
    return returnval;
  },
  deleteDocument: function (colid, docid){
    var returnval = false;
    try {
      $.ajax({
        cache: false,
        type: 'DELETE',
        async: false,
        contentType: "application/json",
        url: "/_api/document/" + colid + "/" + docid,
        success: function () {
          returnval = true;
        },
        error: function () {
          returnval = false;
        }
      });
    }
    catch (e) {
          returnval = false;
    }
    return returnval;
  },
  addDocument: function (collectionID, key) {
    var self = this;
    self.createTypeDocument(collectionID, key);
  },
  createTypeEdge: function (collectionID, from, to, key) {
    var result = false, newEdge;

    if (key) {
      newEdge = JSON.stringify({
        _key: key
      });
    }
    else {
      newEdge = JSON.stringify({});
    }

    $.ajax({
      cache: false,
      type: "POST",
      async: false,
      url: "/_api/edge?collection=" + collectionID + "&from=" + from + "&to=" + to,
      data: newEdge,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        result = data._id;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  createTypeDocument: function (collectionID, key) {
    var result = false, newDocument;

    if (key) {
      newDocument = JSON.stringify({
        _key: key
      });
    }
    else {
      newDocument = JSON.stringify({});
    }

    $.ajax({
      cache: false,
      type: "POST",
      async: false,
      url: "/_api/document?collection=" + encodeURIComponent(collectionID),
      data: newDocument,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        result = data._id;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  getCollectionInfo: function (identifier) {
    var self = this;

    $.ajax({
      cache: false,
      type: "GET",
      url: "/_api/collection/" + identifier + "?" + arangoHelper.getRandomToken(),
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        self.collectionInfo = data;
      },
      error: function(data) {
      }
    });

    return self.collectionInfo;
  },
  getEdge: function (colid, docid){
    var result = false, self = this;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: "GET",
      async: false,
      url: "/_api/edge/" + colid +"/"+ docid,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.add(data);
        result = true;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  getDocument: function (colid, docid) {
    var result = false, self = this;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: "GET",
      async: false,
      url: "/_api/document/" + colid +"/"+ docid,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.add(data);
        result = true;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  saveEdge: function (colid, docid, model) {
    var result = false;
    $.ajax({
      cache: false,
      type: "PUT",
      async: false,
      url: "/_api/edge/" + colid + "/" + docid,
      data: model,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        result = true;
      },
      error: function(data) {
        result = false;
      }
    });
    return result;
  },
  saveDocument: function (colid, docid, model) {
    var result = false;
    $.ajax({
      cache: false,
      type: "PUT",
      async: false,
      url: "/_api/document/" + colid + "/" + docid,
      data: model,
      contentType: "application/json",
      processData: false,
      success: function(data) {
          result = true;
      },
      error: function(data) {
          result = false;
      }
    });
    return result;

  },

  updateLocalDocument: function (data) {
    this.clearDocument();
    this.add(data);
  },
  clearDocument: function () {
    this.reset();
  }

});

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, window, Backbone, arangoDocumentModel, _, arangoHelper, $*/
(function() {
  "use strict";

  window.arangoDocuments = window.PaginatedCollection.extend({
    collectionID: 1,

    filters: [],

    MAX_SORT: 12000,

    lastQuery: {},

    sortAttribute: "_key",

    url: '/_api/documents',
    model: window.arangoDocumentModel,

    loadTotal: function() {
      var self = this;
      $.ajax({
        cache: false,
        type: "GET",
        url: "/_api/collection/" + this.collectionID + "/count",
        contentType: "application/json",
        processData: false,
        async: false,
        success: function(data) {
          self.setTotal(data.count);
        }
      });
    },

    setCollection: function(id) {
      this.resetFilter();
      this.collectionID = id;
      this.setPage(1);
      this.loadTotal();
    },

    setSort: function(key) {
      this.sortAttribute = key;
    },

    getSort: function() {
      return this.sortAttribute;
    },

    addFilter: function(attr, op, val) {
      this.filters.push({
        attr: attr,
        op: op,
        val: val
      });
    },

    setFiltersForQuery: function(bindVars) {
      if (this.filters.length === 0) {
        return "";
      }
      var query = " FILTER",
      parts = _.map(this.filters, function(f, i) {
        var res = " x.`";
        res += f.attr;
        res += "` ";
        res += f.op;
        res += " @param";
        res += i;
        bindVars["param" + i] = f.val;
        return res;
      });
      return query + parts.join(" &&");
    },

    setPagesize: function(size) {
      this.setPageSize(size);
    },

    resetFilter: function() {
      this.filters = [];
    },

    moveDocument: function (key, fromCollection, toCollection, callback) {
      var querySave, queryRemove, queryObj, bindVars = {
        "@collection": fromCollection,
        "filterid": key
      }, queryObj1, queryObj2;

      querySave = "FOR x IN @@collection";
      querySave += " FILTER x._key == @filterid";
      querySave += " INSERT x IN ";
      querySave += toCollection;

      queryRemove = "FOR x in @@collection";
      queryRemove += " FILTER x._key == @filterid";
      queryRemove += " REMOVE x IN @@collection";

      queryObj1 = {
        query: querySave,
        bindVars: bindVars
      };

      queryObj2 = {
        query: queryRemove,
        bindVars: bindVars
      };

      window.progressView.show();
      // first insert docs in toCollection
      $.ajax({
        cache: false,
        type: 'POST',
        async: true,
        url: '/_api/cursor',
        data: JSON.stringify(queryObj1),
        contentType: "application/json",
        success: function(data) {
          // if successful remove unwanted docs
          $.ajax({
            cache: false,
            type: 'POST',
            async: true,
            url: '/_api/cursor',
            data: JSON.stringify(queryObj2),
            contentType: "application/json",
            success: function(data) {
              if (callback) {
                callback();
              }
              window.progressView.hide();
            },
            error: function(data) {
              window.progressView.hide();
              arangoHelper.arangoNotification(
                "Document error", "Documents inserted, but could not be removed."
              );
            }
          });
        },
        error: function(data) {
          window.progressView.hide();
          arangoHelper.arangoNotification("Document error", "Could not move selected documents.");
        }
      });
    },

    getDocuments: function (callback) {
      window.progressView.showWithDelay(300, "Fetching documents...");
      var self = this,
          query,
          bindVars,
          tmp,
          queryObj;
      bindVars = {
        "@collection": this.collectionID,
        "offset": this.getOffset(),
        "count": this.getPageSize()
      };

      // fetch just the first 25 attributes of the document
      // this number is arbitrary, but may reduce HTTP traffic a bit
      query = "FOR x IN @@collection LET att = SLICE(ATTRIBUTES(x), 0, 25)";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT) {
        if (this.getSort() === '_key') {
          query += " SORT TO_NUMBER(x." + this.getSort() + ") == 0 ? x."
                + this.getSort() + " : TO_NUMBER(x." + this.getSort() + ")";
        }
        else {
          query += " SORT x." + this.getSort();
        }
      }

      if (bindVars.count !== 'all') {
        query += " LIMIT @offset, @count RETURN KEEP(x, att)";
      }
      else {
        tmp = {
          "@collection": this.collectionID
        };
        bindVars = tmp;
        query += " RETURN KEEP(x, att)";
      }

      queryObj = {
        query: query,
        bindVars: bindVars
      };
      if (this.getTotal() < 10000 || this.filters.length > 0) {
        queryObj.options = {
          fullCount: true,
        };
      }

      $.ajax({
        cache: false,
        type: 'POST',
        async: true,
        url: '/_api/cursor',
        data: JSON.stringify(queryObj),
        contentType: "application/json",
        success: function(data) {
          window.progressView.toShow = false;
          self.clearDocuments();
          if (data.extra && data.extra.stats.fullCount !== undefined) {
            self.setTotal(data.extra.stats.fullCount);
          }
          if (self.getTotal() !== 0) {
            _.each(data.result, function(v) {
              self.add({
                "id": v._id,
                "rev": v._rev,
                "key": v._key,
                "content": v
              });
            });
          }
          self.lastQuery = queryObj;
          callback();
          window.progressView.hide();
        },
        error: function(data) {
          window.progressView.hide();
          arangoHelper.arangoNotification("Document error", "Could not fetch requested documents.");
        }
      });
    },

    clearDocuments: function () {
      this.reset();
    },

    buildDownloadDocumentQuery: function() {
      var self = this, query, queryObj, bindVars;

      bindVars = {
        "@collection": this.collectionID
      };

      query = "FOR x in @@collection";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT) {
        query += " SORT x." + this.getSort();
      }

      query += " RETURN x";

      queryObj = {
        query: query,
        bindVars: bindVars
      };

      return queryObj;
    },

    uploadDocuments : function (file) {
      var result;
      $.ajax({
        type: "POST",
        async: false,
        url:
        '/_api/import?type=auto&collection='+
        encodeURIComponent(this.collectionID)+
        '&createCollection=false',
        data: file,
        processData: false,
        contentType: 'json',
        dataType: 'json',
        complete: function(xhr) {
          if (xhr.readyState === 4 && xhr.status === 201) {
            result = true;
          } else {
            result = "Upload error";
          }

          try {
            var data = JSON.parse(xhr.responseText);
            if (data.errors > 0) {
              result = "At least one error occurred during upload";
            }
          }
          catch (err) {
          }               
        }
      });
      return result;
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global window, $, _ */
(function () {

  "use strict";

  window.ArangoLogs = window.PaginatedCollection.extend({
    upto: false,
    loglevel: 0,
    totalPages: 0,

    parse: function(response) {
      var myResponse = [];
      _.each(response.lid, function(val, i) {
        myResponse.push({
          level: response.level[i],
          lid: val,
          text: response.text[i],
          timestamp: response.timestamp[i],
          totalAmount: response.totalAmount
        });
      });
      this.totalAmount = response.totalAmount;
      this.totalPages = Math.ceil(this.totalAmount / this.pagesize);
      return myResponse;
    },

    initialize: function(options) {
      if (options.upto === true) {
        this.upto = true;
      }
      this.loglevel = options.loglevel;
    },

    model: window.newArangoLog,

    url: function() {
      var type, rtnStr, offset, size;
      offset = this.page * this.pagesize;
      var inverseOffset = this.totalAmount - ((this.page + 1) * this.pagesize);
      if (inverseOffset < 0 && this.page === (this.totalPages - 1)) {
        inverseOffset = 0;
        size = (this.totalAmount % this.pagesize);
      }
      else {
        size = this.pagesize;
      }

      //if totalAmount (first fetch) = 0, then set size to 1 (reduce traffic)
      if (this.totalAmount === 0) {
        size = 1;
      }

      if (this.upto) {
        type = 'upto';
      }
      else  {
        type = 'level';
      }
      rtnStr = '/_admin/log?'+type+'='+this.loglevel+'&size='+size+'&offset='+inverseOffset;
      return rtnStr;
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, activeUser, window, ArangoQuery, $, data, _, arangoHelper*/
(function() {
  "use strict";

  window.ArangoQueries = Backbone.Collection.extend({

    initialize: function(models, options) {
      var result;
      $.ajax("whoAmI", {async:false}).done(
        function(data) {
        result = data.name;
      }
      );
      this.activeUser = result;

      if (this.activeUser === 0 || this.activeUser === undefined || this.activeUser === null) {
        this.activeUser = "root";
      }

    },

    url: '/_api/user/',

    model: ArangoQuery,

    activeUser: 0,

    parse: function(response) {
      var self = this, toReturn;

      _.each(response.result, function(val) {
        if (val.user === self.activeUser) {
          try {
            if (val.extra.queries) {
              toReturn = val.extra.queries;
            }
          }
          catch (e) {
          }
        }
      });
      return toReturn;
    },

    saveCollectionQueries: function() {
      if (this.activeUser === 0) {
        return false;
      }

      var returnValue = false;
      var queries = [];

      this.each(function(query) {
        queries.push({
          value: query.attributes.value,
          name: query.attributes.name
        });
      });

      // save current collection
      $.ajax({
        cache: false,
        type: "PATCH",
        async: false,
        url: "/_api/user/" + encodeURIComponent(this.activeUser),
        data: JSON.stringify({
          extra: {
           queries: queries
          }
        }),
        contentType: "application/json",
        processData: false,
        success: function() {
          returnValue = true;
        },
        error: function() {
          returnValue = false;
        }
      });

      return returnValue;
    },

    saveImportQueries: function(file, callback) {

      if (this.activeUser === 0) {
        return false;
      }

      window.progressView.show("Fetching documents...");
      $.ajax({
        cache: false,
        type: "POST",
        async: false,
        url: "query/upload/" + encodeURIComponent(this.activeUser),
        data: file,
        contentType: "application/json",
        processData: false,
        success: function() {
          window.progressView.hide();
          callback();
        },
        error: function() {
          window.progressView.hide();
          arangoHelper.arangoError("Query error", "queries could not be imported");
        }
      });
    }

  });
}());

/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global window, Backbone, $, window */

window.ArangoReplication = Backbone.Collection.extend({
  model: window.Replication,

  url: "../api/user",

  getLogState: function () {
    var returnVal;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/replication/logger-state",
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        returnVal = data;
      },
      error: function(data) {
        returnVal = data;
      }
    });
    return returnVal;
  },
  getApplyState: function () {
    var returnVal;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/replication/applier-state",
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        returnVal = data;
      },
      error: function(data) {
        returnVal = data;
      }
    });
    return returnVal;
  }

});

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, window */
window.StatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,
  url: "/_admin/statistics"
});

/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global require, exports, Backbone, window */
window.StatisticsDescriptionCollection = Backbone.Collection.extend({
  model: window.StatisticsDescription,
  url: "/_admin/statistics-description",
  parse: function(response) {
    return response;
  }
});

/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global window, Backbone, $,_, window */

window.ArangoUsers = Backbone.Collection.extend({
  model: window.Users,

  activeUser: "",
  activeUserSettings: {
    "query" : {},
    "shell" : {},
    "testing": true
  },

  sortOptions: {
    desc: false
  },

  url: "/_api/user",

  //comparator : function(obj) {
  //  return obj.get("user").toLowerCase();
  //},

  comparator: function(item, item2) {
    var a = item.get('user').toLowerCase();
    var b = item2.get('user').toLowerCase();
    if (this.sortOptions.desc === true) {
      return a < b ? 1 : a > b ? -1 : 0;
    }
    return a > b ? 1 : a < b ? -1 : 0;
  },

  login: function (username, password) {
    this.activeUser = username;
    return true;
  },

  setSortingDesc: function(yesno) {
    this.sortOptions.desc = yesno;
  },

  logout: function () {
    this.activeUser = undefined;
    this.reset();
    $.ajax("unauthorized", {async:false}).error(
      function () {
        window.App.navigate("");
        window.location.reload();
      }
    );
  },

  setUserSettings: function (identifier, content) {
    this.activeUserSettings.identifier = content;
  },

  loadUserSettings: function () {
    var self = this;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/user/" + encodeURIComponent(self.activeUser),
      contentType: "application/json",
      processData: false,
      async: false,
      success: function(data) {
        self.activeUserSettings = data.extra;
      },
      error: function(data) {
      }
    });
  },

  saveUserSettings: function () {
    var self = this;
    $.ajax({
      cache: false,
      type: "PUT",
      async: false, // sequential calls!
      url: "/_api/user/" + encodeURIComponent(self.activeUser),
      data: JSON.stringify({ extra: self.activeUserSettings }),
      contentType: "application/json",
      processData: false,
      success: function(data) {
      },
      error: function(data) {
      }
    });
  },

  parse: function(response)  {
    var result = [];
    _.each(response.result, function(object) {
      result.push(object);
    });
    return result;
  },

  whoAmI: function() {
    if (this.activeUser) {
      return this.activeUser;
    }
    var result;
    $.ajax("whoAmI", {async:false}).done(
      function(data) {
        result = data.name;
      }
    );
    this.activeUser = result;
    return this.activeUser;
  }


});

/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, alert, $ */
(function() {
  "use strict";
  window.FoxxCollection = Backbone.Collection.extend({
    model: window.Foxx,

    sortOptions: {
      desc: false
    },

    url: "/_admin/aardvark/foxxes",

    comparator: function(item, item2) {
      var a, b;
      if (this.sortOptions.desc === true) {
        a = item.get('mount');
        b = item2.get('mount');
        return a < b ? 1 : a > b ? -1 : 0;
      }
      a = item.get('mount');
      b = item2.get('mount');
      return a > b ? 1 : a < b ? -1 : 0;
    },

    setSortingDesc: function(val) {
      this.sortOptions.desc = val;
    },

    // Install Foxx from github repo
    // info is expected to contain: "url" and "version"
    installFromGithub: function (info, mount, callback, flag) {
      var url = "/_admin/aardvark/foxxes/git?mount=" + encodeURIComponent(mount);
      if (flag !== undefined) {
        if (flag) {
          url += "&replace=true";
        } else {
          url += "&upgrade=true";
        }
      }
      $.ajax({
        cache: false,
        type: "PUT",
        url: url,
        data: JSON.stringify(info),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(data);
        },
        error: function(err) {
          callback(err);
        }
      });
    },

    // Install Foxx from arango store
    // info is expected to contain: "name" and "version"
    installFromStore: function (info, mount, callback, flag) {
      var url = "/_admin/aardvark/foxxes/store?mount=" + encodeURIComponent(mount);
      if (flag !== undefined) {
        if (flag) {
          url += "&replace=true";
        } else {
          url += "&upgrade=true";
        }
      }
      $.ajax({
        cache: false,
        type: "PUT",
        url: url,
        data: JSON.stringify(info),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(data);
        },
        error: function(err) {
          callback(err);
        }
      });
    },

    installFromZip: function(fileName, mount, callback, flag) {
      var url = "/_admin/aardvark/foxxes/zip?mount=" + encodeURIComponent(mount);
      if (flag !== undefined) {
        if (flag) {
          url += "&replace=true";
        } else {
          url += "&upgrade=true";
        }
      }
      $.ajax({
        cache: false,
        type: "PUT",
        url: url,
        data: JSON.stringify({zipFile: fileName}),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(data);
        },
        error: function(err) {
          callback(err);
        }
      });
    },

    generate: function (info, mount, callback, flag) {
      var url = "/_admin/aardvark/foxxes/generate?mount=" + encodeURIComponent(mount);
      if (flag !== undefined) {
        if (flag) {
          url += "&replace=true";
        } else {
          url += "&upgrade=true";
        }
      }
      $.ajax({
        cache: false,
        type: "PUT",
        url: url,
        data: JSON.stringify(info),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(data);
        },
        error: function(err) {
          callback(err);
        }
      });
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone */
(function () {
  "use strict";

  window.GraphCollection = Backbone.Collection.extend({
    model: window.Graph,

    sortOptions: {
      desc: false
    },

    url: "/_api/gharial",

    comparator: function(item, item2) {
      var a = item.get('_key') || "";
      var b = item2.get('_key') || "";
      a = a.toLowerCase();
      b = b.toLowerCase();
      if (this.sortOptions.desc === true) {
        return a < b ? 1 : a > b ? -1 : 0;
      }
      return a > b ? 1 : a < b ? -1 : 0;
    },

    setSortingDesc: function(val) {
      this.sortOptions.desc = val;
    },

    parse: function(res) {
      if (!res.error) {
        return res.graphs;
      }
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone */
(function() {
  "use strict";
  window.NotificationCollection = Backbone.Collection.extend({
    model: window.Notification,
    url: ""
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, $ */
(function() {
  "use strict";
  window.QueryManagementActive = Backbone.Collection.extend({

    model: window.queryManagementModel,

    url: function() {
      return '/_api/query/current';
    },

    killRunningQuery: function(id, callback) {
      var self = this;
      $.ajax({
        url: '/_api/query/'+encodeURIComponent(id),
        type: 'DELETE',
        success: function(result) {
          callback();
        }
      });
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, $ */
(function() {
  "use strict";
  window.QueryManagementSlow = Backbone.Collection.extend({
    model: window.queryManagementModel,
    url: "/_api/query/slow",

    deleteSlowQueryHistory: function(callback) {
      var self = this;
      $.ajax({
        url: self.url,
        type: 'DELETE',
        success: function(result) {
          callback();
        }
      });
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, $, window, arangoHelper, templateEngine */

(function() {
    "use strict";
    window.PaginationView = Backbone.View.extend({

        // Subclasses need to overwrite this
        collection : null,
        paginationDiv : "",
        idPrefix : "",


        rerender : function () {},

        jumpTo: function(page) {
            this.collection.setPage(page);
            this.rerender();
        },

        firstPage: function() {
            this.jumpTo(1);
        },

        lastPage: function() {
            this.jumpTo(this.collection.getLastPageNumber());
        },

        firstDocuments: function () {
            this.jumpTo(1);
        },
        lastDocuments: function () {
            this.jumpTo(this.collection.getLastPageNumber());
        },
        prevDocuments: function () {
            this.jumpTo(this.collection.getPage() - 1);
        },
        nextDocuments: function () {
            this.jumpTo(this.collection.getPage() + 1);
        },

        renderPagination: function () {
            $(this.paginationDiv).html("");
            var self = this;
            var currentPage = this.collection.getPage();
            var totalPages = this.collection.getLastPageNumber();
            var target = $(this.paginationDiv),
                options = {
                    page: currentPage,
                    lastPage: totalPages,
                    click: function(i) {
                        self.jumpTo(i);
                        options.page = i;
                    }
                };
            target.html("");
            target.pagination(options);
            $(this.paginationDiv).prepend(
                '<ul class="pre-pagi"><li><a id="' + this.idPrefix +
                    '_first" class="pagination-button">'+
                    '<span><i class="fa fa-angle-double-left"/></span></a></li></ul>'
            );
            $(this.paginationDiv).append(
                '<ul class="las-pagi"><li><a id="' + this.idPrefix +
                    '_last" class="pagination-button">'+
                    '<span><i class="fa fa-angle-double-right"/></span></a></li></ul>'

            );
        }

    });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global window, document, Backbone, SwaggerUi, hljs, templateEngine, $ */
(function() {
  "use strict";
  window.ApiView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("apiView.ejs"),

    initialize: function() {
      this.swaggerUi = new SwaggerUi({
          discoveryUrl: "api-docs.json",
          apiKey: false,
          dom_id:"swagger-ui-container",
          supportHeaderParams: true,
          supportedSubmitMethods: ['get', 'post', 'put', 'delete', 'patch', 'head'],
          onComplete: function(){
            $('pre code').each(function(i, e) {
              hljs.highlightBlock(e);
            });
          },
          onFailure: function(data) {
            var div = document.createElement("div"),
              strong = document.createElement("strong");
            strong.appendChild(
              document.createTextNode("Sorry the code is not documented properly")
            );
            div.appendChild(strong);
            div.appendChild(document.createElement("br"));
            div.appendChild(document.createTextNode(JSON.stringify(data)));
            $("#swagger-ui-container").append(div);
          },
          docExpansion: "none"
      });
    },

    render: function(){
      $(this.el).html(this.template.render({}));
      this.swaggerUi.load();
      return this;
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, window, SwaggerUi, hljs, document, $, arango */
/*global templateEngine*/

(function() {
  "use strict";

  window.AppDocumentationView = Backbone.View.extend({

    el: 'body',
    template: templateEngine.createTemplate("appDocumentationView.ejs"),

    initialize: function() {
      var internal = require("internal");
      // var url = "foxxes/docu?mount=" + this.options.mount;
      var url = internal.arango.databasePrefix("/_admin/aardvark/foxxes/billboard?mount=" + this.options.mount);
      this.swaggerUi = new SwaggerUi({
          discoveryUrl: url,
          apiKey: false,
          dom_id: "swagger-ui-container",
          supportHeaderParams: true,
          supportedSubmitMethods: ['get', 'post', 'put', 'delete', 'patch', 'head'],
          onComplete: function(swaggerApi, swaggerUi){
            $('pre code').each(function(i, e) {hljs.highlightBlock(e);});
          },
          onFailure: function(data) {
            var div = document.createElement("div"),
              strong = document.createElement("strong");
            strong.appendChild(
              document.createTextNode("Sorry the code is not documented properly")
            );
            div.appendChild(strong);
            div.appendChild(document.createElement("br"));
            div.appendChild(document.createTextNode(JSON.stringify(data)));
            $("#swagger-ui-container").append(div);
          },
          docExpansion: "list"
      });
    },

    render: function(){
      $(this.el).html(this.template.render({}));
      this.swaggerUi.load();
      return this;
    }
  });
}());

/*jshint browser: true */
/*global Backbone, $, window, arangoHelper, templateEngine, Joi, _*/
(function() {
  "use strict";

  window.ApplicationDetailView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate('applicationDetailView.ejs'),

    events: {
      'click .open': 'openApp',
      'click .delete': 'deleteApp',
      'click #configure-app': 'showConfigureDialog',
      'click #app-switch-mode': 'toggleDevelopment',
      'click #app-setup': 'setup',
      'click #app-teardown': 'teardown',
      "click #app-scripts": "toggleScripts",
      "click #app-upgrade": "upgradeApp",
      "click #download-app": "downloadApp"
    },

    downloadApp: function() {
      if (!this.model.isSystem()) {
        this.model.download();
      }
    },

    upgradeApp: function() {
      var mount = this.model.get("mount");
      window.foxxInstallView.upgrade(mount, function() {
        window.App.applicationDetail(encodeURIComponent(mount));
      });
    },

    toggleScripts: function() {
      $("#scripts_dropdown").toggle(200);
    },

    updateConfig: function() {
      this.model.getConfiguration(function(cfg) {
        this._appConfig = cfg;
        if (Object.keys(this._appConfig).length === 0) {
          $("#configure-app").addClass("disabled");
        } else {
          $("#configure-app").removeClass("disabled");
        }
      }.bind(this));
    },

    toggleDevelopment: function() {
      this.model.toggleDevelopment(!this.model.isDevelopment(), function() {
        if (this.model.isDevelopment()) {
          $("#app-switch-mode").val("Set Pro");
          $("#app-development-indicator").css("display", "inline");
          $("#app-development-path").css("display", "inline");
        } else {
          $("#app-switch-mode").val("Set Dev");
          $("#app-development-indicator").css("display", "none");
          $("#app-development-path").css("display", "none");
        }
      }.bind(this));
    },

    setup: function() {
      this.model.setup(function() {

      });
    },

    teardown: function() {
      this.model.teardown(function() {

      });
    },

    render: function() {
      $(this.el).html(this.template.render({
        app: this.model,
        db: arangoHelper.currentDatabase()
      }));

      $.get(this.appUrl()).success(function () {
        $('.open', this.el).prop('disabled', false);
      }.bind(this));

      this.updateConfig();

      return $(this.el);
    },

    openApp: function() {
      window.open(this.appUrl(), this.model.get('title')).focus();
    },

    deleteApp: function() {
      var buttons = [];
      buttons.push(
        window.modalView.createDeleteButton("Delete", function() {
          this.model.destroy(function (result) {
            if (result.error === false) {
              window.modalView.hide();
              window.App.navigate("applications", { trigger: true });
            }
          });
        }.bind(this))
      );
      window.modalView.show(
        "modalDeleteConfirmation.ejs",
        "Delete Foxx App mounted at '" + this.model.get('mount') + "'",
        buttons,
        undefined,
        undefined,
        undefined,
        true
      );
    },

    appUrl: function () {
      return window.location.origin + '/_db/' +
        encodeURIComponent(arangoHelper.currentDatabase()) +
        this.model.get('mount');
    },

    readConfig: function() {
      var cfg = {};
      _.each(this._appConfig, function(opt, key) {
        var $el = $("#app_config_" + key);
        var val = window.arangoHelper.escapeHtml($el.val());
        if (opt.type === "boolean") {
          cfg[key] = $el.is(":checked");
          return;
        }
        if (val === "" && opt.hasOwnProperty("default")) {
          cfg[key] = opt.default;
          return;
        } 
        if (opt.type === "number") {
          cfg[key] = parseFloat(val);
        } else if (opt.type === "integer") {
          cfg[key] = parseInt(val, 10);
        } else {
          cfg[key] = val;
          return;
        }
      });
      this.model.setConfiguration(cfg, function() {
        window.modalView.hide();
        this.updateConfig();
      }.bind(this));
    },

    showConfigureDialog: function(e) {
      if (Object.keys(this._appConfig).length === 0) {
        e.stopPropagation();
        return;
      }
      var buttons = [],
          tableContent = [],
          entry;
      _.each(this._appConfig, function(obj, name) {
        var def;
        var current;
        var check;
        switch (obj.type) {
          case "boolean":
          case "bool":
            def = obj.current || false;
            entry = window.modalView.createCheckboxEntry(
              "app_config_" + name,
              name,
              def,
              obj.description,
              def
            );
            break;
          case "integer":
            check = [{
              rule: Joi.number().integer().optional().allow(""),
              msg: "Has to be an integer."
            }];
            /* falls through */
          case "number":
            if (check === undefined) {
              check = [{
                rule: Joi.number().optional().allow(""),
                msg: "Has to be a number."
              }];
            }
            /* falls through */
          default:
            if (check === undefined) {
              check = [{
                rule: Joi.string().optional().allow(""),
                msg: "Has to be a string."
              }];
            }
            if (obj.hasOwnProperty("default")) {
              def = String(obj.default);
            } else {
              def = "";
              check.push({
                rule: Joi.string().required(),
                msg: "No default found. Has to be added"
              });
            }
            if (obj.hasOwnProperty("current")) {
              current = String(obj.current);
            } else {
              current = "";
            }
            entry = window.modalView.createTextEntry(
              "app_config_" + name,
              name,
              current,
              obj.description,
              def,
              true,
              check
            );
        }
        tableContent.push(entry);
      });
      buttons.push(
        window.modalView.createSuccessButton("Configure", this.readConfig.bind(this))
      );
      window.modalView.show(
        "modalTable.ejs", "Configure application", buttons, tableContent
      );

    }

  });
}());

/*jshint browser: true */
/*global Backbone, $, window, arangoHelper, templateEngine, _*/
(function() {
  "use strict";

  window.ApplicationsView = Backbone.View.extend({
    el: '#content',

    template: templateEngine.createTemplate("applicationsView.ejs"),

    events: {
      "click #addApp"                : "createInstallModal",
      "click #foxxToggle"            : "slideToggle",
      "click #checkDevel"            : "toggleDevel",
      "click #checkProduction"       : "toggleProduction",
      "click #checkSystem"           : "toggleSystem",
      "click .css-label"             : "checkBoxes"
    },


    checkBoxes: function (e) {
      //chrome bugfix

      var isChromium = window.chrome,
      vendorName = window.navigator.vendor,
      clicked = e.currentTarget;

      if (clicked.id === '' || clicked.id === undefined) {
        if(isChromium !== null && isChromium !== undefined && vendorName === "Google Inc.") {
          $(clicked).prev().click();
        }
      }
      else {
        $('#'+clicked.id).click();
      }
    },

    toggleDevel: function() {
      var self = this;
      this._showDevel = !this._showDevel;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
      });
    },

    toggleProduction: function() {
      var self = this;
      this._showProd = !this._showProd;
      _.each(this._installedSubViews, function(v) {
        v.toggle("production", self._showProd);
      });
    },

    toggleSystem: function() {
      this._showSystem = !this._showSystem;
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("system", self._showSystem);
      });
    },

    reload: function() {
      var self = this;

      // unbind and remove any unused views
      _.each(this._installedSubViews, function (v) {
        v.undelegateEvents();
      });

      this.collection.fetch({
        success: function() {
          self.createSubViews();
          self.render();
        }
      });
    },

    createSubViews: function() {
      var self = this;
      this._installedSubViews = { };

      self.collection.each(function (foxx) {
        var subView = new window.FoxxActiveView({
          model: foxx,
          appsView: self
        });
        self._installedSubViews[foxx.get('mount')] = subView;
      });
    },

    initialize: function() {
      this._installedSubViews = {};
      this._showDevel = true;
      this._showProd = true;
      this._showSystem = false;
      this.reload();
    },

    slideToggle: function() {
      $('#foxxToggle').toggleClass('activated');
      $('#foxxDropdownOut').slideToggle(200);
    },

    createInstallModal: function(event) {
      event.preventDefault();
      window.foxxInstallView.install(this.reload.bind(this));
    },

    render: function() {
      this.collection.sort();

      $(this.el).html(this.template.render({}));
      _.each(this._installedSubViews, function (v) {
        $("#installedList").append(v.render());
      });
      this.delegateEvents();
      $('#checkDevel').attr('checked', this._showDevel);
      $('#checkProduction').attr('checked', this._showProd);
      $('#checkSystem').attr('checked', this._showSystem);
      
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
        v.toggle("system", self._showSystem);
      });

      arangoHelper.fixTooltips("icon_arangodb", "left");
      return this;
    }

  });

  /* Info for mountpoint
   *
   *
      window.modalView.createTextEntry(
        "mount-point",
        "Mount",
        "",
        "The path the app will be mounted. Has to start with /. Is not allowed to start with /_",
        "/my/app",
        true,
        [
          {
            rule: Joi.string().required(),
            msg: "No mountpoint given."
          },
          {
            rule: Joi.string().regex(/^\/[^_]/),
            msg: "Mountpoints with _ are reserved for internal use."
          },
          {
            rule: Joi.string().regex(/^(\/[a-zA-Z0-9_\-%]+)+$/),
            msg: "Mountpoints have to start with / and can only contain [a-zA-Z0-9_-%]"
          }
        ]
      )
   */
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, window, exports, Backbone, EJS, $, templateEngine, arangoHelper, Joi*/

(function() {
  "use strict";

  window.CollectionListItemView = Backbone.View.extend({

    tagName: "div",
    className: "tile",
    template: templateEngine.createTemplate("collectionsItemView.ejs"),

    initialize: function () {
      this.collectionsView = this.options.collectionsView;
    },
    events: {
      'click .iconSet.icon_arangodb_settings2': 'createEditPropertiesModal',
      'click .pull-left' : 'noop',
      'click .icon_arangodb_settings2' : 'editProperties',
//      'click #editCollection' : 'editProperties',
      'click .spanInfo' : 'showProperties',
      'click': 'selectCollection'
    },

    render: function () {
      $(this.el).html(this.template.render({
        model: this.model
      }));
      $(this.el).attr('id', 'collection_' + this.model.get('name'));
      return this;
    },

    editProperties: function (event) {
      event.stopPropagation();
      this.createEditPropertiesModal();
    },

    showProperties: function(event) {
      event.stopPropagation();
      this.createInfoModal();
/*
      window.App.navigate(
        "collectionInfo/" + encodeURIComponent(this.model.get("id")), {trigger: true}
      );
*/
    },

    selectCollection: function(event) {

      //check if event was fired from disabled button
      if ($(event.target).hasClass("disabled")) {
        return 0;
      }

      window.App.navigate(
        "collection/" + encodeURIComponent(this.model.get("name")) + "/documents/1", {trigger: true}
      );
    },

    noop: function(event) {
      event.stopPropagation();
    },

    unloadCollection: function () {
      this.model.unloadCollection();
      this.render();
      window.modalView.hide();
    },

    loadCollection: function () {
      this.model.loadCollection();
      this.render();
      window.modalView.hide();
    },

    truncateCollection: function () {
      this.model.truncateCollection();
      this.render();
      window.modalView.hide();
    },

    deleteCollection: function () {
      this.model.destroy(
        {
          error: function() {
            arangoHelper.arangoError('Could not delete collection.');
          },
          success: function() {
            window.modalView.hide();
          }
        }
      );
      this.collectionsView.render();
    },

    saveModifiedCollection: function() {
      var newname;
      if (window.isCoordinator()) {
        newname = this.model.get('name');
      }
      else {
        newname = $('#change-collection-name').val();
      }

      var status = this.model.get('status');

      if (status === 'loaded') {
        var result;
        if (this.model.get('name') !== newname) {
          result = this.model.renameCollection(newname);
        }

        var wfs = $('#change-collection-sync').val();
        var journalSize;
        try {
          journalSize = JSON.parse($('#change-collection-size').val() * 1024 * 1024);
        }
        catch (e) {
          arangoHelper.arangoError('Please enter a valid number');
          return 0;
        }
        var changeResult = this.model.changeCollection(wfs, journalSize);

        if (result !== true) {
          if (result !== undefined) {
            arangoHelper.arangoError("Collection error: " + result);
            return 0;
          }
        }

        if (changeResult !== true) {
          arangoHelper.arangoNotification("Collection error", changeResult);
          return 0;
        }

        if (changeResult === true) {
          this.collectionsView.render();
          window.modalView.hide();
        }
      }
      else if (status === 'unloaded') {
        if (this.model.get('name') !== newname) {
          var result2 = this.model.renameCollection(newname);
          if (result2 === true) {
            this.collectionsView.render();
            window.modalView.hide();
          }
          else {
            arangoHelper.arangoError("Collection error: " + result2);
          }
        }
        else {
          window.modalView.hide();
        }
      }
    },



    //modal dialogs

    createEditPropertiesModal: function() {

      var collectionIsLoaded = false;

      if (this.model.get('status') === "loaded") {
        collectionIsLoaded = true;
      }

      var buttons = [],
        tableContent = [];

      if (! window.isCoordinator()) {
        tableContent.push(
          window.modalView.createTextEntry(
            "change-collection-name",
            "Name",
            this.model.get('name'),
            false,
            "",
            true,
            [
              {
                rule: Joi.string().regex(/^[a-zA-Z]/),
                msg: "Collection name must always start with a letter."
              },
              {
                rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
                msg: 'Only Symbols "_" and "-" are allowed.'
              },
              {
                rule: Joi.string().required(),
                msg: "No collection name given."
              }
            ]
          )
        );
      }

      if (collectionIsLoaded) {
        // needs to be refactored. move getProperties into model
        var journalSize = this.model.getProperties().journalSize;
        journalSize = journalSize/(1024*1024);

        tableContent.push(
          window.modalView.createTextEntry(
            "change-collection-size",
            "Journal size",
            journalSize,
            "The maximal size of a journal or datafile (in MB). Must be at least 1.",
            "",
            true,
            [
              {
                rule: Joi.string().allow('').optional().regex(/^[0-9]*$/),
                msg: "Must be a number."
              }
            ]
          )
        );
      }


      if (collectionIsLoaded) {
        // prevent "unexpected sync method error"
        var wfs = this.model.getProperties().waitForSync;
        tableContent.push(
          window.modalView.createSelectEntry(
            "change-collection-sync",
            "Wait for sync",
            wfs,
            "Synchronise to disk before returning from a create or update of a document.",
            [{value: false, label: "No"}, {value: true, label: "Yes"}]        )
        );
      }


      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "change-collection-id", "ID", this.model.get('id'), ""
        )
      );
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "change-collection-type", "Type", this.model.get('type'), ""
        )
      );
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          "change-collection-status", "Status", this.model.get('status'), ""
        )
      );
      buttons.push(
        window.modalView.createDeleteButton(
          "Delete",
          this.deleteCollection.bind(this)
        )
      );
      buttons.push(
        window.modalView.createDeleteButton(
          "Truncate",
          this.truncateCollection.bind(this)
        )
      );
      if (collectionIsLoaded) {
        buttons.push(
          window.modalView.createNotificationButton(
            "Unload",
            this.unloadCollection.bind(this)
          )
        );
      } else {
        buttons.push(
          window.modalView.createNotificationButton(
            "Load",
            this.loadCollection.bind(this)
          )
        );
      }

      buttons.push(
        window.modalView.createSuccessButton(
          "Save",
          this.saveModifiedCollection.bind(this)
        )
      );

      window.modalView.show(
        "modalTable.ejs",
        "Modify Collection",
        buttons,
        tableContent
      );
    },

    createInfoModal: function() {
      var buttons = [],
        tableContent = this.model;
      window.modalView.show(
        "modalCollectionInfo.ejs",
        "Collection: " + this.model.get('name'),
        buttons,
        tableContent
      );
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global _, Backbone, templateEngine, window, setTimeout, clearTimeout, arangoHelper, Joi, $*/

(function() {
  "use strict";
  window.CollectionsView = Backbone.View.extend({
    el: '#content',
    el2: '#collectionsThumbnailsIn',

    searchTimeout: null,

    initialize: function () {
    },

    template: templateEngine.createTemplate("collectionsView.ejs"),

    render: function () {
      var dropdownVisible = false;
      if ($('#collectionsDropdown').is(':visible')) {
        dropdownVisible = true;
      }

      $(this.el).html(this.template.render({}));
      this.setFilterValues();

      if (dropdownVisible === true) {
        $('#collectionsDropdown2').show();
      }

      var searchOptions = this.collection.searchOptions;

      this.collection.getFiltered(searchOptions).forEach(function (arango_collection) {
        $('#collectionsThumbnailsIn', this.el).append(new window.CollectionListItemView({
          model: arango_collection,
          collectionsView: this
        }).render().el);
      }, this);

      //if type in collectionsDropdown2 is changed,
      //the page will be rerendered, so check the toggel button
      if($('#collectionsDropdown2').css('display') === 'none') {
        $('#collectionsToggle').removeClass('activated');

      } else {
        $('#collectionsToggle').addClass('activated');
      }

      var length;

      try {
        length = searchOptions.searchPhrase.length;
      }
      catch (ignore) {
      }
      $('#searchInput').val(searchOptions.searchPhrase);
      $('#searchInput').focus();
      $('#searchInput')[0].setSelectionRange(length, length);

      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "left");

      return this;
    },

    events: {
      "click #createCollection"        : "createCollection",
      "keydown #searchInput"           : "restrictToSearchPhraseKey",
      "change #searchInput"            : "restrictToSearchPhrase",
      "click #searchSubmit"            : "restrictToSearchPhrase",
      "click .checkSystemCollections"  : "checkSystem",
      "click #checkLoaded"             : "checkLoaded",
      "click #checkUnloaded"           : "checkUnloaded",
      "click #checkDocument"           : "checkDocument",
      "click #checkEdge"               : "checkEdge",
      "click #sortName"                : "sortName",
      "click #sortType"                : "sortType",
      "click #sortOrder"               : "sortOrder",
      "click #collectionsToggle"       : "toggleView",
      "click .css-label"               : "checkBoxes"
    },

    updateCollectionsView: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
        }
      });
    },


    toggleView: function() {
      $('#collectionsToggle').toggleClass('activated');
      $('#collectionsDropdown2').slideToggle(200);
    },

    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    checkSystem: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeSystem;

      searchOptions.includeSystem = ($('.checkSystemCollections').is(":checked") === true);

      if (oldValue !== searchOptions.includeSystem) {
        this.render();
      }
    },
    checkEdge: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeEdge;

      searchOptions.includeEdge = ($('#checkEdge').is(":checked") === true);

      if (oldValue !== searchOptions.includeEdge) {
        this.render();
      }
    },
    checkDocument: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeDocument;

      searchOptions.includeDocument = ($('#checkDocument').is(":checked") === true);

      if (oldValue !== searchOptions.includeDocument) {
        this.render();
      }
    },
    checkLoaded: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeLoaded;

      searchOptions.includeLoaded = ($('#checkLoaded').is(":checked") === true);

      if (oldValue !== searchOptions.includeLoaded) {
        this.render();
      }
    },
    checkUnloaded: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.includeUnloaded;

      searchOptions.includeUnloaded = ($('#checkUnloaded').is(":checked") === true);

      if (oldValue !== searchOptions.includeUnloaded) {
        this.render();
      }
    },
    sortName: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.sortBy;

      searchOptions.sortBy = (($('#sortName').is(":checked") === true) ? 'name' : 'type');
      if (oldValue !== searchOptions.sortBy) {
        this.render();
      }
    },
    sortType: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.sortBy;

      searchOptions.sortBy = (($('#sortType').is(":checked") === true) ? 'type' : 'name');
      if (oldValue !== searchOptions.sortBy) {
        this.render();
      }
    },
    sortOrder: function () {
      var searchOptions = this.collection.searchOptions;
      var oldValue = searchOptions.sortOrder;

      searchOptions.sortOrder = (($('#sortOrder').is(":checked") === true) ? -1 : 1);
      if (oldValue !== searchOptions.sortOrder) {
        this.render();
      }
    },

    setFilterValues: function () {
      var searchOptions = this.collection.searchOptions;
      $('#checkLoaded').attr('checked', searchOptions.includeLoaded);
      $('#checkUnloaded').attr('checked', searchOptions.includeUnloaded);
      $('.checkSystemCollections').attr('checked', searchOptions.includeSystem);
      $('#checkEdge').attr('checked', searchOptions.includeEdge);
      $('#checkDocument').attr('checked', searchOptions.includeDocument);
      $('#sortName').attr('checked', searchOptions.sortBy !== 'type');
      $('#sortType').attr('checked', searchOptions.sortBy === 'type');
      $('#sortOrder').attr('checked', searchOptions.sortOrder !== 1);
    },

    search: function () {
      var searchOptions = this.collection.searchOptions;
      var searchPhrase = $('#searchInput').val();
      if (searchPhrase === searchOptions.searchPhrase) {
        return;
      }
      searchOptions.searchPhrase = searchPhrase;

      this.render();
    },

    resetSearch: function () {
      if (this.searchTimeout) {
        clearTimeout(this.searchTimeout);
        this.searchTimeout = null;
      }

      var searchOptions = this.collection.searchOptions;
      searchOptions.searchPhrase = null;
    },

    restrictToSearchPhraseKey: function (e) {
      // key pressed in search box
      var self = this;

      // force a new a search
      this.resetSearch();

      self.searchTimeout = setTimeout(function (){
        self.search();
      }, 200);
    },

    restrictToSearchPhrase: function () {
      // force a new a search
      this.resetSearch();

      // search executed
      this.search();
    },

    createCollection: function(e) {
      e.preventDefault();
      this.createNewCollectionModal();
    },

    submitCreateCollection: function() {
      var collName = $('#new-collection-name').val();
      var collSize = $('#new-collection-size').val();
      var collType = $('#new-collection-type').val();
      var collSync = $('#new-collection-sync').val();
      var shards = 1;
      var shardBy = [];
      if (window.isCoordinator()) {
        shards = $('#new-collection-shards').val();
        if (shards === "") {
          shards = 1;
        }
        shards = parseInt(shards, 10);
        if (shards < 1) {
          arangoHelper.arangoError(
            "Number of shards has to be an integer value greater or equal 1"
          );
          return 0;
        }
        shardBy = _.pluck($('#new-collection-shardBy').select2("data"), "text");
        if (shardBy.length === 0) {
          shardBy.push("_key");
        }
      }
      //no new system collections via webinterface
      //var isSystem = (collName.substr(0, 1) === '_');
      if (collName.substr(0, 1) === '_') {
        arangoHelper.arangoError('No "_" allowed as first character!');
        return 0;
      }
      var isSystem = false;
      var wfs = (collSync === "true");
      if (collSize > 0) {
        try {
          collSize = JSON.parse(collSize) * 1024 * 1024;
        }
        catch (e) {
          arangoHelper.arangoError('Please enter a valid number');
          return 0;
        }
      }
      if (collName === '') {
        arangoHelper.arangoError('No collection name entered!');
        return 0;
      }

      var returnobj = this.collection.newCollection(
        collName, wfs, isSystem, collSize, collType, shards, shardBy
      );
      if (returnobj.status !== true) {
        arangoHelper.arangoError(returnobj.errorMessage);
      }
      this.updateCollectionsView();
      window.modalView.hide();
    },

    createNewCollectionModal: function() {
      var buttons = [],
        tableContent = [],
        advanced = {},
        advancedTableContent = [];

      tableContent.push(
        window.modalView.createTextEntry(
          "new-collection-name",
          "Name",
          "",
          false,
          "",
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z]/),
              msg: "Collection name must always start with a letter."
            },
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only symbols, "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: "No collection name given."
            }
          ]
        )
      );
      tableContent.push(
        window.modalView.createSelectEntry(
          "new-collection-type",
          "Type",
          "",
          "The type of the collection to create.",
          [{value: 2, label: "Document"}, {value: 3, label: "Edge"}]
        )
      );
      if (window.isCoordinator()) {
        tableContent.push(
          window.modalView.createTextEntry(
            "new-collection-shards",
            "Shards",
            "",
            "The number of shards to create. You cannot change this afterwards. "
              + "Recommended: DBServers squared",
            "",
            true
          )
        );
        tableContent.push(
          window.modalView.createSelect2Entry(
            "new-collection-shardBy",
            "shardBy",
            "",
            "The keys used to distribute documents on shards. "
              + "Type the key and press return to add it.",
            "_key",
            false
          )
        );
      }
      buttons.push(
        window.modalView.createSuccessButton(
          "Save",
          this.submitCreateCollection.bind(this)
        )
      );
      advancedTableContent.push(
        window.modalView.createTextEntry(
          "new-collection-size",
          "Journal size",
          "",
          "The maximal size of a journal or datafile (in MB). Must be at least 1.",
          "",
          false,
          [
            {
              rule: Joi.string().allow('').optional().regex(/^[0-9]*$/),
              msg: "Must be a number."
            }
          ]
      )
      );
      advancedTableContent.push(
        window.modalView.createSelectEntry(
          "new-collection-sync",
          "Sync",
          "",
          "Synchronise to disk before returning from a create or update of a document.",
          [{value: false, label: "No"}, {value: true, label: "Yes"}]
        )
      );
      advanced.header = "Advanced";
      advanced.content = advancedTableContent;
      window.modalView.show(
        "modalTable.ejs",
        "New Collection",
        buttons,
        tableContent,
        advanced
      );
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
/*global document, console, Dygraph, _,templateEngine */

(function () {
  "use strict";

  function fmtNumber (n, nk) {
    if (n === undefined || n === null) {
      n = 0;
    }

    return n.toFixed(nk);
  }

  window.DashboardView = Backbone.View.extend({
    el: '#content',
    interval: 10000, // in milliseconds
    defaultTimeFrame: 20 * 60 * 1000, // 20 minutes in milliseconds
    defaultDetailFrame: 2 * 24 * 60 * 60 * 1000,
    history: {},
    graphs: {},

    events: {
      // will be filled in initialize
    },

    tendencies: {
      asyncPerSecondCurrent: [
        "asyncPerSecondCurrent", "asyncPerSecondPercentChange"
      ],

      syncPerSecondCurrent: [
        "syncPerSecondCurrent", "syncPerSecondPercentChange"
      ],

      clientConnectionsCurrent: [
        "clientConnectionsCurrent", "clientConnectionsPercentChange"
      ],

      clientConnectionsAverage: [
        "clientConnections15M", "clientConnections15MPercentChange"
      ],

      numberOfThreadsCurrent: [
        "numberOfThreadsCurrent", "numberOfThreadsPercentChange"
      ],

      numberOfThreadsAverage: [
        "numberOfThreads15M", "numberOfThreads15MPercentChange"
      ],

      virtualSizeCurrent: [
        "virtualSizeCurrent", "virtualSizePercentChange"
      ],

      virtualSizeAverage: [
        "virtualSize15M", "virtualSize15MPercentChange"
      ]
    },

    barCharts: {
      totalTimeDistribution: [
        "queueTimeDistributionPercent", "requestTimeDistributionPercent"
      ],
      dataTransferDistribution: [
        "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"
      ]
    },

    barChartsElementNames: {
      queueTimeDistributionPercent: "Queue",
      requestTimeDistributionPercent: "Computation",
      bytesSentDistributionPercent: "Bytes sent",
      bytesReceivedDistributionPercent: "Bytes received"

    },

    getDetailFigure : function (e) {
      var figure = $(e.currentTarget).attr("id").replace(/ChartButton/g, "");
      return figure;
    },

    showDetail: function (e) {
      var self = this,
          figure = this.getDetailFigure(e),
          options;

      options = this.dygraphConfig.getDetailChartConfig(figure);

      this.getHistoryStatistics(figure);
      this.detailGraphFigure = figure;

      window.modalView.hideFooter = true;
      window.modalView.hide();
      window.modalView.show(
        "modalGraph.ejs",
        options.header,
        undefined,
        undefined,
        undefined,
        this.events
      );

      window.modalView.hideFooter = false;

      $('#modal-dialog').on('hidden', function () {
        self.hidden();
      });

      $('#modal-dialog').toggleClass("modal-chart-detail", true);

      options.height = $(window).height() * 0.7;
      options.width = $('.modal-inner-detail').width();

      // Reselect the labelsDiv. It was not known when requesting options
      options.labelsDiv = $(options.labelsDiv)[0];

      this.detailGraph = new Dygraph(
        document.getElementById("lineChartDetail"),
        this.history[this.server][figure],
        options
      );
    },

    hidden: function () {
      this.detailGraph.destroy();
      delete this.detailGraph;
      delete this.detailGraphFigure;
    },


    getCurrentSize: function (div) {
      if (div.substr(0,1) !== "#") {
        div = "#" + div;
      }
      var height, width;
      $(div).attr("style", "");
      height = $(div).height();
      width = $(div).width();
      return {
        height: height,
        width: width
      };
    },

    prepareDygraphs: function () {
      var self = this, options;
      this.dygraphConfig.getDashBoardFigures().forEach(function (f) {
        options = self.dygraphConfig.getDefaultConfig(f);
        var dimensions = self.getCurrentSize(options.div);
        options.height = dimensions.height;
        options.width = dimensions.width;
        self.graphs[f] = new Dygraph(
          document.getElementById(options.div),
          self.history[self.server][f] || [],
          options
        );
      });
    },

    initialize: function () {
      this.dygraphConfig = this.options.dygraphConfig;
      this.d3NotInitialised = true;
      this.events["click .dashboard-sub-bar-menu-sign"] = this.showDetail.bind(this);
      this.events["mousedown .dygraph-rangesel-zoomhandle"] = this.stopUpdating.bind(this);
      this.events["mouseup .dygraph-rangesel-zoomhandle"] = this.startUpdating.bind(this);

      this.server = this.options.serverToShow;

      if (! this.server) {
        this.server = "-local-";
      }

      this.history[this.server] = {};
    },

    updateCharts: function () {
      var self = this;
      if (this.detailGraph) {
        this.updateLineChart(this.detailGraphFigure, true);
        return;
      }
      this.prepareD3Charts(this.isUpdating);
      this.prepareResidentSize(this.isUpdating);
      this.updateTendencies();
      Object.keys(this.graphs).forEach(function (f) {
        self.updateLineChart(f, false);
      });
    },

    updateTendencies: function () {
      var self = this, map = this.tendencies;

      var tempColor = "";
      Object.keys(map).forEach(function (a) {
        var v = self.history[self.server][a][1];
        var p = "";

        if (v < 0) {
          tempColor = "red";
        }
        else {
          tempColor = "green";
          p = "+";
        }
        $("#" + a).html(self.history[self.server][a][0] + '<br/><span class="dashboard-figurePer" style="color: '
          + tempColor +';">' + p + v + '%</span>');
      });
    },


    updateDateWindow: function (graph, isDetailChart) {
      var t = new Date().getTime();
      var borderLeft, borderRight;
      if (isDetailChart && graph.dateWindow_) {
        borderLeft = graph.dateWindow_[0];
        borderRight = t - graph.dateWindow_[1] - this.interval * 5 > 0 ?
        graph.dateWindow_[1] : t;
        return [borderLeft, borderRight];
      }
      return [t - this.defaultTimeFrame, t];


    },

    updateLineChart: function (figure, isDetailChart) {
      var g = isDetailChart ? this.detailGraph : this.graphs[figure],
      opts = {
        file: this.history[this.server][figure],
        dateWindow: this.updateDateWindow(g, isDetailChart)
      };
      g.updateOptions(opts);
    },

    mergeDygraphHistory: function (newData, i) {
      var self = this, valueList;

      this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {

        // check if figure is known
        if (! self.dygraphConfig.mapStatToFigure[f]) {
          return;
        }

        // need at least an empty history
        if (! self.history[self.server][f]) {
          self.history[self.server][f] = [];
        }

        // generate values for this key
        valueList = [];

        self.dygraphConfig.mapStatToFigure[f].forEach(function (a) {
          if (! newData[a]) {
            return;
          }

          if (a === "times") {
            valueList.push(new Date(newData[a][i] * 1000));
          }
          else {
            valueList.push(newData[a][i]);
          }
        });

        // if we found at list one value besides times, then use the entry
        if (valueList.length > 1) {
          self.history[self.server][f].push(valueList);
        }
      });
    },

    cutOffHistory: function (f, cutoff) {
      var self = this;

      while (self.history[self.server][f].length !== 0) {
        var v = self.history[self.server][f][0][0];

        if (v >= cutoff) {
          break;
        }

        self.history[self.server][f].shift();
      }
    },

    cutOffDygraphHistory: function (cutoff) {
      var self = this;
      var cutoffDate = new Date(cutoff);

      this.dygraphConfig.getDashBoardFigures(true).forEach(function (f) {

        // check if figure is known
        if (! self.dygraphConfig.mapStatToFigure[f]) {
          return;
        }

        // history must be non-empty
        if (! self.history[self.server][f]) {
          return;
        }

        self.cutOffHistory(f, cutoffDate);
      });
    },

    mergeHistory: function (newData) {
      var self = this, i;

      for (i = 0; i < newData.times.length; ++i) {
        this.mergeDygraphHistory(newData, i);
      }

      this.cutOffDygraphHistory(new Date().getTime() - this.defaultTimeFrame);

      // convert tendency values
      Object.keys(this.tendencies).forEach(function (a) {
        var n1 = 1;
        var n2 = 1;

        if (a === "virtualSizeCurrent" || a === "virtualSizeAverage") {
          newData[self.tendencies[a][0]] /= (1024 * 1024 * 1024);
          n1 = 2;
        }
        else if (a === "clientConnectionsCurrent") {
          n1 = 0;
        }
        else if (a === "numberOfThreadsCurrent") {
          n1 = 0;
        }

        self.history[self.server][a] = [
          fmtNumber(newData[self.tendencies[a][0]], n1),
          fmtNumber(newData[self.tendencies[a][1]] * 100, n2)
        ];
      });

      // update distribution
      Object.keys(this.barCharts).forEach(function (a) {
        self.history[self.server][a] = self.mergeBarChartData(self.barCharts[a], newData);
      });

      // update physical memory
      self.history[self.server].physicalMemory = newData.physicalMemory;
      self.history[self.server].residentSizeCurrent = newData.residentSizeCurrent;
      self.history[self.server].residentSizePercent = newData.residentSizePercent;

      // generate chart description
      self.history[self.server].residentSizeChart =
      [
        {
          "key": "",
          "color": this.dygraphConfig.colors[1],
          "values": [
            {
              label: "used",
              value: newData.residentSizePercent * 100
            }
          ]
        },
        {
          "key": "",
          "color": this.dygraphConfig.colors[0],
          "values": [
            {
              label: "used",
              value: 100 - newData.residentSizePercent * 100
            }
          ]
        }
      ]
      ;

      // remember next start
      this.nextStart = newData.nextStart;
    },

    mergeBarChartData: function (attribList, newData) {
      var i, v1 = {
        "key": this.barChartsElementNames[attribList[0]],
        "color": this.dygraphConfig.colors[0],
        "values": []
      }, v2 = {
        "key": this.barChartsElementNames[attribList[1]],
        "color": this.dygraphConfig.colors[1],
        "values": []
      };
      for (i = newData[attribList[0]].values.length - 1;  0 <= i;  --i) {
        v1.values.push({
          label: this.getLabel(newData[attribList[0]].cuts, i),
          value: newData[attribList[0]].values[i]
        });
        v2.values.push({
          label: this.getLabel(newData[attribList[1]].cuts, i),
          value: newData[attribList[1]].values[i]
        });
      }
      return [v1, v2];
    },

    getLabel: function (cuts, counter) {
      if (!cuts[counter]) {
        return ">" + cuts[counter - 1];
      }
      return counter === 0 ? "0 - " +
                         cuts[counter] : cuts[counter - 1] + " - " + cuts[counter];
    },

    getStatistics: function (callback) {
      var self = this;
      var url = "/_db/_system/_admin/aardvark/statistics/short";
      var urlParams = "?start=";

      if (self.nextStart) {
        urlParams += self.nextStart;
      }
      else {
        urlParams += (new Date().getTime() - self.defaultTimeFrame) / 1000;
      }

      if (self.server !== "-local-") {
        url = self.server.endpoint + "/_admin/aardvark/statistics/cluster";
        urlParams += "&type=short&DBserver=" + self.server.target;

        if (! self.history.hasOwnProperty(self.server)) {
          self.history[self.server] = {};
        }
      }

      $.ajax(
        url + urlParams,
        {async: true}
      ).done(
        function (d) {
          if (d.times.length > 0) {
            self.isUpdating = true;
            self.mergeHistory(d);
          }
          if (self.isUpdating === false) {
            return;
          }
          if (callback) {
            callback();
          }
          self.updateCharts();
      });
    },

    getHistoryStatistics: function (figure) {
      var self = this;
      var url = "statistics/long";

      var urlParams
        = "?filter=" + this.dygraphConfig.mapStatToFigure[figure].join();

      if (self.server !== "-local-") {
        url = self.server.endpoint + "/_admin/aardvark/statistics/cluster";
        urlParams += "&type=long&DBserver=" + self.server.target;

        if (! self.history.hasOwnProperty(self.server)) {
          self.history[self.server] = {};
        }
      }

      $.ajax(
        url + urlParams,
        {async: true}
      ).done(
        function (d) {
          var i;

          self.history[self.server][figure] = [];

          for (i = 0;  i < d.times.length;  ++i) {
            self.mergeDygraphHistory(d, i, true);
          }
        }
      );
    },

    prepareResidentSize: function (update) {
      var self = this;

      var dimensions = this.getCurrentSize('#residentSizeChartContainer');

      var current = self.history[self.server].residentSizeCurrent / 1024 / 1024;
      var currentA = "";

      if (current < 1025) {
        currentA = fmtNumber(current, 2) + " MB";
      }
      else {
        currentA = fmtNumber(current / 1024, 2) + " GB";
      }

      var currentP = fmtNumber(self.history[self.server].residentSizePercent * 100, 2);
      var data = [fmtNumber(self.history[self.server].physicalMemory / 1024 / 1024 / 1024, 0) + " GB"];

      nv.addGraph(function () {
        var chart = nv.models.multiBarHorizontalChart()
          .x(function (d) {
            return d.label;
          })
          .y(function (d) {
            return d.value;
          })
          .width(dimensions.width)
          .height(dimensions.height)
          .margin({
            top: ($("residentSizeChartContainer").outerHeight() - $("residentSizeChartContainer").height()) / 2,
            right: 1,
            bottom: ($("residentSizeChartContainer").outerHeight() - $("residentSizeChartContainer").height()) / 2,
            left: 1
          })
          .showValues(false)
          .showYAxis(false)
          .showXAxis(false)
          .transitionDuration(100)
          .tooltips(false)
          .showLegend(false)
          .showControls(false)
          .stacked(true);

        chart.yAxis
          .tickFormat(function (d) {return d + "%";})
          .showMaxMin(false);
        chart.xAxis.showMaxMin(false);

        d3.select('#residentSizeChart svg')
          .datum(self.history[self.server].residentSizeChart)
          .call(chart);

        d3.select('#residentSizeChart svg').select('.nv-zeroLine').remove();

        if (update) {
          d3.select('#residentSizeChart svg').select('#total').remove();
          d3.select('#residentSizeChart svg').select('#percentage').remove();
        }

        d3.select('.dashboard-bar-chart-title .percentage')
          .html(currentA + " ("+ currentP + " %)");

        d3.select('.dashboard-bar-chart-title .absolut')
          .html(data[0]);

        nv.utils.windowResize(chart.update);

        return chart;
      }, function() {
        d3.selectAll("#residentSizeChart .nv-bar").on('click',
          function() {
            // no idea why this has to be empty, well anyways...
          }
        );
      });
    },

    prepareD3Charts: function (update) {
      var self = this;

      var barCharts = {
        totalTimeDistribution: [
          "queueTimeDistributionPercent", "requestTimeDistributionPercent"],
        dataTransferDistribution: [
          "bytesSentDistributionPercent", "bytesReceivedDistributionPercent"]
      };

      if (this.d3NotInitialised) {
          update = false;
          this.d3NotInitialised = false;
      }

      _.each(Object.keys(barCharts), function (k) {
        var dimensions = self.getCurrentSize('#' + k
          + 'Container .dashboard-interior-chart');

        var selector = "#" + k + "Container svg";

        nv.addGraph(function () {
          var tickMarks = [0, 0.25, 0.5, 0.75, 1];
          var marginLeft = 75;
          var marginBottom = 23;
          var bottomSpacer = 6;

          if (dimensions.width < 219) {
            tickMarks = [0, 0.5, 1];
            marginLeft = 72;
            marginBottom = 21;
            bottomSpacer = 5;
          }
          else if (dimensions.width < 299) {
            tickMarks = [0, 0.3334, 0.6667, 1];
            marginLeft = 77;
          }
          else if (dimensions.width < 379) {
            marginLeft = 87;
          }
          else if (dimensions.width < 459) {
            marginLeft = 95;
          }
          else if (dimensions.width < 539) {
            marginLeft = 100;
          }
          else if (dimensions.width < 619) {
            marginLeft = 105;
          }

          var chart = nv.models.multiBarHorizontalChart()
            .x(function (d) {
              return d.label;
            })
            .y(function (d) {
              return d.value;
            })
            .width(dimensions.width)
            .height(dimensions.height)
            .margin({
              top: 5,
              right: 20,
              bottom: marginBottom,
              left: marginLeft
            })
            .showValues(false)
            .showYAxis(true)
            .showXAxis(true)
            .transitionDuration(100)
            .tooltips(false)
            .showLegend(false)
            .showControls(false)
            .forceY([0,1]);

          chart.yAxis
            .showMaxMin(false);

          var yTicks2 = d3.select('.nv-y.nv-axis')
            .selectAll('text')
            .attr('transform', 'translate (0, ' + bottomSpacer + ')') ;

          chart.yAxis
            .tickValues(tickMarks)
            .tickFormat(function (d) {return fmtNumber(((d * 100 * 100) / 100), 0) + "%";});

          d3.select(selector)
            .datum(self.history[self.server][k])
            .call(chart);

          nv.utils.windowResize(chart.update);

          return chart;
        }, function() {
          d3.selectAll(selector + " .nv-bar").on('click',
            function() {
              // no idea why this has to be empty, well anyways...
            }
          );
        });
      });

    },

    stopUpdating: function () {
      this.isUpdating = false;
    },

  startUpdating: function () {
    var self = this;
    if (self.timer) {
      return;
    }
    self.timer = window.setInterval(function () {
        self.getStatistics();
      },
      self.interval
    );
  },


  resize: function () {
    if (!this.isUpdating) {
      return;
    }
    var self = this, dimensions;
      _.each(this.graphs,function (g) {
      dimensions = self.getCurrentSize(g.maindiv_.id);
      g.resize(dimensions.width, dimensions.height);
    });
    if (this.detailGraph) {
      dimensions = this.getCurrentSize(this.detailGraph.maindiv_.id);
      this.detailGraph.resize(dimensions.width, dimensions.height);
    }
    this.prepareD3Charts(true);
    this.prepareResidentSize(true);
  },

  template: templateEngine.createTemplate("dashboardView.ejs"),

  render: function (modalView) {
    if (!modalView)  {
      $(this.el).html(this.template.render());
    }
    var callback = function() {
      this.prepareDygraphs();
      if (this.isUpdating) {
        this.prepareD3Charts();
        this.prepareResidentSize();
        this.updateTendencies();
      }
      this.startUpdating();
    }.bind(this);

    //check if user has _system permission
    var authorized = this.options.database.hasSystemAccess();
    if (!authorized) {
      $('.contentDiv').remove();
      $('.headerBar').remove();
      $('.dashboard-headerbar').remove();
      $('.dashboard-row').remove();
      $('#content').append(
        '<div style="color: red">You do not have permission to view this page.</div>'
      );
      $('#content').append(
        '<div style="color: red">You can switch to \'_system\' to see the dashboard.</div>'
      );
    }
    else {
      this.getStatistics(callback);
    }
  }
});
}());

/*jshint browser: true */
/*jshint unused: false */
/*global window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine, Joi*/
(function() {

  "use strict";

  window.databaseView = Backbone.View.extend({
    users: null,
    el: '#content',

    template: templateEngine.createTemplate("databaseView.ejs"),

    dropdownVisible: false,

    currentDB: "",

    events: {
      "click #createDatabase"       : "createDatabase",
      "click #submitCreateDatabase" : "submitCreateDatabase",
      "click .editDatabase"         : "editDatabase",
      "click .icon"                 : "editDatabase",
      "click #selectDatabase"       : "updateDatabase",
      "click #submitDeleteDatabase" : "submitDeleteDatabase",
      "click .contentRowInactive a" : "changeDatabase",
      "keyup #databaseSearchInput"  : "search",
      "click #databaseSearchSubmit" : "search",
      "click #databaseToggle"       : "toggleSettingsDropdown",
      "click .css-label"            : "checkBoxes",
      "change #dbSortDesc"          : "sorting",
      "click svg"                   : "switchDatabase"
    },

    sorting: function() {
      if ($('#dbSortDesc').is(":checked")) {
        this.collection.setSortingDesc(true);
      }
      else {
        this.collection.setSortingDesc(false);
      }

      if ($('#databaseDropdown').is(":visible")) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }

      this.render();
    },

    initialize: function() {
      this.collection.fetch({async:false});
    },

    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    render: function(){
      this.currentDatabase();

      //sorting
      this.collection.sort();

      $(this.el).html(this.template.render({
        collection   : this.collection,
        searchString : '',
        currentDB    : this.currentDB
      }));

      if (this.dropdownVisible === true) {
        $('#dbSortDesc').attr('checked', this.collection.sortOptions.desc);
        $('#databaseToggle').toggleClass('activated');
        $('#databaseDropdown2').show();
      }

      this.replaceSVGs();
      return this;
    },

    toggleSettingsDropdown: function() {
      //apply sorting to checkboxes
      $('#dbSortDesc').attr('checked', this.collection.sortOptions.desc);

      $('#databaseToggle').toggleClass('activated');
      $('#databaseDropdown2').slideToggle(200);
    },

    selectedDatabase: function () {
      return $('#selectDatabases').val();
    },

    handleError: function(status, text, dbname) {
      if (status === 409) {
        arangoHelper.arangoError("DB", "Database " + dbname + " already exists.");
        return;
      }
      if (status === 400) {
        arangoHelper.arangoError("DB", "Invalid Parameters");
        return;
      }
      if (status === 403) {
        arangoHelper.arangoError("DB", "Insufficent rights. Execute this from _system database");
        return;
      }
    },

    validateDatabaseInfo: function (db, user, pw) {
      if (user === "") {
        arangoHelper.arangoError("DB", "You have to define an owner for the new database");
        return false;
      }
      if (db === "") {
        arangoHelper.arangoError("DB", "You have to define a name for the new database");
        return false;
      }
      if (db.indexOf("_") === 0) {
        arangoHelper.arangoError("DB ", "Databasename should not start with _");
        return false;
      }
      if (!db.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError("DB", "Databasename may only contain numbers, letters, _ and -");
        return false;
      }
      return true;
    },

    createDatabase: function(e) {
      e.preventDefault();
      this.createAddDatabaseModal();
    },

    switchDatabase: function(e) {
      var changeTo = $(e.currentTarget).parent().find("h5").text();
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
    },

    submitCreateDatabase: function() {
      var self = this;
      var name  = $('#newDatabaseName').val();
      var userName = $('#newUser').val();
      var userPassword = $('#newPassword').val();
      if (!this.validateDatabaseInfo(name, userName, userPassword)) {
        return;
      }
      var options = {
        name: name,
        users: [
          {
            username: userName,
            passwd: userPassword,
            active: true
          }
        ]
      };
      this.collection.create(options, {
        wait:true,
        error: function(data, err) {
          self.handleError(err.status, err.statusText, name);
        },
        success: function(data) {
          self.updateDatabases();
          window.modalView.hide();
          window.App.naviView.dbSelectionView.render($("#dbSelect"));
        }
      });
    },

    submitDeleteDatabase: function(dbname) {
      var toDelete = this.collection.where({name: dbname});
      toDelete[0].destroy({wait: true, url:"/_api/database/"+dbname});
      this.updateDatabases();
      window.App.naviView.dbSelectionView.render($("#dbSelect"));
      window.modalView.hide();
    },

    currentDatabase: function() {
      this.currentDB = this.collection.getCurrentDatabase();
    },

    changeDatabase: function(e) {
      var changeTo = $(e.currentTarget).attr("id");
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
    },

    updateDatabases: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
          window.App.handleSelectDatabase();
        }
      });
    },

    editDatabase: function(e) {
      var dbName = this.evaluateDatabaseName($(e.currentTarget).attr("id"), '_edit-database'),
        isDeleteable = true;
      if(dbName === this.currentDB) {
        isDeleteable = false;
      }
      this.createEditDatabaseModal(dbName, isDeleteable);
    },

    search: function() {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#databaseSearchInput');
      searchString = $("#databaseSearchInput").val();
      reducedCollection = this.collection.filter(
        function(u) {
          return u.get("name").indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        collection   : reducedCollection,
        searchString : searchString,
        currentDB    : this.currentDB
      }));
      this.replaceSVGs();

      //after rendering, get the "new" element
      searchInput = $('#databaseSearchInput');
      //set focus on end of text in input field
      strLength= searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    replaceSVGs: function() {
      $(".svgToReplace").each(function() {
        var img = $(this);
        var id = img.attr("id");
        var src = img.attr("src");
        $.get(src, function(d) {
          var svg = $(d).find("svg");
          svg.attr("id", id)
            .attr("class", "tile-icon-svg")
            .removeAttr("xmlns:a");
          img.replaceWith(svg);
        }, "xml");
      });
    },

    evaluateDatabaseName : function(str, substr) {
      var index = str.lastIndexOf(substr);
      return str.substring(0, index);
    },

    createEditDatabaseModal: function(dbName, isDeleteable) {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry("id_name", "Name", dbName, "")
      );
      if (isDeleteable) {
        buttons.push(
          window.modalView.createDeleteButton(
            "Delete",
            this.submitDeleteDatabase.bind(this, dbName)
          )
        );
      } else {
        buttons.push(window.modalView.createDisabledButton("Delete"));
      }
      window.modalView.show(
        "modalTable.ejs",
        "Edit database",
        buttons,
        tableContent
      );
    },

    createAddDatabaseModal: function() {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createTextEntry(
          "newDatabaseName",
          "Name",
          "",
          false,
          "Database Name",
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z]/),
              msg: "Database name must start with a letter."
            },
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only Symbols "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: "No database name given."
            }
          ]
        )
      );
      tableContent.push(
        window.modalView.createTextEntry(
          "newUser",
          "Username",
          this.users !== null ? this.users.whoAmI() : 'root',
          "Please define the owner of this database. This will be the only user having "
            + "initial access to this database if authentication is turned on. Please note "
            + "that if you specify a username different to your account you will not be "
            + "able to access the database with your account after having creating it. "
            + "Specifying a username is mandatory even with authentication turned off. "
            + "If there is a failure you will be informed.",
          "Database Owner",
          true,
          [
            {
              rule: Joi.string().required(),
              msg: "No username given."
            }
          ]
        )
      );
      tableContent.push(
        window.modalView.createPasswordEntry(
          "newPassword",
          "Password",
          "",
          false,
          "",
          false
        )
      );
      buttons.push(
        window.modalView.createSuccessButton(
          "Create",
          this.submitCreateDatabase.bind(this)
        )
      );
      window.modalView.show(
        "modalTable.ejs",
        "Create Database",
        buttons,
        tableContent
      );
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global templateEngine, window, Backbone, $*/
(function() {
  "use strict";
  window.DBSelectionView = Backbone.View.extend({

    template: templateEngine.createTemplate("dbSelectionView.ejs"),

    events: {
      "click .dbSelectionLink": "changeDatabase"
    },

    initialize: function(opts) {
      this.current = opts.current;
    },

    changeDatabase: function(e) {
      var changeTo = $(e.currentTarget).closest(".dbSelectionLink.tab").attr("id");
      var url = this.collection.createDatabaseURL(changeTo);
      window.location.replace(url);
    },

    render: function(el) {
      this.$el = el;
      this.$el.html(this.template.render({
        list: this.collection.getDatabasesForUser(),
        current: this.current.get("name")
      }));
      this.delegateEvents();
      return this.el;
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, $, window, arangoHelper, jsoneditor, templateEngine */
/*global document, _ */

(function() {
  "use strict";

  var createDocumentLink = function(id) {
    var split = id.split("/");
    return "collection/"
      + encodeURIComponent(split[0]) + "/"
      + encodeURIComponent(split[1]);

  };

  window.DocumentView = Backbone.View.extend({
    el: '#content',
    colid: 0,
    docid: 0,

    template: templateEngine.createTemplate("documentView.ejs"),

    events: {
      "click #saveDocumentButton" : "saveDocument",
      "click #deleteDocumentButton" : "deleteDocumentModal",
      "click #confirmDeleteDocument" : "deleteDocument",
      "click #document-from" : "navigateToDocument",
      "click #document-to" : "navigateToDocument",
      "dblclick #documentEditor tr" : "addProperty"
    },

    editor: 0,

    setType: function (type) {
      var result, type2;
      if (type === 'edge') {
        result = this.collection.getEdge(this.colid, this.docid);
        type2 = "Edge: ";
      }
      else if (type === 'document') {
        result = this.collection.getDocument(this.colid, this.docid);
        type2 = "Document: ";
      }
      if (result === true) {
        this.type = type;
        this.fillInfo(type2);
        this.fillEditor();
        return true;
      }
    },

    deleteDocumentModal: function() {
      var buttons = [], tableContent = [];
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          'doc-delete-button',
          'Delete',
          'Delete this ' + this.type + '?',
          undefined,
          undefined,
          false,
          /[<>&'"]/
      )
      );
      buttons.push(
        window.modalView.createDeleteButton('Delete', this.deleteDocument.bind(this))
      );
      window.modalView.show('modalTable.ejs', 'Delete Document', buttons, tableContent, undefined,
      null);
    },

    deleteDocument: function() {

      var result;

      if (this.type === 'document') {
        result = this.collection.deleteDocument(this.colid, this.docid);
        if (result === false) {
          arangoHelper.arangoError('Document error:','Could not delete');
          return;
        }
      }
      else if (this.type === 'edge') {
        result = this.collection.deleteEdge(this.colid, this.docid);
        if (result === false) {
          arangoHelper.arangoError('Edge error:', 'Could not delete');
          return;
        }
      }
      if (result === true) {
        var navigateTo =  "collection/" + encodeURIComponent(this.colid) + '/documents/1';
        window.modalView.hide();
        window.App.navigate(navigateTo, {trigger: true});
      }
    },

    navigateToDocument: function(e) {
      var navigateTo = $(e.target).attr("documentLink");
      if (navigateTo) {
        window.App.navigate(navigateTo, {trigger: true});
      }
    },

    fillInfo: function(type) {
      var mod = this.collection.first();
      var _id = mod.get("_id"),
        _key = mod.get("_key"),
        _rev = mod.get("_rev"),
        _from = mod.get("_from"),
        _to = mod.get("_to");

      $('#document-type').text(type);
      $('#document-id').text(_id);
      $('#document-key').text(_key);
      $('#document-rev').text(_rev);

      if (_from && _to) {

        var hrefFrom = createDocumentLink(_from);
        var hrefTo = createDocumentLink(_to);
        $('#document-from').text(_from);
        $('#document-from').attr("documentLink", hrefFrom);
        $('#document-to').text(_to);
        $('#document-to').attr("documentLink", hrefTo);
      }
      else {
        $('.edge-info-container').hide();
      }
    },

    fillEditor: function() {
      var toFill = this.removeReadonlyKeys(this.collection.first().attributes);
      this.editor.set(toFill);
      $('.ace_content').attr('font-size','11pt');
    },

    jsonContentChanged: function() {
      this.enableSaveButton();
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      this.disableSaveButton();
      this.breadcrumb();

      var self = this;

      var container = document.getElementById('documentEditor');
      var options = {
        change: function(){self.jsonContentChanged();},
        search: true,
        mode: 'tree',
        modes: ['tree', 'code']
      };
      this.editor = new window.jsoneditor.JSONEditor(container, options);

      return this;
    },

    addProperty: function (e) {
      var node, searchResult;
      try {
        node = e.currentTarget.cells[2].childNodes[0].
          childNodes[0].childNodes[0].childNodes[1].
          childNodes[0].textContent;
      } catch (ex) {

      }
      if (node) {
        if (node === "object") {
          if (_.isEmpty(this.editor.get())) {
            this.editor.set({
              "": ""
            });
            this.editor.node.childs[0].focus("field");
          } else {
            this.editor.node.childs[0]._onInsertBefore(undefined, undefined, "auto");
          }
          return;
        }
        searchResult = this.editor.node.search(node);
        var breakLoop = false;
        searchResult.forEach(function (s) {
          if (breakLoop) {
            return;
          }
          if (s.elem === "field" ) {
            s.node._onInsertAfter(undefined, undefined, "auto");
            breakLoop = true;
          }
        });

      }
    },

    removeReadonlyKeys: function (object) {
      return _.omit(object, ["_key", "_id", "_from", "_to", "_rev"]);
    },

    saveDocument: function () {
      var model, result;

      try {
        model = this.editor.get();
      }
      catch (e) {
        this.errorConfirmation();
        this.disableSaveButton();
        return;
      }

      model = JSON.stringify(model);

      if (this.type === 'document') {
        result = this.collection.saveDocument(this.colid, this.docid, model);
        if (result === false) {
          arangoHelper.arangoError('Document error:','Could not save');
          return;
        }
      }
      else if (this.type === 'edge') {
        result = this.collection.saveEdge(this.colid, this.docid, model);
        if (result === false) {
          arangoHelper.arangoError('Edge error:', 'Could not save');
          return;
        }
      }

      if (result === true) {
        this.successConfirmation();
        this.disableSaveButton();
      }
    },

    successConfirmation: function () {
      $('#documentEditor .tree').animate({backgroundColor: '#C6FFB0'}, 500);
      $('#documentEditor .tree').animate({backgroundColor: '#FFFFF'}, 500);

      $('#documentEditor .ace_content').animate({backgroundColor: '#C6FFB0'}, 500);
      $('#documentEditor .ace_content').animate({backgroundColor: '#FFFFF'}, 500);
    },

    errorConfirmation: function () {
      $('#documentEditor .tree').animate({backgroundColor: '#FFB0B0'}, 500);
      $('#documentEditor .tree').animate({backgroundColor: '#FFFFF'}, 500);

      $('#documentEditor .ace_content').animate({backgroundColor: '#FFB0B0'}, 500);
      $('#documentEditor .ace_content').animate({backgroundColor: '#FFFFF'}, 500);
    },

    enableSaveButton: function () {
      $('#saveDocumentButton').prop('disabled', false);
      $('#saveDocumentButton').addClass('button-success');
      $('#saveDocumentButton').removeClass('button-close');
    },

    disableSaveButton: function () {
      $('#saveDocumentButton').prop('disabled', true);
      $('#saveDocumentButton').addClass('button-close');
      $('#saveDocumentButton').removeClass('button-success');
    },

    breadcrumb: function () {
      var name = window.location.hash.split("/");
      $('#transparentHeader').append(
        '<div class="breadcrumb">'+
        '<a href="#collections" class="activeBread">Collections</a>'+
        '<span class="disabledBread">&gt</span>'+
        '<a class="activeBread" href="#collection/' + name[1] + '/documents/1">' + name[1] + '</a>'+
        '<span class="disabledBread">&gt</span>'+
        '<a class="disabledBread">' + name[2] + '</a>'+
        '</div>'
      );
    },

    escaped: function (value) {
      return value.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
      .replace(/"/g, "&quot;").replace(/'/g, "&#39;");
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, arangoHelper, _, $, window, arangoHelper, templateEngine, Joi, btoa */

(function() {
  "use strict";
  window.DocumentsView = window.PaginationView.extend({
    filters : { "0" : true },
    filterId : 0,
    paginationDiv : "#documentsToolbarF",
    idPrefix : "documents",

    addDocumentSwitch: true,
    activeFilter: false,
    lastCollectionName: undefined,
    restoredFilters: [],

    editMode: false,

    allowUpload: false,

    el: '#content',
    table: '#documentsTableID',

    template: templateEngine.createTemplate("documentsView.ejs"),

    collectionContext : {
      prev: null,
      next: null
    },

    editButtons: ["#deleteSelected", "#moveSelected"],

    initialize : function () {
      this.documentStore = this.options.documentStore;
      this.collectionsStore = this.options.collectionsStore;
      this.tableView = new window.TableView({
        el: this.table,
        collection: this.collection
      });
      this.tableView.setRowClick(this.clicked.bind(this));
      this.tableView.setRemoveClick(this.remove.bind(this));
    },

    setCollectionId : function (colid, pageid) {
      this.collection.setCollection(colid);
      var type = arangoHelper.collectionApiType(colid);
      this.pageid = pageid;
      this.type = type;

      this.checkCollectionState();

      this.collection.getDocuments(this.getDocsCallback.bind(this));
      this.collectionModel = this.collectionsStore.get(colid);
    },

    getDocsCallback: function() {
      //Hide first/last pagination
      $('#documents_last').css("visibility", "hidden");
      $('#documents_first').css("visibility", "hidden");
      this.drawTable();
      this.renderPaginationElements();
    },

    events: {
      "click #collectionPrev"      : "prevCollection",
      "click #collectionNext"      : "nextCollection",
      "click #filterCollection"    : "filterCollection",
      "click #markDocuments"       : "editDocuments",
      "click #indexCollection"     : "indexCollection",
      "click #importCollection"    : "importCollection",
      "click #exportCollection"    : "exportCollection",
      "click #filterSend"          : "sendFilter",
      "click #addFilterItem"       : "addFilterItem",
      "click .removeFilterItem"    : "removeFilterItem",
      "click #deleteSelected"      : "deleteSelectedDocs",
      "click #moveSelected"        : "moveSelectedDocs",
      "click #addDocumentButton"   : "addDocumentModal",
      "click #documents_first"     : "firstDocuments",
      "click #documents_last"      : "lastDocuments",
      "click #documents_prev"      : "prevDocuments",
      "click #documents_next"      : "nextDocuments",
      "click #confirmDeleteBtn"    : "confirmDelete",
      "click .key"                 : "nop",
      "keyup"                      : "returnPressedHandler",
      "keydown .queryline input"   : "filterValueKeydown",
      "click #importModal"         : "showImportModal",
      "click #resetView"           : "resetView",
      "click #confirmDocImport"    : "startUpload",
      "click #exportDocuments"     : "startDownload",
      "change #newIndexType"       : "selectIndexType",
      "click #createIndex"         : "createIndex",
      "click .deleteIndex"         : "prepDeleteIndex",
      "click #confirmDeleteIndexBtn"    : "deleteIndex",
      "click #documentsToolbar ul"      : "resetIndexForms",
      "click #indexHeader #addIndex"    : "toggleNewIndexView",
      "click #indexHeader #cancelIndex" : "toggleNewIndexView",
      "change #documentSize"            : "setPagesize",
      "change #docsSort"                : "setSorting"
    },

    showSpinner: function() {
      $('#uploadIndicator').show();
    },

    hideSpinner: function() {
      $('#uploadIndicator').hide();
    },

    showImportModal: function() {
      $("#docImportModal").modal('show');
    },

    hideImportModal: function() {
      $("#docImportModal").modal('hide');
    },

    setPagesize: function() {
      var size = $('#documentSize').find(":selected").val();
      this.collection.setPagesize(size);
      this.collection.getDocuments(this.getDocsCallback.bind(this));
    },

    setSorting: function() {
      var sortAttribute = $('#docsSort').val();

      if (sortAttribute === '' || sortAttribute === undefined || sortAttribute === null) {
        sortAttribute = '_key';
      }

      this.collection.setSort(sortAttribute);
    },

    returnPressedHandler: function(event) {
      if (event.keyCode === 13 && $(event.target).is($('#docsSort'))) {
        this.collection.getDocuments(this.getDocsCallback.bind(this));
      }
      if (event.keyCode === 13) {
        if ($("#confirmDeleteBtn").attr("disabled") === false) {
          this.confirmDelete();
        }
      }
    },
    toggleNewIndexView: function () {
      $('#indexEditView').toggle("fast");
      $('#newIndexView').toggle("fast");
      this.resetIndexForms();
    },
    nop: function(event) {
      event.stopPropagation();
    },

    resetView: function () {
      //clear all input/select - fields
      $('input').val('');
      $('select').val('==');
      this.removeAllFilterItems();
      $('#documentSize').val(this.collection.getPageSize());

      $('#documents_last').css("visibility", "visible");
      $('#documents_first').css("visibility", "visible");
      this.addDocumentSwitch = true;
      this.collection.resetFilter();
      this.collection.loadTotal();
      this.restoredFilters = [];

      //for resetting json upload
      this.allowUpload = false;
      this.files = undefined;
      this.file = undefined;
      $('#confirmDocImport').attr("disabled", true);

      this.markFilterToggle();
      this.collection.getDocuments(this.getDocsCallback.bind(this));
    },

    startDownload: function() {
      var query = this.collection.buildDownloadDocumentQuery();

      if (query !== '' || query !== undefined || query !== null) {
        window.open(encodeURI("query/result/download/" + btoa(JSON.stringify(query))));
      }
      else {
        arangoHelper.arangoError("Document error", "could not download documents");
      }
    },

    startUpload: function () {
      var result;
      if (this.allowUpload === true) {
          this.showSpinner();
        result = this.collection.uploadDocuments(this.file);
        if (result !== true) {
          this.hideSpinner();
          this.hideImportModal();
          this.resetView();
          arangoHelper.arangoError(result);
          return;
        }
        this.hideSpinner();
        this.hideImportModal();
        this.resetView();
        return;
      }
    },

    uploadSetup: function () {
      var self = this;
      $('#importDocuments').change(function(e) {
        self.files = e.target.files || e.dataTransfer.files;
        self.file = self.files[0];
        $('#confirmDocImport').attr("disabled", false);

        self.allowUpload = true;
      });
    },

    buildCollectionLink : function (collection) {
      return "collection/" + encodeURIComponent(collection.get('name')) + '/documents/1';
    },

    prevCollection : function () {
      if (this.collectionContext.prev !== null) {
        $('#collectionPrev').parent().removeClass('disabledPag');
        window.App.navigate(
          this.buildCollectionLink(
            this.collectionContext.prev
          ),
          {
            trigger: true
          }
        );
      }
      else {
        $('#collectionPrev').parent().addClass('disabledPag');
      }
    },

    nextCollection : function () {
      if (this.collectionContext.next !== null) {
        $('#collectionNext').parent().removeClass('disabledPag');
        window.App.navigate(
          this.buildCollectionLink(
            this.collectionContext.next
          ),
          {
            trigger: true
          }
        );
      }
      else {
        $('#collectionNext').parent().addClass('disabledPag');
      }
    },

    markFilterToggle: function () {
      if (this.restoredFilters.length > 0) {
        $('#filterCollection').addClass('activated');
      }
      else {
        $('#filterCollection').removeClass('activated');
      }
    },

    //need to make following functions automatically!

    editDocuments: function () {
      $('#indexCollection').removeClass('activated');
      $('#importCollection').removeClass('activated');
      $('#exportCollection').removeClass('activated');
      this.markFilterToggle();
      $('#markDocuments').toggleClass('activated');
      this.changeEditMode();
      $('#filterHeader').hide();
      $('#importHeader').hide();
      $('#indexHeader').hide();
      $('#editHeader').slideToggle(200);
      $('#exportHeader').hide();
    },

    filterCollection : function () {
      $('#indexCollection').removeClass('activated');
      $('#importCollection').removeClass('activated');
      $('#exportCollection').removeClass('activated');
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      this.markFilterToggle();
      this.activeFilter = true;
      $('#importHeader').hide();
      $('#indexHeader').hide();
      $('#editHeader').hide();
      $('#exportHeader').hide();
      $('#filterHeader').slideToggle(200);

      var i;
      for (i in this.filters) {
        if (this.filters.hasOwnProperty(i)) {
          $('#attribute_name' + i).focus();
          return;
        }
      }
    },

    exportCollection: function () {
      $('#indexCollection').removeClass('activated');
      $('#importCollection').removeClass('activated');
      $('#filterHeader').removeClass('activated');
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      $('#exportCollection').toggleClass('activated');
      this.markFilterToggle();
      $('#exportHeader').slideToggle(200);
      $('#importHeader').hide();
      $('#indexHeader').hide();
      $('#filterHeader').hide();
      $('#editHeader').hide();
    },

    importCollection: function () {
      this.markFilterToggle();
      $('#indexCollection').removeClass('activated');
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      $('#importCollection').toggleClass('activated');
      $('#exportCollection').removeClass('activated');
      $('#importHeader').slideToggle(200);
      $('#filterHeader').hide();
      $('#indexHeader').hide();
      $('#editHeader').hide();
      $('#exportHeader').hide();
    },

    indexCollection: function () {
      this.markFilterToggle();
      $('#importCollection').removeClass('activated');
      $('#exportCollection').removeClass('activated');
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      $('#indexCollection').toggleClass('activated');
      $('#newIndexView').hide();
      $('#indexEditView').show();
      $('#indexHeader').slideToggle(200);
      $('#importHeader').hide();
      $('#editHeader').hide();
      $('#filterHeader').hide();
      $('#exportHeader').hide();
    },

    changeEditMode: function (enable) {
      if (enable === false || this.editMode === true) {
        $('#documentsTableID tbody tr').css('cursor', 'default');
        $('.deleteButton').fadeIn();
        $('.addButton').fadeIn();
        $('.selected-row').removeClass('selected-row');
        this.editMode = false;
        this.tableView.setRowClick(this.clicked.bind(this));
      }
      else {
        $('#documentsTableID tbody tr').css('cursor', 'copy');
        $('.deleteButton').fadeOut();
        $('.addButton').fadeOut();
        $('.selectedCount').text(0);
        this.editMode = true;
        this.tableView.setRowClick(this.editModeClick.bind(this));
      }
    },

    getFilterContent: function () {
      var filters = [ ];
      var i;

      for (i in this.filters) {
        if (this.filters.hasOwnProperty(i)) {
          var value = $('#attribute_value' + i).val();

          try {
            value = JSON.parse(value);
          }
          catch (err) {
            value = String(value);
          }

          if ($('#attribute_name' + i).val() !== ''){
            filters.push({
                attribute : $('#attribute_name'+i).val(),
                operator : $('#operator'+i).val(),
                value : value
            });
          }
        }
      }
      return filters;
    },

    sendFilter : function () {
      this.restoredFilters = this.getFilterContent();
      var self = this;
      this.collection.resetFilter();
      this.addDocumentSwitch = false;
      _.each(this.restoredFilters, function (f) {
        if (f.operator !== undefined) {
          self.collection.addFilter(f.attribute, f.operator, f.value);
        }
      });
      this.collection.setToFirst();

      this.collection.getDocuments(this.getDocsCallback.bind(this));
      this.markFilterToggle();
    },

    restoreFilter: function () {
      var self = this, counter = 0;

      this.filterId = 0;
      $('#docsSort').val(this.collection.getSort());
      _.each(this.restoredFilters, function (f) {
        //change html here and restore filters
        if (counter !== 0) {
          self.addFilterItem();
        }
        if (f.operator !== undefined) {
          $('#attribute_name' + counter).val(f.attribute);
          $('#operator' + counter).val(f.operator);
          $('#attribute_value' + counter).val(f.value);
        }
        counter++;

        //add those filters also to the collection
        self.collection.addFilter(f.attribute, f.operator, f.value);
      });
    },

    addFilterItem : function () {
      // adds a line to the filter widget

      var num = ++this.filterId;
      $('#filterHeader').append(' <div class="queryline querylineAdd">'+
                                '<input id="attribute_name' + num +
                                '" type="text" placeholder="Attribute name">'+
                                '<select name="operator" id="operator' +
                                num + '" class="filterSelect">'+
                                '    <option value="==">==</option>'+
                                '    <option value="!=">!=</option>'+
                                '    <option value="&lt;">&lt;</option>'+
                                '    <option value="&lt;=">&lt;=</option>'+
                                '    <option value="&gt;=">&gt;=</option>'+
                                '    <option value="&gt;">&gt;</option>'+
                                '</select>'+
                                '<input id="attribute_value' + num +
                                '" type="text" placeholder="Attribute value" ' +
                                'class="filterValue">'+
                                ' <a class="removeFilterItem" id="removeFilter' + num + '">' +
                                '<i class="icon icon-minus arangoicon"></i></a></div>');
      this.filters[num] = true;
    },

    filterValueKeydown : function (e) {
      if (e.keyCode === 13) {
        this.sendFilter();
      }
    },

    removeFilterItem : function (e) {

      // removes line from the filter widget
      var button = e.currentTarget;

      var filterId = button.id.replace(/^removeFilter/, '');
      // remove the filter from the list
      delete this.filters[filterId];
      delete this.restoredFilters[filterId];

      // remove the line from the DOM
      $(button.parentElement).remove();
    },

    removeAllFilterItems : function () {
      var childrenLength = $('#filterHeader').children().length;
      var i;
      for (i = 1; i <= childrenLength; i++) {
        $('#removeFilter'+i).parent().remove();
      }
      this.filters = { "0" : true };
      this.filterId = 0;
    },

    addDocumentModal: function () {
      var collid  = window.location.hash.split("/")[1],
      buttons = [], tableContent = [],
      // second parameter is "true" to disable caching of collection type
      doctype = arangoHelper.collectionApiType(collid, true);
      if (doctype === 'edge') {

        tableContent.push(
          window.modalView.createTextEntry(
            'new-edge-from-attr',
            '_from',
            '',
            "document _id: document handle of the linked vertex (incoming relation)",
            undefined,
            false,
            [
              {
                rule: Joi.string().required(),
                msg: "No _from attribute given."
              }
            ]
          )
        );

        tableContent.push(
          window.modalView.createTextEntry(
            'new-edge-to',
            '_to',
            '',
            "document _id: document handle of the linked vertex (outgoing relation)",
            undefined,
            false,
            [
              {
                rule: Joi.string().required(),
                msg: "No _to attribute given."
              }
            ]
          )
        );

        tableContent.push(
          window.modalView.createTextEntry(
            'new-edge-key-attr',
            '_key',
            undefined,
            "the edges unique key(optional attribute, leave empty for autogenerated key",
            'is optional: leave empty for autogenerated key',
            false,
            [
              {/*optional validation rules for joi*/}
            ]
          )
        );

        buttons.push(
          window.modalView.createSuccessButton('Create', this.addEdge.bind(this))
        );

        window.modalView.show(
          'modalTable.ejs',
          'Create edge',
          buttons,
          tableContent
        );

        return;
      }
      else {
        tableContent.push(
          window.modalView.createTextEntry(
            'new-document-key-attr',
            '_key',
            undefined,
            "the documents unique key(optional attribute, leave empty for autogenerated key",
            'is optional: leave empty for autogenerated key',
            false,
            [
              {/*optional validation rules for joi*/}
            ]
          )
        );

        buttons.push(
          window.modalView.createSuccessButton('Create', this.addDocument.bind(this))
        );

        window.modalView.show(
          'modalTable.ejs',
          'Create document',
          buttons,
          tableContent
        );
      }
    },

    addEdge: function () {
      var collid  = window.location.hash.split("/")[1];
      var from = $('.modal-body #new-edge-from-attr').last().val();
      var to = $('.modal-body #new-edge-to').last().val();
      var key = $('.modal-body #new-edge-key-attr').last().val();

      var result;
      if (key !== '' || key !== undefined) {
        result = this.documentStore.createTypeEdge(collid, from, to, key);
      }
      else {
        result = this.documentStore.createTypeEdge(collid, from, to);
      }

      if (result !== false) {
        //$('#edgeCreateModal').modal('hide');
        window.modalView.hide();
        window.location.hash = "collection/"+result;
      }
      //Error
      else {
        arangoHelper.arangoError('Creating edge failed');
      }
    },

    addDocument: function() {
      var collid = window.location.hash.split("/")[1];
      var key = $('.modal-body #new-document-key-attr').last().val();
      var result;
      if (key !== '' || key !== undefined) {
        result = this.documentStore.createTypeDocument(collid, key);
      }
      else {
        result = this.documentStore.createTypeDocument(collid);
      }
      //Success
      if (result !== false) {
        window.modalView.hide();
        window.location.hash = "collection/" + result;
      }
      else {
        arangoHelper.arangoError('Creating document failed');
      }
    },

    moveSelectedDocs: function() {
      var buttons = [], tableContent = [],
      toDelete = this.getSelectedDocs();

      if (toDelete.length === 0) {
        return;
      }

      tableContent.push(
        window.modalView.createTextEntry(
          'move-documents-to',
          'Move to',
          '',
          false,
          'collection-name',
          true,
          [
            {
              rule: Joi.string().regex(/^[a-zA-Z]/),
              msg: "Collection name must always start with a letter."
            },
            {
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only Symbols "_" and "-" are allowed.'
            },
            {
              rule: Joi.string().required(),
              msg: "No collection name given."
            }
          ]
        )
      );

      buttons.push(
        window.modalView.createSuccessButton('Move', this.confirmMoveSelectedDocs.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Move documents',
        buttons,
        tableContent
      );
    },

    confirmMoveSelectedDocs: function() {
      var toMove = this.getSelectedDocs(),
      self = this,
      toCollection = $('.modal-body').last().find('#move-documents-to').val();

      var callback = function() {
        this.collection.getDocuments(this.getDocsCallback.bind(this));
        $('#markDocuments').click();
        window.modalView.hide();
      }.bind(this);

      _.each(toMove, function(key) {
        self.collection.moveDocument(key, self.collection.collectionID, toCollection, callback);
      });
    },

    deleteSelectedDocs: function() {
      var buttons = [], tableContent = [];
      var toDelete = this.getSelectedDocs();

      if (toDelete.length === 0) {
        return;
      }

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          undefined,
          toDelete.length + ' documents selected',
          'Do you want to delete all selected documents?',
          undefined,
          undefined,
          false,
          undefined
        )
      );

      buttons.push(
        window.modalView.createDeleteButton('Delete', this.confirmDeleteSelectedDocs.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Delete documents',
        buttons,
        tableContent
      );
    },

    confirmDeleteSelectedDocs: function() {
      var toDelete = this.getSelectedDocs();
      var deleted = [], self = this;

      _.each(toDelete, function(key) {
        var result = false;
        if (self.type === 'document') {
          result = self.documentStore.deleteDocument(
            self.collection.collectionID, key
          );
          if (result) {
            //on success
            deleted.push(true);
            self.collection.setTotalMinusOne();
          }
          else {
            deleted.push(false);
            arangoHelper.arangoError('Document error', 'Could not delete document.');
          }
        }
        else if (self.type === 'edge') {
          result = self.documentStore.deleteEdge(self.collection.collectionID, key);
          if (result === true) {
            //on success
            self.collection.setTotalMinusOne();
            deleted.push(true);
          }
          else {
            deleted.push(false);
            arangoHelper.arangoError('Edge error', 'Could not delete edge');
          }
        }
      });
      this.collection.getDocuments(this.getDocsCallback.bind(this));
      $('#markDocuments').click();
      window.modalView.hide();
    },

    getSelectedDocs: function() {
      var toDelete = [];
      _.each($('#documentsTableID tbody tr'), function(element) {
        if ($(element).hasClass('selected-row')) {
          toDelete.push($($(element).children()[1]).find('.key').text());
        }
      });
      return toDelete;
    },

    remove: function (a) {
      this.docid = $(a.currentTarget).closest("tr").attr("id").substr(4);
      $("#confirmDeleteBtn").attr("disabled", false);
      $('#docDeleteModal').modal('show');
    },

    confirmDelete: function () {
      $("#confirmDeleteBtn").attr("disabled", true);
      var hash = window.location.hash.split("/");
      var check = hash[3];
      //to_do - find wrong event handler
      if (check !== 'source') {
        this.reallyDelete();
      }
    },

    reallyDelete: function () {
      var self = this;
      var row = $(self.target).closest("tr").get(0);

      var deleted = false;
      var result;
      if (this.type === 'document') {
        result = this.documentStore.deleteDocument(
          this.collection.collectionID, this.docid
        );
        if (result) {
          //on success
          this.collection.setTotalMinusOne();
          deleted = true;
        }
        else {
          arangoHelper.arangoError('Doc error');
        }
      }
      else if (this.type === 'edge') {
        result = this.documentStore.deleteEdge(this.collection.collectionID, this.docid);
        if (result === true) {
          //on success
          this.collection.setTotalMinusOne();
          deleted = true;
        }
        else {
          arangoHelper.arangoError('Edge error');
        }
      }

      if (deleted === true) {
        this.collection.getDocuments(this.getDocsCallback.bind(this));
        $('#docDeleteModal').modal('hide');
      }

    },

    editModeClick: function(event) {
      var target = $(event.currentTarget);

      if(target.hasClass('selected-row')) {
        target.removeClass('selected-row');
      } else {
        target.addClass('selected-row');
      }

      var selected = this.getSelectedDocs();
      $('.selectedCount').text(selected.length);

      _.each(this.editButtons, function(button) {
        if (selected.length > 0) {
          $(button).prop('disabled', false);
          $(button).removeClass('button-neutral');
          $(button).removeClass('disabled');
          if (button === "#moveSelected") {
            $(button).addClass('button-success');
          }
          else {
            $(button).addClass('button-danger');
          }
        }
        else {
          $(button).prop('disabled', true);
          $(button).addClass('disabled');
          $(button).addClass('button-neutral');
          if (button === "#moveSelected") {
            $(button).removeClass('button-success');
          }
          else {
            $(button).removeClass('button-danger');
          }
        }
      });
    },

    clicked: function (event) {
      var self = event.currentTarget;
      window.App.navigate("collection/" + this.collection.collectionID + "/" + $(self).attr("id").substr(4), true);
    },

    drawTable: function() {
      this.tableView.setElement($(this.table)).render();

      // we added some icons, so we need to fix their tooltips
      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");

      $(".prettify").snippet("javascript", {
        style: "nedit",
        menu: false,
        startText: false,
        transparent: true,
        showNum: false
      });
    },

    checkCollectionState: function() {
      if (this.lastCollectionName === this.collectionName) {
        if (this.activeFilter) {
          this.filterCollection();
          this.restoreFilter();
        }
      }
      else {
        if (this.lastCollectionName !== undefined) {
          this.collection.resetFilter();
          this.collection.setSort('_key');
          this.restoredFilters = [];
          this.activeFilter = false;
        }
      }
    },

    render: function() {
      $(this.el).html(this.template.render({}));
      this.tableView.setElement($(this.table)).drawLoading();

      this.collectionContext = this.collectionsStore.getPosition(
        this.collection.collectionID
      );

      this.getIndex();
      this.breadcrumb();

      this.checkCollectionState();

      //set last active collection name
      this.lastCollectionName = this.collectionName;

      if (this.collectionContext.prev === null) {
        $('#collectionPrev').parent().addClass('disabledPag');
      }
      if (this.collectionContext.next === null) {
        $('#collectionNext').parent().addClass('disabledPag');
      }

      this.uploadSetup();

      $("[data-toggle=tooltip]").tooltip();
      $('.upload-info').tooltip();

      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");
      this.renderPaginationElements();
      this.selectActivePagesize();
      this.markFilterToggle();
      return this;
    },

    rerender : function () {
      this.collection.getDocuments(this.getDocsCallback.bind(this));
    },

    selectActivePagesize: function() {
      $('#documentSize').val(this.collection.getPageSize());
    },

    renderPaginationElements: function () {
      this.renderPagination();
      var total = $('#totalDocuments');
      if (total.length === 0) {
        $('#documentsToolbarFL').append(
          '<a id="totalDocuments" class="totalDocuments"></a>'
        );
        total = $('#totalDocuments');
      }
      total.html("Total: " + this.collection.getTotal() + " document(s)");
    },

    breadcrumb: function () {
      this.collectionName = window.location.hash.split("/")[1];
      $('#transparentHeader').append(
        '<div class="breadcrumb">'+
        '<a class="activeBread" href="#collections">Collections</a>'+
        '<span class="disabledBread">&gt</span>'+
        '<a class="disabledBread">'+this.collectionName+'</a>'+
        '</div>'
      );
    },

    resetIndexForms: function () {
      $('#indexHeader input').val('').prop("checked", false);
      $('#newIndexType').val('Cap').prop('selected',true);
      this.selectIndexType();
    },
    stringToArray: function (fieldString) {
      var fields = [];
      fieldString.split(',').forEach(function(field){
        field = field.replace(/(^\s+|\s+$)/g,'');
        if (field !== '') {
          fields.push(field);
        }
      });
      return fields;
    },
    createIndex: function () {
      //e.preventDefault();
      var self = this;
      var indexType = $('#newIndexType').val();
      var result;
      var postParameter = {};
      var fields;
      var unique;
      var sparse;

      switch (indexType) {
        case 'Cap':
          var size = parseInt($('#newCapSize').val(), 10) || 0;
          var byteSize = parseInt($('#newCapByteSize').val(), 10) || 0;
          postParameter = {
            type: 'cap',
            size: size,
            byteSize: byteSize
          };
          break;
        case 'Geo':
          //HANDLE ARRAY building
          fields = $('#newGeoFields').val();
          var geoJson = self.checkboxToValue('#newGeoJson');
          var constraint = self.checkboxToValue('#newGeoConstraint');
          var ignoreNull = self.checkboxToValue('#newGeoIgnoreNull');
          postParameter = {
            type: 'geo',
            fields: self.stringToArray(fields),
            geoJson: geoJson,
            constraint: constraint,
            ignoreNull: ignoreNull
          };
          break;
        case 'Hash':
          fields = $('#newHashFields').val();
          unique = self.checkboxToValue('#newHashUnique');
          sparse = self.checkboxToValue('#newHashSparse');
          postParameter = {
            type: 'hash',
            fields: self.stringToArray(fields),
            unique: unique,
            sparse: sparse
          };
          break;
        case 'Fulltext':
          fields = ($('#newFulltextFields').val());
          var minLength =  parseInt($('#newFulltextMinLength').val(), 10) || 0;
          postParameter = {
            type: 'fulltext',
            fields: self.stringToArray(fields),
            minLength: minLength
          };
          break;
        case 'Skiplist':
          fields = $('#newSkiplistFields').val();
          unique = self.checkboxToValue('#newSkiplistUnique');
          sparse = self.checkboxToValue('#newSkiplistSparse');
          postParameter = {
            type: 'skiplist',
            fields: self.stringToArray(fields),
            unique: unique,
            sparse: sparse
          };
          break;
      }
      result = self.collectionModel.createIndex(postParameter);
      if (result === true) {
        $('#collectionEditIndexTable tbody tr').remove();
        self.getIndex();
        self.toggleNewIndexView();
        self.resetIndexForms();
      }
      else {
        if (result.responseText) {
          var message = JSON.parse(result.responseText);
          arangoHelper.arangoNotification("Document error", message.errorMessage);
        }
        else {
          arangoHelper.arangoNotification("Document error", "Could not create index.");
        }
      }
    },

    prepDeleteIndex: function (e) {
      this.lastTarget = e;
      this.lastId = $(this.lastTarget.currentTarget).
                    parent().
                    parent().
                    first().
                    children().
                    first().
                    text();
      $("#indexDeleteModal").modal('show');
    },
    deleteIndex: function () {
      var result = this.collectionModel.deleteIndex(this.lastId);
      if (result === true) {
        $(this.lastTarget.currentTarget).parent().parent().remove();
      }
      else {
        arangoHelper.arangoError("Could not delete index");
      }
      $("#indexDeleteModal").modal('hide');
    },
    selectIndexType: function () {
      $('.newIndexClass').hide();
      var type = $('#newIndexType').val();
      $('#newIndexType'+type).show();
    },
    checkboxToValue: function (id) {
      return $(id).prop('checked');
    },
    getIndex: function () {
      this.index = this.collectionModel.getIndex();
      var cssClass = 'collectionInfoTh modal-text';
      if (this.index) {
        var fieldString = '';
        var actionString = '';

        $.each(this.index.indexes, function(k, v) {
          if (v.type === 'primary' || v.type === 'edge') {
            actionString = '<span class="icon_arangodb_locked" ' +
              'data-original-title="No action"></span>';
          }
          else {
            actionString = '<span class="deleteIndex icon_arangodb_roundminus" ' +
              'data-original-title="Delete index" title="Delete index"></span>';
          }

          if (v.fields !== undefined) {
            fieldString = v.fields.join(", ");
          }

          //cut index id
          var position = v.id.indexOf('/');
          var indexId = v.id.substr(position + 1, v.id.length);
          var selectivity = (
            v.hasOwnProperty("selectivityEstimate") ? 
            (v.selectivityEstimate * 100).toFixed(2) + "%" : 
            "n/a"
          );
          var sparse = (v.hasOwnProperty("sparse") ? v.sparse : "n/a");

          $('#collectionEditIndexTable').append(
            '<tr>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + indexId + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + v.type + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + v.unique + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + sparse + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + selectivity + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + fieldString + '</th>' +
            '<th class=' + JSON.stringify(cssClass) + '>' + actionString + '</th>' +
            '</tr>'
          );
        });

        arangoHelper.fixTooltips("deleteIndex", "left");
      }
    }
  });
}());

/*jshint browser: true, evil: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, window*/

(function() {
  "use strict";

  window.EditListEntryView = Backbone.View.extend({

    template: templateEngine.createTemplate("editListEntryView.ejs"),

    initialize: function(opts) {
      this.key = opts.key;
      this.value = opts.value;
      this.render();
    },

    events: {
      "click .deleteAttribute": "removeRow"
    },

    render: function() {
      $(this.el).html(this.template.render({
        key: this.key,
        value: JSON.stringify(this.value),
        isReadOnly: this.isReadOnly()
      }));
    },

    isReadOnly: function() {
      return this.key.indexOf("_") === 0;
    },

    getKey: function() {
      return $(".key").val();
    },

    getValue: function() {
      var val = $(".val").val();
      try {
        val = JSON.parse(val);
      } catch (e) {
        try {
          eval("val = " + val);
          return val;
        } catch (e2) {
          return $(".val").val();
        }
      }
      return val;
    },

    removeRow: function() {
      this.remove();
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, arangoHelper, window*/

(function() {
  "use strict";
  window.FooterView = Backbone.View.extend({
    el: '#footerBar',
    system: {},
    isOffline: true,
    isOfflineCounter: 0,
    firstLogin: true,

    events: {
      'click .footer-center p' : 'showShortcutModal'
    },

    initialize: function () {
      //also server online check
      var self = this;
      window.setInterval(function(){
        self.getVersion();
      }, 15000);
      self.getVersion();
    },

    template: templateEngine.createTemplate("footerView.ejs"),

    showServerStatus: function(isOnline) {
      if (isOnline === true) {
        $('.serverStatusIndicator').addClass('isOnline');
        $('.serverStatusIndicator').addClass('fa-check-circle-o');
        $('.serverStatusIndicator').removeClass('fa-times-circle-o');
      }
      else {
        $('.serverStatusIndicator').removeClass('isOnline');
        $('.serverStatusIndicator').removeClass('fa-check-circle-o');
        $('.serverStatusIndicator').addClass('fa-times-circle-o');
      }
    },

    showShortcutModal: function() {
      window.arangoHelper.hotkeysFunctions.showHotkeysModal();
    },

    getVersion: function () {
      var self = this;

      // always retry this call, because it also checks if the server is online
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/version",
        contentType: "application/json",
        processData: false,
        async: true,
        success: function(data) {
          self.showServerStatus(true);
          if (self.isOffline === true) {
            self.isOffline = false;
            self.isOfflineCounter = 0;
            if (!self.firstLogin) {
              window.setTimeout(function(){
                self.showServerStatus(true);
              }, 1000);
            } else {
              self.firstLogin = false;
            }
            self.system.name = data.server;
            self.system.version = data.version;
            self.render();
          }
        },
        error: function (data) {
          self.isOffline = true;
          self.isOfflineCounter++;
          if (self.isOfflineCounter >= 1) {
            //arangoHelper.arangoError("Server", "Server is offline");
            self.showServerStatus(false);
          }
        }
      });

      if (! self.system.hasOwnProperty('database')) {
        $.ajax({
          type: "GET",
          cache: false,
          url: "/_api/database/current",
          contentType: "application/json",
          processData: false,
          async: true,
          success: function(data) {
            var name = data.result.name;
            self.system.database = name;

            var timer = window.setInterval(function () {
              var navElement = $('#databaseNavi');

              if (navElement) {
                window.clearTimeout(timer);
                timer = null;

                if (name === '_system') {
                  // show "logs" button
                  $('.logs-menu').css('visibility', 'visible');
                  $('.logs-menu').css('display', 'inline');
                  // show dbs menues
                  $('#databaseNavi').css('display','inline');
                }
                else {
                  // hide "logs" button
                  $('.logs-menu').css('visibility', 'hidden');
                  $('.logs-menu').css('display', 'none');
                }
                self.render();
              }
            }, 50);
          }
        });
      }
    },

    renderVersion: function () {
      if (this.system.hasOwnProperty('database') && this.system.hasOwnProperty('name')) {
        $(this.el).html(this.template.render({
          name: this.system.name,
          version: this.system.version,
          database: this.system.database
        }));
      }
    },

    render: function () {
      if (!this.system.version) {
        this.getVersion();
      }
      $(this.el).html(this.template.render({
        name: this.system.name,
        version: this.system.version
      }));
      return this;
    }

  });
}());

// obsolete file

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, EJS, arangoHelper, _, templateEngine, Joi*/

(function() {
  'use strict';

  window.FoxxActiveView = Backbone.View.extend({
    tagName: 'div',
    className: 'tile',
    template: templateEngine.createTemplate('foxxActiveView.ejs'),
    _show: true,

    events: {
      'click' : 'openAppDetailView'
    },

    openAppDetailView: function() {
      window.App.navigate('applications/' + encodeURIComponent(this.model.get('mount')), { trigger: true });
    },

    toggle: function(type, shouldShow) {
      switch (type) {
        case "devel":
          if (this.model.isDevelopment()) {
            this._show = shouldShow;
          }
          break;
        case "production":
          if (!this.model.isDevelopment() && !this.model.isSystem()) {
            this._show = shouldShow;
          }
          break;
        case "system":
          if (this.model.isSystem()) {
            this._show = shouldShow;
          }
          break;
        default:
      }
      if (this._show) {
        $(this.el).show();
      } else {
        $(this.el).hide();
      }
    },

    render: function(){
      $(this.el).html(this.template.render({
        model: this.model
      }));
      return $(this.el);
    }
  });
}());

/*jshint browser: true */
/*global require, $, Joi, _, alert, templateEngine*/
(function() {
  "use strict";

  var errors = require("internal").errors;
  var appStoreTemplate = templateEngine.createTemplate("applicationListView.ejs");

  var FoxxInstallView = function(opts) {
    this.collection = opts.collection;
  };

  var installCallback = function(result) {
    if (result.error === false) {
      this.collection.fetch({ async: false });
      window.modalView.hide();
      this.reload();
    } else {
      // TODO Error handling properly!
      switch(result.errorNum) {
        case errors.ERROR_APPLICATION_DOWNLOAD_FAILED.code:
          alert("Unable to download application from the given repository.");
          break;
        default:
          alert("Error: " + result.errorNum + ". " + result.errorMessage);
      }
    }
  };

  var setMountpointValidators = function() {
    window.modalView.modalBindValidation({
      id: "new-app-mount",
      validateInput: function() {
        return [
          {
            rule: Joi.string().regex(/^(\/(APP[^\/]+|(?!APP)[a-zA-Z0-9_\-%]+))+$/i),
            msg: "May not contain /APP"
          },
          {
            rule: Joi.string().regex(/^(\/[a-zA-Z0-9_\-%]+)+$/),
            msg: "Can only contain [a-zA-Z0-9_-%]"
          },
          {
            rule: Joi.string().regex(/^\/[^_]/),
            msg: "Mountpoints with _ are reserved for internal use"
          },
          {
            rule: Joi.string().regex(/[^\/]$/),
            msg: "May not end with /"
          },
          {
            rule: Joi.string().regex(/^\//),
            msg: "Has to start with /"
          },
          {
            rule: Joi.string().required().min(2),
            msg: "Has to be non-empty"
          },
        ];
      }
    });
  };

  var setGithubValidators = function() {
    window.modalView.modalBindValidation({
      id: "repository",
      validateInput: function() {
        return [
          {
            rule: Joi.string().required().regex(/^[a-zA-Z0-9_\-]+\/[a-zA-Z0-9_\-]+$/),
            msg: "No valid github account and repository."
          }
        ];
      }
    });
  };

  var setNewAppValidators = function() {
    window.modalView.modalBindValidation({
      id: "new-app-author",
      validateInput: function() {
        return [
          {
            rule: Joi.string().required().min(1),
            msg: "Has to be non empty."
          }
        ];
      }
    });
    window.modalView.modalBindValidation({
      id: "new-app-name",
      validateInput: function() {
        return [
          {
            rule: Joi.string().required().regex(/^[a-zA-Z\-_][a-zA-Z0-9\-_]*$/),
            msg: "Can only contain a to z, A to Z, 0-9, '-' and '_'."
          }
        ];
      }
    });

    window.modalView.modalBindValidation({
      id: "new-app-description",
      validateInput: function() {
        return [
          {
            rule: Joi.string().required().min(1),
            msg: "Has to be non empty."
          }
        ];
      }
    });

    window.modalView.modalBindValidation({
      id: "new-app-license",
      validateInput: function() {
        return [
          {
            rule: Joi.string().required().regex(/^[a-zA-Z0-9 \.,;\-]+$/),
            msg: "Has to be non empty."
          }
        ];
      }
    });
    window.modalView.modalTestAll();
  };

  var switchModalButton = function(event) {
    window.modalView.clearValidators();
    var openTab = $(event.currentTarget).attr("href").substr(1);
    var button = $("#modalButton1");
    if (!this._upgrade) {
      setMountpointValidators();
    }
    switch (openTab) {
      case "newApp":
        button.html("Generate");
        button.prop("disabled", false);
        setNewAppValidators();
        break;
      case "appstore":
        button.html("Install");
        button.prop("disabled", true);
        break;
      case "github":
        setGithubValidators();
        button.html("Install");
        button.prop("disabled", false);
        break;
      case "zip":
        button.html("Install");
        button.prop("disabled", false);
        break;
      default:
    }
  };

  var installFoxxFromStore = function(e) {
    if (window.modalView.modalTestAll()) {
      var mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop("checked");
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
      var toInstall = $(e.currentTarget).attr("appId");
      var version = $(e.currentTarget).attr("appVersion");
      if (flag !== undefined) {
        this.collection.installFromStore({name: toInstall, version: version}, mount, installCallback.bind(this), flag);
      } else {
        this.collection.installFromStore({name: toInstall, version: version}, mount, installCallback.bind(this));
      }
    }
  };

  var installFoxxFromZip = function(files, data) {
    if (window.modalView.modalTestAll()) {
      var mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop("checked");
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
      if (flag !== undefined) {
        this.collection.installFromZip(data.filename, mount, installCallback.bind(this), flag);
      } else {
        this.collection.installFromZip(data.filename, mount, installCallback.bind(this));
      }
    }
  };


  var installFoxxFromGithub = function() {
    if (window.modalView.modalTestAll()) {
      var url, version, mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop("checked");
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
      url = window.arangoHelper.escapeHtml($('#repository').val());
      version = window.arangoHelper.escapeHtml($('#tag').val());

      if (version === '') {
        version = "master";
      }
      var info = {
        url: window.arangoHelper.escapeHtml($('#repository').val()),
        version: window.arangoHelper.escapeHtml($('#tag').val())
      };

      try {
        Joi.assert(url, Joi.string().regex(/^[a-zA-Z0-9_\-]+\/[a-zA-Z0-9_\-]+$/));
      } catch (e) {
        return;
      }
      //send server req through collection
      if (flag !== undefined) {
        this.collection.installFromGithub(info, mount, installCallback.bind(this), flag);
      } else {
        this.collection.installFromGithub(info, mount, installCallback.bind(this));

      }
    }
  };

  var generateNewFoxxApp = function() {
    if (window.modalView.modalTestAll()) {
      var mount, flag;
      if (this._upgrade) {
        mount = this.mount;
        flag = $('#new-app-teardown').prop("checked");
      } else {
        mount = window.arangoHelper.escapeHtml($('#new-app-mount').val());
      }
      var info = {
        name: window.arangoHelper.escapeHtml($("#new-app-name").val()),
        collectionNames: _.map($('#new-app-collections').select2("data"), function(d) {
          return window.arangoHelper.escapeHtml(d.text);
        }),
        //        authenticated: window.arangoHelper.escapeHtml($("#new-app-name").val()),
        author: window.arangoHelper.escapeHtml($("#new-app-author").val()),
        license: window.arangoHelper.escapeHtml($("#new-app-license").val()),
        description: window.arangoHelper.escapeHtml($("#new-app-description").val())
      };
      if (flag !== undefined) {
        this.collection.generate(info, mount, installCallback.bind(this), flag);
      } else {
        this.collection.generate(info, mount, installCallback.bind(this));
      }
    }
  };

  var addAppAction = function() {
    var openTab = $(".modal-body .tab-pane.active").attr("id");
    switch (openTab) {
      case "newApp":
        generateNewFoxxApp.apply(this);
        break;
      case "github":
        installFoxxFromGithub.apply(this);
        break;
      case "zip":
        break;
      default:
    }
  };

  var render = function(scope, upgrade) {
    var buttons = [];
    var modalEvents = {
      "click #infoTab a"   : switchModalButton.bind(scope),
      "click .install-app" : installFoxxFromStore.bind(scope)
    };
    buttons.push(
      window.modalView.createSuccessButton("Generate", addAppAction.bind(scope))
    );
    window.modalView.show(
      "modalApplicationMount.ejs",
      "Install application",
      buttons,
      upgrade,
      undefined,
      modalEvents
    );
    $("#new-app-collections").select2({
      tags: [],
      showSearchBox: false,
      minimumResultsForSearch: -1,
      width: "336px"
    });
    $("#upload-foxx-zip").uploadFile({
      url: "/_api/upload?multipart=true",
      allowedTypes: "zip",
      onSuccess: installFoxxFromZip.bind(scope)
    });
    $.get("foxxes/fishbowl", function(list) {
      var table = $("#appstore-content");
      _.each(_.sortBy(list, "name"), function(app) {
        table.append(appStoreTemplate.render(app));
      });
    }).fail(function() {
      var table = $("#appstore-content");
      table.append("<tr><td>Store is not available. ArangoDB is not able to connect to github.com</td></tr>");
    });
  };

  FoxxInstallView.prototype.install = function(callback) {
    this.reload = callback;
    this._upgrade = false;
    delete this.mount;
    render(this, false);
    window.modalView.clearValidators();
    setMountpointValidators();
    setNewAppValidators();

  };

  FoxxInstallView.prototype.upgrade = function(mount, callback) {
    this.reload = callback;
    this._upgrade = true;
    this.mount = mount;
    render(this, true);
    window.modalView.clearValidators();
    setNewAppValidators();
  };

  window.FoxxInstallView = FoxxInstallView;
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, _, window, templateEngine, arangoHelper, GraphViewerUI, require */

(function() {
  "use strict";

  window.GraphManagementView = Backbone.View.extend({
    el: '#content',
    template: templateEngine.createTemplate("graphManagementView.ejs"),
    edgeDefintionTemplate: templateEngine.createTemplate("edgeDefinitionTable.ejs"),
    eCollList : [],
    removedECollList : [],

    dropdownVisible: false,

    events: {
      "click #deleteGraph"                        : "deleteGraph",
      "click .icon_arangodb_settings2.editGraph"  : "editGraph",
      "click #createGraph"                        : "addNewGraph",
      "keyup #graphManagementSearchInput"         : "search",
      "click #graphManagementSearchSubmit"        : "search",
      "click .tile-graph"                         : "loadGraphViewer",
      "click #graphManagementToggle"              : "toggleGraphDropdown",
      "click .css-label"                          : "checkBoxes",
      "change #graphSortDesc"                     : "sorting"
    },

    loadGraphViewer: function(e) {
      var name = $(e.currentTarget).attr("id");
      name = name.substr(0, name.length - 5);
      var edgeDefs = this.collection.get(name).get("edgeDefinitions");
      if (!edgeDefs || edgeDefs.length === 0) {
        // User Info
        return;
      }
      var adapterConfig = {
        type: "gharial",
        graphName: name,
        baseUrl: require("internal").arango.databasePrefix("/")
      };
      var width = $("#content").width() - 75;
      $("#content").html("");
      this.ui = new GraphViewerUI($("#content")[0], adapterConfig, width, 680, {
        nodeShaper: {
          label: "_key",
          color: {
            type: "attribute",
            key: "_key"
          }
        }

      }, true);

      //decorate hash. is needed for router url management
      window.location.hash = "#graphdetail";
    },

    handleResize: function(w) {
      if (!this.width || this.width !== w) {
        this.width = w;
        if (this.ui) {
          this.ui.changeWidth(w);
        }
      }
    },

    addNewGraph: function(e) {
      e.preventDefault();
      this.createEditGraphModal();
    },

    deleteGraph: function() {
      var self = this;
      var name = $("#editGraphName")[0].value;
      this.collection.get(name).destroy({
        success: function() {
          self.updateGraphManagementView();
          window.modalView.hide();
        },
        error: function(xhr, err) {
          var response = JSON.parse(err.responseText),
            msg = response.errorMessage;
          arangoHelper.arangoError(msg);
          window.modalView.hide();
        }
      });
    },

    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    toggleGraphDropdown: function() {
      //apply sorting to checkboxes
      $('#graphSortDesc').attr('checked', this.collection.sortOptions.desc);

      $('#graphManagementToggle').toggleClass('activated');
      $('#graphManagementDropdown2').slideToggle(200);
    },

    sorting: function() {
      if ($('#graphSortDesc').is(":checked")) {
        this.collection.setSortingDesc(true);
      }
      else {
        this.collection.setSortingDesc(false);
      }

      if ($('#graphManagementDropdown').is(":visible")) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }

      this.render();
    },

    render: function() {
      this.collection.fetch({
        async: false
      });

      this.collection.sort();

      $(this.el).html(this.template.render({
        graphs: this.collection,
        searchString : ''
      }));

      if (this.dropdownVisible === true) {
        $('#graphManagementDropdown2').show();
        $('#graphSortDesc').attr('checked', this.collection.sortOptions.desc);
        $('#graphManagementToggle').toggleClass('activated');
        $('#graphManagementDropdown').show();
      }

      this.events["click .tableRow"] = this.showHideDefinition.bind(this);
      this.events['change tr[id*="newEdgeDefinitions"]'] = this.setFromAndTo.bind(this);
      this.events["click .graphViewer-icon-button"] = this.addRemoveDefinition.bind(this);

      return this;
    },

    setFromAndTo : function (e) {
      e.stopPropagation();
      var map = this.calculateEdgeDefinitionMap(), id, i, tmp;

      if (e.added) {
        if (this.eCollList.indexOf(e.added.id) === -1 &&
          this.removedECollList.indexOf(e.added.id) !== -1) {
          id = e.currentTarget.id.split("row_newEdgeDefinitions")[1];
          $('input[id*="newEdgeDefinitions' + id  + '"]').select2("val", null);
          $('input[id*="newEdgeDefinitions' + id  + '"]').attr(
            "placeholder","The collection "+ e.added.id + " is already used."
          );
          return;
        }
        this.removedECollList.push(e.added.id);
        this.eCollList.splice(this.eCollList.indexOf(e.added.id),1);
      } else {
        this.eCollList.push(e.removed.id);
        this.removedECollList.splice(this.removedECollList.indexOf(e.removed.id),1);
      }

      if (map[e.val]) {
        id = e.currentTarget.id.split("row_newEdgeDefinitions")[1];
        $('#s2id_fromCollections'+id).select2("val", map[e.val].from);
        $('#fromCollections'+id).attr('disabled', true);
        $('#s2id_toCollections'+id).select2("val", map[e.val].to);
        $('#toCollections'+id).attr('disabled', true);
      } else {
        id = e.currentTarget.id.split("row_newEdgeDefinitions")[1];
        $('#s2id_fromCollections'+id).select2("val", null);
        $('#fromCollections'+id).attr('disabled', false);
        $('#s2id_toCollections'+id).select2("val", null);
        $('#toCollections'+id).attr('disabled', false);
      }
      /* following not needed? => destroys webif modal
      tmp = $('input[id*="newEdgeDefinitions"]');
      for (i = 0; i < tmp.length ; i++) {
        id = tmp[i].id;
        $('#' + id).select2({
          tags : this.eCollList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: "336px",
          maximumSelectionSize: 1
        });
      }*/
    },

    editGraph : function(e) {
      e.stopPropagation();
      this.collection.fetch();
      this.graphToEdit = this.evaluateGraphName($(e.currentTarget).attr("id"), '_settings');
      var graph = this.collection.findWhere({_key: this.graphToEdit});
      this.createEditGraphModal(
        graph
      );
    },


    saveEditedGraph: function() {
      var name = $("#editGraphName")[0].value,
        editedVertexCollections = _.pluck($('#newVertexCollections').select2("data"), "text"),
        edgeDefinitions = [],
        newEdgeDefinitions = {},
        collection,
        from,
        to,
        index,
        edgeDefinitionElements;

      edgeDefinitionElements = $('[id^=s2id_newEdgeDefinitions]').toArray();
      edgeDefinitionElements.forEach(
        function(eDElement) {
          index = $(eDElement).attr("id");
          index = index.replace("s2id_newEdgeDefinitions", "");
          collection = _.pluck($('#s2id_newEdgeDefinitions' + index).select2("data"), "text")[0];
          if (collection && collection !== "") {
            from = _.pluck($('#s2id_fromCollections' + index).select2("data"), "text");
            to = _.pluck($('#s2id_toCollections' + index).select2("data"), "text");
            if (from.length !== 0 && to.length !== 0) {
              var edgeDefinition = {
                collection: collection,
                from: from,
                to: to
              };
              edgeDefinitions.push(edgeDefinition);
              newEdgeDefinitions[collection] = edgeDefinition;
            }
          }
        }
      );
      //if no edge definition is left
      if (edgeDefinitions.length === 0) {
        $('#s2id_newEdgeDefinitions0 .select2-choices').css("border-color", "red");
        return;
      }

      //get current edgeDefs/orphanage
      var graph = this.collection.findWhere({_key: name});
      var currentEdgeDefinitions = graph.get("edgeDefinitions");
      var currentOrphanage = graph.get("orphanCollections");
      var currentCollections = [];

      //delete removed orphans
      currentOrphanage.forEach(
        function(oC) {
          if (editedVertexCollections.indexOf(oC) === -1) {
            graph.deleteVertexCollection(oC);
          }
        }
      );
      //add new orphans
      editedVertexCollections.forEach(
        function(vC) {
          if (currentOrphanage.indexOf(vC) === -1) {
            graph.addVertexCollection(vC);
          }
        }
      );

      //evaluate all new, edited and deleted edge definitions
      var newEDs = [];
      var editedEDs = [];
      var deletedEDs = [];


      currentEdgeDefinitions.forEach(
        function(eD) {
          var collection = eD.collection;
          currentCollections.push(collection);
          var newED = newEdgeDefinitions[collection];
          if (newED === undefined) {
            deletedEDs.push(collection);
          } else if (JSON.stringify(newED) !== JSON.stringify(eD)) {
            editedEDs.push(collection);
          }
        }
      );
      edgeDefinitions.forEach(
        function(eD) {
          var collection = eD.collection;
          if (currentCollections.indexOf(collection) === -1) {
            newEDs.push(collection);
          }
        }
      );



      newEDs.forEach(
        function(eD) {
          graph.addEdgeDefinition(newEdgeDefinitions[eD]);
        }
      );

      editedEDs.forEach(
        function(eD) {
          graph.modifyEdgeDefinition(newEdgeDefinitions[eD]);
        }
      );

      deletedEDs.forEach(
        function(eD) {
          graph.deleteEdgeDefinition(eD);
        }
      );
      this.updateGraphManagementView();
      window.modalView.hide();
    },

    evaluateGraphName : function(str, substr) {
      var index = str.lastIndexOf(substr);
      return str.substring(0, index);
    },

    search: function() {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#graphManagementSearchInput');
      searchString = $("#graphManagementSearchInput").val();
      reducedCollection = this.collection.filter(
        function(u) {
          return u.get("_key").indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        graphs        : reducedCollection,
        searchString  : searchString
      }));

      //after rendering, get the "new" element
      searchInput = $('#graphManagementSearchInput');
      //set focus on end of text in input field
      strLength= searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    updateGraphManagementView: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
        }
      });
    },

    createNewGraph: function() {
      var name = $("#createNewGraphName").val(),
        vertexCollections = _.pluck($('#newVertexCollections').select2("data"), "text"),
        edgeDefinitions = [],
        self = this,
        collection,
        from,
        to,
        index,
        edgeDefinitionElements;

      if (!name) {
        arangoHelper.arangoError(
          "A name for the graph has to be provided."
        );
        return 0;
      }

      if (this.collection.findWhere({_key: name})) {
        arangoHelper.arangoError(
          "The graph '" + name + "' already exists."
        );
        return 0;
      }

      edgeDefinitionElements = $('[id^=s2id_newEdgeDefinitions]').toArray();
      edgeDefinitionElements.forEach(
        function(eDElement) {
          index = $(eDElement).attr("id");
          index = index.replace("s2id_newEdgeDefinitions", "");
          collection = _.pluck($('#s2id_newEdgeDefinitions' + index).select2("data"), "text")[0];
          if (collection && collection !== "") {
            from = _.pluck($('#s2id_fromCollections' + index).select2("data"), "text");
            to = _.pluck($('#s2id_toCollections' + index).select2("data"), "text");
            if (from !== 1 && to !== 1) {
              edgeDefinitions.push(
                {
                  collection: collection,
                  from: from,
                  to: to
                }
              );
            }
          }
        }
      );

      this.collection.create({
        name: name,
        edgeDefinitions: edgeDefinitions,
        orphanCollections: vertexCollections
      }, {
        success: function() {
          self.updateGraphManagementView();
          window.modalView.hide();
        },
        error: function(obj, err) {
          var response = JSON.parse(err.responseText),
            msg = response.errorMessage;
          // Gritter does not display <>
          msg = msg.replace("<", "");
          msg = msg.replace(">", "");
          arangoHelper.arangoError(msg);
        }
      });
    },

    createEditGraphModal: function(graph) {
      var buttons = [],
          collList = [],
          tableContent = [],
          collections = this.options.collectionCollection.models,
          self = this,
          name = "",
          edgeDefinitions = [{collection : "", from : "", to :""}],
          orphanCollections = "",
          title,
          sorter = function(l, r) {
            l = l.toLowerCase();
            r = r.toLowerCase();
            if (l < r) {
              return -1;
            }
            if (l > r) {
              return 1;
            }
            return 0;
          };

      this.eCollList = [];
      this.removedECollList = [];

      collections.forEach(function (c) {
        if (c.get("isSystem")) {
          return;
        }
        collList.push(c.id);
        if (c.get('type') === "edge") {
          self.eCollList.push(c.id);
        }
      });
      window.modalView.enableHotKeys = false;
      this.counter = 0;

      if (graph) {
        title = "Edit Graph";

        name = graph.get("_key");
        edgeDefinitions = graph.get("edgeDefinitions");
        if (!edgeDefinitions || edgeDefinitions.length === 0 ) {
          edgeDefinitions = [{collection : "", from : "", to :""}];
        }
        orphanCollections = graph.get("orphanCollections");

        tableContent.push(
          window.modalView.createReadOnlyEntry(
            "editGraphName",
            "Name",
            name,
            "The name to identify the graph. Has to be unique"
          )
        );

        buttons.push(
          window.modalView.createDeleteButton("Delete", this.deleteGraph.bind(this))
        );
        buttons.push(
          window.modalView.createSuccessButton("Save", this.saveEditedGraph.bind(this))
        );

      } else {
        title = "Create Graph";

        tableContent.push(
          window.modalView.createTextEntry(
            "createNewGraphName",
            "Name",
            "",
            "The name to identify the graph. Has to be unique.",
            "graphName",
            true
          )
        );

        buttons.push(
          window.modalView.createSuccessButton("Create", this.createNewGraph.bind(this))
        );
      }

      edgeDefinitions.forEach(
        function(edgeDefinition) {
          if (self.counter  === 0) {
            if (edgeDefinition.collection) {
              self.removedECollList.push(edgeDefinition.collection);
              self.eCollList.splice(self.eCollList.indexOf(edgeDefinition.collection),1);
            }
            tableContent.push(
              window.modalView.createSelect2Entry(
                "newEdgeDefinitions" + self.counter,
                "Edge definitions",
                edgeDefinition.collection,
                "An edge definition defines a relation of the graph",
                "Edge definitions",
                true,
                false,
                true,
                1,
                self.eCollList.sort(sorter)
              )
            );
          } else {
            tableContent.push(
              window.modalView.createSelect2Entry(
                "newEdgeDefinitions" + self.counter,
                "Edge definitions",
                edgeDefinition.collection,
                "An edge definition defines a relation of the graph",
                "Edge definitions",
                false,
                true,
                false,
                1,
                self.eCollList.sort(sorter)
              )
            );
          }
          tableContent.push(
            window.modalView.createSelect2Entry(
              "fromCollections" + self.counter,
              "fromCollections",
              edgeDefinition.from,
              "The collections that contain the start vertices of the relation.",
              "fromCollections",
              true,
              false,
              false,
              10,
              collList.sort(sorter)
            )
          );
          tableContent.push(
            window.modalView.createSelect2Entry(
              "toCollections" + self.counter,
              "toCollections",
              edgeDefinition.to,
              "The collections that contain the end vertices of the relation.",
              "toCollections",
              true,
              false,
              false,
              10,
              collList.sort(sorter)
            )
          );
          self.counter++;
        }
      );

      tableContent.push(
        window.modalView.createSelect2Entry(
          "newVertexCollections",
          "Vertex collections",
          orphanCollections,
          "Collections that are part of a graph but not used in an edge definition",
          "Vertex Collections",
          false,
          false,
          false,
          10,
          collList.sort(sorter)
        )
      );

      window.modalView.show(
        "modalGraphTable.ejs", title, buttons, tableContent, null, this.events
      );

      if (graph) {
        var i;
        for (i = 0; i <= this.counter; i++) {
          $('#row_fromCollections' + i).hide();
          $('#row_toCollections' + i).hide();
        }
      }

    },

    showHideDefinition : function(e) {
      e.stopPropagation();
      var id = $(e.currentTarget).attr("id"), number;
      if (id.indexOf("row_newEdgeDefinitions") !== -1 ) {
        number = id.split("row_newEdgeDefinitions")[1];
        $('#row_fromCollections' + number).toggle();
        $('#row_toCollections' + number).toggle();
      }
    },

    addRemoveDefinition : function(e) {
      var collList = [],
        collections = this.options.collectionCollection.models;

      collections.forEach(function (c) {
        if (c.get("isSystem")) {
          return;
        }
        collList.push(c.id);
      });
      e.stopPropagation();
      var id = $(e.currentTarget).attr("id"), number;
      if (id.indexOf("addAfter_newEdgeDefinitions") !== -1 ) {
        this.counter++;
        $('#row_newVertexCollections').before(
          this.edgeDefintionTemplate.render({
            number: this.counter
          })
        );
        $('#newEdgeDefinitions'+this.counter).select2({
          tags: this.eCollList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: "336px",
          maximumSelectionSize: 1
        });
        $('#fromCollections'+this.counter).select2({
          tags: collList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: "336px",
          maximumSelectionSize: 10
        });
        $('#toCollections'+this.counter).select2({
          tags: collList,
          showSearchBox: false,
          minimumResultsForSearch: -1,
          width: "336px",
          maximumSelectionSize: 10
        });
        window.modalView.undelegateEvents();
        window.modalView.delegateEvents(this.events);
        return;
      }
      if (id.indexOf("remove_newEdgeDefinitions") !== -1 ) {
        number = id.split("remove_newEdgeDefinitions")[1];
        $('#row_newEdgeDefinitions' + number).remove();
        $('#row_fromCollections' + number).remove();
        $('#row_toCollections' + number).remove();
      }
    },

    calculateEdgeDefinitionMap : function () {
      var edgeDefinitionMap = {};
      this.collection.models.forEach(function(m) {
        m.get("edgeDefinitions").forEach(function (ed) {
          edgeDefinitionMap[ed.collection] = {
            from : ed.from,
            to : ed.to
          };
        });
      });
      return edgeDefinitionMap;
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, arangoHelper, window, setTimeout, $, templateEngine*/

(function() {
  "use strict";
  window.loginView = Backbone.View.extend({
    el: '#content',
    el2: '.header',
    el3: '.footer',

    init: function () {
    },

    events: {
      "click #submitLogin" : "login",
      "keydown #loginUsername" : "checkKey",
      "keydown #loginPassword" : "checkKey"
    },

    template: templateEngine.createTemplate("loginView.ejs"),

    render: function() {
      this.addDummyUser();

      $(this.el).html(this.template.render({}));
      $(this.el2).hide();
      $(this.el3).hide();

      $('#loginUsername').focus();

      //DEVELOPMENT
      $('#loginUsername').val('admin');
      $('#loginPassword').val('admin');

      return this;
    },

    addDummyUser: function () {
      this.collection.add({
        "userName" : "admin",
        "sessionId" : "abc123",
        "password" :"admin",
        "userId" : 1
      });
    },

    checkKey: function (e) {
      if (e.keyCode === 13) {
        this.login();
      }
    },

    login: function () {
      var username = $('#loginUsername').val();
      var password = $('#loginPassword').val();

      if (username === '' || password === '') {
        //Heiko: Form-Validator - please fill out all req. fields
        return;
      }
      var callback = this.collection.login(username, password);

      if (callback === true) {
        $(this.el2).show();
        $(this.el3).show();
        window.App.navigate("/", {trigger: true});
        $('#currentUser').text(username);
        this.collection.loadUserSettings();
      }

    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, arangoHelper, $, window, templateEngine*/

(function () {
  "use strict";

  window.LogsView = window.PaginationView.extend({

    el: '#content',
    id: '#logContent',
    paginationDiv: "#logPaginationDiv",
    idPrefix: "logTable",
    fetchedAmount: false,

    initialize: function () {
      this.convertModelToJSON();
    },

    currentLoglevel: "logall",

    events: {
      "click #arangoLogTabbar button": "setActiveLoglevel",
      "click #logTable_first": "firstPage",
      "click #logTable_last": "lastPage"
    },

    template: templateEngine.createTemplate("logsView.ejs"),
    tabbar: templateEngine.createTemplate("arangoTabbar.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),

    tabbarElements: {
      id: "arangoLogTabbar",
      titles: [
        ["Debug", "logdebug"],
        ["Warning", "logwarning"],
        ["Error", "logerror"],
        ["Info", "loginfo"],
        ["All", "logall"]
      ]
    },

    tableDescription: {
      id: "arangoLogTable",
      titles: ["Loglevel", "Date", "Message"],
      rows: []
    },

    convertedRows: null,

    setActiveLoglevel: function (e) {
      $('.arangodb-tabbar').removeClass('arango-active-tab');

      if (this.currentLoglevel !== e.currentTarget.id) {
        this.currentLoglevel = e.currentTarget.id;
        this.convertModelToJSON();
      }
    },

    initTotalAmount: function() {
      var self = this;
      this.collection = this.options[this.currentLoglevel];
      this.collection.fetch({
        data: $.param({ test: true}),
        success: function () {
          self.convertModelToJSON();
        }
      });
      this.fetchedAmount = true;
    },

    invertArray: function (array) {
      var rtnArr = [], counter = 0, i;
      for (i = array.length - 1; i >= 0; i--) {
        rtnArr[counter] = array[i];
        counter++;
      }
      return rtnArr;
    },

    convertModelToJSON: function () {
      if (!this.fetchedAmount) {
        this.initTotalAmount();
        return;
      }

      var self = this;
      var date;
      var rowsArray = [];
      this.collection = this.options[this.currentLoglevel];
      this.collection.fetch({
        success: function () {
          self.collection.each(function (model) {
            date = new Date(model.get('timestamp') * 1000);
            rowsArray.push([
              model.getLogStatus(),
              arangoHelper.formatDT(date),
              model.get('text')]);
          });
          self.tableDescription.rows = self.invertArray(rowsArray);
          //invert order
          self.render();
        }
      });
    },

    render: function () {
      $(this.el).html(this.template.render({}));
      $(this.id).html(this.tabbar.render({content: this.tabbarElements}));
      $(this.id).append(this.table.render({content: this.tableDescription}));
      $('#' + this.currentLoglevel).addClass('arango-active-tab');
      this.renderPagination();
      return this;
    },

    rerender: function () {
      this.convertModelToJSON();
    }
  });
}());


/*jshint browser: true */
/*global Backbone, $, window, setTimeout, Joi, _ */
/*global templateEngine*/

(function () {
  "use strict";

  var createButtonStub = function(type, title, cb) {
    return {
      type: type,
      title: title,
      callback: cb
    };
  };

  var createTextStub = function(type, label, value, info, placeholder, mandatory, joiObj,
                                addDelete, addAdd, maxEntrySize, tags) {
    var obj = {
      type: type,
      label: label
    };
    if (value !== undefined) {
      obj.value = value;
    }
    if (info !== undefined) {
      obj.info = info;
    }
    if (placeholder !== undefined) {
      obj.placeholder = placeholder;
    }
    if (mandatory !== undefined) {
      obj.mandatory = mandatory;
    }
    if (addDelete !== undefined) {
      obj.addDelete = addDelete;
    }
    if (addAdd !== undefined) {
      obj.addAdd = addAdd;
    }
    if (maxEntrySize !== undefined) {
      obj.maxEntrySize = maxEntrySize;
    }
    if (tags !== undefined) {
      obj.tags = tags;
    }
    if (joiObj){
      // returns true if the string contains the match
      obj.validateInput = function() {
        // return regexp.test(el.val());
        return joiObj;
      };
    }
    return obj;
  };

  window.ModalView = Backbone.View.extend({

    _validators: [],
    _validateWatchers: [],
    baseTemplate: templateEngine.createTemplate("modalBase.ejs"),
    tableTemplate: templateEngine.createTemplate("modalTable.ejs"),
    el: "#modalPlaceholder",
    contentEl: "#modalContent",
    hideFooter: false,
    confirm: {
      list: "#modal-delete-confirmation",
      yes: "#modal-confirm-delete",
      no: "#modal-abort-delete"
    },
    enabledHotkey: false,
    enableHotKeys : true,

    buttons: {
      SUCCESS: "success",
      NOTIFICATION: "notification",
      DELETE: "danger",
      NEUTRAL: "neutral",
      CLOSE: "close"
    },
    tables: {
      READONLY: "readonly",
      TEXT: "text",
      PASSWORD: "password",
      SELECT: "select",
      SELECT2: "select2",
      CHECKBOX: "checkbox"
    },
    closeButton: {
      type: "close",
      title: "Cancel"
    },

    initialize: function() {
      Object.freeze(this.buttons);
      Object.freeze(this.tables);
      var self = this;
      this.closeButton.callback = function() {
        self.hide();
      };
    },

    createModalHotkeys: function() {
      //submit modal
      $(this.el).bind('keydown', 'return', function(){
        $('.modal-footer .button-success').click();
      });
      $("input", $(this.el)).bind('keydown', 'return', function(){
        $('.modal-footer .button-success').click();
      });
      $("select", $(this.el)).bind('keydown', 'return', function(){
        $('.modal-footer .button-success').click();
      });
    },

    createInitModalHotkeys: function() {
      var self = this;
      //navigate through modal buttons
      //left cursor
      $(this.el).bind('keydown', 'left', function(){
        self.navigateThroughButtons('left');
      });
      //right cursor
      $(this.el).bind('keydown', 'right', function(){
        self.navigateThroughButtons('right');
      });

    },

    navigateThroughButtons: function(direction) {
      var hasFocus = $('.modal-footer button').is(':focus');
      if (hasFocus === false) {
        if (direction === 'left') {
          $('.modal-footer button').first().focus();
        }
        else if (direction === 'right') {
          $('.modal-footer button').last().focus();
        }
      }
      else if (hasFocus === true) {
        if (direction === 'left') {
          $(':focus').prev().focus();
        }
        else if (direction === 'right') {
          $(':focus').next().focus();
        }
      }

    },

    createCloseButton: function(cb) {
      var self = this;
      return createButtonStub(this.buttons.CLOSE, this.closeButton.title, function () {
          self.closeButton.callback();
          cb();
      });
    },

    createSuccessButton: function(title, cb) {
      return createButtonStub(this.buttons.SUCCESS, title, cb);
    },

    createNotificationButton: function(title, cb) {
      return createButtonStub(this.buttons.NOTIFICATION, title, cb);
    },

    createDeleteButton: function(title, cb) {
      return createButtonStub(this.buttons.DELETE, title, cb);
    },

    createNeutralButton: function(title, cb) {
      return createButtonStub(this.buttons.NEUTRAL, title, cb);
    },

    createDisabledButton: function(title) {
      var disabledButton = createButtonStub(this.buttons.NEUTRAL, title);
      disabledButton.disabled = true;
      return disabledButton;
    },

    createReadOnlyEntry: function(id, label, value, info, addDelete, addAdd) {
      var obj = createTextStub(this.tables.READONLY, label, value, info,undefined, undefined,
        undefined,addDelete, addAdd);
      obj.id = id;
      return obj;
    },

    createTextEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.TEXT, label, value, info, placeholder, mandatory,
                               regexp);
      obj.id = id;
      return obj;
    },

    createSelect2Entry: function(
      id, label, value, info, placeholder, mandatory, addDelete, addAdd, maxEntrySize, tags) {
      var obj = createTextStub(this.tables.SELECT2, label, value, info, placeholder,
        mandatory, undefined, addDelete, addAdd, maxEntrySize, tags);
      obj.id = id;
      return obj;
    },

    createPasswordEntry: function(id, label, value, info, placeholder, mandatory) {
      var obj = createTextStub(this.tables.PASSWORD, label, value, info, placeholder, mandatory);
      obj.id = id;
      return obj;
    },

    createCheckboxEntry: function(id, label, value, info, checked) {
      var obj = createTextStub(this.tables.CHECKBOX, label, value, info);
      obj.id = id;
      if (checked) {
        obj.checked = checked;
      }
      return obj;
    },

    createSelectEntry: function(id, label, selected, info, options) {
      var obj = createTextStub(this.tables.SELECT, label, null, info);
      obj.id = id;
      if (selected) {
        obj.selected = selected;
      }
      obj.options = options;
      return obj;
    },

    createOptionEntry: function(label, value) {
      return {
        label: label,
        value: value || label
      };
    },

    show: function(templateName, title, buttons, tableContent, advancedContent,
        events, ignoreConfirm) {
      var self = this, lastBtn, closeButtonFound = false;
      buttons = buttons || [];
      ignoreConfirm = ignoreConfirm || false;
      this.clearValidators();
      // Insert close as second from right
      if (buttons.length > 0) {
        buttons.forEach(function (b) {
            if (b.type === self.buttons.CLOSE) {
                closeButtonFound = true;
            }
        });
        if (!closeButtonFound) {
            lastBtn = buttons.pop();
            buttons.push(self.closeButton);
            buttons.push(lastBtn);
        }
      } else {
        buttons.push(this.closeButton);
      }
      $(this.el).html(this.baseTemplate.render({
        title: title,
        buttons: buttons,
        hideFooter: this.hideFooter
      }));
      _.each(buttons, function(b, i) {
        if (b.disabled || !b.callback) {
          return;
        }
        if (b.type === self.buttons.DELETE && !ignoreConfirm) {
          $("#modalButton" + i).bind("click", function() {
            $(self.confirm.yes).unbind("click");
            $(self.confirm.yes).bind("click", b.callback);
            $(self.confirm.list).css("display", "block");
          });
          return;
        }
        $("#modalButton" + i).bind("click", b.callback);
      });
      $(this.confirm.no).bind("click", function() {
        $(self.confirm.list).css("display", "none");
      });

      var template = templateEngine.createTemplate(templateName),
        model = {};
      model.content = tableContent || [];
      model.advancedContent = advancedContent || false;
      $(".modal-body").html(template.render(model));
      $('.modalTooltips').tooltip({
        position: {
          my: "left top",
          at: "right+55 top-1"
        }
      });
      var ind = buttons.indexOf(this.closeButton);
      buttons.splice(ind, 1);

      var completeTableContent = tableContent;
      try {
        _.each(advancedContent.content, function(x) {
          completeTableContent.push(x);
        });
      }
      catch(ignore) {
      }

      //handle select2
      _.each(completeTableContent, function(r) {
        if (r.type === self.tables.SELECT2) {
          $('#'+r.id).select2({
            tags: r.tags || [],
            showSearchBox: false,
            minimumResultsForSearch: -1,
            width: "336px",
            maximumSelectionSize: r.maxEntrySize || 8
          });
        }
      });//handle select2

      self.testInput = (function(){
        _.each(completeTableContent, function(r){
          self.modalBindValidation(r);
        });
      }());
      if (events) {
          this.events = events;
          this.delegateEvents();
      }

      $("#modal-dialog").modal("show");

      //enable modal hotkeys after rendering is complete
      if (this.enabledHotkey === false) {
        this.createInitModalHotkeys();
        this.enabledHotkey = true;
      }
      if (this.enableHotKeys) {
        this.createModalHotkeys();
      }

      //if input-field is available -> autofocus first one
      var focus = $('#modal-dialog').find('input');
      if (focus) {
        setTimeout(function() {
          var focus = $('#modal-dialog');
          if (focus.length > 0) {
            focus = focus.find('input'); 
              if (focus.length > 0) {
                $(focus[0]).focus();
              }
          }
        }, 800);
      }

    },

    modalBindValidation: function(entry) {
      var self = this;
      if (entry.hasOwnProperty("id")
        && entry.hasOwnProperty("validateInput")) {
        var validCheck = function() {
          var $el = $("#" + entry.id);
          var validation = entry.validateInput($el);
          var error = false, msg;
          _.each(validation, function(validator) {
            var schema = Joi.object().keys({
              toCheck: validator.rule
            });
            var valueToCheck = $el.val();
            Joi.validate(
              {
                toCheck: valueToCheck
              },
              schema,
              function (err) {
                if (err) {
                  msg = validator.msg;
                  error = true;
                }
              }
            );
          });
          if (error) {
            return msg;
          }
        };
        var $el = $('#' + entry.id);
        // catch result of validation and act
        $el.on('keyup focusout', function() {
          var msg = validCheck();
          var errorElement = $el.next()[0];
          if (msg !== undefined) {
            $el.addClass('invalid-input');
            if (errorElement) {
              //error element available
              $(errorElement).text(msg);
            }
            else {
              //error element not available
              $el.after('<p class="errorMessage">' + msg+ '</p>');
            }
            $('.modal-footer .button-success')
              .prop('disabled', true)
              .addClass('disabled');
          } else {
            $el.removeClass('invalid-input');
            if (errorElement) {
              $(errorElement).remove();
            }
            self.modalTestAll();
          }
        });
        this._validators.push(validCheck);
        this._validateWatchers.push($el);
      }
      
    },

    modalTestAll: function() {
      var tests = _.map(this._validators, function(v) {
        return v();
      });
      var invalid = _.any(tests);
      if (invalid) {
        $('.modal-footer .button-success')
          .prop('disabled', true)
          .addClass('disabled');
      } else {
        $('.modal-footer .button-success')
          .prop('disabled', false)
          .removeClass('disabled');
      }
      return !invalid;
    },

    clearValidators: function() {
      this._validators = [];
      _.each(this._validateWatchers, function(w) {
        w.unbind('keyup focusout');
      });
      this._validateWatchers = [];
    },

    hide: function() {
      this.clearValidators();
      $("#modal-dialog").modal("hide");
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, window, arangoHelper*/
(function () {
  "use strict";
  window.NavigationView = Backbone.View.extend({
    el: '#navigationBar',

    events: {
      "change #arangoCollectionSelect": "navigateBySelect",
      "click .tab": "navigateByTab",
      "mouseenter .dropdown": "showDropdown",
      "mouseleave .dropdown": "hideDropdown"
    },

    initialize: function () {
      this.userCollection = this.options.userCollection;
      this.currentDB = this.options.currentDB;
      this.dbSelectionView = new window.DBSelectionView({
        collection: this.options.database,
        current: this.currentDB
      });
      this.userBarView = new window.UserBarView({
        userCollection: this.userCollection
      });
      this.notificationView = new window.NotificationView({
        collection: this.options.notificationCollection
      });
      this.statisticBarView = new window.StatisticBarView({
          currentDB: this.currentDB
      });
      this.handleKeyboardHotkeys();
    },

    handleSelectDatabase: function () {
      this.dbSelectionView.render($("#dbSelect"));
    },

    template: templateEngine.createTemplate("navigationView.ejs"),

    render: function () {
      $(this.el).html(this.template.render({
        isSystem: this.currentDB.get("isSystem")
      }));
      this.dbSelectionView.render($("#dbSelect"));
      this.notificationView.render($("#notificationBar"));
      if (this.userCollection.whoAmI() !== null) {
        this.userBarView.render();
      }
      this.statisticBarView.render($("#statisticBar"));
      return this;
    },

    navigateBySelect: function () {
      var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    handleKeyboardHotkeys: function () {
      arangoHelper.enableKeyboardHotkeys(true);
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "") {
        navigateTo = $(tab).attr("class");
      }
      if (navigateTo === "links") {
        $("#link_dropdown").slideToggle(200);
        e.preventDefault();
        return;
      }
      if (navigateTo === "tools") {
        $("#tools_dropdown").slideToggle(200);
        e.preventDefault();
        return;
      }
      if (navigateTo === "dbselection") {
        $("#dbs_dropdown").slideToggle(200);
        e.preventDefault();
        return;
      }
      window.App.navigate(navigateTo, {trigger: true});
      e.preventDefault();
    },

    handleSelectNavigation: function () {
      var self = this;
      $("#arangoCollectionSelect").change(function() {
        self.navigateBySelect();
      });
    },

    selectMenuItem: function (menuItem) {
      $('.navlist li').removeClass('active');
      if (menuItem) {
        $('.' + menuItem).addClass('active');
      }
    },

    showDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "links" || navigateTo === "link_dropdown") {
        $("#link_dropdown").show(200);
        return;
      }
      if (navigateTo === "tools" || navigateTo === "tools_dropdown") {
        $("#tools_dropdown").show(200);
        return;
      }
      if (navigateTo === "dbselection" || navigateTo === "dbs_dropdown") {
        $("#dbs_dropdown").show(200);
        return;
      }
    },

    hideDropdown: function (e) {
      var tab = e.target || e.srcElement;
      tab = $(tab).closest(".dropdown");
      var navigateTo = tab.attr("id");
      if (navigateTo === "linkDropdown") {
        $("#link_dropdown").hide();
        return;
      }
      if (navigateTo === "toolsDropdown") {
        $("#tools_dropdown").hide();
        return;
      }
      if (navigateTo === "dbSelect") {
        $("#dbs_dropdown").hide();
        return;
      }
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, window*/
(function () {
  "use strict";

  window.NotificationView = Backbone.View.extend({

    events: {
      "click .navlogo #stat_hd"       : "toggleNotification",
      "click .notificationItem .fa"   : "removeNotification",
      "click #removeAllNotifications" : "removeAllNotifications"
    },

    initialize: function () {
      this.collection.bind("add", this.renderNotifications.bind(this));
      this.collection.bind("remove", this.renderNotifications.bind(this));
      this.collection.bind("reset", this.renderNotifications.bind(this));
    },

    notificationItem: templateEngine.createTemplate("notificationItem.ejs"),

    el: '#notificationBar',

    template: templateEngine.createTemplate("notificationView.ejs"),

    toggleNotification: function () {
      var counter = this.collection.length;
      if (counter !== 0) {
        $('#notification_menu').toggle();
      }
    },

    removeAllNotifications: function () {
      this.collection.reset();
      $('#notification_menu').hide();
    },

    removeNotification: function(e) {
      var cid = e.target.id;
      this.collection.get(cid).destroy();
    },

    renderNotifications: function() {
      $('#stat_hd_counter').text(this.collection.length);
      if (this.collection.length === 0) {
        $('#stat_hd').removeClass('fullNotification');
        $('#notification_menu').hide();
      }
      else {
        $('#stat_hd').addClass('fullNotification');
      }

      $('.innerDropdownInnerUL').html(this.notificationItem.render({
        notifications : this.collection
      }));
    },

    render: function () {
      $(this.el).html(this.template.render({
        notifications : this.collection
      }));

      this.renderNotifications();
      this.delegateEvents();
      return this.el;
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, setTimeout, Joi, _ */
/*global templateEngine*/

(function () {
  "use strict";

  window.ProgressView = Backbone.View.extend({

    template: templateEngine.createTemplate("progressBase.ejs"),

    el: "#progressPlaceholder",

    el2: "#progressPlaceholderIcon",

    toShow: false,

    action: function(){},

    events: {
      "click .progress-action button": "performAction",
    },

    performAction: function() {
      //this.action();
      window.progressView.hide();
    },

    initialize: function() {
    },

    showWithDelay: function(delay, msg, action, button) {
    var self = this;
      self.toShow = true;

      setTimeout(function(delay) {
        if (self.toShow === true) {
          self.show(msg, action, button);
        }
      }, delay);
    },

    show: function(msg, action, button) {
      $(this.el).html(this.template.render({}));
      $(".progress-text").text(msg);
      $(".progress-action").html('<button class="button-danger">Cancel</button>');
      this.action = this.hide();
      //$(".progress-action").html(button);
      //this.action = action;

      $(this.el).show();
      //$(this.el2).html('<i class="fa fa-spinner fa-spin"></i>');
    },

    hide: function() {
      $(this.el).hide();
      this.action = function(){};
      //$(this.el2).html('');
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _ */
/*global _, arangoHelper, templateEngine, jQuery, Joi*/

(function () {
  "use strict";
  window.queryManagementView = Backbone.View.extend({
    el: '#content',

    id: '#queryManagementContent',

    templateActive: templateEngine.createTemplate("queryManagementViewActive.ejs"),
    templateSlow: templateEngine.createTemplate("queryManagementViewSlow.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),
    tabbar: templateEngine.createTemplate("arangoTabbar.ejs"),

    initialize: function () {
      this.activeCollection = new window.QueryManagementActive();
      this.slowCollection = new window.QueryManagementSlow();
      this.convertModelToJSON(true);
    },

    events: {
      "click #arangoQueryManagementTabbar button" : "switchTab",
      "click #deleteSlowQueryHistory" : "deleteSlowQueryHistoryModal",
      "click #arangoQueryManagementTable .fa-minus-circle" : "deleteRunningQueryModal"
    },

    tabbarElements: {
      id: "arangoQueryManagementTabbar",
      titles: [
        ["Active", "activequeries"],
        ["Slow", "slowqueries"]
      ]
    },

    tableDescription: {
      id: "arangoQueryManagementTable",
      titles: ["ID", "Query String", "Runtime", "Started", ""],
      rows: [],
      unescaped: [false, false, false, false, true]
    },

    switchTab: function(e) {
      if (e.currentTarget.id === 'activequeries') {
        this.convertModelToJSON(true);
      }
      else if (e.currentTarget.id === 'slowqueries') {
        this.convertModelToJSON(false);
      }
    },

    deleteRunningQueryModal: function(e) {
      this.killQueryId = $(e.currentTarget).attr('data-id');
      var buttons = [], tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          undefined,
          "Running Query",
          'Do you want to kill the running query?',
          undefined,
          undefined,
          false,
          undefined
        )
      );

      buttons.push(
        window.modalView.createDeleteButton('Kill', this.killRunningQuery.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Kill Running Query',
        buttons,
        tableContent
      );

      $('.modal-delete-confirmation strong').html('Really kill?');

    },

    killRunningQuery: function() {
      this.collection.killRunningQuery(this.killQueryId, this.killRunningQueryCallback.bind(this));
      window.modalView.hide();
    },

    killRunningQueryCallback: function() {
      this.convertModelToJSON(true);
      this.renderActive();
    },

    deleteSlowQueryHistoryModal: function() {
      var buttons = [], tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry(
          undefined,
          "Slow Query Log",
          'Do you want to delete the slow query log entries?',
          undefined,
          undefined,
          false,
          undefined
        )
      );

      buttons.push(
        window.modalView.createDeleteButton('Delete', this.deleteSlowQueryHistory.bind(this))
      );

      window.modalView.show(
        'modalTable.ejs',
        'Delete Slow Query Log',
        buttons,
        tableContent
      );
    },

    deleteSlowQueryHistory: function() {
      this.collection.deleteSlowQueryHistory(this.slowQueryCallback.bind(this));
      window.modalView.hide();
    },

    slowQueryCallback: function() {
      this.convertModelToJSON(false);
      this.renderSlow();
    },

    render: function() {
      this.convertModelToJSON(true);
    },

    renderActive: function() {
      this.$el.html(this.templateActive.render({}));
      $(this.id).html(this.tabbar.render({content: this.tabbarElements}));
      $(this.id).append(this.table.render({content: this.tableDescription}));
      $('#activequeries').addClass("arango-active-tab");
    },

    renderSlow: function() {
      this.$el.html(this.templateSlow.render({}));
      $(this.id).html(this.tabbar.render({content: this.tabbarElements}));
      $(this.id).append(this.table.render({
        content: this.tableDescription,
      }));
      $('#slowqueries').addClass("arango-active-tab");
    },

    convertModelToJSON: function (active) {
      var self = this;
      var rowsArray = [];

      if (active === true) {
        this.collection = this.activeCollection;
      }
      else {
        this.collection = this.slowCollection;
      }

      this.collection.fetch({
        success: function () {
          self.collection.each(function (model) {

          var button = '';
            if (active) {
              button = '<i data-id="'+model.get('id')+'" class="fa fa-minus-circle"></i>';
            }
            rowsArray.push([
              model.get('id'),
              model.get('query'),
              model.get('runTime').toFixed(2) + ' s',
              model.get('started'),
              button
            ]);
          });

          var message = "No running queries.";
          if (!active) {
            message = "No slow queries.";
          }

          if (rowsArray.length === 0) {
            rowsArray.push([
              message,
              "",
              "",
              ""
            ]);
          }

          self.tableDescription.rows = rowsArray;

          if (active) {
            self.renderActive();
          }
          else {
            self.renderSlow();
          }
        }
      });

    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _, console */
/*global _, arangoHelper, templateEngine, jQuery, Joi, d3*/

(function () {
  "use strict";
  window.queryView = Backbone.View.extend({
    el: '#content',
    id: '#customsDiv',
    warningTemplate: templateEngine.createTemplate("warningList.ejs"),
    tabArray: [],
    execPending: false,

    initialize: function () {
      this.refreshAQL();
      this.tableDescription.rows = this.customQueries;
    },

    events: {
      "click #result-switch": "switchTab",
      "click #query-switch": "switchTab",
      'click #customs-switch': "switchTab",
//      'click #explain-switch': "switchTab",
      'click #submitQueryButton': 'submitQuery',
//      'click #explainQueryButton': 'explainQuery',
      'click #commentText': 'commentText',
      'click #uncommentText': 'uncommentText',
      'click #undoText': 'undoText',
      'click #redoText': 'redoText',
      'click #smallOutput': 'smallOutput',
      'click #bigOutput': 'bigOutput',
      'click #clearOutput': 'clearOutput',
      'click #clearInput': 'clearInput',
      'click #clearQueryButton': 'clearInput',
      'click #addAQL': 'addAQL',
      'mouseover #querySelect': function(){this.refreshAQL(true);},
      'change #querySelect': 'importSelected',
      'keypress #aqlEditor': 'aqlShortcuts',
      'click #arangoQueryTable .table-cell0': 'editCustomQuery',
      'click #arangoQueryTable .table-cell1': 'editCustomQuery',
      'click #arangoQueryTable .table-cell2 a': 'deleteAQL',
      'click #confirmQueryImport': 'importCustomQueries',
      'click #confirmQueryExport': 'exportCustomQueries',
      'click #downloadQueryResult': 'downloadQueryResult',
      'click #importQueriesToggle': 'showImportMenu'
    },

    showImportMenu: function(e) {
      $('#importQueriesToggle').toggleClass('activated');
      $('#importHeader').slideToggle(200);
    },

    createCustomQueryModal: function(){
      var buttons = [], tableContent = [];
      tableContent.push(
        window.modalView.createTextEntry(
          'new-query-name',
          'Name',
          '',
          undefined,
          undefined,
          false,
          [
            {
              rule: Joi.string().required(),
              msg: "No query name given."
            }
          ]
        )
      );
      buttons.push(
        window.modalView.createSuccessButton('Save', this.saveAQL.bind(this))
      );
      window.modalView.show('modalTable.ejs', 'Save Query', buttons, tableContent, undefined,
        {'keyup #new-query-name' : this.listenKey.bind(this)});
    },

    updateTable: function () {
      this.tableDescription.rows = this.customQueries;

      _.each(this.tableDescription.rows, function(k) {
        k.thirdRow = '<a class="deleteButton"><span class="icon_arangodb_roundminus"' +
                     ' title="Delete query"></span></a>';
      });

      // escape all columns but the third (which contains HTML)
      this.tableDescription.unescaped = [ false, false, true ];

      this.$(this.id).html(this.table.render({content: this.tableDescription}));
    },

    editCustomQuery: function(e) {
      var queryName = $(e.target).parent().children().first().text();
      var inputEditor = ace.edit("aqlEditor");
      inputEditor.setValue(this.getCustomQueryValueByName(queryName));
      this.deselect(inputEditor);
      $('#querySelect').val(queryName);
      this.switchTab("query-switch");
    },

    initTabArray: function() {
      var self = this;
      $(".arango-tab").children().each( function() {
        self.tabArray.push($(this).children().first().attr("id"));
      });
    },

    listenKey: function (e) {
      if (e.keyCode === 13) {
        this.saveAQL(e);
      }
      this.checkSaveName();
    },

    checkSaveName: function() {
      var saveName = $('#new-query-name').val();
      if ( saveName === "Insert Query"){
        $('#new-query-name').val('');
        return;
      }

      //check for invalid query names, if present change the box-shadow to red
      // and disable the save functionality
      var found = this.customQueries.some(function(query){
        return query.name === saveName;
      });
      if(found){
        $('#modalButton1').removeClass('button-success');
        $('#modalButton1').addClass('button-warning');
        $('#modalButton1').text('Update');
      } else {
        $('#modalButton1').removeClass('button-warning');
        $('#modalButton1').addClass('button-success');
        $('#modalButton1').text('Save');
      }
    },

    clearOutput: function () {
      var outputEditor = ace.edit("queryOutput");
      outputEditor.setValue('');
    },

    clearInput: function () {
      var inputEditor = ace.edit("aqlEditor");
      this.setCachedQuery(inputEditor.getValue());
      inputEditor.setValue('');
    },

    smallOutput: function () {
      var outputEditor = ace.edit("queryOutput");
      outputEditor.getSession().foldAll();
    },

    bigOutput: function () {
      var outputEditor = ace.edit("queryOutput");
      outputEditor.getSession().unfold();
    },

    aqlShortcuts: function (e) {
      if (e.ctrlKey && e.keyCode === 13) {
        this.submitQuery();
      }
      else if (e.metaKey && !e.ctrlKey && e.keyCode === 13) {
        this.submitQuery();
      }
    },

    queries: [
    ],

    customQueries: [],


    tableDescription: {
      id: "arangoQueryTable",
      titles: ["Name", "Content", ""],
      rows: []
    },

    template: templateEngine.createTemplate("queryView.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),

    render: function () {
      var self = this;
      this.$el.html(this.template.render({}));
      this.$(this.id).html(this.table.render({content: this.tableDescription}));
      // fill select box with # of results
      var querySize = 1000;

      var sizeBox = $('#querySize');
      sizeBox.empty();
      [ 100, 250, 500, 1000, 2500, 5000, 10000 ].forEach(function (value) {
        sizeBox.append('<option value="' + _.escape(value) + '"' +
          (querySize === value ? ' selected' : '') +
          '>' + _.escape(value) + ' results</option>');
      });

      var outputEditor = ace.edit("queryOutput");
      outputEditor.setReadOnly(true);
      outputEditor.setHighlightActiveLine(false);
      outputEditor.getSession().setMode("ace/mode/json");
      outputEditor.setFontSize("16px");
      outputEditor.setValue('');

      var inputEditor = ace.edit("aqlEditor");
      inputEditor.getSession().setMode("ace/mode/aql");
      inputEditor.setFontSize("16px");
      inputEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      //get cached query if available
      var query = this.getCachedQuery();
      if (query !== null && query !== undefined && query !== "") {
        inputEditor.setValue(query);
      }

      inputEditor.getSession().selection.on('changeCursor', function (e) {
        var inputEditor = ace.edit("aqlEditor");
        var session = inputEditor.getSession();
        var cursor = inputEditor.getCursorPosition();
        var token = session.getTokenAt(cursor.row, cursor.column);
        if (token) {
          if (token.type === "comment") {
            $("#commentText i")
            .removeClass("fa-comment")
            .addClass("fa-comment-o")
            .attr("data-original-title", "Uncomment");
          } else {
            $("#commentText i")
            .removeClass("fa-comment-o")
            .addClass("fa-comment")
            .attr("data-original-title", "Comment");
          }
        }
        //cache query in localstorage
        self.setCachedQuery(inputEditor.getValue());
      });
      $('#queryOutput').resizable({
        handles: "s",
        ghost: true,
        stop: function () {
          setTimeout(function () {
            var outputEditor = ace.edit("queryOutput");
            outputEditor.resize();
          }, 200);
        }
      });

      arangoHelper.fixTooltips(".queryTooltips, .icon_arangodb", "top");

      $('#aqlEditor .ace_text-input').focus();

      var windowHeight = $(window).height() - 295;
      $('#aqlEditor').height(windowHeight - 19);
      $('#queryOutput').height(windowHeight);

      inputEditor.resize();
      outputEditor.resize();

      this.initTabArray();
      this.renderSelectboxes();
      this.deselect(outputEditor);
      this.deselect(inputEditor);

      // Max: why do we need to tell those elements to show themselves?
      $("#queryDiv").show();
      $("#customsDiv").show();

      this.initQueryImport();

      this.switchTab('query-switch');
      return this;
    },

    getCachedQuery: function() {
      if(typeof(Storage) !== "undefined") {
        var cache = localStorage.getItem("cachedQuery");
        if (cache !== undefined) {
          var query = JSON.parse(cache);
          return query;
        }
      }
    },

    setCachedQuery: function(query) {
      if(typeof(Storage) !== "undefined") {
        localStorage.setItem("cachedQuery", JSON.stringify(query));
      }
    },

    initQueryImport: function () {
      var self = this;
      self.allowUpload = false;
      $('#importQueries').change(function(e) {
        self.files = e.target.files || e.dataTransfer.files;
        self.file = self.files[0];

        self.allowUpload = true;
      });
    },

    importCustomQueries: function () {
      var result, self = this;
      if (this.allowUpload === true) {

        var callback = function() {
          this.collection.fetch({async: false});
          this.updateLocalQueries();
          this.renderSelectboxes();
          this.updateTable();
          self.allowUpload = false;
          $('#customs-switch').click();
        };

        self.collection.saveImportQueries(self.file, callback.bind(this));
      }
    },

    downloadQueryResult: function() {
      var inputEditor = ace.edit("aqlEditor");
      var query = inputEditor.getValue();
      if (query !== '' || query !== undefined || query !== null) {
        window.open("query/result/download/" + encodeURIComponent(btoa(JSON.stringify({ query: query }))));
      }
      else {
        arangoHelper.arangoError("Query error", "could not query result.");
      }
    },

    exportCustomQueries: function () {
      var name, toExport = {}, exportArray = [];
      _.each(this.customQueries, function(value, key) {
        exportArray.push({name: value.name, value: value.value});
      });
      toExport = {
        "extra": {
          "queries": exportArray
        }
      };

      $.ajax("whoAmI", {async:false}).done(
        function(data) {
          name = data.name;

          if (name === null) {
            name = "root";
          }

        });

        window.open("query/download/" + encodeURIComponent(name));
      },

      deselect: function (editor) {
        var current = editor.getSelection();
        var currentRow = current.lead.row;
        var currentColumn = current.lead.column;

        current.setSelectionRange({
          start: {
            row: currentRow,
            column: currentColumn
          },
          end: {
            row: currentRow,
            column: currentColumn
          }
        });

        editor.focus();
      },

      addAQL: function () {
        //update queries first, before showing
        this.refreshAQL(true);

        //render options
        this.createCustomQueryModal();
        $('#new-query-name').val($('#querySelect').val());
        setTimeout(function () {
          $('#new-query-name').focus();
        }, 500);
        this.checkSaveName();
      },

      getAQL: function () {
        var self = this, result;

        this.collection.fetch({
          async: false
        });

        //old storage method
        var item = localStorage.getItem("customQueries");
        if (item) {
          var queries = JSON.parse(item);
          //save queries in user collections extra attribute
          _.each(queries, function(oldQuery) {
            self.collection.add({
              value: oldQuery.value,
              name: oldQuery.name
            });
          });
          result = self.collection.saveCollectionQueries();

          if (result === true) {
            //and delete them from localStorage
            localStorage.removeItem("customQueries");
          }
        }

        this.updateLocalQueries();
      },

      deleteAQL: function (e) {
        var deleteName = $(e.target).parent().parent().parent().children().first().text();

        var toDelete = this.collection.findWhere({name: deleteName});
        this.collection.remove(toDelete);
        this.collection.saveCollectionQueries();

        this.updateLocalQueries();
        this.renderSelectboxes();
        this.updateTable();
      },

      updateLocalQueries: function () {
        var self = this;
        this.customQueries = [];

        this.collection.each(function(model) {
          self.customQueries.push({
            name: model.get("name"),
            value: model.get("value")
          });
        });
      },

      saveAQL: function (e) {
        e.stopPropagation();

        //update queries first, before writing
        this.refreshAQL();

        var inputEditor = ace.edit("aqlEditor");
        var saveName = $('#new-query-name').val();
        var isUpdate = $('#modalButton1').text() === 'Update';

        if ($('#new-query-name').hasClass('invalid-input')) {
          return;
        }

        //Heiko: Form-Validator - illegal query name
        if (saveName.trim() === '') {
          return;
        }

        var content = inputEditor.getValue();
        //check for already existing entry
        var quit = false;
        $.each(this.customQueries, function (k, v) {
          if (v.name === saveName) {
            v.value = content;
            quit = !isUpdate;
            return;
          }
        });

        if (quit === true) {
          //Heiko: Form-Validator - name already taken
          window.modalView.hide();
          return;
        }

        if (! isUpdate) {
          this.collection.add({
            name: saveName,
            value: content
          });
        }
        else {
          var toModifiy = this.collection.findWhere({name: saveName});
          toModifiy.set("value", content);
        }
        this.collection.saveCollectionQueries();

        window.modalView.hide();

        this.updateLocalQueries();
        this.renderSelectboxes();
        $('#querySelect').val(saveName);
      },

      getSystemQueries: function () {
        var self = this;
        $.ajax({
          type: "GET",
          cache: false,
          url: "js/arango/aqltemplates.json",
          contentType: "application/json",
          processData: false,
          async: false,
          success: function (data) {
            self.queries = data;
          },
          error: function (data) {
            arangoHelper.arangoNotification("Query", "Error while loading system templates");
          }
        });
      },

      getCustomQueryValueByName: function (qName) {
        var returnVal;
        $.each(this.customQueries, function (k, v) {
          if (qName === v.name) {
            returnVal = v.value;
          }
        });
        return returnVal;
      },

      refreshAQL: function(select) {
        this.getAQL();
        this.getSystemQueries();
        this.updateLocalQueries();

        if (select) {
          var previous = $("#querySelect" ).val();
          this.renderSelectboxes();
          $("#querySelect" ).val(previous);
        }
      },

      importSelected: function (e) {
        var inputEditor = ace.edit("aqlEditor");
        $.each(this.queries, function (k, v) {
          if ($('#' + e.currentTarget.id).val() === v.name) {
            inputEditor.setValue(v.value);
          }
        });
        $.each(this.customQueries, function (k, v) {
          if ($('#' + e.currentTarget.id).val() === v.name) {
            inputEditor.setValue(v.value);
          }
        });

        this.deselect(ace.edit("aqlEditor"));
      },

      renderSelectboxes: function () {
        this.sortQueries();
        var selector = '';
        selector = '#querySelect';
        $(selector).empty();

        $(selector).append('<option id="emptyquery">Insert Query</option>');
        $(selector).append('<optgroup label="Example queries">');
        jQuery.each(this.queries, function (k, v) {
          $(selector).append('<option id="' + _.escape(v.name) + '">' + _.escape(v.name) + '</option>');
        });
        $(selector).append('</optgroup>');

        if (this.customQueries.length > 0) {
          $(selector).append('<optgroup label="Custom queries">');
          jQuery.each(this.customQueries, function (k, v) {
            $(selector).append('<option id="' + _.escape(v.name) + '">' + _.escape(v.name) + '</option>');
          });
          $(selector).append('</optgroup>');
        }
      },
      undoText: function () {
        var inputEditor = ace.edit("aqlEditor");
        inputEditor.undo();
      },
      redoText: function () {
        var inputEditor = ace.edit("aqlEditor");
        inputEditor.redo();
      },
      commentText: function () {
        var inputEditor = ace.edit("aqlEditor");
        inputEditor.toggleCommentLines();
      },
      sortQueries: function () {
        this.queries = _.sortBy(this.queries, 'name');
        this.customQueries = _.sortBy(this.customQueries, 'name');
      },

      abortQuery: function () {
        /*
         $.ajax({
           type: "DELETE",
           url: "/_api/cursor/currentFrontendQuery",
           contentType: "application/json",
           processData: false,
           success: function (data) {
           },
           error: function (data) {
           }
         });
         */
      },

      readQueryData: function() {
        var inputEditor = ace.edit("aqlEditor");
        var selectedText = inputEditor.session.getTextRange(inputEditor.getSelectionRange());
        var sizeBox = $('#querySize');
        var data = {
          query: selectedText || inputEditor.getValue(),
          batchSize: parseInt(sizeBox.val(), 10),
          id: "currentFrontendQuery"
        };
        return JSON.stringify(data);
      },

      heatmapColors: [
        "#313695",
        "#4575b4",
        "#74add1",
        "#abd9e9",
        "#e0f3f8",
        "#ffffbf",
        "#fee090",
        "#fdae61",
        "#f46d43",
        "#d73027",
        "#a50026",
      ],

      heatmap: function(value) {
        return this.heatmapColors[Math.floor(value * 10)];
      },

      followQueryPath: function(root, nodes) {
        var known = {};
        var estCost = 0;
        known[nodes[0].id] = root;
        var i, nodeData, j, dep;
        for (i = 1; i < nodes.length; ++i) {
          nodeData = this.preparePlanNodeEntry(nodes[i], nodes[i-1].estimatedCost);
          known[nodes[i].id] = nodeData;
          dep = nodes[i].dependencies;
          estCost = nodeData.estimatedCost;
          for (j = 0; j < dep.length; ++j) {
            known[dep[j]].children.push(nodeData);
          }
        }
        return estCost;
      },

      preparePlanNodeEntry: function(node, parentCost) {
        var json = {
          estimatedCost: node.estimatedCost,
          estimatedNrItems: node.estimatedNrItems,
          type: node.type,
          children: []
        };
        switch (node.type) {
          case "SubqueryNode":
            json.relativeCost =  json.estimatedCost - this.followQueryPath(json, node.subquery.nodes);
            break;
          default:
            json.relativeCost = json.estimatedCost - parentCost|| json.estimatedCost;

        }
        return json;
      },
      /*
      drawTree: function() {
        var treeHeight = 0;
        var heatmap = this.heatmap.bind(this);
        if (!this.treeData) {
          return;
        }
        var treeData = this.treeData;
        // outputEditor.setValue(JSON.stringify(treeData, undefined, 2));

        var margin = {top: 20, right: 20, bottom: 20, left: 20},
            width = $("svg#explainOutput").parent().width() - margin.right - margin.left,
            height = 500 - margin.top - margin.bottom;

        var i = 0;
        var maxCost = 0;

        var tree = d3.layout.tree().size([width, height]);

        d3.select("svg#explainOutput g").remove();

        var svg = d3.select("svg#explainOutput")
          .attr("width", width + margin.right + margin.left)
          .attr("height", height + margin.top + margin.bottom)
          .append("g")
          .attr("transform", "translate(" + margin.left + "," + margin.top + ")");

        var root = treeData[0];

        // Compute the new tree layout.
        var nodes = tree.nodes(root).reverse(),
            links = tree.links(nodes);

        var diagonal = d3.svg.diagonal()
          .projection(function(d) { return [d.x, d.y]; });

        // Normalize for fixed-depth.
        nodes.forEach(function(d) { d.y = d.depth * 80 + margin.top; });

        // Declare the nodes¦
        var node = svg.selectAll("g.node")
          .data(nodes, function(d) {
            if (!d.id) {
              d.id = ++i;
            }
            if (treeHeight < d.y) {
              treeHeight = d.y;
            }
            if (maxCost < d.relativeCost) {
              maxCost = d.relativeCost;
            }
            return d.id;
          });

          treeHeight += 60;
          $(".query-output").height(treeHeight);

        // Enter the nodes.
        var nodeEnter = node.enter().append("g")
          .attr("class", "node")
          .attr("transform", function(d) { 
            return "translate(" + d.x + "," + d.y + ")"; });

        nodeEnter.append("circle")
          .attr("r", 10)
          .style("fill", function(d) {return heatmap(d.relativeCost / maxCost);});

        nodeEnter.append("text")
          .attr("dx", "0")
          .attr("dy", "-15")
          .attr("text-anchor", function() {return "middle"; })
          .text(function(d) { return d.type.replace("Node",""); })
          .style("fill-opacity", 1);

        nodeEnter.append("text")
          .attr("dx", "0")
          .attr("dy", "25")
          .attr("text-anchor", function() { return "middle"; })
          .text(function(d) { return "Cost: " + d.relativeCost; })
          .style("fill-opacity", 1);

        // Declare the links¦
        var link = svg.selectAll("path.link")
          .data(links, function(d) { return d.target.id; });

        // Enter the links.
        link.enter().insert("path", "g")
          .attr("class", "link")
          .attr("d", diagonal);

      },
      */

      resize: function() {
        // this.drawTree();
      },

      /*
      showExplainPlan: function(plan) {
        $("svg#explainOutput").html();
        var nodes = plan.nodes;
        if (nodes.length > 0) {
          // Handle root element differently
          var treeData = this.preparePlanNodeEntry(nodes[0]);
          this.followQueryPath(treeData, nodes);
          this.treeData = [treeData];
          this.drawTree();
        }
      },
      */

     /*
      showExplainWarnings: function(warnings) {
        $(".explain-warnings").html(this.warningTemplate.render({warnings: warnings}));
      },
      */

      /*
      fillExplain: function(callback) {
        var self = this;
        $("svg#explainOutput").html();
        $.ajax({
          type: "POST",
          url: "/_api/explain",
          data: this.readQueryData(),
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (typeof callback === "function") {
              callback();
            }
            self.showExplainWarnings(data.warnings);
            self.showExplainPlan(data.plan);
          },
          error: function (errObj) {
            var res = errObj.responseJSON;
            // Display ErrorMessage
            console.log("Error:", res.errorMessage);
          }
        });
      },
      */

      fillResult: function(callback) {
        var self = this;
        var outputEditor = ace.edit("queryOutput");
        // clear result
        outputEditor.setValue('');
        window.progressView.show(
          "Query is operating..."
        );
        this.execPending = false;
        $.ajax({
          type: "POST",
          url: "/_api/cursor",
          data: this.readQueryData(),
          contentType: "application/json",
          processData: false,
          success: function (data) {
            var warnings = "";
            if (data.extra && data.extra.warnings && data.extra.warnings.length > 0) {
              warnings += "Warnings:" + "\r\n\r\n";
              data.extra.warnings.forEach(function(w) {
                warnings += "[" + w.code + "], '" + w.message + "'\r\n";
              });
            }
            if (warnings !== "") {
              warnings += "\r\n" + "Result:" + "\r\n\r\n";
            }
            outputEditor.setValue(warnings + JSON.stringify(data.result, undefined, 2));
            self.switchTab("result-switch");
            window.progressView.hide();
            self.deselect(outputEditor);
            $('#downloadQueryResult').show();
            if (typeof callback === "function") {
              callback();
            }
          },
          error: function (data) {
            self.switchTab("result-switch");
            $('#downloadQueryResult').hide();
            try {
              var temp = JSON.parse(data.responseText);
              outputEditor.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);
              arangoHelper.arangoError("Query error", temp.errorNum);
            }
            catch (e) {
              outputEditor.setValue('ERROR');
              arangoHelper.arangoError("Query error", "ERROR");
            }
            window.progressView.hide();
            if (typeof callback === "function") {
              callback();
            }
          }
        });
      },

      submitQuery: function () {
        var outputEditor = ace.edit("queryOutput");
        // this.fillExplain();
        this.fillResult(this.switchTab.bind(this, "result-switch"));
        outputEditor.resize();
        var inputEditor = ace.edit("aqlEditor");
        this.deselect(inputEditor);
      },

      /*
      explainQuery: function() {
        this.fillExplain(this.switchTab.bind(this, "explain-switch"));
        this.execPending = true;
        var inputEditor = ace.edit("aqlEditor");
        this.deselect(inputEditor);
      },
      */

      // This function changes the focus onto the tab that has been clicked
      // it can be given an event-object or the id of the tab to switch to
      //    e.g. switchTab("result-switch");
      // note that you need to ommit the #
      switchTab: function (e) {
        // defining a callback function for Array.forEach() the tabArray holds the ids of
        // the tabs a-tags, from which we can create the appropriate content-divs ids.
        // The convention is #result-switch (a-tag), #result (content-div), and
        // #tabContentResult (pane-div).
        // We set the clicked element's tags to active/show and the others to hide.
        var switchId;
        if (typeof e === 'string') {
          switchId = e;
        } else {
          switchId = e.target.id;
        }
        var self = this;
        var changeTab = function (element){
          var divId = "#" + element.replace("-switch", "");
          var contentDivId = "#tabContent" + divId.charAt(1).toUpperCase() + divId.substr(2);
          if (element === switchId) {
            $("#" + element).parent().addClass("active");
            $(divId).addClass("active");
            $(contentDivId).show();
            if (switchId === 'query-switch') {
              // issue #1000: set focus to query input
              $('#aqlEditor .ace_text-input').focus();
            } else if (switchId === 'result-switch' && self.execPending) {
              self.fillResult();
            }
          } else {
            $("#" + element).parent().removeClass("active");
            $(divId).removeClass("active");
            $(contentDivId).hide();
          }
        };
        this.tabArray.forEach(changeTab);
        this.updateTable();
      }
    });
  }());

/*jshint browser: true, evil: true */
/*jshint unused: false */
/*global require, exports, Backbone, EJS, $, window, ace, jqconsole, handler, help, location*/
/*global templateEngine*/

(function() {
  "use strict";
  window.shellView = Backbone.View.extend({
    resizing: false,

    el: '#content',

    template: templateEngine.createTemplate("shellView.ejs"),

    render: function() {
      $(this.el).html(this.template.render({}));

      this.replShell();

      $("#shell_workspace").trigger("resize", [ 150 ]);

      this.resize();

      // evil: the resize event is globally bound to window, but there is
      // no elegant alternative... (is there?)
      var self = this;
      $(window).resize(function () {
        self.resize();
      });

      this.executeJs("start_pretty_print()");

      return this;
    },

    resize: function () {
      // prevent endless recursion
      if (! this.resizing) {
        this.resizing = true;
        var windowHeight = $(window).height() - 250;
        $('#shell_workspace').height(windowHeight);
        this.resizing = false;
      }
    },

    executeJs: function (data) {
      var internal = require("internal");
      try {
        var result = window.eval(data);
        if (result !== undefined) {
          internal.browserOutputBuffer = "";
          internal.printShell.apply(internal.printShell, [ result ]);
          jqconsole.Write('==> ' + internal.browserOutputBuffer + '\n', 'jssuccess');
        }
        internal.browserOutputBuffer = "";
      } catch (e) {
        if (e instanceof internal.ArangoError) {
          if (e.hasOwnProperty("errorMessage")) {
            jqconsole.Write(e.errorMessage + '\n', 'jserror');
          }
          else {
            jqconsole.Write(e.message + '\n', 'jserror');
          }
        }
        else {
          jqconsole.Write(e.name + ': ' + e.message + '\n', 'jserror');
        }
      }
    },

    replShellPromptHelper: function(command) {
      // Continue line if can't compile the command.
      try {
        var f = new Function(command);
      }
      catch (e) {
        if (/[\[\{\(]$/.test(command)) {
          return 1;
        }
        return 0;
      }
      return false;
    },

    replShellHandlerHelper: function(command) {

    },

    replShell: function () {
      var self = this;
      // Creating the console.
      var internal = require("internal");
      var client = require("org/arangodb/arangosh");
      var header = 'Welcome to arangosh. Copyright (c) ArangoDB GmbH\n';
      window.jqconsole = $('#replShell').jqconsole(header, 'JSH> ', "...>");
      this.executeJs(internal.print(client.HELP));
      // Abort prompt on Ctrl+Z.
      jqconsole.RegisterShortcut('Z', function() {
        jqconsole.AbortPrompt();
        handler();
      });
      // Move to line end Ctrl+E.
      jqconsole.RegisterShortcut('E', function() {
        jqconsole.MoveToEnd();
        handler();
      });
      jqconsole.RegisterMatching('{', '}', 'brace');
      jqconsole.RegisterMatching('(', ')', 'paren');
      jqconsole.RegisterMatching('[', ']', 'bracket');

      // Handle a command.
      var handler = function(command) {
        if (command === 'help') {
          //command = "require(\"arangosh\").HELP";
          command = help();
        }
        if (command === "exit") {
          location.reload();
        }

        self.executeJs(command);
        jqconsole.Prompt(true, handler, self.replShellPromptHelper(command));
      };

      // Initiate the first prompt.
      handler();
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, window*/
(function () {
  "use strict";
  window.StatisticBarView = Backbone.View.extend({
    el: '#statisticBar',

    events: {
      "change #arangoCollectionSelect": "navigateBySelect",
      "click .tab": "navigateByTab"
    },

    template: templateEngine.createTemplate("statisticBarView.ejs"),

    initialize : function () {
      this.currentDB = this.options.currentDB;
    },

    replaceSVG: function($img) {
      var imgID = $img.attr('id');
      var imgClass = $img.attr('class');
      var imgURL = $img.attr('src');

      $.get(imgURL, function(data) {
        // Get the SVG tag, ignore the rest
        var $svg = $(data).find('svg');

        // Add replaced image's ID to the new SVG
        if(imgID === undefined) {
          $svg = $svg.attr('id', imgID);
        }
        // Add replaced image's classes to the new SVG
        if(imgClass === undefined) {
          $svg = $svg.attr('class', imgClass+' replaced-svg');
        }

        // Remove any invalid XML tags as per http://validator.w3.org
        $svg = $svg.removeAttr('xmlns:a');

        // Replace image with new SVG
        $img.replaceWith($svg);

      }, 'xml');
    },

    render: function () {
      var self = this;
      $(this.el).html(this.template.render({
        isSystem: this.currentDB.get("isSystem")
      }));

      $('img.svg').each(function() {
        self.replaceSVG($(this));
      });
      return this;
    },

    navigateBySelect: function () {
      var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "links") {
        $("#link_dropdown").slideToggle(200);
        e.preventDefault();
        return;
      }
      if (navigateTo === "tools") {
        $("#tools_dropdown").slideToggle(200);
        e.preventDefault();
        return;
      }
      window.App.navigate(navigateTo, {trigger: true});
      e.preventDefault();
    },
    handleSelectNavigation: function () {
      $("#arangoCollectionSelect").change(function () {
        var navigateTo = $(this).find("option:selected").val();
        window.App.navigate(navigateTo, {trigger: true});
      });
    },


    selectMenuItem: function (menuItem) {
      $('.navlist li').removeClass('active');
      if (menuItem) {
        $('.' + menuItem).addClass('active');
      }
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, window, templateEngine, $ */

(function() {
  "use strict";

  window.TableView = Backbone.View.extend({

    template: templateEngine.createTemplate("tableView.ejs"),
    loading: templateEngine.createTemplate("loadingTableView.ejs"),

    initialize: function() {
      this.rowClickCallback = this.options.rowClick;
    },

    events: {
      "click tbody tr": "rowClick",
      "click .deleteButton": "removeClick",
    },

    rowClick: function(event) {
      if (this.hasOwnProperty("rowClickCallback")) {
        this.rowClickCallback(event); 
      }
    },

    removeClick: function(event) {
      if (this.hasOwnProperty("removeClickCallback")) {
        this.removeClickCallback(event); 
        event.stopPropagation();
      }
    },

    setRowClick: function(callback) {
      this.rowClickCallback = callback;
    },

    setRemoveClick: function(callback) {
      this.removeClickCallback = callback;
    },

    render: function() {
      $(this.el).html(this.template.render({
        docs: this.collection
      }));
    },

    drawLoading: function() {
      $(this.el).html(this.loading.render({}));
    }

  });

}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, _, window, document, templateEngine, FileReader */

(function() {
  "use strict";

  window.testView = Backbone.View.extend({
    el: '#content',

    events: {
      "change #fileInput" : "readJSON"
    },

    template: templateEngine.createTemplate("testView.ejs"),

    readJSON: function() {
      var fileInput = document.getElementById('fileInput');
      var file = fileInput.files[0];
      var textType = 'application/json';

      if (file.type.match(textType)) {
        var reader = new FileReader();

        reader.onload = function(e) {
          $('#fileDisplayArea pre').text(reader.result);
        };

        reader.readAsText(file);
      }
      else {
        $('#fileDisplayArea pre').text("File not supported!");
      }
    },

    render: function() {
      $(this.el).html(this.template.render());
      return this;
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, window*/
(function () {
  "use strict";

  window.UserBarView = Backbone.View.extend({

    events: {
      "change #userBarSelect"         : "navigateBySelect",
      "click .tab"                    : "navigateByTab",
      "mouseenter .dropdown"          : "showDropdown",
      "mouseleave .dropdown"          : "hideDropdown",
      "click #userLogout"             : "userLogout"
    },

    initialize: function () {
      this.userCollection = this.options.userCollection;
      this.userCollection.fetch({async:false});
      this.userCollection.bind("change:extra", this.render.bind(this));
    },

    template: templateEngine.createTemplate("userBarView.ejs"),

    navigateBySelect: function () {
      var navigateTo = $("#arangoCollectionSelect").find("option:selected").val();
      window.App.navigate(navigateTo, {trigger: true});
    },

    navigateByTab: function (e) {
      var tab = e.target || e.srcElement;
      tab = $(tab).closest("a");
      var navigateTo = tab.attr("id");
      if (navigateTo === "user") {
        $("#user_dropdown").slideToggle(200);
        e.preventDefault();
        return;
      }
      window.App.navigate(navigateTo, {trigger: true});
      e.preventDefault();
    },

    showDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "user") {
        $("#user_dropdown").show(200);
        return;
      }
    },

    hideDropdown: function (e) {
      $("#user_dropdown").hide();
    },

    render: function (el) {
      var username = this.userCollection.whoAmI(),
        img = null,
        name = null,
        active = false,
        currentUser = null;
      if (username !== null) {
        currentUser = this.userCollection.findWhere({user: username});
        currentUser.set({loggedIn : true});
        name = currentUser.get("extra").name;
        img = currentUser.get("extra").img;
        active = currentUser.get("active");
      }
      if (! img) {
        img = "img/arangodblogoAvatar_24.png";
      } 
      else {
        img = "https://s.gravatar.com/avatar/" + img + "?s=24";
      }
      if (! name) {
        name = "";
      }

      this.$el = $("#userBar");
      this.$el.html(this.template.render({
        img : img,
        name : name,
        username : username,
        active : active
      }));

      this.delegateEvents();
      return this.$el;
    },

    userLogout : function() {
      this.userCollection.whoAmI();
      this.userCollection.logout();
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global _, window, document, Backbone, EJS, SwaggerUi, hljs, $, arangoHelper, templateEngine,
  CryptoJS, Joi */
(function() {

  "use strict";

  window.userManagementView = Backbone.View.extend({
    el: '#content',
    el2: '#userManagementThumbnailsIn',

    template: templateEngine.createTemplate("userManagementView.ejs"),

    events: {
      "click #createUser"                   : "createUser",
      "click #submitCreateUser"             : "submitCreateUser",
//      "click #deleteUser"                   : "removeUser",
//      "click #submitDeleteUser"             : "submitDeleteUser",
      "click .editUser"                     : "editUser",
      "click .icon"                         : "editUser",
      "click #submitEditUser"               : "submitEditUser",
      "click #userManagementToggle"         : "toggleView",
      "keyup #userManagementSearchInput"    : "search",
      "click #userManagementSearchSubmit"   : "search",
      "click #callEditUserPassword"         : "editUserPassword",
      "click #submitEditUserPassword"       : "submitEditUserPassword",
      "click #submitEditCurrentUserProfile" : "submitEditCurrentUserProfile",
      "click .css-label"                    : "checkBoxes",
      "change #userSortDesc"                : "sorting"

    },

    dropdownVisible: false,

    initialize: function() {
      //fetch collection defined in router
      this.collection.fetch({async:false});
      this.currentUser = this.collection.findWhere({user: this.collection.whoAmI()});
    },

    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    sorting: function() {
      if ($('#userSortDesc').is(":checked")) {
        this.collection.setSortingDesc(true);
      }
      else {
        this.collection.setSortingDesc(false);
      }

      if ($('#userManagementDropdown').is(":visible")) {
        this.dropdownVisible = true;
      } else {
        this.dropdownVisible = false;
      }


      this.render();
    },

    render: function (isProfile) {
      var dropdownVisible = false;
      if ($('#userManagementDropdown').is(':visible')) {
        dropdownVisible = true;
      }

      this.collection.sort();
      $(this.el).html(this.template.render({
        collection   : this.collection,
        searchString : ''
      }));
//      this.setFilterValues();

      if (dropdownVisible === true) {
        $('#userManagementDropdown2').show();
        $('#userSortDesc').attr('checked', this.collection.sortOptions.desc);
        $('#userManagementToggle').toggleClass('activated');
        $('#userManagementDropdown').show();
      }

//      var searchOptions = this.collection.searchOptions;

      //************
/*      this.userCollection.getFiltered(searchOptions).forEach(function (arango_collection) {
        $('#userManagementThumbnailsIn', this.el).append(new window.CollectionListItemView({
          model: arango_collection
        }).render().el);
      }, this);
*/
/*      $('#searchInput').val(searchOptions.searchPhrase);
      $('#searchInput').focus();
      var val = $('#searchInput').val();
      $('#searchInput').val('');
      $('#searchInput').val(val);
*/
//      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "left");
      if (!!isProfile) {
        this.editCurrentUser();
      }

      return this;
    },

    search: function() {
      var searchInput,
        searchString,
        strLength,
        reducedCollection;

      searchInput = $('#userManagementSearchInput');
      searchString = $("#userManagementSearchInput").val();
      reducedCollection = this.collection.filter(
        function(u) {
          return u.get("user").indexOf(searchString) !== -1;
        }
      );
      $(this.el).html(this.template.render({
        collection   : reducedCollection,
        searchString : searchString
      }));

      //after rendering, get the "new" element
      searchInput = $('#userManagementSearchInput');
      //set focus on end of text in input field
      strLength= searchInput.val().length;
      searchInput.focus();
      searchInput[0].setSelectionRange(strLength, strLength);
    },

    createUser : function(e) {
      e.preventDefault();
      this.createCreateUserModal();
    },

    submitCreateUser: function() {
      var self = this;
      var userName      = $('#newUsername').val();
      var name          = $('#newName').val();
      var userPassword  = $('#newPassword').val();
      var status        = $('#newStatus').is(':checked');
      if (!this.validateUserInfo(name, userName, userPassword, status)) {
        return;
      }
      var options = {
        user: userName,
        passwd: userPassword,
        active: status,
        extra:{name: name}
      };
      this.collection.create(options, {
        wait:true,
        error: function(data, err) {
          //Fix this. Function not available
          //self.handleError(err.status, err.statusText, name);
        },
        success: function(data) {
          self.updateUserManagement();
          window.modalView.hide();
        }
      });
    },

    validateUserInfo: function (name, username, pw, status) {
      if (username === "") {
        arangoHelper.arangoError("You have to define an username");
        $('#newUsername').closest("th").css("backgroundColor", "red");
        return false;
      }
/*      if (!username.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError("Name may only contain numbers, letters, _ and -");
        return false;
      }
      if (!user.match(/^[a-zA-Z][a-zA-Z\-]*$/)) {
        arangoHelper.arangoError("Name may only letters and -");
        return false;
      }*/
      return true;
    },

    updateUserManagement: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
//          window.App.handleSelectDatabase();
        }
      });
    },

    submitDeleteUser: function(username) {
      var toDelete = this.collection.findWhere({user: username});
      toDelete.destroy({wait: true});
      window.modalView.hide();
      this.updateUserManagement();
    },

    editUser : function(e) {
      this.collection.fetch();
      var username = this.evaluateUserName($(e.currentTarget).attr("id"), '_edit-user');
      if (username === '') {
        username = $(e.currentTarget).attr('id');
      }
      var user = this.collection.findWhere({user: username});
      if (user.get("loggedIn")) {
        this.editCurrentUser();
      } else {
        this.createEditUserModal(
          user.get("user"),
          user.get("extra").name,
          user.get("active")
        );
      }
    },

    editCurrentUser: function() {
      this.createEditCurrentUserModal(
        this.currentUser.get("user"),
        this.currentUser.get("extra").name,
        this.currentUser.get("extra").img
      );
    },

    submitEditUser : function(username) {
      var name = $('#editName').val();
      var status = $('#editStatus').is(':checked');

      if (!this.validateStatus(status)) {
        $('#editStatus').closest("th").css("backgroundColor", "red");
        return;
      }
      if (!this.validateName(name)) {
        $('#editName').closest("th").css("backgroundColor", "red");
        return;
      }
      var user = this.collection.findWhere({"user": username});
      //img may not be set, so keep the value
//      var img = user.get("extra").img;
//      user.set({"extra": {"name":name, "img":img}, "active":status});
      user.save({"extra": {"name":name}, "active":status}, {type: "PATCH"});
      window.modalView.hide();
      this.updateUserManagement();
    },

    validateUsername: function (username) {
      if (username === "") {
        arangoHelper.arangoError("You have to define an username");
        $('#newUsername').closest("th").css("backgroundColor", "red");
        return false;
      }
      if (!username.match(/^[a-zA-Z][a-zA-Z0-9_\-]*$/)) {
        arangoHelper.arangoError(
          "Wrong Username", "Username may only contain numbers, letters, _ and -"
        );
        return false;
      }
      return true;
    },

    validatePassword: function (passwordw) {
      return true;
    },

    validateName: function (name) {
      if (name === "") {
        return true;
      }
      if (!name.match(/^[a-zA-Z][a-zA-Z0-9_\-\ ]*$/)) {
        arangoHelper.arangoError(
          "Wrong Username", "Username may only contain numbers, letters, _ and -"
        );
        return false;
      }
      return true;
    },

    validateStatus: function (status) {
      if (status === "") {
        return false;
      }
      return true;
    },

    toggleView: function() {
      //apply sorting to checkboxes
      $('#userSortDesc').attr('checked', this.collection.sortOptions.desc);

      $('#userManagementToggle').toggleClass("activated");
      $('#userManagementDropdown2').slideToggle(200);
    },

    setFilterValues: function () {
      /*
      var searchOptions = this.collection.searchOptions;
      $('#checkLoaded').attr('checked', searchOptions.includeLoaded);
      $('#checkUnloaded').attr('checked', searchOptions.includeUnloaded);
      $('#checkSystem').attr('checked', searchOptions.includeSystem);
      $('#checkEdge').attr('checked', searchOptions.includeEdge);
      $('#checkDocument').attr('checked', searchOptions.includeDocument);
      $('#sortName').attr('checked', searchOptions.sortBy !== 'type');
      $('#sortType').attr('checked', searchOptions.sortBy === 'type');
      $('#sortOrder').attr('checked', searchOptions.sortOrder !== 1);
      */
    },

    evaluateUserName : function(str, substr) {
      var index = str.lastIndexOf(substr);
      return str.substring(0, index);
    },


    editUserPassword : function () {
      window.modalView.hide();
      this.createEditUserPasswordModal();
    },

    submitEditUserPassword : function () {
      var oldPasswd   = $('#oldCurrentPassword').val(),
        newPasswd     = $('#newCurrentPassword').val(),
        confirmPasswd = $('#confirmCurrentPassword').val();
      $('#oldCurrentPassword').val('');
      $('#newCurrentPassword').val('');
      $('#confirmCurrentPassword').val('');
      //check input
      //clear all "errors"
      $('#oldCurrentPassword').closest("th").css("backgroundColor", "white");
      $('#newCurrentPassword').closest("th").css("backgroundColor", "white");
      $('#confirmCurrentPassword').closest("th").css("backgroundColor", "white");


      //check
      var hasError = false;
      //Check old password
      if (!this.validateCurrentPassword(oldPasswd)) {
        $('#oldCurrentPassword').closest("th").css("backgroundColor", "red");
        hasError = true;
      }
      //check confirmation
      if (newPasswd !== confirmPasswd) {
        $('#confirmCurrentPassword').closest("th").css("backgroundColor", "red");
        hasError = true;
      }
      //check new password
      if (!this.validatePassword(newPasswd)) {
        $('#newCurrentPassword').closest("th").css("backgroundColor", "red");
        hasError = true;
      }

      if (hasError) {
        return;
      }
      this.currentUser.setPassword(newPasswd);
      window.modalView.hide();
    },

    validateCurrentPassword : function (pwd) {
      return this.currentUser.checkPassword(pwd);
    },


    submitEditCurrentUserProfile: function() {
      var name    = $('#editCurrentName').val();
      var img     = $('#editCurrentUserProfileImg').val();
      img = this.parseImgString(img);

      /*      if (!this.validateName(name)) {
       $('#editName').closest("th").css("backgroundColor", "red");
       return;
       }*/

      this.currentUser.setExtras(name, img);
      this.updateUserProfile();
      window.modalView.hide();
    },

    updateUserProfile: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.render();
        }
      });
    },

    parseImgString : function(img) {
      //if already md5
      if (img.indexOf("@") === -1) {
        return img;
      }
      //else generate md5
      return CryptoJS.MD5(img).toString();
    },


    //modal dialogs

    createEditUserModal: function(username, name, active) {
      var buttons, tableContent;
      tableContent = [
        {
          type: window.modalView.tables.READONLY,
          label: "Username",
          value: _.escape(username)
        },
        {
          type: window.modalView.tables.TEXT,
          label: "Name",
          value: name,
          id: "editName",
          placeholder: "Name"
        },
        {
          type: window.modalView.tables.CHECKBOX,
          label: "Active",
          value: "active",
          checked: active,
          id: "editStatus"
        }
      ];
      buttons = [
        {
          title: "Delete",
          type: window.modalView.buttons.DELETE,
          callback: this.submitDeleteUser.bind(this, username)
        },
        {
          title: "Save",
          type: window.modalView.buttons.SUCCESS,
          callback: this.submitEditUser.bind(this, username)
        }
      ];
      window.modalView.show("modalTable.ejs", "Edit User", buttons, tableContent);
    },

    createCreateUserModal: function() {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createTextEntry(
          "newUsername",
          "Username",
          "",
          false,
          "Username",
          true,
          [
            {
              rule: Joi.string().required(),
              msg: "No username given."
            }
          ]
        )
      );
      tableContent.push(
        window.modalView.createTextEntry("newName", "Name", "", false, "Name", false)
      );
      tableContent.push(
        window.modalView.createPasswordEntry("newPassword", "Password", "", false, "", false)
      );
      tableContent.push(
        window.modalView.createCheckboxEntry("newStatus", "Active", "active", false, true)
      );
      buttons.push(
        window.modalView.createSuccessButton("Create", this.submitCreateUser.bind(this))
      );

      window.modalView.show("modalTable.ejs", "Create New User", buttons, tableContent);
    },

    createEditCurrentUserModal: function(username, name, img) {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry("id_username", "Username", username)
      );
      tableContent.push(
        window.modalView.createTextEntry("editCurrentName", "Name", name, false, "Name", false)
      );
      tableContent.push(
        window.modalView.createTextEntry(
          "editCurrentUserProfileImg",
          "Gravatar account (Mail)",
          img,
          "Mailaddress or its md5 representation of your gravatar account."
            + " The address will be converted into a md5 string. "
            + "Only the md5 string will be stored, not the mailaddress.",
          "myAccount(at)gravatar.com"
        )
      );

      buttons.push(
        window.modalView.createNotificationButton(
          "Change Password",
          this.editUserPassword.bind(this)
        )
      );
      buttons.push(
        window.modalView.createSuccessButton(
          "Save",
          this.submitEditCurrentUserProfile.bind(this)
        )
      );

      window.modalView.show("modalTable.ejs", "Edit User Profile", buttons, tableContent);
    },

    createEditUserPasswordModal: function() {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createPasswordEntry(
          "oldCurrentPassword",
          "Old Password",
          "",
          false,
          "old password",
          false
        )
      );
      tableContent.push(
        window.modalView.createPasswordEntry(
          "newCurrentPassword",
          "New Password",
          "",
          false,
          "new password",
          false
        )
      );
      tableContent.push(
        window.modalView.createPasswordEntry(
          "confirmCurrentPassword",
          "Confirm New Password",
          "",
          false,
          "confirm new password",
          false)
      );

      buttons.push(
        window.modalView.createSuccessButton(
          "Save",
          this.submitEditUserPassword.bind(this)
        )
      );

      window.modalView.show("modalTable.ejs", "Edit User Password", buttons, tableContent);
    }

  });
}());

/*jshint unused: false */
/*global window, $, Backbone, document, arangoCollectionModel*/
/*global arangoHelper,dashboardView,arangoDatabase, _*/

(function () {
  "use strict";

  window.Router = Backbone.Router.extend({
    routes: {
      "": "dashboard",
      "dashboard": "dashboard",
      "collections": "collections",
      "new": "newCollection",
      "login": "login",
      "collection/:colid/documents/:pageid": "documents",
      "collection/:colid/:docid": "document",
      "shell": "shell",
      "query": "query",
      "queryManagement": "queryManagement",
      "api": "api",
      "databases": "databases",
      "applications": "applications",
      "applications/:mount": "applicationDetail",
      "application/documentation/:mount": "appDocumentation",
      "graph": "graphManagement",
      "userManagement": "userManagement",
      "userProfile": "userProfile",
      "logs": "logs"
    },

    logs: function () {
      if (!this.logsView) {
        var newLogsAllCollection = new window.ArangoLogs(
          {upto: true, loglevel: 4}
        ),
        newLogsDebugCollection = new window.ArangoLogs(
          {loglevel: 4}
        ),
        newLogsInfoCollection = new window.ArangoLogs(
          {loglevel: 3}
        ),
        newLogsWarningCollection = new window.ArangoLogs(
          {loglevel: 2}
        ),
        newLogsErrorCollection = new window.ArangoLogs(
          {loglevel: 1}
        );
        this.logsView = new window.LogsView({
          logall: newLogsAllCollection,
          logdebug: newLogsDebugCollection,
          loginfo: newLogsInfoCollection,
          logwarning: newLogsWarningCollection,
          logerror: newLogsErrorCollection
        });
      }
      this.logsView.render();
      this.naviView.selectMenuItem('tools-menu');
    },

    initialize: function () {
      // This should be the only global object
      window.modalView = new window.ModalView();

      this.foxxList = new window.FoxxCollection();
      window.foxxInstallView = new window.FoxxInstallView({
        collection: this.foxxList
      });
      window.progressView = new window.ProgressView();
      var self = this;

      this.arangoDatabase = new window.ArangoDatabase();

      this.currentDB = new window.CurrentDatabase();
      this.currentDB.fetch({
        async: false
      });

      this.userCollection = new window.ArangoUsers();

      this.arangoCollectionsStore = new window.arangoCollections();
      this.arangoDocumentStore = new window.arangoDocument();
      arangoHelper.setDocumentStore(this.arangoDocumentStore);

      this.arangoCollectionsStore.fetch({async: false});

      this.footerView = new window.FooterView();
      this.notificationList = new window.NotificationCollection();
      this.naviView = new window.NavigationView({
        database: this.arangoDatabase,
        currentDB: this.currentDB,
        notificationCollection: self.notificationList,
        userCollection: this.userCollection
      });

      this.queryCollection = new window.ArangoQueries();

      this.footerView.render();
      this.naviView.render();

      $(window).resize(function () {
        self.handleResize();
      });
      window.checkVersion();

    },

    checkUser: function () {
      if (this.userCollection.models.length === 0) {
        this.navigate("login", {trigger: true});
        return false;
      }
      return true;
    },

    applicationDetail: function (mount) {
      this.naviView.selectMenuItem('applications-menu');

      if (this.foxxList.length === 0) {
        this.foxxList.fetch({ async: false });
      }
      if (!this.hasOwnProperty('applicationDetailView')) {
        this.applicationDetailView = new window.ApplicationDetailView({
          model: this.foxxList.get(decodeURIComponent(mount))
        });
      }

      this.applicationDetailView.model = this.foxxList.get(decodeURIComponent(mount));
      this.applicationDetailView.render();
    },

    login: function () {
      if (!this.loginView) {
        this.loginView = new window.loginView({
          collection: this.userCollection
        });
      }
      this.loginView.render();
      this.naviView.selectMenuItem('');
    },

    collections: function () {
      var naviView = this.naviView, self = this;
      if (!this.collectionsView) {
        this.collectionsView = new window.CollectionsView({
          collection: this.arangoCollectionsStore
        });
      }
      this.arangoCollectionsStore.fetch({
        success: function () {
          self.collectionsView.render();
          naviView.selectMenuItem('collections-menu');
        }
      });
    },

    documents: function (colid, pageid) {
      if (!this.documentsView) {
        this.documentsView = new window.DocumentsView({
          collection: new window.arangoDocuments(),
          documentStore: this.arangoDocumentStore,
          collectionsStore: this.arangoCollectionsStore
        });
      }
      this.documentsView.setCollectionId(colid, pageid);
      this.documentsView.render();

    },

    document: function (colid, docid) {
      if (!this.documentView) {
        this.documentView = new window.DocumentView({
          collection: this.arangoDocumentStore
        });
      }
      this.documentView.colid = colid;
      this.documentView.docid = docid;
      this.documentView.render();
      var type = arangoHelper.collectionApiType(colid);
      this.documentView.setType(type);
    },

    shell: function () {
      if (!this.shellView) {
        this.shellView = new window.shellView();
      }
      this.shellView.render();
      this.naviView.selectMenuItem('tools-menu');
    },

    query: function () {
      if (!this.queryView) {
        this.queryView = new window.queryView({
          collection: this.queryCollection
        });
      }
      this.queryView.render();
      this.naviView.selectMenuItem('query-menu');
    },

    queryManagement: function () {
      if (!this.queryManagementView) {
        this.queryManagementView = new window.queryManagementView({
          collection: undefined
        });
      }
      this.queryManagementView.render();
      this.naviView.selectMenuItem('tools-menu');
    },

    api: function () {
      if (!this.apiView) {
        this.apiView = new window.ApiView();
      }
      this.apiView.render();
      this.naviView.selectMenuItem('tools-menu');
    },

    databases: function () {
      if (arangoHelper.databaseAllowed() === true) {
        if (! this.databaseView) {
          this.databaseView = new window.databaseView({
            users: this.userCollection,
            collection: this.arangoDatabase
          });
          }
          this.databaseView.render();
          this.naviView.selectMenuItem('databases-menu');
        }
        else {
          this.navigate("#", {trigger: true});
          this.naviView.selectMenuItem('dashboard-menu');
          $('#databaseNavi').css('display', 'none');
          $('#databaseNaviSelect').css('display', 'none');
        }
      },

      dashboard: function () {
        this.naviView.selectMenuItem('dashboard-menu');
        if (this.dashboardView === undefined) {
          this.dashboardView = new window.DashboardView({
            dygraphConfig: window.dygraphConfig,
            database: this.arangoDatabase
          });
        }
        this.dashboardView.render();
      },

      graphManagement: function () {
        if (!this.graphManagementView) {
          this.graphManagementView =
          new window.GraphManagementView(
            {
              collection: new window.GraphCollection(),
              collectionCollection: this.arangoCollectionsStore
            }
          );
        }
        this.graphManagementView.render();
        this.naviView.selectMenuItem('graphviewer-menu');
      },

      applications: function () {
        if (this.applicationsView === undefined) {
          this.applicationsView = new window.ApplicationsView({
            collection: this.foxxList
          });
        }
        this.applicationsView.reload();
        this.naviView.selectMenuItem('applications-menu');
      },

      appDocumentation: function (mount) {
        var docuView = new window.AppDocumentationView({mount: mount});
        docuView.render();
        this.naviView.selectMenuItem('applications-menu');
      },

      handleSelectDatabase: function () {
        this.naviView.handleSelectDatabase();
      },

      handleResize: function () {
        if (this.dashboardView) {
          this.dashboardView.resize();
        }
        if (this.graphManagementView) {
          this.graphManagementView.handleResize($("#content").width());
        }
        if (this.queryView) {
          this.queryView.resize();
        }
      },

      userManagement: function () {
        if (!this.userManagementView) {
          this.userManagementView = new window.userManagementView({
            collection: this.userCollection
          });
        }
        this.userManagementView.render();
        this.naviView.selectMenuItem('tools-menu');
      },

      userProfile: function () {
        if (!this.userManagementView) {
          this.userManagementView = new window.userManagementView({
            collection: this.userCollection
          });
        }
        this.userManagementView.render(true);
        this.naviView.selectMenuItem('tools-menu');
      }
    });

  }());

/*jshint unused: false */
/*global $, window, _*/
(function() {
  "use strict";

  var disableVersionCheck = function() {
    $.ajax({
      type: "POST",
      url: "/_admin/aardvark/disableVersionCheck"
    });
  };

  var isVersionCheckEnabled = function(cb) {
    $.ajax({
      type: "GET",
      url: "/_admin/aardvark/shouldCheckVersion",
      success: function(data) {
        if (data === true) {
          cb();
        }
      }
    });
  };

  var showInterface = function(currentVersion, json) {
    var buttons = [];
    buttons.push(window.modalView.createNotificationButton("Don't ask again", function() {
      disableVersionCheck();
      window.modalView.hide();
    }));
    buttons.push(window.modalView.createSuccessButton("Download Page", function() {
      window.open('https://www.arangodb.com/download','_blank');
      window.modalView.hide();
    }));
    var infos = [];
    var cEntry = window.modalView.createReadOnlyEntry.bind(window.modalView);
    infos.push(cEntry("current", "Current", currentVersion.toString()));
    if (json.major) {
      infos.push(cEntry("major", "Major", json.major.version));
    }
    if (json.minor) {
      infos.push(cEntry("minor", "Minor", json.minor.version));
    }
    if (json.bugfix) {
      infos.push(cEntry("bugfix", "Bugfix", json.bugfix.version));
    }
    window.modalView.show(
      "modalTable.ejs", "New Version Available", buttons, infos
    );
  };

  window.checkVersion = function() {
    // this checks for version updates
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/version",
      contentType: "application/json",
      processData: false,
      async: true,
      success: function(data) {
        var currentVersion =
          window.versionHelper.fromString(data.version);
        window.parseVersions = function (json) {
          if (_.isEmpty(json)) {
            return; // no new version.
          }
          if (/-devel$/.test(data.version)) {
            return; // ignore version in devel
          }
          isVersionCheckEnabled(showInterface.bind(window, currentVersion, json));
        };
        $.ajax({
          type: "GET",
          async: true,
          crossDomain: true,
          dataType: "jsonp",
          url: "https://www.arangodb.com/repositories/versions.php" +
          "?jsonp=parseVersions&version=" + encodeURIComponent(currentVersion.toString())
        });
      }
    });
  };
}());

/*jshint unused: false */
/*global window, $, Backbone, document */

(function() {
  "use strict";
  // We have to start the app only in production mode, not in test mode
  if (!window.hasOwnProperty("TEST_BUILD")) {
    $(document).ready(function() {
      window.App = new window.Router();
      Backbone.history.start();
      window.App.handleResize();
    });
  }

}());
