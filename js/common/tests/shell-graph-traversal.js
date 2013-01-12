/*jslint indent: 2,
         nomen: true,
         maxlen: 80,
         sloppy: true */
/*global require,
    db,
    assertEqual, assertTrue */

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
var console = require("console");
var internal = require("internal");
var traversal = require("org/arangodb/graph/traversal");

// -----------------------------------------------------------------------------
// --SECTION--                                            graph traversal module
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test: Graph tree traversal
////////////////////////////////////////////////////////////////////////////////

function GraphTreeTraversalSuite () {
  var vertices;
  var edges;
      
  var visitor = traversal.TrackingVisitor;

  var filter = function (config, vertex, path) {
    var r = [ ];
    if (config.noVisit && config.noVisit[vertex._id]) {
      r.push(traversal.Traverser.EXCLUDE);
    }
    if (config.noExpand && config.noExpand[vertex._id]) {
      r.push(traversal.Traverser.PRUNE);
    }
    return r;
  };

  var expander = function (config, vertex, path) {
    var r = [ ];
    
    var edgesList = edges[vertex._id];
    if (edgesList != undefined) {
      for (i = 0; i < edgesList.length; ++i) {
        r.push({ edge: edgesList[i], vertex: vertices[edgesList[i]._to] });
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
        vertices: traversal.Traverser.UNIQUE_NONE,
        edges: traversal.Traverser.UNIQUE_NONE
      }, 
      visitor: visitor,
      filter: filter,
      expander: expander,

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
      vertices = { };
      edges    = { };
  
      [ "World", "Nothing",
        "Europe", "Asia", "America", "Australia", "Antarctica", "Africa", "Blackhole",
        "DE", "FR", "GB", "IE", "CN", "JP", "TW", "US", "MX", "AU", "EG", "ZA", "AN",
        "London", "Paris", "Lyon", "Cologne", "Dusseldorf", "Beijing", "Shanghai", "Tokyo", "Kyoto", "Taipeh", "Perth", "Sydney" 
      ].forEach(function (item) {
        var key = item;
        var vertex = {
          _id : "vertices/" + key,
          _key : key,
          name : item
        };
        vertices[vertex._id] = vertex;
      });

      var connect = function (from, to) {
        var key = from + "x" + to;
        var edge = {
          _id : "edges/" + key,
          _key : key, 
          _from : "vertices/" + from,
          _to : "vertices/" + to,
          what : from + "->" + to
        };

        if (edges[edge._from] == undefined) {
          edges[edge._from] = [ ];
        }
        edges[edge._from].push(edge);
      };

      connect("World", "Europe");
      connect("World", "Asia");
      connect("World", "America");
      connect("World", "Australia");
      connect("World", "Africa");
      connect("World", "Antarctica");
      connect("Europe", "DE"); 
      connect("Europe", "FR"); 
      connect("Europe", "GB"); 
      connect("Europe", "IE"); 
      connect("Asia", "CN"); 
      connect("Asia", "JP"); 
      connect("Asia", "TW"); 
      connect("America", "US"); 
      connect("America", "MX"); 
      connect("Australia", "AU"); 
      connect("Antarctica", "AN"); 
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
      config.strategy = traversal.Traverser.DEPTH_FIRST;
      config.order = traversal.Traverser.PRE_ORDER;
      config.itemOrder = traversal.Traverser.FORWARD;
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
      traverser.traverse(result, vertices["vertices/World"]);
      
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
        "vertices/AN",
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
      config.strategy = traversal.Traverser.DEPTH_FIRST;
      config.order = traversal.Traverser.PRE_ORDER;
      config.itemOrder = traversal.Traverser.BACKWARD;
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
      traverser.traverse(result, vertices["vertices/World"]);

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
      config.strategy = traversal.Traverser.DEPTH_FIRST;
      config.order = traversal.Traverser.POST_ORDER;
      config.itemOrder = traversal.Traverser.FORWARD;
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
      traverser.traverse(result, vertices["vertices/World"]);

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
      config.strategy = traversal.Traverser.DEPTH_FIRST;
      config.order = traversal.Traverser.POST_ORDER;
      config.itemOrder = traversal.Traverser.BACKWARD;
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
      traverser.traverse(result, vertices["vertices/World"]);

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
      config.strategy = traversal.Traverser.BREADTH_FIRST;
      config.order = traversal.Traverser.PRE_ORDER;
      config.itemOrder = traversal.Traverser.FORWARD;
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
      traverser.traverse(result, vertices["vertices/World"]);

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
      config.strategy = traversal.Traverser.BREADTH_FIRST;
      config.order = traversal.Traverser.PRE_ORDER;
      config.itemOrder = traversal.Traverser.BACKWARD;
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
      traverser.traverse(result, vertices["vertices/World"]);
      
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
      config.strategy = traversal.Traverser.BREADTH_FIRST;
      config.order = traversal.Traverser.POST_ORDER;
      config.itemOrder = traversal.Traverser.FORWARD;
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
      traverser.traverse(result, vertices["vertices/World"]);

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
      config.strategy = traversal.Traverser.BREADTH_FIRST;
      config.order = traversal.Traverser.POST_ORDER;
      config.itemOrder = traversal.Traverser.BACKWARD;
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
      traverser.traverse(result, vertices["vertices/World"]);

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
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                        collection graph traversal
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test: collection-based graph traversal
////////////////////////////////////////////////////////////////////////////////

function CollectionTraversalSuite () {
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

      [ [ "A", "B" ], [ "B", "C" ], [ "C", "D" ], [ "A", "D" ], [ "D", "E" ], [ "D", "F" ], [ "B", "G" ], [ "B", "I" ], [ "G", "H" ], [ "I", "H"] ].forEach(function (item) {
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
        sort: function (l, r) { return l._key < r._key ? -1 : 1 },
        edgeCollection: edgeCollection
      }; 

      var expander = traversal.CollectionOutboundExpander;
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
        sort: function (l, r) { return l._key < r._key ? -1 : 1 },
        edgeCollection: edgeCollection
      }; 

      var expander = traversal.CollectionInboundExpander;
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
        edgeCollection: internal.db._collection(en),
        strategy: traversal.Traverser.DEPTH_FIRST,
        order: traversal.Traverser.PRE_ORDER,
        itemOrder: traversal.Traverser.FORWARD,
        filter: traversal.VisitAllFilter,
        expander: traversal.CollectionOutboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1 }
      };
      
      var traverser = new traversal.Traverser(config);
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
        edgeCollection: internal.db._collection(en),
        strategy: traversal.Traverser.DEPTH_FIRST,
        order: traversal.Traverser.PRE_ORDER,
        itemOrder: traversal.Traverser.FORWARD,
        filter: traversal.VisitAllFilter,
        expander: traversal.CollectionInboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1 }
      };
     
      var result = getResult(); 
      var traverser = new traversal.Traverser(config);
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

    testIterateUniqueVertices : function () {
      var config = { 
        edgeCollection: internal.db._collection(en),
        strategy: traversal.Traverser.DEPTH_FIRST,
        order: traversal.Traverser.PRE_ORDER,
        itemOrder: traversal.Traverser.FORWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_GLOBAL,
          edges: traversal.Traverser.UNIQUE_NONE
        }, 
        filter: traversal.VisitAllFilter,
        expander: traversal.CollectionOutboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1 }
      };
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
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

    testIterateUniqueEdges : function () {
      var config = { 
        edgeCollection: internal.db._collection(en),
        strategy: traversal.Traverser.DEPTH_FIRST,
        order: traversal.Traverser.PRE_ORDER,
        itemOrder: traversal.Traverser.FORWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_NONE,
          edges: traversal.Traverser.UNIQUE_GLOBAL
        }, 
        filter: traversal.VisitAllFilter,
        expander: traversal.CollectionOutboundExpander,

        sort: function (l, r) { return l._key < r._key ? -1 : 1 }
      };
      
      var result = getResult();
      var traverser = new traversal.Traverser(config);
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

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(GraphTreeTraversalSuite);
jsunity.run(CollectionTraversalSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
