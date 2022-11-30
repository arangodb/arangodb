var jsunity = require("jsunity");
var db = require("@arangodb").db;
var analyzers = require("@arangodb/analyzers");
var ERRORS = require("@arangodb").errors;

function IResearchFeatureDDLTestSuite() {
    let docCount = 0;
    return {
        setUpAll: function () {
            db._create("collection123");
        },

        tearDownAll: function () {
            db._drop("collection123");
            // db._dropView("TestView");

        },

        testViewModifyImmutableProperties: function () {

            db["collection123"].save({ name_1: "1", "value": [{ "nested_1": [{ "nested_2": "foo123" }] }], "my": {"Nested": {"field": 1}}, "another": {"field": 2 }, "field": "345635"});
            db["collection123"].save({ name_1: "1", "value": [{ "nested_1": [{ "nested_2": "foo123" }] }], "my": {"Nested": {"field": 1}}, "another": {"field": 2 }, "field": "345635"});
            
            print("here")

            var view = db._createView("TestView", "arangosearch", {
                primarySort: [
                    { field: "my.Nested.field", direction: "asc" },
                    { field: "another.field", asc: false },
                ],
                "links": {
                    "collection123": {"fields": { "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}
                  }
            });
            print("here")

            // view.properties({ 
            //   writebufferActive: 225,
            //   writebufferIdle: 112,
            //   writebufferSizeMax: 414040192,
            //   locale: "en_EN.UTF-8",
            //   version: 2,
            //   primarySort: [ { field: "field", asc: false } ],
            //   primarySortCompression:"lz4",
            //   "cleanupIntervalStep": 442,
            //   "links": {
            //     "collection123": {"fields": { "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}}}
            //   }
            // }, false); // full update
            // print("here")
        }
    };
}

jsunity.run(IResearchFeatureDDLTestSuite);
return jsunity.done();    