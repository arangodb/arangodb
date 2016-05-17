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
    collapseCounter = 0,
    collapsePrevEdge = {},
    collapsePrevNode = {},

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

      var checkEdges = [];
      _.each(removedEdges, function(edge) {
        if (collapseCounter === 0) {
          collapsePrevEdge = edge;
          collapsePrevNode = node;
          checkEdges.push(edge);
        }
        else {
          if (node !== undefined) {
            if (node._id === collapsePrevEdge.target._id) {
              if (edge.target._id === collapsePrevNode._id) {
                checkEdges.push(collapsePrevEdge);
              }
            }
            else {
              checkEdges.push(edge);
            }
            collapsePrevEdge = edge;
            collapsePrevNode = node;
          }
        }

        collapseCounter++;
      });


      _.each(checkEdges, handleRemovedEdge);
      collapseCounter = 0;
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

  colours.push({back: "#C8E6C9", front: "black"});
  colours.push({back: "#8aa249", front: "white"});
  colours.push({back: "#8BC34A", front: "black"});
  colours.push({back: "#388E3C", front: "white"});
  colours.push({back: "#4CAF50", front: "white"});
  colours.push({back: "#212121", front: "white"});
  colours.push({back: "#727272", front: "white"});
  colours.push({back: "#B6B6B6", front: "black"});
  colours.push({back: "#e5f0a3", front: "black"});
  colours.push({back: "#6c4313", front: "white"});
  colours.push({back: "#9d8564", front: "white"});

  /*
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
  */

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
        var counter = 0;
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
    _.each(nodes, function(node) {
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
    sendQuery(queries.traversal, {
      example: nodeId
    }, function(res) {

      var nodes = [];
      nodes = self.getRandomNodes();

      if (nodes.length > 0) {
        _.each(nodes, function(node) {
          sendQuery(queries.traversal, {
            example: node.vertex._id
          }, function(res2) {
            _.each(res2[0][0], function(obj) {
              res[0][0].push(obj);
            });
            parseResultOfTraversal(res, callback);
          });
        });
      }
      else {
        sendQuery(queries.traversal, {
          example: nodeId
        }, function(res) {
          parseResultOfTraversal(res, callback);
        });
      }
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

  self.getNodeExampleFromTreeByAttributeValue = function(attribute, value, callback) {
    var example = {};

    example[attribute] = value;
    sendQuery(queries.traversal, {
      example: example
    }, function(res) {

      if (res[0][0] === undefined) {
        arangoHelper.arangoError("Graph error", "no nodes found");
        throw "No suitable nodes have been found.";
      }
      else {
        _.each(res[0][0], function(node) {
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
          url: "/_api/collection/" + encodeURIComponent(collections[i]) + "/count",
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
        //chunks[0] = $.trim(label.substring(0,10)) + "-";
        chunks[0] = $.trim(label.substring(0,10));
        chunks[1] = $.trim(label.substring(10));
        if (chunks[1].length > 12) {
          chunks[1] = chunks[1].split(/\W/)[0];
          if (chunks[1].length > 2) {
            chunks[1] = chunks[1].substring(0,5) + "...";
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

    adjustLabelFontSize = function (nodes) {
      var texts = [], childText = '';

      _.each(nodes, function(node) {
        texts = $(node).find('text');

        $(node).css("width", "90px");
        $(node).css("height", "36px");

         $(node).textfill({
           innerTag: 'text',
           maxFontPixels: 16,
           minFontPixels: 10,
           explicitWidth: 90,
           explicitHeight: 36,
         });
      });
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
              var title = chunks[0];
              if (chunks.length === 2) {
                title = title + chunks[1];
              }
              if (title.length > 15) {
                title = title.substring(0, 13) + "...";
              }
              if (title === undefined || title === '') {
                title = "ATTR NOT SET";
              }
              d3.select(this).append("tspan")
                .attr("x", "0")
                .attr("dy", "5")
                .text(title);
              /*if (chunks.length === 2) {
                d3.select(this).append("tspan")
                  .attr("x", "0")
                  .attr("dy", "16")
                  .text(chunks[1]);
              }*/
            });
          adjustLabelFontSize(node);
        };
      } else {
        addLabel = function (node) {
          var textN = node.append("text") // Append a label for the node
            .attr("text-anchor", "middle") // Define text-anchor
            .attr("fill", addLabelColor) // Force a black color
            .attr("stroke", "none"); // Make it readable
          textN.each(function(d) {
            var chunks = splitLabel(findFirstValue(label, d._data));
            var title = chunks[0];
            if (chunks.length === 2) {
              title = title + chunks[1];
            }
            if (title.length > 15) {
              title = title.substring(0, 13) + "...";
            }
            if (title === undefined || title === '') {
              title = "ATTR NOT SET";
            }
            d3.select(this).append("tspan")
              .attr("x", "0")
              .attr("dy", "5")
              .text(title);
            /*if (chunks.length === 2) {
              d3.select(this).append("tspan")
                .attr("x", "0")
                .attr("dy", "16")
                .text(chunks[1]);
            }*/
          });
          adjustLabelFontSize(node);
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
             }).attr("stroke", function(n) {
               if (!n._expanded) {
                 // if node is not expanded
                 return "transparent";
               }
               // if node is expanded
               return "#fff";
             }).attr("fill-opacity", function(n) {
               if (!n._expanded) {
                 // if node is not expanded
                 return "0.3";
               }
               // if node is expanded
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
    arangoHelper.arangoError("Server-side", "createEdge was triggered.");
  };

  self.deleteEdge = function (edgeToRemove, callback) {
    arangoHelper.arangoError("Server-side", "deleteEdge was triggered.");
  };

  self.patchEdge = function (edgeToPatch, patchData, callback) {
    arangoHelper.arangoError("Server-side", "patchEdge was triggered.");
  };

  self.createNode = function (nodeToAdd, callback) {
    arangoHelper.arangoError("Server-side", "createNode was triggered.");
  };

  self.deleteNode = function (nodeToRemove, callback) {
    arangoHelper.arangoError("Server-side", "deleteNode was triggered.");
    arangoHelper.arangoError("Server-side", "onNodeDelete was triggered.");
  };

  self.patchNode = function (nodeToPatch, patchData, callback) {
    arangoHelper.arangoError("Server-side", "patchNode was triggered.");
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
      url = window.URL,
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
      /*
      var focus = d3.mouse(this);
      focus[0] -= currentTranslation[0];
      focus[0] /= currentZoom;
      focus[1] -= currentTranslation[1];
      focus[1] /= currentZoom;
      fisheye.focus(focus);
      nodeShaper.updateNodes();
      edgeShaper.updateEdges();*/
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
/*global document, Storage, localStorage, window*/
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
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
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
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
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

          var obj = {
            label: labels
          };
          self.applyLocalStorage(obj);
          shaper.changeTo(obj);
        }
      );
    });
  };

  this.applyLocalStorage = function(obj) {
    if (Storage !== "undefined") {
      try {
        var toStore = JSON.parse(localStorage.getItem('graphSettings')),
        graphName = (window.location.hash).split("/")[1],
        dbName = (window.location.pathname).split('/')[2],
        combinedGraphName = graphName + dbName;

        _.each(obj, function(value, key) {
          if (key !== undefined) {
            if (!toStore[combinedGraphName].viewer.hasOwnProperty('edgeShaper')) {
              toStore[combinedGraphName].viewer.edgeShaper = {};
            } 
            toStore[combinedGraphName].viewer.edgeShaper[key] = value;
          }
        });

        localStorage.setItem('graphSettings', JSON.stringify(toStore));
      }
      catch (e) {
        console.log(e);
      }
    }
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
/*global EventDispatcher, arangoHelper*/
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
        icon: "hand-pointer-o",
        title: "Expand a node."
      },
      add: {
        icon: "plus-square",
        title: "Add a node."
      },
      trash: {
        icon: "minus-square",
        title: "Remove a node/edge."
      },
      drag: {
        icon: "hand-rock-o",
        title: "Drag a node."
      },
      edge: {
        icon: "external-link-square",
        title: "Create an edge between two nodes."
      },
      edit: {
        icon: "pencil-square",
        title: "Edit attributes of a node."
      },
      view: {
        icon: "search",
        title: "View attributes of a node."
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
        /*var deleteCallback = function() {
          console.log("callback");
          dispatcher.events.DELETENODE(function() {
            $("#control_event_node_delete_modal").modal('hide');
            nodeShaper.reshapeNodes();
            edgeShaper.reshapeEdges();
            start();
          })(n);
        };*/

        arangoHelper.openDocEditor(n._id, 'document');
          /*
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
          */
        },
        edgeCallback = function(e) {
          arangoHelper.openDocEditor(e._id, 'edge');
          /*modalDialogHelper.createModalEditDialog(
            "Edit Edge " + e._id,
            "control_event_edge_edit_",
            e._data,
            function(newData) {
              dispatcher.events.PATCHEDGE(e, newData, function() {
                $("#control_event_edge_edit_modal").modal('hide');
              })();
            }
          );*/
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

  //nodeShaper.addMenuEntry("View", callbacks.nodes.view);
  nodeShaper.addMenuEntry("Edit", callbacks.nodes.edit);
  nodeShaper.addMenuEntry("Spot", callbacks.nodes.spot);
  nodeShaper.addMenuEntry("Trash", callbacks.nodes.del);

  //edgeShaper.addMenuEntry("View", callbacks.edges.view);
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
    self.addControlExpand();
    self.addControlDrag();
    //self.addControlView();
    self.addControlEdit();
    self.addControlConnect();
    self.addControlNewNode();
    self.addControlDelete();
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
      uiComponentsHelper.createButton(list, "Switch Graph", prefix, function() {
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
          },//currently disabled outbound only view
          /*{
            type: "checkbox",
            id: "undirected",
            selected: (adapter.getDirection() === "any")
          }*/], function () {
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
      modalDialogHelper.createModalChangeDialog(label + " by attribute",
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

  width = container.getBoundingClientRect().width;
  height = container.getBoundingClientRect().height;
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
/*global GraphViewer, d3, window, arangoHelper, Storage, localStorage*/
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
    width = (optWidth + 20 || container.getBoundingClientRect().width - 81 + 20),
    height = optHeight || container.getBoundingClientRect().height,
    menubar = document.createElement("ul"),
    background = document.createElement("div"),
    colourList,
    nodeShaperUI,
    edgeShaperUI,
    adapterUI,
    slider,
    searchAttrExampleList,
    searchAttrExampleList2,
    //mousePointerBox = document.createElement("div"),
    svg,

    makeDisplayInformationDiv = function() {
      if (graphViewer.adapter.NODES_TO_DISPLAY < graphViewer.adapter.TOTAL_NODES) {
        $('.headerBar').append(
          '<div class="infoField">Graph too big. A random section is rendered.<div class="fa fa-info-circle"></div></div>'
        );
        $('.infoField .fa-info-circle').attr("title", "You can display additional/other vertices by using the toolbar buttons.").tooltip();
      }
    },

    makeFilterDiv = function() {
      var
        div = document.createElement("div"),
        innerDiv = document.createElement("div"),
        queryLine = document.createElement("div"),
        searchAttrDiv = document.createElement("div"),
        searchAttrExampleToggle = document.createElement("button"),
        searchAttrExampleCaret = document.createElement("span"),
        searchValueField = document.createElement("input"),
        searchStart = document.createElement("i"),
        equalsField = document.createElement("span"),
        searchAttrField,

        showSpinner = function() {
          $(background).css("cursor", "progress");
        },

        hideSpinner = function() {
          $(background).css("cursor", "");
        },

        alertError = function(msg) {
          arangoHelper.arangoError("Graph", msg);
        },

        resultCB = function(res) {
          hideSpinner();
          if (res && res.errorCode && res.errorCode === 404) {
            arangoHelper.arangoError("Graph error", "could not find a matching node.");
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
      div.className = "headerDropdown smallDropdown";
      innerDiv.className = "dropdownInner";
      queryLine.className = "queryline";

      searchAttrField = document.createElement("input");
      searchAttrExampleList = document.createElement("ul");

      searchAttrDiv.className = "pull-left input-append searchByAttribute";
      searchAttrField.id = "attribute";
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
      searchStart.className = "fa fa-search";
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

      var title = document.createElement("p");
      title.className = "dropdown-title";
      title.innerHTML = "Filter graph by attribute:";
      div.appendChild(title);

      div.appendChild(innerDiv);
      return div;
    },

    makeNodeDiv = function () {
      var
        div2 = document.createElement("div"),
        innerDiv2 = document.createElement("div"),
        queryLine2 = document.createElement("div"),
        searchAttrDiv2 = document.createElement("div"),
        searchAttrExampleToggle2 = document.createElement("button"),
        searchAttrExampleCaret2 = document.createElement("span"),
        searchValueField2 = document.createElement("input"),
        searchStart2 = document.createElement("i"),
        equalsField2 = document.createElement("span"),
        searchAttrField2,

        showSpinner = function() {
          $(background).css("cursor", "progress");
        },

        hideSpinner = function() {
          $(background).css("cursor", "");
        },

        alertError = function(msg) {
          arangoHelper.arangoError("Graph", msg);
        },

        resultCB2 = function(res) {
          hideSpinner();
          if (res && res.errorCode && res.errorCode === 404) {
            arangoHelper.arangoError("Graph error", "could not find a matching node.");
            return;
          }
          return;
        },

        addCustomNode = function() {
          showSpinner();
          if (searchAttrField2.value !== "") {
            graphViewer.loadGraphWithAdditionalNode(
              searchAttrField2.value,
              searchValueField2.value,
              resultCB2
            );
          }
        };

      div2.id = "nodeDropdown";
      div2.className = "headerDropdown smallDropdown";
      innerDiv2.className = "dropdownInner";
      queryLine2.className = "queryline";

      searchAttrField2 = document.createElement("input");
      searchAttrExampleList2 = document.createElement("ul");

      searchAttrDiv2.className = "pull-left input-append searchByAttribute";
      searchAttrField2.id = "attribute";
      searchAttrField2.type = "text";
      searchAttrField2.placeholder = "Attribute name";
      searchAttrExampleToggle2.id = "attribute_example_toggle2";
      searchAttrExampleToggle2.className = "button-neutral gv_example_toggle";
      searchAttrExampleCaret2.className = "caret gv_caret";
      searchAttrExampleList2.className = "gv-dropdown-menu";
      searchValueField2.id = "value";
      searchValueField2.className = "searchInput gv_searchInput";
      //searchValueField.className = "filterValue";
      searchValueField2.type = "text";
      searchValueField2.placeholder = "Attribute value";
      searchStart2.id = "loadnode";
      searchStart2.className = "fa fa-search";
      equalsField2.className = "searchEqualsLabel";
      equalsField2.appendChild(document.createTextNode("=="));

      innerDiv2.appendChild(queryLine2);
      queryLine2.appendChild(searchAttrDiv2);

      searchAttrDiv2.appendChild(searchAttrField2);
      searchAttrDiv2.appendChild(searchAttrExampleToggle2);
      searchAttrDiv2.appendChild(searchAttrExampleList2);
      searchAttrExampleToggle2.appendChild(searchAttrExampleCaret2);
      queryLine2.appendChild(equalsField2);
      queryLine2.appendChild(searchValueField2);
      queryLine2.appendChild(searchStart2);

      updateAttributeExamples(searchAttrExampleList2);
      //searchAttrExampleList2.onclick = function() {
      //  updateAttributeExamples(searchAttrExampleList2);
      //};

      searchStart2.onclick = addCustomNode;
      $(searchValueField2).keypress(function(e) {
        if (e.keyCode === 13 || e.which === 13) {
          addCustomNode();
          return false;
        }
      });

      searchAttrExampleToggle2.onclick = function() {
        $(searchAttrExampleList2).slideToggle(200);
      };

      var title = document.createElement("p");
      title.className = "dropdown-title";
      title.innerHTML = "Add specific node by attribute:";
      div2.appendChild(title);

      div2.appendChild(innerDiv2);
      return div2;
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

    makeConfigure = function (div, idConf, idFilter, idNode) {
      var ul, lists,
      liConf, aConf, spanConf,
      liNode, aNode, spanNode,
      liFilter, aFilter, spanFilter;

      div.className = "headerButtonBar";
      ul = document.createElement("ul");
      ul.className = "headerButtonList";

      div.appendChild(ul);

      //CONF
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

      //NODE
      liNode = document.createElement("li");
      liNode.className = "enabled";
      aNode = document.createElement("a");
      aNode.id = idNode;
      aNode.className = "headerButton";
      spanNode = document.createElement("span");
      spanNode.className = "fa fa-search-plus";
      $(spanNode).attr("title", "Show additional vertices");

      ul.appendChild(liNode);
      liNode.appendChild(aNode);
      aNode.appendChild(spanNode);

      //FILTER
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
      lists.node = makeNodeDiv();

      aConf.onclick = function () {
        $('#filterdropdown').removeClass('activated');
        $('#nodedropdown').removeClass('activated');
        $('#configuredropdown').toggleClass('activated');
        $(lists.configure).slideToggle(200);
        $(lists.filter).hide();
        $(lists.node).hide();
      };

      aNode.onclick = function () {
        $('#filterdropdown').removeClass('activated');
        $('#configuredropdown').removeClass('activated');
        $('#nodedropdown').toggleClass('activated');
        $(lists.node).slideToggle(200);
        $(lists.filter).hide();
        $(lists.configure).hide();
      };

      aFilter.onclick = function () {
        $('#configuredropdown').removeClass('activated');
        $('#nodedropdown').removeClass('activated');
        $('#filterdropdown').toggleClass('activated');
        $(lists.filter).slideToggle(200);
        $(lists.node).hide();
        $(lists.configure).hide();
      };

      return lists;
    },

    createSVG = function () {
  console.log(height);
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

    createOptionBox = function() {
      //create select option box 
      var optionBox = '<li class="enabled" style="float:right">'+
      '<select id="graphSize" class="documents-size">'+
      '<optgroup label="Starting points:">'+
      '<option value="5" selected="">5 vertices</option>'+
      '<option value="10">10 vertices</option>'+
      '<option value="20">20 vertices</option>'+
      '<option value="50">50 vertices</option>'+
      '<option value="100">100 vertices</option>'+
      '<option value="500">500 vertices</option>'+
      '<option value="1000">1000 vertices</option>'+
      '<option value="2500">2500 vertices</option>'+
      '<option value="5000">5000 vertices</option>'+
      '<option value="all">All vertices</option>'+
      '</select>'+
      '</optgroup>'+
      '</li>';
      $('.headerBar .headerButtonList').prepend(optionBox);
    },

    updateAttributeExamples = function(e) {
      var element;

      if (e) {
        element = $(e);
      }
      else {
        element = $(searchAttrExampleList);
      }

      element.innerHTML = "";
      var throbber = document.createElement("li"),
        throbberImg = document.createElement("img");
      $(throbber).append(throbberImg);
      throbberImg.className = "gv-throbber";
      element.append(throbber);
      graphViewer.adapter.getAttributeExamples(function(res) {
        $(element).html('');
        _.each(res, function(r) {
          var entry = document.createElement("li"),
            link = document.createElement("a"),
            lbl = document.createElement("label");
          $(entry).append(link);
          $(link).append(lbl);
          $(lbl).append(document.createTextNode(r));
          lbl.className = "gv_dropdown_label";
          element.append(entry);
          entry.onclick = function() {
            element.value = r;
            $(element).parent().find('input').val(r);
            $(element).slideToggle(200);
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
          "filterdropdown",
          "nodedropdown"
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

      //title.appendChild(document.createTextNode("Graph Viewer"));
      //title.className = "arangoHeader";

      /*
      nodeShaperDropDown.id = "nodeshapermenu";
      edgeShaperDropDown.id = "edgeshapermenu";
      layouterDropDown.id = "layoutermenu";
      adapterDropDown.id = "adaptermenu";
      */

      menubar.appendChild(transparentHeader);
      menubar.appendChild(configureLists.configure);
      menubar.appendChild(configureLists.filter);
      menubar.appendChild(configureLists.node);
      transparentHeader.appendChild(buttons);
      //transparentHeader.appendChild(title);

      adapterUI.addControlChangeGraph(function() {
        updateAttributeExamples();
        graphViewer.start(true);
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

  if (Storage !== "undefined") {
    this.graphSettings = {};

    this.loadLocalStorage = function() {
      //graph name not enough, need to set db name also
      var dbName = adapterConfig.baseUrl.split('/')[2],
      combinedGraphName = adapterConfig.graphName + dbName;
      
      if (localStorage.getItem('graphSettings') === null || localStorage.getItem('graphSettings')  === 'null') {
        var obj = {};
        obj[combinedGraphName] = {
          viewer: viewerConfig,
          adapter: adapterConfig
        };
        localStorage.setItem('graphSettings', JSON.stringify(obj));
      }
      else {
        try {
          var settings = JSON.parse(localStorage.getItem('graphSettings'));
          this.graphSettings = settings;

          if (settings[combinedGraphName].viewer !== undefined) {
            viewerConfig = settings[combinedGraphName].viewer;  
          }
          if (settings[combinedGraphName].adapter !== undefined) {
            adapterConfig = settings[combinedGraphName].adapter;
          }
        }
        catch (e) {
          console.log("Could not load graph settings, resetting graph settings.");
          this.graphSettings[combinedGraphName] = {
            viewer: viewerConfig,
            adapter: adapterConfig
          };
          localStorage.setItem('graphSettings', JSON.stringify(this.graphSettings));
        }
      }

    };
    this.loadLocalStorage();
    
    this.writeLocalStorage = function() {

    };
  }

  graphViewer = new GraphViewer(svg, width, height, adapterConfig, viewerConfig);

  createToolbox();
  createZoomUIWidget();
  createMenu();
  createColourList();
  makeDisplayInformationDiv();
  createOptionBox();

  $('#graphSize').on('change', function() {
    var size = $('#graphSize').find(":selected").val();
      graphViewer.loadGraphWithRandomStart(function(node) {
        if (node && node.errorCode) {
          arangoHelper.arangoError("Graph", "Sorry your graph seems to be empty");
        }
      }, size);
  });

  if (startNode) {
    if (typeof startNode === "string") {
      graphViewer.loadGraph(startNode);
    } else {
      graphViewer.loadGraphWithRandomStart(function(node) {
        if (node && node.errorCode) {
          arangoHelper.arangoError("Graph", "Sorry your graph seems to be empty");
        }
      });
    }
  }

  this.changeWidth = function(w) {
    graphViewer.changeWidth(w);
    var reducedW = w - 55;
    svg.attr("width", reducedW)
      .style("width", reducedW + "px");
  };

  //add events for writing/reading local storage (input fields)


  
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
  width = container.getBoundingClientRect().width;
  height = container.getBoundingClientRect().height;
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
/*global document, Storage, localStorage, window*/
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

  this.applyLocalStorage = function(obj) {
    if (Storage !== "undefined") {
      try {
        var toStore = JSON.parse(localStorage.getItem('graphSettings')),
        graphName = (window.location.hash).split("/")[1],
        dbName = (window.location.pathname).split('/')[2],
        combinedGraphName = graphName + dbName;

        _.each(obj, function(value, key) {
          if (key !== undefined) {
            toStore[combinedGraphName].viewer.nodeShaper[key] = value;
          }
        });

        localStorage.setItem('graphSettings', JSON.stringify(toStore));
      }
      catch (e) {
        console.log(e);
      }
    }
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
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
      modalDialogHelper.createModalChangeDialog("Change label attribute",
        idprefix, [{
          type: "text",
          id: "key"
        }], function () {
          var key = $("#" + idprefix + "key").attr("value");
          var shaperObj = {
            label: key
          };
          self.applyLocalStorage(shaperObj);
          shaper.changeTo(shaperObj);
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
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
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
          var shaperObj = {
            label: key,
            color: {
              type: "attribute",
              key: colourkey
            }
          };
          self.applyLocalStorage(shaperObj);
          shaper.changeTo(shaperObj);
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
    uiComponentsHelper.createButton(list, "Configure Label", prefix, function() {
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

          var shaperObj = {
            label: labels,
            color: {
              type: "attribute",
              key: colours
            }
          };

          self.applyLocalStorage(shaperObj);

          shaper.changeTo(shaperObj);
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

      if (button.id === "control_event_new_node") {
        $('.node').css('cursor', 'pointer');
        $('.gv-background').css('cursor', 'copy');
      }
      else if (button.id === "control_event_drag") {
        $('.node').css('cursor', '-webkit-grabbing');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_expand") {
        $('.node').css('cursor', 'grabbing');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_view") {
        $('.node').css('cursor', '-webkit-zoom-in');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_edit") {
        $('.gv-background .node').css('cursor', 'context-menu');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_connect") {
        $('.node').css('cursor', 'ne-resize');
        $('.gv-background').css('cursor', 'default');
      }
      else if (button.id === "control_event_delete") {
        $('.node').css('cursor', 'pointer');
        $('.gv-background').css('cursor', 'default');
      }

      $(button).toggleClass("active", true);
      callback();
    };
    icon.className = "fa gv_icon_icon fa-" + iconInfo.icon;
    icon.title = iconInfo.title;
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

  this.start = function(expand) {
    layouter.stop();

    if (expand) {
      if ($('.infoField').text() !== '') {
        _.each(nodes, function(node) {
          _.each(adapter.randomNodes, function(compare) {
            if (node._id === compare._id) {
              node._expanded = true;
            }
          });
        });
      }
      else {
        _.each(nodes, function(node) {
          node._expanded = true;
        });
      }
    }

    //expand all wanted nodes
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

  this.loadGraphWithRandomStart = function(callback, size) {
    adapter.loadRandomNode(function (node) {
      if (node.errorCode && node.errorCode === 404) {
        callback(node);
        return;
      }
      node._expanded = true;
      self.start(true);
      if (_.isFunction(callback)) {
        callback();
      }
    }, size);
  };

  this.loadGraphWithAdditionalNode = function(attribute, value, callback) {
    adapter.loadAdditionalNodeByAttributeValue(attribute, value, function (node) {
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

  this.loadGraphWithAttributeValue = function(attribute, value, callback) {

    //clear random and defined nodes
    adapter.randomNodes = [];
    adapter.definedNodes = [];

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

/*jshint unused: false */
/*global window, $, document, arangoHelper, _ */

(function() {
  "use strict";
  var isCoordinator = null;

  window.isCoordinator = function(callback) {
    if (isCoordinator === null) {
      $.ajax(
        "cluster/amICoordinator",
        {
          async: true,
          success: function(d) {
            isCoordinator = d;
            callback(false, d);
          },
          error: function(d) {
            isCoordinator = d;
            callback(true, d);
          }
        }
      );
    }
    else {
      callback(false, isCoordinator);
    }
  };

  window.versionHelper = {
    fromString: function (s) {
      var parts = s.replace(/-[a-zA-Z0-9_\-]*$/g, '').split('.');
      return {
        major: parseInt(parts[0], 10) || 0,
        minor: parseInt(parts[1], 10) || 0,
        patch: parseInt(parts[2], 10) || 0,
        toString: function() {
          return this.major + "." + this.minor + "." + this.patch;
        }
      };
    },
    toString: function (v) {
      return v.major + '.' + v.minor + '.' + v.patch;
    }
  };

  window.arangoHelper = {
    lastNotificationMessage: null,

    CollectionTypes: {},
    systemAttributes: function () {
      return {
        '_id' : true,
        '_rev' : true,
        '_key' : true,
        '_bidirectional' : true,
        '_vertices' : true,
        '_from' : true,
        '_to' : true,
        '$id' : true
      };
    },

    getCurrentSub: function() {
      return window.App.naviView.activeSubMenu;
    },

    parseError: function(title, err) {
      var msg;

      try {
        msg = JSON.parse(err.responseText).errorMessage;
      }
      catch (e) {
        msg = e;
      }

      this.arangoError(title, msg);
    },

    setCheckboxStatus: function(id) {
      _.each($(id).find('ul').find('li'), function(element) {
         if (!$(element).hasClass("nav-header")) {
           if ($(element).find('input').attr('checked')) {
             if ($(element).find('i').hasClass('css-round-label')) {
               $(element).find('i').addClass('fa-dot-circle-o');
             }
             else {
               $(element).find('i').addClass('fa-check-square-o');
             }
           }
           else {
             if ($(element).find('i').hasClass('css-round-label')) {
               $(element).find('i').addClass('fa-circle-o');
             }
             else {
               $(element).find('i').addClass('fa-square-o');
             }
           }
         }
      });
    },

    parseInput: function(element) {
      var parsed,
      string = $(element).val();

      try {
        parsed = JSON.parse(string);
      }
      catch (e) {
        parsed = string;
      }
      
      return parsed;
    },

    calculateCenterDivHeight: function() {
      var navigation = $('.navbar').height();
      var footer = $('.footer').height();
      var windowHeight = $(window).height();

      return windowHeight - footer - navigation - 110;
    },

    fixTooltips: function (selector, placement) {
      $(selector).tooltip({
        placement: placement,
        hide: false,
        show: false
      });
    },

    currentDatabase: function (callback) {
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/database/current",
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(false, data.result.name);
        },
        error: function(data) {
          callback(true, data);
        }
      });
    },

    allHotkeys: {
      /*
      global: {
        name: "Site wide",
        content: [{
          label: "scroll up",
          letter: "j"
        },{
          label: "scroll down",
          letter: "k"
        }]
      },
      */
      jsoneditor: {
        name: "AQL editor",
        content: [{
          label: "Execute Query",
          letter: "Ctrl/Cmd + Return"
        },{
          label: "Explain Query",
          letter: "Ctrl/Cmd + Shift + Return"
        },{
          label: "Save Query",
          letter: "Ctrl/Cmd + Shift + S"
        },{
          label: "Open search",
          letter: "Ctrl + Space"
        },{
          label: "Toggle comments",
          letter: "Ctrl/Cmd + Shift + C"
        },{
          label: "Undo",
          letter: "Ctrl/Cmd + Z"
        },{
          label: "Redo",
          letter: "Ctrl/Cmd + Shift + Z"
        }]
      },
      doceditor: {
        name: "Document editor",
        content: [{
          label: "Insert",
          letter: "Ctrl + Insert"
        },{
          label: "Save",
          letter: "Ctrl + Return, Cmd + Return"
        },{
          label: "Append",
          letter: "Ctrl + Shift + Insert"
        },{
          label: "Duplicate",
          letter: "Ctrl + D"
        },{
          label: "Remove",
          letter: "Ctrl + Delete"
        }]
      },
      modals: {
        name: "Modal",
        content: [{
          label: "Submit",
          letter: "Return"
        },{
          label: "Close",
          letter: "Esc"
        },{
          label: "Navigate buttons",
          letter: "Arrow keys"
        },{
          label: "Navigate content",
          letter: "Tab"
        }]
      }
    },

    hotkeysFunctions: {
      scrollDown: function () {
        window.scrollBy(0,180);
      },
      scrollUp: function () {
        window.scrollBy(0,-180);
      },
      showHotkeysModal: function () {
        var buttons = [],
        content = window.arangoHelper.allHotkeys;

        window.modalView.show("modalHotkeys.ejs", "Keyboard Shortcuts", buttons, content);
      }
    },

    //object: {"name": "Menu 1", func: function(), active: true/false }
    buildSubNavBar: function(menuItems) {
      $('#subNavigationBar .bottom').html('');
      var cssClass;

      _.each(menuItems, function(menu, name) {
        cssClass = '';

        if (menu.active) {
          cssClass += ' active';
        }
        if (menu.disabled) {
          cssClass += ' disabled';
        }

        $('#subNavigationBar .bottom').append(
          '<li class="subMenuEntry ' + cssClass + '"><a>' + name + '</a></li>'
        );
        if (!menu.disabled) {
          $('#subNavigationBar .bottom').children().last().bind('click', function() {
            window.App.navigate(menu.route, {trigger: true});
          });
        }
      });
    },

    buildNodeSubNav: function(node, activeKey, disabled) {
      var menus = {
        Dashboard: {
          route: '#node/' + encodeURIComponent(node)
        },
        Logs: {
          route: '#nLogs/' + encodeURIComponent(node),
          disabled: true
        }
      };

      menus[activeKey].active = true;
      menus[disabled].disabled = true;
      this.buildSubNavBar(menus);
    },

    //nav for cluster/nodes view
    buildNodesSubNav: function(type) {

      var menus = {
        Coordinators: {
          route: '#cNodes'
        },
        DBServers: {
          route: '#dNodes'
        }
      };

      if (type === 'coordinator') {
        menus.Coordinators.active = true;
      }
      else {
        menus.DBServers.active = true;
      }

      this.buildSubNavBar(menus);
    },

    //nav for collection view
    buildCollectionSubNav: function(collectionName, activeKey) {

      var defaultRoute = '#collection/' + encodeURIComponent(collectionName);

      var menus = {
        Content: {
          route: defaultRoute + '/documents/1'
        },
        Indices: {
          route: '#cIndices/' + encodeURIComponent(collectionName)
        },
        Info: {
          route: '#cInfo/' + encodeURIComponent(collectionName)
        },
        Settings: {
          route: '#cSettings/' + encodeURIComponent(collectionName)
        }
      };

      menus[activeKey].active = true;
      this.buildSubNavBar(menus);
    },

    enableKeyboardHotkeys: function (enable) {
      var hotkeys = window.arangoHelper.hotkeysFunctions;
      if (enable === true) {
        $(document).on('keydown', null, 'j', hotkeys.scrollDown);
        $(document).on('keydown', null, 'k', hotkeys.scrollUp);
      }
    },

    databaseAllowed: function (callback) {

      var dbCallback = function(error, db) {
        if (error) {
          arangoHelper.arangoError("","");
        }
        else {
          $.ajax({
            type: "GET",
            cache: false,
            url: "/_db/"+ encodeURIComponent(db) + "/_api/database/",
            contentType: "application/json",
            processData: false,
            success: function() {
              callback(false, true);
            },
            error: function() {
              callback(true, false);
            }
          });
        }
      }.bind(this);

      this.currentDatabase(dbCallback);
    },

    arangoNotification: function (title, content, info) {
      window.App.notificationList.add({title:title, content: content, info: info, type: 'success'});
    },

    arangoError: function (title, content, info) {
      window.App.notificationList.add({title:title, content: content, info: info, type: 'error'});
    },

    hideArangoNotifications: function() {
      $.noty.clearQueue();
      $.noty.closeAll();
    },

    openDocEditor: function (id, type, callback) {
      var ids = id.split("/"),
      self = this;

      var docFrameView = new window.DocumentView({
        collection: window.App.arangoDocumentStore
      });

      docFrameView.breadcrumb = function(){};

      docFrameView.colid = ids[0];
      docFrameView.docid = ids[1];

      docFrameView.el = '.arangoFrame .innerDiv';
      docFrameView.render();
      docFrameView.setType(type);

      //remove header
      $('.arangoFrame .headerBar').remove();
      //append close button
      $('.arangoFrame .outerDiv').prepend('<i class="fa fa-times"></i>');
      //add close events
      $('.arangoFrame .outerDiv').click(function() {
        self.closeDocEditor();
      });
      $('.arangoFrame .innerDiv').click(function(e) {
        e.stopPropagation();
      });
      $('.fa-times').click(function() {
        self.closeDocEditor();
      });

      $('.arangoFrame').show();
       
      docFrameView.customView = true;
      docFrameView.customDeleteFunction = function() {
        window.modalView.hide();
        $('.arangoFrame').hide();
        //callback();
      };

      $('.arangoFrame #deleteDocumentButton').click(function(){
        docFrameView.deleteDocumentModal();
      });
      $('.arangoFrame #saveDocumentButton').click(function(){
        docFrameView.saveDocument();
      });
      $('.arangoFrame #deleteDocumentButton').css('display', 'none');
    },

    closeDocEditor: function () {
      $('.arangoFrame .outerDiv .fa-times').remove();
      $('.arangoFrame').hide();
    },

    addAardvarkJob: function (object, callback) {
      $.ajax({
          cache: false,
          type: "POST",
          url: "/_admin/aardvark/job",
          data: JSON.stringify(object),
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (callback) {
              callback(false, data);
            }
          },
          error: function(data) {
            if (callback) {
              callback(true, data);
            }
          }
      });
    },

    deleteAardvarkJob: function (id, callback) {
      $.ajax({
          cache: false,
          type: "DELETE",
          url: "/_admin/aardvark/job/" + encodeURIComponent(id),
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (callback) {
              callback(false, data);
            }
          },
          error: function(data) {
            if (callback) {
              callback(true, data);
            }
          }
      });
    },

    deleteAllAardvarkJobs: function (callback) {
      $.ajax({
          cache: false,
          type: "DELETE",
          url: "/_admin/aardvark/job",
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (callback) {
              callback(false, data);
            }
          },
          error: function(data) {
            if (callback) {
              callback(true, data);
            }
          }
      });
    },

    getAardvarkJobs: function (callback) {
      $.ajax({
          cache: false,
          type: "GET",
          url: "/_admin/aardvark/job",
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (callback) {
              callback(false, data);
            }
          },
          error: function(data) {
            if (callback) {
              callback(true, data);
            }
          }
      });
    },

    getPendingJobs: function(callback) {

      $.ajax({
          cache: false,
          type: "GET",
          url: "/_api/job/pending",
          contentType: "application/json",
          processData: false,
          success: function (data) {
            callback(false, data);
          },
          error: function(data) {
            callback(true, data);
          }
      });
    },

    syncAndReturnUninishedAardvarkJobs: function(type, callback) {

      var callbackInner = function(error, AaJobs) {
        if (error) {
          callback(true);
        }
        else {

          var callbackInner2 = function(error, pendingJobs) {
            if (error) {
              arangoHelper.arangoError("", "");
            }
            else {
              var array = [];
              if (pendingJobs.length > 0) {
                _.each(AaJobs, function(aardvark) {
                  if (aardvark.type === type || aardvark.type === undefined) {

                     var found = false; 
                    _.each(pendingJobs, function(pending) {
                      if (aardvark.id === pending) {
                        found = true;
                      } 
                    });

                    if (found) {
                      array.push({
                        collection: aardvark.collection,
                        id: aardvark.id,
                        type: aardvark.type,
                        desc: aardvark.desc 
                      });
                    }
                    else {
                      window.arangoHelper.deleteAardvarkJob(aardvark.id);
                    }
                  }
                });
              }
              else {
                if (AaJobs.length > 0) {
                  this.deleteAllAardvarkJobs(); 
                }
              }
              callback(false, array);
            }
          }.bind(this);

          this.getPendingJobs(callbackInner2);

        }
      }.bind(this);

      this.getAardvarkJobs(callbackInner);
    }, 

    getRandomToken: function () {
      return Math.round(new Date().getTime());
    },

    isSystemAttribute: function (val) {
      var a = this.systemAttributes();
      return a[val];
    },

    isSystemCollection: function (val) {
      return val.name.substr(0, 1) === '_';
    },

    setDocumentStore : function (a) {
      this.arangoDocumentStore = a;
    },

    collectionApiType: function (identifier, refresh, toRun) {
      // set "refresh" to disable caching collection type
      if (refresh || this.CollectionTypes[identifier] === undefined) {
        var callback = function(error, data, toRun) {
          if (error) {
            arangoHelper.arangoError("Error", "Could not detect collection type");
          }
          else {
            this.CollectionTypes[identifier] = data.type;
            if (this.CollectionTypes[identifier] === 3) {
              toRun(false, "edge");
            }
            else {
              toRun(false, "document");
            }
          }
        }.bind(this);
        this.arangoDocumentStore.getCollectionInfo(identifier, callback, toRun);
      }
      else {
        toRun(false, this.CollectionTypes[identifier]);
      }
    },

    collectionType: function (val) {
      if (! val || val.name === '') {
        return "-";
      }
      var type;
      if (val.type === 2) {
        type = "document";
      }
      else if (val.type === 3) {
        type = "edge";
      }
      else {
        type = "unknown";
      }

      if (this.isSystemCollection(val)) {
        type += " (system)";
      }

      return type;
    },

    formatDT: function (dt) {
      var pad = function (n) {
        return n < 10 ? '0' + n : n;
      };

      return dt.getUTCFullYear() + '-'
      + pad(dt.getUTCMonth() + 1) + '-'
      + pad(dt.getUTCDate()) + ' '
      + pad(dt.getUTCHours()) + ':'
      + pad(dt.getUTCMinutes()) + ':'
      + pad(dt.getUTCSeconds());
    },

    escapeHtml: function (val) {
      // HTML-escape a string
      return String(val).replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
    }

  };
}());

/*jshint unused: false */
/*global EJS, window, _, $*/
(function() {
  "use strict";
  // For tests the templates are loaded some where else.
  // We need to use a different engine there.
  if (!window.hasOwnProperty("TEST_BUILD")) {
    var TemplateEngine = function() {
      var exports = {};
      exports.createTemplate = function(id) {
        var template = $("#" + id.replace(".", "\\.")).html();
        return {
          render: function(params) {
            var tmp = _.template(template);
            tmp = tmp(params);

            return tmp;
          }
        };
      };
      return exports;
    };
    window.templateEngine = new TemplateEngine();
  }
}());

/*jshint node:false, browser:true, strict: false, unused: false */
/*global global:true, $, jqconsole */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB web browser shell
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012-2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            Module
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief Module constructor
////////////////////////////////////////////////////////////////////////////////

function Module (id) {
  this.id = id;
  this.exports = {};
  this.definition = null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief module cache
////////////////////////////////////////////////////////////////////////////////

Module.prototype.moduleCache = {};
Module.prototype.moduleCache["/internal"] = new Module("/internal");

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

if (typeof global === 'undefined' && typeof window !== 'undefined') {
  global = window;
}

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief module
////////////////////////////////////////////////////////////////////////////////

global.module = Module.prototype.moduleCache["/"] = new Module("/");

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief normalises a path
////////////////////////////////////////////////////////////////////////////////

Module.prototype.normalise = function (path) {
  var i;
  var n;
  var p;
  var q;
  var x;

  if (path === "") {
    return this.id;
  }

  p = path.split('/');

  // relative path
  if (p[0] === "." || p[0] === "..") {
    q = this.id.split('/');
    q.pop();
    q = q.concat(p);
  }

  // absolute path
  else {
    q = p;
  }

  // normalize path
  n = [];

  for (i = 0;  i < q.length;  ++i) {
    x = q[i];

    if (x === "..") {
      if (n.length === 0) {
        throw "cannot cross module top";
      }

      n.pop();
    }
    else if (x !== "" && x !== ".") {
      n.push(x);
    }
  }

  return "/" + n.join('/');
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief define
////////////////////////////////////////////////////////////////////////////////

Module.prototype.define = function (path, definition) {

  // first get rid of any ".." and "."
  path = this.normalise(path);
  var match = path.match(/(.+)\/index$/);
  if (match) {
    path = match[1];
  }

  // check if you already know the module, return the exports
  if (! Module.prototype.moduleCache.hasOwnProperty(path)) {
    Module.prototype.moduleCache[path] = new Module(path);
  }

  Module.prototype.moduleCache[path].definition = definition;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief require
////////////////////////////////////////////////////////////////////////////////

Module.prototype.require = function (path) {
  var module;

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (Module.prototype.moduleCache.hasOwnProperty(path)) {
    module = Module.prototype.moduleCache[path];
  }
  else {
    module = Module.prototype.moduleCache[path] = new Module(path);
  }

  if (module.definition !== null) {
    var definition;

    definition = module.definition;
    module.definition = null;
    definition.call(window, module.exports, module);
  }
  return module.exports;
};

function require (path) {
  return global.module.require(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief print
////////////////////////////////////////////////////////////////////////////////

function print () {
  var internal = require("internal");
  internal.print.apply(internal.print, arguments);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoConnection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief arango server connection
////////////////////////////////////////////////////////////////////////////////

function ArangoConnection () {
  this._databaseName = "_system";

  var path = global.document.location.pathname;

  if (path.substr(0, 5) === '/_db/') {
    var i = 5, n = path.length;
    while (i < n) {
      if (path[i] === '/') {
        break;
      }
      i++;
    }

    if (i > 5) {
      this._databaseName = path.substring(5, i);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief getDatabaseName
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.getDatabaseName = function () {
  return this._databaseName;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief setDatabaseName
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.setDatabaseName = function (name) {
  this._databaseName = name;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief databasePrefix
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.databasePrefix = function (url) {
  if (url.substr(0, 7) === 'http://' || url.substr(0, 8) === 'https://') {
    return url;
  }

  if (url.substr(0, 5) !== '/_db/') {
    if (url[0] === '/') {
      // relative URL, starting at /
      return "/_db/" + this.getDatabaseName() + url;
    }
  }

  // everything else
  return url;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.get = function (url) {
  var msg = null;

  $.ajax({
    async: false,
    cache: false,
    type: "GET",
    url: url,
    contentType: "application/json",
    dataType: "json",
    processData: false,
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;
};

ArangoConnection.prototype.GET = ArangoConnection.prototype.get;

////////////////////////////////////////////////////////////////////////////////
/// @brief delete
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype._delete = function (url) {
  var msg = null;

  $.ajax({
    async: false,
    type: "DELETE",
    url: url,
    contentType: "application/json",
    dataType: "json",
    processData: false,
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;
};

ArangoConnection.prototype.DELETE = ArangoConnection.prototype._delete;

////////////////////////////////////////////////////////////////////////////////
/// @brief post
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.post = function (url, body) {
  var msg = null;

  $.ajax({
    async: false,
    type: "POST",
    url: url,
    data: body,
    contentType: "application/json",
    dataType: "json",
    processData: false,
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;
};

ArangoConnection.prototype.POST = ArangoConnection.prototype.post;

////////////////////////////////////////////////////////////////////////////////
/// @brief put
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.put = function (url, body) {
  var msg = null;

  $.ajax({
    async: false,
    type: "PUT",
    url: url,
    data: body,
    contentType: "application/json",
    dataType: "json",
    processData: false,
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;
};

ArangoConnection.prototype.PUT = ArangoConnection.prototype.put;

////////////////////////////////////////////////////////////////////////////////
/// @brief patch
////////////////////////////////////////////////////////////////////////////////

ArangoConnection.prototype.patch = function (url, body) {
  var msg = null;

  $.ajax({
    async: false,
    type: "PATCH",
    url: url,
    data: body,
    contentType: "application/json",
    dataType: "json",
    processData: false,
    success: function(data) {
      msg = data;
    },
    error: function(data) {
      try {
        msg = JSON.parse(data.responseText);
      }
      catch (err) {
        msg = data.responseText;
      }
    }
  });

  return msg;
};

ArangoConnection.prototype.PATCH = ArangoConnection.prototype.patch;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

(function () {
  var internal = Module.prototype.moduleCache["/internal"].exports;

////////////////////////////////////////////////////////////////////////////////
/// @brief arango
////////////////////////////////////////////////////////////////////////////////

  internal.arango = new ArangoConnection();

////////////////////////////////////////////////////////////////////////////////
/// @brief browserOutputBuffer
////////////////////////////////////////////////////////////////////////////////

  internal.browserOutputBuffer = "";

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to shell window
////////////////////////////////////////////////////////////////////////////////

  internal.output = function () {
    var i;

    for (i = 0;  i < arguments.length;  ++i) {
      var value = arguments[i];
      var text;

      if (value === null) {
        text = "null";
      }
      else if (value === undefined) {
        text = "undefined";
      }
      else if (typeof(value) === "object") {
        try {
          text = JSON.stringify(value);
        }
        catch (err) {
          text = String(value);
        }
      }
      else {
        text = String(value);
      }

      require('internal').browserOutputBuffer += text;
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief outputs text to browser window
////////////////////////////////////////////////////////////////////////////////

  internal.print = internal.printBrowser = function () {
    require('internal').printShell.apply(require('internal').printShell, arguments);

    jqconsole.Write('==> ' + require('internal').browserOutputBuffer, 'jssuccess');
    require('internal').browserOutputBuffer = "";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief globally rewrite URLs for AJAX requests to contain the database name
////////////////////////////////////////////////////////////////////////////////

  $(global.document).ajaxSend(function(event, jqxhr, settings) {
    settings.url = require('internal').arango.databasePrefix(settings.url);
  });

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

  global.DEFINE_MODULE = function (name, exports) {
    var path = Module.prototype.normalise(name);
    var module = Module.prototype.moduleCache[path];
    if (module) {
      Object.keys(module.exports).forEach(function (key) {
        exports[key] = module.exports[key];
      });
    } else {
      module = new Module(path);
      Module.prototype.moduleCache[path] = module;
    }
    module.exports = exports;
  };

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}"
// End:

"use strict";

/*global _, Dygraph, window, document */

(function () {
  "use strict";
  window.dygraphConfig = {
    defaultFrame : 20 * 60 * 1000,

    zeropad: function (x) {
      if (x < 10) {
        return "0" + x;
      }
      return x;
    },

    xAxisFormat: function (d) {
      if (d === -1) {
        return "";
      }
      var date = new Date(d);
      return this.zeropad(date.getHours()) + ":"
        + this.zeropad(date.getMinutes()) + ":"
        + this.zeropad(date.getSeconds());
    },

    mergeObjects: function (o1, o2, mergeAttribList) {
      if (!mergeAttribList) {
        mergeAttribList = [];
      }
      var vals = {}, res;
      mergeAttribList.forEach(function (a) {
        var valO1 = o1[a],
        valO2 = o2[a];
        if (valO1 === undefined) {
          valO1 = {};
        }
        if (valO2 === undefined) {
          valO2 = {};
        }
        vals[a] = _.extend(valO1, valO2);
      });
      res = _.extend(o1, o2);
      Object.keys(vals).forEach(function (k) {
        res[k] = vals[k];
      });
      return res;
    },

    mapStatToFigure : {
      residentSize : ["times", "residentSizePercent"],
      pageFaults : ["times", "majorPageFaultsPerSecond", "minorPageFaultsPerSecond"],
      systemUserTime : ["times", "systemTimePerSecond", "userTimePerSecond"],
      totalTime : ["times", "avgQueueTime", "avgRequestTime", "avgIoTime"],
      dataTransfer : ["times", "bytesSentPerSecond", "bytesReceivedPerSecond"],
      requests : ["times", "getsPerSecond", "putsPerSecond", "postsPerSecond",
                  "deletesPerSecond", "patchesPerSecond", "headsPerSecond",
                  "optionsPerSecond", "othersPerSecond"]
    },

    //colors for dygraphs
    colors: ["rgb(95, 194, 135)", "rgb(238, 190, 77)", "#81ccd8", "#7ca530", "#3c3c3c",
             "#aa90bd", "#e1811d", "#c7d4b2", "#d0b2d4"],


    // figure dependend options
    figureDependedOptions: {
      clusterRequestsPerSecond: {
        showLabelsOnHighlight: true,
        title: '',
        header : "Cluster Requests per Second",
        stackedGraph: true,
        div: "lineGraphLegend",
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }

              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      residentSize: {
        header: "Memory",
        axes: {
          y: {
            labelsKMG2: false,
            axisLabelFormatter: function (y) {
              return parseFloat(y.toPrecision(3) * 100) + "%";
            },
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3) * 100) + "%";
            }
          }
        }
      },

      pageFaults: {
        header : "Page Faults",
        visibility: [true, false],
        labels: ["datetime", "Major Page", "Minor Page"],
        div: "pageFaultsChart",
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      systemUserTime: {
        div: "systemUserTimeChart",
        header: "System and User Time",
        labels: ["datetime", "System Time", "User Time"],
        stackedGraph: true,
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      },

      totalTime: {
        div: "totalTimeChart",
        header: "Total Time",
        labels: ["datetime", "Queue", "Computation", "I/O"],
        labelsKMG2: false,
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        },
        stackedGraph: true
      },

      dataTransfer: {
        header: "Data Transfer",
        labels: ["datetime", "Bytes sent", "Bytes received"],
        stackedGraph: true,
        div: "dataTransferChart"
      },

      requests: {
        header: "Requests",
        labels: ["datetime", "Reads", "Writes"],
        stackedGraph: true,
        div: "requestsChart",
        axes: {
          y: {
            valueFormatter: function (y) {
              return parseFloat(y.toPrecision(3));
            },
            axisLabelFormatter: function (y) {
              if (y === 0) {
                return 0;
              }
              return parseFloat(y.toPrecision(3));
            }
          }
        }
      }
    },

    getDashBoardFigures : function (all) {
      var result = [], self = this;
      Object.keys(this.figureDependedOptions).forEach(function (k) {
        // ClusterRequestsPerSecond should not be ignored. Quick Fix
        if (k !== "clusterRequestsPerSecond" && (self.figureDependedOptions[k].div || all)) {
          result.push(k);
        }
      });
      return result;
    },

    //configuration for chart overview
    getDefaultConfig: function (figure) {
      var self = this;
      var result = {
        digitsAfterDecimal: 1,
        drawGapPoints: true,
        fillGraph: true,
        fillAlpha: 0.85,
        showLabelsOnHighlight: false,
        strokeWidth: 0.0,
        lineWidth: 0.0,
        strokeBorderWidth: 0.0,
        includeZero: true,
        highlightCircleSize: 2.5,
        labelsSeparateLines : true,
        strokeBorderColor: 'rgba(0,0,0,0)',
        interactionModel: {},
        maxNumberWidth : 10,
        colors: [this.colors[0]],
        xAxisLabelWidth: "50",
        rightGap: 15,
        showRangeSelector: false,
        rangeSelectorHeight: 50,
        rangeSelectorPlotStrokeColor: '#365300',
        rangeSelectorPlotFillColor: '',
        // rangeSelectorPlotFillColor: '#414a4c',
        pixelsPerLabel: 50,
        labelsKMG2: true,
        dateWindow: [
          new Date().getTime() -
            this.defaultFrame,
          new Date().getTime()
        ],
        axes: {
          x: {
            valueFormatter: function (d) {
              return self.xAxisFormat(d);
            }
          },
          y: {
            ticker: Dygraph.numericLinearTicks
          }
        }
      };
      if (this.figureDependedOptions[figure]) {
        result = this.mergeObjects(
          result, this.figureDependedOptions[figure], ["axes"]
        );
        if (result.div && result.labels) {
          result.colors = this.getColors(result.labels);
          result.labelsDiv = document.getElementById(result.div + "Legend");
          result.legend = "always";
          result.showLabelsOnHighlight = true;
        }
      }
      return result;

    },

    getDetailChartConfig: function (figure) {
      var result = _.extend(
        this.getDefaultConfig(figure),
        {
          showRangeSelector: true,
          interactionModel: null,
          showLabelsOnHighlight: true,
          highlightCircleSize: 2.5,
          legend: "always",
          labelsDiv: "div#detailLegend.dashboard-legend-inner"
        }
      );
      if (figure === "pageFaults") {
        result.visibility = [true, true];
      }
      if (!result.labels) {
        result.labels = ["datetime", result.header];
        result.colors = this.getColors(result.labels);
      }
      return result;
    },

    getColors: function (labels) {
      var colorList;
      colorList = this.colors.concat([]);
      return colorList.slice(0, labels.length - 1);
    }
  };
}());

/*global window, Backbone, $, arangoHelper */
(function() {
  'use strict';
  window.arangoCollectionModel = Backbone.Model.extend({

    idAttribute: "name",

    urlRoot: "/_api/collection",
    defaults: {
      id: "",
      name: "",
      status: "",
      type: "",
      isSystem: false,
      picture: "",
      locked: false,
      desc: undefined
    },

    getProperties: function (callback) {
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/collection/" + encodeURIComponent(this.get("id")) + "/properties",
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(false, data);
        },
        error: function(data) {
          callback(true, data);
        }
      });
    },
    getFigures: function (callback) {
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/collection/" + this.get("id") + "/figures",
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(false, data);
        },
        error: function() {
          callback(true);
        }
      });
    },
    getRevision: function (callback, figures) {
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/collection/" + this.get("id") + "/revision",
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(false, data, figures);
        },
        error: function() {
          callback(true);
        }
      });
    },

    getIndex: function (callback) {
      $.ajax({
        type: "GET",
        cache: false,
        url: "/_api/index/?collection=" + this.get("id"),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(false, data);
        },
        error: function(data) {
          callback(true, data);
        }
      });
    },

    createIndex: function (postParameter, callback) {

      var self = this;

      $.ajax({
          cache: false,
          type: "POST",
          url: "/_api/index?collection="+ self.get("id"),
          headers: {
            'x-arango-async': 'store' 
          },
          data: JSON.stringify(postParameter),
          contentType: "application/json",
          processData: false,
          success: function (data, textStatus, xhr) {
            if (xhr.getResponseHeader('x-arango-async-id')) {
              window.arangoHelper.addAardvarkJob({
                id: xhr.getResponseHeader('x-arango-async-id'),
                type: 'index',
                desc: 'Creating Index',
                collection: self.get("id")
              });
              callback(false, data);
            }
            else {
              callback(true, data);
            }
          },
          error: function(data) {
            callback(true, data);
          }
      });
    },

    deleteIndex: function (id, callback) {

      var self = this;

      $.ajax({
          cache: false,
          type: 'DELETE',
          url: "/_api/index/"+ this.get("name") +"/"+encodeURIComponent(id),
          headers: {
            'x-arango-async': 'store' 
          },
          success: function (data, textStatus, xhr) {
            if (xhr.getResponseHeader('x-arango-async-id')) {
              window.arangoHelper.addAardvarkJob({
                id: xhr.getResponseHeader('x-arango-async-id'),
                type: 'index',
                desc: 'Removing Index',
                collection: self.get("id")
              });
              callback(false, data);
            }
            else {
              callback(true, data);
            }
          },
          error: function (data) {
            callback(true, data);
          }
        });
      callback();
    },

    truncateCollection: function () {
      $.ajax({
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/truncate",
        success: function () {
          arangoHelper.arangoNotification('Collection truncated.');
        },
        error: function () {
          arangoHelper.arangoError('Collection error.');
        }
      });
    },

    loadCollection: function (callback) {

      $.ajax({
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/load",
        success: function () {
          callback(false);
        },
        error: function () {
          callback(true);
        }
      });
      callback();
    },

    unloadCollection: function (callback) {
      $.ajax({
        cache: false,
        type: 'PUT',
        url: "/_api/collection/" + this.get("id") + "/unload?flush=true",
        success: function () {
          callback(false);
        },
        error: function () {
          callback(true);
        }
      });
      callback();
    },

    renameCollection: function (name, callback) {
      var self = this;

      $.ajax({
        cache: false,
        type: "PUT",
        url: "/_api/collection/" + this.get("id") + "/rename",
        data: JSON.stringify({ name: name }),
        contentType: "application/json",
        processData: false,
        success: function() {
          self.set("name", name);
          callback(false);
        },
        error: function(data) {
          callback(true, data);
        }
      });
    },

    changeCollection: function (wfs, journalSize, indexBuckets, callback) {
      var result = false;
      if (wfs === "true") {
        wfs = true;
      }
      else if (wfs === "false") {
        wfs = false;
      }
      var data = {
        waitForSync: wfs,
        journalSize: parseInt(journalSize),
        indexBuckets: parseInt(indexBuckets)
      };

      $.ajax({
        cache: false,
        type: "PUT",
        url: "/_api/collection/" + this.get("id") + "/properties",
        data: JSON.stringify(data),
        contentType: "application/json",
        processData: false,
        success: function() {
          callback(false);
        },
        error: function(data) {
          callback(false, data);
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

  checkPassword: function(passwd, callback) {
    $.ajax({
      cache: false,
      type: "POST",
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({ passwd: passwd }),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },

  setPassword: function(passwd) {
    $.ajax({
      cache: false,
      type: "PATCH",
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({ passwd: passwd }),
      contentType: "application/json",
      processData: false
    });
  },

  setExtras: function(name, img, callback) {
    $.ajax({
      cache: false,
      type: "PATCH",
      url: "/_api/user/" + this.get("user"),
      data: JSON.stringify({"extra": {"name":name, "img":img}}),
      contentType: "application/json",
      processData: false,
      success: function() {
        callback(false);
      },
      error: function() {
        callback(true);
      }
    });
  }

});

/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterCoordinator = Backbone.Model.extend({

    defaults: {
      "name": "",
      "status": "ok",
      "address": "",
      "protocol": ""
    },

    idAttribute: "name",
    /*
    url: "/_admin/aardvark/cluster/Coordinators";

    updateUrl: function() {
      this.url = window.getNewRoute("Coordinators");
    },
    */
    forList: function() {
      return {
        name: this.get("name"),
        status: this.get("status"),
        url: this.get("url")
      };
    }

  });
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.ClusterServer = Backbone.Model.extend({
    defaults: {
      name: "",
      address: "",
      role: "",
      status: "ok"
    },

    idAttribute: "name",
    /*
    url: "/_admin/aardvark/cluster/DBServers";

    updateUrl: function() {
      this.url = window.getNewRoute("DBServers");
    },
    */
    forList: function() {
      return {
        name: this.get("name"),
        address: this.get("address"),
        status: this.get("status")
      };
    }

  });
}());


/*global window, Backbone */
(function() {
  "use strict";

  window.Coordinator = Backbone.Model.extend({

    defaults: {
      address: "",
      protocol: "",
      name: "",
      status: ""
    }

  });
}());

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
/*global Backbone, $, _, arango */
(function () {
  "use strict";

  var sendRequest = function (foxx, callback, method, part, body, args) {
    var req = {
      contentType: "application/json",
      processData: false,
      type: method
    };
    callback = callback || function () {};
    args = _.extend({mount: foxx.encodedMount()}, args);
    var qs = _.reduce(args, function (base, value, key) {
      return base + encodeURIComponent(key) + '=' + encodeURIComponent(value) + '&';
    }, '?');
    req.url = "/_admin/aardvark/foxxes" + (part ? '/' + part : '') + qs.slice(0, qs.length - 1);
    if (body !== undefined) {
      req.data = JSON.stringify(body);
    }
    $.ajax(req).then(
      function (data) {
        callback(null, data);
      },
      function (xhr) {
        window.xhr = xhr;
        callback(_.extend(
          xhr.status
          ? new Error(xhr.responseJSON ? xhr.responseJSON.errorMessage : xhr.responseText)
          : new Error('Network Error'),
          {statusCode: xhr.status}
        ));
      }
    );
  };

  window.Foxx = Backbone.Model.extend({
    idAttribute: "mount",

    defaults: {
      "author": "Unknown Author",
      "name": "",
      "version": "Unknown Version",
      "description": "No description",
      "license": "Unknown License",
      "contributors": [],
      "scripts": {},
      "config": {},
      "deps": {},
      "git": "",
      "system": false,
      "development": false
    },

    isNew: function () {
      return false;
    },

    encodedMount: function () {
      return encodeURIComponent(this.get("mount"));
    },

    destroy: function (options, callback) {
      sendRequest(this, callback, "DELETE", undefined, undefined, options);
    },

    isBroken: function () {
      return false;
    },

    needsAttention: function () {
      return this.isBroken() || this.needsConfiguration() || this.hasUnconfiguredDependencies();
    },

    needsConfiguration: function () {
      return _.any(this.get('config'), function (cfg) {
        return cfg.current === undefined && cfg.required !== false;
      });
    },

    hasUnconfiguredDependencies: function () {
      return _.any(this.get('deps'), function (dep) {
        return dep.current === undefined && dep.definition.required !== false;
      });
    },

    getConfiguration: function (callback) {
      sendRequest(this, function (err, data) {
        if (!err) {
          this.set("config", data);
        }
        if (typeof callback === "function") {
          callback(err, data);
        }
      }.bind(this), "GET", "config");
    },

    setConfiguration: function (data, callback) {
      sendRequest(this, callback, "PATCH", "config", data);
    },

    getDependencies: function (callback) {
      sendRequest(this, function (err, data) {
        if (!err) {
          this.set("deps", data);
        }
        if (typeof callback === "function") {
          callback(err, data);
        }
      }.bind(this), "GET", "deps");
    },

    setDependencies: function (data, callback) {
      sendRequest(this, callback, "PATCH", "deps", data);
    },

    toggleDevelopment: function (activate, callback) {
      sendRequest(this, function (err, data) {
        if (!err) {
          this.set("development", activate);
        }
        if (typeof callback === "function") {
          callback(err, data);
        }
      }.bind(this), "PATCH", "devel", activate);
    },

    runScript: function (name, options, callback) {
      sendRequest(this, callback, "POST", "scripts/" + name, options);
    },

    runTests: function (options, callback) {
      sendRequest(this, function (err, data) {
        if (typeof callback === "function") {
          callback(err ? err.responseJSON : err, data);
        }
      }.bind(this), "POST", "tests", options);
    },

    isSystem: function () {
      return this.get("system");
    },

    isDevelopment: function () {
      return this.get("development");
    },

    download: function () {
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

/*global window, Backbone */
(function() {
  "use strict";

  window.workMonitorModel = Backbone.Model.extend({

    defaults: {
      name: "",
      number: "",
      status: "",
      type: ""
    }

  });
}());

/*global window, Backbone */
(function() {
  "use strict";

  window.AutomaticRetryCollection = Backbone.Collection.extend({

    _retryCount: 0,


    checkRetries: function() {
      var self = this;
      this.updateUrl();
      if (this._retryCount > 10) {
        window.setTimeout(function() {
          self._retryCount = 0;
        }, 10000);
        window.App.clusterUnreachable();
        return false;
      }
      return true;
    },

    successFullTry: function() {
      this._retryCount = 0;
    },

    failureTry: function(retry, ignore, err) {
      if (err.status === 401) {
        window.App.requestAuth();
      } else {
        window.App.clusterPlan.rotateCoordinator();
        this._retryCount++;
        retry();
      }
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

/*global Backbone, window */
/* jshint strict: false */

window.ClusterStatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,

  url: "/_admin/statistics",

  updateUrl: function() {
    this.url = window.App.getNewRoute(this.host) + this.url;
  },

  initialize: function(models, options) {
    this.host = options.host;
    window.App.registerForUpdate(this);
  },

  // The callback has to be invokeable for each result individually
  // TODO RE-ADD Auth
  /*
  fetch: function(callback, errCB) {
    this.forEach(function (m) {
      m.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: function() {
          errCB(m);
        }
      }).done(function() {
        callback(m);
      });
    });
  }*/
});

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, window, arangoCollectionModel, $, arangoHelper, data, _ */
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
            return 'unloading';
          case 5:
            return 'deleted';
          case 6:
            return 'loading';
          default:
            return;
        }
      },

      translateTypePicture : function (type) {
        var returnString = "";
        switch (type) {
          case 'document':
            returnString += "fa-file-text-o";
            break;
          case 'edge':
            returnString += "fa-share-alt";
            break;
          case 'unknown':
            returnString += "fa-question";
            break;
          default:
            returnString += "fa-cogs";
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

      newCollection: function (object, callback) {
        var data = {};
        data.name = object.collName;
        data.waitForSync = object.wfs;
        if (object.journalSize > 0) {
          data.journalSize = object.journalSize;
        }
        data.isSystem = object.isSystem;
        data.type = parseInt(object.collType, 10);
        if (object.shards) {
          data.numberOfShards = object.shards;
          data.shardKeys = object.keys;
        }

        $.ajax({
          cache: false,
          type: "POST",
          url: "/_api/collection",
          data: JSON.stringify(data),
          contentType: "application/json",
          processData: false,
          success: function(data) {
            callback(false, data);
          },
          error: function(data) {
            callback(true, data);
          }
        });
      }
  });
}());

/*jshint browser: true */
/*global window, arangoHelper, Backbone, $, window, _*/

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

    getDatabasesForUser: function(callback) {
      $.ajax({
        type: "GET",
        cache: false,
        url: this.url + "/user",
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(false, (data.result).sort());
        },
        error: function() {
          callback(true, []);
        }
      });
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
        if (base.indexOf("#service") === 0) {
          base = "#services";
        }
        url += base;
      }
      return url;
    },

    getCurrentDatabase: function(callback) {
      $.ajax({
        type: "GET",
        cache: false,
        url: this.url + "/current",
        contentType: "application/json",
        processData: false,
        success: function(data) {
          if (data.code === 200) {
            callback(false, data.result.name);
          }
          else {
            callback(false, data);
          }
        },
        error: function(data) {
          callback(true, data);
        }
      });
    },

    hasSystemAccess: function(callback) {

      var callback2 = function(error, list) {
        if (error) {
          arangoHelper.arangoError("DB","Could not fetch databases");
        }
        else {
          callback(false, _.contains(list, "_system"));
        }
      }.bind(this);

      this.getDatabasesForUser(callback2);
    }
  });
}());

/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, window, arangoDocument, arangoDocumentModel, $, arangoHelper */

window.arangoDocument = Backbone.Collection.extend({
  url: '/_api/document/',
  model: arangoDocumentModel,
  collectionInfo: {},

  deleteEdge: function (colid, docid, callback) {
    this.deleteDocument(colid, docid, callback);
  },

  deleteDocument: function (colid, docid, callback) {
    $.ajax({
      cache: false,
      type: 'DELETE',
      contentType: "application/json",
      url: "/_api/document/" + encodeURIComponent(colid) + "/" + encodeURIComponent(docid),
      success: function () {
        callback(false);
      },
      error: function () {
        callback(true);
      }
    });
  },
  addDocument: function (collectionID, key) {
    var self = this;
    self.createTypeDocument(collectionID, key);
  },
  createTypeEdge: function (collectionID, from, to, key, callback) {
    var newEdge;

    if (key) {
      newEdge = JSON.stringify({
        _key: key,
        _from: from,
        _to: to
      });
    }
    else {
      newEdge = JSON.stringify({
        _from: from,
        _to: to
      });
    }

    $.ajax({
      cache: false,
      type: "POST",
      url: "/_api/document?collection=" + encodeURIComponent(collectionID),
      data: newEdge,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },
  createTypeDocument: function (collectionID, key, callback) {
    var newDocument;

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
      url: "/_api/document?collection=" + encodeURIComponent(collectionID),
      data: newDocument,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data._id);
      },
      error: function(data) {
        callback(true, data._id);
      }
    });
  },
  getCollectionInfo: function (identifier, callback, toRun) {
    var self = this;

    $.ajax({
      cache: false,
      type: "GET",
      url: "/_api/collection/" + identifier + "?" + arangoHelper.getRandomToken(),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.collectionInfo = data;
        callback(false, data, toRun);
      },
      error: function(data) {
        callback(true, data, toRun);
      }
    });
  },
  getEdge: function (colid, docid, callback){
    this.getDocument(colid, docid, callback);
  },
  getDocument: function (colid, docid, callback) {
    var self = this;
    this.clearDocument();
    $.ajax({
      cache: false,
      type: "GET",
      url: "/_api/document/" + encodeURIComponent(colid) +"/"+ encodeURIComponent(docid),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.add(data);
        callback(false, data, 'document');
      },
      error: function(data) {
        self.add(true, data);
      }
    });
  },
  saveEdge: function (colid, docid, model, callback) {
    $.ajax({
      cache: false,
      type: "PUT",
      url: "/_api/edge/" + encodeURIComponent(colid) + "/" + encodeURIComponent(docid),
      data: model,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },
  saveDocument: function (colid, docid, model, callback) {
    $.ajax({
      cache: false,
      type: "PUT",
      url: "/_api/document/" + encodeURIComponent(colid) + "/" + encodeURIComponent(docid),
      data: model,
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
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
/*global window, Backbone, arangoDocumentModel, _, arangoHelper, $*/
(function() {
  "use strict";

  window.arangoDocuments = window.PaginatedCollection.extend({
    collectionID: 1,

    filters: [],
    checkCursorTimer: undefined,

    MAX_SORT: 12000,

    lastQuery: {},
    sortAttribute: "",

    url: '/_api/documents',
    model: window.arangoDocumentModel,

    loadTotal: function(callback) {
      var self = this;
      $.ajax({
        cache: false,
        type: "GET",
        url: "/_api/collection/" + this.collectionID + "/count",
        contentType: "application/json",
        processData: false,
        success: function(data) {
          self.setTotal(data.count);
          callback(false);
        },
        error: function() {
          callback(true);
        }
      });
    },

    setCollection: function(id) {
      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("Documents","Could not fetch documents count");
        }
      }.bind(this);
      this.resetFilter();
      this.collectionID = id;
      this.setPage(1);
      this.loadTotal(callback);
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
      var query = " FILTER", res = '',
      parts = _.map(this.filters, function(f, i) {
        if (f.op === 'LIKE') {
          res = " " + f.op + "(x.`" + f.attr + "`, @param";
          res += i;
          res += ")";
        }
        else {
          if (f.op === 'IN' || f.op === 'NOT IN') {
            res = ' ';
          }
          else {
            res = " x.`";
          }

          res += f.attr;

          if (f.op === 'IN' || f.op === 'NOT IN') {
            res += " ";
          }
          else {
            res += "` ";
          }

          res += f.op;

          if (f.op === 'IN' || f.op === 'NOT IN') {
            res += " x.@param";
          }
          else {
            res += " @param";
          }
          res += i;
        }

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
      var querySave, queryRemove, bindVars = {
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
        url: '/_api/cursor',
        data: JSON.stringify(queryObj1),
        contentType: "application/json",
        success: function() {
          // if successful remove unwanted docs
          $.ajax({
            cache: false,
            type: 'POST',
            url: '/_api/cursor',
            data: JSON.stringify(queryObj2),
            contentType: "application/json",
            success: function() {
              if (callback) {
                callback();
              }
              window.progressView.hide();
            },
            error: function() {
              window.progressView.hide();
              arangoHelper.arangoError(
                "Document error", "Documents inserted, but could not be removed."
              );
            }
          });
        },
        error: function() {
          window.progressView.hide();
          arangoHelper.arangoError("Document error", "Could not move selected documents.");
        }
      });
    },

    getDocuments: function (callback) {
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
        else if (this.getSort() !== '') {
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
      /*
      if (this.getTotal() < 10000 || this.filters.length > 0) {
        queryObj.options = {
          fullCount: true,
        };
      }*/

      var checkCursorStatus = function(jobid) {
        $.ajax({
          cache: false,
          type: 'PUT',
          url: '/_api/job/' + encodeURIComponent(jobid),
          contentType: 'application/json',
          success: function(data, textStatus, xhr) {
            if (xhr.status === 201) {
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

              callback(false, data);
            }
            else if (xhr.status === 204) {
              self.checkCursorTimer = window.setTimeout(function() {
                checkCursorStatus(jobid);
              }, 500);
            }

          },
          error: function(data) {
            callback(false, data);
          }
        });
      };

      $.ajax({
        cache: false,
        type: 'POST',
        url: '/_api/cursor',
        data: JSON.stringify(queryObj),
        headers: {
          'x-arango-async': 'store'
        },
        contentType: "application/json",
        success: function (data, textStatus, xhr) {

          if (xhr.getResponseHeader('x-arango-async-id')) {
            var jobid = xhr.getResponseHeader('x-arango-async-id');

            var cancelRunningCursor = function() {
              $.ajax({
                url: '/_api/job/'+ encodeURIComponent(jobid) + "/cancel",
                type: 'PUT',
                success: function() {
                  window.clearTimeout(self.checkCursorTimer);
                  arangoHelper.arangoNotification("Documents", "Canceled operation.");
                  $('.dataTables_empty').text('Canceled.');
                  window.progressView.hide();
                }
              });
            };

            window.progressView.showWithDelay(300, "Fetching documents...", cancelRunningCursor);

            checkCursorStatus(jobid);
          }
          else {
            callback(true, data);
          }
        },
        error: function(data) {
          callback(false, data);
        }
      });
    },

    clearDocuments: function () {
      this.reset();
    },

    buildDownloadDocumentQuery: function() {
      var query, queryObj, bindVars;

      bindVars = {
        "@collection": this.collectionID
      };

      query = "FOR x in @@collection";
      query += this.setFiltersForQuery(bindVars);
      // Sort result, only useful for a small number of docs
      if (this.getTotal() < this.MAX_SORT && this.getSort().length > 0) {
        query += " SORT x." + this.getSort();
      }

      query += " RETURN x";

      queryObj = {
        query: query,
        bindVars: bindVars
      };

      return queryObj;
    },

    uploadDocuments : function (file, callback) {
      $.ajax({
        type: "POST",
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
            callback(false);
          } else {
            try {
              var data = JSON.parse(xhr.responseText);
              if (data.errors > 0) {
                var result = "At least one error occurred during upload";
                callback(false, result);
              }
            }
            catch (err) {
              console.log(err);
            }               
          }
        }
      });
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
/*global Backbone, activeUser, window, ArangoQuery, $, data, _, arangoHelper*/
(function() {
  "use strict";

  window.ArangoQueries = Backbone.Collection.extend({

    initialize: function(models, options) {
      var self = this;

      $.ajax("whoAmI?_=" + Date.now(), {async: true}).done(
        function(data) {
          if (this.activeUser === false) {
            self.activeUser = "root";
          }
          else {
            self.activeUser = data.user;
          }
        }
      );
    },

    url: '/_api/user/',

    model: ArangoQuery,

    activeUser: null,

    parse: function(response) {
      var self = this, toReturn;
      if (this.activeUser === false) {
        this.activeUser = "root";
      }

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

    saveCollectionQueries: function(callback) {
      if (this.activeUser === 0) {
        return false;
      }
      if (this.activeUser === false) {
        this.activeUser = "root";
      }

      var queries = [];

      this.each(function(query) {
        queries.push({
          value: query.attributes.value,
          parameter: query.attributes.parameter,
          name: query.attributes.name
        });
      });

      // save current collection
      $.ajax({
        cache: false,
        type: "PATCH",
        url: "/_api/user/" + encodeURIComponent(this.activeUser),
        data: JSON.stringify({
          extra: {
           queries: queries
          }
        }),
        contentType: "application/json",
        processData: false,
        success: function(data) {
          callback(false, data);
        },
        error: function() {
          callback(true);
        }
      });
    },

    saveImportQueries: function(file, callback) {

      if (this.activeUser === 0) {
        return false;
      }

      window.progressView.show("Fetching documents...");
      $.ajax({
        cache: false,
        type: "POST",
        url: "query/upload/" + encodeURIComponent(this.activeUser),
        data: file,
        contentType: "application/json",
        processData: false,
        success: function() {
          window.progressView.hide();
          arangoHelper.arangoNotification("Queries successfully imported.");
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

  getLogState: function (callback) {
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/replication/logger-state",
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },
  getApplyState: function (callback) {
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/replication/applier-state",
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  }

});

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, window */
window.StatisticsCollection = Backbone.Collection.extend({
  model: window.Statistics,
  url: "/_admin/statistics"
});

/*jshint browser: true */
/*jshint strict: false, unused: false */
/*global Backbone, window */
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

  activeUser: null,
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

  login: function (username, password, callback) {
    var self = this;

    $.ajax("login", {
      method: "POST",
      data: JSON.stringify({
        username: username,
        password: password
      }),
      dataType: "json"
    }).success(
      function (data) {
        self.activeUser = data.user;
        callback(false, self.activeUser);
      }
    ).error(
      function () {
        self.activeUser = null;
        callback(true, null);
      }
    );
  },

  setSortingDesc: function(yesno) {
    this.sortOptions.desc = yesno;
  },

  logout: function () {
    $.ajax("logout", {method:"POST"});
    this.activeUser = null;
    this.reset();
    window.App.navigate("");
    window.location.reload();
  },

  setUserSettings: function (identifier, content) {
    this.activeUserSettings.identifier = content;
  },

  loadUserSettings: function (callback) {
    var self = this;
    $.ajax({
      type: "GET",
      cache: false,
      url: "/_api/user/" + encodeURIComponent(self.activeUser),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        self.activeUserSettings = data.extra;
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
      }
    });
  },

  saveUserSettings: function (callback) {
    var self = this;
    $.ajax({
      cache: false,
      type: "PUT",
      url: "/_api/user/" + encodeURIComponent(self.activeUser),
      data: JSON.stringify({ extra: self.activeUserSettings }),
      contentType: "application/json",
      processData: false,
      success: function(data) {
        callback(false, data);
      },
      error: function(data) {
        callback(true, data);
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

  whoAmI: function(callback) {
    if (this.activeUser) {
      callback(false, this.activeUser);
      return;
    }
    $.ajax("whoAmI?_=" + Date.now())
    .success(
      function(data) {
        callback(false, data.user);
      }
    ).error(
      function(data) {
        callback(true, null);
      }
    );
  }


});

/*global window */
(function() {
  "use strict";
  window.ClusterCoordinators = window.AutomaticRetryCollection.extend({
    model: window.ClusterCoordinator,

    url: "/_admin/aardvark/cluster/Coordinators",

    updateUrl: function() {
      this.url = window.App.getNewRoute("Coordinators");
    },

    initialize: function() {
      //window.App.registerForUpdate(this);
    },

    statusClass: function(s) {
      switch (s) {
        case "ok":
          return "success";
        case "warning":
          return "warning";
        case "critical":
          return "danger";
        case "missing":
          return "inactive";
        default:
          return "danger";
      }
    },

    getStatuses: function(cb, nextStep) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb, nextStep))
      }).done(function() {
        self.successFullTry();
        self.forEach(function(m) {
          cb(self.statusClass(m.get("status")), m.get("address"));
        });
        nextStep();
      });
    },

    byAddress: function (res, callback) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.byAddress.bind(self, res, callback))
      }).done(function() {
        self.successFullTry();
        res = res || {};
        self.forEach(function(m) {
          var addr = m.get("address");
          addr = addr.split(":")[0];
          res[addr] = res[addr] || {};
          res[addr].coords = res[addr].coords || [];
          res[addr].coords.push(m);
        });
        callback(res);
      });
    },

    checkConnection: function(callback) {
      var self = this;
      if(!this.checkRetries()) {
        return;
      }
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.checkConnection.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        callback();
      });
    }

  });
}());



/*global window */
(function() {

  "use strict";

  window.ClusterServers = window.AutomaticRetryCollection.extend({

    model: window.ClusterServer,
    host: '',

    url: "/_admin/aardvark/cluster/DBServers",

    updateUrl: function() {
      //this.url = window.App.getNewRoute("DBServers");
      this.url = window.App.getNewRoute(this.host) + this.url;
    },

    initialize: function(models, options) {
      this.host = options.host;
      //window.App.registerForUpdate(this);
    },

    statusClass: function(s) {
      switch (s) {
        case "ok":
          return "success";
        case "warning":
          return "warning";
        case "critical":
          return "danger";
        case "missing":
          return "inactive";
        default:
          return "danger";
      }
    },

    getStatuses: function(cb) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this,
        completed = function() {
          self.successFullTry();
          self._retryCount = 0;
          self.forEach(function(m) {
            cb(self.statusClass(m.get("status")), m.get("address"));
          });
        };
      // This is the first function called in
      // Each update loop
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getStatuses.bind(self, cb))
      }).done(completed);
    },

    byAddress: function (res, callback) {
      if(!this.checkRetries()) {
        return;
      }
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.byAddress.bind(self, res, callback))
      }).done(function() {
        self.successFullTry();
        res = res || {};
        self.forEach(function(m) {
          var addr = m.get("address");
          addr = addr.split(":")[0];
          res[addr] = res[addr] || {};
          res[addr].dbs = res[addr].dbs || [];
          res[addr].dbs.push(m);
        });
        callback(res);
      }).error(function(e) {
        console.log("error");
        console.log(e);
      });
    },

    getList: function() {
      throw "Do not use";
      /*
      var self = this;
      this.fetch({
        beforeSend: window.App.addAuth.bind(window.App),
        error: self.failureTry.bind(self, self.getList.bind(self, callback))
      }).done(function() {
        self.successFullTry();
        var res = [];
        _.each(self.where({role: "primary"}), function(m) {
          var e = {};
          e.primary = m.forList();
          if (m.get("secondary")) {
            e.secondary = self.get(m.get("secondary")).forList();
          }
          res.push(e);
        });
        callback(res);
      });
      */
    },

    getOverview: function() {
      throw "Do not use DbServer.getOverview";
      /*
      this.fetch({
        async: false,
        beforeSend: window.App.addAuth.bind(window.App)
      });
      var res = {
        plan: 0,
        having: 0,
        status: "ok"
      },
      self = this,
      updateStatus = function(to) {
        if (res.status === "critical") {
          return;
        }
        res.status = to;
      };
      _.each(this.where({role: "primary"}), function(m) {
        res.plan++;
        switch (m.get("status")) {
          case "ok":
            res.having++;
            break;
          case "warning":
            res.having++;
            updateStatus("warning");
            break;
          case "critical":
            var bkp = self.get(m.get("secondary"));
            if (!bkp || bkp.get("status") === "critical") {
              updateStatus("critical");
            } else {
              if (bkp.get("status") === "ok") {
                res.having++;
                updateStatus("warning");
              }
            }
            break;
          default:
            console.debug("Undefined server state occurred. This is still in development");
        }
      });
      return res;
      */
    }
  });

}());


/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, $ */
(function() {
  "use strict";
  window.CoordinatorCollection = Backbone.Collection.extend({
    model: window.Coordinator,

    url: "/_admin/aardvark/cluster/Coordinators"

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global window, Backbone, $ */
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
/*global window, Backbone, $, arangoHelper */
(function () {
  "use strict";

  window.GraphCollection = Backbone.Collection.extend({
    model: window.Graph,

    sortOptions: {
      desc: false
    },

    url: "/_api/gharial",

    dropAndDeleteGraph: function(name, callback) {
      $.ajax({
        type: "DELETE",
        url: "/_api/gharial/" + encodeURIComponent(name) + "?dropCollections=true",
        contentType: "application/json",
        processData: true,
        success: function() {
          callback(true);
        },
        error: function() {
          callback(false);
        }
      });
    },

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
/*global window, Backbone, $ */
(function() {
  "use strict";
  window.WorkMonitorCollection = Backbone.Collection.extend({

    model: window.workMonitorModel,

    url: "/_admin/work-monitor",

    parse: function(response) {
      return response.work;
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, window, arangoHelper, templateEngine */

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
                      var split = window.location.hash.split("/");
                      if (split[2] === 'documents') {
                        options.page = i;
                        window.location.hash = split[0] + "/" + split[1] + "/" + split[2] + "/" + i;
                      }
                      else {
                        self.jumpTo(i);
                        options.page = i;
                      }
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
/*global Backbone, $, window, arangoHelper, templateEngine, Joi, _*/
(function() {
  'use strict';

  window.ApplicationDetailView = Backbone.View.extend({
    el: '#content',

    divs: ['#readme', '#swagger', '#app-info', '#sideinformation', '#information', '#settings'],
    navs: ['#service-info', '#service-api', '#service-readme', '#service-settings'],

    template: templateEngine.createTemplate('applicationDetailView.ejs'),

    events: {
      'click .open': 'openApp',
      'click .delete': 'deleteApp',
      'click #app-deps': 'showDepsDialog',
      'click #app-switch-mode': 'toggleDevelopment',
      'click #app-scripts [data-script]': 'runScript',
      'click #app-tests': 'runTests',
      'click #app-replace': 'replaceApp',
      'click #download-app': 'downloadApp',
      'click .subMenuEntries li': 'changeSubview',
      'mouseenter #app-scripts': 'showDropdown',
      'mouseleave #app-scripts': 'hideDropdown'
    },

    changeSubview: function(e) {
      _.each(this.navs, function(nav) {
        $(nav).removeClass('active');
      });

      $(e.currentTarget).addClass('active');

      _.each(this.divs, function(div) {
        $('.headerButtonBar').hide();
        $(div).hide();
      });

      if (e.currentTarget.id === 'service-readme') {
        $('#readme').show();
      }
      else if (e.currentTarget.id === 'service-api') {
        $('#swagger').show();
      }
      else if (e.currentTarget.id === 'service-info') {
        $('#information').show();
        $('#sideinformation').show();
      }
      else if (e.currentTarget.id === 'service-settings') {
        this.showConfigDialog();
        $('.headerButtonBar').show();
        $('#settings').show();
      }
    },

    downloadApp: function() {
      if (!this.model.isSystem()) {
        this.model.download();
      }
    },

    replaceApp: function() {
      var mount = this.model.get('mount');
      window.foxxInstallView.upgrade(mount, function() {
        window.App.applicationDetail(encodeURIComponent(mount));
      });
      $('.createModalDialog .arangoHeader').html("Replace Service");
      $('#infoTab').click();
    },

    updateConfig: function() {
      this.model.getConfiguration(function () {
        $('#app-warning')[this.model.needsAttention() ? 'show' : 'hide']();
        $('#app-warning-config')[this.model.needsConfiguration() ? 'show' : 'hide']();

        if (this.model.needsConfiguration()) {
          $('#app-config').addClass('error');
        }
        else {
          $('#app-config').removeClass('error');
        }
      }.bind(this));
    },

    updateDeps: function() {
      this.model.getDependencies(function () {
        $('#app-warning')[this.model.needsAttention() ? 'show' : 'hide']();
        $('#app-warning-deps')[this.model.hasUnconfiguredDependencies() ? 'show' : 'hide']();
        if (this.model.hasUnconfiguredDependencies()) {
          $('#app-deps').addClass('error');
        }
        else {
          $('#app-deps').removeClass('error');
        }
      }.bind(this));
    },

    toggleDevelopment: function() {
      this.model.toggleDevelopment(!this.model.isDevelopment(), function() {
        if (this.model.isDevelopment()) {
          $('.app-switch-mode').text('Set Production');
          $('#app-development-indicator').css('display', 'inline');
          $('#app-development-path').css('display', 'inline');
        } else {
          $('.app-switch-mode').text('Set Development');
          $('#app-development-indicator').css('display', 'none');
          $('#app-development-path').css('display', 'none');
        }
      }.bind(this));
    },

    runScript: function(event) {
      event.preventDefault();
      var script = $(event.currentTarget).attr('data-script');
      var tableContent = [
        window.modalView.createBlobEntry(
          'app_script_arguments',
          'Script arguments',
          '', null, 'optional', false,
          [{
            rule: function (v) {
              return v && JSON.parse(v);
            },
            msg: 'Must be well-formed JSON or empty'
          }]
        )
      ];
      var buttons = [
        window.modalView.createSuccessButton('Run script', function() {
          var opts = $('#app_script_arguments').val();
          opts = opts && JSON.parse(opts);
          window.modalView.hide();
          this.model.runScript(script, opts, function (err, result) {
            var info;
            if (err) {
              info = (
                '<p>The script failed with an error'
                + (err.statusCode ? (' (HTTP ' + err.statusCode + ')') : '')
                + ':</p>'
                + '<pre>' + err.message + '</pre>'
              );
            } else if (result) {
              info = (
                '<p>Script results:</p>'
                + '<pre>' + JSON.stringify(result, null, 2) + '</pre>'
              );
            } else {
              info = '<p>The script ran successfully.</p>';
            }
            window.modalView.show(
              'modalTable.ejs',
              'Result of script "' + script + '"',
              undefined,
              undefined,
              undefined,
              info
            );
          });
        }.bind(this))
      ];
      window.modalView.show(
        'modalTable.ejs',
        'Run script "' + script + '" on "' + this.model.get('mount') + '"',
        buttons,
        tableContent
      );
    },

    showSwagger: function(event) {
      event.preventDefault();
      this.render('swagger');
    },

    showReadme: function(event) {
      event.preventDefault();
      this.render('readme');
    },

    runTests: function(event) {
      event.preventDefault();
      var warning = (
        '<p><strong>WARNING:</strong> Running tests may result in destructive side-effects including data loss.'
        + ' Please make sure not to run tests on a production database.</p>'
      );
      if (this.model.isDevelopment()) {
        warning += (
          '<p><strong>WARNING:</strong> This app is running in <strong>development mode</strong>.'
          + ' If any of the tests access the app\'s HTTP API they may become non-deterministic.</p>'
        );
      }
      var buttons = [
        window.modalView.createSuccessButton('Run tests', function () {
          window.modalView.hide();
          this.model.runTests({reporter: 'suite'}, function (err, result) {
            window.modalView.show(
              'modalTestResults.ejs',
              'Test results',
              undefined,
              undefined,
              undefined,
              err || result
            );
          });
        }.bind(this))
      ];
      window.modalView.show(
        'modalTable.ejs',
        'Run tests for app "' + this.model.get('mount') + '"',
        buttons,
        undefined,
        undefined,
        warning
      );
    },

    render: function(mode) {

      var callback = function(error, db) {
        var self = this;
        if (error) {
          arangoHelper.arangoError("DB","Could not get current database");
        }
        else {
          $(this.el).html(this.template.render({
            app: this.model,
            db: db,
            mode: mode
          }));

          $.get(this.appUrl(db)).success(function () {
            $(".open", this.el).prop('disabled', false);
          }.bind(this));

          this.updateConfig();
          this.updateDeps();

          if (mode === 'swagger') {
            $.get( "./foxxes/docs/swagger.json?mount=" + encodeURIComponent(this.model.get('mount')), function(data) {
              if (Object.keys(data.paths).length < 1) {
                self.render('readme');
                $('#app-show-swagger').attr('disabled', 'true');
              }
            });
          }
        }

        this.breadcrumb();
      }.bind(this);

      arangoHelper.currentDatabase(callback);

      if (_.isEmpty(this.model.get('config'))) {
        $('#service-settings').attr('disabled', true);
      }
      return $(this.el);
    },

    breadcrumb: function() {
      console.log(this.model.toJSON());
      var string = 'Service: ' + this.model.get('name') + '<i class="fa fa-ellipsis-v" aria-hidden="true"></i>';
      
      var contributors = '<p class="mount"><span>Contributors:</span>';
      if (this.model.get('contributors') && this.model.get('contributors').length > 0) {
        _.each(this.model.get('contributors'), function (contributor) {
          contributors += '<a href="mailto:' + contributor.email + '">' + contributor.name + '</a>';
        });
      } 
      else {
        contributors += 'No contributors';
      }
      contributors += '</p>';
      $('.information').append(
        contributors
      );

      //information box info tab
      if (this.model.get("author")) {
        $('.information').append(
          '<p class="mount"><span>Author:</span>' + this.model.get("author") + '</p>'
        );
      }
      if (this.model.get("mount")) {
        $('.information').append(
          '<p class="mount"><span>Mount:</span>' + this.model.get("mount") + '</p>'
        );
      }
      if (this.model.get("development")) {
        if (this.model.get("path")) {
          $('.information').append(
            '<p class="path"><span>Path:</span>' + this.model.get("path") + '</p>'
          );
        }
      }
      $('#subNavigationBar .breadcrumb').html(string);
      
    },

    openApp: function() {

      var callback = function(error, db) {
        if (error) {
          arangoHelper.arangoError("DB","Could not get current database");
        }
        else {
          window.open(this.appUrl(db), this.model.get('title')).focus();
        }
      }.bind(this);

      arangoHelper.currentDatabase(callback);
    },

    deleteApp: function() {
      var buttons = [
        window.modalView.createDeleteButton('Delete', function() {
          var opts = {teardown: $('#app_delete_run_teardown').is(':checked')};
          this.model.destroy(opts, function (err, result) {
            if (!err && result.error === false) {
              window.modalView.hide();
              window.App.navigate('services', {trigger: true});
            }
          });
        }.bind(this))
      ];
      var tableContent = [
        window.modalView.createCheckboxEntry(
          'app_delete_run_teardown',
          'Run teardown?',
          true,
          'Should this app\'s teardown script be executed before removing the app?',
          true
        )
      ];
      window.modalView.show(
        'modalTable.ejs',
        'Delete Foxx App mounted at "' + this.model.get('mount') + '"',
        buttons,
        tableContent,
        undefined,
        '<p>Are you sure? There is no way back...</p>',
        true
      );
    },

    appUrl: function (currentDB) {
      return window.location.origin + '/_db/'
      + encodeURIComponent(currentDB)
      + this.model.get('mount');
    },

    applyConfig: function() {
      var cfg = {};
      _.each(this.model.get('config'), function(opt, key) {
        var $el = $('#app_config_' + key);
        var val = $el.val();
        if (opt.type === 'boolean' || opt.type === 'bool') {
          cfg[key] = $el.is(':checked');
          return;
        }
        if (val === '' && opt.hasOwnProperty('default')) {
          cfg[key] = opt.default;
          if (opt.type === 'json') {
            cfg[key] = JSON.stringify(opt.default);
          }
          return;
        }
        if (opt.type === 'number') {
          cfg[key] = parseFloat(val);
        } else if (opt.type === 'integer' || opt.type === 'int') {
          cfg[key] = parseInt(val, 10);
        } else if (opt.type === 'json') {
          cfg[key] = val && JSON.stringify(JSON.parse(val));
        } else {
          cfg[key] = window.arangoHelper.escapeHtml(val);
          return;
        }
      });
      this.model.setConfiguration(cfg, function() {
        this.updateConfig();
        arangoHelper.arangoNotification(this.model.get("name"), "Settings applied.");
      }.bind(this));
    },

    showConfigDialog: function() {
      if (_.isEmpty(this.model.get('config'))) {
        $('#settings .buttons').html($('#hidden_buttons').html()); 
        return;
      }
      var tableContent = _.map(this.model.get('config'), function(obj, name) {
        var defaultValue = obj.default === undefined ? '' : String(obj.default);
        var currentValue = obj.current === undefined ? '' : String(obj.current);
        var methodName = 'createTextEntry';
        var mandatory = false;
        var checks = [];
        if (obj.type === 'boolean' || obj.type === 'bool') {
          methodName = 'createCheckboxEntry';
          obj.default = obj.default || false;
          defaultValue = obj.default || false;
          currentValue = obj.current || false;
        } else if (obj.type === 'json') {
          methodName = 'createBlobEntry';
          defaultValue = obj.default === undefined ? '' : JSON.stringify(obj.default);
          currentValue = obj.current === undefined ? '' : obj.current;
          checks.push({
            rule: function (v) {
              return v && JSON.parse(v);
            },
            msg: 'Must be well-formed JSON or empty.'
          });
        } else if (obj.type === 'integer' || obj.type === 'int') {
          checks.push({
            rule: Joi.number().integer().optional().allow(''),
            msg: 'Has to be an integer.'
          });
        } else if (obj.type === 'number') {
          checks.push({
            rule: Joi.number().optional().allow(''),
            msg: 'Has to be a number.'
          });
        } else {
          if (obj.type === 'password') {
            methodName = 'createPasswordEntry';
          }
          checks.push({
            rule: Joi.string().optional().allow(''),
            msg: 'Has to be a string.'
          });
        }
        if (obj.default === undefined && obj.required !== false) {
          mandatory = true;
          checks.unshift({
            rule: Joi.any().required(),
            msg: 'This field is required.'
          });
        }
        return window.modalView[methodName](
          'app_config_' + name,
          name,
          currentValue,
          obj.description,
          defaultValue,
          mandatory,
          checks
        );
      });

      var buttons = [
        window.modalView.createSuccessButton('Apply', this.applyConfig.bind(this))
      ];

      window.modalView.show(
        'modalTable.ejs', 'Configuration', buttons, tableContent, null, null, null, null, null, 'settings'
      );
      $('.modal-footer').prepend($('#hidden_buttons').html()); 
    },

    applyDeps: function() {
      var deps = {};
      _.each(this.model.get('deps'), function(title, name) {
        var $el = $('#app_deps_' + name);
        deps[name] = window.arangoHelper.escapeHtml($el.val());
      });
      this.model.setDependencies(deps, function() {
        window.modalView.hide();
        this.updateDeps();
      }.bind(this));
    },

    showDepsDialog: function() {
      if (_.isEmpty(this.model.get('deps'))) {
        return;
      }
      var tableContent = _.map(this.model.get('deps'), function(obj, name) {
        var currentValue = obj.current === undefined ? '' : String(obj.current);
        var defaultValue = '';
        var description = obj.definition.name;
        if (obj.definition.version !== '*') {
          description += '@' + obj.definition.version;
        }
        var checks = [{
          rule: Joi.string().optional().allow(''),
          msg: 'Has to be a string.'
        }];
        if (obj.definition.required) {
          checks.push({
            rule: Joi.string().required(),
            msg: 'This value is required.'
          });
        }
        return window.modalView.createTextEntry(
          'app_deps_' + name,
          obj.title,
          currentValue,
          description,
          defaultValue,
          obj.definition.required,
          checks
        );
      });
      var buttons = [
        window.modalView.createSuccessButton('Apply', this.applyDeps.bind(this))
      ];
      window.modalView.show(
        'modalTable.ejs', 'Dependencies', buttons, tableContent
      );

    },

    showDropdown: function () {
      if (!_.isEmpty(this.model.get('scripts'))) {
        $('#scripts_dropdown').show(200);
      }
    },

    hideDropdown: function () {
      $('#scripts_dropdown').hide();
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
      "click #checkSystem"           : "toggleSystem"
    },

    fixCheckboxes: function() {
      if (this._showDevel) {
        $('#checkDevel').attr('checked', 'checked');
      }
      else {
        $('#checkDevel').removeAttr('checked');
      }
      if (this._showSystem) {
        $('#checkSystem').attr('checked', 'checked');
      }
      else {
        $('#checkSystem').removeAttr('checked');
      }
      if (this._showProd) {
        $('#checkProduction').attr('checked', 'checked');
      }
      else {
        $('#checkProduction').removeAttr('checked');
      }
      $('#checkDevel').next().removeClass('fa fa-check-square-o fa-square-o').addClass('fa');
      $('#checkSystem').next().removeClass('fa fa-check-square-o fa-square-o').addClass('fa');
      $('#checkProduction').next().removeClass('fa fa-check-square-o fa-square-o').addClass('fa');
      arangoHelper.setCheckboxStatus('#foxxDropdown');
    },

    toggleDevel: function() {
      var self = this;
      this._showDevel = !this._showDevel;
      _.each(this._installedSubViews, function(v) {
        v.toggle("devel", self._showDevel);
      });
      this.fixCheckboxes();
    },

    toggleProduction: function() {
      var self = this;
      this._showProd = !this._showProd;
      _.each(this._installedSubViews, function(v) {
        v.toggle("production", self._showProd);
      });
      this.fixCheckboxes();
    },

    toggleSystem: function() {
      this._showSystem = !this._showSystem;
      var self = this;
      _.each(this._installedSubViews, function(v) {
        v.toggle("system", self._showSystem);
      });
      this.fixCheckboxes();
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
      arangoHelper.setCheckboxStatus("#foxxDropdown");
      
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
/*global arangoHelper, prettyBytes, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  "use strict";

  window.ClusterView = Backbone.View.extend({

    el: '#content',
    template: templateEngine.createTemplate("clusterView.ejs"),

    events: {
    },

    statsEnabled: false,
    historyInit: false,
    initDone: false,
    interval: 5000,
    maxValues: 100,
    knownServers: [],
    chartData: {},
    charts: {},
    nvcharts: [],
    startHistory: {},
    startHistoryAccumulated: {},

    initialize: function (options) {
      var self = this;

      if (window.App.isCluster) {
        this.dbServers = options.dbServers;
        this.coordinators = options.coordinators;
        this.updateServerTime();

        //start polling with interval
        window.setInterval(function() {
          if (window.location.hash === '#cluster'
              || window.location.hash === '#') {

            var callback = function(data) {
              self.rerenderValues(data);
              self.rerenderGraphs(data);
            };

            // now fetch the statistics history
            self.getCoordStatHistory(callback);
          }
        }, this.interval);
      }
    },

    render: function () {
      this.$el.html(this.template.render({}));
      //this.initValues();

      if (!this.initDone) {
        if (this.coordinators.first() !== undefined) {
          this.getServerStatistics();
        }
        else {
          this.waitForCoordinators();
        }
        this.initDone = true;
      }
      this.initGraphs();
    },

    waitForCoordinators: function() {
      var self = this; 

      window.setTimeout(function() {
        if (self.coordinators) {
          self.getServerStatistics();
        }
        else {
          self.waitForCoordinators();
        }
      }, 500);
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    },

    getServerStatistics: function() {
      var self = this;

      this.data = undefined;

      var coord = this.coordinators.first();

      this.statCollectCoord = new window.ClusterStatisticsCollection([],
        {host: coord.get('address')}
      );
      this.statCollectDBS = new window.ClusterStatisticsCollection([],
        {host: coord.get('address')}
      );

      // create statistics collector for DB servers
      var dbsmodels = [];
      _.each(this.dbServers, function(dbs) {
        dbs.each(function(model) {
          dbsmodels.push(model);
        });
      });

      _.each(dbsmodels, function (dbserver) {
        if (dbserver.get("status") !== "ok") {
          return;
        }

        if (self.knownServers.indexOf(dbserver.id) === -1) {
          self.knownServers.push(dbserver.id);
        }

        var stat = new window.Statistics({name: dbserver.id});
        stat.url = coord.get("protocol") + "://"
        + coord.get("address")
        + "/_admin/clusterStatistics?DBserver="
        + dbserver.get("name");
        self.statCollectDBS.add(stat);
      });

      // create statistics collector for coordinator
      this.coordinators.forEach(function (coordinator) {
        if (coordinator.get("status") !== "ok") {return;}

        if (self.knownServers.indexOf(coordinator.id) === -1) {
          self.knownServers.push(coordinator.id);
        }

        var stat = new window.Statistics({name: coordinator.id});

        stat.url = coordinator.get("protocol") + "://"
        + coordinator.get("address")
        + "/_admin/statistics";

        self.statCollectCoord.add(stat);
      });

      // first load history callback
      var callback = function(data) {
        self.rerenderValues(data);
        self.rerenderGraphs(data);
      }.bind(this);

      // now fetch the statistics history
      self.getCoordStatHistory(callback);

      //special case nodes
      self.coordinators.fetch({
        success: function() {
          self.renderNode(true);
        },
        error: function() {
          self.renderNode(false);
        }
      });
    },
    
    rerenderValues: function(data) {
      var self = this;

      //TODO cache value state like graph data

      //NODE
      this.coordinators.fetch({
        success: function() {
          self.renderNode(true);
        },
        error: function() {
          self.renderNode(false);
        }
      });

      //Connections
      this.renderValue('#clusterConnections', Math.round(data.clientConnectionsCurrent));
      this.renderValue('#clusterConnectionsAvg', Math.round(data.clientConnections15M));

      //RAM
      var totalMem = data.physicalMemory;
      var usedMem = data.residentSizeCurrent;
      this.renderValue('#clusterRam', [usedMem, totalMem]);
    },

    renderValue: function(id, value, error) {
      if (typeof value === 'number') {
        $(id).html(value);
      }
      else if ($.isArray(value)) {
        var a = value[0], b = value[1];

        var percent = 1 / (b/a) * 100;
        $(id).html(percent.toFixed(1) + ' %');
      }
      else if (typeof value === 'string') {
        $(id).html(value);
      }

      if (error) {
        $(id).addClass('negative');
        $(id).removeClass('positive');
      }
      else {
        $(id).addClass('positive');
        $(id).removeClass('negative');
      }

    },

    renderNode: function(connection) {
      var ok = 0, error = 0;

      if (connection) {
        this.coordinators.each(function(value) {
          if (value.toJSON().status === 'ok') {
            ok++;
          }
          else {
            error++;
          }
        });
        
        if (error > 0) {
          var total = error + ok;
          this.renderValue('#clusterNodes', ok + '/' + total, true);
        }
        else {
          this.renderValue('#clusterNodes', ok);
        }
      }
      else {
        this.renderValue('#clusterNodes', 'OFFLINE', true);
      }

    },

    initValues: function() {

      var values = [
        "#clusterNodes",
        "#clusterRam",
        "#clusterConnections",
        "#clusterConnectionsAvg",
      ];

      _.each(values, function(id) {
        $(id).html('<i class="fa fa-spin fa-circle-o-notch" style="color: rgba(0, 0, 0, 0.64);"></i>'); 
      });
    },

    graphData: {
      data: {
        sent: [],
        received: []
      },
      http: [],
      average: []
    },

    checkArraySizes: function() {
      var self = this;

      _.each(self.chartsOptions, function(val1, key1) {
        _.each(val1.options, function(val2, key2) {
          if (val2.values.length > self.maxValues - 1) {
            self.chartsOptions[key1].options[key2].values.shift();
          }
        });
      });
    },

    formatDataForGraph: function(data) {
      var self = this;

      if (!self.historyInit) {
        _.each(data.times, function(time, key) {

          //DATA
          self.chartsOptions[0].options[0].values.push({x:time, y: data.bytesSentPerSecond[key]});
          self.chartsOptions[0].options[1].values.push({x:time, y: data.bytesReceivedPerSecond[key]});

          //HTTP
          self.chartsOptions[1].options[0].values.push({x:time, y: self.calcTotalHttp(data.http, key)});

          //AVERAGE
          self.chartsOptions[2].options[0].values.push({x:time, y: data.avgRequestTime[key]});
        });
        self.historyInit = true;
      }
      else {
        self.checkArraySizes();

        //DATA
        self.chartsOptions[0].options[0].values.push({
          x: data.times[data.times.length - 1],
          y: data.bytesSentPerSecond[data.bytesSentPerSecond.length - 1]
        });
        self.chartsOptions[0].options[1].values.push({
          x: data.times[data.times.length - 1],
          y: data.bytesReceivedPerSecond[data.bytesReceivedPerSecond.length - 1]
        });
        //HTTP
        self.chartsOptions[1].options[0].values.push({
          x: data.times[data.times.length - 1],
          y: self.calcTotalHttp(data.http, data.bytesSentPerSecond.length - 1)
        });
        //AVERAGE
        self.chartsOptions[2].options[0].values.push({
          x: data.times[data.times.length - 1],
          y: data.avgRequestTime[data.bytesSentPerSecond.length - 1]
        });


      }
    },

    chartsOptions: [
      {
        id: "#clusterData",
        count: 2,
        options: [
          {
            area: true,
            values: [],
            key: "Bytes out",
            color: 'rgb(23,190,207)',
            strokeWidth: 2,
            fillOpacity: 0.1
          },
          {
            area: true,
            values: [],
            key: "Bytes in",
            color: "rgb(188, 189, 34)",
            strokeWidth: 2,
            fillOpacity: 0.1
          }
        ]
      },
      {
        id: "#clusterHttp",
        options: [{
          area: true,
          values: [],
          key: "Bytes",
          color: "rgb(0, 166, 90)",
          fillOpacity: 0.1
        }]
      },
      {
        id: "#clusterAverage",
        data: [],
        options: [{
          area: true,
          values: [],
          key: "Bytes",
          color: "rgb(243, 156, 18)",
          fillOpacity: 0.1
        }]
      }
    ],

    initGraphs: function() {
      var self = this;

      var noData = 'Fetching data...';
      if (self.statsEnabled === false) {
        noData = 'Statistics disabled.';
      }

      _.each(self.chartsOptions, function(c) {
        nv.addGraph(function() {
          self.charts[c.id] = nv.models.stackedAreaChart()
          .options({
            useInteractiveGuideline: true,
            showControls: false,
            noData: noData,
            duration: 0
          });

          self.charts[c.id].xAxis
          .axisLabel("")
          .tickFormat(function(d) {
            var x = new Date(d * 1000);
            return (x.getHours() < 10 ? '0' : '') + x.getHours() + ":" +  
              (x.getMinutes() < 10 ? '0' : '') + x.getMinutes() + ":" + 
              (x.getSeconds() < 10 ? '0' : '') + x.getSeconds();
          })
          .staggerLabels(false);

          self.charts[c.id].yAxis
          .axisLabel('')
          .tickFormat(function(d) {
            if (d === null) {
              return 'N/A';
            }
            var formatted = parseFloat(d3.format(".2f")(d));
            return prettyBytes(formatted);
          });

          var data, lines = self.returnGraphOptions(c.id);
          if (lines.length > 0) {
            _.each(lines, function (val, key) {
              c.options[key].values = val;
            });
          }
          else {
            c.options[0].values = [];
          }
          data = c.options;

          self.chartData[c.id] = d3.select(c.id).append('svg')
          .datum(data)
          .transition().duration(300)
          .call(self.charts[c.id])
          .each('start', function() {
            window.setTimeout(function() {
              d3.selectAll(c.id + ' *').each(function() {
                if (this.__transition__) {
                  this.__transition__.duration = 0;
                }
              });
            }, 0);
          });

          nv.utils.windowResize(self.charts[c.id].update);
          self.nvcharts.push(self.charts[c.id]);
          
          return self.charts[c.id];
        });
      });
    },

    returnGraphOptions: function(id) {
      var arr = []; 
      if (id === '#clusterData') {
        //arr =  [this.graphData.data.sent, this.graphData.data.received];
        arr = [
          this.chartsOptions[0].options[0].values,
          this.chartsOptions[0].options[1].values
        ];
      }
      else if (id === '#clusterHttp') {
        arr = [this.chartsOptions[1].options[0].values];
      }
      else if (id === '#clusterAverage') {
        arr = [this.chartsOptions[2].options[0].values];
      }
      else {
        arr = [];
      }

      return arr;
    },

    rerenderGraphs: function(input) {

      if (!this.statsEnabled) {
        return;
      }

      var self = this, data, lines;
      this.formatDataForGraph(input);

      _.each(self.chartsOptions, function(c) {
        lines = self.returnGraphOptions(c.id);

        if (lines.length > 0) {
          _.each(lines, function (val, key) {
            c.options[key].values = val;
          });
        }
        else {
          c.options[0].values = [];
        }
        data = c.options;

        //update nvd3 chart
        if (data[0].values.length > 0) {
          if (self.historyInit) {
            if (self.charts[c.id]) {
              self.charts[c.id].update();
            }
          }
        }
        
      });
    },

    calcTotalHttp: function(object, pos) {
      var sum = 0;
      _.each(object, function(totalHttp) {
        sum += totalHttp[pos];
      });
      return sum;
    },

    getCoordStatHistory: function(callback) {
      var self = this, promises = [], historyUrl;

      var merged = {
        http: {}
      };

      var getHistory = function(url) {
        return $.get(url, {count: self.statCollectCoord.size()}, null, 'json');
      };

      var mergeHistory = function(data) {

        var onetime = ['times'];
        var values = [
          'physicalMemory',
          'residentSizeCurrent',
          'clientConnections15M',
          'clientConnectionsCurrent'
        ];
        var http = [
          'optionsPerSecond',
          'putsPerSecond',
          'headsPerSecond',
          'postsPerSecond',
          'getsPerSecond',
          'deletesPerSecond',
          'othersPerSecond',
          'patchesPerSecond'
        ];
        var arrays = [
          'bytesSentPerSecond',
          'bytesReceivedPerSecond',
          'avgRequestTime'
        ];

        var counter = 0, counter2;

        _.each(data, function(stat) {
          if (stat.enabled) {
            self.statsEnabled = true;
          }
          else {
            self.statsEnabled = false;
          }

          if (typeof stat === 'object') {
            if (counter === 0) {
              //one time value
              _.each(onetime, function(value) {
                merged[value] = stat[value];
              });

              //values
              _.each(values, function(value) {
                merged[value] = stat[value];
              });

              //http requests arrays
              _.each(http, function(value) {
                merged.http[value] = stat[value];
              });

              //arrays
              _.each(arrays, function(value) {
                merged[value] = stat[value];
              });

            }
            else {
              //values
              _.each(values, function(value) {
                merged[value] = merged[value] + stat[value];
              });
              //http requests arrays
              _.each(http, function(value) {
                  counter2 = 0;
                  _.each(stat[value], function(x) {
                    merged.http[value][counter] = merged.http[value][counter] + x;
                    counter2++;
                  });
              });
              _.each(arrays, function(value) {
                counter2 = 0;
                _.each(stat[value], function(x) {
                  merged[value][counter] = merged[value][counter] + x;
                  counter2++;
                });
              });
            }
          counter++;
          }
        });
      };

      this.statCollectCoord.each(function(coord) {
        historyUrl = coord.url + '/short';
        promises.push(getHistory(historyUrl));
      });

      $.when.apply($, promises).done(function() {
        //wait until all data is here
        var arr = [];
        _.each(promises, function(stat) {
          arr.push(stat.responseJSON);
        });
        mergeHistory(arr);
        callback(merged);
      });
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global window, exports, Backbone, EJS, _, $, templateEngine, arangoHelper, Joi*/

(function() {
  "use strict";

  window.CollectionListItemView = Backbone.View.extend({

    tagName: "div",
    className: "tile pure-u-1-1 pure-u-sm-1-2 pure-u-md-1-3 pure-u-lg-1-4 pure-u-xl-1-6",
    template: templateEngine.createTemplate("collectionsItemView.ejs"),

    initialize: function (options) {
      this.collectionsView = options.collectionsView;
    },

    events: {
      'click .iconSet.icon_arangodb_settings2': 'createEditPropertiesModal',
      'click .pull-left' : 'noop',
      'click .icon_arangodb_settings2' : 'editProperties',
      'click .spanInfo' : 'showProperties',
      'click': 'selectCollection'
    },

    render: function () {
      if (this.model.get("locked")) {
        $(this.el).addClass('locked');
        $(this.el).addClass(this.model.get("lockType"));
      } 
      else {
        $(this.el).removeClass('locked');
      }
      if (this.model.get("status") === 'loading' || this.model.get("status") === 'unloading') {
        $(this.el).addClass('locked');
      }
      $(this.el).html(this.template.render({
        model: this.model
      }));
      $(this.el).attr('id', 'collection_' + this.model.get('name'));

      return this;
    },

    editProperties: function (event) {
      if (this.model.get("locked")) {
        return 0;
      }
      event.stopPropagation();
      this.createEditPropertiesModal();
    },

    showProperties: function(event) {
      if (this.model.get("locked")) {
        return 0;
      }
      event.stopPropagation();
      this.createInfoModal();
    },

    selectCollection: function(event) {

      //check if event was fired from disabled button
      if ($(event.target).hasClass("disabled")) {
        return 0;
      }
      if (this.model.get("locked")) {
        return 0;
      }
      if (this.model.get("status") === 'loading' ) {
        return 0;
      }

      if (this.model.get("status") === 'unloaded' ) {
        this.loadCollection();
      }
      else {
        window.App.navigate(
          "collection/" + encodeURIComponent(this.model.get("name")) + "/documents/1", {trigger: true}
        );
      }

    },

    noop: function(event) {
      event.stopPropagation();
    },

    unloadCollection: function () {

      var unloadCollectionCallback = function(error) {
        if (error) {
          arangoHelper.arangoError('Collection error', this.model.get("name") + ' could not be unloaded.');
        }
        else if (error === undefined) {
          this.model.set("status", "unloading");
          this.render();
        }
        else {
          if (window.location.hash === "#collections") {
            this.model.set("status", "unloaded");
            this.render();
          }
          else {
            arangoHelper.arangoNotification("Collection " + this.model.get("name") + " unloaded.");
          }
        }
      }.bind(this);

      this.model.unloadCollection(unloadCollectionCallback);
      window.modalView.hide();
    },

    loadCollection: function () {

      var loadCollectionCallback = function(error) {
        if (error) {
          arangoHelper.arangoError('Collection error', this.model.get("name") + ' could not be loaded.');
        }
        else if (error === undefined) {
          this.model.set("status", "loading");
          this.render();
        }
        else {
          if (window.location.hash === "#collections") {
            this.model.set("status", "loaded");
            this.render();
          }
          else {
            arangoHelper.arangoNotification("Collection " + this.model.get("name") + " loaded.");
          }
        }
      }.bind(this);

      this.model.loadCollection(loadCollectionCallback);
      window.modalView.hide();
    },

    truncateCollection: function () {
      this.model.truncateCollection();
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

      var callback = function(error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError("Error", "Could not get coordinator info");
        }
        else {
          var newname;
          if (isCoordinator) {
            newname = this.model.get('name');
          }
          else {
            newname = $('#change-collection-name').val();
          }
          var status = this.model.get('status');

          if (status === 'loaded') {
            var journalSize;
            try {
              journalSize = JSON.parse($('#change-collection-size').val() * 1024 * 1024);
            }
            catch (e) {
              arangoHelper.arangoError('Please enter a valid number');
              return 0;
            }

            var indexBuckets;
            try {
              indexBuckets = JSON.parse($('#change-index-buckets').val());
              if (indexBuckets < 1 || parseInt(indexBuckets) !== Math.pow(2, Math.log2(indexBuckets))) {
                throw "invalid indexBuckets value";
              }
            }
            catch (e) {
              arangoHelper.arangoError('Please enter a valid number of index buckets');
              return 0;
            }
            var callbackChange = function(error) {
              if (error) {
                arangoHelper.arangoError("Collection error: " + error.responseText);
              }
              else {
                this.collectionsView.render();
                window.modalView.hide();
              }
            }.bind(this);

            var callbackRename = function(error) {
              if (error) {
                arangoHelper.arangoError("Collection error: " + error.responseText);
              }
              else {
                var wfs = $('#change-collection-sync').val();
                this.model.changeCollection(wfs, journalSize, indexBuckets, callbackChange);
              }
            }.bind(this);

            this.model.renameCollection(newname, callbackRename);
          }
          else if (status === 'unloaded') {
            if (this.model.get('name') !== newname) {

              var callbackRename2 = function(error, data) {
                if (error) {
                  arangoHelper.arangoError("Collection error: " + data.responseText);
                }
                else {
                  this.collectionsView.render();
                  window.modalView.hide();
                }
              }.bind(this);

              this.model.renameCollection(newname, callbackRename2);
            }
            else {
              window.modalView.hide();
            }
          }
        }
      }.bind(this);

      window.isCoordinator(callback);
    },

    createEditPropertiesModal: function() {

      var callback = function(error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError("Error","Could not get coordinator info");
        }
        else {
          var collectionIsLoaded = false;

          if (this.model.get('status') === "loaded") {
            collectionIsLoaded = true;
          }

          var buttons = [],
            tableContent = [];

          if (!isCoordinator) {
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

          var after = function() {

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

            var tabBar = ["General", "Indices"],
              templates =  ["modalTable.ejs", "indicesView.ejs"];

            window.modalView.show(
              templates,
              "Modify Collection",
              buttons,
              tableContent, null, null,
              this.events, null,
              tabBar
            );
            if (this.model.get("status") === 'loaded') {
              this.getIndex();
            }
            else {
              $($('#infoTab').children()[1]).remove();
            }
          }.bind(this);

          if (collectionIsLoaded) {

            var callback2 = function(error, data) {
              if (error) {
                arangoHelper.arangoError("Collection", "Could not fetch properties");
              }
              else {
                var journalSize = data.journalSize/(1024*1024);
                var indexBuckets = data.indexBuckets;
                var wfs = data.waitForSync;

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

                tableContent.push(
                  window.modalView.createTextEntry(
                    "change-index-buckets",
                    "Index buckets",
                    indexBuckets,
                    "The number of index buckets for this collection. Must be at least 1 and a power of 2.",
                    "",
                    true,
                    [
                      {
                        rule: Joi.string().allow('').optional().regex(/^[1-9][0-9]*$/),
                        msg: "Must be a number greater than 1 and a power of 2."
                      }
                    ]
                  )
                );

                // prevent "unexpected sync method error"
                tableContent.push(
                  window.modalView.createSelectEntry(
                    "change-collection-sync",
                    "Wait for sync",
                    wfs,
                    "Synchronize to disk before returning from a create or update of a document.",
                    [{value: false, label: "No"}, {value: true, label: "Yes"}]        )
                );
              }
              after();

            }.bind(this);

            this.model.getProperties(callback2);
          }
          else {
            after(); 
          }

        }
      }.bind(this);
      window.isCoordinator(callback);
    },

    bindIndexEvents: function() {
      this.unbindIndexEvents();
      var self = this;

      $('#indexEditView #addIndex').bind('click', function() {
        self.toggleNewIndexView();

        $('#cancelIndex').unbind('click');
        $('#cancelIndex').bind('click', function() {
          self.toggleNewIndexView();
        });

        $('#createIndex').unbind('click');
        $('#createIndex').bind('click', function() {
          self.createIndex();
        });

      });

      $('#newIndexType').bind('change', function() {
        self.selectIndexType();
      });

      $('.deleteIndex').bind('click', function(e) {
        self.prepDeleteIndex(e);
      });

      $('#infoTab a').bind('click', function(e) {
        $('#indexDeleteModal').remove();
        if ($(e.currentTarget).html() === 'Indices'  && !$(e.currentTarget).parent().hasClass('active')) {

          $('#newIndexView').hide();
          $('#indexEditView').show();

          $('#modal-dialog .modal-footer .button-danger').hide();  
          $('#modal-dialog .modal-footer .button-success').hide();  
          $('#modal-dialog .modal-footer .button-notification').hide();
          //$('#addIndex').detach().appendTo('#modal-dialog .modal-footer');
        }
        if ($(e.currentTarget).html() === 'General' && !$(e.currentTarget).parent().hasClass('active')) {
          $('#modal-dialog .modal-footer .button-danger').show();  
          $('#modal-dialog .modal-footer .button-success').show();  
          $('#modal-dialog .modal-footer .button-notification').show();
          var elem = $('.index-button-bar')[0]; 
          var elem2 = $('.index-button-bar2')[0]; 
          //$('#addIndex').detach().appendTo(elem);
          if ($('#cancelIndex').is(':visible')) {
            $('#cancelIndex').detach().appendTo(elem2);
            $('#createIndex').detach().appendTo(elem2);
          }
        }
      });

    },

    unbindIndexEvents: function() {
      $('#indexEditView #addIndex').unbind('click');
      $('#newIndexType').unbind('change');
      $('#infoTab a').unbind('click');
      $('.deleteIndex').unbind('click');
      /*
      //$('#documentsToolbar ul').unbind('click');
      this.markFilterToggle();
      this.changeEditMode(false);
      "click #documentsToolbar ul"    : "resetIndexForms"
      */
    },

    createInfoModal: function() {

      var callbackRev = function(error, revision, figures) {
        if (error) {
          arangoHelper.arangoError("Figures", "Could not get revision.");        
        }
        else {
          var buttons = [];
          var tableContent = {
            figures: figures,
            revision: revision,
            model: this.model
          };
          window.modalView.show(
            "modalCollectionInfo.ejs",
            "Collection: " + this.model.get('name'),
            buttons,
            tableContent
          );
        }
      }.bind(this);

      var callback = function(error, data) {
        if (error) {
          arangoHelper.arangoError("Figures", "Could not get figures.");        
        }
        else {
          var figures = data;
          this.model.getRevision(callbackRev, figures);
        }
      }.bind(this);

      this.model.getFigures(callback);
    },
    //index functions
    resetIndexForms: function () {
      $('#indexHeader input').val('').prop("checked", false);
      $('#newIndexType').val('Geo').prop('selected',true);
      this.selectIndexType();
    },

    createIndex: function () {
      //e.preventDefault();
      var self = this;
      var indexType = $('#newIndexType').val();
      var postParameter = {};
      var fields;
      var unique;
      var sparse;

      switch (indexType) {
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
      var callback = function(error, msg){
        if (error) {
          if (msg) {
            var message = JSON.parse(msg.responseText);
            arangoHelper.arangoError("Document error", message.errorMessage);
          }
          else {
            arangoHelper.arangoError("Document error", "Could not create index.");
          }
        }
        self.refreshCollectionsView();
      };

      window.modalView.hide();
      //$($('#infoTab').children()[1]).find('a').click();
      self.model.createIndex(postParameter, callback);
    },

    lastTarget: null,

    prepDeleteIndex: function (e) {
      var self = this;
      this.lastTarget = e;

      this.lastId = $(this.lastTarget.currentTarget).
        parent().
        parent().
        first().
        children().
        first().
        text();
      //window.modalView.hide();

      //delete modal
      $("#modal-dialog .modal-footer").after(
        '<div id="indexDeleteModal" style="display:block;" class="alert alert-error modal-delete-confirmation">' +
        '<strong>Really delete?</strong>' +
        '<button id="indexConfirmDelete" class="button-danger pull-right modal-confirm-delete">Yes</button>' +
        '<button id="indexAbortDelete" class="button-neutral pull-right">No</button>' +
        '</div>'
      );
      $('#indexConfirmDelete').unbind('click');
      $('#indexConfirmDelete').bind('click', function() {
        $('#indexDeleteModal').remove();
        self.deleteIndex();
      });

      $('#indexAbortDelete').unbind('click');
      $('#indexAbortDelete').bind('click', function() {
        $('#indexDeleteModal').remove();
      });
    },

    refreshCollectionsView: function() {
      window.App.arangoCollectionsStore.fetch({
        success: function () {
          window.App.collectionsView.render();
        }
      });
    },

    deleteIndex: function () {
      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("Could not delete index");
          $("tr th:contains('"+ this.lastId+"')").parent().children().last().html(
            '<span class="deleteIndex icon_arangodb_roundminus"' + 
              ' data-original-title="Delete index" title="Delete index"></span>'
          );
          this.model.set("locked", false);
          this.refreshCollectionsView();
        }
        else if (!error && error !== undefined) {
          $("tr th:contains('"+ this.lastId+"')").parent().remove();
          this.model.set("locked", false);
          this.refreshCollectionsView();
        }
        this.refreshCollectionsView();
      }.bind(this);

      this.model.set("locked", true);
      this.model.deleteIndex(this.lastId, callback);

      $("tr th:contains('"+ this.lastId+"')").parent().children().last().html(
        '<i class="fa fa-circle-o-notch fa-spin"></i>'
      );
    },

    selectIndexType: function () {
      $('.newIndexClass').hide();
      var type = $('#newIndexType').val();
      $('#newIndexType'+type).show();
    },

    getIndex: function () {

      var callback = function(error, data) {
        if (error) {
          window.arangoHelper.arangoError('Index', data.errorMessage);
        }
        else {
          this.renderIndex(data);
        }
      }.bind(this);

      this.model.getIndex(callback);
    },

    renderIndex: function(data) {

      this.index = data;

      var cssClass = 'collectionInfoTh modal-text';
      if (this.index) {
        var fieldString = '';
        var actionString = '';

        _.each(this.index.indexes, function(v) {
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
      }
      this.bindIndexEvents();
    },

    toggleNewIndexView: function () {
      var elem = $('.index-button-bar2')[0];

      if ($('#indexEditView').is(':visible')) {
        $('#indexEditView').hide();
        $('#newIndexView').show();
        $('#cancelIndex').detach().appendTo('#modal-dialog .modal-footer');
        $('#createIndex').detach().appendTo('#modal-dialog .modal-footer');
      }
      else {
        $('#indexEditView').show();
        $('#newIndexView').hide();
        $('#cancelIndex').detach().appendTo(elem);
        $('#createIndex').detach().appendTo(elem);
      }

      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "right");
      this.resetIndexForms();
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

    checkboxToValue: function (id) {
      return $(id).prop('checked');
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
    refreshRate: 10000,

    template: templateEngine.createTemplate("collectionsView.ejs"),


    refetchCollections: function() {
      var self = this;
      this.collection.fetch({
        success: function() {
          self.checkLockedCollections();
        }
      });
    },

    checkLockedCollections: function() {
        var callback = function(error, lockedCollections) {
          var self = this;
          if (error) {
            console.log("Could not check locked collections");
          }
          else {
            this.collection.each(function(model) {
              model.set('locked', false);
            });

            _.each(lockedCollections, function(locked) {
              var model = self.collection.findWhere({
                id: locked.collection 
              });
              model.set('locked', true);
              model.set('lockType', locked.type);
              model.set('desc', locked.desc);
            });

            this.collection.each(function(model) {
              if (!model.get("locked")) {
                $('#collection_' + model.get("name")).find('.corneredBadge').removeClass('loaded unloaded');
                $('#collection_' + model.get("name") + ' .corneredBadge').text(model.get("status"));
                $('#collection_' + model.get("name") + ' .corneredBadge').addClass(model.get("status"));
              }

              if (model.get("locked") || model.get("status") === 'loading') {
                $('#collection_' + model.get("name")).addClass('locked');
                if (model.get("locked")) {
                  $('#collection_' + model.get("name")).find('.corneredBadge').removeClass('loaded unloaded');
                  $('#collection_' + model.get("name")).find('.corneredBadge').addClass('inProgress');
                  $('#collection_' + model.get("name") + ' .corneredBadge').text(model.get("desc"));
                }
                else {
                  $('#collection_' + model.get("name") + ' .corneredBadge').text(model.get("status"));
                }
              }
              else {
                $('#collection_' + model.get("name")).removeClass('locked');
                $('#collection_' + model.get("name") + ' .corneredBadge').text(model.get("status"));
                if ($('#collection_' + model.get("name") + ' .corneredBadge').hasClass('inProgress')) {
                  $('#collection_' + model.get("name") + ' .corneredBadge').text(model.get("status"));
                  $('#collection_' + model.get("name") + ' .corneredBadge').removeClass('inProgress');
                  $('#collection_' + model.get("name") + ' .corneredBadge').addClass('loaded');
                }
                if (model.get('status') === 'unloaded') {
                  $('#collection_' + model.get("name") + ' .icon_arangodb_info').addClass('disabled');
                }
              }
            });
          }
        }.bind(this);

        window.arangoHelper.syncAndReturnUninishedAardvarkJobs('index', callback);
    },

    initialize: function() {
      var self = this;

      window.setInterval(function() {
        if (window.location.hash === '#collections' && window.VISIBLE) {
          self.refetchCollections();
        }
      }, self.refreshRate);

    },

    render: function () {

      this.checkLockedCollections();
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
      arangoHelper.setCheckboxStatus("#collectionsDropdown");

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

    restrictToSearchPhraseKey: function () {
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

      var callbackCoord = function(error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError("DB","Could not check coordinator state");
        }
        else {
          var collName = $('#new-collection-name').val(),
          collSize = $('#new-collection-size').val(),
          collType = $('#new-collection-type').val(),
          collSync = $('#new-collection-sync').val(),
          shards = 1,
          shardBy = [];

          if (isCoordinator) {
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
          //no new system collections via webinterface
          //var isSystem = (collName.substr(0, 1) === '_');
          var callback = function(error, data) {
            if (error) {
              try {
                data = JSON.parse(data.responseText);
                arangoHelper.arangoError("Error", data.errorMessage);
              }
              catch (e) {
                console.log(e);
              }
            }
            else {
              this.updateCollectionsView();
            }
            window.modalView.hide();

          }.bind(this);

          this.collection.newCollection({
            collName: collName,
            wfs: wfs,
            isSystem: isSystem,
            collSize: collSize,
            collType: collType,
            shards: shards,
            shardBy: shardBy
          }, callback);
        }
      }.bind(this);

      window.isCoordinator(callbackCoord);
    },

    createNewCollectionModal: function() {

      var callbackCoord2 = function(error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError("DB","Could not check coordinator state");
        }
        else {
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

          if (isCoordinator) {
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
              "Wait for sync",
              "",
              "Synchronize to disk before returning from a create or update of a document.",
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
      }.bind(this);

      window.isCoordinator(callbackCoord2);
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, flush, window, arangoHelper, nv, d3, localStorage*/
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
      "click .subViewNavbar .subMenuEntry" : "toggleViews"
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

    initialize: function (options) {
      this.options = options;
      this.dygraphConfig = options.dygraphConfig;
      this.d3NotInitialized = true;
      this.events["click .dashboard-sub-bar-menu-sign"] = this.showDetail.bind(this);
      this.events["mousedown .dygraph-rangesel-zoomhandle"] = this.stopUpdating.bind(this);
      this.events["mouseup .dygraph-rangesel-zoomhandle"] = this.startUpdating.bind(this);

      this.serverInfo = options.serverToShow;

      if (! this.serverInfo) {
        this.server = "-local-";
      } else {
        this.server = this.serverInfo.target;
      }

      this.history[this.server] = {};
    },

    toggleViews: function(e) {
      var id = e.currentTarget.id.split('-')[0], self = this;
      var views = ['replication', 'requests', 'system'];

      _.each(views, function(view) {
        if (id !== view) {
          $('#' + view).hide();
        }
        else {
          $('#' + view).show();
          self.resize();
          $(window).resize();
        }
      });

      $('.subMenuEntries').children().removeClass('active');
      $('#' + id + '-statistics').addClass('active');

      window.setTimeout(function() {
        self.resize();
        $(window).resize();
      }, 200);

    },
		
		cleanupHistory: function(f) {
			// clean up too big history data
			if (this.history[this.server].hasOwnProperty(f)) {
	      if (this.history[this.server][f].length > this.defaultTimeFrame / this.interval) {
			    while (this.history[this.server][f].length > this.defaultTimeFrame / this.interval) {
	          this.history[this.server][f].shift();
	        }
			  }
			}
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
        var p = "";
        var v = 0;
        if (self.history.hasOwnProperty(self.server) &&
            self.history[self.server].hasOwnProperty(a)) {
          v = self.history[self.server][a][1];
        }

        if (v < 0) {
          tempColor = "#d05448";
        }
        else {
          tempColor = "#7da817";
          p = "+";
        }
        if (self.history.hasOwnProperty(self.server) &&
            self.history[self.server].hasOwnProperty(a)) {
          $("#" + a).html(self.history[self.server][a][0] + '<br/><span class="dashboard-figurePer" style="color: '
            + tempColor +';">' + p + v + '%</span>');
        }
        else {
          $("#" + a).html('<br/><span class="dashboard-figurePer" style="color: '
            + "#000" +';">' + '<p class="dataNotReadyYet">data not ready yet</p>' + '</span>');
        }
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

      //round line chart values to 10th decimals
      var pointer = 0, dates = [];
      _.each(opts.file, function(value) {

        var rounded = value[0].getSeconds() - (value[0].getSeconds() % 10); 
        opts.file[pointer][0].setSeconds(rounded);
        dates.push(opts.file[pointer][0]);

        pointer++;
      });
      //get min/max dates of array
      var maxDate = new Date(Math.max.apply(null, dates));
      var minDate = new Date(Math.min.apply(null, dates));
      var tmpDate = new Date(minDate.getTime()), missingDates = [];
      var tmpDatesComplete = [];

      while (tmpDate < maxDate) {
        tmpDate = new Date(tmpDate.setSeconds(tmpDate.getSeconds() + 10));
        tmpDatesComplete.push(tmpDate);
      }

      //iterate through all date ranges
      _.each(tmpDatesComplete, function(date) {
        var tmp = false;

        //iterate through all available real date values
        _.each(opts.file, function(availableDates) {
          //if real date is inside date range
          if (Math.floor(date.getTime()/1000) === Math.floor(availableDates[0].getTime()/1000)) {
            tmp = true;
          }
        });

        if (tmp === false) {
          //a value is missing
          if (date < new Date()) {
            missingDates.push(date);
          }
        }
      });

      _.each(missingDates, function(date) {
        if (figure === 'systemUserTime' ||
            figure === 'requests' ||
            figure === 'pageFaults' ||
            figure === 'dataTransfer') {
          opts.file.push([date, 0, 0]);
        }
        if (figure === 'totalTime') {
          opts.file.push([date, 0, 0, 0]);
        }
      });

      if (opts.file === undefined) {
        $('#loadingScreen span').text('Statistics not ready yet. Waiting.');
        $('#loadingScreen').show();
        $('#content').hide();
      }
      else {
        $('#content').show();
        $('#loadingScreen').hide();

        //sort for library
        opts.file.sort(function(a,b){
          return new Date(b[0]) - new Date(a[0]);
        });

        g.updateOptions(opts);
        
        //clean up history
        if (this.history[this.server].hasOwnProperty(figure)) {
          this.cleanupHistory(figure);
        }
      }
      $(window).trigger('resize');
      this.resize();
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

          // HTTP requests combine all types to one
          // 0: date, 1: GET", 2: "PUT", 3: "POST", 4: "DELETE", 5: "PATCH",
          // 6: "HEAD", 7: "OPTIONS", 8: "OTHER"
          //
          var read = 0, write = 0;
          if (valueList.length === 9) {

            read += valueList[1];
            read += valueList[6];
            read += valueList[7];
            read += valueList[8];

            write += valueList[2];
            write += valueList[3];
            write += valueList[4];
            write += valueList[5];

            valueList = [valueList[0], read, write];
          }

          self.history[self.server][f].push(valueList);
        }
      });
		},

    cutOffHistory: function (f, cutoff) {
      var self = this, v;

      while (self.history[self.server][f].length !== 0) {
        v = self.history[self.server][f][0][0];

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
          "color": this.dygraphConfig.colors[2],
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
        "color": this.dygraphConfig.colors[1],
        "values": []
      }, v2 = {
        "key": this.barChartsElementNames[attribList[1]],
        "color": this.dygraphConfig.colors[2],
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

    renderReplicationStatistics: function(object) {
      $('#repl-numbers table tr:nth-child(1) > td:nth-child(2)').html(object.state.totalEvents);
      $('#repl-numbers table tr:nth-child(2) > td:nth-child(2)').html(object.state.totalRequests);
      $('#repl-numbers table tr:nth-child(3) > td:nth-child(2)').html(object.state.totalFailedConnects);

      if (object.state.lastAppliedContinuousTick) {
        $('#repl-ticks table tr:nth-child(1) > td:nth-child(2)').html(object.state.lastAppliedContinuousTick);
      }
      else {
        $('#repl-ticks table tr:nth-child(1) > td:nth-child(2)').html("no data available").addClass('no-data');
      }
      if (object.state.lastProcessedContinuousTick) {
        $('#repl-ticks table tr:nth-child(2) > td:nth-child(2)').html(object.state.lastProcessedContinuousTick);
      }
      else {
        $('#repl-ticks table tr:nth-child(2) > td:nth-child(2)').html("no data available").addClass('no-data');
      }
      if (object.state.lastAvailableContinuousTick) {
        $('#repl-ticks table tr:nth-child(3) > td:nth-child(2)').html(object.state.lastAvailableContinuousTick);
      }
      else {
        $('#repl-ticks table tr:nth-child(3) > td:nth-child(2)').html("no data available").addClass('no-data');
      }

      $('#repl-progress table tr:nth-child(1) > td:nth-child(2)').html(object.state.progress.message);
      $('#repl-progress table tr:nth-child(2) > td:nth-child(2)').html(object.state.progress.time);
      $('#repl-progress table tr:nth-child(3) > td:nth-child(2)').html(object.state.progress.failedConnects);
    },

    getReplicationStatistics: function() {
      var self = this;

      $.ajax(
        '/_api/replication/applier-state',
        {async: true}
      ).done(
        function (d) {
          if (d.hasOwnProperty('state')) {
            var running;
            if (d.state.running) {
              running = "active";
            }
            else {
              running = "inactive";
            }
            running = '<span class="state">' + running + '</span>';
            $('#replication-chart .dashboard-sub-bar').html("Replication " + running);

            self.renderReplicationStatistics(d);
          }
      });
    },

    getStatistics: function (callback, modalView) {
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
        url = self.serverInfo.endpoint + "/_admin/aardvark/statistics/cluster";
        urlParams += "&type=short&DBserver=" + self.serverInfo.target;

        if (! self.history.hasOwnProperty(self.server)) {
          self.history[self.server] = {};
        }
      }

      $.ajax(
        url + urlParams,
        { 
          async: true,
          xhrFields: {
            withCredentials: true
          },
          crossDomain: true
        }
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
            callback(d.enabled, modalView);
          }
          self.updateCharts();
      }).error(function(e) {
        console.log("stat fetch req error");
        console.log(e);
      });

      this.getReplicationStatistics();
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

      var origin = window.location.href.split("/"), 
      preUrl = origin[0] + '//' + origin[2] + '/' + origin[3] + '/_system/' + origin[5] + '/' + origin[6] + '/';

      $.ajax(
        preUrl + url + urlParams,
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

    addEmptyDataLabels: function () {
      if ($('.dataNotReadyYet').length === 0) {
        $('#dataTransferDistribution').prepend('<p class="dataNotReadyYet"> data not ready yet </p>');
        $('#totalTimeDistribution').prepend('<p class="dataNotReadyYet"> data not ready yet </p>');
        $('.dashboard-bar-chart-title').append('<p class="dataNotReadyYet"> data not ready yet </p>');
      }
    },

    removeEmptyDataLabels: function () {
      $('.dataNotReadyYet').remove();
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


      if (self.history[self.server].residentSizeChart === undefined) {
        this.addEmptyDataLabels();
        return;
      }
      else {
        this.removeEmptyDataLabels();
      }

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
          //.transitionDuration(100)
          //.tooltip(false)
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

      if (this.d3NotInitialized) {
          update = false;
          this.d3NotInitialized = false;
      }

      _.each(Object.keys(barCharts), function (k) {

        var dimensions = self.getCurrentSize('#' + k
          + 'Container .dashboard-interior-chart');

        var selector = "#" + k + "Container svg";

        if (self.history[self.server].residentSizeChart === undefined) {
          self.addEmptyDataLabels();
          return;
        }
        else {
          self.removeEmptyDataLabels();
        }

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
            //.transitionDuration(100)
            //.tooltips(false)
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

        if (window.App.isCluster) {
          if (window.location.hash.indexOf(self.serverInfo.target) > -1) {
            self.getStatistics();
          }
        }
        else {
          self.getStatistics();
        }
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

    var callback = function(enabled, modalView) {
      if (!modalView)  {
        $(this.el).html(this.template.render());
      }

      if (!enabled) {
        $(this.el).html('');
        if (this.server) {
          $(this.el).append(
            '<div style="color: red">Server statistics (' + this.server + ') are disabled.</div>'
          );
        }
        else {
          $(this.el).append(
            '<div style="color: red">Server statistics are disabled.</div>'
          );
        }
        return;
      }

      this.prepareDygraphs();
      if (this.isUpdating) {
        this.prepareD3Charts();
        this.prepareResidentSize();
        this.updateTendencies();
        $(window).trigger('resize');
      }
      this.startUpdating();
      $(window).resize();
    }.bind(this);

    var errorFunction = function() {
        $(this.el).html('');
        $('.contentDiv').remove();
        $('.headerBar').remove();
        $('.dashboard-headerbar').remove();
        $('.dashboard-row').remove();
        $(this.el).append(
          '<div style="color: red">You do not have permission to view this page.</div>'
        );
        $(this.el).append(
          '<div style="color: red">You can switch to \'_system\' to see the dashboard.</div>'
        );
    }.bind(this);

    var callback2 = function(error, authorized) {
      if (!error) {
        if (!authorized) {
          errorFunction();
        }
        else {
          this.getStatistics(callback, modalView);
        }
      }
    }.bind(this);

    if (window.App.currentDB.get("name") !== '_system') {
      errorFunction();
      return;
    }

    //check if user has _system permission
    this.options.database.hasSystemAccess(callback2);
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
      "click #dbSortDesc"           : "sorting"
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
      this.collection.fetch({async: true});
    },
      
    checkBoxes: function (e) {
      //chrome bugfix
      var clicked = e.currentTarget.id;
      $('#'+clicked).click();
    },

    render: function() {

      var callback = function(error, db) {
        if (error) {
          arangoHelper.arangoError("DB","Could not get current db properties");
        }
        else {
          this.currentDB = db;
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
          
          arangoHelper.setCheckboxStatus("#databaseDropdown");

          this.replaceSVGs();
        }
      }.bind(this);

      this.collection.getCurrentDatabase(callback);

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

    validateDatabaseInfo: function (db, user) {
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
      if (!$(e.target).parent().hasClass('iconSet')) {
        var changeTo = $(e.currentTarget).find("h5").text();
        if (changeTo !== '') {
          var url = this.collection.createDatabaseURL(changeTo);
          window.location.replace(url);
        }
      }
    },

    submitCreateDatabase: function() {
      var self = this, userPassword,
      name  = $('#newDatabaseName').val(),
      userName = $('#newUser').val();

      if ($('#useDefaultPassword').val() === 'true') {
        userPassword = 'ARANGODB_DEFAULT_ROOT_PASSWORD'; 
      }
      else {
        userPassword = $('#newPassword').val();
      }

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
        success: function() {
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
        isDeletable = true;
      if(dbName === this.currentDB) {
        isDeletable = false;
      }
      this.createEditDatabaseModal(dbName, isDeletable);
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

    createEditDatabaseModal: function(dbName, isDeletable) {
      var buttons = [],
        tableContent = [];

      tableContent.push(
        window.modalView.createReadOnlyEntry("id_name", "Name", dbName, "")
      );
      if (isDeletable) {
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
        "Delete database",
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
        window.modalView.createSelectEntry(
          "useDefaultPassword",
          "Use default password",
          true,
          "Read the password from the environment variable ARANGODB_DEFAULT_ROOT_PASSWORD.",
          [{value: false, label: "No"}, {value: true, label: "Yes"}]        )
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

      $('#useDefaultPassword').change(function() {

        if ($('#useDefaultPassword').val() === 'true') {
          $('#row_newPassword').hide();
        }
        else {
          $('#row_newPassword').show();
        }
      });

      $('#row_newPassword').hide();
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global templateEngine, window, Backbone, $, arangoHelper */
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

      var callback = function(error, list) {
        if (error) {
          arangoHelper.arangoError("DB","Could not fetch databases");
        }
        else {
          this.$el = el;
          this.$el.html(this.template.render({
            list: list,
            current: this.current.get("name")
          }));
          this.delegateEvents();
        }
      }.bind(this);

      this.collection.getDatabasesForUser(callback);

      return this.el;
    }
  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, window, arangoHelper, jsoneditor, templateEngine, JSONEditor */
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

    customView: false,
    defaultMode: 'tree',

    template: templateEngine.createTemplate("documentView.ejs"),

    events: {
      "click #saveDocumentButton" : "saveDocument",
      "click #deleteDocumentButton" : "deleteDocumentModal",
      "click #confirmDeleteDocument" : "deleteDocument",
      "click #document-from" : "navigateToDocument",
      "click #document-to" : "navigateToDocument",
      "keydown #documentEditor .ace_editor" : "keyPress",
      "keyup .jsoneditor .search input" : "checkSearchBox",
      "click .jsoneditor .modes" : "storeMode"
    },

    checkSearchBox: function(e) {
      if ($(e.currentTarget).val() === '') {
        this.editor.expandAll();
      }
    },

    storeMode: function() {
      var self = this;

      $('.type-modes').on('click', function(elem) {
        self.defaultMode = $(elem.currentTarget).text().toLowerCase();
      });
    },

    keyPress: function(e) {
      if (e.ctrlKey && e.keyCode === 13) {
        e.preventDefault();
        this.saveDocument();
      }
      else if (e.metaKey && e.keyCode === 13) {
        e.preventDefault();
        this.saveDocument();
      }
    },

    editor: 0,

    setType: function (type) {
      if (type === 2) {
        type = 'document';
      }
      else {
        type = 'edge';
      }

      var callback = function(error, data, type) {
        if (error) {
          console.log(data);
          arangoHelper.arangoError("Error", "Could not fetch data.");
        }
        else {
          var type2 = type + ': '; 
          this.type = type;
          this.fillInfo(type2);
          this.fillEditor();
        }
      }.bind(this);

      if (type === 'edge') {
        this.collection.getEdge(this.colid, this.docid, callback);
      }
      else if (type === 'document') {
        this.collection.getDocument(this.colid, this.docid, callback);
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
      window.modalView.show('modalTable.ejs', 'Delete Document', buttons, tableContent);
    },

    deleteDocument: function() {

      var successFunction = function() {
        if (this.customView) {
          this.customDeleteFunction();
        }
        else {
          var navigateTo = "collection/" + encodeURIComponent(this.colid) + '/documents/1';
          window.modalView.hide();
          window.App.navigate(navigateTo, {trigger: true});
        }
      }.bind(this);

      if (this.type === 'document') {
        var callbackDoc = function(error) {
          if (error) {
            arangoHelper.arangoError('Error', 'Could not delete document');
          }
          else {
            successFunction();
          }
        }.bind(this);
        this.collection.deleteDocument(this.colid, this.docid, callbackDoc);
      }
      else if (this.type === 'edge') {
        var callbackEdge = function(error) {
          if (error) {
            arangoHelper.arangoError('Edge error', 'Could not delete edge');
          }
          else {
            successFunction();
          }
        }.bind(this);
        this.collection.deleteEdge(this.colid, this.docid, callbackEdge);
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
      $('.disabledBread').last().text(this.collection.first().get('_key'));
      this.editor.set(toFill);
      $('.ace_content').attr('font-size','11pt');
    },

    jsonContentChanged: function() {
      this.enableSaveButton();
    },

    resize: function() {
      $('#documentEditor').height($('.centralRow').height() - 300);
    },

    render: function() {
      $(this.el).html(this.template.render({}));

      $('#documentEditor').height($('.centralRow').height() - 300);
      this.disableSaveButton();
      this.breadcrumb();

      var self = this;

      var container = document.getElementById('documentEditor');
      var options = {
        change: function(){self.jsonContentChanged();},
        search: true,
        mode: 'tree',
        modes: ['tree', 'code'],
        iconlib: "fontawesome4"
      };
      this.editor = new JSONEditor(container, options);
      this.editor.setMode(this.defaultMode);

      return this;
    },

    removeReadonlyKeys: function (object) {
      return _.omit(object, ["_key", "_id", "_from", "_to", "_rev"]);
    },

    saveDocument: function () {
      if ($('#saveDocumentButton').attr('disabled') === undefined) {
        if (this.collection.first().attributes._id.substr(0, 1) === '_') {

          var buttons = [], tableContent = [];
          tableContent.push(
            window.modalView.createReadOnlyEntry(
              'doc-save-system-button',
              'Caution',
              'You are modifying a system collection. Really continue?',
              undefined,
              undefined,
              false,
              /[<>&'"]/
          )
          );
          buttons.push(
            window.modalView.createSuccessButton('Save', this.confirmSaveDocument.bind(this))
          );
          window.modalView.show('modalTable.ejs', 'Modify System Collection', buttons, tableContent);
        }
        else {
          this.confirmSaveDocument();
        }
      }
    },

    confirmSaveDocument: function () {

      window.modalView.hide();

      var model;

      try {
        model = this.editor.get();
      }
      catch (e) {
        this.errorConfirmation(e);
        this.disableSaveButton();
        return;
      }

      model = JSON.stringify(model);

      if (this.type === 'document') {
        var callback = function(error) {
          if (error) {
            arangoHelper.arangoError('Error', 'Could not save document.');
          }
          else {
            this.successConfirmation();
            this.disableSaveButton();
          }
        }.bind(this);

        this.collection.saveDocument(this.colid, this.docid, model, callback);
      }
      else if (this.type === 'edge') {
        var callbackE = function(error) {
          if (error) {
            arangoHelper.arangoError('Error', 'Could not save edge.');
          }
          else {
            this.successConfirmation();
            this.disableSaveButton();
          }
        }.bind(this);

        this.collection.saveEdge(this.colid, this.docid, model, callbackE);
      }
    },

    successConfirmation: function () {

      arangoHelper.arangoNotification('Document saved.');

      $('#documentEditor .tree').animate({backgroundColor: '#C6FFB0'}, 500);
      $('#documentEditor .tree').animate({backgroundColor: '#FFFFF'}, 500);

      $('#documentEditor .ace_content').animate({backgroundColor: '#C6FFB0'}, 500);
      $('#documentEditor .ace_content').animate({backgroundColor: '#FFFFF'}, 500);
    },

    errorConfirmation: function (e) {
      arangoHelper.arangoError("Document editor: ", e);

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
      $('#subNavigationBar .breadcrumb').html(
        '<a href="#collection/' + name[1] + '/documents/1">Collection: ' + name[1].toLowerCase() + '</a>' + 
        '<i class="fa fa-chevron-right"></i>' +
        'Document: ' + name[2]
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
/*global arangoHelper, _, $, window, arangoHelper, templateEngine, Joi, btoa */
/*global numeral */

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

    initialize : function (options) {
      this.documentStore = options.documentStore;
      this.collectionsStore = options.collectionsStore;
      this.tableView = new window.TableView({
        el: this.table,
        collection: this.collection
      });
      this.tableView.setRowClick(this.clicked.bind(this));
      this.tableView.setRemoveClick(this.remove.bind(this));
    },

    resize: function() {
      $('#docPureTable').height($('.centralRow').height() - 210);
      $('#docPureTable .pure-table-body').css('max-height', $('#docPureTable').height() - 47);
    },

    setCollectionId : function (colid, page) {
      this.collection.setCollection(colid);
      this.collection.setPage(page);
      this.page = page;

      var callback = function(error, type) {
        if (error) {
          arangoHelper.arangoError("Error", "Could not get collection properties.");
        }
        else {
          this.type = type;
          this.collection.getDocuments(this.getDocsCallback.bind(this));
          this.collectionModel = this.collectionsStore.get(colid);
        }
      }.bind(this);

      arangoHelper.collectionApiType(colid, null, callback);
    },

    getDocsCallback: function(error) {
      //Hide first/last pagination
      $('#documents_last').css("visibility", "hidden");
      $('#documents_first').css("visibility", "hidden");

      if (error) {
        window.progressView.hide();
        arangoHelper.arangoError("Document error", "Could not fetch requested documents.");
      }
      else if (!error || error !== undefined){
        window.progressView.hide();
        this.drawTable();
        this.renderPaginationElements();
      }

    },

    events: {
      "click #collectionPrev"      : "prevCollection",
      "click #collectionNext"      : "nextCollection",
      "click #filterCollection"    : "filterCollection",
      "click #markDocuments"       : "editDocuments",
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
      "change #documentSize"       : "setPagesize",
      "change #docsSort"           : "setSorting"
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

    nop: function(event) {
      event.stopPropagation();
    },

    resetView: function () {

      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("Document", "Could not fetch documents count");
        }
      }.bind(this);

      //clear all input/select - fields
      $('input').val('');
      $('select').val('==');
      this.removeAllFilterItems();
      $('#documentSize').val(this.collection.getPageSize());

      $('#documents_last').css("visibility", "visible");
      $('#documents_first').css("visibility", "visible");
      this.addDocumentSwitch = true;
      this.collection.resetFilter();
      this.collection.loadTotal(callback);
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

      var callback = function(error, msg) {
        if (error) {
          arangoHelper.arangoError("Upload", msg);
          this.hideSpinner();
        }
        else {
          this.hideSpinner();
          this.hideImportModal();
          this.resetView();
        }
      }.bind(this);

      if (this.allowUpload === true) {
        this.showSpinner();
        this.collection.uploadDocuments(this.file, callback);
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
    /*
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
    },*/

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
      $('#importCollection').removeClass('activated');
      $('#exportCollection').removeClass('activated');
      this.markFilterToggle();
      $('#markDocuments').toggleClass('activated');
      this.changeEditMode();
      $('#filterHeader').hide();
      $('#importHeader').hide();
      $('#editHeader').slideToggle(200);
      $('#exportHeader').hide();
    },

    filterCollection : function () {
      $('#importCollection').removeClass('activated');
      $('#exportCollection').removeClass('activated');
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      this.markFilterToggle();
      this.activeFilter = true;
      $('#importHeader').hide();
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
      $('#importCollection').removeClass('activated');
      $('#filterHeader').removeClass('activated');
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      $('#exportCollection').toggleClass('activated');
      this.markFilterToggle();
      $('#exportHeader').slideToggle(200);
      $('#importHeader').hide();
      $('#filterHeader').hide();
      $('#editHeader').hide();
    },

    importCollection: function () {
      this.markFilterToggle();
      $('#markDocuments').removeClass('activated');
      this.changeEditMode(false);
      $('#importCollection').toggleClass('activated');
      $('#exportCollection').removeClass('activated');
      $('#importHeader').slideToggle(200);
      $('#filterHeader').hide();
      $('#editHeader').hide();
      $('#exportHeader').hide();
    },

    changeEditMode: function (enable) {
      if (enable === false || this.editMode === true) {
        $('#docPureTable .pure-table-body .pure-table-row').css('cursor', 'default');
        $('.deleteButton').fadeIn();
        $('.addButton').fadeIn();
        $('.selected-row').removeClass('selected-row');
        this.editMode = false;
        this.tableView.setRowClick(this.clicked.bind(this));
      }
      else {
        $('#docPureTable .pure-table-body .pure-table-row').css('cursor', 'copy');
        $('.deleteButton').fadeOut();
        $('.addButton').fadeOut();
        $('.selectedCount').text(0);
        this.editMode = true;
        this.tableView.setRowClick(this.editModeClick.bind(this));
      }
    },

    getFilterContent: function () {
      var filters = [ ];
      var i, value;

      for (i in this.filters) {
        if (this.filters.hasOwnProperty(i)) {
          value = $('#attribute_value' + i).val();

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
      buttons = [], tableContent = [];
      // second parameter is "true" to disable caching of collection type

      var callback = function(error, type) {
        if (error) {
          arangoHelper.arangoError("Error", "Could not fetch collection type");
        }
        else {
          if (type === 'edge') {

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
                  {
                    rule: Joi.string().allow('').optional(),
                    msg: ""
                  }
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
                  {
                    rule: Joi.string().allow('').optional(),
                    msg: ""
                  }
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
        }
      }.bind(this);
      arangoHelper.collectionApiType(collid, true, callback);
    },

    addEdge: function () {
      var collid  = window.location.hash.split("/")[1];
      var from = $('.modal-body #new-edge-from-attr').last().val();
      var to = $('.modal-body #new-edge-to').last().val();
      var key = $('.modal-body #new-edge-key-attr').last().val();
      var url;


      var callback = function(error, data) {
        if (error) {
          arangoHelper.arangoError('Error', 'Could not create edge');
        }
        else {
          window.modalView.hide();
          data = data._id.split('/');

          try {
            url = "collection/" + data[0] + '/' + data[1];
            decodeURI(url);
          } catch (ex) {
            url = "collection/" + data[0] + '/' + encodeURIComponent(data[1]);
          }
          window.location.hash = url;
        }
      }.bind(this);

      if (key !== '' || key !== undefined) {
        this.documentStore.createTypeEdge(collid, from, to, key, callback);
      }
      else {
        this.documentStore.createTypeEdge(collid, from, to, null, callback);
      }
    },

    addDocument: function() {
      var collid = window.location.hash.split("/")[1];
      var key = $('.modal-body #new-document-key-attr').last().val();
      var url;

      var callback = function(error, data) {
        if (error) {
          arangoHelper.arangoError('Error', 'Could not create document');
        }
        else {
          window.modalView.hide();
          data = data.split('/');

          try {
            url = "collection/" + data[0] + '/' + data[1];
            decodeURI(url);
          } catch (ex) {
            url = "collection/" + data[0] + '/' + encodeURIComponent(data[1]);
          }

          window.location.hash = url;
        }
      }.bind(this);

      if (key !== '' || key !== undefined) {
        this.documentStore.createTypeDocument(collid, key, callback);
      }
      else {
        this.documentStore.createTypeDocument(collid, null, callback);
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
        if (self.type === 'document') {
          var callback = function(error) {
            if (error) {
              deleted.push(false);
              arangoHelper.arangoError('Document error', 'Could not delete document.');
            }
            else {
              deleted.push(true);
              self.collection.setTotalMinusOne();
              self.collection.getDocuments(this.getDocsCallback.bind(this));
              $('#markDocuments').click();
              window.modalView.hide();
            }
          }.bind(self);
          self.documentStore.deleteDocument(self.collection.collectionID, key, callback);
        }
        else if (self.type === 'edge') {

          var callback2 = function(error) {
            if (error) {
              deleted.push(false);
              arangoHelper.arangoError('Edge error', 'Could not delete edge');
            }
            else {
              self.collection.setTotalMinusOne();
              deleted.push(true);
              self.collection.getDocuments(this.getDocsCallback.bind(this));
              $('#markDocuments').click();
              window.modalView.hide();
            }
          }.bind(self);

          self.documentStore.deleteEdge(self.collection.collectionID, key, callback2);
        }
      });

    },

    getSelectedDocs: function() {
      var toDelete = [];
      _.each($('#docPureTable .pure-table-body .pure-table-row'), function(element) {
        if ($(element).hasClass('selected-row')) {
          toDelete.push($($(element).children()[1]).find('.key').text());
        }
      });
      return toDelete;
    },

    remove: function (a) {
      this.docid = $(a.currentTarget).parent().parent().prev().find('.key').text();
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
      if (this.type === 'document') {

        var callback = function(error) {
          if (error) {
            arangoHelper.arangoError('Error', 'Could not delete document');
          }
          else {
            this.collection.setTotalMinusOne();
            this.collection.getDocuments(this.getDocsCallback.bind(this));
            $('#docDeleteModal').modal('hide');
          }
        }.bind(this);

        this.documentStore.deleteDocument(this.collection.collectionID, this.docid, callback);
      }
      else if (this.type === 'edge') {

        var callback2 = function(error) {
          if (error) {
            arangoHelper.arangoError('Edge error', "Could not delete edge");
          }
          else {
            this.collection.setTotalMinusOne();
            this.collection.getDocuments(this.getDocsCallback.bind(this));
            $('#docDeleteModal').modal('hide');
          }
        }.bind(this);

        this.documentStore.deleteEdge(this.collection.collectionID, this.docid, callback2);
      }
    },

    editModeClick: function(event) {
      var target = $(event.currentTarget);

      if(target.hasClass('selected-row')) {
        target.removeClass('selected-row');
      } else {
        target.addClass('selected-row');
      }

      console.log(target);
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

      var url, doc = $(self).attr("id").substr(4);

      try {
        url = "collection/" + this.collection.collectionID + '/' + doc;
        decodeURI(doc);
      } catch (ex) {
        url = "collection/" + this.collection.collectionID + '/' + encodeURIComponent(doc);
      }

      window.location.hash = url;
    },

    drawTable: function() {
      this.tableView.setElement($('#docPureTable')).render();
      // we added some icons, so we need to fix their tooltips
      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");

      $(".prettify").snippet("javascript", {
        style: "nedit",
        menu: false,
        startText: false,
        transparent: true,
        showNum: false
      });
      this.resize();
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
          this.collection.setSort('');
          this.restoredFilters = [];
          this.activeFilter = false;
        }
      }
    },

    render: function() {
      $(this.el).html(this.template.render({}));

      if (this.type === 2) {
        this.type = "document";
      }
      else if (this.type === 3) {
        this.type = "edge";
      }

      this.tableView.setElement($(this.table)).drawLoading();

      this.collectionContext = this.collectionsStore.getPosition(
        this.collection.collectionID
      );

      this.collectionName = window.location.hash.split("/")[1];
      //fill navigation and breadcrumb
      this.breadcrumb();
      window.arangoHelper.buildCollectionSubNav(this.collectionName, 'Content');

      this.checkCollectionState();

      //set last active collection name
      this.lastCollectionName = this.collectionName;

      /*
      if (this.collectionContext.prev === null) {
        $('#collectionPrev').parent().addClass('disabledPag');
      }
      if (this.collectionContext.next === null) {
        $('#collectionNext').parent().addClass('disabledPag');
      }
      */

      this.uploadSetup();

      $("[data-toggle=tooltip]").tooltip();
      $('.upload-info').tooltip();

      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "top");
      this.renderPaginationElements();
      this.selectActivePagesize();
      this.markFilterToggle();
      this.resize();
      return this;
    },

    rerender: function () {
      this.collection.getDocuments(this.getDocsCallback.bind(this));
      this.resize();
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
      if (this.type === 'document') {
        total.html(numeral(this.collection.getTotal()).format('0,0') + " document(s)");
      }
      if (this.type === 'edge') {
        total.html(numeral(this.collection.getTotal()).format('0,0') + " edge(s)");
      }
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + this.collectionName
      );
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
/*global Backbone, document, templateEngine, $, arangoHelper, window*/

(function() {
  "use strict";
  window.FooterView = Backbone.View.extend({
    el: '#footerBar',
    system: {},
    isOffline: true,
    isOfflineCounter: 0,
    firstLogin: true,
    timer: 15000,
    lap: 0,
    timerFunction: null,

    events: {
      'click .footer-center p' : 'showShortcutModal'
    },

    initialize: function () {
      //also server online check
      var self = this;
      window.setInterval(function() {
        self.getVersion();
      }, self.timer);
      self.getVersion();

      window.VISIBLE = true;
      document.addEventListener('visibilitychange', function () {
        window.VISIBLE = !window.VISIBLE;
      });

      $('#offlinePlaceholder button').on('click', function() {
        self.getVersion();
      });
    },

    template: templateEngine.createTemplate("footerView.ejs"),

    showServerStatus: function(isOnline) {
      var self = this;

      if (!window.App.isCluster) {
        if (isOnline === true) {
          $('#healthStatus').removeClass('negative');
          $('#healthStatus').addClass('positive');
          $('.health-state').html('GOOD');
          $('.health-icon').html('<i class="fa fa-check-circle"></i>');
          $('#offlinePlaceholder').hide();
        }
        else {
          $('#healthStatus').removeClass('positive');
          $('#healthStatus').addClass('negative');
          $('.health-state').html('UNKNOWN');
          $('.health-icon').html('<i class="fa fa-exclamation-circle"></i>');

          //show offline overlay
          $('#offlinePlaceholder').show();
          this.reconnectAnimation(0);
        }
      }
      else {
        self.collection.fetch({
          success: function() {
            self.renderClusterState(true);
          },
          error: function() {
            self.renderClusterState(false);
          }
        });
      }
    },

    reconnectAnimation: function(lap) {
      var self = this;

      if (lap === 0) {
        self.lap = lap;
        $('#offlineSeconds').text(self.timer / 1000);
        clearTimeout(self.timerFunction);
      }

      if (self.lap < this.timer / 1000) {
        self.lap++;
        $('#offlineSeconds').text(self.timer / 1000 - self.lap);

        self.timerFunction = window.setTimeout(function() {
          if (self.timer / 1000 - self.lap === 0) {
            self.getVersion();
          }
          else {
            self.reconnectAnimation(self.lap);
          }
        }, 1000);
      }
    },

    renderClusterState: function(connection) {
      var error = 0;

      if (connection) {
        this.collection.each(function(value) {
          if (value.toJSON().status !== 'ok') {
            error++;
          }
        });

        if (error > 0) {
          $('#healthStatus').removeClass('positive');
          $('#healthStatus').addClass('negative');
          if (error === 1) {
            $('.health-state').html(error + ' NODE ERROR');
          }
          else {
            $('.health-state').html(error + ' NODES ERROR');
          }
          $('.health-icon').html('<i class="fa fa-exclamation-circle"></i>');
        }
        else {
          $('#healthStatus').removeClass('negative');
          $('#healthStatus').addClass('positive');
          $('.health-state').html('NODES OK');
          $('.health-icon').html('<i class="fa fa-check-circle"></i>');
        }
      }
      else {
        $('#healthStatus').removeClass('positive');
        $('#healthStatus').addClass('negative');
        $('.health-state').html(window.location.host + ' OFFLINE');
        $('.health-icon').html('<i class="fa fa-exclamation-circle"></i>');
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
        error: function () {
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
    className: 'tile pure-u-1-1 pure-u-sm-1-2 pure-u-md-1-3 pure-u-lg-1-4 pure-u-xl-1-6',
    template: templateEngine.createTemplate('foxxActiveView.ejs'),
    _show: true,

    events: {
      'click' : 'openAppDetailView'
    },

    openAppDetailView: function() {
      window.App.navigate('service/' + encodeURIComponent(this.model.get('mount')), { trigger: true });
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

      var conf = function() {
        if (this.model.needsConfiguration()) {

          if ($(this.el).find('.warning-icons').length > 0) {
            $(this.el).find('.warning-icons')
            .append('<span class="fa fa-cog" title="Needs configuration"></span>');
          }
          else {
            $(this.el).find('img')
            .after(
              '<span class="warning-icons"><span class="fa fa-cog" title="Needs configuration"></span></span>'
            );
          }
        }
      }.bind(this);

      var depend = function() {
        if (this.model.hasUnconfiguredDependencies()) {

          if ($(this.el).find('.warning-icons').length > 0) {
            $(this.el).find('.warning-icons')
            .append('<span class="fa fa-cubes" title="Unconfigured dependencies"></span>');
          }
          else {
            $(this.el).find('img')
            .after(
              '<span class="warning-icons"><span class="fa fa-cubes" title="Unconfigured dependencies"></span></span>'
            );
          }
        }
      }.bind(this);

      /*isBroken function in model doesnt make sense
      var broken = function() {
        $(this.el).find('warning-icons')
        .append('<span class="fa fa-warning" title="Mount error"></span>');
      }.bind(this);
       */

      this.model.getConfiguration(conf);
      this.model.getDependencies(depend);

      return $(this.el);
    }
  });
}());

/*jshint browser: true */
/*global $, Joi, _, arangoHelper, templateEngine, window*/
(function() {
  "use strict";

  var errors = require("internal").errors;
  var appStoreTemplate = templateEngine.createTemplate("applicationListView.ejs");

  var FoxxInstallView = function(opts) {
    this.collection = opts.collection;
  };

  var installCallback = function(result) {
    var self = this;

    if (result.error === false) {
      this.collection.fetch({
        success: function() {
          window.modalView.hide();
          self.reload();
        }
      });
                           
    } else {
      var res = result;
      if (result.hasOwnProperty("responseJSON")) {
        res = result.responseJSON;
      } 
      switch(res.errorNum) {
        case errors.ERROR_APPLICATION_DOWNLOAD_FAILED.code:
          arangoHelper.arangoError("Services", "Unable to download application from the given repository.");
          break;
        default:
          arangoHelper.arangoError("Services", res.errorNum + ". " + res.errorMessage);
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
            rule: Joi.string().regex(/^\/([^_]|_open\/)/),
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
            msg: "No valid Github account and repository."
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

  var switchTab = function(openTab) {
    window.modalView.clearValidators();
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
    
    if (! button.prop("disabled") && ! window.modalView.modalTestAll()) {
      // trigger the validation so the "ok" button has the correct state
      button.prop("disabled", true);
    }
  };

  var switchModalButton = function(event) {
    var openTab = $(event.currentTarget).attr("href").substr(1);
    switchTab.call(this, openTab);
  };

  var installFoxxFromStore = function(e) {
    switchTab.call(this, "appstore");
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
    if (data === undefined) {
      data = this._uploadData;
    }
    else {
      this._uploadData = data;
    }
    if (data && window.modalView.modalTestAll()) {
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
        documentCollections: _.map($('#new-app-document-collections').select2("data"), function(d) {
          return window.arangoHelper.escapeHtml(d.text);
        }),
        edgeCollections: _.map($('#new-app-edge-collections').select2("data"), function(d) {
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
        installFoxxFromZip.apply(this);
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
      "Install Service",
      buttons,
      upgrade,
      undefined,
      undefined,
      modalEvents
    );
    $("#new-app-document-collections").select2({
      tags: [],
      showSearchBox: false,
      minimumResultsForSearch: -1,
      width: "336px"
    });
    $("#new-app-edge-collections").select2({
      tags: [],
      showSearchBox: false,
      minimumResultsForSearch: -1,
      width: "336px"
    });

    var checkButton = function() {
      var button = $("#modalButton1");
        if (! button.prop("disabled") && ! window.modalView.modalTestAll()) {
          button.prop("disabled", true);
        }
        else {
          button.prop("disabled", false);
        }
    };

    $('.select2-search-field input').focusout(function() {
      checkButton();
      window.setTimeout(function() {
        if ($('.select2-drop').is(':visible')) {
          if (!$('#select2-search-field input').is(':focus')) {
            $('#s2id_new-app-document-collections').select2('close');
            $('#s2id_new-app-edge-collections').select2('close');
            checkButton();
          }
        }
      }, 80);
    });
    $('.select2-search-field input').focusin(function() {
      if ($('.select2-drop').is(':visible')) {
        var button = $("#modalButton1");
        button.prop("disabled", true);
      }
    });
    $("#upload-foxx-zip").uploadFile({
      url: "/_api/upload?multipart=true",
      allowedTypes: "zip",
      multiple: false,
      onSuccess: installFoxxFromZip.bind(scope)
    });
    $.get("foxxes/fishbowl", function(list) {
      var table = $("#appstore-content");
      table.html('');
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
    this._uploadData = undefined;
    delete this.mount;
    render(this, false);
    window.modalView.clearValidators();
    setMountpointValidators();
    setNewAppValidators();
  };

  FoxxInstallView.prototype.upgrade = function(mount, callback) {
    this.reload = callback;
    this._upgrade = true;
    this._uploadData = undefined;
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

    initialize: function(options) {
      this.options = options;
    },

    events: {
      "click #deleteGraph"                        : "deleteGraph",
      "click .icon_arangodb_settings2.editGraph"  : "editGraph",
      "click #createGraph"                        : "addNewGraph",
      "keyup #graphManagementSearchInput"         : "search",
      "click #graphManagementSearchSubmit"        : "search",
      "click .tile-graph"                         : "redirectToGraphViewer",
      "click #graphManagementToggle"              : "toggleGraphDropdown",
      "click .css-label"                          : "checkBoxes",
      "change #graphSortDesc"                     : "sorting"
    },

    toggleTab: function(e) {
      var id = e.currentTarget.id;
      id = id.replace('tab-', '');
      $('#tab-content-create-graph .tab-pane').removeClass('active');
      $('#tab-content-create-graph #' + id).addClass('active');

      if (id === 'exampleGraphs') {
        $('#modal-dialog .modal-footer .button-success').css("display", "none");
      }
      else {
        $('#modal-dialog .modal-footer .button-success').css("display", "initial");
      }
    },

    redirectToGraphViewer: function(e) {
      var name = $(e.currentTarget).attr("id");
      name = name.substr(0, name.length - 5);
      window.location = window.location + '/' + encodeURIComponent(name);
    },

    loadGraphViewer: function(graphName, refetch) {

      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("","");
        }
        else {
          var edgeDefs = this.collection.get(graphName).get("edgeDefinitions");
          if (!edgeDefs || edgeDefs.length === 0) {
            // User Info
            return;
          }
          var adapterConfig = {
            type: "gharial",
            graphName: graphName,
            baseUrl: require("internal").arango.databasePrefix("/")
          };
          var width = $("#content").width() - 75;
          $("#content").html("");

          var height = arangoHelper.calculateCenterDivHeight();

          this.ui = new GraphViewerUI($("#content")[0], adapterConfig, width, $('.centralRow').height() - 135, {
            nodeShaper: {
              label: "_key",
              color: {
                type: "attribute",
                key: "_key"
              }
            }

          }, true);

          $('.contentDiv').height(height);
        }
      }.bind(this);

      if (refetch) {
        this.collection.fetch({
          success: function() {
            callback();
          }
        });
      }
      else {
        callback();
      }

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

      if ($('#dropGraphCollections').is(':checked')) {

        var callback = function(success) {
          if (success) {
            self.collection.remove(self.collection.get(name));
            self.updateGraphManagementView();
            window.modalView.hide();
          }
          else {
            window.modalView.hide();
            arangoHelper.arangoError("Graph", "Could not delete Graph.");
          }
        }.bind(this);

        this.collection.dropAndDeleteGraph(name, callback);
      }
      else {
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
      }
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

    createExampleGraphs: function(e) {
      var graph = $(e.currentTarget).attr('graph-id'), self = this;
      $.ajax({
        type: "POST",
        url: "/_admin/aardvark/graph-examples/create/" + encodeURIComponent(graph),
        success: function () {
          window.modalView.hide();
          self.updateGraphManagementView();
          arangoHelper.arangoNotification('Example Graphs', 'Graph: ' + graph + ' created.');
        },
        error: function (err) {
          window.modalView.hide();
          console.log(err);
          if (err.responseText) {
            try {
              var msg = JSON.parse(err.responseText);
              arangoHelper.arangoError('Example Graphs', msg.errorMessage);
            }
            catch (e) {
              arangoHelper.arangoError('Example Graphs', 'Could not create example graph: ' + graph);
            }
          }
          else {
            arangoHelper.arangoError('Example Graphs', 'Could not create example graph: ' + graph);
          }
        }
      });
    },

    render: function(name, refetch) {

      var self = this;
      this.collection.fetch({

        success: function() {
          self.collection.sort();

          $(self.el).html(self.template.render({
            graphs: self.collection,
            searchString : ''
          }));

          if (self.dropdownVisible === true) {
            $('#graphManagementDropdown2').show();
            $('#graphSortDesc').attr('checked', self.collection.sortOptions.desc);
            $('#graphManagementToggle').toggleClass('activated');
            $('#graphManagementDropdown').show();
          }

          self.events["click .tableRow"] = self.showHideDefinition.bind(self);
          self.events['change tr[id*="newEdgeDefinitions"]'] = self.setFromAndTo.bind(self);
          self.events["click .graphViewer-icon-button"] = self.addRemoveDefinition.bind(self);
          self.events["click #graphTab a"] = self.toggleTab.bind(self);
          self.events["click .createExampleGraphs"] = self.createExampleGraphs.bind(self);
          self.events["focusout .select2-search-field input"] = function(e){
            if ($('.select2-drop').is(':visible')) {
              if (!$('#select2-search-field input').is(':focus')) {
                window.setTimeout(function() { 
                  $(e.currentTarget).parent().parent().parent().select2('close');
                }, 80);
              }
            } 
          }.bind(self);
          arangoHelper.setCheckboxStatus("#graphManagementDropdown");
        }
      });

      if (name) {
        this.loadGraphViewer(name, refetch);
      }
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
        $('#s2id_newEdgeDefinitions0')
        .parent()
        .parent()
        .next().find('.select2-choices').css("border-color", "red");
        $('#s2id_newEdgeDefinitions0').
          parent()
          .parent()
          .next()
          .next()
          .find('.select2-choices')
          .css("border-color", "red");
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

      if (edgeDefinitions.length === 0) {
        $('#s2id_newEdgeDefinitions0 .select2-choices').css("border-color", "red");
        $('#s2id_newEdgeDefinitions0').parent()
        .parent()
        .next()
        .find('.select2-choices')
        .css("border-color", "red");
        $('#s2id_newEdgeDefinitions0').parent()
        .parent()
        .next()
        .next()
        .find('.select2-choices')
        .css("border-color", "red");
        return;
      }

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
        if (c.get('type') === "edge") {
          self.eCollList.push(c.id);
        }
        else {
          collList.push(c.id);
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
      } 
      else {
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
        "modalGraphTable.ejs", title, buttons, tableContent, undefined, undefined, this.events
      );

      if (graph) {

        $('.modal-body table').css('border-collapse', 'separate');
        var i;

        $('.modal-body .spacer').remove();
        for (i = 0; i <= this.counter; i++) {
          $('#row_fromCollections' + i).show();
          $('#row_toCollections' + i).show();
          $('#row_newEdgeDefinitions' + i).addClass('first');
          $('#row_fromCollections' + i).addClass('middle');
          $('#row_toCollections' + i).addClass('last');
          $('#row_toCollections' + i).after('<tr id="spacer'+ i +'" class="spacer"></tr>');
        }
        
        $('#graphTab').hide(); 
        $('#modal-dialog .modal-delete-confirmation').append(
          '<fieldset><input type="checkbox" id="dropGraphCollections" name="" value="">' + 
            '<label for="mc">also drop collections?</label>' +
          '</fieldset>'
        );
      }

    },

    showHideDefinition : function(e) {
      /*e.stopPropagation();
      var id = $(e.currentTarget).attr("id"), number;
      if (id.indexOf("row_newEdgeDefinitions") !== -1 ) {
        number = id.split("row_newEdgeDefinitions")[1];
        $('#row_fromCollections' + number).toggle();
        $('#row_toCollections' + number).toggle();
      }*/
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
        
        var i;
        $('.modal-body .spacer').remove();
        for (i = 0; i <= this.counter; i++) {
          $('#row_fromCollections' + i).show();
          $('#row_toCollections' + i).show();
          $('#row_newEdgeDefinitions' + i).addClass('first');
          $('#row_fromCollections' + i).addClass('middle');
          $('#row_toCollections' + i).addClass('last');
          $('#row_toCollections' + i).after('<tr id="spacer'+ i +'" class="spacer"></tr>');
        }
        return;
      }
      if (id.indexOf("remove_newEdgeDefinitions") !== -1 ) {
        number = id.split("remove_newEdgeDefinitions")[1];
        $('#row_newEdgeDefinitions' + number).remove();
        $('#row_fromCollections' + number).remove();
        $('#row_toCollections' + number).remove();
        $('#spacer' + number).remove();
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
/*global arangoHelper, Backbone, templateEngine, $, window*/
(function () {
  "use strict";

  window.HelpUsView = Backbone.View.extend({

    el: "#content",

    template: templateEngine.createTemplate("helpUsView.ejs"),

    render: function () {
      this.$el.html(this.template.render({}));
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global _, arangoHelper, Backbone, window, templateEngine, $ */

(function() {
  "use strict";

  window.IndicesView = Backbone.View.extend({

    el: "#content",

    initialize: function(options) {
      this.collectionName = options.collectionName;
      this.model = this.collection;
    },

    template: templateEngine.createTemplate("indicesView.ejs"),

    events: {
    },

    render: function() {
      $(this.el).html(this.template.render({
        model: this.model
      }));

      this.breadcrumb();
      window.arangoHelper.buildCollectionSubNav(this.collectionName, 'Indices');

      this.getIndex();
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + this.collectionName
      );
    },

    getIndex: function () {

      var callback = function(error, data) {
        if (error) {
          window.arangoHelper.arangoError('Index', data.errorMessage);
        }
        else {
          this.renderIndex(data);
        }
      }.bind(this);

      this.model.getIndex(callback);
    },

    createIndex: function () {
      //e.preventDefault();
      var self = this;
      var indexType = $('#newIndexType').val();
      var postParameter = {};
      var fields;
      var unique;
      var sparse;

      switch (indexType) {
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

      var callback = function(error, msg){
        if (error) {
          if (msg) {
            var message = JSON.parse(msg.responseText);
            arangoHelper.arangoError("Document error", message.errorMessage);
          }
          else {
            arangoHelper.arangoError("Document error", "Could not create index.");
          }
        }
        //toggle back
        self.toggleNewIndexView();

        //rerender
        self.render();
      };

      this.model.createIndex(postParameter, callback);
    },

    bindIndexEvents: function() {
      this.unbindIndexEvents();
      var self = this;

      $('#indexEditView #addIndex').bind('click', function() {
        self.toggleNewIndexView();

        $('#cancelIndex').unbind('click');
        $('#cancelIndex').bind('click', function() {
          self.toggleNewIndexView();
          self.render();
        });

        $('#createIndex').unbind('click');
        $('#createIndex').bind('click', function() {
          self.createIndex();
        });

      });

      $('#newIndexType').bind('change', function() {
        self.selectIndexType();
      });

      $('.deleteIndex').bind('click', function(e) {
        self.prepDeleteIndex(e);
      });

      $('#infoTab a').bind('click', function(e) {
        $('#indexDeleteModal').remove();
        if ($(e.currentTarget).html() === 'Indices'  && !$(e.currentTarget).parent().hasClass('active')) {

          $('#newIndexView').hide();
          $('#indexEditView').show();

          $('#indexHeaderContent #modal-dialog .modal-footer .button-danger').hide();  
          $('#indexHeaderContent #modal-dialog .modal-footer .button-success').hide();  
          $('#indexHeaderContent #modal-dialog .modal-footer .button-notification').hide();
        }
        if ($(e.currentTarget).html() === 'General' && !$(e.currentTarget).parent().hasClass('active')) {
          $('#indexHeaderContent #modal-dialog .modal-footer .button-danger').show();  
          $('#indexHeaderContent #modal-dialog .modal-footer .button-success').show();  
          $('#indexHeaderContent #modal-dialog .modal-footer .button-notification').show();
          var elem2 = $('.index-button-bar2')[0]; 
          //$('#addIndex').detach().appendTo(elem);
          if ($('#cancelIndex').is(':visible')) {
            $('#cancelIndex').detach().appendTo(elem2);
            $('#createIndex').detach().appendTo(elem2);
          }
        }
      });
    },

    prepDeleteIndex: function (e) {
      var self = this;
      this.lastTarget = e;

      this.lastId = $(this.lastTarget.currentTarget).
        parent().
        parent().
        first().
        children().
        first().
        text();
      //window.modalView.hide();

      //delete modal
      $("#modal-dialog .modal-footer").after(
        '<div id="indexDeleteModal" style="display:block;" class="alert alert-error modal-delete-confirmation">' +
        '<strong>Really delete?</strong>' +
        '<button id="indexConfirmDelete" class="button-danger pull-right modal-confirm-delete">Yes</button>' +
        '<button id="indexAbortDelete" class="button-neutral pull-right">No</button>' +
        '</div>'
      );
      $('#indexConfirmDelete').unbind('click');
      $('#indexConfirmDelete').bind('click', function() {
        $('#indexDeleteModal').remove();
        self.deleteIndex();
      });

      $('#indexAbortDelete').unbind('click');
      $('#indexAbortDelete').bind('click', function() {
        $('#indexDeleteModal').remove();
      });
    },

    unbindIndexEvents: function() {
      $('#indexEditView #addIndex').unbind('click');
      $('#newIndexType').unbind('change');
      $('#infoTab a').unbind('click');
      $('.deleteIndex').unbind('click');
    },

    deleteIndex: function () {
      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("Could not delete index");
          $("tr th:contains('"+ this.lastId+"')").parent().children().last().html(
            '<span class="deleteIndex icon_arangodb_roundminus"' + 
              ' data-original-title="Delete index" title="Delete index"></span>'
          );
          this.model.set("locked", false);
        }
        else if (!error && error !== undefined) {
          $("tr th:contains('"+ this.lastId+"')").parent().remove();
          this.model.set("locked", false);
        }
      }.bind(this);

      this.model.set("locked", true);
      this.model.deleteIndex(this.lastId, callback);

      $("tr th:contains('"+ this.lastId+"')").parent().children().last().html(
        '<i class="fa fa-circle-o-notch fa-spin"></i>'
      );
    },
    renderIndex: function(data) {

      this.index = data;

      var cssClass = 'collectionInfoTh modal-text';
      if (this.index) {
        var fieldString = '';
        var actionString = '';

        _.each(this.index.indexes, function(v) {
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
      }
      this.bindIndexEvents();
    },

    selectIndexType: function () {
      $('.newIndexClass').hide();
      var type = $('#newIndexType').val();
      $('#newIndexType'+type).show();
    },

    resetIndexForms: function () {
      $('#indexHeader input').val('').prop("checked", false);
      $('#newIndexType').val('Geo').prop('selected',true);
      this.selectIndexType();
    },

    toggleNewIndexView: function () {
      var elem = $('.index-button-bar2')[0];

      if ($('#indexEditView').is(':visible')) {
        $('#indexEditView').hide();
        $('#newIndexView').show();
        $('#cancelIndex').detach().appendTo('#indexHeaderContent #modal-dialog .modal-footer');
        $('#createIndex').detach().appendTo('#indexHeaderContent #modal-dialog .modal-footer');
      }
      else {
        $('#indexEditView').show();
        $('#newIndexView').hide();
        $('#cancelIndex').detach().appendTo(elem);
        $('#createIndex').detach().appendTo(elem);
      }

      arangoHelper.fixTooltips(".icon_arangodb, .arangoicon", "right");
      this.resetIndexForms();
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

    checkboxToValue: function (id) {
      return $(id).prop('checked');
    }

  });

}());

/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, window, templateEngine, $ */

(function() {
  "use strict";

  window.InfoView = Backbone.View.extend({

    el: "#content",

    initialize: function(options) {
      this.collectionName = options.collectionName;
      this.model = this.collection;
    },

    events: {
    },

    render: function() {
      this.breadcrumb();
      window.arangoHelper.buildCollectionSubNav(this.collectionName, 'Info');

      this.renderInfoView();
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + this.collectionName
      );
    },

    renderInfoView: function() {
      if (this.model.get("locked")) {
        return 0;
      }
      var callbackRev = function(error, revision, figures) {
        if (error) {
          arangoHelper.arangoError("Figures", "Could not get revision.");        
        }
        else {
          var buttons = [];
          var tableContent = {
            figures: figures,
            revision: revision,
            model: this.model
          };
          window.modalView.show(
            "modalCollectionInfo.ejs",
            "Collection: " + this.model.get('name'),
            buttons,
            tableContent, null, null,
            null, null,
            null, 'content'
          );
        }
      }.bind(this);

      var callback = function(error, data) {
        if (error) {
          arangoHelper.arangoError("Figures", "Could not get figures.");        
        }
        else {
          var figures = data;
          this.model.getRevision(callbackRev, figures);
        }
      }.bind(this);

      this.model.getFigures(callback);
    }

  });

}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, document, EJS, _, arangoHelper, window, setTimeout, $, templateEngine*/

(function() {
  "use strict";
  window.loginView = Backbone.View.extend({
    el: '#content',
    el2: '.header',
    el3: '.footer',
    loggedIn: false,

    events: {
      "keyPress #loginForm input" : "keyPress",
      "click #submitLogin" : "validate",
      "submit #dbForm"    : "goTo",
      "click #logout"     : "logout",
      "change #loginDatabase" : "renderDBS"
    },

    template: templateEngine.createTemplate("loginView.ejs"),

    render: function() {
      $(this.el).html(this.template.render({}));
      $(this.el2).hide();
      $(this.el3).hide();
      $('.bodyWrapper').show();

      $('#loginUsername').focus();

      return this;
    },

    clear: function () {
      $('#loginForm input').removeClass("form-error");
      $('.wrong-credentials').hide();
    },

    keyPress: function(e) {
      if (e.ctrlKey && e.keyCode === 13) {
        e.preventDefault();
        this.validate();
      }
      else if (e.metaKey && e.keyCode === 13) {
        e.preventDefault();
        this.validate();
      }
    },

    validate: function(event) {
      event.preventDefault();
      this.clear();

      var username = $('#loginUsername').val();
      var password = $('#loginPassword').val();

      if (!username) {
        //do not send unneccessary requests if no user is given
        return;
      }

      var callback = function(error) {
        var self = this;
        if (error) {
          $('.wrong-credentials').show();
          $('#loginDatabase').html('');
          $('#loginDatabase').append(
            '<option>_system</option>'
          ); 
        }
        else {
          $('.wrong-credentials').hide();
          self.loggedIn = true;
          //get list of allowed dbs
          $.ajax("/_api/database/user").success(function(data) {

            $('#loginForm').hide();
            $('#databases').show();

            //enable db select and login button
            $('#loginDatabase').html('');
            //fill select with allowed dbs
            _.each(data.result, function(db) {
              $('#loginDatabase').append(
                '<option>' + db + '</option>'
              ); 
            });

            self.renderDBS();
          });
        }
      }.bind(this);

      this.collection.login(username, password, callback);
    },

    renderDBS: function() {
      var db = $('#loginDatabase').val();
      $('#goToDatabase').html("Select: "  + db);
      $('#goToDatabase').focus();
    },

    logout: function() {
      this.collection.logout();
    },

    goTo: function (e) {
      e.preventDefault();
      var username = $('#loginUsername').val();
      var database = $('#loginDatabase').val();

      var callback2 = function(error) {
        if (error) {
          arangoHelper.arangoError("User", "Could not fetch user settings"); 
        }
      };

      var path = window.location.protocol + "//" + window.location.host + "/_db/" + database + "/_admin/aardvark/index.html";

      window.location.href = path;

      //show hidden divs
      $(this.el2).show();
      $(this.el3).show();
      $('.bodyWrapper').show();
      $('.navbar').show();

      $('#currentUser').text(username);
      this.collection.loadUserSettings(callback2);
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

    initialize: function (options) {
      this.options = options;
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
        ["All", "logall"],
        ["Info", "loginfo"],
        ["Error", "logerror"],
        ["Warning", "logwarning"],
        ["Debug", "logdebug"]
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
      $('#logContent').append('<div id="logPaginationDiv" class="pagination-line"></div>');
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

  var createButtonStub = function(type, title, cb, confirm) {
    return {
      type: type,
      title: title,
      callback: cb,
      confirm: confirm
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
      BLOB: "blob",
      PASSWORD: "password",
      SELECT: "select",
      SELECT2: "select2",
      CHECKBOX: "checkbox"
    },

    initialize: function() {
      Object.freeze(this.buttons);
      Object.freeze(this.tables);
    },

    createModalHotkeys: function() {
      //submit modal
      $(this.el).unbind('keydown');
      $(this.el).unbind('return');
      $(this.el).bind('keydown', 'return', function(){
        $('.createModalDialog .modal-footer .button-success').click();
      });

      $('.modal-body input').unbind('keydown');
      $('.modal-body input').unbind('return');
      $(".modal-body input", $(this.el)).bind('keydown', 'return', function(){
        $('.createModalDialog .modal-footer .button-success').click();
      });

      $('.modal-body select').unbind('keydown');
      $('.modal-body select').unbind('return');
      $(".modal-body select", $(this.el)).bind('keydown', 'return', function(){
        $('.createModalDialog .modal-footer .button-success').click();
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
      var hasFocus = $('.createModalDialog .modal-footer button').is(':focus');
      if (hasFocus === false) {
        if (direction === 'left') {
          $('.createModalDialog .modal-footer button').first().focus();
        }
        else if (direction === 'right') {
          $('.createModalDialog .modal-footer button').last().focus();
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

    createCloseButton: function(title, cb) {
      var self = this;
      return createButtonStub(this.buttons.CLOSE, title, function () {
        self.hide();
        if (cb) {
          cb();
        }
      });
    },

    createSuccessButton: function(title, cb) {
      return createButtonStub(this.buttons.SUCCESS, title, cb);
    },

    createNotificationButton: function(title, cb) {
      return createButtonStub(this.buttons.NOTIFICATION, title, cb);
    },

    createDeleteButton: function(title, cb, confirm) {
      return createButtonStub(this.buttons.DELETE, title, cb, confirm);
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

    createBlobEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.BLOB, label, value, info, placeholder, mandatory,
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

    createPasswordEntry: function(id, label, value, info, placeholder, mandatory, regexp) {
      var obj = createTextStub(this.tables.PASSWORD, label, value, info, placeholder, mandatory, regexp);
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
                   extraInfo, events, noConfirm, tabBar, divID) {
      var self = this, lastBtn, confirmMsg, closeButtonFound = false;
      buttons = buttons || [];
      noConfirm = Boolean(noConfirm);
      this.clearValidators();
      if (buttons.length > 0) {
        buttons.forEach(function (b) {
          if (b.type === self.buttons.CLOSE) {
              closeButtonFound = true;
          }
          if (b.type === self.buttons.DELETE) {
              confirmMsg = confirmMsg || b.confirm;
          }
        });
        if (!closeButtonFound) {
          // Insert close as second from right
          lastBtn = buttons.pop();
          buttons.push(self.createCloseButton('Cancel'));
          buttons.push(lastBtn);
        }
      } else {
        buttons.push(self.createCloseButton('Close'));
      }
      if (!divID) {
        $(this.el).html(this.baseTemplate.render({
          title: title,
          buttons: buttons,
          hideFooter: this.hideFooter,
          confirm: confirmMsg,
          tabBar: tabBar
        }));
      }
      else {
        //render into custom div
        $('#' + divID).html(this.baseTemplate.render({
          title: title,
          buttons: buttons,
          hideFooter: this.hideFooter,
          confirm: confirmMsg,
          tabBar: tabBar
        }));
        //remove not needed modal elements
        $('#' + divID + " #modal-dialog").removeClass("fade hide modal");
        $('#' + divID + " .modal-header").remove();
        $('#' + divID + " .modal-tabbar").remove();
        $('#' + divID + " .modal-tabbar").remove();
        $('#' + divID + " .button-close").remove();
        if ($('#' + divID + " .modal-footer").children().length === 0) {
          $('#' + divID + " .modal-footer").remove();
        }

      }
      _.each(buttons, function(b, i) {
        if (b.disabled || !b.callback) {
          return;
        }
        if (b.type === self.buttons.DELETE && !noConfirm) {
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

      var template;
      if (typeof templateName === 'string') {
        template = templateEngine.createTemplate(templateName);
        $(".createModalDialog .modal-body").html(template.render({
          content: tableContent,
          advancedContent: advancedContent,
          info: extraInfo
        }));
      }
      else {
        var counter = 0;
        _.each(templateName, function(v) {
          template = templateEngine.createTemplate(v);
          $(".createModalDialog .modal-body .tab-content #" + tabBar[counter]).html(template.render({
            content: tableContent,
            advancedContent: advancedContent,
            info: extraInfo
          }));

          counter++;
        });
      }

      $('.createModalDialog .modalTooltips').tooltip({
        position: {
          my: "left top",
          at: "right+55 top-1"
        }
      });

      var completeTableContent = tableContent || [];
      if (advancedContent && advancedContent.content) {
        completeTableContent = completeTableContent.concat(advancedContent.content);
      }

      _.each(completeTableContent, function(row) {
        self.modalBindValidation(row);
        if (row.type === self.tables.SELECT2) {
          //handle select2
          $('#'+row.id).select2({
            tags: row.tags || [],
            showSearchBox: false,
            minimumResultsForSearch: -1,
            width: "336px",
            maximumSelectionSize: row.maxEntrySize || 8
          });
        }
      });

      if (events) {
        this.events = events;
        this.delegateEvents();
      }

      if ($('#accordion2')) {
        $('#accordion2 .accordion-toggle').bind("click", function() {
          if ($('#collapseOne').is(":visible")) {
            $('#collapseOne').hide();
            setTimeout(function() {
              $('.accordion-toggle').addClass('collapsed');
            }, 100);
          }
          else {
            $('#collapseOne').show();
            setTimeout(function() {
              $('.accordion-toggle').removeClass('collapsed');
            }, 100);
          }
        });
        $('#collapseOne').hide();
        setTimeout(function() {
          $('.accordion-toggle').addClass('collapsed');
        }, 100);
      }

      if (!divID) {
        $("#modal-dialog").modal("show");
      }

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
        }, 400);
      }

    },

    modalBindValidation: function(entry) {
      var self = this;
      if (entry.hasOwnProperty("id")
        && entry.hasOwnProperty("validateInput")) {
        var validCheck = function() {
          var $el = $("#" + entry.id);
          var validation = entry.validateInput($el);
          var error = false;
          _.each(validation, function(validator) {
            var value = $el.val();
            if (!validator.rule) {
              validator = {rule: validator};
            }
            if (typeof validator.rule === 'function') {
              try {
                validator.rule(value);
              } catch (e) {
                error = validator.msg || e.message;
              }
            } else {
              var result = Joi.validate(value, validator.rule);
              if (result.error) {
                error = validator.msg || result.error.message;
              }
            }
            if (error) {
              return false;
            }
          });
          if (error) {
            return error;
          }
        };
        var $el = $('#' + entry.id);
        // catch result of validation and act
        $el.on('keyup focusout', function() {
          var msg = validCheck();
          var errorElement = $el.next()[0];
          if (msg) {
            $el.addClass('invalid-input');
            if (errorElement) {
              //error element available
              $(errorElement).text(msg);
            }
            else {
              //error element not available
              $el.after('<p class="errorMessage">' + msg+ '</p>');
            }
            $('.createModalDialog .modal-footer .button-success')
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
        $('.createModalDialog .modal-footer .button-success')
          .prop('disabled', true)
          .addClass('disabled');
      } else {
        $('.createModalDialog .modal-footer .button-success')
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
/*global Backbone, templateEngine, $, window, arangoHelper, _*/
(function () {
  "use strict";
  window.NavigationView = Backbone.View.extend({
    el: '#navigationBar',
    subEl: '#subNavigationBar',

    events: {
      "change #arangoCollectionSelect": "navigateBySelect",
      "click .tab": "navigateByTab",
      "click li": "switchTab",
      "click .arangodbLogo": "selectMenuItem",
      "mouseenter .dropdown > *": "showDropdown",
      'click .shortcut-icons p' : 'showShortcutModal',
      "mouseleave .dropdown": "hideDropdown"
    },

    renderFirst: true,
    activeSubMenu: undefined,

    initialize: function (options) {

      var self = this;

      this.userCollection = options.userCollection;
      this.currentDB = options.currentDB;
      this.dbSelectionView = new window.DBSelectionView({
        collection: options.database,
        current: this.currentDB
      });
      this.userBarView = new window.UserBarView({
        userCollection: this.userCollection
      });
      this.notificationView = new window.NotificationView({
        collection: options.notificationCollection
      });
      this.statisticBarView = new window.StatisticBarView({
        currentDB: this.currentDB
      });

      this.isCluster = options.isCluster;

      this.handleKeyboardHotkeys();

      Backbone.history.on("all", function () {
        self.selectMenuItem();
      });
    },

    showShortcutModal: function() {
      arangoHelper.hotkeysFunctions.showHotkeysModal();
    },

    handleSelectDatabase: function () {
      this.dbSelectionView.render($("#dbSelect"));
    },

    template: templateEngine.createTemplate("navigationView.ejs"),
    templateSub: templateEngine.createTemplate("subNavigationView.ejs"),

    render: function () {
      var self = this;

      $(this.el).html(this.template.render({
        currentDB: this.currentDB,
        isCluster: this.isCluster
      }));

      if (this.currentDB.get("name") !== '_system') {
        $('#dashboard').parent().remove();
      }

      $(this.subEl).html(this.templateSub.render({
        currentDB: this.currentDB.toJSON()
      }));
      
      this.dbSelectionView.render($("#dbSelect"));
      //this.notificationView.render($("#notificationBar"));

      var callback = function(error) {
        if (!error) {
          this.userBarView.render();
        }
      }.bind(this);

      this.userCollection.whoAmI(callback);
      //this.statisticBarView.render($("#statisticBar"));

      if (this.renderFirst) {
        this.renderFirst = false;

        this.selectMenuItem();

        $('.arangodbLogo').on('click', function() {
          self.selectMenuItem();
        });
      }

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

      var tab = e.target || e.srcElement,
      navigateTo = tab.id,
      dropdown = false;

      if ($(tab).hasClass('fa')) {
        return;
      }

      if (navigateTo === "") {
        navigateTo = $(tab).attr("class");
      }
      
      if (navigateTo === "links") {
        dropdown = true;
        $("#link_dropdown").slideToggle(1);
        e.preventDefault();
      }
      else if (navigateTo === "tools") {
        dropdown = true;
        $("#tools_dropdown").slideToggle(1);
        e.preventDefault();
      }
      else if (navigateTo === "dbselection") {
        dropdown = true;
        $("#dbs_dropdown").slideToggle(1);
        e.preventDefault();
      }

      if (!dropdown) {
        window.App.navigate(navigateTo, {trigger: true});
        e.preventDefault();
      }
    },

    handleSelectNavigation: function () {
      var self = this;
      $("#arangoCollectionSelect").change(function() {
        self.navigateBySelect();
      });
    },

    subViewConfig: {
      documents: 'collections',
      collection: 'collections'
    },

    subMenuConfig: {
      /*
      collection: [
        {
          name: 'Settings',
          view: undefined
        },
        {
          name: 'Indices',
          view: undefined
        },
        {
          name: 'Content',
          view: undefined,
          active: true
        }
      ],*/
      cluster: [
        {
          name: 'Dashboard',
          view: undefined,
          active: true
        },
        {
          name: 'Logs',
          view: undefined,
          disabled: true
        }
      ],
      collections: [
        {
          name: '',
          view: undefined,
          active: false
        }
      ],
      queries: [
        {
          name: 'Editor',
          route: 'query',
          active: true
        },
        {
          name: 'Running Queries',
          route: 'queryManagement',
          params: {
            active: true
          },
          active: undefined
        },
        {
          name: 'Slow Query History',
          route: 'queryManagement',
          params: {
            active: false
          },
          active: undefined
        }
      ]
    },

    renderSubMenu: function(id) {
      var self = this;

      if (id === undefined) {
        if (window.isCluster) {
          id = 'cluster';
        }
        else {
          id = 'dashboard';
        }
      }

      if (this.subMenuConfig[id]) {
        $(this.subEl + ' .bottom').html('');
        var cssclass = "";

        _.each(this.subMenuConfig[id], function(menu) {
          if (menu.active) {
            cssclass = 'active';
          }
          else {
            cssclass = '';
          }
          if (menu.disabled) {
            cssclass = 'disabled';
          }

          $(self.subEl +  ' .bottom').append(
            '<li class="subMenuEntry ' + cssclass + '"><a>' + menu.name + '</a></li>'
          );
          if (!menu.disabled) {
            $(self.subEl + ' .bottom').children().last().bind('click', function(elem) {
              self.activeSubMenu = menu;
              self.renderSubView(menu, elem);
            });
          }
        });
      }

    },

    renderSubView: function(menu, element) {
      //trigger routers route
      if (window.App[menu.route]) {
        if (window.App[menu.route].resetState) {
          window.App[menu.route].resetState();
        }
        window.App[menu.route]();
      }

      //select active sub view entry
      $(this.subEl + ' .bottom').children().removeClass('active');
      $(element.currentTarget).addClass('active');
    },

    switchTab: function(e) {
      var id = $(e.currentTarget).children().first().attr('id');

      if (id) {
        this.selectMenuItem(id + '-menu');
      }
    },

    /*
    breadcrumb: function (name) {

      if (window.location.hash.split('/')[0] !== '#collection') {
        $('#subNavigationBar .breadcrumb').html(
          '<a class="activeBread" href="#' + name + '">' + name + '</a>'
        );
      }

    },
    */

    selectMenuItem: function (menuItem, noMenuEntry) {

      if (menuItem === undefined) {
        menuItem = window.location.hash.split('/')[0];
        menuItem = menuItem.substr(1, menuItem.length - 1);
      }

      //Location for selecting MainView Primary Navigaation Entry
      if (menuItem === '') {
        if (window.App.isCluster) {
          menuItem = 'cluster';
        }
        else {
          menuItem = 'dashboard';
        }
      }
      else if (menuItem === 'cNodes' || menuItem === 'dNodes') {
        menuItem = 'nodes';
      }
      try {
        this.renderSubMenu(menuItem.split('-')[0]);
      }
      catch (e) {
        this.renderSubMenu(menuItem);
      }

      //this.breadcrumb(menuItem.split('-')[0]);

      $('.navlist li').removeClass('active');
      if (typeof menuItem === 'string') {
        if (noMenuEntry) {
          $('.' + this.subViewConfig[menuItem] + '-menu').addClass('active');
        }
        else if (menuItem) {
          $('.' + menuItem).addClass('active');
          $('.' + menuItem + '-menu').addClass('active');
        }
      }
      arangoHelper.hideArangoNotifications();
    },

    showSubDropdown: function(e) {
      console.log($(e.currentTarget));
      console.log($(e.currentTarget).find('.subBarDropdown'));
      $(e.currentTarget).find('.subBarDropdown').toggle();  
    },

    showDropdown: function (e) {
      var tab = e.target || e.srcElement;
      var navigateTo = tab.id;
      if (navigateTo === "links" || navigateTo === "link_dropdown" || e.currentTarget.id === 'links') {
        $("#link_dropdown").fadeIn(1);
      }
      else if (navigateTo === "tools" || navigateTo === "tools_dropdown" || e.currentTarget.id === 'tools') {
        $("#tools_dropdown").fadeIn(1);
      }
      else if (navigateTo === "dbselection" || navigateTo === "dbs_dropdown" || e.currentTarget.id === 'dbselection') {
        $("#dbs_dropdown").fadeIn(1);
      }
    },

    hideDropdown: function (e) {
      var tab = e.target || e.srcElement;
      tab = $(tab).parent();
      $("#link_dropdown").fadeOut(1);
      $("#tools_dropdown").fadeOut(1);
      $("#dbs_dropdown").fadeOut(1);
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  "use strict";

  window.NodeView = Backbone.View.extend({

    el: '#content',
    template: templateEngine.createTemplate("nodeView.ejs"),
    interval: 5000,
    dashboards: [],

    events: {
    },

    initialize: function (options) {

      if (window.App.isCluster) {
        this.coordinators = options.coordinators;
        this.dbServers = options.dbServers;
        this.coordname = options.coordname;
        this.updateServerTime();

        //start polling with interval
        window.setInterval(function() {
          if (window.location.hash.indexOf('#node/') === 0) {

            var callback = function(data) {
            };

          }
        }, this.interval);
      }
    },

    breadcrumb: function(name) {
      $('#subNavigationBar .breadcrumb').html("Node: " + name);
    },

    render: function () {
      this.$el.html(this.template.render({coords: []}));

      var callback = function() {
        this.continueRender();
        this.breadcrumb(this.coordname);
        //window.arangoHelper.buildNodeSubNav(this.coordname, 'Dashboard', 'Logs');
        $(window).trigger('resize');
      }.bind(this);

      var cb = function() {
        console.log("node complete");
      };

      if (!this.initCoordDone) {
        this.waitForCoordinators(cb);
      }

      if (!this.initDBDone) {
        this.waitForDBServers(callback);
      }
      else {
        this.coordname = window.location.hash.split('/')[1];
        this.coordinator = this.coordinators.findWhere({name: this.coordname});
        callback();
      }

    },

    continueRender: function() {
      var self = this;

      this.dashboards[this.coordinator.get('name')] = new window.DashboardView({
        dygraphConfig: window.dygraphConfig,
        database: window.App.arangoDatabase,
        serverToShow: {
          raw: this.coordinator.get('address'),
          isDBServer: false,
          endpoint: this.coordinator.get('protocol') + "://" + this.coordinator.get('address'),
          target: this.coordinator.get('name')
        }
      });
      this.dashboards[this.coordinator.get('name')].render();
      window.setTimeout(function() {
        self.dashboards[self.coordinator.get('name')].resize();
      }, 500);
    },

    waitForCoordinators: function(callback) {
      var self = this; 

      window.setTimeout(function() {
        if (self.coordinators.length === 0) {
          self.waitForCoordinators(callback);
        }
        else {
          self.coordinator = self.coordinators.findWhere({name: self.coordname});
          self.initCoordDone = true;
          callback();
        }
      }, 200);
    },

    waitForDBServers: function(callback) {
      var self = this; 

      window.setTimeout(function() {
        if (self.dbServers[0].length === 0) {
          self.waitForDBServers(callback);
        }
        else {
          self.initDBDone = true;
          self.dbServer = self.dbServers[0];

          self.dbServer.each(function(model) {
            if (model.get("name") === 'DBServer1') {
              self.dbServer = model;
            }
          });

          callback();
        }
      }, 200);
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, templateEngine, $, window, _, nv, d3 */
(function () {
  "use strict";

  window.NodesView = Backbone.View.extend({

    el: '#content',
    template: templateEngine.createTemplate("nodesView.ejs"),
    interval: 5000,
    knownServers: [],

    events: {
      "click .pure-table-body .pure-table-row" : "navigateToNode"
    },

    initialize: function (options) {

      if (window.App.isCluster) {
        this.dbServers = options.dbServers;
        this.coordinators = options.coordinators;
        this.updateServerTime();
        this.toRender = options.toRender;

        //start polling with interval
        window.setInterval(function() {
          if (window.location.hash === '#cNodes' || window.location.hash === '#dNodes') {

            var callback = function(data) {
            };

          }
        }, this.interval);
      }
    },

    navigateToNode: function(elem) {

      if (window.location.hash === '#dNodes') {
        return;
      }

      var name = $(elem.currentTarget).attr('node');
      window.App.navigate("#node/" + encodeURIComponent(name), {trigger: true});
    },

    render: function () {

      window.arangoHelper.buildNodesSubNav(this.toRender);

      var callback = function() {
        this.continueRender();
      }.bind(this);

      if (!this.initDone) {
        this.waitForCoordinators(callback);
      }
      else {
        callback();
      }
    },

    continueRender: function() {
      var coords;

      if (this.toRender === 'coordinator') {
        coords = this.coordinators.toJSON();
      }
      else {
        coords = this.dbServers.toJSON();
      }

      this.$el.html(this.template.render({
        coords: coords,
        type: this.toRender
      }));
    },

    waitForCoordinators: function(callback) {
      var self = this; 

      window.setTimeout(function() {
        if (self.coordinators.length === 0) {
          self.waitForCoordinators(callback);
        }
        else {
          this.initDone = true;
          callback();
        }
      }, 200);
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, templateEngine, $, window, noty */
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
      $.noty.clearQueue();
      $.noty.closeAll();
      this.collection.reset();
      $('#notification_menu').hide();
    },

    removeNotification: function(e) {
      var cid = e.target.id;
      this.collection.get(cid).destroy();
    },

    renderNotifications: function(a, b, event) {

      if (event) {
        if (event.add) {
          var latestModel = this.collection.at(this.collection.length - 1),
          message = latestModel.get('title'),
          time = 3000,
          closeWidth = ['click'],
          buttons;

          if (latestModel.get('content')) {
            message = message + ": " + latestModel.get('content');
          }

          if (latestModel.get('type') === 'error') {
            time = false;
            closeWidth = ['button'];
            buttons = [{
              addClass: 'button-danger', text: 'Close', onClick: function($noty) {
                $noty.close();
              }
            }];
          }
          $.noty.clearQueue();
          $.noty.closeAll();

          noty({
            theme: 'relax',
            text: message,
            template: 
              '<div class="noty_message arango_message">' + 
              '<div><i class="fa fa-close"></i></div><span class="noty_text arango_text"></span>' + 
              '<div class="noty_close arango_close"></div></div>',
            maxVisible: 1,
            closeWith: ['click'],
            type: latestModel.get('type'),
            layout: 'bottom',
            timeout: time,
            buttons: buttons,
            animation: {
              open: {height: 'show'},
              close: {height: 'hide'},
              easing: 'swing',
              speed: 200,
              closeWith: closeWidth
            }
          });

          if (latestModel.get('type') === 'success') {
            latestModel.destroy();
            return;
          }
        }
      }

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
      $('.notificationInfoIcon').tooltip({
        position: {
          my: "left top",
          at: "right+55 top-1"
        }
      });
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
    lastDelay: 0,

    action: function(){},

    events: {
      "click .progress-action button": "performAction",
    },

    performAction: function() {
      if (typeof this.action === 'function') {
        this.action();
      }
      window.progressView.hide();
    },

    initialize: function() {
    },

    showWithDelay: function(delay, msg, action, button) {
      var self = this;
      self.toShow = true;
      self.lastDelay = delay;

      setTimeout(function() {
        if (self.toShow === true) {
          self.show(msg, action, button);
        }
      }, self.lastDelay);
    },

    show: function(msg, callback, buttonText) {
      $(this.el).html(this.template.render({}));
      $(".progress-text").text(msg);

      if (!buttonText) {
        $(".progress-action").html('<button class="button-danger">Cancel</button>');
      }
      else {
        $(".progress-action").html('<button class="button-danger">' + buttonText + '</button>');
      }

      if (!callback) {
        this.action = this.hide();
      }
      else {
        this.action = callback; 
      }
      //$(".progress-action").html(button);
      //this.action = action;

      $(this.el).show();
      //$(this.el2).html('<i class="fa fa-spinner fa-spin"></i>');
    },

    hide: function() {
      var self = this;
      self.toShow = false;

      $(this.el).hide();

      this.action = function(){};
    }

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _ */
/*global _, arangoHelper, templateEngine, jQuery, Joi*/

(function () {
  "use strict";
  window.queryManagementView = Backbone.View.extend({
    el: '#content',

    id: '#queryManagementContent',

    templateActive: templateEngine.createTemplate("queryManagementViewActive.ejs"),
    templateSlow: templateEngine.createTemplate("queryManagementViewSlow.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),
    active: true,
    shouldRender: true,
    timer: 0,
    refreshRate: 2000,

    initialize: function () {
      var self = this; 
      this.activeCollection = new window.QueryManagementActive();
      this.slowCollection = new window.QueryManagementSlow();
      this.convertModelToJSON(true);

      window.setInterval(function() {
        if (window.location.hash === '#queries' && window.VISIBLE && self.shouldRender 
            && arangoHelper.getCurrentSub().route === 'queryManagement') {
          if (self.active) {
            if ($('#arangoQueryManagementTable').is(':visible')) {
              self.convertModelToJSON(true);
            }
          }
          else {
            if ($('#arangoQueryManagementTable').is(':visible')) {
              self.convertModelToJSON(false);
            }
          }
        }
      }, self.refreshRate);
    },

    events: {
      "click #deleteSlowQueryHistory" : "deleteSlowQueryHistoryModal",
      "click #arangoQueryManagementTable .fa-minus-circle" : "deleteRunningQueryModal"
    },

    tableDescription: {
      id: "arangoQueryManagementTable",
      titles: ["ID", "Query String", "Runtime", "Started", ""],
      rows: [],
      unescaped: [false, false, false, false, true]
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
      var options = arangoHelper.getCurrentSub();
      if (options.params.active) {
        this.active = true;
        this.convertModelToJSON(true);
      }
      else {
        this.active = false;
        this.convertModelToJSON(false);
      }
    },

    addEvents: function() {
      var self = this;
      $('#queryManagementContent tbody').on('mousedown', function() {
        clearTimeout(self.timer);
        self.shouldRender = false;
      });
      $('#queryManagementContent tbody').on('mouseup', function() {
        self.timer = window.setTimeout(function() {
          self.shouldRender = true;
        }, 3000);
      });
    },

    renderActive: function() {
      this.$el.html(this.templateActive.render({}));
      $(this.id).append(this.table.render({content: this.tableDescription}));
      $('#activequeries').addClass("arango-active-tab");
      this.addEvents();
    },

    renderSlow: function() {
      this.$el.html(this.templateSlow.render({}));
      $(this.id).append(this.table.render({
        content: this.tableDescription,
      }));
      $('#slowqueries').addClass("arango-active-tab");
      this.addEvents();
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
/*global Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _, console, btoa*/
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
      'click #submitQueryButton': 'submitQuery',
      'click #explainQueryButton': 'explainQuery',
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
      'click #export-query': 'exportCustomQueries',
      'click #import-query': 'openExportDialog',
      'click #closeQueryModal': 'closeExportDialog',
      'click #downloadQueryResult': 'downloadQueryResult'
    },

    openExportDialog: function() {
      $('#queryImportDialog').modal('show'); 
    },

    closeExportDialog: function() {
      $('#queryImportDialog').modal('hide'); 
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
      window.modalView.show('modalTable.ejs', 'Save Query', buttons, tableContent, undefined, undefined,
        {'keyup #new-query-name' : this.listenKey.bind(this)});
    },

    updateTable: function () {
      this.tableDescription.rows = this.customQueries;

      _.each(this.tableDescription.rows, function(k) {
        k.thirdRow = '<a class="deleteButton"><span class="icon_arangodb_roundminus"' +
                     ' title="Delete query"></span></a>';
        if (k.hasOwnProperty('parameter')) {
          delete k.parameter;
        }
      });

      // escape all columns but the third (which contains HTML)
      this.tableDescription.unescaped = [ false, false, true ];

      this.$(this.id).html(this.table.render({content: this.tableDescription}));
    },

    editCustomQuery: function(e) {
      var queryName = $(e.target).parent().children().first().text(),
      inputEditor = ace.edit("aqlEditor"),
      varsEditor = ace.edit("varsEditor");
      inputEditor.setValue(this.getCustomQueryValueByName(queryName));
      varsEditor.setValue(JSON.stringify(this.getCustomQueryParameterByName(queryName)));
      this.deselect(varsEditor);
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
      var inputEditor = ace.edit("aqlEditor"),
      varsEditor = ace.edit("varsEditor");
      this.setCachedQuery(inputEditor.getValue(), varsEditor.getValue());
      inputEditor.setValue('');
      varsEditor.setValue('');
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
      [ 100, 250, 500, 1000, 2500, 5000, 10000, "all" ].forEach(function (value) {
        sizeBox.append('<option value="' + _.escape(value) + '"' +
          (querySize === value ? ' selected' : '') +
          '>' + _.escape(value) + ' results</option>');
      });

      var outputEditor = ace.edit("queryOutput");
      outputEditor.setReadOnly(true);
      outputEditor.setHighlightActiveLine(false);
      outputEditor.getSession().setMode("ace/mode/json");
      outputEditor.setFontSize("13px");
      outputEditor.setValue('');

      var inputEditor = ace.edit("aqlEditor");
      inputEditor.getSession().setMode("ace/mode/aql");
      inputEditor.setFontSize("13px");
      inputEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      var varsEditor = ace.edit("varsEditor");
      varsEditor.getSession().setMode("ace/mode/aql");
      varsEditor.setFontSize("13px");
      varsEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      //get cached query if available
      var queryObject = this.getCachedQuery();
      if (queryObject !== null && queryObject !== undefined && queryObject !== "") {
        inputEditor.setValue(queryObject.query);
        if (queryObject.parameter === '' || queryObject === undefined) {
          varsEditor.setValue('{}');
        }
        else {
          varsEditor.setValue(queryObject.parameter);
        }
      }

      var changedFunction = function() {
        var session = inputEditor.getSession(),
        cursor = inputEditor.getCursorPosition(),
        token = session.getTokenAt(cursor.row, cursor.column);
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
        var a = inputEditor.getValue(),
        b = varsEditor.getValue();

        if (a.length === 1) {
          a = "";
        }
        if (b.length === 1) {
          b = "";
        }

        self.setCachedQuery(a, b);
      };

      inputEditor.getSession().selection.on('changeCursor', function () {
        changedFunction();
      });

      varsEditor.getSession().selection.on('changeCursor', function () {
        changedFunction();
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

      arangoHelper.fixTooltips(".vars-editor-header i, .queryTooltips, .icon_arangodb", "top");

      $('#aqlEditor .ace_text-input').focus();

      var windowHeight = $(window).height() - 295;
      $('#aqlEditor').height(windowHeight - 100 - 29);
      $('#varsEditor').height(100);
      $('#queryOutput').height(windowHeight);

      inputEditor.resize();
      outputEditor.resize();

      this.initTabArray();
      this.renderSelectboxes();
      this.deselect(varsEditor);
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
      if (Storage !== "undefined") {
        var cache = localStorage.getItem("cachedQuery");
        if (cache !== undefined) {
          var query = JSON.parse(cache);
          return query;
        }
      }
    },

    setCachedQuery: function(query, vars) {
      if (Storage !== "undefined") {
        var myObject = {
          query: query,
          parameter: vars
        };
        localStorage.setItem("cachedQuery", JSON.stringify(myObject));
      }
    },

    initQueryImport: function () {
      var self = this;
      self.allowUpload = false;
      $('#importQueries').change(function(e) {
        self.files = e.target.files || e.dataTransfer.files;
        self.file = self.files[0];

        self.allowUpload = true;
        $('#confirmQueryImport').removeClass('disabled');
      });
    },

    importCustomQueries: function () {
      var self = this;
      if (this.allowUpload === true) {

        var callback = function() {
          this.collection.fetch({
            success: function() {
              self.updateLocalQueries();
              self.renderSelectboxes();
              self.updateTable();
              self.allowUpload = false;
              $('#customs-switch').click();
              $('#confirmQueryImport').addClass('disabled');
              $('#queryImportDialog').modal('hide'); 
            },
            error: function(data) {
              arangoHelper.arangoError("Custom Queries", data.responseText);
            }
          });
        }.bind(this);

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
      _.each(this.customQueries, function(value) {
        exportArray.push({name: value.name, value: value.value, parameter: value.parameter});
      });
      toExport = {
        "extra": {
          "queries": exportArray
        }
      };

      $.ajax("whoAmI?_=" + Date.now()).success(
        function(data) {
          name = data.user;

          if (name === null || name === false) {
            name = "root";
          }
          window.open("query/download/" + encodeURIComponent(name));
        });

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

      getAQL: function (originCallback) {
        var self = this, result;

        this.collection.fetch({
          success: function() {
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

              var callback = function(error, data) {
                if (error) {
                  arangoHelper.arangoError(
                    "Custom Queries",
                    "Could not import old local storage queries"
                  );
                }
                else {
                  localStorage.removeItem("customQueries");
                }
              }.bind(self);
              self.collection.saveCollectionQueries(callback);
            }
            self.updateLocalQueries();

            if (originCallback) {
              originCallback();
            }
          }
        });
      },

      deleteAQL: function (e) {
        var callbackSave = function(error) {
          if (error) {
            arangoHelper.arangoError("Query", "Could not delete query.");
          }
          else {
            this.updateLocalQueries();
            this.renderSelectboxes();
            this.updateTable();
          }
        }.bind(this);

        var deleteName = $(e.target).parent().parent().parent().children().first().text();
        var toDelete = this.collection.findWhere({name: deleteName});

        this.collection.remove(toDelete);
        this.collection.saveCollectionQueries(callbackSave);

      },

      updateLocalQueries: function () {
        var self = this;
        this.customQueries = [];

        this.collection.each(function(model) {
          self.customQueries.push({
            name: model.get("name"),
            value: model.get("value"),
            parameter: model.get("parameter")
          });
        });
      },

      saveAQL: function (e) {
        e.stopPropagation();

        //update queries first, before writing
        this.refreshAQL();

        var inputEditor = ace.edit("aqlEditor"),
        varsEditor = ace.edit("varsEditor"),
        saveName = $('#new-query-name').val(),
        bindVars = varsEditor.getValue();

        if ($('#new-query-name').hasClass('invalid-input')) {
          return;
        }

        //Heiko: Form-Validator - illegal query name
        if (saveName.trim() === '') {
          return;
        }

        var content = inputEditor.getValue(),
        //check for already existing entry
        quit = false;
        $.each(this.customQueries, function (k, v) {
          if (v.name === saveName) {
            v.value = content;
            quit = true;
            return;
          }
        });

        if (quit === true) {
          //Heiko: Form-Validator - name already taken
          // Update model and save
          this.collection.findWhere({name: saveName}).set("value", content);
        }
        else {
          if (bindVars === '' || bindVars === undefined) {
            bindVars = '{}';
          }

          if (typeof bindVars === 'string') {
            try {
              bindVars = JSON.parse(bindVars);
            }
            catch (err) {
              console.log("could not parse bind parameter");
            }
          }
          this.collection.add({
            name: saveName,
            parameter: bindVars, 
            value: content
          });
        }

        var callback = function(error) {
          if (error) {
            arangoHelper.arangoError("Query", "Could not save query");
          }
          else {
            var self = this;
            this.collection.fetch({
              success: function() {
                self.updateLocalQueries();
                self.renderSelectboxes();
                $('#querySelect').val(saveName);
              }
            });
          }
        }.bind(this);
        this.collection.saveCollectionQueries(callback);
        window.modalView.hide();
      },

      getSystemQueries: function (callback) {
        var self = this;
        $.ajax({
          type: "GET",
          cache: false,
          url: "js/arango/aqltemplates.json",
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (callback) {
              callback(false);
            }
            self.queries = data;
          },
          error: function () {
            if (callback) {
              callback(true);
            }
            arangoHelper.arangoNotification("Query", "Error while loading system templates");
          }
        });
      },

      getCustomQueryValueByName: function (qName) {
        return this.collection.findWhere({name: qName}).get("value");
      },

      getCustomQueryParameterByName: function (qName) {
        return this.collection.findWhere({name: qName}).get("parameter");
      },

      refreshAQL: function(select) {

        var self = this;

        var callback = function(error) {
          if (error) {
            arangoHelper.arangoError('Query', 'Could not reload Queries');
          }
          else {
            self.updateLocalQueries();
            if (select) {
              var previous = $("#querySelect").val();
              self.renderSelectboxes();
              $("#querySelect").val(previous);
            }
          }
        }.bind(self);

        var originCallback = function() {
          self.getSystemQueries(callback);
        }.bind(self);

        this.getAQL(originCallback);
      },

      importSelected: function (e) {
        var inputEditor = ace.edit("aqlEditor"),
        varsEditor = ace.edit("varsEditor");
        _.each(this.queries, function (v) {
          if ($('#' + e.currentTarget.id).val() === v.name) {
            inputEditor.setValue(v.value);

            if (v.hasOwnProperty('parameter')) {
              if (v.parameter === '' || v.parameter === undefined) {
                v.parameter = '{}';
              }
              if (typeof v.parameter === 'object') {
                varsEditor.setValue(JSON.stringify(v.parameter));
              }
              else {
                varsEditor.setValue(v.parameter);
              }
            }
            else {
              varsEditor.setValue('{}');
            }
          }
        });
        _.each(this.customQueries, function (v) {
          if ($('#' + e.currentTarget.id).val() === v.name) {
            inputEditor.setValue(v.value);

            if (v.hasOwnProperty('parameter')) {
              if (v.parameter === '' ||
                  v.parameter === undefined || 
                  JSON.stringify(v.parameter) === '{}') 
              {
                v.parameter = '{}';
              }
              varsEditor.setValue(v.parameter);
            }
            else {
              varsEditor.setValue('{}');
            }
          }
        });
        this.deselect(ace.edit("varsEditor"));
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

      readQueryData: function() {
        var inputEditor = ace.edit("aqlEditor");
        var varsEditor = ace.edit("varsEditor");
        var selectedText = inputEditor.session.getTextRange(inputEditor.getSelectionRange());
        var sizeBox = $('#querySize');
        var data = {
          query: selectedText || inputEditor.getValue(),
          id: "currentFrontendQuery"
        };

        if (sizeBox.val() !== 'all') {
          data.batchSize = parseInt(sizeBox.val(), 10);
        }

        var bindVars = varsEditor.getValue();

        if (bindVars.length > 0) {
          try {
            var params = JSON.parse(bindVars);
            if (Object.keys(params).length !== 0) {
              data.bindVars = params;
            }
          }
          catch (e) {
            arangoHelper.arangoError("Query error", "Could not parse bind parameters.");
            return false;
          }
        }
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
        console.log(estCost);
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

      fillExplain: function(callback) {

        var self = this;
        var outputEditor = ace.edit("queryOutput");
        var queryData = this.readQueryData();
        $('.queryExecutionTime').text('');
        this.execPending = false;

        if (queryData) {
          window.progressView.show(
            "Explain is operating..."
          );

          $.ajax({
            type: "POST",
            url: "/_admin/aardvark/query/explain/",
            data: queryData,
            contentType: "application/json",
            processData: false,
            success: function (data) {
              outputEditor.setValue(data.msg);
              self.switchTab("result-switch");
              window.progressView.hide();
              self.deselect(outputEditor);
              $('#downloadQueryResult').hide();
              if (typeof callback === "function") {
                callback();
              }
              $.noty.clearQueue();
              $.noty.closeAll();
            },
            error: function (data) {
              window.progressView.hide();
              try {
                var temp = JSON.parse(data.responseText);
                arangoHelper.arangoError("Explain error", temp.errorNum);
              }
              catch (e) {
                arangoHelper.arangoError("Explain error", "ERROR");
              }
              window.progressView.hide();
              if (typeof callback === "function") {
                callback();
              }
            }
          });
        }
      /*
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
          }
        });
      */
      },

      resize: function() {
        // this.drawTree();
      },

      checkQueryTimer: undefined,

      queryCallbackFunction: function(queryID, callback) {

        var self = this;
        var outputEditor = ace.edit("queryOutput");

        var cancelRunningQuery = function() {

          $.ajax({
            url: '/_api/job/'+ encodeURIComponent(queryID) + "/cancel",
            type: 'PUT',
            success: function() {
              window.clearTimeout(self.checkQueryTimer);
              arangoHelper.arangoNotification("Query", "Query canceled.");
              window.progressView.hide();
            }
          });
        };

        window.progressView.show(
          "Query is operating...",
          cancelRunningQuery,
          "Cancel Query"
        );

        $('.queryExecutionTime').text('');
        this.execPending = false;

        var warningsFunc = function(data) {
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
        };

        var fetchQueryResult = function(data) {
          warningsFunc(data);
          self.switchTab("result-switch");
          window.progressView.hide();
          
          var time = "-";
          if (data && data.extra && data.extra.stats) {
            time = data.extra.stats.executionTime.toFixed(3) + " s";
          }

          $('.queryExecutionTime').text("Execution time: " + time);

          self.deselect(outputEditor);
          $('#downloadQueryResult').show();

          if (typeof callback === "function") {
            callback();
          }
        };

        //check if async query is finished
        var checkQueryStatus = function() {
          $.ajax({
            type: "PUT",
            url: "/_api/job/" + encodeURIComponent(queryID),
            contentType: "application/json",
            processData: false,
            success: function (data, textStatus, xhr) {

              //query finished, now fetch results
              if (xhr.status === 201) {
                fetchQueryResult(data);
              }
              //query not ready yet, retry
              else if (xhr.status === 204) {
                self.checkQueryTimer = window.setTimeout(function() {
                  checkQueryStatus(); 
                }, 500);
              }
            },
            error: function (resp) {
              try {
                var error = JSON.parse(resp.responseText);
                if (error.errorMessage) {
                  arangoHelper.arangoError("Query", error.errorMessage);
                }
              }
              catch (e) {
                arangoHelper.arangoError("Query", "Something went wrong.");
              }

              window.progressView.hide();
            }
          });
        };
        checkQueryStatus();
      },

      fillResult: function(callback) {
        var self = this;
        var outputEditor = ace.edit("queryOutput");
        // clear result
        outputEditor.setValue('');

        var queryData = this.readQueryData();
        if (queryData) {
          $.ajax({
            type: "POST",
            url: "/_api/cursor",
            headers: {
              'x-arango-async': 'store' 
            },
            data: queryData,
            contentType: "application/json",
            processData: false,
            success: function (data, textStatus, xhr) {
              if (xhr.getResponseHeader('x-arango-async-id')) {
                self.queryCallbackFunction(xhr.getResponseHeader('x-arango-async-id'), callback);
              }
              $.noty.clearQueue();
              $.noty.closeAll();
            },
            error: function (data) {
              self.switchTab("result-switch");
              $('#downloadQueryResult').hide();
              try {
                var temp = JSON.parse(data.responseText);
                outputEditor.setValue('[' + temp.errorNum + '] ' + temp.errorMessage);
                //arangoHelper.arangoError("Query error", temp.errorMessage);
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
        }
      },

      submitQuery: function () {
        var outputEditor = ace.edit("queryOutput");
        this.fillResult(this.switchTab.bind(this, "result-switch"));
        outputEditor.resize();
        var inputEditor = ace.edit("aqlEditor");
        this.deselect(inputEditor);
        $('#downloadQueryResult').show();
      },

      explainQuery: function() {
        this.fillExplain();
      },

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

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, setTimeout, localStorage, ace, Storage, window, _, console, btoa*/
/*global _, arangoHelper, templateEngine, jQuery, Joi, d3*/

(function () {
  "use strict";
  window.queryView2 = Backbone.View.extend({
    el: '#content',
    bindParamId: '#bindParamEditor',
    myQueriesId: '#queryTable',
    template: templateEngine.createTemplate("queryView2.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),

    outputDiv: '#outputEditors',
    outputTemplate: templateEngine.createTemplate("queryViewOutput.ejs"),
    outputCounter: 0,

    allowUpload: false,

    customQueries: [],
    queries: [],

    state: {
      lastQuery: {
        query: undefined,
        bindParam: undefined
      }
    },

    settings: {
      aqlWidth: undefined
    },

    currentQuery: {},
    initDone: false,

    bindParamRegExp: /@(@?\w+\d*)/,
    bindParamTableObj: {},

    bindParamTableDesc: {
      id: "arangoBindParamTable",
      titles: ["Key", "Value"],
      rows: []
    },

    myQueriesTableDesc: {
      id: "arangoMyQueriesTable",
      titles: ["Name", "Actions"],
      rows: []
    },

    execPending: false,

    aqlEditor: null,
    queryPreview: null,

    initialize: function () {
      this.refreshAQL();
    },

    allowParamToggle: true,

    events: {
      "click #executeQuery": "executeQuery",
      "click #explainQuery": "explainQuery",
      "click #clearQuery": "clearQuery",
      'click .outputEditorWrapper #downloadQueryResult': 'downloadQueryResult',
      'click .outputEditorWrapper .switchAce': 'switchAce',
      "click .outputEditorWrapper .fa-close": "closeResult",
      "click #toggleQueries1": "toggleQueries",
      "click #toggleQueries2": "toggleQueries",
      "click #saveCurrentQuery": "addAQL",
      "click #exportQuery": "exportCustomQueries",
      "click #importQuery": "openImportDialog",
      "click #removeResults": "removeResults",
      "click #querySpotlight": "showSpotlight",
      "click #deleteQuery": "selectAndDeleteQueryFromTable",
      "click #explQuery": "selectAndExplainQueryFromTable",
      "keydown #arangoBindParamTable input": "updateBindParams",
      "change #arangoBindParamTable input": "updateBindParams",
      "click #arangoMyQueriesTable tbody tr" : "showQueryPreview",
      "dblclick #arangoMyQueriesTable tbody tr" : "selectQueryFromTable",
      "click #arangoMyQueriesTable #copyQuery" : "selectQueryFromTable",
      'click #closeQueryModal': 'closeExportDialog',
      'click #confirmQueryImport': 'importCustomQueries',
      'click #switchTypes': 'toggleBindParams',
      "click #arangoMyQueriesTable #runQuery" : "selectAndRunQueryFromTable"
    },

    clearQuery: function() {
      this.aqlEditor.setValue('', 1);
    },

    toggleBindParams: function() {

      if (this.allowParamToggle) {
        $('#bindParamEditor').toggle();
        $('#bindParamAceEditor').toggle();

        if ($('#switchTypes').text() === 'JSON') {
          $('#switchTypes').text('Table');
          this.updateQueryTable();
          this.bindParamAceEditor.setValue(JSON.stringify(this.bindParamTableObj, null, "\t"), 1);
          this.deselect(this.bindParamAceEditor);
        }
        else {
          $('#switchTypes').text('JSON');
          this.renderBindParamTable();
        }
      }
      else {
        arangoHelper.arangoError("Bind parameter", "Could not parse bind parameter");
      }
      this.resize();
    },

    openExportDialog: function() {
      $('#queryImportDialog').modal('show'); 
    },

    closeExportDialog: function() {
      $('#queryImportDialog').modal('hide'); 
    },

    initQueryImport: function () {
      var self = this;
      self.allowUpload = false;
      $('#importQueries').change(function(e) {
        self.files = e.target.files || e.dataTransfer.files;
        self.file = self.files[0];

        self.allowUpload = true;
        $('#confirmQueryImport').removeClass('disabled');
      });
    },

    importCustomQueries: function () {
      var self = this;
      if (this.allowUpload === true) {

        var callback = function() {
          this.collection.fetch({
            success: function() {
              self.updateLocalQueries();
              self.updateQueryTable();
              self.resize();
              self.allowUpload = false;
              $('#confirmQueryImport').addClass('disabled');
              $('#queryImportDialog').modal('hide'); 
            },
            error: function(data) {
              arangoHelper.arangoError("Custom Queries", data.responseText);
            }
          });
        }.bind(this);

        self.collection.saveImportQueries(self.file, callback.bind(this));
      }
    },

    removeResults: function() {
      $('.outputEditorWrapper').hide('fast', function() {
        $('.outputEditorWrapper').remove();
      });
      $('#removeResults').hide();
    },

    getCustomQueryParameterByName: function (qName) {
      return this.collection.findWhere({name: qName}).get("parameter");
    },

    getCustomQueryValueByName: function (qName) {
      var obj;

      if (qName) {
        obj = this.collection.findWhere({name: qName});
      }
      if (obj) {
        obj = obj.get("value");
      }
      else {
        _.each(this.queries, function(query) {
          if (query.name === qName) {
            obj = query.value;
          }
        });
      }
      return obj;
    },

    openImportDialog: function() {
      $('#queryImportDialog').modal('show'); 
    },

    closeImportDialog: function() {
      $('#queryImportDialog').modal('hide'); 
    },

    exportCustomQueries: function () {
      var name;

      $.ajax("whoAmI?_=" + Date.now()).success(function(data) {
        name = data.user;

        if (name === null || name === false) {
          name = "root";
        }
        window.open("query/download/" + encodeURIComponent(name));
      });
    },

    toggleQueries: function(e) {

      if (e) {
        if (e.currentTarget.id === "toggleQueries1") {
          this.updateQueryTable();
          $('#bindParamAceEditor').hide();
          $('#bindParamEditor').show();
          $('#switchTypes').text('JSON');
          $('.aqlEditorWrapper').first().width($(window).width() * 0.33);
          this.queryPreview.setValue("No query selected.", 1);
          this.deselect(this.queryPreview);
        }
        else {
          if (this.settings.aqlWidth === undefined) {
            $('.aqlEditorWrapper').first().width($(window).width() * 0.33);
          }
          else {
            $('.aqlEditorWrapper').first().width(this.settings.aqlWidth);
          }
        }
      }
      else {
        if (this.settings.aqlWidth === undefined) {
          $('.aqlEditorWrapper').first().width($(window).width() * 0.33);
        }
        else {
          $('.aqlEditorWrapper').first().width(this.settings.aqlWidth);
        }
      }
      this.resize();

      var divs = [
        "aqlEditor", "queryTable", "previewWrapper", "querySpotlight",
        "bindParamEditor", "toggleQueries1", "toggleQueries2",
        "saveCurrentQuery", "querySize", "executeQuery", "switchTypes", 
        "explainQuery", "importQuery", "exportQuery"
      ];
      _.each(divs, function(div) {
        $("#" + div).toggle();
      });
      this.resize();
    },

    showQueryPreview: function(e) {
      $('#arangoMyQueriesTable tr').removeClass('selected');
      $(e.currentTarget).addClass('selected');

      var name = this.getQueryNameFromTable(e);
      this.queryPreview.setValue(this.getCustomQueryValueByName(name), 1);
      this.deselect(this.queryPreview);
    },

    getQueryNameFromTable: function(e) {
      var name; 
      if ($(e.currentTarget).is('tr')) {
        name = $(e.currentTarget).children().first().text();
      }
      else if ($(e.currentTarget).is('span')) {
        name = $(e.currentTarget).parent().parent().prev().text();
      }
      return name;
    },

    deleteQueryModal: function(name){
      var buttons = [], tableContent = [];
      tableContent.push(
        window.modalView.createReadOnlyEntry(
          undefined,
          name,
          'Do you want to delete the query?',
          undefined,
          undefined,
          false,
          undefined
        )
      );
      buttons.push(
        window.modalView.createDeleteButton('Delete', this.deleteAQL.bind(this, name))
      );
      window.modalView.show(
        'modalTable.ejs', 'Delete Query', buttons, tableContent
      );
    },

    selectAndDeleteQueryFromTable: function(e) {
      var name = this.getQueryNameFromTable(e);
      this.deleteQueryModal(name);
    },

    selectAndExplainQueryFromTable: function(e) {
      this.selectQueryFromTable(e, false);
      this.explainQuery();
    },

    selectAndRunQueryFromTable: function(e) {
      this.selectQueryFromTable(e, false);
      this.executeQuery();
    },

    selectQueryFromTable: function(e, toggle) {
      var name = this.getQueryNameFromTable(e),
      self = this;

      if (toggle === undefined) {
        this.toggleQueries();
      }

      //backup the last query
      this.state.lastQuery.query = this.aqlEditor.getValue();
      this.state.lastQuery.bindParam = this.bindParamTableObj;

      this.aqlEditor.setValue(this.getCustomQueryValueByName(name), 1);
      this.fillBindParamTable(this.getCustomQueryParameterByName(name));
      this.updateBindParams();

      //render a button to revert back to last query
      $('#lastQuery').remove();
      $('#queryContent .arangoToolbarTop .pull-left')
        .append('<span id="lastQuery" class="clickable">Previous Query</span>');

        $('#lastQuery').hide().fadeIn(500)
        .on('click', function() {
          self.aqlEditor.setValue(self.state.lastQuery.query, 1);
          self.fillBindParamTable(self.state.lastQuery.bindParam);
          self.updateBindParams();

          $('#lastQuery').fadeOut(500, function () {
            $(this).remove();
          });
        }
      );
    },

    deleteAQL: function (name) {
      var callbackRemove = function(error) {
        if (error) {
          arangoHelper.arangoError("Query", "Could not delete query.");
        }
        else {
          this.updateLocalQueries();
          this.updateQueryTable();
          this.resize();
          window.modalView.hide();
        }
      }.bind(this);

      var toDelete = this.collection.findWhere({name: name});

      this.collection.remove(toDelete);
      this.collection.saveCollectionQueries(callbackRemove);
    },

    switchAce: function(e) {
      var count = $(e.currentTarget).attr('counter');

      if ($(e.currentTarget).text() === 'Result') {
        $(e.currentTarget).text('AQL');
      }
      else {
        $(e.currentTarget).text('Result');
      }
      $('#outputEditor' + count).toggle();
      $('#sentWrapper' + count).toggle();
      this.deselect(ace.edit("outputEditor" + count));
      this.deselect(ace.edit("sentQueryEditor" + count));
      this.deselect(ace.edit("sentBindParamEditor" + count));
    },

    downloadQueryResult: function(e) {
      var count = $(e.currentTarget).attr('counter'),
      editor = ace.edit("sentQueryEditor" + count),
      query = editor.getValue();

      if (query !== '' || query !== undefined || query !== null) {
        if (Object.keys(this.bindParamTableObj).length === 0) {
          window.open("query/result/download/" + encodeURIComponent(btoa(JSON.stringify({ query: query }))));
        }
        else {
          window.open("query/result/download/" + encodeURIComponent(btoa(JSON.stringify({
            query: query,
            bindVars: this.bindParamTableObj
          }))));
        }
      }
      else {
        arangoHelper.arangoError("Query error", "could not query result.");
      }
    },

    explainQuery: function() {

      if (this.verifyQueryAndParams()) {
        return;
      }

      this.$(this.outputDiv).prepend(this.outputTemplate.render({
        counter: this.outputCounter,
        type: "Explain"
      }));

      var counter = this.outputCounter,
      outputEditor = ace.edit("outputEditor" + counter),
      sentQueryEditor = ace.edit("sentQueryEditor" + counter),
      sentBindParamEditor = ace.edit("sentBindParamEditor" + counter);

      sentQueryEditor.getSession().setMode("ace/mode/aql");
      sentQueryEditor.setOption("vScrollBarAlwaysVisible", true);
      sentQueryEditor.setReadOnly(true);
      this.setEditorAutoHeight(sentQueryEditor);

      outputEditor.setReadOnly(true);
      outputEditor.getSession().setMode("ace/mode/json");
      outputEditor.setOption("vScrollBarAlwaysVisible", true);
      this.setEditorAutoHeight(outputEditor);

      sentBindParamEditor.setValue(JSON.stringify(this.bindParamTableObj), 1);
      sentBindParamEditor.setOption("vScrollBarAlwaysVisible", true);
      sentBindParamEditor.getSession().setMode("ace/mode/json");
      sentBindParamEditor.setReadOnly(true);
      this.setEditorAutoHeight(sentBindParamEditor);

      this.fillExplain(outputEditor, sentQueryEditor, counter);
      this.outputCounter++;
    },

    fillExplain: function(outputEditor, sentQueryEditor, counter) {
      sentQueryEditor.setValue(this.aqlEditor.getValue(), 1);

      var self = this,
      queryData = this.readQueryData();
      $('#outputEditorWrapper' + counter + ' .queryExecutionTime').text('');
      this.execPending = false;

      if (queryData) {

        var afterResult = function() {
          $('#outputEditorWrapper' + counter + ' #spinner').remove();
          $('#outputEditor' + counter).css('opacity', '1');
          $('#outputEditorWrapper' + counter + ' .fa-close').show();
          $('#outputEditorWrapper' + counter + ' .switchAce').show();
        };

        $.ajax({
          type: "POST",
          url: "/_admin/aardvark/query/explain/",
          data: queryData,
          contentType: "application/json",
          processData: false,
          success: function (data) {
            if (data.msg.includes('errorMessage')) {
              self.removeOutputEditor(counter);
              arangoHelper.arangoError("Explain", data.msg);
            }
            else {
              outputEditor.setValue(data.msg, 1);
              self.deselect(outputEditor);
              $.noty.clearQueue();
              $.noty.closeAll();
              self.handleResult(counter);
            }
            afterResult();
          },
          error: function (data) {
            try {
              var temp = JSON.parse(data.responseText);
              arangoHelper.arangoError("Explain", temp.errorMessage);
            }
            catch (e) {
              arangoHelper.arangoError("Explain", "ERROR");
            }
            self.handleResult(counter);
            self.removeOutputEditor(counter);
            afterResult();
          }
        });
      }
    },

    removeOutputEditor: function(counter) {
      $('#outputEditorWrapper' + counter).hide();
      $('#outputEditorWrapper' + counter).remove();
      if ($('.outputEditorWrapper').length === 0) {
        $('#removeResults').hide(); 
      }
    },

    getCachedQueryAfterRender: function() {
      //get cached query if available
      var queryObject = this.getCachedQuery(),
      self = this;

      if (queryObject !== null && queryObject !== undefined && queryObject !== "") {
        this.aqlEditor.setValue(queryObject.query, 1);

        //reset undo history for initial text value
        this.aqlEditor.getSession().setUndoManager(new ace.UndoManager());

        if (queryObject.parameter !== '' || queryObject !== undefined) {
          try {
            // then fill values into input boxes
            self.bindParamTableObj = JSON.parse(queryObject.parameter);

            var key;
            _.each($('#arangoBindParamTable input'), function(element) {
              key = $(element).attr('name');
              $(element).val(self.bindParamTableObj[key]);
            });

            //resave cached query
            self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));
          }
          catch (ignore) {
          }
        }
      }
    },

    getCachedQuery: function() {
      if (Storage !== "undefined") {
        var cache = localStorage.getItem("cachedQuery");
        if (cache !== undefined) {
          var query = JSON.parse(cache);
          this.currentQuery = query;
          try {
            this.bindParamTableObj = JSON.parse(query.parameter);
          }
          catch (ignore) {
          }
          return query;
        }
      }
    },

    setCachedQuery: function(query, vars) {
      if (Storage !== "undefined") {
        var myObject = {
          query: query,
          parameter: vars
        };
        this.currentQuery = myObject;
        localStorage.setItem("cachedQuery", JSON.stringify(myObject));
      }
    },

    closeResult: function(e) {
      var target = $("#" + $(e.currentTarget).attr('element')).parent();
      $(target).hide('fast', function() {
        $(target).remove();
        if ($('.outputEditorWrapper').length === 0) {
          $('#removeResults').hide();
        }
      });
    },

    fillSelectBoxes: function() {
      // fill select box with # of results
      var querySize = 1000,
      sizeBox = $('#querySize');
      sizeBox.empty();

      [ 100, 250, 500, 1000, 2500, 5000, 10000, "all" ].forEach(function (value) {
        sizeBox.append('<option value="' + _.escape(value) + '"' +
          (querySize === value ? ' selected' : '') +
          '>' + _.escape(value) + ' results</option>');
      });
    },

    render: function() {
      this.$el.html(this.template.render({}));

      this.afterRender();

      if (!this.initDone) {
        //init aql editor width
        this.settings.aqlWidth = $('.aqlEditorWrapper').width(); 
      }

      this.initDone = true;
      this.renderBindParamTable(true);
    },

    afterRender: function() {
      var self = this;
      this.initAce();
      this.initTables();
      this.fillSelectBoxes();
      this.makeResizeable();
      this.initQueryImport();
      this.getCachedQueryAfterRender();

      //set height of editor wrapper
      $('.inputEditorWrapper').height($(window).height() / 10 * 5 + 25);
      window.setTimeout(function() {
        self.resize();
      }, 10);
      self.deselect(self.aqlEditor);
    },

    showSpotlight: function(type) {
      
      var callback, cancelCallback;

      if (type === undefined || type.type === 'click') {
        type = 'aql';
      }

      if (type === 'aql') {
        callback = function(string) {
          this.aqlEditor.insert(string);
          $('#aqlEditor .ace_text-input').focus();
        }.bind(this);

        cancelCallback = function() {
          $('#aqlEditor .ace_text-input').focus();
        };
      }
      else {
        var focused = $(':focus');
        callback = function(string) {
          var old = $(focused).val();
          $(focused).val(old + string);
          $(focused).focus();
        }.bind(this);

        cancelCallback = function() {
          $(focused).focus();
        };
      }

      window.spotlightView.show(callback, cancelCallback, type);
    },

    resize: function() {
      this.resizeFunction();
    },

    resizeFunction: function() {
      if ($('#toggleQueries1').is(':visible')) {
        this.aqlEditor.resize();
        $('#arangoBindParamTable thead').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable thead th').css('width', $('#bindParamEditor').width() / 2);
        $('#arangoBindParamTable tr').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable tbody').css('height', $('#aqlEditor').height() - 35);
        $('#arangoBindParamTable tbody').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable tbody tr').css('width', $('#bindParamEditor').width());
        $('#arangoBindParamTable tbody td').css('width', $('#bindParamEditor').width() / 2);
      }
      else {
        this.queryPreview.resize();
        $('#arangoMyQueriesTable thead').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable thead th').css('width', $('#queryTable').width() / 2);
        $('#arangoMyQueriesTable tr').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable tbody').css('height', $('#queryTable').height() - 35);
        $('#arangoMyQueriesTable tbody').css('width', $('#queryTable').width());
        $('#arangoMyQueriesTable tbody td').css('width', $('#queryTable').width() / 2);
      }
    },

    makeResizeable: function() {
      var self = this;

      $(".aqlEditorWrapper").resizable({
        resize: function() {
          self.resizeFunction();
          self.settings.aqlWidth = $('.aqlEditorWrapper').width();
        },
        handles: "e"
      });

      $(".inputEditorWrapper").resizable({
        resize: function() {
          self.resizeFunction();
        },
        handles: "s"
      });

      //one manual start
      this.resizeFunction();
    },

    initTables: function() {
       this.$(this.bindParamId).html(this.table.render({content: this.bindParamTableDesc}));
       this.$(this.myQueriesId).html(this.table.render({content: this.myQueriesTableDesc}));
    },

    checkType: function(val) {
      var type = "stringtype";

      try {
        val = JSON.parse(val);
        if (val instanceof Array) {
          type = 'arraytype';
        }
        else {
          type = typeof val + 'type';
        }
      }
      catch(ignore) {
      }

      return type;
    },

    updateBindParams: function(e) {

      var id, self = this;

      if (e) {
        id = $(e.currentTarget).attr("name");
        //this.bindParamTableObj[id] = $(e.currentTarget).val();
        this.bindParamTableObj[id] = arangoHelper.parseInput(e.currentTarget);

        var types = [
          "arraytype", "objecttype", "booleantype", "numbertype", "stringtype"
        ];
        _.each(types, function(type) {
          $(e.currentTarget).removeClass(type);
        });
        $(e.currentTarget).addClass(self.checkType($(e.currentTarget).val()));
      }
      else {
        _.each($('#arangoBindParamTable input'), function(element) {
          id = $(element).attr("name");
          self.bindParamTableObj[id] = arangoHelper.parseInput(element);
        });
      }
      this.setCachedQuery(this.aqlEditor.getValue(), JSON.stringify(this.bindParamTableObj));

      //fire execute if return was pressed
      if (e) {
        if ((e.ctrlKey || e.metaKey) && e.keyCode === 13) {
          e.preventDefault();
          this.executeQuery();
        }
        if ((e.ctrlKey || e.metaKey) && e.keyCode === 32) {
          e.preventDefault();
          this.showSpotlight('bind');
        }
      }
    },

    parseQuery: function (query) {

      var STATE_NORMAL = 0,
      STATE_STRING_SINGLE = 1,
      STATE_STRING_DOUBLE = 2,
      STATE_STRING_TICK = 3,
      STATE_COMMENT_SINGLE = 4,
      STATE_COMMENT_MULTI = 5,
      STATE_BIND = 6,
      STATE_STRING_BACKTICK = 7;

      query += " ";
      var self = this;
      var start;
      var state = STATE_NORMAL;
      var n = query.length;
      var i, c;

      var bindParams = [];

      for (i = 0; i < n; ++i) {
        c = query.charAt(i);

        switch (state) {
          case STATE_NORMAL: 
            if (c === '@') {
              state = STATE_BIND;
              start = i;
            } else if (c === '\'') {
              state = STATE_STRING_SINGLE;
            } else if (c === '"') {
              state = STATE_STRING_DOUBLE;
            } else if (c === '`') {
              state = STATE_STRING_TICK;
            } else if (c === '´') {
              state = STATE_STRING_BACKTICK;
            } else if (c === '/') {
              if (i + 1 < n) {
                if (query.charAt(i + 1) === '/') {
                  state = STATE_COMMENT_SINGLE;
                  ++i;
                } else if (query.charAt(i + 1) === '*') {
                  state = STATE_COMMENT_MULTI;
                  ++i;
                } 
              }
            }
            break;
          case STATE_COMMENT_SINGLE:
            if (c === '\r' || c === '\n') {
              state = STATE_NORMAL;
            }
            break;
          case STATE_COMMENT_MULTI:
            if (c === '*') {
              if (i + 1 <= n && query.charAt(i + 1) === '/') {
                state = STATE_NORMAL;
                ++i;
              }
            }
            break;
          case STATE_STRING_SINGLE:
            if (c === '\\') {
              ++i;
            } else if (c === '\'') {
              state = STATE_NORMAL;
            }
            break;
          case STATE_STRING_DOUBLE:
            if (c === '\\') {
              ++i;
            } else if (c === '"') {
              state = STATE_NORMAL;
            }
            break;
          case STATE_STRING_TICK:
            if (c === '`') {
              state = STATE_NORMAL;
            }
            break;
          case STATE_STRING_BACKTICK:
            if (c === '´') {
              state = STATE_NORMAL;
            }
            break;
          case STATE_BIND:
            if (!/^[@a-zA-Z0-9_]+$/.test(c)) {
              bindParams.push(query.substring(start, i));
              state = STATE_NORMAL;
              start = undefined;
            } 
            break;
        }
      }

      var match;
      _.each(bindParams, function(v, k) {
        match = v.match(self.bindParamRegExp);

        if (match) {
          bindParams[k] = match[1];
        }
      });

      return {
        query: query,
        bindParams: bindParams 
      };

    },

    checkForNewBindParams: function() {
      var self = this;
      //Remove comments
      var foundBindParams = this.parseQuery(this.aqlEditor.getValue()).bindParams;

      var newObject = {};
      _.each(foundBindParams, function(word) {
        if (self.bindParamTableObj[word]) {
          newObject[word] = self.bindParamTableObj[word];
        }
        else {
          newObject[word] = '';
        }
      });

      Object.keys(foundBindParams).forEach(function(keyNew) {
        Object.keys(self.bindParamTableObj).forEach(function(keyOld) {
          if (keyNew === keyOld) {
            newObject[keyNew] = self.bindParamTableObj[keyOld];
          }
        });
      });
      self.bindParamTableObj = newObject;
    },

    renderBindParamTable: function(init) {
      $('#arangoBindParamTable tbody').html('');

      if (init) {
        this.getCachedQuery();
      }

      var counter = 0;

      _.each(this.bindParamTableObj, function(val, key) {
        $('#arangoBindParamTable tbody').append(
          "<tr>" + 
            "<td>" + key + "</td>" + 
            '<td><input name=' + key + ' type="text"></input></td>' + 
          "</tr>"
        );
        counter ++;
        _.each($('#arangoBindParamTable input'), function(element) {
          if ($(element).attr('name') === key) {
            if (val instanceof Array) {
              $(element).val(JSON.stringify(val)).addClass('arraytype');
            }
            else if (typeof val === 'object') {
              $(element).val(JSON.stringify(val)).addClass(typeof val + 'type');
            }
            else {
              $(element).val(val).addClass(typeof val + 'type');
            }
          }
        });
      });
      if (counter === 0) {
        $('#arangoBindParamTable tbody').append(
          '<tr class="noBgColor">' + 
            "<td>No bind parameters defined.</td>" + 
            '<td></td>' + 
          "</tr>"
        );
      }
    },

    fillBindParamTable: function(object) {
      _.each(object, function(val, key) {
        _.each($('#arangoBindParamTable input'), function(element) {
          if ($(element).attr('name') === key) {
            $(element).val(val);
          }
        });
      });
    },

    initAce: function() {

      var self = this;

      //init aql editor
      this.aqlEditor = ace.edit("aqlEditor");
      this.aqlEditor.getSession().setMode("ace/mode/aql");
      this.aqlEditor.setFontSize("10pt");

      this.bindParamAceEditor = ace.edit("bindParamAceEditor");
      this.bindParamAceEditor.getSession().setMode("ace/mode/json");
      this.bindParamAceEditor.setFontSize("10pt");

      this.bindParamAceEditor.getSession().on('change', function() {
        try {
          self.bindParamTableObj = JSON.parse(self.bindParamAceEditor.getValue());
          self.allowParamToggle = true;
          self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));
        }
        catch (e) {
          if (self.bindParamAceEditor.getValue() === '') {
            _.each(self.bindParamTableObj, function(v, k) {
              self.bindParamTableObj[k] = '';
            });
            self.allowParamToggle = true;
          }
          else {
            self.allowParamToggle = false;
          }
        }
      });

      this.aqlEditor.getSession().on('change', function() {
        self.checkForNewBindParams();
        self.renderBindParamTable();
        if (self.initDone) {
          self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));
        }
        self.bindParamAceEditor.setValue(JSON.stringify(self.bindParamTableObj, null, "\t"), 1);
        $('#aqlEditor .ace_text-input').focus();

        self.resize();
      });

      this.aqlEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      this.aqlEditor.commands.addCommand({
        name: "executeQuery",
        bindKey: {win: "Ctrl-Return", mac: "Command-Return", linux: "Ctrl-Return"},
        exec: function() {
          self.executeQuery();
        }
      });

      this.aqlEditor.commands.addCommand({
        name: "saveQuery",
        bindKey: {win: "Ctrl-Shift-S", mac: "Command-Shift-S", linux: "Ctrl-Shift-S"},
        exec: function() {
          self.addAQL();
        }
      });

      this.aqlEditor.commands.addCommand({
        name: "explainQuery",
        bindKey: {win: "Ctrl-Shift-Return", mac: "Command-Shift-Return", linux: "Ctrl-Shift-Return"},
        exec: function() {
          self.explainQuery();
        }
      });

      this.aqlEditor.commands.addCommand({
        name: "togglecomment",
        bindKey: {win: "Ctrl-Shift-C", linux: "Ctrl-Shift-C", mac: "Command-Shift-C"},
        exec: function (editor) {
          editor.toggleCommentLines();
        },
        multiSelectAction: "forEach"
      });

      this.aqlEditor.commands.addCommand({
        name: "showSpotlight",
        bindKey: {win: "Ctrl-Space", mac: "Ctrl-Space", linux: "Ctrl-Space"},
        exec: function() {
          self.showSpotlight();
        }
      });

      this.queryPreview = ace.edit("queryPreview");
      this.queryPreview.getSession().setMode("ace/mode/aql");
      this.queryPreview.setReadOnly(true);
      this.queryPreview.setFontSize("13px");

      //auto focus this editor
      $('#aqlEditor .ace_text-input').focus();
    },

    updateQueryTable: function () {
      var self = this;
      this.updateLocalQueries();

      this.myQueriesTableDesc.rows = this.customQueries;
      _.each(this.myQueriesTableDesc.rows, function(k) {
        k.secondRow = '<span class="spanWrapper">' + 
          '<span id="copyQuery" title="Copy query"><i class="fa fa-copy"></i></span>' +
          '<span id="explQuery" title="Explain query"><i class="fa fa-comments"></i></i></span>' +
          '<span id="runQuery" title="Run query"><i class="fa fa-play-circle-o"></i></i></span>' +
          '<span id="deleteQuery" title="Delete query"><i class="fa fa-minus-circle"></i></span>' +
          '</span>';
        if (k.hasOwnProperty('parameter')) {
          delete k.parameter;
        }
        delete k.value;
      });

      function compare(a,b) {
        var x;
        if (a.name < b.name) {
          x = -1;
        }
        else if (a.name > b.name) {
          x = 1;
        }
        else {
          x = 0;
        }
        return x;
      }

      this.myQueriesTableDesc.rows.sort(compare);

      _.each(this.queries, function(val) {
        if (val.hasOwnProperty('parameter')) {
          delete val.parameter;
        }
        self.myQueriesTableDesc.rows.push({
          name: val.name,
          thirdRow: '<span class="spanWrapper">' + 
            '<span id="copyQuery" title="Copy query"><i class="fa fa-copy"></i></span></span>' 
        });
      });

      // escape all columns but the third (which contains HTML)
      this.myQueriesTableDesc.unescaped = [ false, true, true ];


      this.$(this.myQueriesId).html(this.table.render({content: this.myQueriesTableDesc}));
    },

    listenKey: function (e) {
      if (e.keyCode === 13) {
        this.saveAQL(e);
      }
      this.checkSaveName();
    },

    addAQL: function () {
      //update queries first, before showing
      this.refreshAQL(true);

      //render options
      this.createCustomQueryModal();
      setTimeout(function () {
        $('#new-query-name').focus();
      }, 500);
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
      window.modalView.show('modalTable.ejs', 'Save Query', buttons, tableContent, undefined, undefined,
        {'keyup #new-query-name' : this.listenKey.bind(this)});
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
      if (found) {
        $('#modalButton1').removeClass('button-success');
        $('#modalButton1').addClass('button-warning');
        $('#modalButton1').text('Update');
      } 
      else {
        $('#modalButton1').removeClass('button-warning');
        $('#modalButton1').addClass('button-success');
        $('#modalButton1').text('Save');
      }
    },

    saveAQL: function (e) {
      e.stopPropagation();

      //update queries first, before writing
      this.refreshAQL();

      var saveName = $('#new-query-name').val(),
      bindVars = this.bindParamTableObj;

      if ($('#new-query-name').hasClass('invalid-input')) {
        return;
      }

      //Heiko: Form-Validator - illegal query name
      if (saveName.trim() === '') {
        return;
      }

      var content = this.aqlEditor.getValue(),
      //check for already existing entry
      quit = false;
      _.each(this.customQueries, function (v) {
        if (v.name === saveName) {
          v.value = content;
          quit = true;
          return;
        }
      });

      if (quit === true) {
        //Heiko: Form-Validator - name already taken
        // Update model and save
        this.collection.findWhere({name: saveName}).set("value", content);
      }
      else {
        if (bindVars === '' || bindVars === undefined) {
          bindVars = '{}';
        }

        if (typeof bindVars === 'string') {
          try {
            bindVars = JSON.parse(bindVars);
          }
          catch (err) {
            arangoHelper.arangoError("Query", "Could not parse bind parameter");
          }
        }
        this.collection.add({
          name: saveName,
          parameter: bindVars, 
          value: content
        });
      }

      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("Query", "Could not save query");
        }
        else {
          var self = this;
          this.collection.fetch({
            success: function() {
              self.updateLocalQueries();
            }
          });
        }
      }.bind(this);
      this.collection.saveCollectionQueries(callback);
      window.modalView.hide();
    },

    verifyQueryAndParams: function() {
      var quit = false;

      if (this.aqlEditor.getValue().length === 0) {
        arangoHelper.arangoError("Query", "Your query is empty");
        quit = true;
      }

      var keys = [];
      _.each(this.bindParamTableObj, function(val, key) {
        if (val === '') {
          quit = true;
          keys.push(key);
        }
      });
      if (keys.length > 0) {
        arangoHelper.arangoError("Bind Parameter", JSON.stringify(keys) + " not defined.");
      }

      return quit;
    },

    executeQuery: function () {

      if (this.verifyQueryAndParams()) {
        return;
      }

      this.$(this.outputDiv).prepend(this.outputTemplate.render({
        counter: this.outputCounter,
        type: "Query"
      }));

      $('#outputEditorWrapper' + this.outputCounter).hide();
      $('#outputEditorWrapper' + this.outputCounter).show('fast');

      var counter = this.outputCounter,
      outputEditor = ace.edit("outputEditor" + counter),
      sentQueryEditor = ace.edit("sentQueryEditor" + counter),
      sentBindParamEditor = ace.edit("sentBindParamEditor" + counter);

      sentQueryEditor.getSession().setMode("ace/mode/aql");
      sentQueryEditor.setOption("vScrollBarAlwaysVisible", true);
      sentQueryEditor.setFontSize("13px");
      sentQueryEditor.setReadOnly(true);
      this.setEditorAutoHeight(sentQueryEditor);

      outputEditor.setFontSize("13px");
      outputEditor.getSession().setMode("ace/mode/json");
      outputEditor.setReadOnly(true);
      outputEditor.setOption("vScrollBarAlwaysVisible", true);
      this.setEditorAutoHeight(outputEditor);

      sentBindParamEditor.setValue(JSON.stringify(this.bindParamTableObj), 1);
      sentBindParamEditor.setOption("vScrollBarAlwaysVisible", true);
      sentBindParamEditor.getSession().setMode("ace/mode/json");
      sentBindParamEditor.setReadOnly(true);
      this.setEditorAutoHeight(sentBindParamEditor);

      this.fillResult(outputEditor, sentQueryEditor, counter);
      this.outputCounter++;
    },

    readQueryData: function() {
      var selectedText = this.aqlEditor.session.getTextRange(this.aqlEditor.getSelectionRange());
      var sizeBox = $('#querySize');
      var data = {
        query: selectedText || this.aqlEditor.getValue(),
        id: "currentFrontendQuery"
      };

      if (sizeBox.val() !== 'all') {
        data.batchSize = parseInt(sizeBox.val(), 10);
      }

      //var parsedBindVars = {}, tmpVar;
      if (Object.keys(this.bindParamTableObj).length > 0) {
        /*
        _.each(this.bindParamTableObj, function(value, key) {
          try {
            tmpVar = JSON.parse(value);
          }
          catch (e) {
            tmpVar = value;
          }
          parsedBindVars[key] = tmpVar;
        });*/
        data.bindVars = this.bindParamTableObj;
      }
      return JSON.stringify(data);
    },

    fillResult: function(outputEditor, sentQueryEditor, counter) {
      var self = this;

      var queryData = this.readQueryData();

      if (queryData) {
        sentQueryEditor.setValue(self.aqlEditor.getValue(), 1);

        $.ajax({
          type: "POST",
          url: "/_api/cursor",
          headers: {
            'x-arango-async': 'store' 
          },
          data: queryData,
          contentType: "application/json",
          processData: false,
          success: function (data, textStatus, xhr) {
            if (xhr.getResponseHeader('x-arango-async-id')) {
              self.queryCallbackFunction(xhr.getResponseHeader('x-arango-async-id'), outputEditor, counter);
            }
            $.noty.clearQueue();
            $.noty.closeAll();
            self.handleResult(counter);
          },
          error: function (data) {
            try {
              var temp = JSON.parse(data.responseText);
              arangoHelper.arangoError('[' + temp.errorNum + ']', temp.errorMessage);
            }
            catch (e) {
              arangoHelper.arangoError("Query error", "ERROR");
            }
            self.handleResult(counter);
          }
        });
      }
    },

    handleResult: function() {
      window.progressView.hide();
      $('#removeResults').show();

      $(".centralRow").animate({ scrollTop: $('#queryContent').height() }, "fast");
    },

    setEditorAutoHeight: function (editor) {
      editor.setOptions({
        maxLines: 100,
        minLines: 10
      }); 
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

    queryCallbackFunction: function(queryID, outputEditor, counter) {

      var self = this;


      var cancelRunningQuery = function(id, counter) {

        $.ajax({
          url: '/_api/job/'+ encodeURIComponent(id) + "/cancel",
          type: 'PUT',
          success: function() {
            window.clearTimeout(self.checkQueryTimer);
            $('#outputEditorWrapper' + counter).remove();
            arangoHelper.arangoNotification("Query", "Query canceled.");
          }
        });
      };

      $('#outputEditorWrapper' + counter + ' #cancelCurrentQuery').bind('click', function() {
        cancelRunningQuery(queryID, counter);
      });

      $('#outputEditorWrapper' + counter + ' #copy2aqlEditor').bind('click', function() {

        if (!$('#toggleQueries1').is(':visible')) {
          self.toggleQueries();
        }

        var aql = ace.edit("sentQueryEditor" + counter).getValue();
        var bindParam = JSON.parse(ace.edit("sentBindParamEditor" + counter).getValue());
        self.aqlEditor.setValue(aql, 1);
        self.deselect(self.aqlEditor);
        if (Object.keys(bindParam).length > 0) {
          self.bindParamTableObj = bindParam;
          self.setCachedQuery(self.aqlEditor.getValue(), JSON.stringify(self.bindParamTableObj));

          if ($('#bindParamEditor').is(':visible')) {
            self.renderBindParamTable();
          }
          else {
            self.bindParamAceEditor.setValue(JSON.stringify(bindParam), 1);
            self.deselect(self.bindParamAceEditor);
          }
        }
        $(".centralRow").animate({ scrollTop: 0 }, "fast");
        self.resize();
      });

      this.execPending = false;

      var warningsFunc = function(data) {
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
        outputEditor.setValue(warnings + JSON.stringify(data.result, undefined, 2), 1);
        outputEditor.getSession().setScrollTop(0);
      };

      var fetchQueryResult = function(data) {
        warningsFunc(data);
        window.progressView.hide();

        var appendSpan = function(value, icon) {
          $('#outputEditorWrapper' + counter + ' .arangoToolbarTop .pull-left').append(
            '<span><i class="fa ' + icon + '"></i><i>' + value + '</i></span>'
          );
        };

        $('#outputEditorWrapper' + counter + ' .pull-left #spinner').remove();
          
        var time = "-";
        if (data && data.extra && data.extra.stats) {
          time = data.extra.stats.executionTime.toFixed(3) + " s";
        }
        appendSpan(time, 'fa-clock-o');

        if (data.extra) {
          if (data.extra.stats) {
            if (data.extra.stats.writesExecuted > 0 || data.extra.stats.writesIgnored > 0) {
              appendSpan(
                data.extra.stats.writesExecuted + ' writes', 'fa-check-circle positive'
              );
              if (data.extra.stats.writesIgnored === 0) {
                appendSpan(
                  data.extra.stats.writesIgnored + ' writes ignored', 'fa-check-circle positive'
                );
              }
              else {
                appendSpan(
                  data.extra.stats.writesIgnored + ' writes ignored', 'fa-exclamation-circle warning'
                );
              }
            }
            if (data.extra.stats.scannedFull > 0) {
              appendSpan(
                data.extra.stats.scannedFull + ' full collection scan', 'fa-exclamation-circle warning'
              );
            }
            else {
              appendSpan(
                data.extra.stats.scannedFull + ' full collection scan', 'fa-check-circle positive'
              );
            }
          }
        }

        $('#outputEditorWrapper' + counter + ' .switchAce').show();
        $('#outputEditorWrapper' + counter + ' .fa-close').show();
        $('#outputEditor' + counter).css('opacity', '1');
        $('#outputEditorWrapper' + counter + ' #downloadQueryResult').show();
        $('#outputEditorWrapper' + counter + ' #copy2aqlEditor').show();
        $('#outputEditorWrapper' + counter + ' #cancelCurrentQuery').remove();

        self.setEditorAutoHeight(outputEditor);
        self.deselect(outputEditor);
      };

      //check if async query is finished
      var checkQueryStatus = function() {
        $.ajax({
          type: "PUT",
          url: "/_api/job/" + encodeURIComponent(queryID),
          contentType: "application/json",
          processData: false,
          success: function (data, textStatus, xhr) {

            //query finished, now fetch results
            if (xhr.status === 201) {
              fetchQueryResult(data);
            }
            //query not ready yet, retry
            else if (xhr.status === 204) {
              self.checkQueryTimer = window.setTimeout(function() {
                checkQueryStatus(); 
              }, 500);
            }
          },
          error: function (resp) {
            var error;

            try {

              if (resp.statusText === 'Gone') {
                arangoHelper.arangoNotification("Query", "Query execution aborted.");
                self.removeOutputEditor(counter);
                return;
              }

              error = JSON.parse(resp.responseText);
              arangoHelper.arangoError("Query", error.errorMessage);
              if (error.errorMessage) {
                if (error.errorMessage.match(/\d+:\d+/g) !== null) {
                  self.markPositionError(
                    error.errorMessage.match(/'.*'/g)[0],
                    error.errorMessage.match(/\d+:\d+/g)[0]
                  );
                }
                else {
                  self.markPositionError(
                    error.errorMessage.match(/\(\w+\)/g)[0]
                  );
                }
                self.removeOutputEditor(counter);
              }
            }
            catch (e) {
              console.log(error);
              if (error.code !== 400) {
                arangoHelper.arangoError("Query", "Successfully aborted.");
              }
              self.removeOutputEditor(counter);
            }

            window.progressView.hide();
          }
        });
      };
      checkQueryStatus();
    },

    markPositionError: function(text, pos) {
      var row;

      if (pos) {
        row = pos.split(":")[0];
        text = text.substr(1, text.length - 2);
      }

      var found = this.aqlEditor.find(text);

      if (!found && pos) {
        this.aqlEditor.selection.moveCursorToPosition({row: row, column: 0});
        this.aqlEditor.selection.selectLine(); 
      }
      window.setTimeout(function() {
        $('.ace_start').first().css('background', 'rgba(255, 129, 129, 0.7)');
      }, 100);
    },

    refreshAQL: function() {
      var self = this;

      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError('Query', 'Could not reload Queries');
        }
        else {
          self.updateLocalQueries();
          self.updateQueryTable();
        }
      }.bind(self);

      var originCallback = function() {
        self.getSystemQueries(callback);
      }.bind(self);

      this.getAQL(originCallback);
    },

    getSystemQueries: function (callback) {
      var self = this;

      $.ajax({
        type: "GET",
        cache: false,
        url: "js/arango/aqltemplates.json",
        contentType: "application/json",
        processData: false,
        success: function (data) {
          if (callback) {
            callback(false);
          }
          self.queries = data;
        },
        error: function () {
          if (callback) {
            callback(true);
          }
          arangoHelper.arangoNotification("Query", "Error while loading system templates");
        }
      });
    },

    updateLocalQueries: function () {
      var self = this;
      this.customQueries = [];

      this.collection.each(function(model) {
        self.customQueries.push({
          name: model.get("name"),
          value: model.get("value"),
          parameter: model.get("parameter")
        });
      });
    },

    getAQL: function (originCallback) {
      var self = this;

      this.collection.fetch({
        success: function() {
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

            var callback = function(error) {
              if (error) {
                arangoHelper.arangoError(
                  "Custom Queries",
                  "Could not import old local storage queries"
                );
              }
              else {
                localStorage.removeItem("customQueries");
              }
            }.bind(self);
            self.collection.saveCollectionQueries(callback);
          }
          self.updateLocalQueries();

          if (originCallback) {
            originCallback();
          }
        }
      });
    }
  });
}());


/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Joi, Backbone, window, templateEngine, $ */

(function() {
  "use strict";

  window.SettingsView = Backbone.View.extend({

    el: "#content",

    initialize: function(options) {
      this.collectionName = options.collectionName;
      this.model = this.collection;
    },

    events: {
    },

    render: function() {
      this.breadcrumb();
      window.arangoHelper.buildCollectionSubNav(this.collectionName, 'Settings');

      this.renderSettings();
    },

    breadcrumb: function () {
      $('#subNavigationBar .breadcrumb').html(
        'Collection: ' + this.collectionName
      );
    },

    unloadCollection: function () {

      var unloadCollectionCallback = function(error) {
        if (error) {
          arangoHelper.arangoError('Collection error', this.model.get("name") + ' could not be unloaded.');
        }
        else if (error === undefined) {
          this.model.set("status", "unloading");
          this.render();
        }
        else {
          if (window.location.hash === "#collections") {
            this.model.set("status", "unloaded");
            this.render();
          }
          else {
            arangoHelper.arangoNotification("Collection " + this.model.get("name") + " unloaded.");
          }
        }
      }.bind(this);

      this.model.unloadCollection(unloadCollectionCallback);
      window.modalView.hide();
    },

    loadCollection: function () {

      var loadCollectionCallback = function(error) {
        if (error) {
          arangoHelper.arangoError('Collection error', this.model.get("name") + ' could not be loaded.');
        }
        else if (error === undefined) {
          this.model.set("status", "loading");
          this.render();
        }
        else {
          if (window.location.hash === "#collections") {
            this.model.set("status", "loaded");
            this.render();
          }
          else {
            arangoHelper.arangoNotification("Collection " + this.model.get("name") + " loaded.");
          }
        }
      }.bind(this);

      this.model.loadCollection(loadCollectionCallback);
      window.modalView.hide();
    },

    truncateCollection: function () {
      this.model.truncateCollection();
      window.modalView.hide();
    },

    deleteCollection: function () {
      this.model.destroy(
        {
          error: function() {
            arangoHelper.arangoError('Could not delete collection.');
          },
          success: function() {
            window.App.navigate("#collections", {trigger: true});
          }
        }
      );
    },

    saveModifiedCollection: function() {

      var callback = function(error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError("Error", "Could not get coordinator info");
        }
        else {
          var newname;
          if (isCoordinator) {
            newname = this.model.get('name');
          }
          else {
            newname = $('#change-collection-name').val();
          }
          var status = this.model.get('status');

          if (status === 'loaded') {
            var journalSize;
            try {
              journalSize = JSON.parse($('#change-collection-size').val() * 1024 * 1024);
            }
            catch (e) {
              arangoHelper.arangoError('Please enter a valid number');
              return 0;
            }

            var indexBuckets;
            try {
              indexBuckets = JSON.parse($('#change-index-buckets').val());
              if (indexBuckets < 1 || parseInt(indexBuckets) !== Math.pow(2, Math.log2(indexBuckets))) {
                throw "invalid indexBuckets value";
              }
            }
            catch (e) {
              arangoHelper.arangoError('Please enter a valid number of index buckets');
              return 0;
            }
            var callbackChange = function(error) {
              if (error) {
                arangoHelper.arangoError("Collection error: " + error.responseText);
              }
              else {
                this.collectionsView.render();
                window.modalView.hide();
              }
            }.bind(this);

            var callbackRename = function(error) {
              if (error) {
                arangoHelper.arangoError("Collection error: " + error.responseText);
              }
              else {
                var wfs = $('#change-collection-sync').val();
                this.model.changeCollection(wfs, journalSize, indexBuckets, callbackChange);
              }
            }.bind(this);

            this.model.renameCollection(newname, callbackRename);
          }
          else if (status === 'unloaded') {
            if (this.model.get('name') !== newname) {

              var callbackRename2 = function(error, data) {
                if (error) {
                  arangoHelper.arangoError("Collection error: " + data.responseText);
                }
                else {
                  this.collectionsView.render();
                  window.modalView.hide();
                }
              }.bind(this);

              this.model.renameCollection(newname, callbackRename2);
            }
            else {
              window.modalView.hide();
            }
          }
        }
      }.bind(this);

      window.isCoordinator(callback);
    },
    renderSettings: function() {

      var callback = function(error, isCoordinator) {
        if (error) {
          arangoHelper.arangoError("Error","Could not get coordinator info");
        }
        else {
          var collectionIsLoaded = false;

          if (this.model.get('status') === "loaded") {
            collectionIsLoaded = true;
          }

          var buttons = [],
            tableContent = [];

          if (!isCoordinator) {
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

          var after = function() {

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

            var tabBar = ["General", "Indices"],
              templates =  ["modalTable.ejs", "indicesView.ejs"];

            window.modalView.show(
              templates,
              "Modify Collection",
              buttons,
              tableContent, null, null,
              this.events, null,
              tabBar, 'content'
            );
            $($('#infoTab').children()[1]).remove();
          }.bind(this);

          if (collectionIsLoaded) {

            var callback2 = function(error, data) {
              if (error) {
                arangoHelper.arangoError("Collection", "Could not fetch properties");
              }
              else {
                var journalSize = data.journalSize/(1024*1024);
                var indexBuckets = data.indexBuckets;
                var wfs = data.waitForSync;

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

                tableContent.push(
                  window.modalView.createTextEntry(
                    "change-index-buckets",
                    "Index buckets",
                    indexBuckets,
                    "The number of index buckets for this collection. Must be at least 1 and a power of 2.",
                    "",
                    true,
                    [
                      {
                        rule: Joi.string().allow('').optional().regex(/^[1-9][0-9]*$/),
                        msg: "Must be a number greater than 1 and a power of 2."
                      }
                    ]
                  )
                );

                // prevent "unexpected sync method error"
                tableContent.push(
                  window.modalView.createSelectEntry(
                    "change-collection-sync",
                    "Wait for sync",
                    wfs,
                    "Synchronize to disk before returning from a create or update of a document.",
                    [{value: false, label: "No"}, {value: true, label: "Yes"}]        )
                );
              }
              after();

            }.bind(this);

            this.model.getProperties(callback2);
          }
          else {
            after(); 
          }

        }
      }.bind(this);
      window.isCoordinator(callback);
    }

  });

}());

/*global window, $, Backbone, templateEngine,  _, d3, Dygraph, document */

(function() {
  "use strict";

  window.ShowClusterView = Backbone.View.extend({
    detailEl: '#modalPlaceholder',
    el: "#content",
    defaultFrame: 20 * 60 * 1000,
    template: templateEngine.createTemplate("showCluster.ejs"),
    modal: templateEngine.createTemplate("waitModal.ejs"),
    detailTemplate: templateEngine.createTemplate("detailView.ejs"),

    events: {
      "change #selectDB"                : "updateCollections",
      "change #selectCol"               : "updateShards",
      "click .dbserver.success"         : "dashboard",
      "click .coordinator.success"      : "dashboard"
    },

    replaceSVGs: function() {
      $(".svgToReplace").each(function() {
        var img = $(this);
        var id = img.attr("id");
        var src = img.attr("src");
        $.get(src, function(d) {
          var svg = $(d).find("svg");
          svg.attr("id", id)
          .attr("class", "icon")
          .removeAttr("xmlns:a");
          img.replaceWith(svg);
        }, "xml");
      });
    },

    updateServerTime: function() {
      this.serverTime = new Date().getTime();
    },

    setShowAll: function() {
      this.graphShowAll = true;
    },

    resetShowAll: function() {
      this.graphShowAll = false;
      this.renderLineChart();
    },


    initialize: function(options) {
      this.options = options;
      this.interval = 10000;
      this.isUpdating = false;
      this.timer = null;
      this.knownServers = [];
      this.graph = undefined;
      this.graphShowAll = false;
      this.updateServerTime();
      this.dygraphConfig = this.options.dygraphConfig;
      this.dbservers = new window.ClusterServers([], {
        interval: this.interval
      });
      this.coordinators = new window.ClusterCoordinators([], {
        interval: this.interval
      });
      this.documentStore =  new window.arangoDocuments();
      this.statisticsDescription = new window.StatisticsDescription();
      this.statisticsDescription.fetch({
        async: false
      });
      this.dbs = new window.ClusterDatabases([], {
        interval: this.interval
      });
      this.cols = new window.ClusterCollections();
      this.shards = new window.ClusterShards();
      this.startUpdating();
    },

    listByAddress: function(callback) {
      var byAddress = {};
      var self = this;
      this.dbservers.byAddress(byAddress, function(res) {
        self.coordinators.byAddress(res, callback);
      });
    },

    updateCollections: function() {
      var self = this;
      var selCol = $("#selectCol");
      var dbName = $("#selectDB").find(":selected").attr("id");
      if (!dbName) {
        return;
      }
      var colName = selCol.find(":selected").attr("id");
      selCol.html("");
      this.cols.getList(dbName, function(list) {
        _.each(_.pluck(list, "name"), function(c) {
          selCol.append("<option id=\"" + c + "\">" + c + "</option>");
        });
        var colToSel = $("#" + colName, selCol);
        if (colToSel.length === 1) {
          colToSel.prop("selected", true);
        }
        self.updateShards();
      });
    },

    updateShards: function() {
      var dbName = $("#selectDB").find(":selected").attr("id");
      var colName = $("#selectCol").find(":selected").attr("id");
      this.shards.getList(dbName, colName, function(list) {
        $(".shardCounter").html("0");
        _.each(list, function(s) {
          $("#" + s.server + "Shards").html(s.shards.length);
        });
      });
    },

    updateServerStatus: function(nextStep) {
      var self = this;
      var callBack = function(cls, stat, serv) {
        var id = serv,
          type, icon;
        id = id.replace(/\./g,'-');
        id = id.replace(/\:/g,'_');
        icon = $("#id" + id);
        if (icon.length < 1) {
          // callback after view was unrendered
          return;
        }
        type = icon.attr("class").split(/\s+/)[1];
        icon.attr("class", cls + " " + type + " " + stat);
        if (cls === "coordinator") {
          if (stat === "success") {
            $(".button-gui", icon.closest(".tile")).toggleClass("button-gui-disabled", false);
          } else {
            $(".button-gui", icon.closest(".tile")).toggleClass("button-gui-disabled", true);
          }

        }
      };
      this.coordinators.getStatuses(callBack.bind(this, "coordinator"), function() {
        self.dbservers.getStatuses(callBack.bind(self, "dbserver"));
        nextStep();
      });
    },

    updateDBDetailList: function() {
      var self = this;
      var selDB = $("#selectDB");
      var dbName = selDB.find(":selected").attr("id");
      selDB.html("");
      this.dbs.getList(function(dbList) {
        _.each(_.pluck(dbList, "name"), function(c) {
          selDB.append("<option id=\"" + c + "\">" + c + "</option>");
        });
        var dbToSel = $("#" + dbName, selDB);
        if (dbToSel.length === 1) {
          dbToSel.prop("selected", true);
        }
        self.updateCollections();
      });
    },

    rerender : function() {
      var self = this;
      this.updateServerStatus(function() {
        self.getServerStatistics(function() {
          self.updateServerTime();
          self.data = self.generatePieData();
          self.renderPieChart(self.data);
          self.renderLineChart();
          self.updateDBDetailList();
        });
      });
    },

    render: function() {
      this.knownServers = [];
      delete this.hist;
      var self = this;
      this.listByAddress(function(byAddress) {
        if (Object.keys(byAddress).length === 1) {
          self.type = "testPlan";
        } else {
          self.type = "other";
        }
        self.updateDBDetailList();
        self.dbs.getList(function(dbList) {
          $(self.el).html(self.template.render({
            dbs: _.pluck(dbList, "name"),
            byAddress: byAddress,
            type: self.type
          }));
          $(self.el).append(self.modal.render({}));
          self.replaceSVGs();
          /* this.loadHistory(); */
          self.getServerStatistics(function() {
            self.data = self.generatePieData();
            self.renderPieChart(self.data);
            self.renderLineChart();
            self.updateDBDetailList();
            self.startUpdating();
          });
        });
      });
    },

    generatePieData: function() {
      var pieData = [];
      var self = this;

      this.data.forEach(function(m) {
        pieData.push({key: m.get("name"), value: m.get("system").virtualSize,
          time: self.serverTime});
      });

      return pieData;
    },

    /*
     loadHistory : function() {
       this.hist = {};

       var self = this;
       var coord = this.coordinators.findWhere({
         status: "ok"
       });

       var endpoint = coord.get("protocol")
       + "://"
       + coord.get("address");

       this.dbservers.forEach(function (dbserver) {
         if (dbserver.get("status") !== "ok") {return;}

         if (self.knownServers.indexOf(dbserver.id) === -1) {
           self.knownServers.push(dbserver.id);
         }

         var server = {
           raw: dbserver.get("address"),
           isDBServer: true,
           target: encodeURIComponent(dbserver.get("name")),
           endpoint: endpoint,
           addAuth: window.App.addAuth.bind(window.App)
         };
       });

       this.coordinators.forEach(function (coordinator) {
         if (coordinator.get("status") !== "ok") {return;}

         if (self.knownServers.indexOf(coordinator.id) === -1) {
           self.knownServers.push(coordinator.id);
         }

         var server = {
           raw: coordinator.get("address"),
           isDBServer: false,
           target: encodeURIComponent(coordinator.get("name")),
           endpoint: coordinator.get("protocol") + "://" + coordinator.get("address"),
           addAuth: window.App.addAuth.bind(window.App)
         };
       });
     },
     */

    addStatisticsItem: function(name, time, requests, snap) {
      var self = this;

      if (! self.hasOwnProperty('hist')) {
        self.hist = {};
      }

      if (! self.hist.hasOwnProperty(name)) {
        self.hist[name] = [];
      }

      var h = self.hist[name];
      var l = h.length;

      if (0 === l) {
        h.push({
          time: time,
          snap: snap,
          requests: requests,
          requestsPerSecond: 0
        });
      }
      else {
        var lt = h[l - 1].time;
        var tt = h[l - 1].requests;

        if (tt < requests) {
          var dt = time - lt;
          var ps = 0;

          if (dt > 0) {
            ps = (requests - tt) / dt;
          }

          h.push({
            time: time,
            snap: snap,
            requests: requests,
            requestsPerSecond: ps
          });
        }
        /*
        else {
          h.times.push({
            time: time,
            snap: snap,
            requests: requests,
            requestsPerSecond: 0
          });
        }
        */
      }
    },

    getServerStatistics: function(nextStep) {
      var self = this;
      var snap = Math.round(self.serverTime / 1000);

      this.data = undefined;

      var statCollect = new window.ClusterStatisticsCollection();
      var coord = this.coordinators.first();

      // create statistics collector for DB servers
      this.dbservers.forEach(function (dbserver) {
        if (dbserver.get("status") !== "ok") {return;}

        if (self.knownServers.indexOf(dbserver.id) === -1) {
          self.knownServers.push(dbserver.id);
        }

        var stat = new window.Statistics({name: dbserver.id});

        stat.url = coord.get("protocol") + "://"
        + coord.get("address")
        + "/_admin/clusterStatistics?DBserver="
        + dbserver.get("name");

        statCollect.add(stat);
      });

      // create statistics collector for coordinator
      this.coordinators.forEach(function (coordinator) {
        if (coordinator.get("status") !== "ok") {return;}

        if (self.knownServers.indexOf(coordinator.id) === -1) {
          self.knownServers.push(coordinator.id);
        }

        var stat = new window.Statistics({name: coordinator.id});

        stat.url = coordinator.get("protocol") + "://"
        + coordinator.get("address")
        + "/_admin/statistics";

        statCollect.add(stat);
      });

      var cbCounter = statCollect.size();

      this.data = [];

      var successCB = function(m) {
        cbCounter--;
        var time = m.get("time");
        var name = m.get("name");
        var requests = m.get("http").requestsTotal;

        self.addStatisticsItem(name, time, requests, snap);
        self.data.push(m);
        if (cbCounter === 0) {
          nextStep();
        }
      };
      var errCB = function() {
        cbCounter--;
        if (cbCounter === 0) {
          nextStep();
        }
      };
      // now fetch the statistics
      statCollect.fetch(successCB, errCB);
    },

    renderPieChart: function(dataset) {
      var w = $("#clusterGraphs svg").width();
      var h = $("#clusterGraphs svg").height();
      var radius = Math.min(w, h) / 2; //change 2 to 1.4. It's hilarious.
      // var color = d3.scale.category20();
      var color = this.dygraphConfig.colors;

      var arc = d3.svg.arc() //each datapoint will create one later.
      .outerRadius(radius - 20)
      .innerRadius(0);
      var pie = d3.layout.pie()
      .sort(function (d) {
        return d.value;
      })
      .value(function (d) {
        return d.value;
      });
      d3.select("#clusterGraphs").select("svg").remove();
      var pieChartSvg = d3.select("#clusterGraphs").append("svg")
      // .attr("width", w)
      // .attr("height", h)
      .attr("class", "clusterChart")
      .append("g") //someone to transform. Groups data.
      .attr("transform", "translate(" + w / 2 + "," + ((h / 2) - 10) + ")");

    var arc2 = d3.svg.arc()
    .outerRadius(radius-2)
    .innerRadius(radius-2);
    var slices = pieChartSvg.selectAll(".arc")
    .data(pie(dataset))
    .enter().append("g")
    .attr("class", "slice");
        slices.append("path")
    .attr("d", arc)
    .style("fill", function (item, i) {
      return color[i % color.length];
    })
    .style("stroke", function (item, i) {
      return color[i % color.length];
    });
        slices.append("text")
    .attr("transform", function(d) { return "translate(" + arc.centroid(d) + ")"; })
    // .attr("dy", "0.35em")
    .style("text-anchor", "middle")
    .text(function(d) {
      var v = d.data.value / 1024 / 1024 / 1024;
      return v.toFixed(2); });

    slices.append("text")
    .attr("transform", function(d) { return "translate(" + arc2.centroid(d) + ")"; })
    // .attr("dy", "1em")
    .style("text-anchor", "middle")
    .text(function(d) { return d.data.key; });
  },

  renderLineChart: function() {
    var self = this;

    var interval = 60 * 20;
    var data = [];
    var hash = [];
    var t = Math.round(new Date().getTime() / 1000) - interval;
    var ks = self.knownServers;
    var f = function() {
      return null;
    };

    var d, h, i, j, tt, snap;

    for (i = 0;  i < ks.length;  ++i) {
      h = self.hist[ks[i]];

      if (h) {
        for (j = 0;  j < h.length;  ++j) {
          snap = h[j].snap;

          if (snap < t) {
            continue;
          }

          if (! hash.hasOwnProperty(snap)) {
            tt = new Date(snap * 1000);

            d = hash[snap] = [ tt ].concat(ks.map(f));
          }
          else {
            d = hash[snap];
          }

          d[i + 1] = h[j].requestsPerSecond;
        }
      }
    }

    data = [];

    Object.keys(hash).sort().forEach(function (m) {
      data.push(hash[m]);
    });

    var options = this.dygraphConfig.getDefaultConfig('clusterRequestsPerSecond');
    options.labelsDiv = $("#lineGraphLegend")[0];
    options.labels = [ "datetime" ].concat(ks);

    self.graph = new Dygraph(
      document.getElementById('lineGraph'),
      data,
      options
    );
  },

  stopUpdating: function () {
    window.clearTimeout(this.timer);
    delete this.graph;
    this.isUpdating = false;
  },

  startUpdating: function () {
    if (this.isUpdating) {
      return;
    }

    this.isUpdating = true;

    var self = this;

    this.timer = window.setInterval(function() {
      self.rerender();
    }, this.interval);
  },


  dashboard: function(e) {
    this.stopUpdating();

    var tar = $(e.currentTarget);
    var serv = {};
    var cur;
    var coord;

    var ip_port = tar.attr("id");
    ip_port = ip_port.replace(/\-/g,'.');
    ip_port = ip_port.replace(/\_/g,':');
    ip_port = ip_port.substr(2);

    serv.raw = ip_port;
    serv.isDBServer = tar.hasClass("dbserver");

    if (serv.isDBServer) {
      cur = this.dbservers.findWhere({
        address: serv.raw
      });
      coord = this.coordinators.findWhere({
        status: "ok"
      });
      serv.endpoint = coord.get("protocol")
      + "://"
      + coord.get("address");
    }
    else {
      cur = this.coordinators.findWhere({
        address: serv.raw
      });
      serv.endpoint = cur.get("protocol")
      + "://"
      + cur.get("address");
    }

    serv.target = encodeURIComponent(cur.get("name"));
    window.App.serverToShow = serv;
    window.App.dashboard();
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

  resize: function () {
    var dimensions;
    if (this.graph) {
      dimensions = this.getCurrentSize(this.graph.maindiv_.id);
      this.graph.resize(dimensions.width, dimensions.height);
    }
  }
});
}());

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, $, window, setTimeout, Joi, _ */
/*global templateEngine*/

(function () {
  "use strict";

  window.SpotlightView = Backbone.View.extend({

    template: templateEngine.createTemplate("spotlightView.ejs"),

    el: "#spotlightPlaceholder",

    displayLimit: 8,
    typeahead: null,
    callbackSuccess: null,
    callbackCancel: null,

    collections: {
      system: [],
      doc: [],
      edge: [],
    },

    events: {
      "focusout #spotlight .tt-input" : "hide",
      "keyup #spotlight .typeahead" : "listenKey"
    },

    aqlKeywordsArray: [],
    aqlBuiltinFunctionsArray: [],

    aqlKeywords: 
      "for|return|filter|sort|limit|let|collect|asc|desc|in|into|" + 
      "insert|update|remove|replace|upsert|options|with|and|or|not|" + 
      "distinct|graph|outbound|inbound|any|all|none|aggregate|like|count",

    aqlBuiltinFunctions: 
"to_bool|to_number|to_string|to_list|is_null|is_bool|is_number|is_string|is_list|is_document|" +
"concat|concat_separator|char_length|lower|upper|substring|left|right|trim|reverse|contains|" +
"like|floor|ceil|round|abs|rand|sqrt|pow|length|min|max|average|sum|median|variance_population|" +
"variance_sample|first|last|unique|matches|merge|merge_recursive|has|attributes|values|unset|unset_recursive|keep|" +
"near|within|within_rectangle|is_in_polygon|fulltext|paths|traversal|traversal_tree|edges|stddev_sample|" + 
"stddev_population|slice|nth|position|translate|zip|call|apply|push|append|pop|shift|unshift|remove_value" + 
"remove_nth|graph_paths|shortest_path|graph_shortest_path|graph_distance_to|graph_traversal|graph_traversal_tree|" + 
"graph_edges|graph_vertices|neighbors|graph_neighbors|graph_common_neighbors|graph_common_properties|" +
"graph_eccentricity|graph_betweenness|graph_closeness|graph_absolute_eccentricity|remove_values|" +
"graph_absolute_betweenness|graph_absolute_closeness|graph_diameter|graph_radius|date_now|" +
"date_timestamp|date_iso8601|date_dayofweek|date_year|date_month|date_day|date_hour|" +
"date_minute|date_second|date_millisecond|date_dayofyear|date_isoweek|date_leapyear|date_quarter|date_days_in_month|" + 
"date_add|date_subtract|date_diff|date_compare|date_format|fail|passthru|sleep|not_null|" +
"first_list|first_document|parse_identifier|current_user|current_database|" +
"collections|document|union|union_distinct|intersection|flatten|" +
"ltrim|rtrim|find_first|find_last|split|substitute|md5|sha1|random_token|AQL_LAST_ENTRY",

    listenKey: function(e) {
      if (e.keyCode === 27) {
        if (this.callbackSuccess) {
          this.callbackCancel();
        }
        this.hide();
      }
      else if (e.keyCode === 13) {
        if (this.callbackSuccess) {
          this.callbackSuccess($(this.typeahead).val());
          this.hide();
        }
      }
    },

    substringMatcher: function(strs) {
      return function findMatches(q, cb) {
        var matches, substrRegex;

        matches = [];

        substrRegex = new RegExp(q, 'i');

        _.each(strs, function(str) {
          if (substrRegex.test(str)) {
            matches.push(str);
          }
        });

        cb(matches);
      };
    },

    updateDatasets: function() {
      var self = this;
      this.collections = {
        system: [],
        doc: [],
        edge: [],
      };

      window.App.arangoCollectionsStore.each(function(collection) {
        if (collection.get("isSystem")) {
          self.collections.system.push(collection.get("name"));
        }
        else if (collection.get("type") === 'document') {
          self.collections.doc.push(collection.get("name"));
        }
        else {
          self.collections.edge.push(collection.get("name"));
        }
      });
    },

    stringToArray: function() {
      var self = this;

      _.each(this.aqlKeywords.split('|'), function(value) {
        self.aqlKeywordsArray.push(value.toUpperCase());
      });
      _.each(this.aqlBuiltinFunctions.split('|'), function(value) {
        self.aqlBuiltinFunctionsArray.push(value.toUpperCase());
      });

      //special case for keywords
      self.aqlKeywordsArray.push(true);
      self.aqlKeywordsArray.push(false);
      self.aqlKeywordsArray.push(null);
    },

    show: function(callbackSuccess, callbackCancel, type) {

      this.callbackSuccess = callbackSuccess;
      this.callbackCancel = callbackCancel;
      this.stringToArray();
      this.updateDatasets();

      var genHeader = function(name, icon, type) {
        var string = '<div class="header-type"><h4>' + name + '</h4>';
        if (icon) {
          string += '<span><i class="fa ' + icon + '"></i></span>';
        }
        if (type) {
          string += '<span class="type">' + type.toUpperCase() + '</span>';
        }
        string += '</div>';

        return string;
      };

      $(this.el).html(this.template.render({}));
      $(this.el).show();

      if (type === 'aql') {
        this.typeahead = $('#spotlight .typeahead').typeahead(
          {
            hint: true,
            highlight: true,
            minLength: 1
          },
          {
            name: 'Functions',
            source: this.substringMatcher(this.aqlBuiltinFunctionsArray),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Functions", "fa-code", "aql")
            }
          },
          {
            name: 'Keywords',
            source: this.substringMatcher(this.aqlKeywordsArray),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Keywords", "fa-code", "aql")
            }
          },
          {
            name: 'Documents',
            source: this.substringMatcher(this.collections.doc),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Documents", "fa-file-text-o", "Collection")
            }
          },
          {
            name: 'Edges',
            source: this.substringMatcher(this.collections.edge),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Edges", "fa-share-alt", "Collection")
            }
          },
          {
            name: 'System',
            limit: this.displayLimit,
            source: this.substringMatcher(this.collections.system),
            templates: {
              header: genHeader("System", "fa-cogs", "Collection")
            }
          }
        );
      }
      else {
        this.typeahead = $('#spotlight .typeahead').typeahead(
          {
            hint: true,
            highlight: true,
            minLength: 1
          },
          {
            name: 'Documents',
            source: this.substringMatcher(this.collections.doc),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Documents", "fa-file-text-o", "Collection")
            }
          },
          {
            name: 'Edges',
            source: this.substringMatcher(this.collections.edge),
            limit: this.displayLimit,
            templates: {
              header: genHeader("Edges", "fa-share-alt", "Collection")
            }
          },
          {
            name: 'System',
            limit: this.displayLimit,
            source: this.substringMatcher(this.collections.system),
            templates: {
              header: genHeader("System", "fa-cogs", "Collection")
            }
          }
        );
      }
      

      $('#spotlight .typeahead').focus();
    },

    hide: function() {
      $(this.el).hide();
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

    initialize : function (options) {
      this.currentDB = options.currentDB;
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

    initialize: function(options) {
      this.rowClickCallback = options.rowClick;
    },

    events: {
      "click .pure-table-body .pure-table-row": "rowClick",
      "click .deleteButton": "removeClick"
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
/*global Backbone, sigma, templateEngine, document, _, $, arangoHelper, window*/

(function() {
  "use strict";
  window.testView = Backbone.View.extend({
    el: '#content',

    graph: {
      edges: [],
      nodes: []
    },

    events: {
    },

    initialize: function () {
      console.log(undefined);
    },

    template: templateEngine.createTemplate("testView.ejs"),

    render: function () {
      $(this.el).html(this.template.render({}));
      this.renderGraph();
      return this;
    },

    renderGraph: function () {

      this.convertData();

      console.log(this.graph);

      this.s = new sigma({
        graph: this.graph,
        container: 'graph-container',
        verbose: true,
        renderers: [
          {
            container: document.getElementById('graph-container'),
            type: 'webgl'
          }
        ]
      });
    },

    convertData: function () {

      var self = this;

      _.each(this.dump, function(value) {

        _.each(value.p, function(lol) {
          self.graph.nodes.push({
            id: lol.verticesvalue.v._id,
            label: value.v._key,
            x: Math.random(),
            y: Math.random(),
            size: Math.random()
          });

          self.graph.edges.push({
            id: value.e._id,
            source: value.e._from,
            target: value.e._to
          });
         

        });
      });

      return null;
    },
		
    dump: [
      {
        "v": {
          "label": "7",
          "_id": "circles/G",
          "_rev": "1841663870851",
          "_key": "G"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_foo",
          "_id": "edges/1841666099075",
          "_rev": "1841666099075",
          "_key": "1841666099075",
          "_from": "circles/A",
          "_to": "circles/G"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            }
          ]
        }
      },
      {
        "v": {
          "label": "8",
          "_id": "circles/H",
          "_rev": "1841664067459",
          "_key": "H"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_blob",
          "_id": "edges/1841666295683",
          "_rev": "1841666295683",
          "_key": "1841666295683",
          "_from": "circles/G",
          "_to": "circles/H"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            },
            {
              "label": "8",
              "_id": "circles/H",
              "_rev": "1841664067459",
              "_key": "H"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_blob",
              "_id": "edges/1841666295683",
              "_rev": "1841666295683",
              "_key": "1841666295683",
              "_from": "circles/G",
              "_to": "circles/H"
            }
          ]
        }
      },
      {
        "v": {
          "label": "9",
          "_id": "circles/I",
          "_rev": "1841664264067",
          "_key": "I"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_blub",
          "_id": "edges/1841666492291",
          "_rev": "1841666492291",
          "_key": "1841666492291",
          "_from": "circles/H",
          "_to": "circles/I"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            },
            {
              "label": "8",
              "_id": "circles/H",
              "_rev": "1841664067459",
              "_key": "H"
            },
            {
              "label": "9",
              "_id": "circles/I",
              "_rev": "1841664264067",
              "_key": "I"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_blob",
              "_id": "edges/1841666295683",
              "_rev": "1841666295683",
              "_key": "1841666295683",
              "_from": "circles/G",
              "_to": "circles/H"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_blub",
              "_id": "edges/1841666492291",
              "_rev": "1841666492291",
              "_key": "1841666492291",
              "_from": "circles/H",
              "_to": "circles/I"
            }
          ]
        }
      },
      {
        "v": {
          "label": "10",
          "_id": "circles/J",
          "_rev": "1841664460675",
          "_key": "J"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_zip",
          "_id": "edges/1841666688899",
          "_rev": "1841666688899",
          "_key": "1841666688899",
          "_from": "circles/G",
          "_to": "circles/J"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            },
            {
              "label": "10",
              "_id": "circles/J",
              "_rev": "1841664460675",
              "_key": "J"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_zip",
              "_id": "edges/1841666688899",
              "_rev": "1841666688899",
              "_key": "1841666688899",
              "_from": "circles/G",
              "_to": "circles/J"
            }
          ]
        }
      },
      {
        "v": {
          "label": "11",
          "_id": "circles/K",
          "_rev": "1841664657283",
          "_key": "K"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "right_zup",
          "_id": "edges/1841666885507",
          "_rev": "1841666885507",
          "_key": "1841666885507",
          "_from": "circles/J",
          "_to": "circles/K"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "7",
              "_id": "circles/G",
              "_rev": "1841663870851",
              "_key": "G"
            },
            {
              "label": "10",
              "_id": "circles/J",
              "_rev": "1841664460675",
              "_key": "J"
            },
            {
              "label": "11",
              "_id": "circles/K",
              "_rev": "1841664657283",
              "_key": "K"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_foo",
              "_id": "edges/1841666099075",
              "_rev": "1841666099075",
              "_key": "1841666099075",
              "_from": "circles/A",
              "_to": "circles/G"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_zip",
              "_id": "edges/1841666688899",
              "_rev": "1841666688899",
              "_key": "1841666688899",
              "_from": "circles/G",
              "_to": "circles/J"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "right_zup",
              "_id": "edges/1841666885507",
              "_rev": "1841666885507",
              "_key": "1841666885507",
              "_from": "circles/J",
              "_to": "circles/K"
            }
          ]
        }
      },
      {
        "v": {
          "label": "2",
          "_id": "circles/B",
          "_rev": "1841662887811",
          "_key": "B"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_bar",
          "_id": "edges/1841665116035",
          "_rev": "1841665116035",
          "_key": "1841665116035",
          "_from": "circles/A",
          "_to": "circles/B"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            }
          ]
        }
      },
      {
        "v": {
          "label": "5",
          "_id": "circles/E",
          "_rev": "1841663477635",
          "_key": "E"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_blub",
          "_id": "edges/1841665705859",
          "_rev": "1841665705859",
          "_key": "1841665705859",
          "_from": "circles/B",
          "_to": "circles/E"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            },
            {
              "label": "5",
              "_id": "circles/E",
              "_rev": "1841663477635",
              "_key": "E"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blub",
              "_id": "edges/1841665705859",
              "_rev": "1841665705859",
              "_key": "1841665705859",
              "_from": "circles/B",
              "_to": "circles/E"
            }
          ]
        }
      },
      {
        "v": {
          "label": "6",
          "_id": "circles/F",
          "_rev": "1841663674243",
          "_key": "F"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_schubi",
          "_id": "edges/1841665902467",
          "_rev": "1841665902467",
          "_key": "1841665902467",
          "_from": "circles/E",
          "_to": "circles/F"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            },
            {
              "label": "5",
              "_id": "circles/E",
              "_rev": "1841663477635",
              "_key": "E"
            },
            {
              "label": "6",
              "_id": "circles/F",
              "_rev": "1841663674243",
              "_key": "F"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blub",
              "_id": "edges/1841665705859",
              "_rev": "1841665705859",
              "_key": "1841665705859",
              "_from": "circles/B",
              "_to": "circles/E"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_schubi",
              "_id": "edges/1841665902467",
              "_rev": "1841665902467",
              "_key": "1841665902467",
              "_from": "circles/E",
              "_to": "circles/F"
            }
          ]
        }
      },
      {
        "v": {
          "label": "3",
          "_id": "circles/C",
          "_rev": "1841663084419",
          "_key": "C"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_blarg",
          "_id": "edges/1841665312643",
          "_rev": "1841665312643",
          "_key": "1841665312643",
          "_from": "circles/B",
          "_to": "circles/C"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            },
            {
              "label": "3",
              "_id": "circles/C",
              "_rev": "1841663084419",
              "_key": "C"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blarg",
              "_id": "edges/1841665312643",
              "_rev": "1841665312643",
              "_key": "1841665312643",
              "_from": "circles/B",
              "_to": "circles/C"
            }
          ]
        }
      },
      {
        "v": {
          "label": "4",
          "_id": "circles/D",
          "_rev": "1841663281027",
          "_key": "D"
        },
        "e": {
          "theFalse": false,
          "theTruth": true,
          "label": "left_blorg",
          "_id": "edges/1841665509251",
          "_rev": "1841665509251",
          "_key": "1841665509251",
          "_from": "circles/C",
          "_to": "circles/D"
        },
        "p": {
          "vertices": [
            {
              "label": "1",
              "_id": "circles/A",
              "_rev": "1841662691203",
              "_key": "A"
            },
            {
              "label": "2",
              "_id": "circles/B",
              "_rev": "1841662887811",
              "_key": "B"
            },
            {
              "label": "3",
              "_id": "circles/C",
              "_rev": "1841663084419",
              "_key": "C"
            },
            {
              "label": "4",
              "_id": "circles/D",
              "_rev": "1841663281027",
              "_key": "D"
            }
          ],
          "edges": [
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_bar",
              "_id": "edges/1841665116035",
              "_rev": "1841665116035",
              "_key": "1841665116035",
              "_from": "circles/A",
              "_to": "circles/B"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blarg",
              "_id": "edges/1841665312643",
              "_rev": "1841665312643",
              "_key": "1841665312643",
              "_from": "circles/B",
              "_to": "circles/C"
            },
            {
              "theFalse": false,
              "theTruth": true,
              "label": "left_blorg",
              "_id": "edges/1841665509251",
              "_rev": "1841665509251",
              "_key": "1841665509251",
              "_from": "circles/C",
              "_to": "circles/D"
            }
          ]
        }
      }
    ],

  });
}());

/*jshint browser: true */
/*jshint unused: false */
/*global arangoHelper, Backbone, templateEngine, $, window*/
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

    initialize: function (options) {
      this.userCollection = options.userCollection;
      this.userCollection.fetch({async: true});
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

    toggleUserMenu: function() {
      $('#userBar .subBarDropdown').toggle();
    },

    showDropdown: function () {
      $("#user_dropdown").fadeIn(1);
    },

    hideDropdown: function () {
      $("#user_dropdown").fadeOut(1);
    },

    render: function () {
      var self = this;

      var callback = function(error, username) {
        if (error) {
          arangoHelper.arangoErro("User", "Could not fetch user.");
        }
        else {
          var img = null,
            name = null,
            active = false,
            currentUser = null;
          if (username !== false) {
            currentUser = this.userCollection.findWhere({user: username});
            currentUser.set({loggedIn : true});
            name = currentUser.get("extra").name;
            img = currentUser.get("extra").img;
            active = currentUser.get("active");
            if (! img) {
              img = "img/default_user.png";
            } 
            else {
              img = "https://s.gravatar.com/avatar/" + img + "?s=80";
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
          }
        }
      }.bind(this);

      $('#userBar').on('click', function() {
        self.toggleUserMenu();
      });

      this.userCollection.whoAmI(callback);
    },

    userLogout : function() {

      var userCallback = function(error) {
        if (error) {
          arangoHelper.arangoError("User", "Logout error");
        }
        else {
          this.userCollection.logout();
        } 
      }.bind(this);
      this.userCollection.whoAmI(userCallback);
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
      "click #createUser"                       : "createUser",
      "click #submitCreateUser"                 : "submitCreateUser",
//      "click #deleteUser"                   : "removeUser",
//      "click #submitDeleteUser"             : "submitDeleteUser",
//      "click .editUser"                     : "editUser",
//      "click .icon"                         : "editUser",
      "click #userManagementThumbnailsIn .tile" : "editUser",
      "click #submitEditUser"                   : "submitEditUser",
      "click #userManagementToggle"             : "toggleView",
      "keyup #userManagementSearchInput"        : "search",
      "click #userManagementSearchSubmit"       : "search",
      "click #callEditUserPassword"             : "editUserPassword",
      "click #submitEditUserPassword"           : "submitEditUserPassword",
      "click #submitEditCurrentUserProfile"     : "submitEditCurrentUserProfile",
      "click .css-label"                        : "checkBoxes",
      "change #userSortDesc"                    : "sorting"

    },

    dropdownVisible: false,

    initialize: function() {
      var self = this,
      callback = function(error, user) {
        if (error || user === null) {
          arangoHelper.arangoError("User", "Could not fetch user data");
        }
        else {
          this.currentUser = this.collection.findWhere({user: user});
        }
      }.bind(this);

      //fetch collection defined in router
      this.collection.fetch({
        success: function() {
          self.collection.whoAmI(callback);
        }
      });
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

      arangoHelper.setCheckboxStatus('#userManagementDropdown');

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
          arangoHelper.parseError("User", err, data);
        },
        success: function() {
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

      if ($(e.currentTarget).find('a').attr('id') === 'createUser') {
        return;
      }

      if ($(e.currentTarget).hasClass('tile')) {
        e.currentTarget = $(e.currentTarget).find('img');
      }

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
      if (str) {
        var index = str.lastIndexOf(substr);
        return str.substring(0, index);
      }
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

      var callback = function(error, data) {
        if (error) {
          arangoHelper.arangoError("User", "Could not verify old password");
        }
        else {
          if (data) {
            //check confirmation
            if (newPasswd !== confirmPasswd) {
              arangoHelper.arangoError("User", "New passwords do not match");
              hasError = true;
            }
            //check new password
            /*if (!this.validatePassword(newPasswd)) {
              $('#newCurrentPassword').closest("th").css("backgroundColor", "red");
              hasError = true;
            }*/

            if (!hasError) {
              this.currentUser.setPassword(newPasswd);
              arangoHelper.arangoNotification("User", "Password changed");
              window.modalView.hide();
            }
          }
        }
      }.bind(this);
      this.currentUser.checkPassword(oldPasswd, callback);
    },

    submitEditCurrentUserProfile: function() {
      var name    = $('#editCurrentName').val();
      var img     = $('#editCurrentUserProfileImg').val();
      img = this.parseImgString(img);


      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("User", "Could not edit user settings");
        }
        else {
          arangoHelper.arangoNotification("User", "Changes confirmed.");
          this.updateUserProfile();
        }
      }.bind(this);

      this.currentUser.setExtras(name, img, callback);
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
              rule: Joi.string().regex(/^[a-zA-Z0-9\-_]*$/),
              msg: 'Only symbols, "_" and "-" are allowed.'
            },
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

/*jshint browser: true */
/*jshint unused: false */
/*global Backbone, EJS, $, window, _ */
/*global _, arangoHelper, templateEngine, jQuery, Joi*/

(function () {
  "use strict";
  window.workMonitorView = Backbone.View.extend({

    el: '#content',
    id: '#workMonitorContent',

    template: templateEngine.createTemplate("workMonitorView.ejs"),
    table: templateEngine.createTemplate("arangoTable.ejs"),

    initialize: function () {
    },

    events: {
    },

    tableDescription: {
      id: "workMonitorTable",
      titles: [
        "Type", "Database", "Task ID", "Started", "Url", "User", "Description", "Method"
      ],
      rows: [],
      unescaped: [false, false, false, false, false, false, false, false]
    },

    render: function() {

      var self = this;

      this.$el.html(this.template.render({}));
      this.collection.fetch({
        success: function() {
          self.parseTableData();
          $(self.id).append(self.table.render({content: self.tableDescription}));
        }
      });
    },

    parseTableData: function() {

      var self = this;

      this.collection.each(function(model) {
        if (model.get('type') === 'AQL query') {

          var parent = model.get('parent');
          if (parent) {
            try {

              self.tableDescription.rows.push([
                model.get('type'),
                "(p) " + parent.database,
                "(p) " + parent.taskId,
                "(p) " + parent.startTime,
                "(p) " + parent.url,
                "(p) " + parent.user,
                model.get('description'),
                "(p) " + parent.method
              ]);
            }
            catch (e) {
              console.log("some parse error");
            }
          }

        }
        else if (model.get('type') !== 'thread') {
          self.tableDescription.rows.push([
            model.get('type'),
            model.get('database'),
            model.get('taskId'),
            model.get('startTime'),
            model.get('url'),
            model.get('user'),
            model.get('description'),
            model.get('method')
          ]);
        }
      });
    }

  });
}());

/*jshint unused: false */
/*global window, $, Backbone, document, arangoCollectionModel*/
/*global arangoHelper, btoa, dashboardView, arangoDatabase, _*/

(function () {
  "use strict";

  window.Router = Backbone.Router.extend({

    toUpdate: [],
    dbServers: [],
    isCluster: undefined,

    routes: {
      "": "cluster",
      "dashboard": "dashboard",
      "collections": "collections",
      "new": "newCollection",
      "login": "login",
      "collection/:colid/documents/:pageid": "documents",
      "cIndices/:colname": "cIndices",
      "cSettings/:colname": "cSettings",
      "cInfo/:colname": "cInfo",
      "collection/:colid/:docid": "document",
      "shell": "shell",
      "queries": "query",
      "workMonitor": "workMonitor",
      "databases": "databases",
      "settings": "databases",
      "services": "applications",
      "service/:mount": "applicationDetail",
      "graphs": "graphManagement",
      "graphs/:name": "showGraph",
      "users": "userManagement",
      "userProfile": "userProfile",
      "cluster": "cluster",
      "nodes": "cNodes",
      "cNodes": "cNodes",
      "dNodes": "dNodes",
      "node/:name": "node",
      //"nLogs/:name": "nLogs",
      "logs": "logs",
      "helpus": "helpUs"
    },

    execute: function(callback, args) {
      $('#subNavigationBar .breadcrumb').html('');
      $('#subNavigationBar .bottom').html('');
      $('#loadingScreen').hide();
      $('#content').show();
      if (callback) {
        callback.apply(this, args);
      }
    },

    checkUser: function () {
      if (window.location.hash === '#login') {
        return;
      }

      var callback = function(error, user) {
        if (error || user === null) {
          if (window.location.hash !== '#login') {
            this.navigate("login", {trigger: true});
          }
        }
        else {
          this.initOnce();

          //show hidden by default divs
          $('.bodyWrapper').show();
          $('.navbar').show();
        }
      }.bind(this);

      this.userCollection.whoAmI(callback); 
    },

    waitForInit: function(origin, param1, param2) {
      if (!this.initFinished) {
        setTimeout(function() {
          if (!param1) {
            origin(false);
          }
          if (param1 && !param2) {
            origin(param1, false);
          }
          if (param1 && param2) {
            origin(param1, param2, false);
          }
        }, 250);
      } else {
        if (!param1) {
          origin(true);
        }
        if (param1 && !param2) {
          origin(param1, true);
        }
        if (param1 && param2) {
          origin(param1, param2, true);
        }
      }
    },

    initFinished: false,

    initialize: function () {
      // This should be the only global object
      window.modalView = new window.ModalView();

      this.foxxList = new window.FoxxCollection();
      window.foxxInstallView = new window.FoxxInstallView({
        collection: this.foxxList
      });
      window.progressView = new window.ProgressView();

      var self = this;

      this.userCollection = new window.ArangoUsers();

      this.initOnce = function () {
        this.initOnce = function() {};

        var callback = function(error, isCoordinator) {
          self = this;
          if (isCoordinator) {
            self.isCluster = true;

            self.coordinatorCollection.fetch({
              success: function() {
                self.fetchDBS();
              }
            });
          }
          else {
            self.isCluster = false;
          }
        }.bind(this);

        window.isCoordinator(callback);

        this.initFinished = true;
        this.arangoDatabase = new window.ArangoDatabase();
        this.currentDB = new window.CurrentDatabase();

        this.arangoCollectionsStore = new window.arangoCollections();
        this.arangoDocumentStore = new window.arangoDocument();

        //Cluster 
        this.coordinatorCollection = new window.ClusterCoordinators();

        arangoHelper.setDocumentStore(this.arangoDocumentStore);

        this.arangoCollectionsStore.fetch();

        window.spotlightView = new window.SpotlightView({
          collection: this.arangoCollectionsStore 
        });

        this.footerView = new window.FooterView({
          collection: self.coordinatorCollection
        });
        this.notificationList = new window.NotificationCollection();

        this.currentDB.fetch({
          success: function() {
            self.naviView = new window.NavigationView({
              database: self.arangoDatabase,
              currentDB: self.currentDB,
              notificationCollection: self.notificationList,
              userCollection: self.userCollection,
              isCluster: self.isCluster
            });
            self.naviView.render();
          }
        });

        this.queryCollection = new window.ArangoQueries();

        this.footerView.render();

        window.checkVersion();
      }.bind(this);


      $(window).resize(function () {
        self.handleResize();
      });

      $(window).scroll(function () {
        //self.handleScroll();
      });

    },

    handleScroll: function() {
      if ($(window).scrollTop() > 50) {
        $('.navbar > .secondary').css('top', $(window).scrollTop());
        $('.navbar > .secondary').css('position', 'absolute');
        $('.navbar > .secondary').css('z-index', '10');
        $('.navbar > .secondary').css('width', $(window).width());
      }
      else {
        $('.navbar > .secondary').css('top', '0');
        $('.navbar > .secondary').css('position', 'relative');
        $('.navbar > .secondary').css('width', '');
      }
    },

    cluster: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.cluster.bind(this));
        return;
      }
      if (this.isCluster === false) {
        if (this.currentDB.get("name") === '_system') {
          this.routes[""] = 'dashboard';
          this.navigate("#dashboard", {trigger: true});
        }
        else {
          this.routes[""] = 'collections';
          this.navigate("#collections", {trigger: true});
        }
        return;
      }

      if (!this.clusterView) {
        this.clusterView = new window.ClusterView({
          coordinators: this.coordinatorCollection,
          dbServers: this.dbServers
        });
      }
      this.clusterView.render();
    },

    node: function (name, initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.node.bind(this), name);
        return;
      }
      if (this.isCluster === false) {
        this.routes[""] = 'dashboard';
        this.navigate("#dashboard", {trigger: true});
        return;
      }

      if (!this.nodeView) {
        this.nodeView = new window.NodeView({
          coordname: name,
          coordinators: this.coordinatorCollection,
          dbServers: this.dbServers
        });
      }
      this.nodeView.render();
    },

    cNodes: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.cNodes.bind(this));
        return;
      }
      if (this.isCluster === false) {
        this.routes[""] = 'dashboard';
        this.navigate("#dashboard", {trigger: true});
        return;
      }

      this.nodesView = new window.NodesView({
        coordinators: this.coordinatorCollection,
        dbServers: this.dbServers[0],
        toRender: 'coordinator'
      });
      this.nodesView.render();
    },

    dNodes: function (initialized) {
      this.checkUser();
      if (!initialized || this.isCluster === undefined) {
        this.waitForInit(this.dNodes.bind(this));
        return;
      }
      if (this.isCluster === false) {
        this.routes[""] = 'dashboard';
        this.navigate("#dashboard", {trigger: true});
        return;
      }

      this.nodesView = new window.NodesView({
        coordinators: this.coordinatorCollection,
        dbServers: this.dbServers[0],
        toRender: 'dbserver'
      });
      this.nodesView.render();
    },

    addAuth: function (xhr) {
      var u = this.clusterPlan.get("user");
      if (!u) {
        xhr.abort();
        if (!this.isCheckingUser) {
          this.requestAuth();
        }
        return;
      }
      var user = u.name;
      var pass = u.passwd;
      var token = user.concat(":", pass);
      xhr.setRequestHeader('Authorization', "Basic " + btoa(token));
    },

    logs: function (name, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.logs.bind(this), name);
        return;
      }
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
    },

    /*
    nLogs: function (nodename, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.nLogs.bind(this), nodename);
        return;
      }
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
      this.nLogsView = new window.LogsView({
        logall: newLogsAllCollection,
        logdebug: newLogsDebugCollection,
        loginfo: newLogsInfoCollection,
        logwarning: newLogsWarningCollection,
        logerror: newLogsErrorCollection
      });
      this.nLogsView.render();
    },
    */

    applicationDetail: function (mount, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.applicationDetail.bind(this), mount);
        return;
      }
      var callback = function() {
        if (!this.hasOwnProperty('applicationDetailView')) {
          this.applicationDetailView = new window.ApplicationDetailView({
            model: this.foxxList.get(decodeURIComponent(mount))
          });
        }

        this.applicationDetailView.model = this.foxxList.get(decodeURIComponent(mount));
        this.applicationDetailView.render('swagger');
      }.bind(this);

      if (this.foxxList.length === 0) {
        this.foxxList.fetch({
          success: function() {
            callback();
          }
        });
      }
      else {
        callback();
      }
    },

    login: function () {

      var callback = function(error, user) {
        if (error || user === null) {
          if (!this.loginView) {
            this.loginView = new window.loginView({
              collection: this.userCollection
            });
          }
          this.loginView.render();
        }
        else {
          this.navigate("", {trigger: true});
        }
      }.bind(this);

      this.userCollection.whoAmI(callback);
    },

    collections: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.collections.bind(this));
        return;
      }
      var self = this;
      if (!this.collectionsView) {
        this.collectionsView = new window.CollectionsView({
          collection: this.arangoCollectionsStore
        });
      }
      this.arangoCollectionsStore.fetch({
        success: function () {
          self.collectionsView.render();
        }
      });
    },

    cIndices: function (colname, initialized) {
      var self = this;

      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.cIndices.bind(this), colname);
        return;
      }
      this.arangoCollectionsStore.fetch({
        success: function () {
          self.indicesView = new window.IndicesView({
            collectionName: colname,
            collection: self.arangoCollectionsStore.findWhere({
              name: colname
            })
          });
          self.indicesView.render();
        }
      });
    },

    cSettings: function (colname, initialized) {
      var self = this;

      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.cSettings.bind(this), colname);
        return;
      }
      this.arangoCollectionsStore.fetch({
        success: function () {
          self.settingsView = new window.SettingsView({
            collectionName: colname,
            collection: self.arangoCollectionsStore.findWhere({
              name: colname
            })
          });
          self.settingsView.render();
        }
      });
    },

    cInfo: function (colname, initialized) {
      var self = this;

      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.cInfo.bind(this), colname);
        return;
      }
      this.arangoCollectionsStore.fetch({
        success: function () {
          self.infoView = new window.InfoView({
            collectionName: colname,
            collection: self.arangoCollectionsStore.findWhere({
              name: colname
            })
          });
          self.infoView.render();
        }
      });
    },

    documents: function (colid, pageid, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.documents.bind(this), colid, pageid);
        return;
      }
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

    document: function (colid, docid, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.document.bind(this), colid, docid);
        return;
      }
      if (!this.documentView) {
        this.documentView = new window.DocumentView({
          collection: this.arangoDocumentStore
        });
      }
      this.documentView.colid = colid;

      var doc = window.location.hash.split("/")[2];
      var test = (doc.split("%").length - 1) % 3;

      if (decodeURI(doc) !== doc && test !== 0) {
        doc = decodeURIComponent(doc);
      }
      this.documentView.docid = doc;

      this.documentView.render();

      var callback = function(error, type) {
        if (!error) {
          this.documentView.setType(type);
        }
        else {
          console.log("Error", "Could not fetch collection type");
        }
      }.bind(this);

      arangoHelper.collectionApiType(colid, null, callback);
    },

    shell: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.shell.bind(this));
        return;
      }
      if (!this.shellView) {
        this.shellView = new window.shellView();
      }
      this.shellView.render();
    },

    query: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.query.bind(this));
        return;
      }
      if (!this.queryView2) {
        this.queryView2 = new window.queryView2({
          collection: this.queryCollection
        });
      }
      this.queryView2.render();
    },
   
    /* 
    test: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.test.bind(this));
        return;
      }
      if (!this.testView) {
        this.testView = new window.testView({
        });
      }
      this.testView.render();
    },
    */
    helpUs: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.helpUs.bind(this));
        return;
      }
      if (!this.testView) {
        this.helpUsView = new window.HelpUsView({
        });
      }
      this.helpUsView.render();
    },

    workMonitor: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.workMonitor.bind(this));
        return;
      }
      if (!this.workMonitorCollection) {
        this.workMonitorCollection = new window.WorkMonitorCollection();
      }
      if (!this.workMonitorView) {
        this.workMonitorView = new window.workMonitorView({
          collection: this.workMonitorCollection
        });
      }
      this.workMonitorView.render();
    },

    queryManagement: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.queryManagement.bind(this));
        return;
      }
      if (!this.queryManagementView) {
        this.queryManagementView = new window.queryManagementView({
          collection: undefined
        });
      }
      this.queryManagementView.render();
    },

    databases: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.databases.bind(this));
        return;
      }

      var callback = function(error) {
        if (error) {
          arangoHelper.arangoError("DB","Could not get list of allowed databases");
          this.navigate("#", {trigger: true});
          $('#databaseNavi').css('display', 'none');
          $('#databaseNaviSelect').css('display', 'none');
        }
        else {
          if (! this.databaseView) {
            this.databaseView = new window.databaseView({
              users: this.userCollection,
              collection: this.arangoDatabase
            });
          }
          this.databaseView.render();
          }
      }.bind(this);

      arangoHelper.databaseAllowed(callback);
    },

    dashboard: function (initialized) {

      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.dashboard.bind(this));
        return;
      }

      if (this.dashboardView === undefined) {
        this.dashboardView = new window.DashboardView({
          dygraphConfig: window.dygraphConfig,
          database: this.arangoDatabase
        });
      }
      this.dashboardView.render();
    },

    graphManagement: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.graphManagement.bind(this));
        return;
      }
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
    },

    showGraph: function (name, initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.showGraph.bind(this), name);
        return;
      }
      if (!this.graphManagementView) {
        this.graphManagementView =
        new window.GraphManagementView(
          {
            collection: new window.GraphCollection(),
            collectionCollection: this.arangoCollectionsStore
          }
        );
        this.graphManagementView.render(name, true);
      }
      else {
        this.graphManagementView.loadGraphViewer(name);
      }
    },

    applications: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.applications.bind(this));
        return;
      }
      if (this.applicationsView === undefined) {
        this.applicationsView = new window.ApplicationsView({
          collection: this.foxxList
        });
      }
      this.applicationsView.reload();
    },

    handleSelectDatabase: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.handleSelectDatabase.bind(this));
        return;
      }
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
      if (this.queryView2) {
        this.queryView2.resize();
      }
      if (this.documentsView) {
        this.documentsView.resize();
      }
      if (this.documentView) {
        this.documentView.resize();
      }
    },

    userManagement: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.userManagement.bind(this));
        return;
      }
      if (!this.userManagementView) {
        this.userManagementView = new window.userManagementView({
          collection: this.userCollection
        });
      }
      this.userManagementView.render();
    },

    userProfile: function (initialized) {
      this.checkUser();
      if (!initialized) {
        this.waitForInit(this.userProfile.bind(this));
        return;
      }
      if (!this.userManagementView) {
        this.userManagementView = new window.userManagementView({
          collection: this.userCollection
        });
      }
      this.userManagementView.render(true);
    },
    
    fetchDBS: function() {
      var self = this;

      this.coordinatorCollection.each(function(coordinator) {
        self.dbServers.push(
          new window.ClusterServers([], {
            host: coordinator.get('address') 
          })
        );
      });
      _.each(this.dbServers, function(dbservers) {
        dbservers.fetch();
      });
    },

    getNewRoute: function(host) {
      return "http://" + host;
    },

    registerForUpdate: function(o) {
      this.toUpdate.push(o);
      o.updateUrl();
    }

  });

}());

/*jshint unused: false */
/*global $, window, navigator, _*/
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
    /*buttons.push(window.modalView.createNotificationButton("Don't ask again", function() {
      disableVersionCheck();
      window.modalView.hide();
    }));*/
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

  var getInformation = function() {
    var nVer = navigator.appVersion;
    var nAgt = navigator.userAgent;
    var browserName  = navigator.appName;
    var fullVersion  = '' + parseFloat(navigator.appVersion); 
    var majorVersion = parseInt(navigator.appVersion,10);
    var nameOffset,verOffset,ix;

    if ((verOffset=nAgt.indexOf("Opera")) !== -1) {
      browserName = "Opera";
      fullVersion = nAgt.substring(verOffset + 6);
      if ((verOffset=nAgt.indexOf("Version")) !== -1) { 
        fullVersion = nAgt.substring(verOffset + 8);
      }
    }
    else if ((verOffset=nAgt.indexOf("MSIE")) !== -1) {
      browserName = "Microsoft Internet Explorer";
      fullVersion = nAgt.substring(verOffset + 5);
    }
    else if ((verOffset=nAgt.indexOf("Chrome")) !== -1) {
      browserName = "Chrome";
      fullVersion = nAgt.substring(verOffset + 7);
    }
    else if ((verOffset=nAgt.indexOf("Safari")) !== -1) {
      browserName = "Safari";
      fullVersion = nAgt.substring(verOffset + 7);
      if ((verOffset=nAgt.indexOf("Version")) !== -1) {
        fullVersion = nAgt.substring(verOffset + 8);
      }
    }
    else if ((verOffset=nAgt.indexOf("Firefox")) !== -1) {
      browserName = "Firefox";
      fullVersion = nAgt.substring(verOffset + 8);
    }
    else if ((nameOffset=nAgt.lastIndexOf(' ') + 1) < (verOffset=nAgt.lastIndexOf('/'))) {
      browserName = nAgt.substring(nameOffset, verOffset);
      fullVersion = nAgt.substring(verOffset + 1);
      if (browserName.toLowerCase() === browserName.toUpperCase()) {
        browserName = navigator.appName;
      }
    }
    if ((ix=fullVersion.indexOf(";")) !== -1) {
      fullVersion=fullVersion.substring(0, ix);
    }
    if ((ix=fullVersion.indexOf(" ")) !== -1) {
      fullVersion=fullVersion.substring(0, ix);
    }

    majorVersion = parseInt('' + fullVersion, 10);

    if (isNaN(majorVersion)) {
      fullVersion  = '' + parseFloat(navigator.appVersion); 
      majorVersion = parseInt(navigator.appVersion, 10);
    }

    return {
      browserName: browserName,
      fullVersion: fullVersion,
      majorVersion: majorVersion,
      appName: navigator.appName,
      userAgent: navigator.userAgent
    };
  };

  var getOS = function() {
    var OSName = "Unknown OS";
    if (navigator.appVersion.indexOf("Win") !== -1) {
      OSName="Windows";
    }
    if (navigator.appVersion.indexOf("Mac") !== -1) {
      OSName="MacOS";
    }
    if (navigator.appVersion.indexOf("X11") !== -1) {
      OSName="UNIX";
    }
    if (navigator.appVersion.indexOf("Linux") !== -1) {
      OSName="Linux";
    }

    return OSName;
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
        $('.navbar #currentVersion').text(data.version.substr(0,3));

        window.parseVersions = function (json) {
          if (_.isEmpty(json)) {
            $('#currentVersion').addClass('up-to-date');
            return; // no new version.
          }
          $('#currentVersion').addClass('out-of-date');
          $('#currentVersion').click(function() {
            showInterface(currentVersion, json);
          });
          //isVersionCheckEnabled(showInterface.bind(window, currentVersion, json));
        };

        //TODO: append to url below
        /*
        var browserInfo = getInformation();
        console.log(browserInfo);
        console.log(encodeURIComponent(JSON.stringify(browserInfo)));

        var osInfo = getOS();
        console.log(osInfo);
        */

        $.ajax({
          type: "GET",
          async: true,
          crossDomain: true,
          timeout: 3000,
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

    //create only this one global event listener
    $(document).click(function(e) {
      e.stopPropagation();

      //hide user info dropdown if out of focus
      if (!$(e.target).hasClass('subBarDropdown')
          && !$(e.target).hasClass('dropdown-header')
          && !$(e.target).hasClass('dropdown-footer')
          && !$(e.target).hasClass('toggle')) {
        if ($('#userInfo').is(':visible')) {
          $('.subBarDropdown').hide();
        }
      }
    });
  }

}());
