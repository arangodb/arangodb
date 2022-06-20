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

const {db} = require("internal");
const arangosh = require('@arangodb/arangosh');
const ggc = require('@arangodb/general-graph-common');
const _ = require('lodash');

const GRAPH_PREFIX = '/_api/gharial/';

// inherited graph class
let AbstractGraph = ggc.__GraphClass;

// TODO There are several db._flushCache() calls here, whenever gharial might
// have added or dropped collections. Maybe this should be called even when
// the request has failed (in which case now it isn't, because an exception is
// thrown first).

class GeneralGraph extends AbstractGraph {
  _deleteEdgeDefinition(name, dropCollection = false) {
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
  }

  _extendEdgeDefinitions(edgeDefinition, options = {}) {
    const data = _.clone(edgeDefinition) || {};
    data.options = options || {};
    const uri = GRAPH_PREFIX + encodeURIComponent(this.__name) + "/edge";
    const requestResult = arangosh.checkRequestResult(db._connection.POST(uri, data));
    const graph = requestResult.graph;
    try {
      this.__updateDefinitions(graph.edgeDefinitions, graph.orphanCollections);
    } catch (ignore) {
    }
  }

  _editEdgeDefinitions(edgeDefinition, options = {}) {
    const data = _.clone(edgeDefinition) || {};
    data.options = options || {};
    const uri = GRAPH_PREFIX + encodeURIComponent(this.__name) + "/edge/" + edgeDefinition.collection;
    const requestResult = arangosh.checkRequestResult(db._connection.PUT(uri, data));
    const graph = requestResult.graph;
    try {
      this.__updateDefinitions(graph.edgeDefinitions, graph.orphanCollections);
    } catch (ignore) {
    }
  }

  _addVertexCollection(name, createCollection = true, options = {}) {
    const data = {options};
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
  }

  _removeVertexCollection(name, dropCollection = false) {
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
  }
}

exports.GeneralGraph = GeneralGraph;
// The following are enterprise only graphs.
// We will only expose their existence if we are in enterprise mode
// otherwise just use the basic graph
exports.SmartGraph = GeneralGraph;
exports.EnterpriseGraph = GeneralGraph;
exports.SatelliteGraph = GeneralGraph;
