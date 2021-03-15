/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertUndefined, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'cluster.default-replication-factor': 2,
    'cluster.max-number-of-shards': 3,
    'cluster.min-replication-factor': 2,
    'cluster.max-replication-factor': 3
  };
}
var jsunity = require('jsunity');
const errors = require('@arangodb').errors;
const cn = "UnitTestsCollection";
let db = require('internal').db;

function testSuite() {
  return {
    setUp: function() {
      db._drop(cn);
    },
    
    tearDown: function() {
      db._drop(cn);
    },
    
    testCreateCollectionNoShards : function() {
      let c = db._create(cn);
      let props = c.properties();
      assertEqual(1, props.numberOfShards);
    },

    testCreateCollectionOneShard : function() {
      let c = db._create(cn, { numberOfShards: 1 });
      let props = c.properties();
      assertEqual(1, props.numberOfShards);
    },
    
    testCreateCollectionMaximumShards : function() {
      let c = db._create(cn, { numberOfShards: 3 });
      let props = c.properties();
      assertEqual(3, props.numberOfShards);
    },
    
    testCreateCollectionTooManyShards : function() {
      try {
        db._create(cn, { numberOfShards: 4 });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_TOO_MANY_SHARDS.code, err.errorNum);
      }
    },
    
    testCreateCollectionMinReplicationFactor : function() {
      let c = db._create(cn, { replicationFactor: 2 });
      let props = c.properties();
      assertEqual(2, props.replicationFactor);
    },
    
    testCreateCollectionMaxReplicationFactor : function() {
      let c = db._create(cn, { replicationFactor: 3 }, 2, { enforceReplicationFactor: false });
      let props = c.properties();
      assertEqual(3, props.replicationFactor);
    },
    
    testCreateCollectionReplicationFactorTooHigh : function() {
      try {
        db._create(cn, { replicationFactor: 4 }, 2, { enforceReplicationFactor: true });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
      
      let c = db._create(cn, { replicationFactor: 4 }, 2, { enforceReplicationFactor: false });
      let props = c.properties();
      assertEqual(4, props.replicationFactor);
    },

    testCreateGraph : function() {
      const graph = require("@arangodb/general-graph");
      const vn = "UnitTestVertices";
      const en = "UnitTestEdges";
      const gn = "UnitTestGraph";
      const edgeDef = [graph._relation(en, vn, vn)];
      try {
        graph._drop(gn, true);
      } catch (err) {}

      let myGraph = graph._create(gn, edgeDef, null, {});
      try {
        let props = db[vn].properties();
        assertEqual(2, props.replicationFactor);
        props = db[en].properties();
        assertEqual(2, props.replicationFactor);
      } finally {
        graph._drop(gn, true);
      }
    },
    
    testCreateSmartGraph : function() {
      if (!require("internal").isEnterprise()) {
        return;
      }
      const smrt = require("@arangodb/smart-graph");
      const vn = "UnitTestVertices";
      const en = "UnitTestEdges";
      const gn = "UnitTestGraph";
      const edgeDef = [smrt._relation(en, vn, vn)];
      try {
        smrt._drop(gn, true);
      } catch (err) {}

      let myGraph = smrt._create(gn, edgeDef, null, {smartGraphAttribute: "value"});
      try {
        let props = db[vn].properties();
        assertEqual(2, props.replicationFactor);
        props = db[en].properties();
        assertEqual(2, props.replicationFactor);
      } finally {
        smrt._drop(gn, true);
      }
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
