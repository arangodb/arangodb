/*jslint indent: 2, nomen: true, maxlen: 100, white: true  plusplus: true */
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
      var dissolveBtn = bBox.append("image")
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
        }),
      collapseBtn = bBox.append("image")
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
        }),
      title = bBox.append("text")
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
