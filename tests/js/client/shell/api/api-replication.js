/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual, assertFalse */

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

    - some bogus parameters
 */

function RestureCollectionSuite() {

    const tryRestore = (parameters) => {
        return arango.PUT('/_api/replication/restore-collection', {
            parameters, indexes: []
        });
    };
    const defaultProps = {
        "isSystem" : false,
        "waitForSync" : false,
        "keyOptions" : {
            "allowUserKeys" : true,
            "type" : "traditional",
            "lastValue" : 0
        },
        "writeConcern" : 1,
        "cacheEnabled" : false,
        "computedValues" : null,
        "syncByRevision" : true,
        "schema" : null
    };
    const validateProperties = (overrides, colName, type) => {
        const col = db._collection(collname);
        const props = col.properties(true);
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

    const validatePropertiesDoNotExist = (colName, illegalProperties)  => {
      const col = db._collection(collname);
      const props = col.properties(true);
      for (const key of illegalProperties) {
        assertFalse(props.hasOwnProperty(key), `Property ${key} should not exist`);
      }
    };
    const collname = "UnitTestRestore";

    return {

        testRestoreNonObject: function() {
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
                assertTrue(res.result);
                validateProperties({}, collname, 2);
            } finally {
                db._drop(collname);
            }
        },

        testRestoreEdge: function () {
            const res = tryRestore({name: collname, type: 3});
            try {
                assertTrue(res.result);
                validateProperties({}, collname, 3);
            } finally {
                db._drop(collname);
            }
        },

        testRestoreDocument: function () {
            const res = tryRestore({name: collname, type: 2});
            try {
                assertTrue(res.result);
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
                assertEqual(res.code, 400);
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
                    assertTrue(res.result);
                    validateProperties({}, collname, 2);
                } finally {
                    db._drop(collname);
                }
            }
        },

        testRestoreDeleted: function () {
            const res = tryRestore({name: collname, type: 2, deleted: true});
            try {
                assertTrue(res.result);
                assertEqual(db._collections().filter((c) => c.name() === collname).length, 0, `Deleted collection was restored.`);
            } finally {
                db._drop(collname);
            }
        },

        testRestoreNumberOfShards: function () {
            const res = tryRestore({name: collname, numberOfShards: 3});
            try {
                assertTrue(res.result);
                validateProperties({numberOfShards: 3}, collname, 2);
            } finally {
                db._drop(collname);
            }
        },

        testRestoreListOfShards: function () {
            const res = tryRestore({name: collname, shards: {"s01": ["server1", "server2"], "s02": ["server3"]}});
            try {
                assertTrue(res.result);
                validateProperties({numberOfShards: 2}, collname, 2);
            } finally {
                db._drop(collname);
            }
        },

        testRestoreReplicationFactor: function () {
            const res = tryRestore({name: collname, replicationFactor: 3});
            try {
                assertTrue(res.result);
                validateProperties({replicationFactor: 3}, collname, 2);
            } finally {
                db._drop(collname);
            }
        },

      testRestoreReplicationFactor0: function () {
        const res = tryRestore({name: collname, replicationFactor: 0});
        try {
          assertTrue(res.result);
          validateProperties({replicationFactor: "satellite"}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testRestoreReplicationSatellite: function () {
        const res = tryRestore({name: collname, replicationFactor: "satellite"});
        try {
          assertTrue(res.result);
          validateProperties({replicationFactor: "satellite"}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testWriteConcern: function () {
        const res = tryRestore({name: collname, writeConcern: 2, replicationFactor: 3});
        try {
          assertTrue(res.result);
          validateProperties({replicationFactor: 3, writeConcern: 2}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testMinReplicationFactor: function () {
        const res = tryRestore({name: collname, minReplicationFactor: 2, replicationFactor: 3});
        try {
          assertTrue(res.result);
          validateProperties({replicationFactor: 3, writeConcern: 2}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testGloballyUniqueIdReal: function () {
        const globallyUniqueId = "h843EE8622105/182";
        const res = tryRestore({name: collname, globallyUniqueId});
        try {
          require("internal").print(db._collection(collname).properties());
          assertTrue(res.result);
          validateProperties({globallyUniqueId}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testGloballyUniqueIdOtherString: function () {
        const res = tryRestore({name: collname, globallyUniqueId: "test"});
        try {
          assertTrue(res.result);
          validateProperties({globallyUniqueId: "test"}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testGloballyUniqueIdNumeric: function () {
        const res = tryRestore({name: collname, globallyUniqueId: 123456});
        try {
          assertTrue(res.result);
          validateProperties({globallyUniqueId: "test"}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testIsSmartStandAlone: function () {
        const res = tryRestore({name: collname, isSmart: true});
        try {
          assertTrue(res.result);
          validateProperties({isSmart: true}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },
      testSmartGraphAttribute: function () {
        const res = tryRestore({name: collname, smartGraphAttribute: "test"});
        try {
          assertTrue(res.result);
          validateProperties({smartGraphAttribute: "test"}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },
      testSmartGraphAttributeIsSmart: function () {
        const res = tryRestore({name: collname, smartGraphAttribute: "test", isSmart: true});
        try {
          assertTrue(res.result);
          validateProperties({smartGraphAttribute: "test", isSmart: true}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testSmartJoin: function () {
        const res = tryRestore({name: collname, smartJoinAttribute: "test"});
        try {
          assertTrue(res.result);
          validateProperties({smartJoinAttribute: "test"}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testSmartJoinIsSmart: function () {
        const res = tryRestore({name: collname, smartJoinAttribute: "test", isSmart: true});
        try {
          assertTrue(res.result);
          validateProperties({smartJoinAttribute: "test", isSmart: true}, collname, 2);
        } finally {
          db._drop(collname);
        }
      },

      testShadowCollections: function () {
        const res = tryRestore({name: collname, shadowCollections: ["123", "456", "789"]});
        try {
          assertTrue(res.result);
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
              assertTrue(res.result);
              assertEqual(db._collections().filter(c => c.name() === name).length, 0, `Collection ${name} should not exist`);
            } finally {
              db._drop(name, true);
            }
          }
      }


    };
}

jsunity.run(RestureCollectionSuite);

return jsunity.done();