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

            colCustomer.save({_key: "Gilberto", name: "Gilberto", age: 25, address: "Milano"});
            colCustomer.save({_key: "Koichi", name: "Koichi", age: "35", address: "San Francisco", isStudent: true});
            colCustomer.save({_key: "Michael", name: "Michael", age: 35, address: {city: "Cologne", country: "Germany"}});
            colCustomer.save({_key: "Victor", name: "Victor", age: "young", address: "Tokyo", isStudent: false});

            colProduct.save({_key: "drone", name: "drone", price: 499.99});
            colProduct.save({_key: "glasses", name: "glasses", price: "expensive"});
            colProduct.save({_key: "macBook", name: "macBook", price: 1499, version: "14.5"});
            colProduct.save({_key: "VR", name: "VR", price: 2499, version: 1});

            let graph = gm._graph(gnPurchaseHistory);
            function makeEdge(from, to, date) {
                graph.purchased.save({_from: vnCustomer + "/" + from, _to: vnProduct + "/" + to, date: date});
            }
            makeEdge("Gilberto", "drone", "5/25/2025");
            makeEdge("Koichi", "macBook", "6/01/2025");
            makeEdge("Michael", "VR", "1/25/2025");
            makeEdge("Victor", "glasses", "4/25/2025");
        },

        tearDownAll: function () {
            gm._drop(gnPurchaseHistory);
            db._drop(enPurchased);
            db._drop(vnCustomer);
            db._drop(vnProduct);
        },

        testGraph: function () {
            const doc = arango.GET_RAW(api + "?sampleNum=1");

            print("Parsed body:", JSON.stringify(doc.parsedBody, null, 2));

            assertEqual(200, doc.code);
        }
    };
}

jsunity.run(restSchemaHandlerTestSuite);
return jsunity.done();