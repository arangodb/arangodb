/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, assertFalse, assertNotEqual, fail */

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
const {arango, db, ArangoError} = require("@arangodb");
const isCluster = require('internal').isCluster();
const isEnterprise = require("internal").isEnterprise();
const _ = require("lodash");
const isWindows = (require("internal").platform.substr(0, 3) === 'win');
const isServer = typeof require("internal").arango === 'undefined';

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
  ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,
  ERROR_INTERNAL,
  ERROR_ARANGO_ILLEGAL_NAME,
  ERROR_HTTP_CORRUPTED_JSON
} = require("internal").errors;

const filterClusterOnlyAttributes = (attributes) => {
  // The following attributes are not returned by the Collection Properties API
  // However the user can still send them to the API. Erase them here as they should
  // not be returned by the server.
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
    const base = {
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
      /* On windows we start with a replicationFactor of 1 */
      "replicationFactor": isWindows ? 1 : 2,
      "minReplicationFactor": 1,
      "writeConcern": 1,
      "shardingStrategy": "hash",
      "cacheEnabled": false,
      "computedValues": null,
      "syncByRevision": true,
      "schema": null,
      "isDisjoint": false
    };
    if (isServer) {
      base.internalValidatorType = 0;
      base.isSmartChild = false;
      base.usesRevisionsAsDocumentIds = true;
      base.statusString = "loaded";
    }
    return base;
  } else {
    const base = {
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
    if (isServer) {
      base.internalValidatorType = 0;
      base.isSmartChild = false;
      base.usesRevisionsAsDocumentIds = true;
      base.statusString = "loaded";
    }
    return base;
  }
};
const tryCreate = (parameters) => {
  try {
    if (parameters.hasOwnProperty("type")) {
      db._create(parameters.name, parameters, parameters.type);
    } else {
      db._create(parameters.name, parameters);
    }
    return {result: true};
  } catch (err) {
    if (isServer && err instanceof ArangoError) {
      // Translate to HTTP style error.
      return {
        error: true,
        errorNum: err.errorNum
      };
    }
    return err;
  }
};

