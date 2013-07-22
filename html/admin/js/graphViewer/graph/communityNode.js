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
  
  if (_.isUndefined(parent) || !_.isFunction(parent.dissolveCommunity)) {
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
        return 2 * def;
      }
      return def;
    },
  
    getCharge = function(def) {
      if (self._expanded) {
        return 8 * def;
      }
      return def;
    },
  
    getSourcePosition = function(e) {    
      if (self._expanded) {
        var p = self.position,
          diff = e._source,
          x = p.x + diff.x,
          y = p.y + diff.y,
          z = p.z + diff.z;
        return {
          x: x,
          y: y,
          z: z
        };
      }
      return self.position;
    },
  
  
    getTargetPosition = function(e) {
      if (self._expanded) {
        var p = self.position,
          diff = e._target,
          x = p.x + diff.x,
          y = p.y + diff.y,
          z = p.z + diff.z;
        return {
          x: x,
          y: y,
          z: z
        };
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
    
    addDistortion = function() {
      // Fake Layouting TODO
      _.each(nodeArray, function(n) {
        n.position = {
          x: n.x,
          y: n.y,
          z: 1
        };
      });
    },
    
    addShape = function (g, shapeFunc, colourMapper) {
      g.attr("stroke", colourMapper.getForegroundCommunityColour());
      shapeFunc(g);
    },
    
    addCollapsedShape = function(g, shapeFunc, colourMapper) {
      g.attr("stroke", colourMapper.getForegroundCommunityColour());
      shapeFunc(g, 9);
      shapeFunc(g, 6);
      shapeFunc(g, 3);
      shapeFunc(g);
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
    
    addNodeShapes = function(g, shapeFunc, colourMapper) {
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
      addShape(interior, shapeFunc, colourMapper);
    },
    
    addBoundingBox = function(g) {
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
      var dissolveBtn = bBox.append("rect")
        .attr("fill", "red") // TODO
        .attr("width", "16")
        .attr("height", "16")
        .attr("x", "5")
        .attr("y", "2")
        .attr("style", "cursor:pointer")
        .on("click", dissolve),
      collapseBtn = bBox.append("rect")
        .attr("fill", "red") // TODO
        .attr("width", "16")
        .attr("height", "16")
        .attr("x", "25")
        .attr("y", "2")
        .attr("style", "cursor:pointer")
        .on("click", collapse),
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
    
    shapeAll = function(g, shapeFunc, colourMapper) {
      if (self._expanded) {
        addBoundingBox(g);
        addDistortion();
        addNodeShapes(g, shapeFunc, colourMapper);
        return;
      }
      addCollapsedShape(g, shapeFunc, colourMapper);
      addCollapsedLabel(g, colourMapper);
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
  
  _.each(initial, function(n) {
    insertNode(n);
  });
  
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
  
  this.dissolve = dissolve;
  this.getDissolveInfo = getDissolveInfo;
  
  this.collapse = collapse;
  this.expand = expand;
  
  this.shape = shapeAll;
  
  // TODO TMP
  this.getSourcePosition = getSourcePosition;
  
  this.getTargetPosition = getTargetPosition;
}
