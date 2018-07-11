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

// overwrite graph class functions
ggc.__GraphClass._deleteEdgeDefinition = function (edgeDefinition, dropCollection) {
  return GeneralGraph._deleteEdgeDefinition(this.__name, edgeDefinition, dropCollection);
};

// new c++ based
exports._listObjects = GeneralGraph._listObjects;
exports._list = GeneralGraph._list;
exports._exists = GeneralGraph._exists;

exports._create = function (name, edgeDefinition, orphans, options) {
  let g = GeneralGraph._create(name, edgeDefinition, orphans, options);
  let graph = new ggc.__GraphClass(g.graph);
  graph._editEdgeDefinitions = function (edgeDefinitions) {
    let result = GeneralGraph._editEdgeDefinitions(name, edgeDefinitions);
    this.__edgeDefinitions = result.graph.edgeDefinitions;
  };
  graph._deleteEdgeDefinition = function (edgeDefinition, dropCollection) {
    let result = GeneralGraph._deleteEdgeDefinition(name, edgeDefinition, dropCollection);
    console.log(result.graph);
    this.__edgeDefinitions = result.graph.edgeDefinitions;
    this.__orphanCollections = result.graph.orphanCollections;
  };
  return graph;
};

exports._graph = function (graphName) {
  let g = GeneralGraph._graph(graphName);
  return new ggc.__GraphClass(g);
};

exports._drop = GeneralGraph._drop;
exports._renameCollection = GeneralGraph._renameCollection;

// js based helper functions
exports.__GraphClass = ggc.__GraphClass;
exports._edgeDefinitions = ggc._edgeDefinitions;
exports._extendEdgeDefinitions = ggc._extendEdgeDefinitions;
exports._relation = ggc._relation;
exports._registerCompatibilityFunctions = ggc._registerCompatibilityFunctions;
