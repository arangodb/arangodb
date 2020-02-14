/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */
/*global arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief setup collections for dump/reload tests
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const isEnterprise = internal.isEnterprise();

(function () {
  'use strict';
  if (!isEnterprise) {
    return;
  }

  const smartGraphName = "UnitTestDumpSmartGraph";
  const edges = "UnitTestDumpSmartEdges";
  const vertices = "UnitTestDumpSmartVertices";
  const gm = require("@arangodb/smart-graph");
  if (gm._exists(smartGraphName)) {
    gm._drop(smartGraphName, true);
  }
  db._drop(edges);
  db._drop(vertices);

  gm._create(smartGraphName, [gm._relation(edges, vertices, vertices)],
    [], {numberOfShards: 5, smartGraphAttribute: "cheesyness"});

  let vDocs = [{cheesyness: "cheese"}];
  let saved = db[vertices].save(vDocs).map(v => v._id);
  let eDocs = [];

  // update smartGraphAttribute. This makes _key inconsistent
  // and on dump/restore used to throw an error. We now ignore
  // that error
  db._update(saved[0], { cheesyness: "bread" });  
})();

return {
  status: true
};
