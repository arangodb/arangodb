/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, assertFalse, assertNotEqual, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {arango, db, ArangoError} = require("@arangodb");
const isCluster = require('internal').isCluster();
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
  ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,
  ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE,
  ERROR_ARANGO_INVALID_KEY_GENERATOR,
  ERROR_INTERNAL,
  ERROR_ARANGO_ILLEGAL_NAME,
  ERROR_HTTP_CORRUPTED_JSON
} = require("internal").errors;

const filterNotExposedAttributes = (attributes) => {
  // The following attributes are not always returned by the Collection Properties API
  // However the user can still send them to the API. Erase them here as they should
  // not be returned by the server.

  // Also note: The way this filter is used, the test will fail, if the properties
  // of a collection expose any of the following ignored attributes.
  if (!isCluster) {
    // NOTE: Some attributes are not on this list.
    // 1) writeConcern is actually exposed on the SingleServer
    // 2) DistributeShardsLike is exposed to simulate SmartGraphs on SingleServer in Enterprise
    // 3) shardingStrategy: is erased later, because the following are only optionally erased on single server

    const enterpriseSimulationAttributes = ["numberOfShards", "shardKeys", "replicationFactor"];
    for (const key of enterpriseSimulationAttributes) {
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
      "replicationFactor": 2,
      "writeConcern": 1,
      "shardingStrategy": "hash",
      "cacheEnabled": false,
      "computedValues": null,
      "syncByRevision": true,
      "schema": null,
      "isDisjoint": false
    };
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
    return err;
  }
};

