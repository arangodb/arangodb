/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const isCluster = internal.isCluster();
const errors = internal.errors;
const cn = "UnitTestsCollection";

function ComputedValuesAfterCreateCollectionTestSuite() {
  'use strict';

  let collection = null;
  return {

    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

    tearDown: function() {
      internal.db._drop(cn);
      collection = null;
    },

    testWithoutReturnKeywordOnExpression: function() {
      try {
        collection.properties({
          computedValues: [{
            name: "newValue",
            expression: "CONCAT(@doc.value1, '+')",
            override: false
          }]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testCreateOnEmptyChangeAfterInsert: function() {
      collection.properties({
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: false
        }]
      });

      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({name: "test" + i});
      }
      collection.insert(docs);

      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.concatValues, "+");
      });

      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc"});
      }
      collection.insert(docs);

      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER HAS (doc, "value1") FILTER HAS (doc, "value2")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      res.forEach(el => {
        assertEqual(res.length, 100);
        assertEqual(el.concatValues, el.value1 + '+' + el.value2);
      });
    },

    testComputedValuesOnlyOnInsert: function() {
      collection.properties({
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: true,
          computeOn: ["insert"]
        }]
      });
      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc", value3: false});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertTrue(el.hasOwnProperty("concatValues"));
        assertEqual(el.concatValues, `${el.value1}+${el.value2}`);
      });

      res = collection.insert({value4: true});
      let docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues, "+");
      res = collection.update(docAttributes["_key"], {value1: "abc123"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues, "+");

      res = collection.replace(docAttributes["_key"], {value1: "abc"});
      docAttributes = collection.document(res["_key"]);
      assertFalse(docAttributes.hasOwnProperty("concatValues"));
    },

    testComputedValuesOnlyOnUpdate: function() {
      collection.properties({
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: true,
          computeOn: ["update"]
        }]
      });
      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc", value3: false});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertFalse(el.hasOwnProperty("concatValues"));
      });

      res = collection.insert({value4: true});
      let docAttributes = collection.document(res["_key"]);
      assertFalse(docAttributes.hasOwnProperty("concatValues"));
      res = collection.update(docAttributes["_key"], {value1: "abc123"});
      docAttributes = collection.document(res["_key"]);
      assertTrue(docAttributes.hasOwnProperty("concatValues"));
      assertEqual(docAttributes.concatValues, "abc123+");

      res = collection.replace(docAttributes["_key"], {value1: "abc"});
      docAttributes = collection.document(res["_key"]);
      assertFalse(docAttributes.hasOwnProperty("concatValues"));
    },

    testComputedValuesOnlyOnReplace: function() {
      collection.properties({
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: true,
          computeOn: ["replace"]
        }]
      });
      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc", value3: false});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertFalse(el.hasOwnProperty("concatValues"));
      });

      res = collection.insert({value4: true});
      let docAttributes = collection.document(res["_key"]);
      assertFalse(docAttributes.hasOwnProperty("concatValues"));
      res = collection.update(docAttributes["_key"], {value1: "abc123"});
      docAttributes = collection.document(res["_key"]);
      assertFalse(docAttributes.hasOwnProperty("concatValues"));
      res = collection.replace(docAttributes["_key"], {value1: "abc"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues, "abc+");
    },

    testCreateAccessNonTopLevel: function() {
      collection.properties({
        computedValues: [{
          name: "value3",
          expression: "RETURN CONCAT(LEFT(@doc.value1, 3), RIGHT(@doc.value2.animal, 2))",
          override: false
        }]
      });

      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "value3");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: [{animal: "dog"}]});
      }
      collection.insert(docs);

      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.value3, `${el.value1.substring(0, 3)}`);
      });
    },

    testCreateAfterInsertedDocumentsInsertAgain: function() {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "test" + i % 2});
      }
      collection.insert(docs);

      collection.properties({
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: false
        }]
      });

      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");


      let res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") RETURN doc`).toArray();
      assertEqual(res.length, 0);

      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc", value3: 0});
      }
      collection.insert(docs);

      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER HAS (doc, "value3")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      res.forEach(el => {
        assertEqual(res.length, 100);
        assertEqual(el.concatValues, el.value1 + '+' + el.value2);
      });
      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER !HAS (doc, "value3")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      assertEqual(res.length, 0);
    },

    testAccessRevOnCreate: function() {
      try {
        collection.properties({
          computedValues: [{
            name: "_rev",
            expression: "RETURN CONCAT(@doc.value1, '+')",
            override: false
          }]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testAccessKeyOnCreate: function() {
      try {
        collection.properties({
          computedValues: [{
            name: "_key",
            expression: "RETURN CONCAT(@doc.value1, '+')",
            override: false
          }]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testAccessIdOnCreate: function() {
      try {
        collection.properties({
          computedValues: [{
            name: "_id",
            expression: "RETURN CONCAT(@doc.value1, '+')",
            override: false
          }]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testSchemaValidationWithComputedValuesOverride: function() {
      collection.properties({
        schema: {
          "rule": {
            "type": "object",
            "properties": {
              "value1": {
                "type": "boolean",
              },
            },
            "required": ["value1"],
          },
          "level": "moderate",
          "message": "Schema validation failed"
        }, computedValues: [{
          name: "value1",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: true
        }]
      });
      if (isCluster) {
        // unfortunately there is no way to test when the new properties
        // have been applied on the DB servers. all we can do is sleep
        // and hope the delay is long enough
        internal.sleep(5);
      }

      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "value1");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        const newValue = i % 2 ? true : false;
        docs.push({value1: newValue, value2: newValue});
      }
      let res = collection.insert(docs);
      res.forEach(el => {
        assertEqual(errors.ERROR_VALIDATION_FAILED.code, el.errorNum);
        assertEqual(el.errorMessage, "Schema validation failed");
      });

      res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      assertEqual(res.length, 0);
    },

    testSchemaValidationWithComputedValuesNoOverride: function() {
      collection.properties({
        schema: {
          "rule": {
            "type": "object",
            "properties": {
              "value1": {
                "type": "string",
              },
            },
            "required": ["value1"],
          },
          "level": "moderate",
          "message": "Schema validation failed"
        }
      });
      collection.properties({
        computedValues: [{
          name: "value1",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: false
        }]
      });
      if (isCluster) {
        // unfortunately there is no way to test when the new properties
        // have been applied on the DB servers. all we can do is sleep
        // and hope the delay is long enough
        internal.sleep(5);
      }

      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "value1");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        const newValue = i % 2 ? true : false;
        docs.push({value1: newValue, value2: newValue});
      }
      let res = collection.insert(docs);

      res.forEach(el => {
        assertEqual(errors.ERROR_VALIDATION_FAILED.code, el.errorNum);
        assertEqual(el.errorMessage, "Schema validation failed");
      });

      res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      assertEqual(res.length, 0);
    },

    testCompoundComputedValues: function() {
      collection.properties({
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: false
        }, {
          name: "value3",
          expression: "return CONCAT(@doc.concatValues, ' ')",
          override: false
        }]
      });
      if (isCluster) {
        // unfortunately there is no way to test when the new properties
        // have been applied on the DB servers. all we can do is sleep
        // and hope the delay is long enough
        internal.sleep(5);
      }

      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 2);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");
      assertTrue(colProperties.computedValues[1].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[1].name, "value3");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc"});
      }
      collection.insert(docs);

      let res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(res.length, 100);
        assertEqual(el.concatValues, el.value1 + '+' + el.value2);
        assertEqual(el.value3, " ");
      });
    },

    testSpecialFunctionsNotAllowedInExpression: function() {
      collection.ensureIndex({type: "geo", fields: ["latitude, longitude"], geoJson: true});
      const schemaTest = {
        "rule": {
          "type": "object",
          "properties": {
            "value1": {
              "type": "boolean",
            },
          },
          "required": ["value1"],
        },
        "level": "moderate",
        "message": "Schema validation failed"
      };
      const specialFunctions = ["COLLECTIONS", "CURRENT_DATABASE", "CURRENT_USER", "VERSION", `CALL("SUBSTRING", @doc.value1, 0, 2)`, `APPLY("SUBSTRING", [@doc.value1, 0, 2])`, `DOCUMENT(${cn}, "test")`, `V8(1 + 1)`, `SCHEMA_GET(${cn}`, `SCHEMA_VALIDATE(@doc, ${schemaTest}`, `COLLECTION_COUNT(${cn}`, `NEAR(${cn}, @doc.latitude, @doc.longitude)`, `WITHIN(${cn}, @doc.latitude, @doc.longitude, 123)`, `WITHIN_RECTANGLE(${cn}, @doc.latitude, @doc.longitude, @doc.latitude, @doc.longitude)`];
      specialFunctions.forEach((el) => {
        try {
          collection.properties({
            computedValues: [{
              name: "newValue",
              expression: `RETURN ${el}`,
              override: false,
              computeOn: ["insert"]
            }]
          });
          fail();
        } catch (error) {
          assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
        }
      });
    }
  };
}

