/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertTrue, assertNotEqual */

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
/// @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const db = require("internal").db;
const {
    randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");

const { versionHas } = require("@arangodb/test-helper");

const dbName = "vectorDB";
const collName = "coll";
const indexName = "vectorIndex";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexCreateAndRemoveTestSuite() {
    let collection;
    const dimension = 500;
    const seed = 12132390894;
    let randomPoint;
    const insertedDocsCount = 100;
    let insertedDocs = [];

    return {
        setUp: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < insertedDocsCount; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                if (i === 50) {
                    randomPoint = vector;
                }
                docs.push({
                    vector
                });
            }
            insertedDocs = db.coll.insert(docs);

            collection.ensureIndex({
                name: "vector_l2",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                params: {
                    metric: "l2",
                    dimension,
                    nLists: 1
                },
            });
        },

        tearDown: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testInsertPoints: function() {
            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < insertedDocsCount; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                docs.push({
                    vector
                });
            }
            collection.insert(docs);

            assertEqual(collection.count(), insertedDocsCount * 2);
        },

        testInsertVectorWithMixOfIntergerAndDoubleComponents: function() {
            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            let vector = Array.from({
                length: dimension
            }, () => gen());
            vector[0] = 0;
            vector[1] = 1;
            vector[2] = 2;
            vector[3] = -1;

            docs.push({
                vector
            });
            collection.insert(docs);

            assertEqual(collection.count(), insertedDocsCount + 1);
        },

        testInsertPointsChangesSearchResults: function() {
            const query = "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp, {nProbe: 10}) " +
                "LIMIT 5 RETURN d";

            const bindVars = {
                qp: randomPoint
            };
            const closesDocKeysPreInsert = db._query(query, bindVars).toArray().map(item => ({
                _key: item._key
            }));
            assertEqual(5, closesDocKeysPreInsert.length);

            let docs = [];
            for (let i = 0; i < 10; ++i) {
                docs.push({
                    vector: randomPoint
                });
            }
            collection.insert(docs);

            const closesDocKeysPostInsert = db._query(query, bindVars).toArray().map(item => ({
                _key: item._key
            }));
            assertEqual(5, closesDocKeysPostInsert.length);

            assertNotEqual(closesDocKeysPreInsert, closesDocKeysPostInsert);
        },

        testRemovePoints: function() {
            const docsToRemove = insertedDocs.slice(0, insertedDocs.length / 2).map(item => ({
                "_key": item._key
            }));

            collection.remove(docsToRemove);

            assertEqual(collection.count(), insertedDocsCount / 2);
        },

        testRemoveChangesSearchResults: function() {
            const query = "FOR d IN " +
                collection.name() +
                " SORT APPROX_NEAR_L2(d.vector, @qp) " +
                "LIMIT 5 RETURN d";

            const bindVars = {
                qp: randomPoint
            };
            const closesDocKeysPreRemove = db._query(query, bindVars).toArray().map(item => ({
                _key: item._key
            }));
            assertEqual(5, closesDocKeysPreRemove.length);

            collection.remove(closesDocKeysPreRemove);

            const closesDocKeysPostRemove = db._query(query, bindVars).toArray().map(item => ({
                _key: item._key
            }));
            assertEqual(5, closesDocKeysPostRemove.length);

            assertNotEqual(closesDocKeysPreRemove, closesDocKeysPostRemove);
        },
    };
}


