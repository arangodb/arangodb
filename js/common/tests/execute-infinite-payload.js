/*jslint indent: 2,
         nomen: true,
         maxlen: 80,
         sloppy: true */
/*global require,
    db,
    assertEqual, assertTrue,
    print,
    PRINT_OBJECT,
    console,
    AvocadoCollection, AvocadoEdgesCollection */

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
/// @author Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

function main(args) {
  var Graph = require("org/arangodb/graph").Graph,
    graph_name = "UnitTestsCollectionGraph",
    vertex = "UnitTestsCollectionVertex",
    edge = "UnitTestsCollectionEdge",
    graph = null,
    base_path = args[1] + "/",
    console = require("console"),
    Helper = require("test-helper").Helper,
    query,
    internal = require("internal"),
    i;

////////////////////////////////////////////////////////////////////////////////
/// set up
////////////////////////////////////////////////////////////////////////////////

  try {
    try {
      // Drop the graph if it exsits
      graph = new Graph(graph_name);
      print("FOUND: ");
      PRINT_OBJECT(graph);
      graph.drop();
    } catch (err1) {
    }

    graph = new Graph(graph_name, vertex, edge);
  } catch (err2) {
    console.error("[FAILED] setup failed:" + err2);
  }

////////////////////////////////////////////////////////////////////////////////
/// Search for Payload
////////////////////////////////////////////////////////////////////////////////

  console.log("Importing");

  Helper.process(base_path + "generated_payload.csv", function (row) {
    graph.addVertex(row[0], {
      name : row[1],
      age  : parseInt(row[2], 10),
      bio  : row[3]
    });
  });

  db[vertex].ensureHashIndex("name");

  console.log("Starting Infinite Seach");

  query = "for x in " + vertex +
        " filter " + "x.name == 'John Doe'" +
        "return x.name";

  start_time = new Date();
    while (true) {
      internal.AQL_QUERY(query);
    }
  end_time = new Date();
  console.log((end_time - start_time) + " ms");


////////////////////////////////////////////////////////////////////////////////
/// tear down
////////////////////////////////////////////////////////////////////////////////

  try {
    if (graph !== null) {
      graph.drop();
    }
  } catch (err) {
    console.error("[FAILED] tear-down failed:" + err);
  }
}
