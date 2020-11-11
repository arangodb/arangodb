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
var startExecution = function(algo, data, params) {
  if (typeof algo !== 'string' || !data) {
    throw "Invalid parameters: _pregelStart(algorithm, graph, params)" +
          "<graph> can be either {vertexCollections:['',..], edgeCollection: ''}" +
          " or {graphName:'<graph>'} or graph name";
    ;
  }

  params = params || {};

  let db = internal.db;  
  if (typeof data === 'object' && !data.graphName) {
    let vcs;
    if (data.vertexCollection && typeof data.vertexCollection === 'string') {
      vcs = [data.vertexCollection];
    } else if (data.vertexCollections && data.vertexCollections instanceof Array) {
      vcs = data.vertexCollections;
    }
    let edges;
    if (typeof data.edgeCollection === 'string') {
      edges = [data.edgeCollection];
    } else if (data.edgeCollections && data.edgeCollections instanceof Array) {
      edges = data.edgeCollections;
    }
    if (!edges || !vcs) {
      throw "no vertex or edge collections specified";
    } 
    return db._pregelStart(algo, vcs, edges, params);    
  }

  let name;// name of graph
  if (typeof data === 'string') {
    name = data;
  } else if (data.graphName &&  typeof data.graphName === 'string') {
    name = data.graphName;
  }

  var graph_module;
  if (internal.isEnterprise()) {
    graph_module = require('@arangodb/smart-graph');
  } else {
    graph_module = require('@arangodb/general-graph');
  }
  let graph = graph_module._graph(name);
  if (graph) {
    let vertexCollections = [];
    let vcs = graph._vertexCollections(true);
    for (var key in vcs) {
      vertexCollections.push(vcs[key].name());
    }
    let edges = graph._edgeCollections();
    if (!params.hasOwnProperty('edgeCollectionRestrictions')) {
      let restrictions = {};
      (graph.__edgeDefinitions || []).forEach((ed) => {
        let edgeCollection = ed.collection;
        let from = ed.from;
        if (Array.isArray(from) && from.length > 0) {
          from.forEach((f) => {
            if (!restrictions[from]) {
              restrictions[from] = {};
            }
            restrictions[from][edgeCollection] = 1;
          });
        }
      });
      Object.keys(restrictions).forEach((v) => {
        restrictions[v] = Object.keys(restrictions[v]);
      });
      params.edgeCollectionRestrictions = restrictions;
    }
    if (edges.length > 0) {
      var edgeCollections = edges.map(e => e.name());
      return db._pregelStart(algo, vertexCollections,
                             edgeCollections, params);        
    } else {
      throw "No edge collection specified";
    }
  } else {
    throw 'invalid graph name';
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