function VectorIndexTestCreationWithVectors() {
    let collection;
    const dimension = 500;
    const seed = 12132390894;

    return {
        setUp: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });
        },

        tearDown: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testCreateVectorIndexWithZeroNLists: function() {
            let gen = randomNumberGeneratorFloat(seed);

            let docs = [];
            for (let i = 0; i < 100; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                vector[0] = 0;
                docs.push({
                    vector
                });
            }
            collection.insert(docs);

            try {
                let result = collection.ensureIndex({
                    name: "vector_l2",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2",
                        dimension: dimension,
                        nLists: 0,
                        trainingIterations: 10,
                    },
                });
              fail();
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code,
                    e.errorNum);
            }
        },

        testCreateVectorIndexWithVectorsContainingIntegersAndDoubles: function() {
            let gen = randomNumberGeneratorFloat(seed);

            let docs = [];
            for (let i = 0; i < 100; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                vector[0] = 0;
                docs.push({
                    vector
                });
            }
            collection.insert(docs);

            try {
                let result = collection.ensureIndex({
                    name: "vector_l2",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2",
                        dimension: dimension,
                        nLists: 1,
                        trainingIterations: 10,
                    },
                });
            } catch (e) {
                assertEqual(undefined, e);
            }
        },

        testCreateVectorIndexWithDifferentDimensionVectors: function() {
            let gen = randomNumberGeneratorFloat(seed);

            let docs = [];
            for (let i = 0; i < 100; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                docs.push({
                    vector
                });
            }
            collection.insert(docs);

            try {
                let result = collection.ensureIndex({
                    name: "vector_l2",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    params: {
                        metric: "l2",
                        dimension: dimension - 10,
                        nLists: 1,
                        trainingIterations: 10,
                    },
                });
              fail();
            } catch (e) {
                // This is for some reason handled differently...
                assertNotEqual(e, undefined);
            }
        },

        testCreatingVectorIndexWhenFieldNotPresent: function() {
            let gen = randomNumberGeneratorFloat(seed);

            let docs = [];
            for (let i = 0; i < 20; ++i) {
                if (i > 10) {
                  const vector = Array.from({
                      length: dimension
                  }, () => gen());
                  docs.push({vector});
                } else {
                  docs.push({value: i});
                }
            }
            collection.insert(docs);

            try {
                let result = collection.ensureIndex({
                    name: "vector_l2",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    sparse: false,
                    params: {
                        metric: "l2",
                        dimension: dimension,
                        nLists: 1,
                        trainingIterations: 10,
                    },
                });
              fail();
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
            }
        },

        testCreatingVectorIndexWhenFieldNotPresentAndSparse: function() {
            let gen = randomNumberGeneratorFloat(seed);

            let docs = [];
            for (let i = 0; i < 20; ++i) {
                if (i > 10) {
                  const vector = Array.from({
                      length: dimension
                  }, () => gen());
                  docs.push({vector});
                } else {
                  docs.push({value: i});
                }
            }
            collection.insert(docs);

            try {
                let result = collection.ensureIndex({
                    name: "vector_l2",
                    type: "vector",
                    fields: ["vector"],
                    inBackground: false,
                    sparse: true,
                    params: {
                        metric: "l2",
                        dimension: dimension,
                        nLists: 1,
                        trainingIterations: 10,
                    },
                });
            } catch (e) {
                assertEqual(undefined, e);
            }
        },

        testCreatingVectorIndexNoFields: function() {
            let gen = randomNumberGeneratorFloat(seed);

            let docs = [];
            for (let i = 0; i < 30; ++i) {
                if (i >= 20) {
                  // Documents with vector and all stored value fields
                  const vector = Array.from({
                      length: dimension
                  }, () => gen());
                  docs.push({vector, name: `doc_${i}`, category: `cat_${i % 3}`, value: i});
                } else if (i >= 10) {
                  // Documents with vector but missing some stored value fields
                  const vector = Array.from({
                      length: dimension
                  }, () => gen());
                  docs.push({vector, name: `doc_${i}`});
                } else {
                  // Documents without vector field
                  docs.push({name: `doc_${i}`, category: `cat_${i % 3}`, value: i});
                }
            }
            collection.insert(docs);

            try {
                let result = collection.ensureIndex({
                    name: "vector_l2_stored",
                    type: "vector",
                    inBackground: false,
                    storedValues: ["name", "category", "value"],
                    params: {
                        metric: "l2",
                        dimension: dimension,
                        nLists: 1,
                        trainingIterations: 10,
                    },
                });
                fail();
            } catch (e) {
                assertEqual(errors.ERROR_BAD_PARAMETER.code, e.errorNum);
            }
        },
    };
}


