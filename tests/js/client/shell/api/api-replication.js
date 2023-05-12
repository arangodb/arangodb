/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {arango, db} = require("@arangodb");

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
        assertEqual(Object.keys(expectedProps).length + 1, Object.keys(props).length, `Check that all properties are reported`);
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
    };
}

jsunity.run(RestureCollectionSuite);

return jsunity.done();