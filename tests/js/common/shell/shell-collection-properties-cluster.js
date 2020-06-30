/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertNull, assertTrue, assertUndefined, fail */
////////////////////////////////////////////////////////////////////////////////
/// @brief test collection properties changes
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

let jsunity = require("jsunity");
let arangodb = require("@arangodb");
let db = arangodb.db;
let ERRORS = arangodb.errors;

function PropertiesChangeSuite() {
  let name1 = "UnitTestsCollection1";
  let name2 = "UnitTestsCollection2";

  return {

    setUp : function () {
      db._drop(name2);
      db._drop(name1);
    },

    tearDown : function () {
      db._drop(name2);
      db._drop(name1);
    },
    
    testShardingStrategyChange : function () {
      let c = db._create(name1, { numberOfShards: 5, shardingStrategy: "hash" });
      assertEqual("hash", c.properties().shardingStrategy);

      // no change
      let result = c.properties({ shardingStrategy: "hash" });
      assertEqual("hash", result.shardingStrategy);
      assertEqual("hash", c.properties().shardingStrategy);
      
      try {
        c.properties({ shardingStrategy: "edge" });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

    testReplicationFactorChange : function () {
      let c = db._create(name1, { numberOfShards: 5, replicationFactor: 2 });
      assertEqual(2, c.properties().replicationFactor);

      let result = c.properties({ replicationFactor: 1 });
      assertEqual(1, result.replicationFactor);
      assertEqual(1, c.properties().replicationFactor);
      
      result = c.properties({ replicationFactor: 2 });
      assertEqual(2, result.replicationFactor);
      assertEqual(2, c.properties().replicationFactor);
    },

    testWriteConcernChange : function () {
      let c = db._create(name1, { numberOfShards: 5, replicationFactor: 2, writeConcern: 1 });
      assertEqual(1, c.properties().writeConcern);

      let result = c.properties({ writeConcern: 2 });
      assertEqual(2, result.writeConcern);
      assertEqual(2, c.properties().writeConcern);
      
      result = c.properties({ writeConcern: 1 });
      assertEqual(1, result.writeConcern);
      assertEqual(1, c.properties().writeConcern);
    },
    
    testReplicationFactorChangeDistributeShardsLike : function () {
      let c1 = db._create(name1, { numberOfShards: 5, replicationFactor: 2 });
      let c2 = db._create(name2, { distributeShardsLike: name1 });

      assertEqual(2, c1.properties().replicationFactor);
      assertEqual(2, c2.properties().replicationFactor);

      // no change
      let result = c2.properties({ replicationFactor: 2 });
      assertEqual(2, result.replicationFactor);
      assertEqual(2, c2.properties().replicationFactor);

      // an actual change
      try {
        c2.properties({ replicationFactor: 1 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, err.errorNum);
      }
      assertEqual(2, c2.properties().replicationFactor);

      // now change properties of prototype collection
      result = c1.properties({ replicationFactor: 2 });
      assertEqual(2, result.replicationFactor);
      assertEqual(2, c1.properties().replicationFactor);
      assertEqual(2, c2.properties().replicationFactor);
      
      result = c1.properties({ replicationFactor: 1 });
      assertEqual(1, result.replicationFactor);
      assertEqual(1, c1.properties().replicationFactor);
      assertEqual(1, c2.properties().replicationFactor);
    },
    
    testWriteConcernChangeDistributeShardsLike : function () {
      let c1 = db._create(name1, { numberOfShards: 5, writeConcern: 1, replicationFactor: 2 });
      let c2 = db._create(name2, { distributeShardsLike: name1 });

      assertEqual(1, c1.properties().writeConcern);
      assertEqual(1, c2.properties().writeConcern);

      // no change
      let result = c2.properties({ writeConcern: 1 });
      assertEqual(1, result.writeConcern);
      assertEqual(1, c2.properties().writeConcern);

      // an actual change
      try {
        c2.properties({ writeConcern: 2 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_FORBIDDEN.code, err.errorNum);
      }
      assertEqual(1, c2.properties().writeConcern);

      // now change properties of prototype collection
      result = c1.properties({ writeConcern: 1 });
      assertEqual(1, result.writeConcern);
      assertEqual(1, c1.properties().writeConcern);
      assertEqual(1, c2.properties().writeConcern);
      
      result = c1.properties({ writeConcern: 2 });
      assertEqual(2, result.writeConcern);
      assertEqual(2, c1.properties().writeConcern);
      assertEqual(2, c2.properties().writeConcern);
    },
  };
}

jsunity.run(PropertiesChangeSuite);

return jsunity.done();