const defaultProps = getDefaultProps();
const validateProperties = (overrides, colName, type, keepEnterpriseSimulationAttributes = false) => {
  if (!isCluster) {
    // SingleServer never exposes a shardingStrategy
    delete overrides.shardingStrategy;
  }
  if (!keepEnterpriseSimulationAttributes) {
    overrides = filterNotExposedAttributes(overrides);
  }
  const col = db._collection(colName);
  const props = col.properties();
  assertTrue(props.hasOwnProperty("globallyUniqueId"));
  if (isCluster && db._properties().replicationVersion === "2") {
    // Replication 2 always exposes the group id
    assertTrue(props.hasOwnProperty("groupId"), `${JSON.stringify(props)} is missing groupId`);
  }
  assertEqual(col.name(), colName);
  assertEqual(col.type(), type);
  const expectedProps = {...defaultProps, ...overrides};
  for (const [key, value] of Object.entries(expectedProps)) {
    assertEqual(props[key], value, `Differ on key ${key} Got: ${JSON.stringify(props)} , expected: ${JSON.stringify(expectedProps)}`);
  }
  // Note we add +1 on expected for the globallyUniqueId.
  const expectedKeys = Object.keys(expectedProps);

  if (!overrides.hasOwnProperty("globallyUniqueId")) {
    // The globallyUniqueId is generated, so we cannot compare equality, we should make
    // sure it is always there.
    expectedKeys.push("globallyUniqueId");
  }
  if (isCluster && db._properties().replicationVersion === "2" && !overrides.hasOwnProperty("groupId")) {
    // Replication 2 always exposes the group id
    expectedKeys.push("groupId");
  }
  const foundKeys = Object.keys(props);
  assertEqual(expectedKeys.length, foundKeys.length, `Check that all reported properties are expected. Missing: ${JSON.stringify(_.difference(expectedKeys, foundKeys))} unexpected: ${JSON.stringify(_.difference(foundKeys, expectedKeys))}`);
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

const validatePropertiesDoNotExist = (colName, illegalProperties) => {
  const col = db._collection(colName);
  const props = col.properties();
  for (const key of illegalProperties) {
    assertFalse(props.hasOwnProperty(key), `Property ${key} should not exist`);
  }
};

const clearLogs = () => {
  arango.DELETE("/_admin/log/entries");
};

const validateNoLogsLeftBehind = () => {
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

  const smartVertexName = "UnitTestSmartVertex";
  const smartEdgeName = "UnitTestSmartEdge";

  const createAndValidateSmartGraph = (vertex, isDisjoint) => {
    const edge = {
      numberOfShards: 3,
      replicationFactor: 2,
      isSmart: true,
      shardingStrategy: "enterprise-hash-smart-edge",
      shardKeys: [
        "_key:"
      ],
      isDisjoint: isDisjoint,
      distributeShardsLike: smartVertexName
    };

    const res = tryCreate({ ...vertex, type: 2, name: smartVertexName });
    try {
      assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
      validateProperties(vertex, smartVertexName, 2, true);
      const resEdge = tryCreate({ ...edge, type: 3, name: smartEdgeName });
      try {
        // Should work, everything required for smartGraphs is there.
        assertTrue(resEdge.result, `Result: ${JSON.stringify(resEdge)}`);
        if (isCluster) {
          // Check special creation path
          const makeSmartEdgeAttributes = (isTo) => {
            const tmp = { ...edge, isSystem: true, isSmart: false, shardingStrategy: "hash" };
            if (isTo) {
              tmp.shardKeys = [":_key"];
            }
            return tmp;
          };
          validateProperties({ ...edge, numberOfShards: 0 }, smartEdgeName, 3, true);
          validateProperties(makeSmartEdgeAttributes(false), `_local_${smartEdgeName}`, 3, true);
          if (!isDisjoint) {
            validateProperties(makeSmartEdgeAttributes(false), `_from_${smartEdgeName}`, 3, true);
            validateProperties(makeSmartEdgeAttributes(true), `_to_${smartEdgeName}`, 3, true);
          } else {
            assertEqual(db._collections().filter(c => c.name() === `_from_${smartEdgeName}` || c.name() === `_to_${smartEdgeName}`).length, 0, "Created incorrect hidden collections");
          }
        } else {
          validateProperties(edge, smartEdgeName, 3, true);
        }
      } finally {
        db._drop(smartEdgeName, true);
      }
    } finally {
      db._drop(smartVertexName, true);
    }
  };

  return {

    setUpAll() {
      // Make sure we start without buffered logs
      clearLogs();
    },

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
          assertTrue(err.code === ERROR_HTTP_SERVER_ERROR.code || err.code === ERROR_HTTP_BAD_PARAMETER.code, `Got response ${err} for body ${JSON.stringify(body)}`);
          assertNotEqual(allowedErrorsCodes.indexOf(err.errorNum), -1 , `Got response ${err.errorNum}, ${err.message} for body ${JSON.stringify(body)}, ${err}`);
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
        validateProperties({}, collname, 3);
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
        const res = tryCreate({ name: collname, type });
        try {
          isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_ARANGO_COLLECTION_TYPE_INVALID.code, res, { type });
        } finally {
          db._drop(collname);
        }
      }

      // arangosh translates "document" and "edge", and does not send
      // non-numeric types or objects at all, so the server uses the default.
      const typeHandledByClient = [
        "test",
        "document",
        "edge",
        {},
        []
      ];
      for (const type of typeHandledByClient) {
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

      const res = tryCreate({ name: collname, type: true });
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, { type: true });
      } finally {
        db._drop(collname);
      }
    },

    testCreateEdgeCollectionFlooredInput: function () {
      const flooredEdgeTypeNumbers = [
        3.1,
        3.9
      ];
      for (const type of flooredEdgeTypeNumbers) {
        const res = tryCreate({name: collname, type});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          validateProperties({}, collname, 3);
        } finally {
          db._drop(collname);
        }
      }
    },

    testCreateDocumentCollectionFlooredInput: function () {
      const flooredDocumentTypeNumbers = [
        2.1,
        2.9
      ];
      for (const type of flooredDocumentTypeNumbers) {
        const res = tryCreate({name: collname, type});
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          // Just floor the value
          validateProperties({}, collname, 2);
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
      const shards = { "s01": ["server1", "server2"], "s02": ["server3"] };
      const res = tryCreate({ name: collname, shards });
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, { shards });
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
      const res = tryCreate({ name: collname, replicationFactor: "satellite" });
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        // WriteConcern is forced to 0 on satellites
        // NOTE: SingleServer for some reason this create returns MORE properties, then the others.
        if (!isCluster) {
          validateProperties({
            replicationFactor: "satellite",
            writeConcern: 0,
            isSmart: false,
            shardKeys: ["_key"],
            numberOfShards: 1,
            isDisjoint: false
          }, collname, 2, true);
        } else {
          validateProperties({ replicationFactor: "satellite", writeConcern: 0 }, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testCreateReplicationFactorZero: function () {
      // 0 does not stand for "satellite"
      const res = tryCreate({ name: collname, replicationFactor: 0 });
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, { replicationFactor: 0 });
      } finally {
        db._drop(collname);
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
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, { isSmart: true });
      } finally {
        db._drop(collname);
      }
    },
    testSmartGraphAttribute: function () {
      const input = { smartGraphAttribute: "test" };
      const res = tryCreate({ name: collname, ...input });

      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, input);
      } finally {
        db._drop(collname);
      }
    },

    testSmartGraphAttributeIsSmart: function () {
      const input = { smartGraphAttribute: "test", isSmart: true };
      const res = tryCreate({ name: collname, ...input });
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, input);
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoin: function () {
      const res = tryCreate({name: collname, smartJoinAttribute: "test"});
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_INVALID_SMART_JOIN_ATTRIBUTE.code, res, { smartJoinAttribute: "test" });
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoinIsSmart: function () {
      const res = tryCreate({name: collname, smartJoinAttribute: "test", isSmart: true});
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, { smartJoinAttribute: "test", isSmart: true });
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoinPrefixShardKey: function () {
      const res = tryCreate({name: collname, smartJoinAttribute: "test", numberOfShards: 3, shardKeys: ["_key:"]});
      try {
        assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
        if (!isCluster) {
          validateProperties({}, collname, 2);
          validatePropertiesDoNotExist(collname, ["smartJoinAttribute"]);
        } else {
          validateProperties({ smartJoinAttribute: "test", numberOfShards: 3, shardKeys: ["_key:"] }, collname, 2);
        }
      } finally {
        db._drop(collname);
      }
    },

    testSmartJoinCorrect: function () {
      const leader = tryCreate({name: leaderName, numberOfShards: 3});
      try {
        const props = {
          smartJoinAttribute: "test",
          shardKeys: ["_key:"],
          distributeShardsLike: leaderName
        };
        const res = tryCreate({
          name: collname,
          ...props
        });
        try {
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          if (!isCluster) {
            validateProperties({}, collname, 2);
            validatePropertiesDoNotExist(collname, ["smartJoinAttribute"]);
          } else {
            // Number of Shards inherited from Leader
            validateProperties({ ...props, numberOfShards: 3 }, collname, 2);
          }
        } finally {
          db._drop(collname);
        }
      } finally {
        db._drop(leaderName);
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
          assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
          assertEqual(db._collections().filter(c => c.name() === name).length, 1, `Collection ${name} should exist`);
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
      const res = tryCreate({name: collname, type: 2, objectId: "deleteMe"});
      try {
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {objectId: "deleteMe"});
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
        createAndValidateSmartGraph(vertex, isDisjoint);
      }
    },

    testCreateSmartAndEnterpriseGraph: function () {
      // This is actually a broken result:
      for (const isDisjoint of [true, false]) {
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
        createAndValidateSmartGraph(vertex, isDisjoint);
      }
    },

    testCreateEnterpriseGraph: function () {
      for (const isDisjoint of [true, false]) {
        const vertex = {
          numberOfShards: 3,
          replicationFactor: 2,
          isSmart: true,
          shardingStrategy: "enterprise-hex-smart-vertex",
          shardKeys: [
            "_key:"
          ],
          isDisjoint: isDisjoint
        };
        createAndValidateSmartGraph(vertex, isDisjoint);
      }
    },

    testCreateSmartEdgeWithoutDistributeShardsLike: function () {
      if (!isCluster) {
        return;
      }

      const vertexName = "UnitTestSmartVertex";
      const edgeName = "UnitTestSmartEdgeNoDSL";

      // Intentionally don't refer to this coll via distributeShardsLike.
      const vertex = tryCreate({
        name: vertexName,
        type: 2,
        numberOfShards: 3,
        replicationFactor: 2,
        isSmart: true,
        shardKeys: ["_key:"],
        smartGraphAttribute: "test",
      });

      try {
        assertTrue(vertex.result, `Vertex create failed: ${JSON.stringify(vertex)}`);

        // Without distributeShardsLike
        try {
          db._create(edgeName, {
            replicationFactor: 2,
            isSmart: true,
            shardKeys: ["_key:"],
            numberOfShards: 1
          }, "edge");

          fail(`Expected creation of smart edge without distributeShardsLike to fail`);
        } catch (err) {
          assertEqual(err.code, ERROR_HTTP_BAD_PARAMETER.code, `Unexpected error: ${JSON.stringify(err)}`);
          assertEqual(err.errorNum, ERROR_BAD_PARAMETER.code, `Unexpected errorNum: ${JSON.stringify(err)}`);

          const msg = String(err.message || "");
          assertTrue(
            msg.toLowerCase().includes("distributeshardslike"),
            `Expected error message to mention distributeShardsLike, got: ${msg}`
          );
        }
      } finally {
        try { db._drop(edgeName, true); } catch (e) {}
        db._drop(vertexName, true);
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
      for (const p of ["test", "foo", ""]) {
        const res = tryCreate({name: collname, [p]: "rejectMe"});
        try {
          isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {[p]: "rejectMe"});
        } finally {
          db._drop(collname, true);
        }
      }

      // doCompact and isVolatile are old, and have been ignored ever since.
      for (const p of ["doCompact", "isVolatile"]) {
        const res = tryCreate({name: collname, [p]: "justIgnoreMe"});
        try {
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
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {
          writeConcern: 3,
          replicationFactor: 2
        });
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
          // Needs to take into account the database defaults
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


function IllegalTypesSuite() {
  // Lists of illegal type values, for each test to iterate over.
  const testValues = {
    string: [1, -3.6, true, false, [], {foo: "bar"}, null],
    integer: [-3, 5.1, true, false, [2], {foo: "bar"}, "test", null],
    bool: [1, -3.1, [true], {foo: "bar"}, "test", null],
    object: [0, -2.1, true, false, [], "test", null],
    array: [0, -2.1, true, false, {foo: "bar"}, "test", null]
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
    /* Cluster Properties, also checked on single server */
    isSmart: "bool",
    shardKeys: "array",
    numberOfShards: "integer",
    replicationFactor: "integer",
    shardingStrategy: "string",
    isDisjoint: "bool",
    distributeShardsLike: "string"
  };

  const collname = "UnitTestRestore";

  // No Setup or teardown required.
  // Every test in the suite is self-contained
  const suite = {
    setUpAll() {
      // Make sure we start without buffered logs
      clearLogs();
    },

    tearDown() {
      validateNoLogsLeftBehind();
    }
  };

  for (const [prop, type] of Object.entries(propertiesToTest)) {
    suite[`testIllegalEntries${prop}`] = function () {
      for (const ignoredValue of testValues[type]) {
        const testParam = {[prop]: ignoredValue};
        const res = tryCreate({name: collname, [prop]: ignoredValue});
        try {
          switch (prop) {
            case "type": {
              if (typeof ignoredValue === "number" && ignoredValue >= 0) {
                // A non-negative number parses, but is not a known collection type
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_ARANGO_COLLECTION_TYPE_INVALID.code, res, testParam);
              } else if (typeof ignoredValue === "number" || typeof ignoredValue === "boolean") {
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              } else {
                // arangosh does not send other types, so the server uses the default
                isAllowed(res, collname, testParam);
              }
              break;
            }
            case "computedValues": {
              if (ignoredValue === null) {
                assertTrue(res.result, `Result: ${JSON.stringify(res)} on input ${JSON.stringify(testParam)}`);
                validateProperties({computedValues: null}, collname, 2);
              } else {
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              }
              break;
            }
            case "shardKeys": {
              isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              break;
            }
            case "numberOfShards": {
              if (typeof ignoredValue === "number" && ignoredValue >= 0) {
                if (isCluster) {
                  assertTrue(res.result, `Result: ${JSON.stringify(res)} on input ${JSON.stringify(testParam)}`);
                  validateProperties({numberOfShards: Math.floor(ignoredValue)}, collname, 2);
                } else {
                  isAllowed(res, collname, testParam);
                }
              } else {
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              }
              break;
            }
            case "schema": {
              if (ignoredValue === null) {
                assertTrue(res.result, `Result: ${JSON.stringify(res)} on input ${JSON.stringify(testParam)}`);
                validateProperties({schema: null}, collname, 2);
              } else {
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_VALIDATION_BAD_PARAMETER.code, res, testParam);
              }
              break;
            }
            case "replicationFactor":
            case "writeConcern": {
              // Doubles are floored, so a non-negative number is a valid replicationFactor
              if (prop === "replicationFactor" && typeof ignoredValue === "number" && ignoredValue >= 0) {
                if (isCluster) {
                  // but more than there are DB-Servers
                  isDisallowed(ERROR_HTTP_SERVER_ERROR.code, ERROR_CLUSTER_INSUFFICIENT_DBSERVERS.code, res, testParam);
                } else {
                  isAllowed(res, collname, testParam);
                }
              } else {
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              }
              break;
            }
            case "shardingStrategy": {
              if (ignoredValue === null) {
                isAllowed(res, collname, testParam);
              } else {
                isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
              }
              break;
            }
            case "keyOptions": {
              isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_ARANGO_INVALID_KEY_GENERATOR.code, res, testParam);
              break;
            }
            default: {
              isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, testParam);
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
  const fixedOneShardValues = ["numberOfShards", "replicationFactor", "writeConcern"];
  const getOneShardShardingValues = () => {
    const props = _.pick(db[oneShardLeader].properties(), fixedOneShardValues);
    return {...props, distributeShardsLike: oneShardLeader, shardingStrategy: "hash"};
  };
  return {
    setUpAll: function () {
      db._createDatabase("UnitTestOneShard", {sharding: "single"});
      db._useDatabase("UnitTestOneShard");
      // Make sure we start without buffered logs
      clearLogs();
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase("UnitTestOneShard");
    },

    tearDown() {
      validateNoLogsLeftBehind();
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
        const value = v === "replicationFactor" ? 3 : 2;
        const res = tryCreate({name: collname, [v]: value});
        try {
          if (isCluster && db._properties().replicationVersion === "2" && v === "writeConcern") {
            // For Replication2 the writeConcern is per CollectionGroup. We cannot create a follower with a different one.
            isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {[v]: value});
          } else if (v === "writeConcern") {
            assertTrue(res.result, `Result: ${JSON.stringify(res)}`);
            if (isCluster) {
              // On Replication1 writeConcern is allowed to differ per Collection.
              validateProperties({
                ...getOneShardShardingValues(),
                writeConcern: value,
              }, collname, 2);
            } else {
              validateProperties({ writeConcern: value }, collname, 2);
            }
          } else {
            isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, { [v]: value });
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
          isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE.code, res, { distributeShardsLike: otherLeaderName });
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
        isDisallowed(ERROR_HTTP_BAD_PARAMETER.code, ERROR_BAD_PARAMETER.code, res, {shardKeys: ["foo", "bar"]});
      } finally {
        // Should be a noop.
        db._drop(collname);
      }
    }
  };
}

jsunity.run(CreateCollectionsSuite);
jsunity.run(IllegalTypesSuite);
jsunity.run(CreateCollectionsInOneShardSuite);

return jsunity.done();
