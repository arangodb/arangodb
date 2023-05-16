/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, assertFalse, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication
///
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {arango, db} = require("@arangodb");
const isCluster = require('internal').isCluster();
const isEnterprise = require("internal").isEnterprise();

/* TODO List
 - Include objectIDs everywhere
 - queryParams:
    - overwrite
    - force
    - ignoreDistributeShardsLikeErrors
 - ErrorCases:
   - Duplicate Name
     - On System (truncate)
       - Change Parameters
     - On User (drop)
   - Illegal replication/writeConcern combis
- OneShard
- Indexes (other suite)
- parameters
    - shadowCollections
    - SmartCollections (EE)
    - writeConcern & minReplicationFactor
        - Database Config
    - replicationFactor
        - Database Config
    - shardingStrategy
    - schema

    - some bogus parameters
 */

function RestureCollectionSuite() {
  const filterClusterOnlyAttributes = (attributes) => {
    // The following attributes are not returned by the Collection Properties API
    // However we support restore cluster -> SingleServer so we need to be able to restore the
    // corresponding collections.
    if (!isCluster) {
      const clusterOnlyAttributes = ["numberOfShards", "shardKeys", "replicationFactor"];
      /* , , "writeConcern" */
      for (const key of clusterOnlyAttributes) {
        delete attributes[key];
      }
    }
    return attributes;
  };
  const getDefaultProps = () => {
    if (isCluster) {
      return {
        "isSmart": false,
        "isSystem": false,
        "waitForSync": false,
        "shardKeys": [
          "_key"
        ],
        "numberOfShards": 1,
        "keyOptions": {
          "allowUserKeys": true,
          "type": "traditional"
        },
        "replicationFactor": 2,
        "minReplicationFactor": 1,
        "writeConcern": 1,
        /* Is this a reasonable default?, We have a new collection */
        "shardingStrategy": isEnterprise ? "enterprise-compat" : "community-compat",
        "cacheEnabled": false,
        "computedValues": null,
        "syncByRevision": true,
        "schema": null,
        "isDisjoint": false
      };
    } else {
      return {
        "isSystem": false,
        "waitForSync": false,
        "keyOptions": {
          "allowUserKeys": true,
          "type": "traditional",
          "lastValue": 0
        },
        "writeConcern": 1,
        "cacheEnabled": false,
        "computedValues": null,
        "syncByRevision": true,
        "schema": null
      };
    }
  };
  const tryRestore = (parameters) => {
    return arango.PUT('/_api/replication/restore-collection', {
      parameters, indexes: []
    });
  };

  const defaultProps = getDefaultProps();
  const validateProperties = (overrides, colName, type) => {
    overrides = filterClusterOnlyAttributes(overrides);
    const col = db._collection(collname);
    const props = col.properties();
    assertTrue(props.hasOwnProperty("globallyUniqueId"));
    assertEqual(col.name(), collname);
    const expectedProps = {...defaultProps, ...overrides};
    for (const [key, value] of Object.entries(expectedProps)) {
      assertEqual(props[key], value, `Differ on key ${key}`);
    }
    // Note we add +1 on expected for the globallyUniqueId.
    let expectedNumberOfProperties = Object.keys(expectedProps).length;
    if (!overrides.hasOwnProperty("globallyUniqueId")) {
      // The globallyUniqueId is generated, so we cannot compare equality, we should make
      // sure it is always there.
      expectedNumberOfProperties++;
    }
    assertEqual(expectedNumberOfProperties, Object.keys(props).length, `Check that all properties are reported`);
  };

  const validatePropertiesAreNotEqual = (forbidden, collname) => {
    const col = db._collection(collname);
    const props = col.properties();
    for (const [key, value] of Object.entries(forbidden)) {
      assertNotEqual(props[key], value, `Using forbidden value on key ${key}`);
    }
  };

  const validatePropertiesDoNotExist = (colName, illegalProperties) => {
    const col = db._collection(collname);
    const props = col.properties(true);
    for (const key of illegalProperties) {
      assertFalse(props.hasOwnProperty(key), `Property ${key} should not exist`);
    }
  };
  const collname = "UnitTestRestore";

  return {

    testRestoreNonObject: function () {
      const tryRequest = (body) => arango.PUT('/_api/replication/restore-collection', body);
      const illegalBodies = [
        [],
        [{parameters: {name: "test", type: 1}, indexes: []}],
        123,
        "test",
        true,
        -15.3
      ];
      for (const body of illegalBodies) {
        const res = tryRequest(body);
        // Sometimes we get internal, sometime Bad_Parameter
        assertTrue(res.code === 500 || res.code === 400);
      }
    },

    testRestoreMinimal: function () {
      const res = tryRestore({name: collname});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testRestoreEdge: function () {
      const res = tryRestore({name: collname, type: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 3);
      } finally {
        db._drop(collname);
      }
    },

    testRestoreDocument: function () {
      const res = tryRestore({name: collname, type: 2});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testRestoreIllegalType: function () {
      const illegalType = [
        0,
        1,
        4
      ];

      for (const type of illegalType) {
        const res = tryRestore({name: collname, type});
        assertEqual(res.code, 400, `Could create ${type}`);
        assertEqual(res.errorNum, 1218);
      }

      const ignoredType = [
        "test",
        "document",
        "edge",
        {},
        [],
        true
      ];

      // ill-formatted types are ignored, and fall back to document
      for (const type of ignoredType) {
        const res = tryRestore({name: collname, type});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties({}, collname, 2);
        } finally {
          db._drop(collname);
        }
      }
    },

    testRestoreDeleted: function () {
      const res = tryRestore({name: collname, type: 2, deleted: true});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        assertEqual(db._collections().filter((c) => c.name() === collname).length, 0, `Deleted collection was restored.`);
      } finally {
        db._drop(collname);
      }
    },

    testRestoreNumberOfShards: function () {
      const res = tryRestore({name: collname,  type: 2,  numberOfShards: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({numberOfShards: 3}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testRestoreListOfShards: function () {
      const res = tryRestore({name: collname, shards: {"s01": ["server1", "server2"], "s02": ["server3"]}});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({numberOfShards: 2}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testRestoreReplicationFactor: function () {
      const res = tryRestore({name: collname, replicationFactor: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({replicationFactor: 3}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testRestoreReplicationFactorSatellite: function () {
      // For this API "satellite" and 0 should be identical
      for (const replicationFactor of ["satellite", 0]) {
        const res = tryRestore({name: collname, replicationFactor: replicationFactor});
        try {
          if (!isEnterprise) {
            if (!isCluster) {
              // SingleServer just ignores satellite.
              validateProperties({replicationFactor: "satellite", writeConcern: 1}, collname, 2);
            } else {
              assertFalse(res.result, `Created a collection with replicationFactor 0`);
            }
          } else {
            assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
            // MinReplicaitonFactor is forced to 0 on satellites
            validateProperties({replicationFactor: "satellite", minReplicationFactor: 0, writeConcern: 0}, collname, 2);
          }
        } finally {
          db._drop(collname);
        }
      }
    },

    testWriteConcern: function () {
      const res = tryRestore({name: collname, writeConcern: 2, replicationFactor: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (isCluster) {
          validateProperties({replicationFactor: 3, minReplicationFactor: 2, writeConcern: 2}, collname, 2);
        } else {
          // WriteConcern on SingleServe has no meaning, it is returned but as default value.
          validateProperties({replicationFactor: 3, writeConcern: 1}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testMinReplicationFactor: function () {
      const res = tryRestore({name: collname, minReplicationFactor: 2, replicationFactor: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (isCluster) {
          validateProperties({replicationFactor: 3, minReplicationFactor: 2, writeConcern: 2}, collname, 2);
        } else {
          // WriteConcern on SingleServe has no meaning, it is returned but as default value.
          validateProperties({replicationFactor: 3, writeConcern: 1}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testGloballyUniqueIdReal: function () {
      const globallyUniqueId = "h843EE8622105/182";
      const res = tryRestore({name: collname, globallyUniqueId});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (isCluster) {
          validateProperties({}, collname, 2);
          validatePropertiesAreNotEqual({globallyUniqueId}, collname);
        } else {
          validateProperties({globallyUniqueId}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testGloballyUniqueIdOtherString: function () {
      const res = tryRestore({name: collname, globallyUniqueId: "test"});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (isCluster) {
          validateProperties({}, collname, 2);
          validatePropertiesAreNotEqual({globallyUniqueId: "test"}, collname);
        } else {
          validateProperties({globallyUniqueId: "test"}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testGloballyUniqueIdNumeric: function () {
      const res = tryRestore({name: collname, globallyUniqueId: 123456});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
        validatePropertiesAreNotEqual({globallyUniqueId: 123456}, collname);
      } finally {
        db._drop(collname);
      }
    },

    testGloballyUniqueIdRestoreTwice: function () {
      const globallyUniqueId = "notSoUnique";
      const res = tryRestore({name: collname, globallyUniqueId});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        // Make sure first collection have this id
        if (isCluster) {
          validateProperties({}, collname, 2);
          validatePropertiesAreNotEqual({globallyUniqueId}, collname);
        } else {
          validateProperties({globallyUniqueId}, collname, 2);
        }
        const otherCollName = `${collname}_2`;
        const res2 = tryRestore({name: otherCollName, globallyUniqueId});
        if (isCluster) {
          try {
            // Cluster regenerates a new id.
            assertTrue(res2.result, `Result: ${JSON.stringify(res2)}`);
            validateProperties({}, otherCollName, 2);
            validatePropertiesAreNotEqual({globallyUniqueId}, otherCollName);
          } finally {
            db._drop(otherCollName);
          }
        } else {
          // SingleServer does not generate a new id.
          assertFalse(res2.result, `Result: ${JSON.stringify(res2)}`);
        }
      } finally {
        db._drop(collname);
      }
    },

    testIsSmartStandAlone: function () {
      const res = tryRestore({name: collname, isSmart: true});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (!isEnterprise) {
          // isSmart is just ignored, we keep default: false
          validateProperties({}, collname, 2);
          if (!isCluster) {
            // SingleSever does not expose is Smart at all
            validatePropertiesDoNotExist(collname, ["isSmart"]);
          }
        } else {
          validateProperties({isSmart: true}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },
    testSmartGraphAttribute: function () {
      const res = tryRestore({name: collname, smartGraphAttribute: "test"});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (!isEnterprise) {
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartGraphAttribute"]);
        } else {
          validateProperties({smartGraphAttribute: "test"}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartGraphAttributeIsSmart: function () {
      const res = tryRestore({name: collname, smartGraphAttribute: "test", isSmart: true});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (!isEnterprise) {
          validateProperties({}, collname, 2);
          if (!isCluster) {
            validatePropertiesDoNotExist(collname, ["isSmart", "smartGraphAttribute"]);
          } else {
            validatePropertiesDoNotExist(collname, ["smartGraphAttribute"]);
          }
        } else {
          validateProperties({smartGraphAttribute: "test",  isSmart: true}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoin: function () {
      const res = tryRestore({name: collname, smartJoinAttribute: "test"});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (!isEnterprise) {
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartJoinAttribute"]);
        } else {
          validateProperties({smartJoinAttribute: "test"}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoinIsSmart: function () {
      const res = tryRestore({name: collname, smartJoinAttribute: "test", isSmart: true});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (!isEnterprise) {
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartJoinAttribute"]);
        } else {
          validateProperties({smartJoinAttribute: "test", isSmart: true}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testShadowCollections: function () {
      const res = tryRestore({name: collname, shadowCollections: ["123", "456", "789"]});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
        validatePropertiesDoNotExist(collname, ["shadowCollections"]);
      } finally {
        db._drop(collname);
      }
    },

    testHiddenSmartGraphCollections: function () {
      const names = [`_local_${collname}`, `_from_${collname}`, `_to_${collname}`];
      for (const name of names) {
        const res = tryRestore({name: name, isSystem: true});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          assertEqual(db._collections().filter(c => c.name() === name).length, 1, `Collection ${name} should exist`);
        } finally {
          db._drop(name, {isSystem: true});
        }
      }
    },

    testShardKeys: function () {
      const res = tryRestore({name: collname, shardKeys: ["test"]});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({shardKeys: ["test"]}, collname, 2);
      } finally {
        db._drop(collname, true);
      }
    },

    testSchema: function () {
      // Schema taken from documentation
      const schema = {
        rule: {
          properties: {nums: {type: "array", items: {type: "number", maximum: 6}}},
          additionalProperties: {type: "string"},
          required: ["nums"]
        },
        level: "moderate",
        message: "The document does not contain an array of numbers in attribute 'nums', or one of the numbers is greater than 6."
      };
      const res = tryRestore({name: collname, schema});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        // Will be automatically injected
        schema.type = "json";
        validateProperties({schema}, collname, 2);
      } finally {
        db._drop(collname, true);
      }
    },

    testUnknownParameters: function () {
      const unknownParameters = ["test", "foo", "", "doCompact", "isVolatile"];

      for (const p of unknownParameters) {
        const res = tryRestore({name: collname, [p]: "justIgnoreMe"});
        try {
          // Unknown parameters are always ignored.
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties({}, collname, 2);
        } finally {
          db._drop(collname, true);
        }
      }
    }


  };
}

jsunity.run(RestureCollectionSuite);

return jsunity.done();