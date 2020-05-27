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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Heiko Kernbach
// / @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arangosh = require('@arangodb/arangosh');
const ggc = require('@arangodb/general-graph-common');
const _ = require('lodash');

const db = internal.db;

const GRAPH_PREFIX = '/_api/gharial/';

// remove me later
exports._exists = ggc._exists;

// TODO There are several db._flushCache() calls here, whenever gharial might
// have added or dropped collections. Maybe this should be called even when
// the request has failed (in which case now it isn't, because an exception is
// thrown first).

exports._listObjects = function () {
  const uri = GRAPH_PREFIX;
  const requestResult = arangosh.checkRequestResult(db._connection.GET(uri));
  return requestResult.graphs;
};

exports._list = function () {
  const uri = GRAPH_PREFIX;
  const requestResult = arangosh.checkRequestResult(db._connection.GET(uri));
  const graphs = requestResult.graphs;

  const result = [];
  _.each(graphs, function (graph) {
    result.push(graph._key);
  });
  return result;
};

// inherited graph class
const CommonGraph = ggc.__GraphClass;

CommonGraph.prototype.__updateDefinitions = function (edgeDefs, orphans) {
  this.__edgeDefinitions = edgeDefs;
  this.__orphanCollections = orphans;
};

CommonGraph.prototype._extendEdgeDefinitions = function (edgeDefinition) {
  const data = edgeDefinition || {};
  const uri = GRAPH_PREFIX + encodeURIComponent(this.__name) + "/edge";
  const requestResult = arangosh.checkRequestResult(db._connection.POST(uri, data));
  const graph = requestResult.graph;
  try {
    this.__updateDefinitions(graph.edgeDefinitions, graph.orphanCollections);
  } catch (ignore) {
  }
};

CommonGraph.prototype._editEdgeDefinitions = function (edgeDefinition) {
  const data = edgeDefinition || {};
  const uri = GRAPH_PREFIX + encodeURIComponent(this.__name) + "/edge/" + edgeDefinition.collection;
  const requestResult = arangosh.checkRequestResult(db._connection.PUT(uri, data));
  const graph = requestResult.graph;
  try {
    this.__updateDefinitions(graph.edgeDefinitions, graph.orphanCollections);
  } catch (ignore) {
  }
};

CommonGraph.prototype._addVertexCollection = function (name, createCollection) {
  const data = {};
  if (name) {
    data.collection = name;
  }
  let uri = GRAPH_PREFIX + encodeURIComponent(this.__name) + "/vertex";
  if (createCollection !== false) {
    uri += "?createCollection=true";
  } else {
    uri += "?createCollection=false";
  }
  const requestResult = arangosh.checkRequestResult(db._connection.POST(uri, data));
  const graph = requestResult.graph;

  try {
    this.__updateDefinitions(graph.edgeDefinitions, graph.orphanCollections);
  } catch (ignore) {
  }

  if (createCollection !== false) {
    db._flushCache();
  }
};

CommonGraph.prototype._removeVertexCollection = function (name, dropCollection) {
  let uri = GRAPH_PREFIX + encodeURIComponent(this.__name) + "/vertex/" + encodeURIComponent(name);
  if (dropCollection === true) {
    uri += "?dropCollection=true";
  } else {
    uri += "?dropCollection=false";
  }
  const requestResult = arangosh.checkRequestResult(db._connection.DELETE(uri));
  const graph = requestResult.graph;

  try {
    this.__updateDefinitions(graph.edgeDefinitions, graph.orphanCollections);
  } catch (ignore) {
  }

  if (dropCollection === true) {
    db._flushCache();
  }
};

CommonGraph.prototype._deleteEdgeDefinition = function (name, dropCollection = false) {
  let uri = GRAPH_PREFIX + encodeURIComponent(this.__name) + "/edge/" + encodeURIComponent(name);
  if (dropCollection === true) {
    uri += "?dropCollections=true";
  } else {
    uri += "?dropCollections=false";
  }

  const requestResult = arangosh.checkRequestResult(db._connection.DELETE(uri));
  const graph = requestResult.graph;

  try {
    this.__updateDefinitions(graph.edgeDefinitions, graph.orphanCollections);
  } catch (ignore) {
  }

  if (dropCollection === true) {
    db._flushCache();
  }
};

exports._graph = function (graphName) {
  const uri = GRAPH_PREFIX + encodeURIComponent(graphName);
  const requestResult = arangosh.checkRequestResult(db._connection.GET(uri));
  return new CommonGraph(requestResult.graph);
};

exports._create = function (name, edgeDefinitions, orphans, options) {
  const data = {};
  if (name) {
    data.name = name;
  }
  if (edgeDefinitions) {
    data.edgeDefinitions = edgeDefinitions;
  }
  if (orphans) {
    data.orphanCollections = orphans;
  }
  if (options) {
    data.options = options;
  }

  const uri = GRAPH_PREFIX;
  const requestResult = arangosh.checkRequestResult(db._connection.POST(uri, data));
  db._flushCache();
  return new CommonGraph(requestResult.graph);
};

exports._drop = function (graphName, dropCollections) {
  let uri = GRAPH_PREFIX + encodeURIComponent(graphName);
  if (dropCollections) {
    uri += "?dropCollections=true";
  }
  const requestResult = arangosh.checkRequestResult(db._connection.DELETE(uri));
  if (dropCollections) {
    db._flushCache();
  }
  return requestResult.result;
};

// js based helper functions
exports.__GraphClass = CommonGraph;
exports._edgeDefinitions = ggc._edgeDefinitions;
exports._extendEdgeDefinitions = ggc._extendEdgeDefinitions;
exports._relation = ggc._relation;
exports._registerCompatibilityFunctions = ggc._registerCompatibilityFunctions;
