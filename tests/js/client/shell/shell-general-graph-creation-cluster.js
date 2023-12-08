/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, fail, arango */

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
let {
  getMaxNumberOfShards,
  getMaxReplicationFactor,
  getMinReplicationFactor
} = require("@arangodb/test-helper");

function GeneralGraphClusterCreationSuite() {
  'use strict';
  const vn = "UnitTestVertices";
  const en = "UnitTestEdges";
  const gn = "UnitTestGraph";
  const edgeDef = [graph._relation(en, vn, vn)];
  const maxNumberOfShards = getMaxNumberOfShards();
  const maxReplicationFactor = getMaxReplicationFactor();
  const minReplicationFactor = getMinReplicationFactor(); 

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
      let myGraph = graph._create(gn, edgeDef, null, { numberOfShards: maxNumberOfShards, replicationFactor: 1 });
      let properties = db._graphs.document(gn);
      assertEqual(1, properties.replicationFactor);
      assertEqual(1, properties.minReplicationFactor);
      assertEqual(maxNumberOfShards, properties.numberOfShards);
    },

    testCreateMoreShardsThanAllowed : function () {
      try {
        graph._create(gn, edgeDef, null, { numberOfShards: maxNumberOfShards + 1, replicationFactor: 1 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_TOO_MANY_SHARDS.code, err.errorNum);
      }
    },
    
    testCreateMoreShardsThanAllowedExistingCollections : function () {
      db._create(vn);
      db._createEdgeCollection(en);
      try {
        graph._create(gn, edgeDef, null, { numberOfShards: maxNumberOfShards + 1, replicationFactor: 1 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_CLUSTER_TOO_MANY_SHARDS.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replicationFactor
////////////////////////////////////////////////////////////////////////////////
    testMinReplicationFactor : function () {
      let myGraph = graph._create(gn, edgeDef, null, { replicationFactor: minReplicationFactor });
      let properties = db._graphs.document(gn);
      assertEqual(minReplicationFactor, properties.replicationFactor);

      graph._drop(gn, true);
      try {
        graph._create(gn, edgeDef, null, { replicationFactor: minReplicationFactor - 1 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testMaxReplicationFactor : function () {
      try {
        let myGraph = graph._create(gn, edgeDef, null, { replicationFactor: maxReplicationFactor });
        let properties = db._graphs.document(gn);
        assertEqual(maxReplicationFactor, properties.replicationFactor);
        
        graph._drop(gn, true);
        try {
          graph._create(gn, edgeDef, null, { replicationFactor: maxReplicationFactor + 1 });
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
      try {
        let myGraph = graph._create(gn, edgeDef, null, { replicationFactor: maxReplicationFactor });
        let properties = db._graphs.document(gn);
        assertEqual(maxReplicationFactor, properties.replicationFactor);
        
        graph._drop(gn, true);
        try {
          graph._create(gn, edgeDef, null, { replicationFactor: maxReplicationFactor + 1 });
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
