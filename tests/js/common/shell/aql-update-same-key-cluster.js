/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB Inc, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = internal.db;
const arangodb = require("@arangodb");
const ERRORS = arangodb.errors;

// helper
const isCluster = internal.isCluster();


function ValidateSameKeyQueryClusterSuite () {
    const collectionName = "testCollection";
    let testCollection;

    return {
        setUp: () => {
            try {
                db._drop(collectionName);
            } catch (ex) {}
            testCollection = db._create(collectionName);
            for (let i = 0; i < 200; ++i) {
                db[testCollection].insert({"_key": `test${i}`});
            }
            assertEqual(db[testCollection].count(), 200);
        },

        tearDown: () => {
            try {
                db._drop(collectionName);
            } catch (ex) {}
        },

        // update ////////////////////////////////////////////////////////////////////////////////////////////
        testUpdateShellFunctionWithSameKey: () => {
            const doc = testCollection.any();
            testCollection.update(doc._key, {"_key": ${doc._key}, "name": "test"});
            assertTrue(Array.isArray(doc.numArray));
        },

        testUpdateShellFunctionWithDifferentKey: () => {
            const doc = testCollection.any();
            testCollection.update(doc._key, `{"_key": "abc", "name": "test"}`);
            assertTrue(Array.isArray(doc.numArray));
        },

        testAQLUpdateSingleDocWithSameKey: () => {
            const doc = testCollection.any();

            const res = db._query(`UPDATE "${doc._key}" WITH {"_key": "${doc._key}", "age": 0} IN ${collectionName}`).toArray();
            assertTrue(res, []);
        },

        testAQLUpdateSingleDocWithDifferentKey: () => {
            const doc = testCollection.any();

            const res = db._query(`UPDATE "${doc._key}" WITH {"_key": "abc", "age": 0} IN ${collectionName}`).toArray();
            assertTrue(res, []);
        },

        testAQLUpdateAllWithSameKey: () => {
            const res = db._query(`FOR doc IN ${collectionName} UPDATE "${doc._key}" WITH {"_key": "${doc._key}", "age": 0} IN ${collectionName}`).toArray();
            assertTrue(res, []);
        },

    }; // return
}

if (isCluster) {
    jsunity.run(ValidateSameKeyQueryClusterSuite);
}

return jsunity.done();
