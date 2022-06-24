'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Graph Classes
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
// / @author Michael Hackstein
// / @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const ggc = require('@arangodb/general-graph-common');
const {ArangoGraph} = require('internal');
// inherited graph class
let AbstractGraph = ggc.__GraphClass;

class GeneralGraph extends AbstractGraph {
  _deleteEdgeDefinition(edgeDefinition, dropCollection = false) {
    let result = ArangoGraph._deleteEdgeDefinition(this.__name, edgeDefinition, dropCollection);
    this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
  }

  _extendEdgeDefinitions(edgeDefinitions, options = {}) {
    let result = ArangoGraph._extendEdgeDefinitions(this.__name, edgeDefinitions, options);
    this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
  }

  _editEdgeDefinitions(edgeDefinitions, options = {}) {
    let result = ArangoGraph._editEdgeDefinitions(this.__name, edgeDefinitions, options);
    this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
  }

  _addVertexCollection(vertexName, createCollection = true, options = {}) {
    let result = ArangoGraph._addVertexCollection(this.__name, vertexName, createCollection, options);
    this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
  }

  _removeVertexCollection(vertexName, dropCollection = false) {
    let result = ArangoGraph._removeVertexCollection(this.__name, vertexName, dropCollection);
    this.__updateDefinitions(result.graph.edgeDefinitions, result.graph.orphanCollections);
  }
}

exports.GeneralGraph = GeneralGraph;
// The following are enterprise only graphs.
// We will only expose their existence if we are in enterprise mode
// otherwise just use the basic graph
exports.SmartGraph = GeneralGraph;
exports.EnterpriseGraph = GeneralGraph;
exports.SatelliteGraph = GeneralGraph;
