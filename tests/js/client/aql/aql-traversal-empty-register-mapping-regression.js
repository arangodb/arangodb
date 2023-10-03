/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for regression returning blocks to the manager
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db, indexId;

const vertexCollectionName = "RegressionGraphVertex";
const edgeCollectionName = "RegressionGraphEdges";

var cleanup = function() {
  db._drop(vertexCollectionName);
  db._drop(edgeCollectionName);
};

var createBaseGraph = function () {
  const vc = db._create(vertexCollectionName);
  const ec = db._createEdgeCollection(edgeCollectionName);
};


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function traversalEmptyRegisterMappingSuite() {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      cleanup();
      createBaseGraph();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      cleanup();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test object access for path object
////////////////////////////////////////////////////////////////////////////////

    testEmptyRegisterMappingCrashes : function () {
      const query = `RETURN COUNT(FOR v, e, p IN 1..1 INBOUND "a" @@edges RETURN 1)`;
      var actual = db._query(query, { "@edges": edgeCollectionName });
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(traversalEmptyRegisterMappingSuite);

return jsunity.done();