function VectorIndexStoredValuesTestSuite() {
    let collection;
    const dimension = 128;
    const seed = 123456789;
    let randomPoint;
    const insertedDocsCount = 50;
    let insertedDocs = [];

    return {
        setUp: function() {
            db._createDatabase(dbName);
            db._useDatabase(dbName);

            collection = db._create(collName, {
                numberOfShards: 3
            });

            // Insert test documents with various fields for storedValues testing
            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < insertedDocsCount; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                if (i === 25) {
                    randomPoint = vector;
                }
                docs.push({
                    vector,
                    name: `doc_${i}`,
                    value: i * 10,
                    category: i % 3 === 0 ? "A" : i % 3 === 1 ? "B" : "C",
                    metadata: {
                        created: new Date().toISOString(),
                        tags: [`tag${i}`, `category${i % 3}`],
                        score: Math.random()
                    },
                    description: `This is document number ${i} with some description text`
                });
            }
            insertedDocs = db.coll.insert(docs);

            // Create vector index with storedValues
            collection.ensureIndex({
                name: "vector_l2_stored",
                type: "vector",
                fields: ["vector"],
                inBackground: false,
                storedValues: ["name", "value", "category", "metadata", "description"],
                params: {
                    metric: "l2",
                    dimension,
                    nLists: 1
                },
            });
        },

        tearDown: function() {
            db._useDatabase("_system");
            db._dropDatabase(dbName);
        },

        testCreateVectorIndexWithStoredValues: function() {
            // Test creating a vector index with storedValues
            const indexInfo = collection.getIndexes().find(idx => idx.name === "vector_l2_stored");
            assertTrue(indexInfo !== undefined, "Vector index with storedValues should exist");
            assertEqual("vector", indexInfo.type);
            assertTrue(indexInfo.storedValues !== undefined, "storedValues should be defined");
            assertEqual(5, indexInfo.storedValues.length, "Should have 5 stored values");
            
            const expectedStoredValues = ["name", "value", "category", "metadata", "description"];
            for (let i = 0; i < expectedStoredValues.length; i++) {
                assertEqual(expectedStoredValues[i], indexInfo.storedValues[i], 
                    `Stored value ${i} should match expected value`);
            }
        },

        testInsertDocumentsWithStoredValues: function() {
            // Test inserting documents when index has storedValues
            const initialCount = collection.count();
            
            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < 10; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                docs.push({
                    vector,
                    name: `new_doc_${i}`,
                    value: (i + 100) * 5,
                    category: i % 2 === 0 ? "X" : "Y",
                    metadata: {
                        created: new Date().toISOString(),
                        tags: [`newtag${i}`],
                        score: Math.random()
                    },
                    description: `New document ${i}`
                });
            }
            
            const insertResult = collection.insert(docs);
            assertEqual(10, insertResult.length, "Should insert 10 documents");
            assertEqual(initialCount + 10, collection.count(), "Collection count should increase by 10");
        },

        testInsertDocumentsWithMissingStoredValues: function() {
            // Test inserting documents with missing storedValues fields
            const initialCount = collection.count();
            
            let docs = [];
            let gen = randomNumberGeneratorFloat(seed);
            for (let i = 0; i < 5; ++i) {
                const vector = Array.from({
                    length: dimension
                }, () => gen());
                // Missing storedValues fields
                docs.push({vector});
            }
            
            const insertResult = collection.insert(docs);
            assertEqual(5, insertResult.length, "Should insert 5 documents");
            assertEqual(initialCount + 5, collection.count(), "Collection count should increase by 5");
        },

        testRemoveDocumentsWithStoredValues: function() {
            // Test removing documents when index has storedValues
            const initialCount = collection.count();
            const docsToRemove = insertedDocs.slice(0, 10).map(item => ({
                "_key": item._key
            }));

            collection.remove(docsToRemove);
            assertEqual(initialCount - 10, collection.count(), "Collection count should decrease by 10");
        },

        testUpdateDocumentsWithStoredValues: function() {
            // Test updating documents when index has storedValues
            const docToUpdate = insertedDocs[0];
            const updateData = {
                name: "updated_name",
                value: 9999,
                category: "UPDATED",
                metadata: {
                    created: new Date().toISOString(),
                    tags: ["updated_tag"],
                    score: 0.999
                },
                description: "This document has been updated"
            };

            collection.update(docToUpdate._key, updateData);
            
            // Verify the update
            const updatedDoc = collection.document(docToUpdate._key);
            assertEqual("updated_name", updatedDoc.name);
            assertEqual(9999, updatedDoc.value);
            assertEqual("UPDATED", updatedDoc.category);
            assertEqual("This document has been updated", updatedDoc.description);
        },

        testReplaceDocumentsWithStoredValues: function() {
            // Test replacing documents when index has storedValues
            const docToReplace = insertedDocs[1];
            const replaceData = {
                vector: randomPoint, // Keep the same vector
                name: "replaced_name",
                value: 8888,
                category: "REPLACED",
                metadata: {
                    created: new Date().toISOString(),
                    tags: ["replaced_tag"],
                    score: 0.888
                },
                description: "This document has been completely replaced"
            };

            collection.replace(docToReplace._key, replaceData);
            
            // Verify the replacement
            const replacedDoc = collection.document(docToReplace._key);
            assertEqual("replaced_name", replacedDoc.name);
            assertEqual(8888, replacedDoc.value);
            assertEqual("REPLACED", replacedDoc.category);
            assertEqual("This document has been completely replaced", replacedDoc.description);
        },

        testStoredValuesWithDifferentDataTypes: function() {
            // Test storedValues with various data types
            let gen = randomNumberGeneratorFloat(seed);
            const vector = Array.from({
                length: dimension
            }, () => gen());
            
            const testDoc = {
                vector,
                name: "test_types", // string
                value: 42, // number
                category: "TEST", // string
                metadata: { // object
                    number: 123,
                    string: "test",
                    boolean: true,
                    array: [1, 2, 3],
                    nullValue: null
                },
                description: "Testing different data types" // string
            };

            const insertResult = collection.insert(testDoc);
            
            // Verify the document was inserted correctly*/
            const insertedDoc = collection.document(insertResult._key);
            assertEqual("test_types", insertedDoc.name);
            assertEqual(42, insertedDoc.value);
            assertEqual("TEST", insertedDoc.category);
            assertEqual("Testing different data types", insertedDoc.description);
            assertTrue(insertedDoc.metadata !== undefined);
            assertEqual(123, insertedDoc.metadata.number);
            assertEqual("test", insertedDoc.metadata.string);
            assertTrue(insertedDoc.metadata.boolean);
            assertEqual(3, insertedDoc.metadata.array.length);
        },

        testStoredValuesIndexSerialization: function() {
            // Test that storedValues are properly serialized in index definition
            const indexes = collection.getIndexes();
            const vectorIndex = indexes.find(idx => idx.name === "vector_l2_stored");
            
            assertTrue(vectorIndex !== undefined, "Vector index should exist");
            assertTrue(vectorIndex.storedValues !== undefined, "storedValues should be defined");
            assertEqual(5, vectorIndex.storedValues.length, "Should have 5 stored values");
            
            // Verify the storedValues array contains the expected fields
            const expectedFields = ["name", "value", "category", "metadata", "description"];
            for (const field of expectedFields) {
                assertTrue(vectorIndex.storedValues.includes(field), 
                    `storedValues should include field: ${field}`);
            }
        },
    };
}


jsunity.run(VectorIndexCreateAndRemoveTestSuite);
jsunity.run(VectorIndexTestCreationWithVectors);
jsunity.run(VectorIndexStoredValuesTestSuite);

return jsunity.done();
