/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, arango, print, assertEqual */

'use strict';
const gm = require("@arangodb/general-graph");
const jsunity = require("jsunity");

let api = "/_api/schema";

function restSchemaHandlerTestSuite() {
    const vnCustomer = "customers";
    const vnProduct = "products";
    const enPurchased = "purchased";
    const gnPurchaseHistory = "purchaseHistory";
    let colCustomer = null;
    let colProduct = null;

    return  {
        setUpAll: function () {
            db._drop(vnCustomer);
            db._drop(vnProduct);
            db._drop(enPurchased);

            colCustomer = db._create(vnCustomer);
            colProduct = db._create(vnProduct);
            gm._create(gnPurchaseHistory, [gm._relation(enPurchased, vnCustomer, vnProduct)]);

            colCustomer.save({_key: "C1", name: "C1", age: 25, address: "Milano"});
            colCustomer.save({_key: "C2", name: "C2", age: "35", address: "San Francisco", isStudent: true});
            colCustomer.save({_key: "C3", name: "C3", age: 55, address: {city: "Berlin", country: "Germany"}});
            colCustomer.save({_key: "C4", name: "C4", age: "young", address: "Tokyo", isStudent: false});

            colProduct.save({_key: "P1", name: "P1", price: 499.99, used: false});
            colProduct.save({_key: "P2", name: "P2", price: "expensive"});
            colProduct.save({_key: "P3", name: "P3", price: 1499, version: "14.5", used: true});
            colProduct.save({_key: "P4", name: "P4", price: 2499, version: 1});

            let graph = gm._graph(gnPurchaseHistory);
            function makeEdge(from, to, date) {
                graph.purchased.save({_from: vnCustomer + "/" + from, _to: vnProduct + "/" + to, date: date});
            }
            makeEdge("C1", "P1", "5/25/2025");
            makeEdge("C2", "P2", "6/01/2025");
            makeEdge("C3", "P3", "1/25/2025");
            makeEdge("C4", "P4", "4/25/2025");
        },

        tearDownAll: function () {
            gm._drop(gnPurchaseHistory);
            db._drop(enPurchased);
            db._drop(vnCustomer);
            db._drop(vnProduct);
        },

        testSchemaEndpointWithNoParameters: function () {
            const doc = arango.GET_RAW(api);
            const body = doc.parsedBody;

            assertEqual(200, doc.code);
            assertTrue(Array.isArray(body.collections));
            assertTrue(Array.isArray(body.graphs));

            // Find collections by name
            const findCollection = (name) =>
                body.collections.find((col) => col.collectionName === name);

            const assertAttribute = (schema, attrName, expectedTypes, optional) => {
                const attr = schema.find((a) => a.attribute === attrName);
                assertTrue(attr, `Attribute '${attrName}' not found`);
                expectedTypes.forEach((type) => {
                    assertTrue(attr.types.includes(type), `'${attrName}' missing type '${type}'`);
                });
                assertEqual(optional, attr.optional, `'${attrName}' optional mismatch`);
            };

            /* Check collection products */
            const products = findCollection("products");
            assertEqual("document", products.collectionType);
            assertEqual(4, products.numOfDocuments);
            const pschema = products.schema;
            assertAttribute(pschema, "_id",    ["string"], false);
            assertAttribute(pschema, "_key",   ["string"], false);
            assertAttribute(pschema, "name",   ["string"], false);
            assertAttribute(pschema, "price",  ["number", "string"], false);
            assertAttribute(pschema, "used",   ["bool"], true);
            assertAttribute(pschema, "version",["string", "number"], true);

            const exampleProduct = products.examples[0];
            assertTrue(exampleProduct._id.startsWith("products/"));

            /* Check collection customers */
            const customers = findCollection("customers");
            assertEqual("document", customers.collectionType);
            assertEqual(4, customers.numOfDocuments);

            const cschema = customers.schema;
            assertAttribute(cschema, "_id",       ["string"], false);
            assertAttribute(cschema, "_key",      ["string"], false);
            assertAttribute(cschema, "address",   ["string", "object"], false);
            assertAttribute(cschema, "age",       ["string", "number"], false);
            assertAttribute(cschema, "isStudent", ["bool"], true);
            assertAttribute(cschema, "name",      ["string"], false);

            const exampleCustomer = customers.examples[0];
            assertTrue(exampleCustomer._id.startsWith("customers/"));

            /* Check collection products */
            const purchased = findCollection("purchased");
            assertEqual("edge", purchased.collectionType);
            assertEqual(4, purchased.numOfEdges);

            const eschema = purchased.schema;
            assertAttribute(eschema, "_from", ["string"], false);
            assertAttribute(eschema, "_to",   ["string"], false);
            assertAttribute(eschema, "_id",   ["string"], false);
            assertAttribute(eschema, "_key",  ["string"], false);
            assertAttribute(eschema, "date",  ["string"], false);

            const exampleEdge = purchased.examples[0];
            assertTrue(exampleEdge._id.startsWith("purchased/"));
            assertTrue(exampleEdge._from.startsWith("customers/"));
            assertTrue(exampleEdge._to.startsWith("products/"));

            /* Check graphs */
            const graph = body.graphs.find((g) => g.name === "purchaseHistory");
            assertEqual("purchaseHistory", graph.name);
            assertEqual(1, graph.relations.length);

            const rel = graph.relations[0];
            assertEqual("purchased", rel.collection);
            assertEqual("customers", rel.from[0]);
            assertEqual("products", rel.to[0]);
        },

        testSchemaEndpointWithNegativeSampleNum: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=-1");
            const body = doc.parsedBody;
            assertEqual(400, doc.code);
            assertTrue(body.error, "Expected error in response body");
        },

        testSchemaEndpointWithFloatingSampleNum: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=12.3");
            const body = doc.parsedBody;
            assertEqual(400, doc.code);
            assertTrue(body.error, "Expected error in response body");
        },

        testSchemaEndpointWithNonNumericSampleNum: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=abc");
            const body = doc.parsedBody;
            assertEqual(400, doc.code);
            assertTrue(body.error);
        },

        testSchemaEndpointWithEmptySampleNum: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=");
            const body = doc.parsedBody;
            assertEqual(200, doc.code);
        },

        testSchemaEndpointWithHugeSampleNum: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=999999999999999999999999999999");
            const body = doc.parsedBody;
            assertEqual(400, doc.code);
            assertTrue(body.error);
        },

        testSchemaEndpointWithNegativeExampleNum: function () {
            const doc = arango.GET_RAW(api + "?exampleNum=-1");
            const body = doc.parsedBody;
            assertEqual(400, doc.code);
            assertTrue(body.error, "Expected error in response body");
        },

        testSchemaEndpointWithFloatingExampleNum: function () {
            const doc = arango.GET_RAW(api + "?exampleNum=12.3");
            const body = doc.parsedBody;
            assertEqual(400, doc.code);
            assertTrue(body.error, "Expected error in response body");
        },

        testSchemaEndpointWithNonNumericExampleNum: function () {
            const doc = arango.GET_RAW(api + "?exampleNum=abc");
            const body = doc.parsedBody;
            assertEqual(400, doc.code);
            assertTrue(body.error);
        },

        testSchemaEndpointWithEmptyExampleNum: function () {
            const doc = arango.GET_RAW(api + "?exampleNum=");
            const body = doc.parsedBody;
            assertEqual(200, doc.code);
        },

        testSchemaEndpointWithHugeExampleNum: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=999999999999999999999999999999");
            const body = doc.parsedBody;
            assertEqual(400, doc.code);
            assertTrue(body.error);
        },

        testSchemaEndpointWithExampleNumIsLargerThanSampleNum: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=10&exampleNum=20");
            const body = doc.parsedBody;
            assertEqual(400, doc.code);
            assertTrue(body.error);
        },

        testSchemaEndpointWithValidExampleNum: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=10&exampleNum=3");
            const body = doc.parsedBody;
            assertEqual(200, doc.code);
            const products = body.collections.find(c => c.collectionName === "products");
            assertTrue(products, "products collection not found");
            assertEqual(3, products.examples.length, "Expected 3 product examples");
        },

        testSchemaEndpointWithSampleNumOnly: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=1");
            const body = doc.parsedBody;

            assertEqual(200, doc.code);
            assertTrue(Array.isArray(body.collections), "collections must be an array");

            body.collections.forEach((collection) => {
                assertTrue(Array.isArray(collection.schema), `schema must be array in ${collection.collectionName}`);

                collection.schema.forEach((attribute) => {
                    assertEqual(false, attribute.optional,
                        `Attribute '${attribute.attribute}' in '${collection.collectionName}' was marked optional`);
                });
            });
        },

        testSchemaEndpointWithNotExistingCollection: function () {
            const doc = arango.GET_RAW(api + "/collection/fake");
            assertEqual(404, doc.code);
        }
    };
}

jsunity.run(restSchemaHandlerTestSuite);
return jsunity.done();