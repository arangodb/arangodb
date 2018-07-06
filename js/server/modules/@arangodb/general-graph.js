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

var changeEdgeDefinitionsForGraph = function (graph, edgeDefinition, self) {
  var graphObj = exports._graph(graph._key);
  var eDs = graph.edgeDefinitions;
  var gotAHit = false;

  // replace edgeDefintion
  eDs.forEach(
    function (eD, id) {
      if (eD.collection === edgeDefinition.collection) {
        gotAHit = true;
        eDs[id].from = edgeDefinition.from;
        eDs[id].to = edgeDefinition.to;
        db._graphs.update(graph._key, {edgeDefinitions: eDs});
        if (graph._key === self.__name) {
          self.__edgeDefinitions[id].from = edgeDefinition.from;
          self.__edgeDefinitions[id].to = edgeDefinition.to;
        }
      }
    }
  );
  if (!gotAHit) {
    return;
  }

// move unused collections to orphanage
};

// new c++ based
exports._listObjects = GeneralGraph._listObjects;

exports._create = function (name, edgeDefinition, orphans, options) {
  let g = GeneralGraph._create(name, edgeDefinition, orphans, options);
  let graph = new ggc.__GraphClass(g.graph);
  graph._editEdgeDefinitions = function (edgeDefinitions) {
    var self = this;
    let result = GeneralGraph._editEdgeDefinitions(name, edgeDefinitions);
    let eDs = result.graph.edgeDefinitions;
    //this.updateBindCollections(result.graph);
    var graphs = exports._listObjects().graphs;
    graphs.forEach(
      function (graph) {
        changeEdgeDefinitionsForGraph(graph, result.graph.edgeDefinitions, null, null, self);
      }
    );
    /*
    eDs.forEach(
      function (eD, id) {
        self.__edgeDefinitions[id].from = eD.from;
        self.__edgeDefinitions[id].to = eD.to;
        console.log("HELP ME");
        console.log(self.edgeDefinitions);
        self.edgeDefinitions[id].from = eD.from;
        self.edgeDefinitions[id].to = eD.to;
      });
    console.log(self.__edgeDefinitions);
    console.log("FUER JAN ");*/
  };
  return graph;
};

// old js based
exports._drop = ggc._drop;
exports._exists = ggc._exists;
exports._graph = ggc._graph;
exports._edgeDefinitions = ggc._edgeDefinitions;
exports._extendEdgeDefinitions = ggc._extendEdgeDefinitions;
exports._list = ggc._list;
exports._relation = ggc._relation;
