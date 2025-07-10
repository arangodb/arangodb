/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, arango, assertEqual, print, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2025-2025 ArangoDB GmbH, Cologne, Germany
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
// / @author Koichi Nakata
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const gm = require("@arangodb/general-graph");
const jsunity = require("jsunity");

let api = "/_api/schema";
let paths = ["", "/collection/products", "/graph/purchaseHistory", "/view/descView"];
let pathsExceptCol = ["", "/graph/purchaseHistory", "/view/descView"];

function restSchemaHandlerTestSuite() {
    const COLLECTION_CUSTOMERS = 'customers';
    const COLLECTION_PRODUCTS = 'products';
    const COLLECTION_SPECIAL_PRODUCTS = 'specialProducts';
    const COLLECTION_COMPANIES = 'companies';
    const COLLECTION_MATERIALS = "materials";
    const COLLECTION_SPECIAL_MATERIALS = "specialMaterials";
    const COLLECTION_LOGS = "logs";
    const COLLECTION_SPECIAL_LOGS = "specialLogs";

    const EDGE_PURCHASED = 'purchased';
    const EDGE_MANUFACTURED = 'manufactured';
    const EDGE_CONTAINS = 'contains';

    const GRAPH_PURCHASE_HISTORY = 'purchaseHistory';
    const GRAPH_MANUFACTURE = 'manufacture';

    const VIEW_PRODUCT_DESCRIPTION = 'productDescView';
    const VIEW_DESCRIPTION = 'descView';

    let customersCollection;
    let productsCollection;
    let companiesCollection;

    let purchaseHistoryGraph;
    let manufactureGraph;

    let productDescriptionArangoSearch;
    let descriptionArangoSearch;

    let prodIndex;
    let cusIndex;

    function tearDown() {
        try { productDescriptionArangoSearch.drop(); } catch (e) {}
        try { descriptionArangoSearch.drop(); } catch (e) {}

        try { gm._drop(GRAPH_PURCHASE_HISTORY); } catch (e) {}
        try { gm._drop(GRAPH_MANUFACTURE); } catch (e) {}

        db._drop(EDGE_PURCHASED);
        db._drop(EDGE_MANUFACTURED);
        db._drop(EDGE_CONTAINS);
        db._drop(COLLECTION_CUSTOMERS);
        db._drop(COLLECTION_PRODUCTS);
        db._drop(COLLECTION_SPECIAL_PRODUCTS);
        db._drop(COLLECTION_COMPANIES);
        db._drop(COLLECTION_MATERIALS);
        db._drop(COLLECTION_SPECIAL_MATERIALS);
        db._drop(COLLECTION_LOGS);
        db._drop(COLLECTION_SPECIAL_LOGS);
    }

    return {
        setUpAll: function () {
            tearDown();

            customersCollection = db._create(COLLECTION_CUSTOMERS);
            productsCollection = db._create(COLLECTION_PRODUCTS);
            companiesCollection = db._create(COLLECTION_COMPANIES);

            gm._create(
                GRAPH_PURCHASE_HISTORY,
                [ gm._relation(EDGE_PURCHASED, COLLECTION_CUSTOMERS, COLLECTION_PRODUCTS) ],
                [ COLLECTION_LOGS ] // Orphan collection
            );
            gm._create(
                GRAPH_MANUFACTURE, [
                    gm._relation(EDGE_MANUFACTURED, COLLECTION_COMPANIES, COLLECTION_PRODUCTS),
                    gm._relation(EDGE_CONTAINS, [
                        COLLECTION_PRODUCTS, COLLECTION_SPECIAL_PRODUCTS
                        ], [
                        COLLECTION_MATERIALS, COLLECTION_SPECIAL_MATERIALS
                    ])
                ],
                [ COLLECTION_LOGS, COLLECTION_SPECIAL_LOGS ] // Orphan collections
            );


            customersCollection.save({
                _key: 'C1', name: 'C1', age: 25, address: 'Milano',
                comment: 'Is a teacher at a University in San Francisco and teaches finance'
            });
            customersCollection.save({
                _key: 'C2', name: 'C2', age: '35', address: 'San Francisco', isStudent: true,
                comment: 'Has been working in financial industry for 5 years'
            });
            customersCollection.save({
                _key: 'C3', name: 'C3', age: 55,
                address: { city: 'Berlin', country: 'Germany'},
                comment: 'Is a student at a University in San Francisco, major is financial'
            });
            customersCollection.save({
                _key: 'C4', name: 'C4', age: 'young', address: 'Tokyo', isStudent: false,
                comment: 'Used to work in financial industry but now working as a teacher at school'
            });

            productsCollection.save({ _key: 'P1', name: 'P1', price: 499.99, used: false, description: 'Made in Germany and new' });
            productsCollection.save({ _key: 'P2', name: 'P2', price: 'expensive', description: 'Made in USA and new' });
            productsCollection.save({ _key: 'P3', name: 'P3', price: 1499, version: '14.5', used: true, description: 'Made in Italy and used' });
            productsCollection.save({ _key: 'P4', name: 'P4', price: 2499, version: 1, description: 'Made in Japan and used' });

            companiesCollection.save({ _key: 'E1', name: 'E1', established: 1990, isPublic: true });
            companiesCollection.save({ _key: 'E2', name: 'E2', established: '5/2025', isPublic: false });
            companiesCollection.save({ _key: 'E3', name: 'E3', established: 1990 });

            purchaseHistoryGraph = gm._graph(GRAPH_PURCHASE_HISTORY);
            manufactureGraph = gm._graph(GRAPH_MANUFACTURE);

            productDescriptionArangoSearch = db._createView(
                VIEW_PRODUCT_DESCRIPTION,
                'arangosearch',
                { links: { products: {
                    includeAllFields: true, analyzers: ['identity'],
                            fields: { description: { analyzers: ['text_en'] } }
                } } }
            );
            descriptionArangoSearch = db._createView(
                VIEW_DESCRIPTION,
                'arangosearch',
                { links: {
                        products: { fields: { description: { analyzers: ['text_en'] } } },
                        customers: { fields: { comment: { analyzers: ['text_en'] } } }
                    }
                }
            );

            function createPurchaseEdge(customerKey, productKey, purchaseDate) {
                purchaseHistoryGraph[EDGE_PURCHASED].save({
                    _from: `${COLLECTION_CUSTOMERS}/${customerKey}`,
                    _to:   `${COLLECTION_PRODUCTS}/${productKey}`,
                    date: purchaseDate
                });
            }

            function createManufactureEdge(companyKey, productKey, amount) {
                manufactureGraph[EDGE_MANUFACTURED].save({
                    _from: `${COLLECTION_COMPANIES}/${companyKey}`,
                    _to:   `${COLLECTION_PRODUCTS}/${productKey}`,
                    amount: amount
                });
            }

            createPurchaseEdge('C1', 'P1', '5/25/2025');
            createPurchaseEdge('C2', 'P2', '6/01/2025');
            createPurchaseEdge('C3', 'P3', '1/25/2025');
            createPurchaseEdge('C4', 'P4', '4/25/2025');

            createManufactureEdge('E1', 'P1', 1000);
            createManufactureEdge('E2', 'P2', 10);

            prodIndex = productsCollection.ensureIndex({ name: "proInd", type: "persistent", fields: ["price"]});
            cusIndex = customersCollection.ensureIndex({ name: "cusInd", type: "geo", fields: ["address"]});
        },

        tearDownAll: tearDown,

        test_GetSchemaWithoutParameters_ShouldReturn200AndAllSchemas: function () {
            const doc = arango.GET_RAW(api);
            const body = doc.parsedBody;

            assertEqual(200, doc.code, 'Expected HTTP 200 for schema endpoint without query parameters');
            assertTrue(Array.isArray(body.collections), 'collections should be an array');
            assertEqual(11, body.collections.length, 'collections should contain 11 objects');
            assertTrue(Array.isArray(body.graphs), 'graphs should be an array');
            assertTrue(Array.isArray(body.views), 'views should be an array');

            const findCollection = name => body.collections.find(col => col.collectionName === name);

            const assertAttribute = (schema, attrName, expectedTypes, optional) => {
                const attr = schema.find(a => a.attribute === attrName);
                assertTrue(attr, `Attribute '${attrName}' not found in schema`);
                expectedTypes.forEach(type => {
                    assertTrue(
                        attr.types.includes(type),
                        `Attribute '${attrName}' missing expected type '${type}'`
                    );
                });
                assertEqual(
                    optional,
                    attr.optional,
                    `Attribute '${attrName}' optional flag expected ${optional}`
                );
            };

            const products = findCollection('products');
            assertTrue(products, 'products collection should exist');
            assertEqual('document', products.collectionType, 'products should be a document collection');
            assertEqual(4, products.numOfDocuments, 'products should contain 4 documents');
            assertTrue(Array.isArray(products.indexes), 'indexes should be an array');
            assertTrue(Array.isArray(products.indexes[0].fields), 'fields should be an array');
            assertEqual('price', products.indexes[0].fields[0], 'fields should be price');
            assertEqual('false', products.indexes[0].sparse, 'sparse should be false');
            assertEqual('persistent', products.indexes[0].type, 'type should be persistent');
            assertEqual('false', products.indexes[0].unique, 'unique should be false');
            assertAttribute(products.schema, '_id', ['string'], false);
            assertAttribute(products.schema, '_key', ['string'], false);
            assertAttribute(products.schema, 'name', ['string'], false);
            assertAttribute(products.schema, 'price', ['number', 'string'], false);
            assertAttribute(products.schema, 'used', ['bool'], true);
            assertAttribute(products.schema, 'version', ['string', 'number'], true);
            assertTrue(
                products.examples[0]._id.startsWith('products/'),
                'Example product _id should start with "products/"'
            );

            const companies = findCollection('companies');
            assertTrue(companies, 'companies collection should exist');
            assertEqual('document', companies.collectionType, 'companies should be a document collection');
            assertEqual(3, companies.numOfDocuments, 'companies should contain 3 documents');
            assertAttribute(companies.schema, '_id', ['string'], false);
            assertAttribute(companies.schema, '_key', ['string'], false);
            assertAttribute(companies.schema, 'name', ['string'], false);
            assertAttribute(companies.schema, 'established', ['number', 'string'], false);
            assertAttribute(companies.schema, 'isPublic', ['bool'], true);
            assertTrue(
                companies.examples[0]._id.startsWith('companies/'),
                'Example company _id should start with "companies/"'
            );

            const customers = findCollection('customers');
            assertTrue(customers, 'customers collection should exist');
            assertEqual('document', customers.collectionType, 'customers should be a document collection');
            assertEqual(4, customers.numOfDocuments, 'customers should contain 4 documents');
            assertTrue(Array.isArray(customers.indexes), 'indexes should be an array');
            assertTrue(Array.isArray(customers.indexes[0].fields), 'fields should be an array');
            assertEqual('address', customers.indexes[0].fields[0], 'fields should be address');
            assertEqual('geo', customers.indexes[0].type, 'type should be geo');
            assertEqual('true', customers.indexes[0].sparse, 'sparse should be true');
            assertEqual('false', customers.indexes[0].unique, 'unique should be false');
            assertAttribute(customers.schema, '_id', ['string'], false);
            assertAttribute(customers.schema, '_key', ['string'], false);
            assertAttribute(customers.schema, 'address', ['string', 'object'], false);
            assertAttribute(customers.schema, 'age', ['string', 'number'], false);
            assertAttribute(customers.schema, 'isStudent', ['bool'], true);
            assertAttribute(customers.schema, 'name', ['string'], false);
            assertTrue(
                customers.examples[0]._id.startsWith('customers/'),
                'Example customer _id should start with "customers/"'
            );

            const purchased = findCollection('purchased');
            assertTrue(purchased, 'purchased edge collection should exist');
            assertEqual('edge', purchased.collectionType, 'purchased should be an edge collection');
            assertEqual(4, purchased.numOfEdges, 'purchased should contain 4 edges');
            assertAttribute(purchased.schema, '_from', ['string'], false);
            assertAttribute(purchased.schema, '_to', ['string'], false);
            assertAttribute(purchased.schema, '_id', ['string'], false);
            assertAttribute(purchased.schema, '_key', ['string'], false);
            assertAttribute(purchased.schema, 'date', ['string'], false);
            assertTrue(
                purchased.examples[0]._id.startsWith('purchased/'),
                'Example purchased _id should start with "purchased/"'
            );
            assertTrue(
                purchased.examples[0]._from.startsWith('customers/'),
                'Purchased edge _from should start with "customers/"'
            );
            assertTrue(
                purchased.examples[0]._to.startsWith('products/'),
                'Purchased edge _to should start with "products/"'
            );

            const manufactured = findCollection('manufactured');
            assertTrue(manufactured, 'manufactured edge collection should exist');
            assertEqual('edge', manufactured.collectionType, 'manufactured should be an edge collection');
            assertEqual(2, manufactured.numOfEdges, 'manufactured should contain 2 edges');
            assertAttribute(manufactured.schema, '_from', ['string'], false);
            assertAttribute(manufactured.schema, '_to', ['string'], false);
            assertAttribute(manufactured.schema, '_id', ['string'], false);
            assertAttribute(manufactured.schema, '_key', ['string'], false);
            assertAttribute(manufactured.schema, 'amount', ['number'], false);
            assertTrue(
                manufactured.examples[0]._id.startsWith('manufactured/'),
                'Example manufactured _id should start with "manufactured/"'
            );
            assertTrue(
                manufactured.examples[0]._from.startsWith('companies/'),
                'Manufactured edge _from should start with "companies/"'
            );
            assertTrue(
                manufactured.examples[0]._to.startsWith('products/'),
                'Manufactured edge _to should start with "products/"'
            );

            assertEqual(2, body.graphs.length, 'There should be exactly 2 graphs');
            const pGraph = body.graphs.find(g => g.name === 'purchaseHistory');
            assertTrue(pGraph, 'purchaseHistory graph should exist');
            assertEqual(1, pGraph.relations.length, 'purchaseHistory should have 1 relation');
            assertEqual(pGraph.relations[0].collection, 'purchased', 'Relation collection name mismatch');
            assertEqual(pGraph.relations[0].from[0], 'customers', 'Graph from vertex mismatch');
            assertEqual(pGraph.relations[0].to[0], 'products', 'Graph to vertex mismatch');
            assertEqual(1, pGraph.orphans.length, 'orphan should be just 1');
            assertEqual(pGraph.orphans[0], 'logs', 'orphan should be logs');

            const mGraph = body.graphs.find(g => g.name === 'manufacture');
            assertTrue(mGraph, 'manufacture graph should exist');
            assertEqual(2, mGraph.relations.length, 'manufacture should have 2 relation');
            const manufacturedRel = mGraph.relations.find(v => v.collection === 'manufactured');
            assertEqual(manufacturedRel.from[0], 'companies', 'Graph from vertex mismatch');
            assertEqual(manufacturedRel.to[0], 'products', 'Graph to vertex mismatch');
            const containsRel = mGraph.relations.find(v => v.collection === 'contains');
            assertEqual(2, containsRel.from.length, 'from vertex of contains should include 2 objects');
            assertTrue(containsRel.from.includes('products'), 'products must be in from vertex');
            assertTrue(containsRel.from.includes('specialProducts'), 'specialProducts must be in to vertex');
            assertEqual(2, containsRel.to.length, 'to vertex of contains should include 2 objects');
            assertTrue(containsRel.to.includes('materials'), 'materials must be in to vertex');
            assertTrue(containsRel.to.includes('specialMaterials'), 'specialMaterials must be in to vertex');
            assertEqual(2, mGraph.orphans.length, 'orphan should be 2');
            assertTrue(mGraph.orphans.includes('logs'), 'orphans should contains logs');
            assertTrue(mGraph.orphans.includes('specialLogs'), 'orphans should contains specialLogs');


            const prodView = body.views.find(v => v.viewName === 'productDescView');
            assertTrue(prodView, 'productDescView should exist');
            assertTrue(Array.isArray(prodView.links), 'Links for productDescView should be an array');
            assertEqual(prodView.links[0].collectionName, 'products', 'View link collection name mismatch');
            assertTrue(Array.isArray(prodView.links[0].fields), 'View link fields should be an array');
            assertEqual(prodView.links[0].fields[0].attribute, 'description', 'View field attribute mismatch');
            assertEqual(prodView.links[0].fields[0].analyzers[0], 'text_en', 'View field analyzer mismatch');
            assertEqual(prodView.links[0].allAttributeAnalyzers[0], 'identity', 'View allAttributeAnalyzers mismatch');

            const descView = body.views.find(v => v.viewName === 'descView');
            assertTrue(descView, 'descView should exist');
            assertEqual(descView.links.length, 2, 'descView should have 2 links');
        },

        test_TooManySuffixes_ShouldReturn404: function() {
            let tooManySuffixes = ['/collection/products/fake', '/graph/purchaseHistory/fake', '/view/descView/fake'];
            tooManySuffixes.forEach(path => {
                const doc = arango.GET_RAW(api + path);
                assertEqual(404, doc.code, `Expected HTTP 404 for too many suffixes`);
            });
        },

        test_InvalidSampleNumValues_ShouldReturn400: function () {
            let invalidParams = [
                '?sampleNum=-1',
                '?sampleNum=12.3',
                '?sampleNum=abc',
                '?sampleNum=999999999999999999999999999999'
            ];
            paths.forEach(path => {
                invalidParams.forEach(param => {
                    const doc = arango.GET_RAW(api + path + param);
                    assertEqual(400, doc.code, `Expected HTTP 400 for invalid sampleNum '${param}' for path '${path}'`);
                    assertTrue(doc.parsedBody.error, 'Error flag should be true for invalid sampleNum');
                });
            });
        },

        test_InvalidExampleNumValues_ShouldReturn400: function () {
            let invalidParams = [
                '?exampleNum=-1',
                '?exampleNum=12.3',
                '?exampleNum=abc',
                '?exampleNum=999999999999999999999999999999'
            ];
            paths.forEach(path => {
                invalidParams.forEach(param => {
                    const doc = arango.GET_RAW(api + path + param);
                    assertEqual(400, doc.code, `Expected HTTP 400 for invalid exampleNum '${param}' for path '${path}'`);
                    assertTrue(doc.parsedBody.error, 'Error flag should be true for invalid exampleNum');
                });
            });
        },

        test_EmptySampleNum_ShouldReturn200: function () {
            paths.forEach(path => {
                const doc = arango.GET_RAW(api + path+ '?sampleNum=');
                assertEqual(200, doc.code, 'Empty sampleNum should default to valid response');
            });
        },

        test_EmptyExampleNum_ShouldReturn200: function () {
            paths.forEach(path => {
                const doc = arango.GET_RAW(api + path + '?exmapleNum=');
                assertEqual(200, doc.code, 'Empty exampleNum should default to valid response');
            });
        },

        test_ExampleNumGreaterThanSampleNum_ShouldReturn400: function () {
            paths.forEach(path => {
                const doc = arango.GET_RAW(api + path + '?sampleNum=10&exampleNum=20');
                assertEqual(400, doc.code, 'exampleNum greater than sampleNum should return 400');
                assertTrue(doc.parsedBody.error, 'Error flag should be true when exampleNum > sampleNum');
            });
        },

        test_ValidExampleNum_ShouldReturnCorrectExampleCount: function () {
            const doc = arango.GET_RAW(api + '?sampleNum=10&exampleNum=3');
            const products = doc.parsedBody.collections.find(c => c.collectionName === 'products');
            assertEqual(200, doc.code, 'Valid exampleNum should return HTTP 200');
            assertTrue(products, 'products collection should be present');
            assertEqual(
                products.examples.length,
                3,
                'Expected exactly 3 examples for products when exampleNum=3'
            );
        },

        test_GetSchemaWithSampleNumOne_ShouldReturnSchemasWithNoOptionalAttributes: function () {
            // This test wants to verify that "optional" field logic is correct;
            // if sampleNum = 1, all "optional" should be false.
            pathsExceptCol.forEach(path => {
                const doc = arango.GET_RAW(api + path + '?sampleNum=1');
                const body = doc.parsedBody;
                assertEqual(200, doc.code, 'Expected HTTP 200 for sampleNum=1');
                assertTrue(Array.isArray(body.collections), 'collections must be an array');

                body.collections.forEach(collection => {
                    assertTrue(
                        Array.isArray(collection.schema),
                        `schema must be an array in '${collection.collectionName}'`
                    );
                    collection.schema.forEach(attribute => {
                        assertEqual(
                            false,
                            attribute.optional,
                            `Attribute '${attribute.attribute}' in '${collection.collectionName}' should not be optional`
                        );
                    });
                });

            });
        },

        test_ExampleNumZero_ShouldReturnNoExamples: function () {
            pathsExceptCol.forEach(path => {
                const doc = arango.GET_RAW(api + path + '?exampleNum=0');
                assertEqual(200, doc.code, 'Expected HTTP 200 for exampleNum=0');
                const body = doc.parsedBody;
                assertTrue(Array.isArray(body.collections),'collections should be an array');

                body.collections.forEach(coll => {
                    assertTrue(Array.isArray(coll.examples),
                        `examples for '${coll.collectionName}' should be an array`);
                    assertEqual(0, coll.examples.length, `Expected 0 examples for '${coll.collectionName}'`);
                });
            });
        },

        test_SampleNumZero_ShouldReturn400: function () {
            paths.forEach(path => {
                const doc = arango.GET_RAW(api + path + '?sampleNum=0');
                assertEqual(400, doc.code, 'Expected HTTP 400 for sampleNum=0');
            });
        },

        test_GetProductsDocument_ShouldReturnCollectionSchema: function () {
            const doc = arango.GET_RAW(api + '/collection/products');
            assertEqual(200, doc.code, 'Expected HTTP 200 for GET /collection/products');

            const body = doc.parsedBody;

            assertEqual("products", body.collectionName, 'collectionName should be "products"');
            assertEqual("document", body.collectionType, 'collectionType should be "document"');
            assertEqual(4, body.numOfDocuments, 'numOfDocuments should be 4');

            assertTrue(Array.isArray(body.indexes), 'indexes should be an array');
            const index = body.indexes.find(idx => idx.name === "proInd");
            assertEqual(["price"], index.fields, "fields for index 'proInd' should be price");
            assertEqual("persistent", index.type, "type for index 'proInd' should be persistent");
            assertEqual(false, index.unique, "unique for index 'proInd' should be false");
            assertEqual(false, index.sparse, "sparse for index 'proInd' should be false");

            assertTrue(Array.isArray(body.schema), 'schema should be an array');
            const attrMap = {};
            body.schema.forEach(attr => { attrMap[attr.attribute] = attr; });
            const expectedSchema = {
                "_id": { types: ["string"], optional: false },
                "_key": { types: ["string"], optional: false },
                "description": { types: ["string"], optional: false },
                "name": { types: ["string"], optional: false },
                "price": { types: ["string","number"], optional: false },
                "used": { types: ["bool"], optional: true },
                "version":{ types: ["string","number"], optional: true }
            };
            Object.entries(expectedSchema).forEach(([attrName, expected]) => {
                const actual = attrMap[attrName];
                assertEqual(expected.types.sort(), actual.types.sort(),
                    `Types for ${attrName} should be ${expected.types}`);
                assertEqual(expected.optional, actual.optional,
                    `Optional for ${attrName} should be ${expected.optional}`);
            });

            assertTrue(Array.isArray(body.examples), 'examples should be an array');
            assertTrue(body.examples.length > 0, 'examples array should have at least one element');
            body.examples.forEach(e => {
                assertTrue(e._id.startsWith('products/'),
                    'Example product _id malformed in graph response');
            });
        },

        test_GetCollectionPurchased_ShouldReturnEdgeCollectionSchema: function () {
            const doc = arango.GET_RAW(api + '/collection/purchased');
            assertEqual(200, doc.code, 'Expected HTTP 200 for GET /collection/purchased');

            const body = doc.parsedBody;

            assertEqual("purchased", body.collectionName, 'collectionName should be "purchased"');
            assertEqual("edge", body.collectionType, 'collectionType should be "edge"');
            assertEqual(4, body.numOfEdges, 'numOfEdges should be 4');

            assertTrue(Array.isArray(body.indexes) && body.indexes.length === 0,
                'indexes should be an empty array');
            assertTrue(Array.isArray(body.schema), 'schema should be an array');

            const attrMap = {};
            body.schema.forEach(attr => { attrMap[attr.attribute] = attr; });
            const expectedSchema = {
                "_from": { types: ["string"], optional: false },
                "_id": { types: ["string"], optional: false },
                "_key": { types: ["string"], optional: false },
                "_to": { types: ["string"], optional: false },
                "date": { types: ["string"], optional: false}
            };
            Object.entries(expectedSchema).forEach(([attrName, expected]) => {
                const actual = attrMap[attrName];
                assertEqual(expected.types.sort(), actual.types.sort(),
                    `Types for attribute ${attrName} should be ${expected.types}`);
                assertEqual(expected.optional, actual.optional,
                    `Optional flag for attribute ${attrName} should be ${expected.optional}`);
            });

            assertTrue(Array.isArray(body.examples), 'examples should be an array');
            assertTrue(body.examples.length > 0, 'examples array should have at least one element');
            body.examples.forEach(e => {
                assertTrue(e._from.startsWith('customers/'));
                assertTrue(e._to.startsWith('products/'));
                assertTrue(e._id.startsWith('purchased/'));
            });
        },

        test_GetSchemaNonExistingCollection_ShouldReturn404: function () {
            const doc = arango.GET_RAW(api + '/collection/fake');
            assertEqual(404, doc.code, 'Expected HTTP 404 for non-existing collection');
        },

        test_GetPurchaseHistoryGraph_ShouldReturnGraphSchema: function () {
            const doc = arango.GET_RAW(api + '/graph/purchaseHistory');
            assertEqual(200, doc.code, 'Expected HTTP 200 for graph/purchaseHistory');
            const body = doc.parsedBody;
            const findCollection = name =>
                body.collections.find(col => col.collectionName === name);
            const assertAttribute = (schema, attrName, expectedTypes, optional) => {
                const attr = schema.find(a => a.attribute === attrName);
                assertTrue(!!attr, `Attribute '${attrName}' not found in schema for graph endpoint`);
                expectedTypes.forEach(type => {
                    assertTrue(attr.types.includes(type),
                        `Attribute '${attrName}' missing type '${type}' in graph schema`);
                });
                assertEqual(optional, attr.optional,
                    `Attribute '${attrName}' optional flag expected ${optional} in graph schema`
                );
            };

            // Validate products in graph
            const products = findCollection('products');
            assertTrue(products, 'products collection missing in graph response');
            assertEqual('document', products.collectionType);
            assertEqual(4, products.numOfDocuments);
            assertAttribute(products.schema, '_id', ['string'], false);
            assertAttribute(products.schema, '_key', ['string'], false);
            assertAttribute(products.schema, 'name', ['string'], false);
            assertAttribute(products.schema, 'price', ['number','string'], false);
            assertAttribute(products.schema, 'used', ['bool'], true);
            assertAttribute(products.schema, 'version', ['number','string'], true);
            assertTrue(
                products.examples[0]._id.startsWith('products/'),
                'Example product _id malformed in graph response'
            );

            // Validate customers in graph
            const customers = findCollection('customers');
            assertTrue(customers, 'customers collection missing in graph response');
            assertEqual('document', customers.collectionType);
            assertEqual(4, customers.numOfDocuments);
            assertAttribute(customers.schema, '_id', ['string'], false);
            assertAttribute(customers.schema, '_key', ['string'], false);
            assertAttribute(customers.schema, 'address', ['string','object'], false);
            assertAttribute(customers.schema, 'age', ['string','number'], false);
            assertAttribute(customers.schema, 'isStudent', ['bool'], true);
            assertAttribute(customers.schema, 'name', ['string'], false);

            // Validate purchased edges in graph
            const purchased = findCollection('purchased');
            assertTrue(purchased, 'purchased edge collection missing in graph response');
            assertEqual('edge', purchased.collectionType);
            assertEqual(4, purchased.numOfEdges);
            assertAttribute(purchased.schema, '_from', ['string'], false);
            assertAttribute(purchased.schema, '_to', ['string'], false);
            assertAttribute(purchased.schema, '_id', ['string'], false);
            assertAttribute(purchased.schema, '_key', ['string'], false);
            assertAttribute(purchased.schema, 'date', ['string'], false);

            const graph = body.graphs.find(g => g.name === 'purchaseHistory');
            assertTrue(graph, 'purchaseHistory graph missing');
            assertEqual(1, graph.relations.length);
            assertEqual(graph.relations[0].collection, 'purchased');
            assertEqual(graph.relations[0].from[0], 'customers');
            assertEqual(graph.relations[0].to[0], 'products');
            assertTrue(Array.isArray(graph.orphans), 'orphans should be an array');
            assertEqual('logs', graph.orphans[0], 'orphans should be logs');
        },

        test_ManufactureGraph_ShouldReturnGraphSchema: function () {
            const doc = arango.GET_RAW(api + '/graph/manufacture');
            assertEqual(200, doc.code, 'Expected HTTP 200 for graph/manufacture');
            const body = doc.parsedBody;

            const graph = body.graphs.find(g => g.name === 'manufacture');
            assertTrue(graph, 'manufacture graph missing');
            assertEqual(2, graph.relations.length);

            const manufacturedRel = graph.relations.find(r => r.collection === 'manufactured');
            assertEqual(manufacturedRel.from[0], 'companies', 'Graph from vertex mismatch');
            assertEqual(manufacturedRel.to[0], 'products', 'Graph to vertex mismatch');

            const containsRel = graph.relations.find(v => v.collection === 'contains');
            assertEqual(2, containsRel.from.length, 'from vertex of contains should include 2 objects');
            assertTrue(containsRel.from.includes('products'), 'products must be in from vertex');
            assertTrue(containsRel.from.includes('specialProducts'), 'specialProducts must be in to vertex');
            assertEqual(2, containsRel.to.length, 'to vertex of contains should include 2 objects');
            assertTrue(containsRel.to.includes('materials'), 'materials must be in to vertex');
            assertTrue(containsRel.to.includes('specialMaterials'), 'specialMaterials must be in to vertex');

            assertEqual(2, graph.orphans.length, 'orphan should be 2');
            assertTrue(graph.orphans.includes('logs'), 'orphans should contains logs');
            assertTrue(graph.orphans.includes('specialLogs'), 'orphans should contains specialLogs');
        },

        test_GetGraphNonExisting_ShouldReturn404: function () {
            const doc = arango.GET_RAW(api + '/graph/fake');
            assertEqual(404, doc.code, 'Expected HTTP 404 for non-existing graph');
        },

        test_CollectionExamplesClamp_ShouldClampExamplesToCollectionSize: function () {
            const doc = arango.GET_RAW(api + '/collection/customers?exampleNum=100');
            assertEqual(200, doc.code, 'Expected HTTP 200 when exampleNum exceeds document count');
            const body = doc.parsedBody;
            assertTrue(
                Array.isArray(body.examples),
                'examples should be an array for single-collection endpoint'
            );
            assertEqual(
                4,
                body.examples.length,
                'Expected examples length clamped to 4 when exampleNum > actual documents'
            );
        },

        test_GetViewByName_ShouldReturnViewAndCollectionSchemas: function () {
            const doc = arango.GET_RAW(api + '/view/descView');
            assertEqual(200, doc.code, 'Expected HTTP 200 for view/descView');
            const body = doc.parsedBody;
            const vDesc = body.views[0];
            assertTrue(
                Array.isArray(vDesc.links),
                'View links should be an array for descView'
            );
            const vCustomers = vDesc.links.find(v => v.collectionName === 'customers');
            assertEqual(
                'comment',
                vCustomers.fields[0].attribute,
                'View field attribute mismatch for customers in descView'
            );
            assertEqual(
                'text_en',
                vCustomers.fields[0].analyzers[0],
                'View field analyzer mismatch for customers in descView'
            );

            const vProducts = vDesc.links.find(v => v.collectionName === 'products');
            assertEqual(
                'description',
                vProducts.fields[0].attribute,
                'View field attribute mismatch for products in descView'
            );
            assertEqual(
                'text_en',
                vProducts.fields[0].analyzers[0],
                'View field analyzer mismatch for products in descView'
            );

            const colls = body.collections;
            assertEqual(2, colls.length, 'Expected 2 collections in view response');
            const cCustomers = colls.find(c => c.collectionName === 'customers');
            assertEqual(
                4,
                cCustomers.numOfDocuments,
                'Expected 4 documents for customers in view collections'
            );
            assertEqual(
                7,
                cCustomers.schema.length,
                'Expected schema length of 7 for customers in view collections'
            );
            const cProducts = colls.find(c => c.collectionName === 'products');
            assertEqual(
                4,
                cProducts.numOfDocuments,
                'Expected 4 documents for products in view collections'
            );
            assertEqual(
                7,
                cProducts.schema.length,
                'Expected schema length of 7 for products in view collections'
            );
        },

        test_GetViewNonExisting_ShouldReturn404: function () {
            const doc = arango.GET_RAW(api + '/view/fake');
            assertEqual(404, doc.code, 'Expected HTTP 404 for non-existing view');
        },

        test_GetViewEmptyName_ShouldReturn404: function () {
            const doc = arango.GET_RAW(api + '/view');
            assertEqual(404, doc.code, 'Expected HTTP 404 for empty view name');
        },

        test_GetViewIncludeAllFieldsTrue_ShouldReturnAnalyzersForAllAttributes: function () {
            const doc = arango.GET_RAW(api + '/view/productDescView');
            assertEqual(200, doc.code, 'Expected HTTP 200 for view/productDescView');
            const body = doc.parsedBody;
            const vProdView = body.views[0];
            assertTrue(
                Array.isArray(vProdView.links),
                'View links should be an array for productDescView'
            );
            assertEqual(
                'description',
                vProdView.links[0].fields[0].attribute,
                'View field attribute mismatch for productDescView'
            );
            assertEqual(
                'text_en',
                vProdView.links[0].fields[0].analyzers[0],
                'View field analyzer mismatch for productDescView'
            );
            assertEqual(
                'identity',
                vProdView.links[0].allAttributeAnalyzers[0],
                'View allAttributeAnalyzers mismatch for productDescView'
            );
        }
    };
}

jsunity.run(restSchemaHandlerTestSuite);
return jsunity.done();