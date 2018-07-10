/* global ArangoGeneralGraph */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Replication management
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2012 triagens GmbH, Cologne, Germany
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
// / @author Heiko Kernbach
// / @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////


const internal = require('internal'); // OK: reloadAuth
const ggc = require('@arangodb/general-graph-common');
const GeneralGraph = internal.ArangoGeneralGraph;
//const arangodb = require("@arangodb");

// new c++ based
exports._listObjects = GeneralGraph._listObjects;

exports._create = function (name, edgeDefinition, orphans, options) {
  let g = GeneralGraph._create(name, edgeDefinition, orphans, options);
  let graph = new ggc.__GraphClass(g.graph);
  graph._editEdgeDefinitions = function (edgeDefinitions) {
    let result = GeneralGraph._editEdgeDefinitions(name, edgeDefinitions);
    this.__edgeDefinitions = result.graph.edgeDefinitions;
  };
  return graph;
};
exports._graph = function (graphName) {
  let g = GeneralGraph._graph(graphName);
  return new ggc.__GraphClass(g);
}

// old js based
exports._drop = ggc._drop;
exports._exists = ggc._exists;
exports._edgeDefinitions = ggc._edgeDefinitions;
exports._extendEdgeDefinitions = ggc._extendEdgeDefinitions;
exports._list = ggc._list;
exports._relation = ggc._relation;