const defaultProps = getDefaultProps();
const validateProperties = (overrides, colName, type, keepClusterSpecificAttributes = false) => {
  if (!keepClusterSpecificAttributes) {
    overrides = filterClusterOnlyAttributes(overrides);
  }
  const col = db._collection(colName);
  const props = col.properties();
  assertTrue(props.hasOwnProperty("globallyUniqueId"));
  if (isServer && !isCluster) {
    // This is a local property only exposed on dbServer and singleServer
    assertTrue(props.hasOwnProperty("objectId"));
  }
  assertEqual(col.name(), colName);
  assertEqual(col.type(), type);
  const expectedProps = {...defaultProps, ...overrides};
  if (keepClusterSpecificAttributes && !isCluster) {
    // In some cases minReplicationFactor is returned
    // but is not part of the expected list. So let us add it
    expectedProps.minReplicationFactor = expectedProps.writeConcern;
  }
  for (const [key, value] of Object.entries(expectedProps)) {
    assertEqual(props[key], value, `Differ on key ${key} ${JSON.stringify(props)} vs ${JSON.stringify(expectedProps)}`);
  }
  // Note we add +1 on expected for the globallyUniqueId.
  const expectedKeys = Object.keys(expectedProps);

  if (!overrides.hasOwnProperty("globallyUniqueId")) {
    // The globallyUniqueId is generated, so we cannot compare equality, we should make
    // sure it is always there.
    expectedKeys.push("globallyUniqueId");
  }
  if (isServer && !isCluster && !overrides.hasOwnProperty("objectId")) {
    // The objectId is generated, so we cannot compare equality, we should make
    // sure it is always there.
    expectedKeys.push("objectId");
  }
  if (isServer && isCluster && overrides.isSmart && type === 3) {
    // Special attribute for smartEdgeColelctions
    expectedKeys.push("shadowCollections");
    assertTrue(Array.isArray(props.shadowCollections), `A smart edge collection is required to have shadowCollections, ${JSON.stringify(props)}`);
    assertEqual(props.shadowCollections.length, 3, `Found incorrect amount of shadowCollections ${JSON.stringify(props.shadowCollections)}`);
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
  if (!isServer) {
    assertEqual(res.code, code, `Different error on input ${JSON.stringify(input)}, ${JSON.stringify(res)}`);
  }
  assertEqual(res.errorNum, errorNum, `Different error on input ${JSON.stringify(input)}, ${JSON.stringify(res)}`);
};

const isAllowed = (res, collname, input) => {
  assertTrue(res.result, `Result: ${JSON.stringify(res)} on input ${JSON.stringify(input)}`);
  validateProperties({}, collname, 2);
  validatePropertiesAreNotEqual(input, collname);
};

const validatePropertiesDoNotExist = (colName, illegalProperties) => {
  const col = db._collection(colName);
  const props = col.properties(true);
  for (const key of illegalProperties) {
    assertFalse(props.hasOwnProperty(key), `Property ${key} should not exist`);
  }
};

const clearLogs = () => {
  if (isServer) {
    // Only client reported so far
    return;
  }
  arango.DELETE("/_admin/log/entries");
};

const validateDeprecationLogEntryWritten = () => {
  if (isServer) {
    // Only client reported so far
    return;
  }
  try {
    const expectedTopic = "deprecation";
    const expectedLogId = "ee638";
    // Note we ask for 2 entries here.
    // This way we can assert that we produce exactly one line per error (which we discard after we asserted it)
    let res = arango.GET("/_admin/log/entries?upto=warning&size=2");
    assertEqual(res.total, 1, `Expecting exactly one message, instead found ${JSON.stringify(res)}.`);
    assertEqual(res.messages[0].topic, expectedTopic, `Expecting specific log topic, instead found ${JSON.stringify(res)}.`);
    assertTrue(res.messages[0].message.startsWith(`[${expectedLogId}]`), `Expected specific logID, instead found ${JSON.stringify(res)}.`)
  } finally {
    // Erase the log we have just read, so the next test is clean.
    clearLogs();
  }
};

const validateNoLogsLeftBehind = () => {
  if (isServer) {
    // Only client reported so far
    return;
  }
  try {
    let res = arango.GET("/_admin/log/entries?upto=warning&size=1");
    assertEqual(res.total, 0, `Expecting no additional logs, every test that expects a log entry needs to run cleanup after itself`);
  } finally {
    clearLogs();
  }
};

function CreateCollectionsSuite() {

  const collname = "UnitTestRestore";
  const leaderName = "UnitTestLeader";

  return {

    tearDown() {
      validateNoLogsLeftBehind();
    },

    testCreateNonObject: function () {
      const illegalBodies = [
        [],
        [{parameters: {name: "test", type: 1}, indexes: []}],
        123,
        "test",
        true,
        -15.3
      ];
      const allowedErrorsCodes = [
        ERROR_INTERNAL.code,
        ERROR_ARANGO_ILLEGAL_NAME.code,
        ERROR_HTTP_CORRUPTED_JSON.code
      ];
      for (const body of illegalBodies) {
        try {
          db._create(collname, body);
          fail();
        } catch (err) {
          // Sometimes we get internal, sometime Bad_Parameter
          if (!isServer) {
            assertTrue(err.code === ERROR_HTTP_SERVER_ERROR.code || err.code === ERROR_HTTP_BAD_PARAMETER.code, `Got response ${err} for body ${JSON.stringify(body)}`);
          }
          if (isServer && err instanceof TypeError) {
            // This is allowed, v8 type error, noting we can assert.
          } else {
            assertNotEqual(allowedErrorsCodes.indexOf(err.errorNum), -1 , `Got response ${err.errorNum}, ${err.message} for body ${JSON.stringify(body)}, ${err}`);
          }
        }
      }
    },

    testCreateMinimal: function () {
      const res = tryCreate({name: collname});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testCreateEdge: function () {
      const res = tryCreate({name: collname, type: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (isServer) {
          // Type in options is ignored, need third parameter
          validateProperties({}, collname, 2);
        } else {
          validateProperties({}, collname, 3);
        }
      } finally {
        db._drop(collname);
      }
    },

    testCreateDocument: function () {
      const res = tryCreate({name: collname, type: 2});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testCreateIllegalType: function () {
      const illegalType = [
        0,
        1,
        4
      ];

      for (const type of illegalType) {
        try {
          // Illegal numeric types are allowed and default to document.
          const res = tryCreate({name: collname, type});
          validateDeprecationLogEntryWritten();
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties({}, collname, 2);
        } finally {
          db._drop(collname);
        }
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
        const res = tryCreate({name: collname, type});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          if (type === "edge") {
            validateProperties({}, collname, 3);
          } else {
            validateProperties({}, collname, 2);
          }
        } finally {
          db._drop(collname);
        }
      }
    },

    testCreateDeleted: function () {
      const res = tryCreate({name: collname, type: 2, deleted: true});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        assertEqual(db._collections().filter((c) => c.name() === collname).length, 1, `Deleted flag was not ignored.`);
      } finally {
        db._drop(collname);
      }
    },

    testCreateNumberOfShards: function () {
      const res = tryCreate({name: collname, type: 2, numberOfShards: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({numberOfShards: 3}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testCreateListOfShards: function () {
      const res = tryCreate({name: collname, shards: {"s01": ["server1", "server2"], "s02": ["server3"]}});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        // The list of Shards is just ignored
        validateProperties({}, collname, 2);
        validateDeprecationLogEntryWritten();
      } finally {
        db._drop(collname);
      }
    },

    testCreateReplicationFactor: function () {
      const res = tryCreate({name: collname, replicationFactor: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({replicationFactor: 3}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testCreateReplicationFactorSatellite: function () {
      // For this API "satellite" and 0 should be identical
      for (const replicationFactor of ["satellite", 0]) {
        const res = tryCreate({name: collname, replicationFactor: replicationFactor});
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
            if (replicationFactor === 0) {
              validateDeprecationLogEntryWritten();
            }
            // MinReplicationFactor is forced to 0 on satellites
            // NOTE: SingleServer Enterprise for some reason this create returns MORE properties, then the others.
            if (!isCluster) {
              validateProperties({replicationFactor: "satellite", minReplicationFactor: 0, writeConcern: 0, isSmart: false, shardKeys: ["_key"], numberOfShards: 1, isDisjoint: false}, collname, 2, true);
            } else {
              validateProperties({replicationFactor: "satellite", minReplicationFactor: 0, writeConcern: 0}, collname, 2);
            }
          }
        } finally {
          db._drop(collname);
        }
      }
    },

    testWriteConcern: function () {
      const res = tryCreate({name: collname, writeConcern: 2, replicationFactor: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({replicationFactor: 3, writeConcern: 2}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testMinReplicationFactor: function () {
      const res = tryCreate({name: collname, minReplicationFactor: 2, replicationFactor: 3});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({replicationFactor: 3, writeConcern: 2}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testGloballyUniqueIdReal: function () {
      const globallyUniqueId = "h843EE8622105/182";
      const res = tryCreate({name: collname, globallyUniqueId});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
        validatePropertiesAreNotEqual({globallyUniqueId}, collname);
      } finally {
        db._drop(collname);
      }
    },

    testGloballyUniqueIdOtherString: function () {
      const res = tryCreate({name: collname, globallyUniqueId: "test"});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
        validatePropertiesAreNotEqual({globallyUniqueId: "test"}, collname);
      } finally {
        db._drop(collname);
      }
    },

    testGloballyUniqueIdNumeric: function () {
      const res = tryCreate({name: collname, globallyUniqueId: 123456});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
        validatePropertiesAreNotEqual({globallyUniqueId: 123456}, collname);
      } finally {
        db._drop(collname);
      }
    },

    testGloballyUniqueIdCreateTwice: function () {
      const globallyUniqueId = "notSoUnique";
      const res = tryCreate({name: collname, globallyUniqueId});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        // Make sure first collection does not have this id
        validateProperties({}, collname, 2);
        validatePropertiesAreNotEqual({globallyUniqueId}, collname);
        const otherCollName = `${collname}_2`;
        const res2 = tryCreate({name: otherCollName, globallyUniqueId});
        try {
          // We always generate a new ID
          assertTrue(res2.result, `Result: ${JSON.stringify(res2)}`);
          validateProperties({}, otherCollName, 2);
          validatePropertiesAreNotEqual({globallyUniqueId}, otherCollName);
        } finally {
          db._drop(otherCollName);
        }
      } finally {
        db._drop(collname);
      }
    },

    testIsSmartStandAlone: function () {
      const res = tryCreate({name: collname, isSmart: true});
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
          if (!isServer) {
            assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
          }
          assertEqual(res.errorNum, ERROR_BAD_PARAMETER.code);
        }
      } finally {
        db._drop(collname);
      }
    },
    testSmartGraphAttribute: function () {
      const res = tryCreate({name: collname, smartGraphAttribute: "test"});

      try {
        if (isEnterprise) {
          assertTrue(res.error, `Result: ${JSON.stringify(res)}`);
          if (!isServer) {
            assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
          }
          assertEqual(res.errorNum, ERROR_BAD_PARAMETER.code);
        } else {
          // Community ignores this, go to defaults
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartGraphAttribute"]);
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartGraphAttributeIsSmart: function () {
      const res = tryCreate({name: collname, smartGraphAttribute: "test", isSmart: true});
      try {
        if (isEnterprise) {
          assertTrue(res.error, `Result: ${JSON.stringify(res)}`);
          if (!isServer) {
            assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
          }
          assertEqual(res.errorNum, ERROR_BAD_PARAMETER.code);
        } else {
          // Community ignores this, go to defaults
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartGraphAttribute"]);
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoin: function () {
      const res = tryCreate({name: collname, smartJoinAttribute: "test"});
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
            if (!isServer) {
              assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
            }
            assertEqual(res.errorNum, ERROR_INVALID_SMART_JOIN_ATTRIBUTE.code);
          }
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoinIsSmart: function () {
      const res = tryCreate({name: collname, smartJoinAttribute: "test", isSmart: true});
      try {
        if (!isEnterprise) {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartJoinAttribute"]);
        } else {
          assertTrue(res.error, `Result: ${JSON.stringify(res)}`);
          if (!isServer) {
            assertEqual(res.code, ERROR_HTTP_BAD_PARAMETER.code);
          }
          assertEqual(res.errorNum, ERROR_BAD_PARAMETER.code);
        }
      } finally {
        db._drop(collname);
      }
    },

    testShadowCollections: function () {
      const res = tryCreate({name: collname, shadowCollections: ["123", "456", "789"]});
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
        const res = tryCreate({name: name, isSystem: true});
        try {
          if (isCluster && isEnterprise) {
            isDisallowed(ERROR_HTTP_SERVER_ERROR.code, ERROR_INTERNAL.code, res, {name: name, isSystem: true});
          } else {
            // SingleServer and Community don't care
            assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
            assertEqual(db._collections().filter(c => c.name() === name).length, 1, `Collection ${name} should exist`);
          }
        } finally {
          db._drop(name, {isSystem: true});
        }
      }
    },

    testShardKeys: function () {
      const res = tryCreate({name: collname, shardKeys: ["test"]});
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
      const res = tryCreate({name: collname, schema});
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
      const res = tryCreate({name: collname, type: 2, objectId: "deleteMe"});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);
      } finally {
        db._drop(collname);
      }
    },

    testCreateDistributeShardsLike: function () {
      const leader = tryCreate({name: leaderName, numberOfShards: 3});
      try {
        // Leader is tested before
        assertTrue(leader.result, `Result: ${JSON.stringify(leader)}`);
        const res = tryCreate({name: collname, distributeShardsLike: leaderName});
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

        const shouldKeepClusterSpecificAttributes = isEnterprise || isCluster;

        const res = tryCreate({...vertex, type: 2, name: vertexName});
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
            // Single Server always falls back default replicationFactor
            vertex.replicationFactor = 1;
          } else {
            if (!isEnterprise) {
              // smart features cannot be set
              vertex.isSmart = false;
              delete vertex.smartGraphAttribute;
              vertex.isDisjoint = false;
            }
          }
          if (isServer) {
            // Server has no special DC2DC case to let isDisjoint pass
            if (isEnterprise) {
              vertex.isDisjoint = false;
            } else {
              // Community does not even have the attribute
              delete vertex.isDisjoint;
            }
          }
          validateProperties(vertex, vertexName, 2, shouldKeepClusterSpecificAttributes);
          const resEdge = tryCreate({...edge, type: "edge", name: edgeName});
          try {
            // Should work, everything required for smartGraphs is there.
            assertTrue(resEdge.result, `Result: ${JSON.stringify(resEdge)}`);
            if (!isCluster) {
              // The following are not available in single server
              if (!isEnterprise) {
                // Well unless we are in enterprise.
                delete edge.isSmart;
                delete edge.isDisjoint;
              }
              delete edge.shardingStrategy;
              // Single Server always falls back default replicationFactor
              edge.replicationFactor = 1;
              // And also erases distributeShardsLike
              delete edge.distributeShardsLike;
            } else {
              if (!isEnterprise) {
                // smartFeatures cannot be set
                edge.isSmart = false;
                edge.shardingStrategy = "hash";
                edge.isDisjoint = false;
              }
            }
            if (isServer) {
              // Server has no special DC2DC case to let isDisjoint pass
              if (isEnterprise) {
                edge.isDisjoint = false;
              } else {
                // Community does not even have the attribute
                delete edge.isDisjoint;
              }
              if (isCluster) {
                // SmartEdge Leader
                edge.internalValidatorType = 0;
              }
            }
            if (isEnterprise && isCluster) {
              // Check special creation path
              const makeSmartEdgeAttributes = (isTo, isLocal) => {
                const tmp = {...edge, isSystem: true, isSmart: false, shardingStrategy: "hash"};
                if (isServer) {
                  tmp.internalValidatorType = isLocal ? 2 : 4;
                  tmp.isSmartChild = true;
                }
                if (isTo) {
                  tmp.shardKeys = [":_key"];
                }
                return tmp;
              };
              validateProperties({...edge, numberOfShards: 0}, edgeName, 3, shouldKeepClusterSpecificAttributes);
              validateProperties(makeSmartEdgeAttributes(false, true), `_local_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
              if (!isDisjoint || isServer) {
                // If we are not disjoint we get all collections, as server test cannot let isDisjoint pass, we will also end up here
                validateProperties(makeSmartEdgeAttributes(false, false), `_from_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
                validateProperties(makeSmartEdgeAttributes(true, false), `_to_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
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

        const shouldKeepClusterSpecificAttributes = isEnterprise || isCluster;

        const res = tryCreate({...vertex, type: 2, name: vertexName});
        try {
          if (!isEnterprise && isCluster) {
            // We are using a shardingStrategy not allowed in Community version.
            isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {...vertex, type: 2, name: vertexName});
          } else {
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
              // Single Server always falls back default replicationFactor
              vertex.replicationFactor = 1;
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
            if (isServer) {
              // Server has no special DC2DC case to let isDisjoint pass
              if (isEnterprise) {
                vertex.isDisjoint = false;
              } else {
                // Community does not even have the attribute
                delete vertex.isDisjoint;
              }
            }
            validateProperties(vertex, vertexName, 2, shouldKeepClusterSpecificAttributes);
            const resEdge = tryCreate({...edge, type: "edge", name: edgeName});
            try {
              // Should work, everything required for smartGraphs is there.
              assertTrue(resEdge.result, `Result: ${JSON.stringify(resEdge)}`);
              if (!isCluster) {
                // The following are not available in single server
                if (!isEnterprise) {
                  // Well unless we are in enterprise.
                  delete edge.isSmart;
                  delete edge.isDisjoint;
                }
                delete edge.shardingStrategy;
                // Single Server always falls back default replicationFactor
                edge.replicationFactor = 1;
                // And also erases distributeShardsLike
                delete edge.distributeShardsLike;
              } else {
                if (!isEnterprise) {
                  // smartFeatures cannot be set
                  edge.isSmart = false;
                  edge.isDisjoint = false;
                  edge.shardingStrategy = "hash";
                }
              }
              if (isServer) {
                // Server has no special DC2DC case to let isDisjoint pass
                if (isEnterprise) {
                  edge.isDisjoint = false;
                } else {
                  // Community does not even have the attribute
                  delete edge.isDisjoint;
                }
                if (isCluster) {
                  // SmartEdge Leader
                  edge.internalValidatorType = 0;
                }
              }
              if (isEnterprise && isCluster) {
                // Check special creation path
                const makeSmartEdgeAttributes = (isTo, isLocal) => {
                  const tmp = {...edge, isSystem: true, isSmart: false, shardingStrategy: "hash"};
                  if (isServer) {
                    tmp.internalValidatorType = isLocal ? 2 : 4;
                    tmp.isSmartChild = true;
                  }
                  if (isTo) {
                    tmp.shardKeys = [":_key"];
                  }
                  return tmp;
                };
                validateProperties({...edge, numberOfShards: 0}, edgeName, 3, shouldKeepClusterSpecificAttributes);
                validateProperties(makeSmartEdgeAttributes(false, true), `_local_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
                if (!isDisjoint || isServer) {
                  validateProperties(makeSmartEdgeAttributes(false, false), `_from_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
                  validateProperties(makeSmartEdgeAttributes(true, false), `_to_${edgeName}`, 3, shouldKeepClusterSpecificAttributes);
                } else {
                  assertEqual(db._collections().filter(c => c.name() === `_from_${edgeName}` || c.name() === `_to_${edgeName}`).length, 0, "Created incorrect hidden collections");
                }
              } else {
                validateProperties(edge, edgeName, 3, shouldKeepClusterSpecificAttributes);
              }
            } finally {
              db._drop(edgeName, true);
            }
          }
        } finally {
          db._drop(vertexName, true);
        }
      }
    },

    testDuplicateName: function () {
      const res = tryCreate({name: collname});
      try {
        // First run should work
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({}, collname, 2);

        const resAgain = tryCreate({name: collname});
        isDisallowed(ERROR_HTTP_CONFLICT.code, ERROR_ARANGO_DUPLICATE_NAME.code, resAgain, {name: collname});
      } finally {
        db._drop(collname);
      }
    },

    testUnknownParameters: function () {
      const unknownParameters = ["test", "foo", "", "doCompact", "isVolatile"];

      for (const p of unknownParameters) {
        const res = tryCreate({name: collname, [p]: "justIgnoreMe"});
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
      const res = tryCreate({name: collname, computedValues});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        // In the response computed values are enriched with more information
        computedValues[0].computeOn = ["insert", "update", "replace"];
        computedValues[0].failOnWarning = false;
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
      const res = tryCreate({name: collname, computedValues});
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {computedValues});
      } finally {
        // Should be noop
        db._drop(collname);
      }
    },

    testIllegalReplicationWriteConcern: function () {
      const res = tryCreate({name: collname, writeConcern: 3, replicationFactor: 2});
      try {
        if (isCluster) {
          isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {
            writeConcern: 3,
            replicationFactor: 2
          });
        } else {
          // On single servers those values are just ignored
          validateProperties({}, collname, 2);
        }
      } finally {
        // Should be noop
        db._drop(collname);
      }
    },

    testCreateSystemCollection: function () {
      const systemName = "_testCollection";
      try {
        const res = tryCreate({name: systemName, isSystem: true});
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        validateProperties({isSystem: true}, systemName, 2);
      } finally {
        db._drop(systemName, {isSystem: true});
      }
    },

    testCreateSystemCollectionName: function () {
      const systemName = "_testCollection";
      const res = tryCreate({name: systemName});
      isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_ARANGO_ILLEGAL_NAME.code, res, {});
    },

    testDatabaseConfiguration: function () {
      db._createDatabase("UnitTestConfiguredDB", {replicationFactor: 3, writeConcern: 2});
      try {
        db._useDatabase("UnitTestConfiguredDB");

        const res = tryCreate({name: collname});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          if (isCluster) {
            // Needs to take into account the database defaults (also note minReplicationFactor is returned with value equal to writeConcern)
            validateProperties({replicationFactor: 3, writeConcern: 2, minReplicationFactor: 2}, collname, 2);
          } else {
            // The single server just does not care.
            validateProperties({}, collname, 2);
          }
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
        const res = tryCreate({name: collname, [prop]: ignoredValue});
        try {
          switch (prop) {
            case "type": {
              if (!isServer && typeof ignoredValue === "number" && ignoredValue < 0) {
                // Client gets into VPack out of range
                isDisallowed(ERROR_HTTP_SERVER_ERROR.code, ERROR_INTERNAL.code, res, testParam);
              } else {
                // But well we can create all others, obviously defaulting to 2 (document)
                isAllowed(res, collname, testParam);
              }
              break;
            }
            case "computedValues": {
              isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              break;
            }
            case "shardKeys": {
              isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              break;
            }
            case "numberOfShards": {
              if (isCluster) {
                if (typeof ignoredValue === "number") {
                  if (ignoredValue < 0) {
                    // This is actually a VPack Error.
                    // We should consider to make this a non-internal Error.
                    isDisallowed(ERROR_HTTP_SERVER_ERROR.code, ERROR_INTERNAL.code, res, testParam);
                  } else {
                    assertTrue(res.result, `Result: ${JSON.stringify(res)} on input ${JSON.stringify(testParam)}`);
                    validateProperties({numberOfShards: Math.floor(ignoredValue)}, collname, 2);
                  }
                } else {
                  isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
                  // isAllowed(res, collname, testParam);
                }
              } else {
                if (typeof ignoredValue === "number") {
                  if (ignoredValue < 0) {
                    // This is actually a VPack Error.
                    // We should consider to make this a non-internal Error.
                    isDisallowed(ERROR_HTTP_SERVER_ERROR.code, ERROR_INTERNAL.code, res, testParam);
                  } else {
                    isAllowed(res, collname, testParam);
                  }
                } else {
                  isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
                }
              }
              break;
            }
            case "schema": {
              isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_VALIDATION_BAD_PARAMETER.code, res, testParam);
              break;
            }
            case "replicationFactor":
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
                  isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
                }
              } else {
                // Just ignore if we are not in cluster.
                isAllowed(res, collname, testParam);
              }
              break;
            }
            case "syncByRevision": {
              isAllowed(res, collname, testParam);
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

function CreateCollectionsInOneShardSuite() {
  const collname = "UnitTestCollection";
  const oneShardLeader = "_graphs";
  const fixedOneShardValues = ["numberOfShards", "replicationFactor", "minReplicationFactor", "writeConcern"];
  const getOneShardShardingValues = () => {
    const props = _.pick(db[oneShardLeader].properties(), fixedOneShardValues);
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

    testCreateMinimalOneShard: function () {
      const res = tryCreate({name: collname});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (isCluster) {
          validateProperties(getOneShardShardingValues(), collname, 2);
        } else {
          // OneShard has no meaning in single server, just assert values are taken
          validateProperties({}, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testCreateWithOtherShardValues: function () {
      for (const v of fixedOneShardValues) {
        // all are numeric values. None of them can be modified in one shard.
        const res = tryCreate({name: collname, [v]: 2});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          if (isCluster) {
            validateProperties(getOneShardShardingValues(), collname, 2);
          } else {
            // OneShard has no meaning in single server, just assert values are taken
            validateProperties({}, collname, 2);
          }
        } finally {
          db._drop(collname);
        }
      }
    },

    testCreateWithDistributeShardsLike: function () {
      const otherLeaderName = "UnitTestLeader";
      const leaderRes = tryCreate({name: otherLeaderName});
      try {
        // Should be allowed
        assertTrue(leaderRes.result, `Result: ${JSON.stringify(leaderRes)}`);
        const res = tryCreate({name: collname, distributeShardsLike: otherLeaderName});
        try {
          // Well we just ignore that something is off here...
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          if (isCluster) {
            validateProperties(getOneShardShardingValues(), collname, 2);
          } else {
            // OneShard has no meaning in single server, just assert default values are hold
            validateProperties({}, collname, 2);
            validatePropertiesDoNotExist(collname, ["distributeShardsLike"]);
          }
        } finally {
          db._drop(collname);
        }
      } finally {
        db._drop(otherLeaderName);
      }
    },

    testCreateMultipleShardKeysOneShard: function () {
      const res = tryCreate({name: collname, shardKeys: ["foo", "bar"]});
      try {
        if (isCluster) {
          isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {shardKeys: ["foo", "bar"]});
        } else {
          // In single servers shardKeys have no effect. They are ignored, hence the create works here
          isAllowed(res, collname, {shardKeys: ["foo", "bar"]});
          validatePropertiesDoNotExist(collname, ["shardKeys"]);
        }
      } finally {
        // Should be a noop.
        db._drop(collname);
      }
    }
  };
}

jsunity.run(CreateCollectionsSuite);
jsunity.run(IgnoreIllegalTypesSuite);
jsunity.run(CreateCollectionsInOneShardSuite);

return jsunity.done();