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
const GeneralGraph = internal.ArangoGeneralGraphModule;

// This is supposed to be a class
const ArangoGraph = internal.ArangoGraph;

// inherited graph class
let CommonGraph = ggc.__GraphClass;

// new c++ based
CommonGraph.prototype.__updateDefinitions = function (edgeDefs, orphans) {
  this.__edgeDefinitions = edgeDefs;
  this.__orphanCollections = orphans;
};

CommonGraph.prototype._deleteEdgeDefinition = function (edgeDefinition, dropCollection = false) {
  let result = ArangoGraph._deleteEdgeDefinition(this.__name, edgeDefinition, dropCollection);
  this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
};

CommonGraph.prototype._extendEdgeDefinitions = function (edgeDefinitions) {
  let result = ArangoGraph._extendEdgeDefinitions(this.__name, edgeDefinitions);
  this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
};

CommonGraph.prototype._editEdgeDefinitions = function (edgeDefinitions) {
  let result = ArangoGraph._editEdgeDefinitions(this.__name, edgeDefinitions);
  this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
};

CommonGraph.prototype._addVertexCollection = function (vertexName, createCollection = true) {
  let result = ArangoGraph._addVertexCollection(this.__name, vertexName, createCollection);
  this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
};

CommonGraph.prototype._removeVertexCollection = function (vertexName, dropCollection = false) {
  let result = ArangoGraph._removeVertexCollection(this.__name, vertexName, dropCollection);
  this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
};


exports._create = function (name, edgeDefinition, orphans, options) {
  let g = GeneralGraph._create(name, edgeDefinition, orphans, options);
  return new CommonGraph(g.graph);
};

exports._drop = GeneralGraph._drop;

exports._exists = GeneralGraph._exists;

exports._graph = function (graphName) {
  let g = GeneralGraph._graph(graphName);
  return new CommonGraph(g);
};

exports._list = GeneralGraph._list;

exports._listObjects = GeneralGraph._listObjects;

exports._renameCollection = GeneralGraph._renameCollection;

// js based helper functions
exports.__GraphClass = CommonGraph;
exports._edgeDefinitions = ggc._edgeDefinitions;
exports._extendEdgeDefinitions = ggc._extendEdgeDefinitions;
exports._relation = ggc._relation;
exports._registerCompatibilityFunctions = ggc._registerCompatibilityFunctions;
