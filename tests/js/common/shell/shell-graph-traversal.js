/*jshint globalstrict:false, strict:false */
/*global assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the graph traversal class
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("@arangodb");
var traversal = require("@arangodb/graph/traversal");
var generalGraph = require("@arangodb/general-graph");

var db = arangodb.db;
var Traverser = traversal.Traverser;

////////////////////////////////////////////////////////////////////////////////
/// @brief create in-memory data-source
////////////////////////////////////////////////////////////////////////////////

function memoryDatasource (nodes, connections) {
  'use strict';
  var vertices = { };
  var inEdges  = { };
  var outEdges = { };

  // create vertices
  nodes.forEach(function (key) {
    vertices["vertices/" + key] = {
      _id: "vertices/" + key,
      _key: key,
      name: key
    };
  });

  // create connections
  var connect = function (from, to, label) {
    var key = from + "x" + to;

    var edge = {
      _id : "edges/" + key,
      _key : key,
      _from : "vertices/" + from,
      _to : "vertices/" + to,
      what : from + "->" + to
    };

    if (typeof label !== "undefined") {
      edge.label = label;
    }

    if (outEdges[edge._from] === undefined) {
      outEdges[edge._from] = [];
    }

    outEdges[edge._from].push(edge);

    if (inEdges[edge._to] === undefined) {
      inEdges[edge._to] = [];
    }

    inEdges[edge._to].push(edge);
  };

  connections.forEach(function (c) {
    connect(c[0], c[1], c[2]);
  });

  // return the datasource
  return {
    inEdges: inEdges,
    outEdges: outEdges,
    vertices: vertices,

    getVertexId: function (vertex) {
      return vertex._id;
    },

    getPeerVertex: function (edge, vertex) {
      if (edge._from === vertex._id) {
        return this.getInVertex(edge);
      }

      if (edge._to === vertex._id) {
        return this.getOutVertex(edge);
      }

      return null;
    },

    getInVertex: function (edge) {
      return this.vertices[edge._to];
    },

    getOutVertex: function (edge) {
      return this.vertices[edge._from];
    },

    getEdgeId: function (edge) {
      return edge._id;
    },

    getLabel: function (edge) {
      return edge.label;
    },

    getAllEdges: function (vertex) {
      return this.inEdges[vertex._id].concat(outEdges[vertex._id]);
    },

    getInEdges: function (vertex) {
      return this.inEdges[vertex._id];
    },

    getOutEdges: function (vertex) {
      return this.outEdges[vertex._id];
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test: in-memory graph traversal
////////////////////////////////////////////////////////////////////////////////

function MemoryTraversalSuite () {
  'use strict';
  var datasourceWorld;
  var datasourcePeople;

  var setUpSourceWorld = function () {
    var nodes = [
      "World", "Nothing", "Europe", "Asia", "America", "Australia", "Antarctica",
      "Africa", "Blackhole", "DE", "FR", "GB", "IE", "CN", "JP", "TW", "US",
      "MX", "AU", "EG", "ZA", "AN", "London", "Paris", "Lyon", "Cologne",
      "Dusseldorf", "Beijing", "Shanghai", "Tokyo", "Kyoto", "Taipeh", "Perth",
      "Sydney"
    ];

    var connections = [
      ["World", "Europe"],
      ["World", "Asia"],
      ["World", "America"],
      ["World", "Australia"],
      ["World", "Africa"],
      ["World", "Antarctica"],
      ["Europe", "DE"],
      ["Europe", "FR"],
      ["Europe", "GB"],
      ["Europe", "IE"],
      ["Asia", "CN"],
      ["Asia", "JP"],
      ["Asia", "TW"],
      ["America", "US"],
      ["America", "MX"],
      ["Australia", "AU"],
      ["Antarctica", "AN"]
    ];

    datasourceWorld = memoryDatasource(nodes, connections);
  };

  var setUpSourcePeople = function () {
    var nodes = [
      "Alice", "Bob", "Charly", "Diana", "Eric", "Frank"
    ];

    var connections = [
      ["Alice", "Bob", "likes"],
      ["Bob", "Alice", "likes"],
      ["Alice", "Diana", "hates"],
      ["Alice", "Eric", "hates"],
      ["Eric", "Alice","hates"],
      ["Bob", "Charly", "likes"],
      ["Charly", "Diana", "hates"],
      ["Diana", "Charly", "hates"],
      ["Diana", "Alice", "likes"],
      ["Diana", "Eric", "likes"],
      ["Alice", "Frank", "l"],
      ["Frank", "Bob", "likes"]
    ];

    datasourcePeople = memoryDatasource(nodes, connections);
  };


  var visitor = traversal.trackingVisitor;

  var filter = function (config, vertex) {
    var r = [ ];

    if (config.noVisit && config.noVisit[vertex._id]) {
      r.push(Traverser.EXCLUDE);
    }

    if (config.noExpand && config.noExpand[vertex._id]) {
      r.push(Traverser.PRUNE);
    }

    return r;
  };

  var expander = function (config, vertex) {
    var r = [ ];
    var edgesList = config.datasource.getOutEdges(vertex);
    var i;

    if (edgesList !== undefined) {
      for (i = 0; i < edgesList.length; ++i) {
        r.push({ edge: edgesList[i], vertex: config.datasource.vertices[edgesList[i]._to] });
      }
    }
    return r;
  };

  var getIds = function (data) {
    var r = [ ];

    data.forEach(function (item) {
      r.push(item._id);
    });

    return r;
  };

  var getVisitedPaths = function (data) {
    var r = [ ];

    data.forEach(function (item) {
      r.push(getIds(item.vertices));
    });

    return r;
  };

  var getResult = function () {
    return {
      visited: {
        vertices: [ ],
        paths: [ ]
      }
    };
  };

  var getConfig = function () {
    return {
      uniqueness: {
        vertices: Traverser.UNIQUE_NONE,
        edges: Traverser.UNIQUE_NONE
      },

      visitor: visitor,
      filter: filter,
      expander: expander,
      datasource: datasourceWorld,

      noVisit: {
        "vertices/Antarctica" : true,
        "vertices/IE": true
      },

      noExpand: {
        "vertices/Africa": true
      }
    };
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      setUpSourceWorld();
      setUpSourcePeople();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test depth-first pre-order forward visitation
////////////////////////////////////////////////////////////////////////////////

    testDepthFirstPreOrderForward : function () {
      var config = getConfig();
      config.strategy = Traverser.DEPTH_FIRST;
      config.order = Traverser.PRE_ORDER;
      config.itemOrder = Traverser.FORWARD;

      var result = getResult();
      var traverser = new Traverser(config);

      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/World",
        "vertices/Europe",
        "vertices/DE",
        "vertices/FR",
        "vertices/GB",
        "vertices/Asia",
        "vertices/CN",
        "vertices/JP",
        "vertices/TW",
        "vertices/America",
        "vertices/US",
        "vertices/MX",
        "vertices/Australia",
        "vertices/AU",
        "vertices/Africa",
        "vertices/AN"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World" ],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test depth-first pre-order backward visitation
////////////////////////////////////////////////////////////////////////////////

    testDepthFirstPreOrderBackward : function () {
      var config = getConfig();
      config.strategy = Traverser.DEPTH_FIRST;
      config.order = Traverser.PRE_ORDER;
      config.itemOrder = Traverser.BACKWARD;

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/World",
        "vertices/AN",
        "vertices/Africa",
        "vertices/Australia",
        "vertices/AU",
        "vertices/America",
        "vertices/MX",
        "vertices/US",
        "vertices/Asia",
        "vertices/TW",
        "vertices/JP",
        "vertices/CN",
        "vertices/Europe",
        "vertices/GB",
        "vertices/FR",
        "vertices/DE"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World" ],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/DE" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test depth-first post-order forward visitation
////////////////////////////////////////////////////////////////////////////////

    testDepthFirstPostOrderForward : function () {
      var config = getConfig();
      config.strategy = Traverser.DEPTH_FIRST;
      config.order = Traverser.POST_ORDER;
      config.itemOrder = Traverser.FORWARD;

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/DE",
        "vertices/FR",
        "vertices/GB",
        "vertices/Europe",
        "vertices/CN",
        "vertices/JP",
        "vertices/TW",
        "vertices/Asia",
        "vertices/US",
        "vertices/MX",
        "vertices/America",
        "vertices/AU",
        "vertices/Australia",
        "vertices/Africa",
        "vertices/AN",
        "vertices/World"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ],
        [ "vertices/World" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test depth-first post-order backward visitation
////////////////////////////////////////////////////////////////////////////////

    testDepthFirstPostOrderBackward : function () {
      var config = getConfig();
      config.strategy = Traverser.DEPTH_FIRST;
      config.order = Traverser.POST_ORDER;
      config.itemOrder = Traverser.BACKWARD;

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/AN",
        "vertices/Africa",
        "vertices/AU",
        "vertices/Australia",
        "vertices/MX",
        "vertices/US",
        "vertices/America",
        "vertices/TW",
        "vertices/JP",
        "vertices/CN",
        "vertices/Asia",
        "vertices/GB",
        "vertices/FR",
        "vertices/DE",
        "vertices/Europe",
        "vertices/World"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test breadth-first pre-order forward visitation
////////////////////////////////////////////////////////////////////////////////

    testBreadthFirstPreOrderForward : function () {
      var config = getConfig();
      config.strategy = Traverser.BREADTH_FIRST;
      config.order = Traverser.PRE_ORDER;
      config.itemOrder = Traverser.FORWARD;

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/World",
        "vertices/Europe",
        "vertices/Asia",
        "vertices/America",
        "vertices/Australia",
        "vertices/Africa",
        "vertices/DE",
        "vertices/FR",
        "vertices/GB",
        "vertices/CN",
        "vertices/JP",
        "vertices/TW",
        "vertices/US",
        "vertices/MX",
        "vertices/AU",
        "vertices/AN"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World" ],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test breadth-first pre-order backward visitation
////////////////////////////////////////////////////////////////////////////////

    testBreadthFirstPreOrderBackward : function () {
      var config = getConfig();
      config.strategy = Traverser.BREADTH_FIRST;
      config.order = Traverser.PRE_ORDER;
      config.itemOrder = Traverser.BACKWARD;

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/World",
        "vertices/Africa",
        "vertices/Australia",
        "vertices/America",
        "vertices/Asia",
        "vertices/Europe",
        "vertices/AN",
        "vertices/AU",
        "vertices/MX",
        "vertices/US",
        "vertices/TW",
        "vertices/JP",
        "vertices/CN",
        "vertices/GB",
        "vertices/FR",
        "vertices/DE"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/DE" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test breadth-first post-order forward visitation
////////////////////////////////////////////////////////////////////////////////

    testBreadthFirstPostOrderForward : function () {
      var config = getConfig();
      config.strategy = Traverser.BREADTH_FIRST;
      config.order = Traverser.POST_ORDER;
      config.itemOrder = Traverser.FORWARD;

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/DE",
        "vertices/FR",
        "vertices/GB",
        "vertices/CN",
        "vertices/JP",
        "vertices/TW",
        "vertices/US",
        "vertices/MX",
        "vertices/AU",
        "vertices/AN",
        "vertices/Europe",
        "vertices/Asia",
        "vertices/America",
        "vertices/Australia",
        "vertices/Africa",
        "vertices/World"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test breadth-first post-order backward visitation
////////////////////////////////////////////////////////////////////////////////

    testBreadthFirstPostOrderBackward : function () {
      var config = getConfig();
      config.strategy = Traverser.BREADTH_FIRST;
      config.order = Traverser.POST_ORDER;
      config.itemOrder = Traverser.BACKWARD;

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/AN",
        "vertices/AU",
        "vertices/MX",
        "vertices/US",
        "vertices/TW",
        "vertices/JP",
        "vertices/CN",
        "vertices/GB",
        "vertices/FR",
        "vertices/DE",
        "vertices/Africa",
        "vertices/Australia",
        "vertices/America",
        "vertices/Asia",
        "vertices/Europe",
        "vertices/World"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minimal depth filter with depth 0
////////////////////////////////////////////////////////////////////////////////

    testMinDepthFilterWithDepth0 : function () {
      var config = {
        datasource: datasourceWorld,
        expander: expander,
        filter: traversal.minDepthFilter,
        minDepth: 0
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/World",
        "vertices/Europe",
        "vertices/DE",
        "vertices/FR",
        "vertices/GB",
        "vertices/IE",
        "vertices/Asia",
        "vertices/CN",
        "vertices/JP",
        "vertices/TW",
        "vertices/America",
        "vertices/US",
        "vertices/MX",
        "vertices/Australia",
        "vertices/AU",
        "vertices/Africa",
        "vertices/Antarctica",
        "vertices/AN"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World"],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Europe", "vertices/IE" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Antarctica"],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minimal depth filter with depth 1
////////////////////////////////////////////////////////////////////////////////

    testMinDepthFilterWithDepth1 : function () {
      var config = {
        datasource: datasourceWorld,
        expander: expander,
        filter: traversal.minDepthFilter,
        minDepth: 1
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/Europe",
        "vertices/DE",
        "vertices/FR",
        "vertices/GB",
        "vertices/IE",
        "vertices/Asia",
        "vertices/CN",
        "vertices/JP",
        "vertices/TW",
        "vertices/America",
        "vertices/US",
        "vertices/MX",
        "vertices/Australia",
        "vertices/AU",
        "vertices/Africa",
        "vertices/Antarctica",
        "vertices/AN"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Europe", "vertices/IE" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Antarctica"],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test minimal depth filter with depth 2
////////////////////////////////////////////////////////////////////////////////

    testMinDepthFilterWithDepth2 : function () {
      var config = {
        datasource: datasourceWorld,
        expander: expander,
        filter: traversal.minDepthFilter,
        minDepth: 2
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/DE",
        "vertices/FR",
        "vertices/GB",
        "vertices/IE",
        "vertices/CN",
        "vertices/JP",
        "vertices/TW",
        "vertices/US",
        "vertices/MX",
        "vertices/AU",
        "vertices/AN"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Europe", "vertices/IE" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test maximal depth filter with depth 0
////////////////////////////////////////////////////////////////////////////////

    testMaxDepthFilterWithDepth0 : function () {
      var config = {
        datasource: datasourceWorld,
        expander: expander,
        filter: traversal.maxDepthFilter,
        maxDepth: 0
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/World"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test maximal depth filter with depth 1
////////////////////////////////////////////////////////////////////////////////

    testMaxDepthFilterWithDepth1 : function () {
      var config = {
        datasource: datasourceWorld,
        expander: expander,
        filter: traversal.maxDepthFilter,
        maxDepth: 1
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/World",
        "vertices/Europe",
        "vertices/Asia",
        "vertices/America",
        "vertices/Australia",
        "vertices/Africa",
        "vertices/Antarctica"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World"],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Antarctica"]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test maximal depth filter with depth 2
////////////////////////////////////////////////////////////////////////////////

    testMaxDepthFilterWithDepth2 : function () {
      var config = {
        datasource: datasourceWorld,
        expander: expander,
        filter: traversal.maxDepthFilter,
        maxDepth: 2
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/World",
        "vertices/Europe",
        "vertices/DE",
        "vertices/FR",
        "vertices/GB",
        "vertices/IE",
        "vertices/Asia",
        "vertices/CN",
        "vertices/JP",
        "vertices/TW",
        "vertices/America",
        "vertices/US",
        "vertices/MX",
        "vertices/Australia",
        "vertices/AU",
        "vertices/Africa",
        "vertices/Antarctica",
        "vertices/AN"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World"],
        [ "vertices/World", "vertices/Europe" ],
        [ "vertices/World", "vertices/Europe", "vertices/DE" ],
        [ "vertices/World", "vertices/Europe", "vertices/FR" ],
        [ "vertices/World", "vertices/Europe", "vertices/GB" ],
        [ "vertices/World", "vertices/Europe", "vertices/IE" ],
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/Asia", "vertices/CN" ],
        [ "vertices/World", "vertices/Asia", "vertices/JP" ],
        [ "vertices/World", "vertices/Asia", "vertices/TW" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Australia", "vertices/AU" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Antarctica"],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test filter by given key value pairs
////////////////////////////////////////////////////////////////////////////////

    testIncludeMatchingAttributesFilter : function () {
      // Can be removed as soon as all expanders use datasource
      var anyExp = function (config, vertex) {
        var result = [ ];
        var edgesList = config.datasource.getAllEdges(vertex);

        if (edgesList !== undefined) {
          var i;

          for (i = 0; i < edgesList.length; ++i) {
            result.push({ edge: edgesList[i], vertex: config.datasource.vertices[edgesList[i]._to] });
          }
        }
        return result;
      };

      var config = {
        uniqueness: {
          vertices: Traverser.UNIQUE_GLOBAL,
          edges: Traverser.UNIQUE_NONE
        },
        matchingAttributes: [{"name": "Alice"},
                             {"name": "Frank"},
                             {"name": "Diana", "key": "FAIL"},
                             {"_id": "vertices/Bob"}],
        visitor: visitor,
        expander: anyExp,
        filter: traversal.includeMatchingAttributesFilter,
        datasource: datasourcePeople
      };

      var result = getResult();
      var traverser = new Traverser(config);

      traverser.traverse(result, config.datasource.vertices["vertices/Alice"]);

      var expectedVisits = [
        "vertices/Alice",
        "vertices/Bob",
        "vertices/Frank"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test combination of filters
////////////////////////////////////////////////////////////////////////////////

    testCombineFilters : function () {
      var excluder1 = function(config, vertex) {
        if (vertex.name && vertex.name === config.exclude1) { return "exclude"; }
      };

      var excluder2 = function(config, vertex) {
        if (vertex.name && vertex.name === config.exclude2) { return "exclude"; }
      };

      var excluder3 = function(config, vertex) {
        if (vertex.name && vertex.name === config.exclude3) { return "exclude"; }
      };

      var pruner1 = function(config, vertex) {
        if (vertex.name && vertex.name === config.prune1) { return "prune"; }
      };

      var pruner2 = function(config, vertex) {
        if (vertex.name && vertex.name === config.prune2) { return "prune"; }
      };

      var config = {
        expander: expander,
        filter: [
          excluder1,
          pruner1,
          excluder2,
          pruner2,
          excluder3
        ],
        exclude1: "Europe",
        exclude2: "AU",
        exclude3: "World",
        prune1: "Asia",
        prune2: "Europe",
        datasource: datasourceWorld
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [
        "vertices/Asia",
        "vertices/America",
        "vertices/US",
        "vertices/MX",
        "vertices/Australia",
        "vertices/Africa",
        "vertices/Antarctica",
        "vertices/AN"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/World", "vertices/Asia" ],
        [ "vertices/World", "vertices/America" ],
        [ "vertices/World", "vertices/America", "vertices/US" ],
        [ "vertices/World", "vertices/America", "vertices/MX" ],
        [ "vertices/World", "vertices/Australia" ],
        [ "vertices/World", "vertices/Africa" ],
        [ "vertices/World", "vertices/Antarctica"],
        [ "vertices/World", "vertices/Antarctica", "vertices/AN" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if exclude or prune can be overridden in combined filters
////////////////////////////////////////////////////////////////////////////////

    testOverrideExcludeAndPruneOfCombinedFilters : function () {
      var excludeAndPrune = function(config, vertex) {
        if (vertex.name && vertex.name === config.excludeAndPrune) { return ["prune", "exclude"]; }
      };

      var config = {
        expander: expander,
        filter: [
          excludeAndPrune,
          traversal.visitAllFilter
        ],
        excludeAndPrune: "World",
        datasource: datasourceWorld
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, config.datasource.vertices["vertices/World"]);

      var expectedVisits = [];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if all edges with one label are followed
////////////////////////////////////////////////////////////////////////////////

    testFollowEdgesWithLabels : function () {
      var config = {
        uniqueness: {
          vertices: Traverser.UNIQUE_GLOBAL,
          edges: Traverser.UNIQUE_NONE
        },
        labels: "likes",
        visitor: visitor,
        expander: traversal.expandEdgesWithLabels,
        datasource: datasourcePeople
      };

      var result = getResult();
      var traverser = new Traverser(config);

      traverser.traverse(result, config.datasource.vertices["vertices/Alice"]);

      var expectedVisits = [
        "vertices/Alice",
        "vertices/Bob",
        "vertices/Frank",
        "vertices/Charly",
        "vertices/Diana",
        "vertices/Eric"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/Alice"],
        [ "vertices/Alice", "vertices/Bob" ],
        [ "vertices/Alice", "vertices/Bob", "vertices/Frank" ],
        [ "vertices/Alice", "vertices/Bob", "vertices/Charly" ],
        [ "vertices/Alice", "vertices/Diana" ],
        [ "vertices/Alice", "vertices/Diana", "vertices/Eric" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if all and only inbound edges with one label are followed
////////////////////////////////////////////////////////////////////////////////

    testFollowInEdgesWithLabels : function () {
      var config = {
        uniqueness: {
          vertices: Traverser.UNIQUE_GLOBAL,
          edges: Traverser.UNIQUE_NONE
        },
        labels: "likes",
        visitor: visitor,
        expander: traversal.expandInEdgesWithLabels,
        datasource: datasourcePeople
      };

      var result = getResult();
      var traverser = new Traverser(config);

      traverser.traverse(result, config.datasource.vertices["vertices/Alice"]);

      var expectedVisits = [
        "vertices/Alice",
        "vertices/Bob",
        "vertices/Frank",
        "vertices/Diana"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/Alice"],
        [ "vertices/Alice", "vertices/Bob" ],
        [ "vertices/Alice", "vertices/Bob", "vertices/Frank" ],
        [ "vertices/Alice", "vertices/Diana" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test if all and only outbound edges with one label are followed
////////////////////////////////////////////////////////////////////////////////

    testFollowOutEdgesWithLabels : function () {
      var config = {
        uniqueness: {
          vertices: Traverser.UNIQUE_GLOBAL,
          edges: Traverser.UNIQUE_NONE
        },
        labels: "likes",
        visitor: visitor,
        expander: traversal.expandOutEdgesWithLabels,
        datasource: datasourcePeople
      };

      var result = getResult();
      var traverser = new Traverser(config);

      traverser.traverse(result, config.datasource.vertices["vertices/Alice"]);

      var expectedVisits = [
        "vertices/Alice",
        "vertices/Bob",
        "vertices/Charly"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));

      var expectedPaths = [
        [ "vertices/Alice"],
        [ "vertices/Alice", "vertices/Bob" ],
        [ "vertices/Alice", "vertices/Bob", "vertices/Charly" ]
      ];

      assertEqual(expectedPaths, getVisitedPaths(result.visited.paths));
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief test: collection-based graph traversal
////////////////////////////////////////////////////////////////////////////////

function CollectionTraversalSuite () {
  'use strict';
  var vn = "UnitTestsVertices";
  var en = "UnitTestsEdges";

  var vertexCollection;
  var edgeCollection;

  var getResult = function () {
    return {
      visited: {
        vertices: [ ],
        paths: [ ]
      }
    };
  };

  var getIds = function (data) {
    var r = [ ];
    data.forEach(function (item) {
      r.push(item._id);
    });
    return r;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(vn);
      db._drop(en);

      vertexCollection = db._create(vn);
      edgeCollection = db._createEdgeCollection(en);

      [ "A", "B", "C", "D", "E", "F", "G", "H", "I" ].forEach(function (item) {
        vertexCollection.save({ _key: item, name: item });
      });

      [ [ "A", "B" ],
        [ "B", "C" ],
        [ "C", "D" ],
        [ "A", "D" ],
        [ "D", "E" ],
        [ "D", "F" ],
        [ "B", "G" ],
        [ "B", "I" ],
        [ "G", "H" ],
        [ "I", "H"] ].forEach(function (item) {
        var l = item[0];
        var r = item[1];
        edgeCollection.save(vn + "/" + l, vn + "/" + r, { _key: l + r, what : l + "->" + r });
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(vn);
      db._drop(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test outbound expander
////////////////////////////////////////////////////////////////////////////////

    testOutboundExpander : function () {
      var config = {
        sort: function (l, r) { return l._key < r._key ? -1 : 1; },
        datasource: traversal.collectionDatasourceFactory(edgeCollection)
      };

      var expander = traversal.outboundExpander;
      var connected;

      connected = [ ];
      expander(config, vertexCollection.document(vn + "/A")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ "B", "D" ], connected);

      connected = [ ];
      expander(config, vertexCollection.document(vn + "/D")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ "E", "F" ], connected);

      connected = [ ];
      expander(config, vertexCollection.document(vn + "/H")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ ], connected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test inbound expander
////////////////////////////////////////////////////////////////////////////////

    testInboundExpander : function () {
      var config = {
        sort: function (l, r) { return l._key < r._key ? -1 : 1; },
        datasource: traversal.collectionDatasourceFactory(edgeCollection)
      };

      var expander = traversal.inboundExpander;
      var connected;

      connected = [ ];
      expander(config, vertexCollection.document(vn + "/D")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ "A", "C" ], connected);

      connected = [ ];
      expander(config, vertexCollection.document(vn + "/H")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ "G", "I" ], connected);

      connected = [ ];
      expander(config, vertexCollection.document(vn + "/A")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ ], connected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testIterateFullOutbound : function () {
      var config = {
        datasource: traversal.collectionDatasourceFactory(db._collection(en)),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        filter: traversal.visitAllFilter,
        expander: traversal.outboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var traverser = new Traverser(config);
      var result = getResult();
      traverser.traverse(result, vertexCollection.document(vn + "/A"));

      var expectedVisits = [
        vn + "/A",
        vn + "/B",
        vn + "/C",
        vn + "/D",
        vn + "/E",
        vn + "/F",
        vn + "/G",
        vn + "/H",
        vn + "/I",
        vn + "/H",
        vn + "/D",
        vn + "/E",
        vn + "/F"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testIterateInbound : function () {
      var config = {
        datasource: traversal.collectionDatasourceFactory(db._collection(en)),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        filter: traversal.visitAllFilter,
        expander: traversal.inboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, vertexCollection.document(vn + "/F"));

      var expectedVisits = [
        vn + "/F",
        vn + "/D",
        vn + "/A",
        vn + "/C",
        vn + "/B",
        vn + "/A"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testIterateUniqueGlobalVertices : function () {
      var config = {
        datasource: traversal.collectionDatasourceFactory(db._collection(en)),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        uniqueness: {
          vertices: Traverser.UNIQUE_GLOBAL,
          edges: Traverser.UNIQUE_NONE
        },
        filter: traversal.visitAllFilter,
        expander: traversal.outboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, vertexCollection.document(vn + "/A"));

      var expectedVisits = [
        vn + "/A",
        vn + "/B",
        vn + "/C",
        vn + "/D",
        vn + "/E",
        vn + "/F",
        vn + "/G",
        vn + "/H",
        vn + "/I"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testIterateUniquePathVertices : function () {
      var config = {
        datasource: traversal.collectionDatasourceFactory(db._collection(en)),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        uniqueness: {
          vertices: Traverser.UNIQUE_PATH,
          edges: Traverser.UNIQUE_NONE
        },
        filter: traversal.visitAllFilter,
        expander: traversal.outboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, vertexCollection.document(vn + "/A"));

      var expectedVisits = [
        vn + "/A",
        vn + "/B",
        vn + "/C",
        vn + "/D",
        vn + "/E",
        vn + "/F",
        vn + "/G",
        vn + "/H",
        vn + "/I",
        vn + "/H",
        vn + "/D",
        vn + "/E",
        vn + "/F"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testIterateUniqueEdges : function () {
      var config = {
        datasource: traversal.collectionDatasourceFactory(db._collection(en)),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        uniqueness: {
          vertices: Traverser.UNIQUE_NONE,
          edges: Traverser.UNIQUE_GLOBAL
        },
        filter: traversal.visitAllFilter,
        expander: traversal.outboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, vertexCollection.document(vn + "/A"));

      var expectedVisits = [
        vn + "/A",
        vn + "/B",
        vn + "/C",
        vn + "/D",
        vn + "/E",
        vn + "/F",
        vn + "/G",
        vn + "/H",
        vn + "/I",
        vn + "/H",
        vn + "/D"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    }
  };
}



////////////////////////////////////////////////////////////////////////////////
/// @brief test: general-graph-based graph traversal
////////////////////////////////////////////////////////////////////////////////

function GeneralGraphTraversalSuite () {
  'use strict';
  // Definition of the edges: A -> BDEH <-> CFGI
  var vnA = "UnitTestsVertices1";
  var enDir = "UnitTestsEdges1";
  var vnBDH = "UnitTestsVertices2";
  var enUndir = "UnitTestsEdges2";
  var vnCEFGI = "UnitTestsVertices3";

  var gn = "UnitTestsGraph";

  var g;

  var getResult = function () {
    return {
      visited: {
        vertices: [ ],
        paths: [ ]
      }
    };
  };

  var getIds = function (data) {
    var r = [ ];
    data.forEach(function (item) {
      r.push(item._id);
    });
    return r;
  };
  var saveVertex = function(colName, key) {
    g[colName].save({ _key: key, name: key });
  };
  var saveEdge = function(edgeCol, fromCol, toCol, nodePair) {
    var l = nodePair[0];
    var r = nodePair[1];
    g[edgeCol].save(fromCol + "/" + l, toCol + "/" + r, {_key: l + r, what: l + "->" + r });
  };

  var cleanUp = function() {
    db._drop(vnA);
    db._drop(vnBDH);
    db._drop(vnCEFGI);
    db._drop(enDir);
    db._drop(enUndir);
    if (db._graphs.exists(gn)) {
      db._graphs.remove(gn);
    }
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      cleanUp();

      var edgeDef = [];
      edgeDef.push(generalGraph._relation(enDir, vnA, vnBDH));
      edgeDef.push(generalGraph._relation(enUndir, [vnBDH, vnCEFGI], [vnBDH, vnCEFGI]));
      g = generalGraph._create(gn, edgeDef);

      saveVertex(vnA, "A");
      [ "B", "D", "H" ].forEach(function (item) {
        saveVertex(vnBDH, item);
      });
      [ "C", "E", "F", "G", "I" ].forEach(function (item) {
        saveVertex(vnCEFGI, item);
      });

      [ [ "A", "B" ], [ "A", "D" ] ].forEach(function (item) {
        saveEdge(enDir, vnA, vnBDH, item);
      });
      [ [ "B", "C" ], [ "D", "E" ], [ "D", "F" ], [ "B", "G" ], [ "B", "I" ] ].forEach(function (item) {
        saveEdge(enUndir, vnBDH, vnCEFGI, item);
      });
      [ [ "C", "D" ], [ "G", "H" ], [ "I", "H"] ].forEach(function (item) {
        saveEdge(enUndir, vnCEFGI, vnBDH, item);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      cleanUp();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test outbound expander
////////////////////////////////////////////////////////////////////////////////

    testGGOutboundExpander : function () {
      var config = {
        sort: function (l, r) { return l._key < r._key ? -1 : 1; },
        datasource: traversal.generalGraphDatasourceFactory(gn)
      };

      var expander = traversal.outboundExpander;
      var connected;

      connected = [ ];
      expander(config, g[vnA].document("A")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ "B", "D" ], connected);

      connected = [ ];
      expander(config, g[vnBDH].document("D")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ "E", "F" ], connected);

      connected = [ ];
      expander(config, g[vnBDH].document("H")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ ], connected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test inbound expander
////////////////////////////////////////////////////////////////////////////////

    testGGInboundExpander : function () {
      var config = {
        sort: function (l, r) { return l._key < r._key ? -1 : 1; },
        datasource: traversal.generalGraphDatasourceFactory(gn)
      };

      var expander = traversal.inboundExpander;
      var connected;

      connected = [ ];
      expander(config, g[vnBDH].document("D")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ "A", "C" ], connected);

      connected = [ ];
      expander(config, g[vnBDH].document("H")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ "G", "I" ], connected);

      connected = [ ];
      expander(config, g[vnA].document("A")).forEach(function(item) {
        connected.push(item.vertex._key);
      });

      assertEqual([ ], connected);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testGGIterateFullOutbound : function () {
      var config = {
        datasource: traversal.generalGraphDatasourceFactory(gn),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        filter: traversal.visitAllFilter,
        expander: traversal.outboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var traverser = new Traverser(config);
      var result = getResult();
      traverser.traverse(result, g[vnA].document("A"));

      var expectedVisits = [
        vnA + "/A",
        vnBDH + "/B",
        vnCEFGI + "/C",
        vnBDH + "/D",
        vnCEFGI + "/E",
        vnCEFGI + "/F",
        vnCEFGI + "/G",
        vnBDH + "/H",
        vnCEFGI + "/I",
        vnBDH + "/H",
        vnBDH + "/D",
        vnCEFGI + "/E",
        vnCEFGI + "/F"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testGGIterateInbound : function () {
      var config = {
        datasource: traversal.generalGraphDatasourceFactory(gn),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        filter: traversal.visitAllFilter,
        expander: traversal.inboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, g[vnCEFGI].document("F"));

      var expectedVisits = [
        vnCEFGI + "/F",
        vnBDH + "/D",
        vnA + "/A",
        vnCEFGI + "/C",
        vnBDH + "/B",
        vnA + "/A"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testGGIterateUniqueGlobalVertices : function () {
      var config = {
        datasource: traversal.generalGraphDatasourceFactory(gn),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        uniqueness: {
          vertices: Traverser.UNIQUE_GLOBAL,
          edges: Traverser.UNIQUE_NONE
        },
        filter: traversal.visitAllFilter,
        expander: traversal.outboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, g[vnA].document("A"));

      var expectedVisits = [
        vnA + "/A",
        vnBDH + "/B",
        vnCEFGI + "/C",
        vnBDH + "/D",
        vnCEFGI + "/E",
        vnCEFGI + "/F",
        vnCEFGI + "/G",
        vnBDH + "/H",
        vnCEFGI + "/I"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testGGIterateUniquePathVertices : function () {
      var config = {
        datasource: traversal.generalGraphDatasourceFactory(gn),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        uniqueness: {
          vertices: Traverser.UNIQUE_PATH,
          edges: Traverser.UNIQUE_NONE
        },
        filter: traversal.visitAllFilter,
        expander: traversal.outboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, g[vnA].document("A"));

      var expectedVisits = [
        vnA + "/A",
        vnBDH + "/B",
        vnCEFGI + "/C",
        vnBDH + "/D",
        vnCEFGI + "/E",
        vnCEFGI + "/F",
        vnCEFGI + "/G",
        vnBDH + "/H",
        vnCEFGI + "/I",
        vnBDH + "/H",
        vnBDH + "/D",
        vnCEFGI + "/E",
        vnCEFGI + "/F"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test iteration
////////////////////////////////////////////////////////////////////////////////

    testGGIterateUniqueEdges : function () {
      var config = {
        datasource: traversal.generalGraphDatasourceFactory(gn),
        strategy: Traverser.DEPTH_FIRST,
        order: Traverser.PRE_ORDER,
        itemOrder: Traverser.FORWARD,
        uniqueness: {
          vertices: Traverser.UNIQUE_NONE,
          edges: Traverser.UNIQUE_GLOBAL
        },
        filter: traversal.visitAllFilter,
        expander: traversal.outboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1; }
      };

      var result = getResult();
      var traverser = new Traverser(config);
      traverser.traverse(result, g[vnA].document("A"));

      var expectedVisits = [
        vnA + "/A",
        vnBDH + "/B",
        vnCEFGI + "/C",
        vnBDH + "/D",
        vnCEFGI + "/E",
        vnCEFGI + "/F",
        vnCEFGI + "/G",
        vnBDH + "/H",
        vnCEFGI + "/I",
        vnBDH + "/H",
        vnBDH + "/D"
      ];

      assertEqual(expectedVisits, getIds(result.visited.vertices));
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(MemoryTraversalSuite);
jsunity.run(CollectionTraversalSuite);
jsunity.run(GeneralGraphTraversalSuite);

return jsunity.done();

