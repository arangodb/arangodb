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
const db = internal.db;
const {GeneralGraph, SmartGraph, EnterpriseGraph, SatelliteGraph} = internal.isEnterprise() ?
  require('@arangodb/ee-graph-classes') : require('@arangodb/graph-classes');


const GRAPH_PREFIX = '/_api/gharial/';

const graphToClass = graph => {
  if (internal.isEnterprise()) {
    if (graph.isSmart) {
      if (Object.hasOwnProperty(graph, "smartGraphAttribute")) {
        return new SmartGraph(graph);
      } else {
        return new EnterpriseGraph(graph);
      }
    } else if (graph.isSatellite) {
      return new SatelliteGraph(graph);
    }
  }
  return new GeneralGraph(graph);
};

// remove me later
exports._exists = ggc._exists;

exports._listObjects = function () {
  const uri = GRAPH_PREFIX;
  const requestResult = arangosh.checkRequestResult(db._connection.GET(uri));
  return requestResult.graphs;
};

exports._list = function () {
  const uri = GRAPH_PREFIX;
  const requestResult = arangosh.checkRequestResult(db._connection.GET(uri));
  const graphs = requestResult.graphs;
  return graphs.map(g => g._key);
};

exports._graph = function (graphName) {
  const uri = GRAPH_PREFIX + encodeURIComponent(graphName);
  const requestResult = arangosh.checkRequestResult(db._connection.GET(uri));
  return graphToClass(requestResult.graph);
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
  return graphToClass(requestResult.graph);
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
exports._edgeDefinitions = ggc._edgeDefinitions;
exports._extendEdgeDefinitions = ggc._extendEdgeDefinitions;
exports._relation = ggc._relation;
exports._registerCompatibilityFunctions = ggc._registerCompatibilityFunctions;
exports.__graphToClass = graphToClass;
