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
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const arangosh = require('@arangodb/arangosh');
const ggc = require('@arangodb/general-graph-common');

const GRAPH_PREFIX = '/api/gharial/';

let graph = {};

// //////////////////////////////////////////////////////////////////////////////
// / @brief return the replication logger state
// //////////////////////////////////////////////////////////////////////////////
/*graph._list = function () {
  var requestResult = internal.db._connection.GET(GRAPH_PREFIX + 'graphs');
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};*/

exports._create = ggc._create;
exports._drop = ggc._drop;
exports._exists = ggc._exists;
exports._graph = ggc._graph;
exports._edgeDefinitions = ggc._edgeDefinitions;
exports._extendEdgeDefinitions = ggc._extendEdgeDefinitions;
exports._listObjects = ggc._listObjects;
exports._list = ggc._list;
exports._relation = ggc._relation;
