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
const _ = require("lodash");

const {
  ERROR_HTTP_BAD_PARAMETER,
  ERROR_HTTP_SERVER_ERROR,
  ERROR_BAD_PARAMETER,
  ERROR_VALIDATION_BAD_PARAMETER,
  ERROR_INVALID_SMART_JOIN_ATTRIBUTE,
  ERROR_ARANGO_COLLECTION_TYPE_INVALID,
  ERROR_CLUSTER_INSUFFICIENT_DBSERVERS,
  ERROR_HTTP_CONFLICT,
  ERROR_ARANGO_DUPLICATE_NAME,
  ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE
} = require("internal").errors;

/* Note: This test does not cover the indexes entry yet. */

/**
 * !!!!!!!!DISCLAIMER!!!!
 * This test is for our most backwards-compatible API.
 * This API is used within arangodump -> arangorestore cycle on
 * the restore side.
 * If a customer needs to do bigger version jumps and cannot do
 * a rolling upgrade this API is our recomendation to upgarade from
 * early versions to newer ones, therefor we need to keep it backwards compatible.
 * Therefore: Please reconsider this when you ever need to adapt this test file, if
 * there is a possible path to make your change in a backwards compatible way so that
 * none of the existing tests here needs to be changed.
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
const tryRestore = (parameters, queryParameter = {}) => {
  assertTrue(Object.keys(queryParameter).length <= 1, `This test suite does not support more than one query parameter at a time, this is doable but not implemented.`);
  let url = '/_api/replication/restore-collection';
  if (Object.keys(queryParameter).length === 1) {
    for (const [param, value] of Object.entries(queryParameter)) {
      if (param === "force" || param === "overwrite" || param === "ignoreDistributeShardsLikeErrors") {
        url += `?${param}=${value}`;
      }
    }
  }
  return arango.PUT(url, {
    parameters, indexes: []
  });
};

const defaultProps = getDefaultProps();
const validateProperties = (overrides, colName, type, keepClusterSpecificAttributes = false) => {
  if (!keepClusterSpecificAttributes) {
    overrides = filterClusterOnlyAttributes(overrides);
  }
  const col = db._collection(colName);
  const props = col.properties();
  assertTrue(props.hasOwnProperty("globallyUniqueId"));
  assertEqual(col.name(), colName);
  assertEqual(col.type(), type);
  const expectedProps = {...defaultProps, ...overrides};
  if (keepClusterSpecificAttributes && !isCluster) {
    // In some cases minReplicationFactor is returned
    // but is not part of the expected list. So let us add it
    expectedProps.minReplicationFactor = expectedProps.writeConcern;
  }
  for (const [key, value] of Object.entries(expectedProps)) {
    assertEqual(props[key], value, `Differ on key ${key}`);
  }
  // Note we add +1 on expected for the globallyUniqueId.
  const expectedKeys = Object.keys(expectedProps);

  if (!overrides.hasOwnProperty("globallyUniqueId")) {
    // The globallyUniqueId is generated, so we cannot compare equality, we should make
    // sure it is always there.
    expectedKeys.push("globallyUniqueId");
  }
  const foundKeys = Object.keys(props);
  assertEqual(expectedKeys.length, foundKeys.length, `Check that all properties are reported expected. Missing: ${JSON.stringify(_.difference(expectedKeys, foundKeys))} unexpected: ${JSON.stringify(_.difference(foundKeys, expectedKeys))}`);
};

const validatePropertiesAreNotEqual = (forbidden, collname) => {
  const col = db._collection(collname);
  const props = col.properties();
  for (const [key, value] of Object.entries(forbidden)) {
    assertNotEqual(props[key], value, `Using forbidden value on key ${key}`);
  }
};

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
      const res = tryRestore({name: collname, type: 2, numberOfShards: 3});
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

    testHiddenSmartGraphCollectionsForce: function () {
      const names = [`_local_${collname}`, `_from_${collname}`, `_to_${collname}`];
      for (const name of names) {
        const res = tryRestore({name: name, isSystem: true}, {force: true});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          // Difference to above: The Enterprise version actually does restore the collection with force
          assertEqual(db._collections().filter(c => c.name() === name).length, 1, `Collection ${name} should exist`);
        } finally {
          db._drop(name, {isSystem: true});
        }
      }
    },

    testIgnoreDistributeShardsLikeErrors: function () {
      const nonExistentCollection = "UnittestIDoNotExist";
      const input = {distributeShardsLike: nonExistentCollection};
      const res = tryRestore({name: collname, ...input});
      try {
        if (isCluster) {
          // First attempt should fail, leader does not exist
          isDisallowed(ERROR_HTTP_SERVER_ERROR.code, ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE.code, res, input);
          assertEqual(db._collections().filter(c => c.name() === collname).length, 0, `Collection ${collname} should not exist`);
          // If we vie the old option to ignoreDistributeShardsLikeErrors we still cannot create the collection
          const retryRes = tryRestore({name: collname, ...input}, {ignoreDistributeShardsLikeErrors: true});
          isDisallowed(ERROR_HTTP_SERVER_ERROR.code, ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE.code, retryRes, input);
          assertEqual(db._collections().filter(c => c.name() === collname).length, 0, `Collection ${collname} should not exist`);
        }
        isAllowed(res,collname, input);
      } finally {
        // Should be noop.
        db._drop(collname);
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

    testObjectIDPollution: function () {
      // ObjectId needs to be ignored
      const res = tryRestore({name: collname, type: 2, objectId: "deleteMe"});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
      } finally {
        db._drop(collname);
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

    testCreateSmartGraph: function () {
      for (const isDisjoint of [true, false]) {
        const vertexName = "UnitTestSmartVertex";
        const edgeName = "UnitTestSmartEdge";
        const vertex = {
          numberOfShards: 3,
          replicationFactor: 2,
          isSmart: true,
          shardingStrategy: "hash",
          shardKeys: [
            "_key:"
          ],
          smartGraphAttribute: "test",
          isDisjoint: isDisjoint
        };

        const edge = {
          numberOfShards: 3,
          replicationFactor: 2,
          isSmart: true,
          shardingStrategy: "enterprise-hash-smart-edge",
          shardKeys: [
            "_key:"
          ],
          isDisjoint: isDisjoint,
          distributeShardsLike: vertexName
        };

        const shouldKeepClusterSpecificAttributes = isEnterprise;

        const res = tryRestore({...vertex, type: 2, name: vertexName});
        try {
          // Should work, is a simple combination of other tests
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          if (!isCluster) {
            // The following are not available in single server
            // NOTE: If at one point they are exposed we will have the validate clash on
            // non set properties.
            if (!isEnterprise) {
              // Well unless we are in enterprise.
              delete vertex.isSmart;
              delete vertex.smartGraphAttribute;
              delete vertex.isDisjoint;
            }
            delete vertex.shardingStrategy;
          } else {
            if (!isEnterprise) {
              // smart features cannot be set
              vertex.isSmart = false;
              delete vertex.smartGraphAttribute;
              vertex.isDisjoint = false;
            }
          }
          validateProperties(vertex, vertexName, 2, shouldKeepClusterSpecificAttributes);
          const resEdge = tryRestore({...edge, type: 3, name: edgeName});
          try {
            // Should work, everything required for smartGraphs is there.
            assertTrue(resEdge.result, `Result: ${JSON.stringify(resEdge)}`);
            if (!isCluster) {
              // The following are not available in single server
              if (!isEnterprise) {
                // Well unless we are in enterprise.
                delete edge.isSmart;
                delete edge.isDisjoint;
                delete edge.distributeShardsLike;
              }
              delete edge.shardingStrategy;
            } else {
              if (!isEnterprise) {
                // smartFeatures cannot be set
                edge.isSmart = false;
                edge.shardingStrategy = "hash";
                edge.isDisjoint = false;
              }
            }
            if (isEnterprise && isCluster) {
              // Check special creation path
              const makeSmartEdgeAttributes = (isTo) => {
                const tmp = {...edge, isSystem: true, isSmart: false, shardingStrategy: "hash"};
                if (isTo) {
                  tmp.shardKeys = [":_key"];
                }
                return tmp;
              }
              validateProperties({...edge, numberOfShards: 0}, edgeName, 3, shouldKeepClusterSpecificAttributes);
              validateProperties(makeSmartEdgeAttributes(false), `_local_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
              if (!isDisjoint) {
                validateProperties(makeSmartEdgeAttributes(false), `_from_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
                validateProperties(makeSmartEdgeAttributes(true), `_to_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
              } else {
                assertEqual(db._collections().filter(c => c.name() === `_from_${edgeName}` || c.name() === `_to_${edgeName}`).length, 0, "Created incorrect hidden collections");
              }
            } else {
              validateProperties(edge, edgeName, 3, shouldKeepClusterSpecificAttributes);
            }
          } finally {
            db._drop(edgeName, true);
          }
        } finally {
          db._drop(vertexName, true);
        }
      }
    },

    testCreateEnterpriseGraph: function () {
      for (const isDisjoint of [true, false]) {
        const vertexName = "UnitTestSmartVertex";
        const edgeName = "UnitTestSmartEdge";
        const vertex = {
          numberOfShards: 3,
          replicationFactor: 2,
          isSmart: true,
          shardingStrategy: "enterprise-hex-smart-vertex",
          shardKeys: [
            "_key:"
          ],
          smartGraphAttribute: "test",
          isDisjoint: isDisjoint
        };

        const edge = {
          numberOfShards: 3,
          replicationFactor: 2,
          isSmart: true,
          shardingStrategy: "enterprise-hash-smart-edge",
          shardKeys: [
            "_key:"
          ],
          isDisjoint: isDisjoint,
          distributeShardsLike: vertexName
        };

        const shouldKeepClusterSpecificAttributes = isEnterprise;

        const res = tryRestore({...vertex, type: 2, name: vertexName});
        try {
          // Should work, is a simple combination of other tests
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          if (!isCluster) {
            // The following are not available in single server
            // NOTE: If at one point they are exposed we will have the validate clash on
            // non set properties.
            if (!isEnterprise) {
              // Well unless we are in enterprise.
              delete vertex.isSmart;
              delete vertex.smartGraphAttribute;
              delete vertex.isDisjoint;
            }
            delete vertex.shardingStrategy;
          } else {
            if (!isEnterprise) {
              // smart features cannot be set
              vertex.isSmart = false;
              delete vertex.smartGraphAttribute;
              vertex.isDisjoint = false;
              // Enterprise ShardingStrategy is forbidden
              vertex.shardingStrategy = "hash";
            }
          }
          validateProperties(vertex, vertexName, 2, shouldKeepClusterSpecificAttributes);
          const resEdge = tryRestore({...edge, type: 3, name: edgeName});
          try {
            // Should work, everything required for smartGraphs is there.
            assertTrue(resEdge.result, `Result: ${JSON.stringify(resEdge)}`);
            if (!isCluster) {
              // The following are not available in single server
              if (!isEnterprise) {
                // Well unless we are in enterprise.
                delete edge.isSmart;
                delete edge.isDisjoint;
                delete edge.distributeShardsLike;
              }
              delete edge.shardingStrategy;
            } else {
              if (!isEnterprise) {
                // smartFeatures cannot be set
                edge.isSmart = false;
                edge.isDisjoint = false;
                edge.shardingStrategy = "hash";
              }
            }
            if (isEnterprise && isCluster) {
              // Check special creation path
              const makeSmartEdgeAttributes = (isTo) => {
                const tmp = {...edge, isSystem: true, isSmart: false, shardingStrategy: "hash"};
                if (isTo) {
                  tmp.shardKeys = [":_key"];
                }
                return tmp;
              }
              validateProperties({...edge, numberOfShards: 0}, edgeName, 3, shouldKeepClusterSpecificAttributes);
              validateProperties(makeSmartEdgeAttributes(false), `_local_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
              if (!isDisjoint) {
                validateProperties(makeSmartEdgeAttributes(false), `_from_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
                validateProperties(makeSmartEdgeAttributes(true), `_to_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
              } else {
                assertEqual(db._collections().filter(c => c.name() === `_from_${edgeName}` || c.name() === `_to_${edgeName}`).length, 0, "Created incorrect hidden collections");
              }
            } else {
              validateProperties(edge, edgeName, 3, shouldKeepClusterSpecificAttributes);
            }
          } finally {
            db._drop(edgeName, true);
          }
        } finally {
          db._drop(vertexName, true);
        }
      }
    },

    testDuplicateName: function () {
      const res = tryRestore({name: collname});
      try {
        // First run should work
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);

        const resAgain = tryRestore({name: collname});
        isDisallowed(ERROR_HTTP_CONFLICT.code, ERROR_ARANGO_DUPLICATE_NAME.code, resAgain, {name: collname});
      } finally {
        db._drop(collname);
      }
    },

    testDuplicateNameOverwrite: function () {
      const res = tryRestore({name: collname});
      try {
        // First run should work
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
        db._collection(collname).save([{}, {}, {}]);
        assertEqual(db._collection(collname).count(), 3, `Test_setup assert, we are not able to create data!`);
        const idBeforeRestore = db._collection(collname)._id;

        // With overwrite we can recreate the collection
        const resAgain = tryRestore({name: collname}, {overwrite: true});
        assertTrue(resAgain.result, `Result: ${JSON.stringify(resAgain)}`);
        validateProperties({}, collname, 2);
        const idAfterRestore = db._collection(collname)._id;
        assertEqual(db._collection(collname).count(), 0, `Data not removed!`);
        assertNotEqual(idBeforeRestore, idAfterRestore, `Collection not dropped, just truncated!`);
      } finally {
        db._drop(collname);
      }
    },

    testDuplicateNameOverwriteNotDropable: function () {
      const res = tryRestore({name: collname});
      const followerName = "UnitTestFollower";
      try {
        // First run should work
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
        db._collection(collname).save([{}, {}, {}]);
        // Create a follower: (NOTE: we enforce the side-effect here, that a leader cannot be dropped)
        const resFollower = tryRestore({name: followerName, distributeShardsLike: collname});
        assertTrue(resFollower.result, `Result: ${JSON.stringify(res)}`);
        const idBeforeRestore = db._collection(collname)._id;
        // With overwrite we can recreate the collection
        const resAgain = tryRestore({name: collname}, {overwrite: true});
        assertTrue(resAgain.result, `Result: ${JSON.stringify(resAgain)}`);
        validateProperties({}, collname, 2);
        const idAfterRestore = db._collection(collname)._id;
        assertEqual(db._collection(collname).count(), 0, `Data not removed!`);
        assertEqual(idBeforeRestore, idAfterRestore, `Collection dropped, it was just truncated!`);
      } finally {
        db._drop(followerName);
        db._drop(collname);
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
    },

    testComputedValues: function () {
      const computedValues = [
        {
          name: "newValue",
          expression: "RETURN @doc.foxx",
          overwrite: true,
          keepNull: true,
        }
      ];
      const res = tryRestore({name: collname, computedValues});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        // Overrides are taken from DatabaseConfiguration, they differ from original
        validateProperties({computedValues}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testComputedValuesIllegal: function () {
      // Taken from failure test for computedValues
      const computedValues = [
        {
          name: "newValue",
          expression: "CONCAT(@doc.value1, '+')",
          overwrite: false
        }
      ];
      const res = tryRestore({name: collname, computedValues});
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {computedValues});
      } finally {
        // Should be noop
        db._drop(collname);
      }
    },

    testIllegalReplicationWriteConcern: function () {
      const res = tryRestore({name: collname, writeConcern: 3, replicationFactor: 2});
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {
          writeConcern: 3,
          replicationFactor: 2
        });
      } finally {
        // Should be noop
        db._drop(collname);
      }
    },

    testRestoreSystemCollection: function () {
      const systemName = "_pregel_queries";
      const idBeforeRestore = db._collection(systemName)._id;
      const propsBeforeRestore = db._collection(systemName).properties();
      const res = tryRestore({name: systemName, isSystem: true});
      const idAfterRestore = db._collection(systemName)._id;
      assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
      // We do not want to change any properties
      validateProperties(propsBeforeRestore, systemName, 2);
      assertEqual(idBeforeRestore, idAfterRestore, `Did drop the collection, not just truncated`);
    },

    testRestoreLeadingSystemCollection: function () {
      // We cannot restore a leading system collection.
      db._createDatabase("UnitTestDB");
      try {
        db._useDatabase("UnitTestDB");
        // Graph is leading, cannot be dropped, will be truncated
        const systemName = "_graphs";
        const idBeforeRestore = db._collection(systemName)._id;
        const propsBeforeRestore = db._collection(systemName).properties();
        const res = tryRestore({name: systemName, isSystem: true});
        const idAfterRestore = db._collection(systemName)._id;
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        // We do not want to change any properties
        validateProperties(propsBeforeRestore, systemName, 2);
        assertEqual(idBeforeRestore, idAfterRestore, `Did drop the collection, not just truncated`);
      } finally {
        db._useDatabase("_system");
        db._dropDatabase("UnitTestDB");
      }
    },

    testDatabaseConfiguration: function () {
      db._createDatabase("UnitTestConfiguredDB", {replicationFactor: 3, writeConcern: 2});
      try {
        db._useDatabase("UnitTestConfiguredDB");

        const res = tryRestore({name: collname});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          // Overrides are taken from DatabaseConfiguration, they differ from original
          validateProperties({replicationFactor: 3, writeConcern: 2}, collname, 2);
        } finally {
          db._drop(collname);
        }

      } finally {
        db._useDatabase("_system");
        db._dropDatabase("UnitTestConfiguredDB");
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

  for (const [prop, type] of Object.entries(propertiesToTest)) {
    suite[`testIllegalEntries${prop}`] = function () {
      for (const ignoredValue of testValues[type]) {
        const testParam = {[prop]: ignoredValue};
        const res = tryRestore({name: collname, [prop]: ignoredValue});
        try {
          switch (prop) {
            case "type": {
              if (typeof ignoredValue === "number") {
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

function RestoreInOneShardSuite() {
  const collname = "UnitTestCollection";
  const oneShardLeader = "_graphs";
  const fixedOneShardValues = ["numberOfShards", "replicationFactor", "minReplicationFactor", "writeConcern"];
  const getOneShardShardingValues = () => {
    const props = _.pick(db[oneShardLeader].properties(), fixedOneShardValues);
    // NOTE: For some reason one-shard uses HASH sharding strategy.
    // Original restore uses compat.
    return {...props, distributeShardsLike: oneShardLeader, shardingStrategy: "hash"};
  };
  return {
    setUpAll: function () {
      db._createDatabase("UnitTestOneShard", {sharding: "single"});
      db._useDatabase("UnitTestOneShard");
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase("UnitTestOneShard");
    },

    testRestoreMinimalOneShard: function () {
      const res = tryRestore({name: collname});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties(getOneShardShardingValues(), collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testRestoreWithOtherShardValues: function () {
      for (const v of fixedOneShardValues) {
        // all are numeric values. None of them can be modified in one shard.
        const res = tryRestore({name: collname, [v]: 2});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties(getOneShardShardingValues(), collname, 2);
        } finally {
          db._drop(collname);
        }
      }
    },

    testRestoreWithDistributeShardsLike: function () {
      const otherLeaderName = "UnitTestLeader";
      const leaderRes = tryRestore({name: otherLeaderName});
      try {
        // Should be allowed
        assertTrue(leaderRes.result, `Result: ${JSON.stringify(leaderRes)}`);
        const res = tryRestore({name: collname, distributeShardsLike: otherLeaderName});
        try {
          // Well we just ignore that something is off here...
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties(getOneShardShardingValues(), collname, 2);
        } finally {
          db._drop(collname);
        }
      } finally {
        db._drop(otherLeaderName);
      }
    },

    testRestoreMultipleShardKeysOneShard: function () {
      const res = tryRestore({name: collname, shardKeys: ["foo", "bar"]});
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {shardKeys: ["foo", "bar"]});
      } finally {
        // Should be a noop.
        db._drop(collname);
      }
    }
  }
}

jsunity.run(RestoreCollectionsSuite);
jsunity.run(IgnoreIllegalTypesSuite);
jsunity.run(RestoreInOneShardSuite);

return jsunity.done();