function ComputedValuesOnCollectionCreationOverrideTestSuite() {
  'use strict';

  let collection = null;
  return {
    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn, {
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          computeOn: ["insert", "update", "replace"],
          override: true
        }
        ]
      });
    },

    tearDown: function() {
      internal.db._drop(cn);
      collection = null;
    },

    testCreateOnEmptyChangeAfterInsert2: function() {
      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({name: "test" + i});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.concatValues, "+");
      });

      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc"});
      }
      collection.insert(docs);

      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER HAS (doc, "value1") FILTER HAS (doc, "value2")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.concatValues, el.value1 + '+' + el.value2);
      });
    },

    testOverrideOnAllOperations: function() {
      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc", value3: false, concatValues: "test" + i});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.concatValues, `${el.value1}+${el.value2}`);
      });

      res = collection.insert({value4: true});
      let docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues, "+");
      res = collection.update(docAttributes["_key"], {value1: "abc123"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues, "abc123+");

      res = collection.replace(docAttributes["_key"], {value1: "abc"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues, "abc+");
    },
  };
}

function ComputedValuesOnCollectionCreationNoOverrideTestSuite() {
  'use strict';

  let collection = null;
  return {
    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn, {
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          computeOn: ["insert", "update", "replace"],
          override: false
        }
        ]
      });
    },

    tearDown: function() {
      internal.db._drop(cn);
      collection = null;
    },

    testCreateOnEmptyChangeAfterInsert3: function() {
      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({name: "test" + i});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.concatValues, "+");
      });

      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc"});
      }
      collection.insert(docs);

      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER HAS (doc, "value1") FILTER HAS (doc, "value2")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.concatValues, el.value1 + '+' + el.value2);
      });
    },

    testNoOverrideOnAllOperations: function() {
      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name, "concatValues");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc", value3: false, concatValues: "test" + i});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach((el, index) => {
        assertEqual(el.concatValues, `test${index}`);
      });

      res = collection.insert({value4: true});
      let docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues, "+");
      res = collection.update(docAttributes["_key"], {value1: "abc123"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues, "+");

      res = collection.replace(docAttributes["_key"], {value1: "abc"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues, "abc+");
    },
  };
}

function ComputedValuesClusterShardsTestSuite() {
  //TODO: use RAND() when differentiating between servers to get the same result

  'use strict';

  let collection = null;
  return {
    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn, {numberOfShards: 4, shardKeys: ["value1"]});
    },

    tearDown: function() {
      internal.db._drop(cn);
      collection = null;
    },

    testAccessShardKeys: function() {
      try {
        collection.properties({
          computedValues:
            [{
              name: "value1",
              expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
              computeOn: ["insert", "update", "replace"],
              override: false
            }]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ComputedValuesOnCollectionCreationOverrideTestSuite);
jsunity.run(ComputedValuesOnCollectionCreationNoOverrideTestSuite);
jsunity.run(ComputedValuesAfterCreateCollectionTestSuite);
if (isCluster) {
  jsunity.run(ComputedValuesClusterShardsTestSuite);
}

return jsunity.done();
