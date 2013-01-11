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
/// @brief test: Graph Traversal
////////////////////////////////////////////////////////////////////////////////

function GraphTraversalSuite() {
  var vertices;
  var edges;

  var visitor = function (traverser, vertex, path) {
    var context = traverser._context;

    context.visited.push(vertex._id);
    var paths = [ ];
    path.vertices.forEach (function (vertex) {
      paths.push(vertex._id);
    });
    context.paths.push(paths);
  };

  var filter = function (traverser, vertex, path) {
    var context = traverser._context;

    return {
      visit: ((context.visit && context.visit[vertex._id] != undefined) ? context.visit[vertex._id] : true),
      expand: ((context.expand && context.expand[vertex._id] != undefined) ? context.expand[vertex._id] : true)
    };
  };

  var expander = function (traverser, vertex, path) {
    var result = [ ];
    
    var edgesList = edges[vertex._id];
    if (edgesList != undefined) {
      for (i = 0; i < edgesList.length; ++i) {
        result.push({ edge: edgesList[i], vertex: vertices[edgesList[i]._to] });
      }
    }
    return result;
  }

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      vertices = { };
      edges    = { };
  
      [ "World", "Nothing",
        "Europe", "Asia", "America", "Australia", "Antarctica", "Blackhole",
        "DE", "FR", "GB", "CN", "JP", "TW", "US", "MX", "AU", 
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
      connect("World", "Antarctica");
      connect("Europe", "DE"); 
      connect("Europe", "FR"); 
      connect("Europe", "GB"); 
      connect("Asia", "CN"); 
      connect("Asia", "JP"); 
      connect("Asia", "TW"); 
      connect("America", "US"); 
      connect("America", "MX"); 
      connect("Australia", "AU"); 
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
      var properties = { 
        strategy: traversal.Traverser.DEPTH_FIRST,
        visitOrder: traversal.Traverser.PRE_ORDER,
        itemOrder: traversal.Traverser.FORWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_NONE,
          edges: traversal.Traverser.UNIQUE_NONE
        }, 
        visitor: visitor,
        filter: filter,
        expander: expander
      };
      
      var traverser = new traversal.Traverser("edges", properties);
      var context = {
        visited: [ ],
        paths: [ ],
        visit: {
          "vertices/Antarctica": false
        }
      }; 
  
      traverser.traverse(vertices["vertices/World"], context);

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
        "vertices/AU"
      ];

      assertEqual(expectedVisits, context.visited);

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
        [ "vertices/World", "vertices/Australia", "vertices/AU" ]
      ];
      
      assertEqual(expectedPaths, context.paths);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test depth-first pre-order backward visitation
////////////////////////////////////////////////////////////////////////////////

    testDepthFirstPreOrderBackward : function () {
      var properties = { 
        strategy: traversal.Traverser.DEPTH_FIRST,
        visitOrder: traversal.Traverser.PRE_ORDER,
        itemOrder: traversal.Traverser.BACKWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_NONE,
          edges: traversal.Traverser.UNIQUE_NONE
        }, 
        visitor: visitor,
        filter: filter,
        expander: expander
      };
      
      var traverser = new traversal.Traverser("edges", properties);
      var context = {
        visited: [ ],
        paths: [ ],
        visit: {
          "vertices/Antarctica": false
        }
      }; 
  
      traverser.traverse(vertices["vertices/World"], context);

      var expectedVisits = [
        "vertices/World",
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

      assertEqual(expectedVisits, context.visited);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test depth-first post-order forward visitation
////////////////////////////////////////////////////////////////////////////////

    testDepthFirstPostOrderForward : function () {
      var properties = { 
        strategy: traversal.Traverser.DEPTH_FIRST,
        visitOrder: traversal.Traverser.POST_ORDER,
        itemOrder: traversal.Traverser.FORWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_NONE,
          edges: traversal.Traverser.UNIQUE_NONE
        }, 
        visitor: visitor,
        filter: filter,
        expander: expander
      };
      
      var traverser = new traversal.Traverser("edges", properties);
      var context = {
        visited: [ ],
        paths: [ ],
        visit: {
          "vertices/Antarctica" : false
        }
      }; 
  
      traverser.traverse(vertices["vertices/World"], context);

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
        "vertices/World"
      ];

      assertEqual(expectedVisits, context.visited);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test depth-first post-order backward visitation
