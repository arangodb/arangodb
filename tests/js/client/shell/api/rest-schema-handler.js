/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, arango, assertEqual, assertTrue */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
// / @author Koichi Nakata
// //////////////////////////////////////////////////////////////////////////////

'use strict';
const gm = require("@arangodb/general-graph");
const jsunity = require("jsunity");

let api = "/_api/schema";

function restSchemaHandlerTestSuite() {
    const vnCustomer = "customers";
    const vnProduct = "products";
    const vnCompany = "companies";
    const enPurchased = "purchased";
    const enManufactured = "manufactured";
    const gnPurchaseHistory = "purchaseHistory";
    const gnManufacture = "manufacture";
    const vnProductDescView = "productDescView";
    const vnDescView = "descView";
    let colCustomer = null;
    let colProduct = null;
    let colCompany = null;
    let pGraph = null;
    let mGraph = null;
    let productDescViewSearch = null;
    let descViewSearch = null;

    return  {
        setUpAll: function () {
            db._drop(vnCustomer);
            db._drop(vnProduct);
            db._drop(vnCompany);
            db._drop(enPurchased);
            db._drop(enManufactured);

            colCustomer = db._create(vnCustomer);
            colProduct = db._create(vnProduct);
            colCompany = db._create(vnCompany);
            gm._create(gnPurchaseHistory, [gm._relation(enPurchased, vnCustomer, vnProduct)]);
            gm._create(gnManufacture, [gm._relation(enManufactured, vnCompany, vnProduct)]);

            colCustomer.save({_key: "C1", name: "C1", age: 25, address: "Milano", comment: "Is a teacher at a University in San Francisco and teaches finance"});
            colCustomer.save({_key: "C2", name: "C2", age: "35", address: "San Francisco", isStudent: true, comment: "Has been working in financial industry for 5 years"});
            colCustomer.save({_key: "C3", name: "C3", age: 55, address: {city: "Berlin", country: "Germany", comment: "Is a student at a University in San Francisco, major is financial"}});
            colCustomer.save({_key: "C4", name: "C4", age: "young", address: "Tokyo", isStudent: false, comment: "Used to work in financial industry but now working as a teacher at school"});

            colProduct.save({_key: "P1", name: "P1", price: 499.99, used: false, description: "Made in Germany and new"});
            colProduct.save({_key: "P2", name: "P2", price: "expensive", description: "Made in USA and new"});
            colProduct.save({_key: "P3", name: "P3", price: 1499, version: "14.5", used: true, description: "Made in Italy and used"});
            colProduct.save({_key: "P4", name: "P4", price: 2499, version: 1, description: "Made in Japan and used"});

            colCompany.save({_key: "E1", name: "E1", established: 1990, isPublic: true});
            colCompany.save({_key: "E2", name: "E2", established: "5/2025", isPublic: false});
            colCompany.save({_key: "E3", name: "E3", established: 1990});

            pGraph = gm._graph(gnPurchaseHistory);
            mGraph = gm._graph(gnManufacture);

            productDescViewSearch = db._createView(vnProductDescView, "arangosearch", {
                links: {products: {fields: {description: {analyzers: ["text_en"]}}}}});
            descViewSearch = db._createView(vnDescView, "arangosearch", {
                links: {products: {fields: {description: {analyzers: ["text_en"]}}},
                        customers: {fields: {comment: {analyzers: ["text_en"]}}}}});


            function makePurchase(fromKey, toKey, date) {
                pGraph.purchased.save({_from: `${vnCustomer}/${fromKey}`, _to: `${vnProduct}/${toKey}`, date});
            }

            function makeManufacture(fromKey, toKey, amount) {
                mGraph.manufactured.save({_from: `${vnCompany}/${fromKey}`, _to:   `${vnProduct}/${toKey}`, amount});
            }

            makePurchase("C1", "P1", "5/25/2025");
            makePurchase("C2", "P2", "6/01/2025");
            makePurchase("C3", "P3", "1/25/2025");
            makePurchase("C4", "P4", "4/25/2025");

            makeManufacture("E1", "P1", 1000);
            makeManufacture("E2", "P2",   10);
        },

        tearDownAll: function () {
            productDescViewSearch.drop();
            descViewSearch.drop();
            gm._drop(gnPurchaseHistory);
            gm._drop(gnManufacture);
            db._drop(enPurchased);
            db._drop(enManufactured);
            db._drop(vnCustomer);
            db._drop(vnProduct);
            db._drop(vnCompany);
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

            /* Check collection companies */
            const companies = findCollection("companies");
            assertEqual("document", products.collectionType);
            assertEqual(3, companies.numOfDocuments);
            const comschema = companies.schema;
            assertAttribute(comschema, "_id",    ["string"], false);
            assertAttribute(comschema, "_key",   ["string"], false);
            assertAttribute(comschema, "name",   ["string"], false);
            assertAttribute(comschema, "established",  ["number", "string"], false);
            assertAttribute(comschema, "isPublic",   ["bool"], true);

            const exampleCompany = companies.examples[0];
            assertTrue(exampleCompany._id.startsWith("companies/"));

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

            /* Check collection purchased */
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

            /* Check collection purchased */
            const manufactured = findCollection("manufactured");
            assertEqual("edge", manufactured.collectionType);
            assertEqual(2, manufactured.numOfEdges);

            const mschema = manufactured.schema;
            assertAttribute(mschema, "_from", ["string"], false);
            assertAttribute(mschema, "_to",   ["string"], false);
            assertAttribute(mschema, "_id",   ["string"], false);
            assertAttribute(mschema, "_key",  ["string"], false);
            assertAttribute(mschema, "amount",  ["number"], false);

            const exampleManufactured = manufactured.examples[0];
            assertTrue(exampleManufactured._id.startsWith("manufactured/"));
            assertTrue(exampleManufactured._from.startsWith("companies/"));
            assertTrue(exampleManufactured._to.startsWith("products/"));


            /* Check graphs */
            assertTrue(Array.isArray(body.graphs), "graphs must be an array");
            assertEqual(2, body.graphs.length, "Expected 2 graphs for graphs");

            const pGraph = body.graphs.find((g) => g.name === "purchaseHistory");
            assertEqual("purchaseHistory", pGraph.name);
            assertEqual(1, pGraph.relations.length);

            const pRel = pGraph.relations[0];
            assertEqual("purchased", pRel.collection);
            assertEqual("customers", pRel.from[0]);
            assertEqual("products", pRel.to[0]);

            const mGraph = body.graphs.find((g) => g.name === "manufacture");
            assertEqual("manufacture", mGraph.name);
            assertEqual(1, mGraph.relations.length);

            const mRel = mGraph.relations[0];
            assertEqual("manufactured", mRel.collection);
            assertEqual("companies", mRel.from[0]);
            assertEqual("products", mRel.to[0]);

            /* Check views */
            const vProductDescView = body.views.find((v) => v.viewName === "productDescView");
            assertTrue(Array.isArray(vProductDescView.links));
            assertEqual("products", vProductDescView.links[0].collectionName);
            assertTrue(Array.isArray(vProductDescView.links[0].fields));
            assertEqual("description", vProductDescView.links[0].fields[0].attribute);
            assertEqual("text_en", vProductDescView.links[0].fields[0].analyzers[0]);

            const vDescView = body.views.find((v) => v.viewName === "descView");
            assertEqual(2, vDescView.links.length);
            const customersView = vDescView.links.find((v) => v.collectionName === "customers");
            assertEqual("comment", customersView.fields[0].attribute);
            assertEqual("text_en", customersView.fields[0].analyzers[0]);
            const productsView = vDescView.links.find((v) => v.collectionName === "products");
            assertEqual("description", productsView.fields[0].attribute);
            assertEqual("text_en", productsView.fields[0].analyzers[0]);
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
        },

        testSchemaEndpointWithExampleNumEqualTo0: function () {
            const doc = arango.GET_RAW(api + "?exampleNum=0");
            assertEqual(200, doc.code, "Expected HTTP 200 on exampleNum=0");
            const body = doc.parsedBody;
            assertTrue(Array.isArray(body.collections), "Expected 'collections' to be an array");
            body.collections.forEach(function(coll) {
                assertTrue(Array.isArray(coll.examples),
                    "Expected 'examples' to be an array for " + coll.collectionName);
                assertEqual(0, coll.examples.length,
                    "Expected zero examples for " + coll.collectionName);
            });
        },

        testSchemaEndpointWithSampleNumEqualTo0: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=0");
            assertEqual(400, doc.code, "Expected HTTP 400 on sampleNum=0");
        },

        testSchemaGraphEndpoint: function () {
            const doc = arango.GET_RAW(api + "/graph/purchaseHistory");
            assertEqual(200, doc.code, "Expected HTTP 200 on graph/purchaseHistory");
            const body = doc.parsedBody;
            const findCollection = (name) =>
                body.collections.find((col) => col.collectionName === name);

            const assertAttribute = (schema, attrName, expectedTypes, optional) => {
                const attr = schema.find((a) => a.attribute === attrName);
                assertTrue(!!attr, `Attribute '${attrName}' not found`);
                expectedTypes.forEach((type) => {
                    assertTrue(attr.types.includes(type),
                        `'${attrName}' missing type '${type}'`);
                });
                assertEqual(optional, attr.optional,
                    `'${attrName}' optional mismatch`);
            };

            const products = findCollection("products");
            assertTrue(products, "products collection missing");
            assertEqual("document", products.collectionType);
            assertEqual(4, products.numOfDocuments);
            const pschema = products.schema;
            assertAttribute(pschema, "_id",    ["string"], false);
            assertAttribute(pschema, "_key",   ["string"], false);
            assertAttribute(pschema, "name",   ["string"], false);
            assertAttribute(pschema, "price",  ["number","string"], false);
            assertAttribute(pschema, "used",   ["bool"], true);
            assertAttribute(pschema, "version",["number","string"], true);
            const exampleProduct = products.examples[0];
            assertTrue(exampleProduct._id.startsWith("products/"),
                "exampleProduct._id malformed");

            const customers = findCollection("customers");
            assertTrue(customers, "customers collection missing");
            assertEqual("document", customers.collectionType);
            assertEqual(4, customers.numOfDocuments);
            const cschema = customers.schema;
            assertAttribute(cschema, "_id",       ["string"], false);
            assertAttribute(cschema, "_key",      ["string"], false);
            assertAttribute(cschema, "address",   ["string","object"], false);
            assertAttribute(cschema, "age",       ["string","number"], false);
            assertAttribute(cschema, "isStudent", ["bool"], true);
            assertAttribute(cschema, "name",      ["string"], false);
            const exampleCustomer = customers.examples[0];
            assertTrue(exampleCustomer._id.startsWith("customers/"),
                "exampleCustomer._id malformed");

            const purchased = findCollection("purchased");
            assertTrue(purchased, "purchased collection missing");
            assertEqual("edge", purchased.collectionType);
            assertEqual(4, purchased.numOfEdges);
            const eschema = purchased.schema;
            assertAttribute(eschema, "_from", ["string"], false);
            assertAttribute(eschema, "_to",   ["string"], false);
            assertAttribute(eschema, "_id",   ["string"], false);
            assertAttribute(eschema, "_key",  ["string"], false);
            assertAttribute(eschema, "date",  ["string"], false);
            const exampleEdge = purchased.examples[0];
            assertTrue(exampleEdge._id.startsWith("purchased/"),
                "exampleEdge._id malformed");
            assertTrue(exampleEdge._from.startsWith("customers/"),
                "exampleEdge._from malformed");
            assertTrue(exampleEdge._to.startsWith("products/"),
                "exampleEdge._to malformed");

            const graph = body.graphs.find((g) => g.name === "purchaseHistory");
            assertTrue(graph, "purchaseHistory graph missing");
            assertEqual("purchaseHistory", graph.name);
            assertEqual(1, graph.relations.length);
            const rel = graph.relations[0];
            assertEqual("purchased", rel.collection);
            assertEqual("customers", rel.from[0]);
            assertEqual("products", rel.to[0]);
        },

        testSchemaGraphEndpointWithNotExistingGraph: function () {
            const doc = arango.GET_RAW(api + "/graph/fake");
            assertEqual(404, doc.code, "Expected HTTP 404 with a not exsting graph");
        },

        testSchemaEndpointWithLargerExampleNumThanActual: function () {
            const doc = arango.GET_RAW(api + "/collection/customers?exampleNum=100");
            assertEqual(200, doc.code, "Expected HTTP 200");
            const body = doc.parsedBody;
            assertTrue(Array.isArray(body.examples));
            assertEqual(4, body.examples.length);
        },

        testSchemaViewEndpoint: function () {
            const doc = arango.GET_RAW(api + "/view/descView");
            assertEqual(200, doc.code, "Expected HTTP 200");
            const body = doc.parsedBody;
            const vDesc = body.views[0];
            assertTrue(Array.isArray(vDesc.links));
            const vCustomers = vDesc.links.find((v) => v.collectionName === "customers");
            assertEqual("comment", vCustomers.fields[0].attribute);
            assertEqual("text_en", vCustomers.fields[0].analyzers[0]);
            const vProducts = vDesc.links.find((v) => v.collectionName === "products");
            assertEqual("description", vProducts.fields[0].attribute);
            assertEqual("text_en", vProducts.fields[0].analyzers[0]);

            const colls = body.collections;
            assertEqual(2, colls.length);
            const cCustomers = colls.find((c) => c.collectionName === "customers");
            assertEqual(4, cCustomers.numOfDocuments);
            assertEqual(7, cCustomers.schema.length);
            const cProducts = colls.find((c) => c.collectionName === "products");
            assertEqual(4, cProducts.numOfDocuments);
            assertEqual(7, cProducts.schema.length);
        },

        testSchemaViewEndpointWithNotExistingView: function () {
            const doc = arango.GET_RAW(api + "/view/fake");
            assertEqual(404, doc.code, "Expected HTTP 404");
        },

        testSchemaViewEndpointWithEmptyViewName: function () {
            const doc = arango.GET_RAW(api + "/view");
            assertEqual(404, doc.code, "Expected HTTP 404");
        },
    };
}

jsunity.run(restSchemaHandlerTestSuite);
return jsunity.done();