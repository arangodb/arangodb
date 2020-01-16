/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the general-graph class
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
/// @author Jan Steemann
/// @author Copyright 2019, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
var graph = require("@arangodb/general-graph");
var ERRORS = arangodb.errors;
let internal = require("internal");

function GeneralGraphClusterCreationSuite() {
  'use strict';
  const vn = "UnitTestVertices";
  const en = "UnitTestEdges";
  const gn = "UnitTestGraph";
  const edgeDef = [graph._relation(en, vn, vn)];

  return {

    setUp: function () {
      try {
        graph._drop(gn, true);
      } catch (ignore) {
      }
    },

    tearDown: function () {
      try {
        graph._drop(gn, true);
      } catch (ignore) {
      }
      db._drop(vn);
      db._drop(en);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test numberOfShards
////////////////////////////////////////////////////////////////////////////////
    
    testCreateAsManyShardsAsAllowed : function () {
      let max = internal.maxNumberOfShards;
      let myGraph = graph._create(gn, edgeDef, null, { numberOfShards: max });
      let properties = db._graphs.document(gn);
      assertEqual(max, properties.numberOfShards);
    },

    testCreateMoreShardsThanAllowed : function () {
      let max = internal.maxNumberOfShards;
      try {
        graph._create(gn, edgeDef, null, { numberOfShards: max + 1 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_TOO_MANY_SHARDS.code, err.errorNum);
      }
    },
    
    testCreateMoreShardsThanAllowedExistingCollections : function () {
      db._create(vn);
      db._createEdgeCollection(en);
      let max = internal.maxNumberOfShards;
      try {
        graph._create(gn, edgeDef, null, { numberOfShards: max + 1 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_TOO_MANY_SHARDS.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replicationFactor
////////////////////////////////////////////////////////////////////////////////
    testMinReplicationFactor : function () {
      let min = internal.minReplicationFactor;
      let myGraph = graph._create(gn, edgeDef, null, { replicationFactor: min });
      let properties = db._graphs.document(gn);
      assertEqual(min, properties.replicationFactor);

      graph._drop(gn, true);
      try {
        graph._create(gn, edgeDef, null, { replicationFactor: min - 1 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testMaxReplicationFactor : function () {
      let max = internal.maxReplicationFactor;
      try {
        let myGraph = graph._create(gn, edgeDef, null, { replicationFactor: max });
        let properties = db._graphs.document(gn);
        assertEqual(max, properties.replicationFactor);
        
        graph._drop(gn, true);
        try {
          graph._create(gn, edgeDef, null, { replicationFactor: max + 1 });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      } catch (err) {
        // if creation fails, then it must have been exactly this error
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
    },
    
    testMaxReplicationFactorExistingCollection : function () {
      db._create(vn);
      db._createEdgeCollection(en);
      let max = internal.maxReplicationFactor;
      try {
        let myGraph = graph._create(gn, edgeDef, null, { replicationFactor: max });
        let properties = db._graphs.document(gn);
        assertEqual(max, properties.replicationFactor);
        
        graph._drop(gn, true);
        try {
          graph._create(gn, edgeDef, null, { replicationFactor: max + 1 });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      } catch (err) {
        // if creation fails, then it must have been exactly this error
        assertEqual(ERRORS.ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, err.errorNum);
      }
    },

  };
}

jsunity.run(GeneralGraphClusterCreationSuite);

return jsunity.done();