////////////////////////////////////////////////////////////////////////////////

    testDepthFirstPostOrderBackward : function () {
      var properties = { 
        strategy: traversal.Traverser.DEPTH_FIRST,
        visitOrder: traversal.Traverser.POST_ORDER,
        itemOrder: traversal.Traverser.BACKWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_NONE,
          edges: traversal.Traverser.UNIQUE_NONE
        }, 
        visitor: visitor,
        filter: filter,
        expander: expander
      };
      
      var traverser = new traversal.Traverser("edges", properties);
      var context = {
        visited: [ ],
        paths: [ ],
        visit: {
          "vertices/Antarctica" : false
        }
      }; 
  
      traverser.traverse(vertices["vertices/World"], context);

      var expectedVisits = [
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

      assertEqual(expectedVisits, context.visited);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test breadth-first pre-order forward visitation
////////////////////////////////////////////////////////////////////////////////

    testBreadthFirstPreOrderForward : function () {
      var properties = { 
        strategy: traversal.Traverser.BREADTH_FIRST,
        visitOrder: traversal.Traverser.PRE_ORDER,
        itemOrder: traversal.Traverser.FORWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_NONE,
          edges: traversal.Traverser.UNIQUE_NONE
        }, 
        visitor: visitor,
        filter: filter,
        expander: expander
      };
      
      var traverser = new traversal.Traverser("edges", properties);
      var context = {
        visited: [ ],
        paths: [ ],
        visit: {
          "vertices/Antarctica" : false
        }
      }; 
  
      traverser.traverse(vertices["vertices/World"], context);

      var expectedVisits = [
        "vertices/World",
        "vertices/Europe",
        "vertices/Asia",
        "vertices/America",
        "vertices/Australia",
        "vertices/DE",
        "vertices/FR",
        "vertices/GB",
        "vertices/CN",
        "vertices/JP",
        "vertices/TW",
        "vertices/US",
        "vertices/MX",
        "vertices/AU"
      ];

      assertEqual(expectedVisits, context.visited);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test breadth-first pre-order backward visitation
////////////////////////////////////////////////////////////////////////////////

    testBreadthFirstPreOrderBackward : function () {
      var properties = { 
        strategy: traversal.Traverser.BREADTH_FIRST,
        visitOrder: traversal.Traverser.PRE_ORDER,
        itemOrder: traversal.Traverser.BACKWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_NONE,
          edges: traversal.Traverser.UNIQUE_NONE
        }, 
        visitor: visitor,
        filter: filter,
        expander: expander
      };
      
      var traverser = new traversal.Traverser("edges", properties);
      var context = {
        visited: [ ],
        paths: [ ],
        visit: {
          "vertices/Antarctica" : false
        }
      }; 
  
      traverser.traverse(vertices["vertices/World"], context);

      var expectedVisits = [
        "vertices/World",
        "vertices/Australia",
        "vertices/America",
        "vertices/Asia",
        "vertices/Europe",
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

      assertEqual(expectedVisits, context.visited);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test breadth-first post-order forward visitation
////////////////////////////////////////////////////////////////////////////////

    testBreadthFirstPostOrderForward : function () {
      var properties = { 
        strategy: traversal.Traverser.BREADTH_FIRST,
        visitOrder: traversal.Traverser.POST_ORDER,
        itemOrder: traversal.Traverser.FORWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_NONE,
          edges: traversal.Traverser.UNIQUE_NONE
        }, 
        visitor: visitor,
        filter: filter,
        expander: expander
      };
      
      var traverser = new traversal.Traverser("edges", properties);
      var context = {
        visited: [ ],
        paths: [ ],
        visit: {
          "vertices/Antarctica" : false
        }
      }; 
  
      traverser.traverse(vertices["vertices/World"], context);

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
        "vertices/Europe",
        "vertices/Asia",
        "vertices/America",
        "vertices/Australia",
        "vertices/World"
      ];

      assertEqual(expectedVisits, context.visited);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test breadth-first post-order backward visitation
////////////////////////////////////////////////////////////////////////////////

    testBreadthFirstPostOrderBackward : function () {
      var properties = { 
        strategy: traversal.Traverser.BREADTH_FIRST,
        visitOrder: traversal.Traverser.POST_ORDER,
        itemOrder: traversal.Traverser.BACKWARD,
        uniqueness: {
          vertices: traversal.Traverser.UNIQUE_NONE,
          edges: traversal.Traverser.UNIQUE_NONE
        }, 
        visitor: visitor,
        filter: filter,
        expander: expander
      };
      
      var traverser = new traversal.Traverser("edges", properties);
      var context = {
        visited: [ ],
        paths: [ ],
        visit: {
          "vertices/Antarctica" : false
        }
      }; 
  
      traverser.traverse(vertices["vertices/World"], context);

      var expectedVisits = [
        "vertices/AU",
        "vertices/MX",
        "vertices/US",
        "vertices/TW",
        "vertices/JP",
        "vertices/CN",
        "vertices/GB",
        "vertices/FR",
        "vertices/DE",
        "vertices/Australia",
        "vertices/America",
        "vertices/Asia",
        "vertices/Europe",
        "vertices/World"
      ];

      assertEqual(expectedVisits, context.visited);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(GraphTraversalSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
