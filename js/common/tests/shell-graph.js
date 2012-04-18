/*jslint indent: 2,
         nomen: true,
         maxlen: 80 */
/*global require, db, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the graph class
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Graph Basics
////////////////////////////////////////////////////////////////////////////////

function graphBasicsSuite() {
  "use strict";
  //var ERRORS = require("internal").errors;
  var Graph = require("graph").Graph,
    vertex = "UnitTestsCollectionVertex",
    edge = "UnitTestsCollectionEdge",
    graph = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(vertex);
      db._drop(edge);

      graph = new Graph(vertex, edge);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(vertex);
      db._drop(edge);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief create a vertex
////////////////////////////////////////////////////////////////////////////////

    testCreateVertex : function () {
      var v = graph.addVertex("name1", { age : 23 });

      assertEqual("name1", v.getId());
      assertEqual(23, v.getProperty("age"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief change a property
////////////////////////////////////////////////////////////////////////////////

    testChangeProperty : function () {
      var v = graph.addVertex("name2", { age : 32 });

      assertEqual("name2", v.getId());
      assertEqual(32, v.getProperty("age"));

      v.setProperty("age", 23);

      assertEqual("name2", v.getId());
      assertEqual(23, v.getProperty("age"));
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(graphBasicsSuite);
jsunity.done();