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

const {
  ERROR_HTTP_BAD_PARAMETER,
  ERROR_HTTP_SERVER_ERROR,
  ERROR_BAD_PARAMETER,
  ERROR_VALIDATION_BAD_PARAMETER,
  ERROR_INVALID_SMART_JOIN_ATTRIBUTE,
  ERROR_ARANGO_COLLECTION_TYPE_INVALID,
  ERROR_CLUSTER_INSUFFICIENT_DBSERVERS
} = require("internal").errors;

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
    - computedValues

 */

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
  const col = db._collection(colName);
  const props = col.properties();
  assertTrue(props.hasOwnProperty("globallyUniqueId"));
  assertEqual(col.name(), colName);
  assertEqual(col.type(), type);
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
  assertEqual(expectedNumberOfProperties, Object.keys(props).length, `Check that all properties are reported expected ${JSON.stringify(Object.keys(expectedProps))} got ${JSON.stringify(Object.keys(props))}`);
};

const validatePropertiesAreNotEqual = (forbidden, collname) => {
  const col = db._collection(collname);
  const props = col.properties();
  for (const [key, value] of Object.entries(forbidden)) {
    assertNotEqual(props[key], value, `Using forbidden value on key ${key}`);
  }
};

function RestoreCollectionsSuite() {



  const validatePropertiesDoNotExist = (colName, illegalProperties) => {
    const col = db._collection(collname);
    const props = col.properties(true);
    for (const key of illegalProperties) {
      assertFalse(props.hasOwnProperty(key), `Property ${key} should not exist`);
    }
  };
  const collname = "UnitTestRestore";
  const leaderName = "UnitTestLeader";

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
        assertTrue(res.code === ERROR_HTTP_SERVER_ERROR.code || res.code === ERROR_HTTP_BAD_PARAMETER.code);
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
        assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code, `Could create ${type}`);
        assertEqual(res.errorNum, ERROR_ARANGO_COLLECTION_TYPE_INVALID.code);
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
            // MinReplicationFactor is forced to 0 on satellites
            // NOTE: SingleServer Enteprise is red: For some reason this restore returns MORE properties, then the others.
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
        if (!isEnterprise) {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          // isSmart is just ignored, we keep default: false
          validateProperties({}, collname, 2);
          if (!isCluster) {
            // SingleSever does not expose is Smart at all
            validatePropertiesDoNotExist(collname, ["isSmart"]);
          }
        } else {
          assertTrue(res.error, `Result: ${JSON.stringify(res)}`);
          assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
          assertEqual(res.errorNum, ERROR_BAD_PARAMETER.code);
        }
      } finally {
        db._drop(collname);
      }
    },
    testSmartGraphAttribute: function () {
      const res = tryRestore({name: collname, smartGraphAttribute: "test"});
      try {
        if (!isEnterprise) {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartGraphAttribute"]);
        } else {
          assertTrue(res.error, `Result: ${JSON.stringify(res)}`);
          assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
          assertEqual(res.errorNum, ERROR_BAD_PARAMETER.code);
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartGraphAttributeIsSmart: function () {
      const res = tryRestore({name: collname, smartGraphAttribute: "test", isSmart: true});
      try {
        if (!isEnterprise) {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties({}, collname, 2);
          if (!isCluster) {
            validatePropertiesDoNotExist(collname, ["isSmart", "smartGraphAttribute"]);
          } else {
            validatePropertiesDoNotExist(collname, ["smartGraphAttribute"]);
          }
        } else {
          assertTrue(res.error, `Result: ${JSON.stringify(res)}`);
          assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
          assertEqual(res.errorNum, ERROR_BAD_PARAMETER.code);
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoin: function () {
      const res = tryRestore({name: collname, smartJoinAttribute: "test"});
      try {
        if (!isEnterprise) {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartJoinAttribute"]);
        } else {
          if (!isCluster) {
            assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
            validateProperties({}, collname, 2);
            validatePropertiesDoNotExist(collname, ["smartJoinAttribute"]);
          } else {
            assertTrue(res.error, `Result: ${JSON.stringify(res)}`);
            assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
            assertEqual(res.errorNum, ERROR_INVALID_SMART_JOIN_ATTRIBUTE.code);
          }
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoinIsSmart: function () {
      const res = tryRestore({name: collname, smartJoinAttribute: "test", isSmart: true});
      try {
        if (!isEnterprise) {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartJoinAttribute"]);
        } else {
          assertTrue(res.error, `Result: ${JSON.stringify(res)}`);
          assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
          assertEqual(res.errorNum, ERROR_BAD_PARAMETER.code);
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
          if (isEnterprise) {
            // Yes the enterprise variant reponds with "yes i did", but in fact it did not.
            assertEqual(db._collections().filter(c => c.name() === name).length, 0, `Collection ${name} should not exist`);

          } else {
            assertEqual(db._collections().filter(c => c.name() === name).length, 1, `Collection ${name} should exist`);
          }
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

    testCreateDistributeShardsLike: function () {
      const leader = tryRestore({name: leaderName, numberOfShards: 3});
      try {
        // Leader is tested before
        assertTrue(leader.result, `Result: ${JSON.stringify(leader)}`);
        const res = tryRestore({name: collname, distributeShardsLike: leaderName});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          if (isCluster) {
            validateProperties({numberOfShards: 3, distributeShardsLike: leaderName}, collname, 2);
          } else {
            // Single server just ignores the value
            validateProperties({numberOfShards: 3}, collname, 2);
          }
        } finally {
          db._drop(collname);
        }
      } finally {
        db._drop(leaderName);
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


function IgnoreIllegalTypesSuite() {
  // Lists of illegal type values, for each test to iterate over.
  const testValues = {
    string: [1, -3.6, true, false, [], {foo: "bar"}],
    integer: [-3, 5.1, true, false, [2], {foo: "bar"}, "test"],
    bool: [1, -3.1, [true], {foo: "bar"}, "test"],
    object: [0, -2.1, true, false, [], "test"],
    array: [0, -2.1, true, false, {foo: "bar"}, "test"]
  };
  // Definition of value types.
  const propertiesToTest = {
    type: "integer",
    isDeleted: "bool",
    isSystem: "bool",
    waitForSync: "bool",
    keyOptions: "object",
    writeConcern: "integer",
    cacheEnabled: "bool",
    computedValues: "array",
    syncByRevision: "bool",
    schema: "object",
    /* Cluster Properties, should also be ignored on single server */
    isSmart: "bool",
    shardKeys: "array",
    numberOfShards: "integer",
    replicationFactor: "integer",
    minReplicationFactor: "integer",
    shardingStrategy: "string",
    isDisjoint: "bool",
    distributeShardsLike: "string"
  };

  const collname = "UnitTestRestore";

  // No Setup or teardown required.
  // Every test in the suite is self-contained
  const suite = {};

  const isDisallowed = (code, errorNum, res, input) => {
    assertTrue(res.error, `Created disallowed Collection on input ${JSON.stringify(input)}`);
    assertEqual(res.code, code, `Different error on input ${JSON.stringify(input)}, ${JSON.stringify(res)}`);
    assertEqual(res.errorNum, errorNum, `Different error on input ${JSON.stringify(input)}, ${JSON.stringify(res)}`);
  };

  const isAllowed = (res, collname, input) => {
    assertTrue(res.result, `Result: ${JSON.stringify(res)} on input ${JSON.stringify(input)}`);
    validateProperties({}, collname, 2);
    validatePropertiesAreNotEqual(input, collname);
  };

  for (const [prop, type] of Object.entries(propertiesToTest)) {
    suite[`testIllegalEntries${prop}`] = function() {
      for (const ignoredValue of testValues[type]) {
        const testParam = {[prop]: ignoredValue};
        const res = tryRestore({name: collname, [prop]: ignoredValue});
        try {
          switch (prop) {
            case "type": {
              if (typeof ignoredValue === "number" ) {
                // We cannot create collections that are of number type, which is not 2 or 3
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, 1218, res, testParam);
              } else {
                // But well we can create all others, obviously defaulting to 2 (document)
                isAllowed(res, collname, testParam);
              }
              break;
            }
            case "computedValues": {
              if (isCluster) {
                // In cluster this is allowed, local it is not.
                // TODO: This may actually be broken!
                isAllowed(res, collname, testParam);
              } else {
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              }
              break;
            }
            case "shardKeys":
            case "numberOfShards": {
              isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              break;
            }
            case "schema": {
              isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_VALIDATION_BAD_PARAMETER.code, res, testParam);
              break;
            }
            case "replicationFactor": {
              if (isCluster) {
                if (typeof ignoredValue === "number") {
                  // We take doubles for integers
                  if (ignoredValue > 2) {
                    // but they are too large to be allowed
                    isDisallowed(ERROR_HTTP_SERVER_ERROR.code, ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, res, testParam);
                  } else {
                    // or too low
                    isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
                  }
                } else {
                  // All others are ignored
                  isAllowed(res, collname, testParam);
                }
              } else {
                // Just ignore if we are not in cluster.
                isAllowed(res, collname, testParam);
              }
              break;
            }
            case "writeConcern":
            case "minReplicationFactor": {
              if (isCluster) {
                if (typeof ignoredValue === "number") {
                  // We take doubles for integers
                  if (ignoredValue > 2) {
                    // but they are too large to be allowed
                    isDisallowed(ERROR_HTTP_SERVER_ERROR.code, ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, res, testParam);
                  } else {
                    // or too low
                    isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
                  }
                } else {
                  isAllowed(res, collname, testParam);
                  // isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
                }
              } else {
                // Just ignore if we are not in cluster.
                isAllowed(res, collname, testParam);
              }
              break;
            }
            case "syncByRevision": {
              if (isCluster) {
                // This is almost standard, but the default value is different, if we give an illegal value.
                assertTrue(res.result, `Result: ${JSON.stringify(res)} on input ${JSON.stringify(testParam)}`);
                validateProperties({syncByRevision: false}, collname, 2);
                validatePropertiesAreNotEqual(testParam, collname);
              } else {
                isAllowed(res, collname, testParam);
              }
              break;
            }
            case "distributeShardsLike": {
              if (isCluster) {
                // Can only be strings
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              } else {
                isAllowed(res, collname, testParam);
              }
              break;
            }
            default: {
              // Most unknown parameter types are ignored.
              isAllowed(res, collname, testParam);
              break;
            }
          }
        } finally {
          db._drop(collname, true);
        }
      }
    };
  }
  return suite;
}

jsunity.run(RestoreCollectionsSuite);
jsunity.run(IgnoreIllegalTypesSuite);

return jsunity.done();