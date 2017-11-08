/* jshint strict: false */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Simon Grätzer
// / @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');

// //////////////////////////////////////////////////////////////////////////////
// / @brief creates a new pregel execution
// //////////////////////////////////////////////////////////////////////////////
var startExecution = function(algo, graph, params) {
  if (typeof algo !== 'string' || !graph) {
    throw "Invalid parameters: pregel(algorithm, graph, params)" +
          "<graph> can be either {vertexCollections:['',..], edgeCollection: ''}" +
          " or {graphName:'<graph>'} or graph name";
    ;
  }

  // check start vertex
  // -----------------------------------------
  if (!algo || typeof algo !== 'string') {
    throw "Invalid parameters: pregel(<algorithm>, <graph>, <params>)"
  }
  params = params || {};

  let db = internal.db;  
  if (typeof graph === 'object' && !graph.graphName) {
    let vcs;
    if (graph.vertexCollection && typeof graph.vertexCollection === 'string') {
      vcs = [graph.vertexCollection];
    } else if (graph.vertexCollections && graph.vertexCollections instanceof Array) {
      vcs = graph.vertexCollections;
    }
    let edges;
    if (typeof graph.edgeCollection === 'string') {
      edges = [graph.edgeCollection]
    } else if (graph.edgeCollections && graph.edgeCollections instanceof Array) {
      edges = graph.edgeCollections;
    }
    if (!edges || !vcs) {
      throw "no vertex or edge collections specified";
    } 
    return db._pregelStart(algo, vcs, edges, params);    
  }

  let name;// name of graph
  if (typeof graph === 'string') {
    name = graph
  } else if (graph.graphName &&  typeof graph.graphName === 'string') {
    name = graph.graphName;
  }

  var graph_module;
  if (internal.isEnterprise()) {
    graph_module = require('@arangodb/smart-graph');
  } else {
    graph_module = require('@arangodb/general-graph');
  }
  var graph = graph_module._graph(name);
  if (graph) {
    let vertexCollections = [];
    let vcs = graph._vertexCollections(true);
    for (var key in vcs) {
      vertexCollections.push(vcs[key].name());
    }
    let edges = graph._edgeCollections();
    if (edges.length > 0) {
      var edgeCollections = edges.map(e => e.name());
      return db._pregelStart(algo, vertexCollections,
                             edgeCollections, params);        
    } else {
      throw "No edge collection specified";
    }
  } else {
    throw 'invalid graphname';
  }
};

exports.start = startExecution;
exports.status = function (executionID) {
  let db = internal.db;
  return db._pregelStatus(executionID);
};
exports.cancel = function (executionID) {
  let db = internal.db;
  return db._pregelCancel(executionID);
};
