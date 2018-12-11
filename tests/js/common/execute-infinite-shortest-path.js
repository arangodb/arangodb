/* jshint globalstrict:false, strict:false, unused:false */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the graph class
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Lucas Dohmen
// / @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

function main (args) {
  'use strict';
  var Graph = require('@arangodb/graph').Graph,
    graph_name = 'UnitTestsCollectionGraph',
    vertex = 'UnitTestsCollectionVertex',
    edge = 'UnitTestsCollectionEdge',
    graph = null,
    base_path = args[1] + '/',
    v1,
    v2,
    e,
    pathes,
    console = require('console'),
    Helper = require('test-helper').Helper,
    caching = false;

  if (args[2] === 'true') {
    caching = true;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / set up
  // //////////////////////////////////////////////////////////////////////////////

  try {
    try {
      // Drop the graph if it exsits
      graph = new Graph(graph_name);
      require('internal').print('FOUND: ');
      require('internal').printObject(graph);
      graph.drop();
    } catch (err1) {}

    graph = new Graph(graph_name, vertex, edge);
  } catch (err2) {
    console.error('[FAILED] setup failed:' + err2);
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / compare with Neo4j on a given network
  // //////////////////////////////////////////////////////////////////////////////

  console.log('Importing');

  Helper.process(base_path + 'generated_edges.csv', function (row) {
    v1 = graph.getOrAddVertex(row[1]);
    v2 = graph.getOrAddVertex(row[2]);
    e = graph.addEdge(v1, v2);
  });

  console.log('Starting Tests');

  var processor = function (row) {
    v1 = graph.getVertex(row[0]);
    v2 = graph.getVertex(row[1]);

    if (v1 !== null && v2 !== null) {
      pathes = v1.pathTo(v2, { cached: caching });
    }
  };

  while (true) {
    Helper.process(base_path + 'generated_testcases.csv', processor);
    console.log('Round Finished');
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / tear down
  // //////////////////////////////////////////////////////////////////////////////

  try {
    if (graph !== null) {
      graph.drop();
    }
  } catch (err) {
    console.error('[FAILED] tear-down failed:' + err);
  }
}
