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
const GeneralGraphModule = internal.ArangoGeneralGraphModule;

const {GeneralGraph, SmartGraph, EnterpriseGraph, SatelliteGraph} = internal.isEnterprise() ?
  require('@arangodb/ee-graph-classes') : require('@arangodb/graph-classes');

const graphToClass = graph => {
  if (internal.isEnterprise()) {
    if (graph.isSmart) {
      if (graph.hasOwnProperty("smartGraphAttribute")) {
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

exports._create = function (name, edgeDefinition, orphans, options) {
  let g = GeneralGraphModule._create(name, edgeDefinition, orphans, options);
  return graphToClass(g.graph);
};

exports._drop = GeneralGraphModule._drop;

exports._exists = GeneralGraphModule._exists;

exports._graph = function (graphName) {
  return graphToClass(GeneralGraphModule._graph(graphName));
};

exports._list = GeneralGraphModule._list;

exports._listObjects = GeneralGraphModule._listObjects;

exports._renameCollection = GeneralGraphModule._renameCollection;

// js based helper functions
exports._edgeDefinitions = ggc._edgeDefinitions;
exports._extendEdgeDefinitions = ggc._extendEdgeDefinitions;
exports._relation = ggc._relation;
exports._registerCompatibilityFunctions = ggc._registerCompatibilityFunctions;
exports.__graphToClass = graphToClass;
