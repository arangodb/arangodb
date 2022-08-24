/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertMatch, assertNull, assertNotNull, assertUndefined, fail */

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const isCluster = internal.isCluster();
const errors = internal.errors;
const aqlfunctions = require("@arangodb/aql/functions");
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
          computedValues: [
            {
              name: "newValue",
              expression: "CONCAT(@doc.value1, '+')",
              overwrite: false
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },
    
    testKeepNullTrue: function() {
      collection.properties({
        computedValues: [
          {
            name: "newValue",
            expression: "RETURN @doc.foxx",
            overwrite: true,
            keepNull: true,
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "newValue");
      assertTrue(colProperties.computedValues[0].keepNull);

      collection.insert({});
      assertEqual(1, collection.count());
      // computed value must be null
      assertNull(collection.toArray()[0].newValue);
    },

    testKeepNullTrue2: function() {
      collection.properties({
        computedValues:
          [{
            name: "newValue",
            expression: "RETURN IS_STRING(@doc.name.first) AND IS_STRING(@doc.name.last) ? " +
              "MERGE(@doc.name, { full: CONCAT_SEPARATOR(' ', @doc.name.first, @doc.name.last) }) : null",
            overwrite: true,
            keepNull: true
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
      assertEqual(colProperties.computedValues[0].name, "newValue");
      assertTrue(colProperties.computedValues[0].keepNull);

      for (let i = 0; i < 50; ++i) {
        collection.insert({name: {first: `a${i}`, last: `b${i}`}});
      }
      for (let i = 0; i < 50; ++i) {
        collection.insert({name: {first: i, last: i}});
      }
      let amountNullValues = 0;
      const docs = collection.toArray();
      docs.forEach(doc => {
        assertTrue(doc.hasOwnProperty("newValue"));
        if (doc["newValue"] === null) {
          amountNullValues++;
        }
      });
      assertEqual(100, collection.count());
      assertEqual(amountNullValues, 50);
    },
    
    testKeepNullFalse: function() {
      collection.properties({
        computedValues: [
          {
            name: "newValue",
            expression: "RETURN @doc.foxx",
            overwrite: true,
            keepNull: false,
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "newValue");
      assertFalse(colProperties.computedValues[0].keepNull);

      collection.insert({});
      assertEqual(1, collection.count());
      // computed value must not be there
      assertUndefined(collection.toArray()[0].newValue);
    },

    testKeepNullFalse2: function() {
      collection.properties({
        computedValues:
          [{
            name: "newValue",
            expression: "RETURN IS_STRING(@doc.name.first) AND IS_STRING(@doc.name.last) ? " +
              "MERGE(@doc.name, { full: CONCAT_SEPARATOR(' ', @doc.name.first, @doc.name.last) }) : null",
            overwrite: true,
            keepNull: false
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
      assertEqual(colProperties.computedValues[0].name, "newValue");
      assertFalse(colProperties.computedValues[0].keepNull);

      for (let i = 0; i < 50; ++i) {
        collection.insert({name: {first: `a${i}`, last: `b${i}`}});
      }
      for (let i = 0; i < 50; ++i) {
        collection.insert({name: {first: i, last: i}});
      }
      const docs = collection.toArray();
      let amountValues = 0;
      docs.forEach(doc => {
        if (doc.hasOwnProperty("newValue")) {
          assertNotNull(doc["newValue"]);
        } else {
          amountValues++;
        }
      });
      assertEqual(100, collection.count());
      assertEqual(amountValues, 50);
    },
    
    testExpressionWithAssert: function() {
      // expression must not be executed immediately
      collection.properties({
        computedValues: [
          {
            name: "value",
            expression: "RETURN ASSERT(false, 'piff!')",
            overwrite: false,
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "value");

      try {
        // inserting any document into the collection will fail with
        // assertion error
        collection.insert({});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_USER_ASSERT.code, err.errorNum);
        assertMatch(/for attribute 'value'/, err.errorMessage);
        assertMatch(/piff!/, err.errorMessage);
      }
    },
    
    testExpressionWithWarning: function() {
      collection.properties({
        computedValues: [
          {
            name: "newValue",
            expression: "RETURN 42 / @doc.value",
            overwrite: false,
            failOnWarning: false,
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "newValue");

      collection.insert({});
      assertEqual(1, collection.count());
      // computed value must be null
      assertNull(collection.toArray()[0].newValue);
    },
    
    testExpressionWithWarningAndFailOnWarning: function() {
      collection.properties({
        computedValues: [
          {
            name: "newValue",
            expression: "RETURN 42 / @doc.value",
            overwrite: false,
            failOnWarning: true,
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "newValue");

      try {
        // inserting any document into the collection will fail with
        // assertion error
        collection.insert({});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_QUERY_DIVISION_BY_ZERO.code, err.errorNum);
        assertMatch(/for attribute 'newValue'/, err.errorMessage);
        assertMatch(/division by zero/, err.errorMessage);
      }
    },
    
    testDefaultValueForComputeOn: function() {
      collection.properties({
        computedValues: [
          {
            name: "value",
            expression: "RETURN 'foo'",
            overwrite: false,
            // computeOn not set
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "value");
      assertEqual(colProperties.computedValues[0].computeOn, ["insert", "update", "replace"]);
    },

    testSetStringValueForComputeOn: function() {
      try {
        collection.properties({
          computedValues: [
            {
              name: "value",
              expression: "RETURN 'foo'",
              overwrite: false,
              computeOn: "insert",
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },
    
    testSetArrayValueForComputeOn: function() {
      collection.properties({
        computedValues: [
          {
            name: "value",
            expression: "RETURN 'foo'",
            overwrite: false,
            computeOn: ["insert", "replace"],
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "value");
      assertEqual(colProperties.computedValues[0].computeOn, ["insert", "replace"]);
    },

    testCreateOnEmptyChangeAfterInsert: function() {
      collection.properties({
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: false
          }
        ]
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


    testComputedValuesOnlyOnInsert: function() {
      collection.properties({
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: true,
            computeOn: ["insert"]
          }
        ]
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
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: true,
            computeOn: ["update"]
          }
        ]
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
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: true,
            computeOn: ["replace"]
          }
        ]
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
        computedValues: [
          {
            name: "value3",
            expression: "RETURN CONCAT(LEFT(@doc.value1, 3), RIGHT(@doc.value2.animal, 2))",
            overwrite: false
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "value3");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: {animal: "dog"}});
      }
      collection.insert(docs);

      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.value3, `${el.value1.substring(0, 3)}og`);
      });
    },

    testCreateAfterInsertedDocumentsInsertAgain: function() {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "test" + i % 2});
      }
      collection.insert(docs);

      collection.properties({
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: false
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "concatValues");


      let res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") RETURN doc`).toArray();
      assertEqual(res.length, 0);

      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc", value3: 0});
      }
      collection.insert(docs);

      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER HAS (doc, "value3")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.concatValues, el.value1 + '+' + el.value2);
      });
      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER !HAS (doc, "value3")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      assertEqual(res.length, 0);
    },

    testAccessRevOnCreate: function() {
      try {
        collection.properties({
          computedValues: [
            {
              name: "_rev",
              expression: "RETURN CONCAT(@doc.value1, '+')",
              overwrite: false
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testAccessKeyOnCreate: function() {
      try {
        collection.properties({
          computedValues: [
            {
              name: "_key",
              expression: "RETURN CONCAT(@doc.value1, '+')",
              overwrite: false
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testAccessIdOnCreate: function() {
      try {
        collection.properties({
          computedValues: [
            {
              name: "_id",
              expression: "RETURN CONCAT(@doc.value1, '+')",
              overwrite: false
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testComputeValuesSubquery: function() {
      try {
        collection.properties({
          computedValues: [
            {
              name: "newValue",
              expression: "RETURN (RETURN CONCAT(@doc.value1, '+'))",
              overwrite: false
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testNotAllowedFor: function() {
      try {
        collection.properties({
          computedValues: [
            {
              name: "newValue",
              expression: "RETURN (FOR i IN 1..10 RETURN i)",
              overwrite: false
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testNowAllowedLet: function() {
      try {
        collection.properties({
          computedValues: [
            {
              name: "newValue",
              expression: "RETURN (LET temp1 = (RETURN 1))",
              overwrite: false
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testSchemaValidationWithComputedValuesoverwrite: function() {
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
        }, computedValues: [
          {
            name: "value1",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: true
          }
        ]
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

    testSchemaValidationWithComputedValuesNooverwrite: function() {
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
        computedValues: [
          {
            name: "value1",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: false
          }
        ]
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


    testRedefineComputedValueUpdateoverwrite: function() {
      collection.properties({
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: false
          }, {
            name: "value3",
            expression: "return CONCAT(@doc.concatValues, ' ')",
            overwrite: false
          }
        ]
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
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.concatValues, el.value1 + '+' + el.value2);
        assertEqual(el.value3, " ");
      });

      collection.properties({
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            computeOn: ["update"],
            overwrite: true
          }, {
            name: "value3",
            expression: "return CONCAT(@doc.concatValues, '+', @doc.value1)",
            computeOn: ["update"],
            overwrite: true
          }
        ]
      });

      if (isCluster) {
        // unfortunately there is no way to test when the new properties
        // have been applied on the DB servers. all we can do is sleep
        // and hope the delay is long enough
        internal.sleep(5);
      }

      res = db._query(`FOR doc IN ${cn} UPDATE doc WITH {value2: "123"} IN ${cn} RETURN {oldConcatValues: OLD.concatValues, new: NEW}`).toArray();
      assertEqual(res.length, 100);
      res.forEach((el, index) => {
        assertEqual(el.new.value1, "test" + index);
        assertEqual(el.new.value2, "123");
        assertEqual(el.new.concatValues, el.new.value1 + "+" + el.new.value2);
        assertEqual(el.new.value3, el.oldConcatValues + "+" + el.new.value1);
      });
    },

    testRedefineComputedValueUpdateNooverwrite: function() {
      collection.properties({
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: false
          }, {
            name: "value3",
            expression: "return CONCAT(@doc.concatValues, ' ')",
            overwrite: false
          }
        ]
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
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.concatValues, el.value1 + '+' + el.value2);
        assertEqual(el.value3, " ");
      });

      collection.properties({
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            computeOn: ["update"],
            overwrite: false
          }, {
            name: "value3",
            expression: "return CONCAT(@doc.concatValues, '+', @doc.value1)",
            computeOn: ["update"],
            overwrite: false
          }
        ]
      });

      if (isCluster) {
        // unfortunately there is no way to test when the new properties
        // have been applied on the DB servers. all we can do is sleep
        // and hope the delay is long enough
        internal.sleep(5);
      }

      res = db._query(`FOR doc IN ${cn} UPDATE doc WITH {value2: "123"} IN ${cn} RETURN {oldValue3: OLD.value3, oldConcatValues: OLD.concatValues, new: NEW}`).toArray();
      assertEqual(res.length, 100);
      res.forEach((el, index) => {
        assertEqual(el.new.value1, "test" + index);
        assertEqual(el.new.value2, "123");
        assertEqual(el.new.concatValues, el.oldConcatValues);
        assertEqual(el.new.value3, el.oldValue3);
      });
    },

    testCompoundComputedValues: function() {
      collection.properties({
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            overwrite: false
          }, {
            name: "value3",
            expression: "return CONCAT(@doc.concatValues, ' ')",
            overwrite: false
          }
        ]
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
      assertEqual(res.length, 100);
      res.forEach(el => {
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
      const specialFunctions = [
        "COLLECTIONS", 
        "CURRENT_DATABASE", 
        "CURRENT_USER", 
        "VERSION", 
        `CALL("SUBSTRING", @doc.value1, 0, 2)`, 
        `APPLY("SUBSTRING", [@doc.value1, 0, 2])`, 
        `DOCUMENT(${cn}, "test")`, `V8(1 + 1)`, 
        `SCHEMA_GET(${cn}`, 
        `SCHEMA_VALIDATE(@doc, ${schemaTest}`, 
        `COLLECTION_COUNT(${cn}`, 
        `NEAR(${cn}, @doc.latitude, @doc.longitude)`, 
        `TOKENS(@doc, 'text_en')`,
        `TOKENS("foo bar", 'text_en')`,
        `WITHIN(${cn}, @doc.latitude, @doc.longitude, 123)`, 
        `WITHIN_RECTANGLE(${cn}, @doc.latitude, @doc.longitude, @doc.latitude, @doc.longitude)`, 
        `PREGEL_RESULT("abc", true)`
      ];
      specialFunctions.forEach((el) => {
        try {
          collection.properties({
            computedValues: [
              {
                name: "newValue",
                expression: `RETURN ${el}`,
                overwrite: false,
                computeOn: ["insert"]
              }
            ]
          });
          fail();
        } catch (error) {
          assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
        }
      });
    },

    testUserDefinedFunctions: function() {
      aqlfunctions.register("UnitTests::cv", function(what) { return what * 2; }, true);
      assertEqual("function(what) { return what * 2; }", aqlfunctions.toArray("UnitTests")[0].code);

      try {
        collection.properties({
          computedValues: [
            {
              name: "newValue",
              expression: `RETURN UnitTests::cv(1)`,
              overwrite: false,
              computeOn: ["insert"]
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      } finally {
        aqlfunctions.unregisterGroup("UnitTests::");
      }
    },

    testArrayOperator: function() {
      collection.properties({
        computedValues: [
          {
            name: "newValue",
            expression: `RETURN @doc.values[* RETURN CURRENT]`,
            overwrite: false,
            computeOn: ["insert"]
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "newValue");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({values: [1, 2, 3, 4, 5, 6, 7, 8]});
      }
      collection.insert(docs);

      const res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.values, [1, 2, 3, 4, 5, 6, 7, 8]);
        assertEqual(el.newValue, [1, 2, 3, 4, 5, 6, 7, 8]);
      });
    },

    testArrayOperatorWithInline: function() {
      collection.properties({
        computedValues: [
          {
            name: "newValue",
            expression: `RETURN @doc.values[* FILTER CURRENT % 2 == 0 LIMIT 0,1 RETURN CURRENT]`,
            overwrite: false,
            computeOn: ["insert"]
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "newValue");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({values: [1, 2, 3, 4, 5, 6, 7, 8]});
      }
      collection.insert(docs);

      const res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.values, [1, 2, 3, 4, 5, 6, 7, 8]);
        assertEqual(el.newValue, [2]);
      });
    },

    testArrayOperatorWithInlineAndLargeValues: function() {
      collection.properties({
        computedValues: [
          {
            name: "newValue",
            expression: `RETURN @doc.values[* LIMIT 1,3 RETURN CURRENT]`,
            overwrite: false,
            computeOn: ["insert"]
          }
        ]
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
      assertEqual(colProperties.computedValues[0].name, "newValue");

      let values = [];
      for (let i = 0; i < 10; ++i) {
        values.push("this-is-a-long-test-string-too-long-for-sso-" + i);
      }
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({values});
      }
      collection.insert(docs);

      const expected = [ values[1], values[2], values[3] ];
      const res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.values, values, el);
        assertEqual(el.newValue, expected, el);
      });
    },

    testRandValues: function() {
      // the result of RAND() must be recomputed every time we
      // invoke the computed values expression.
      collection.properties({
        computedValues: [
          {
            name: "newValue",
            expression: `RETURN TO_STRING(RAND())`,
            overwrite: false,
            computeOn: ["insert"]
          }
        ]
      });
      if (isCluster) {
        // unfortunately there is no way to test when the new properties
        // have been applied on the DB servers. all we can do is sleep
        // and hope the delay is long enough
        internal.sleep(5);
      }

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({});
      }
      collection.insert(docs);

      const res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      assertEqual(res.length, 100);

      let valuesFound = {};
      res.forEach(el => {
        valuesFound[el.newValue] = 1;
      });

      // assume we got at least 10 different RAND() values for 100 
      // documents.
      assertTrue(Object.keys(valuesFound).length >= 10, valuesFound);
    },
    
    testDateValues: function() {
      // the result of DATE_NOW() must be recomputed every time we
      // invoke the computed values expression.
      collection.properties({
        computedValues: [
          {
            name: "newValue",
            expression: `RETURN TO_STRING(DATE_NOW())`,
            overwrite: false,
            computeOn: ["insert"]
          }
        ]
      });
      if (isCluster) {
        // unfortunately there is no way to test when the new properties
        // have been applied on the DB servers. all we can do is sleep
        // and hope the delay is long enough
        internal.sleep(5);
      }

      let valuesCreated = {};
      for (let i = 0; i < 100; ++i) {
        let doc = collection.insert({}, {returnNew: true});
        internal.wait(0.01);
        valuesCreated[doc.new.newValue] = 1;
      }

      // assume we got at least 10 different DATE_NOW() values for 100 
      // documents.
      assertTrue(Object.keys(valuesCreated).length >= 10, valuesCreated);
    },

  };
}

function ComputedValuesOnCollectionCreationoverwriteTestSuite() {
  'use strict';

  let collection = null;
  return {
    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn, {
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            computeOn: ["insert", "update", "replace"],
            overwrite: true
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

    testoverwriteOnAllOperations: function() {
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

function ComputedValuesOnCollectionCreationNooverwriteTestSuite() {
  'use strict';

  let collection = null;
  return {
    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn, {
        computedValues: [
          {
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
            computeOn: ["insert", "update", "replace"],
            overwrite: false
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

    testNooverwriteOnAllOperations: function() {
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
          computedValues: [
            {
              name: "value1",
              expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
              overwrite: false
            }
          ]
        });
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },
  };
}

jsunity.run(ComputedValuesOnCollectionCreationoverwriteTestSuite);
jsunity.run(ComputedValuesOnCollectionCreationNooverwriteTestSuite);
jsunity.run(ComputedValuesAfterCreateCollectionTestSuite);
if (isCluster) {
  jsunity.run(ComputedValuesClusterShardsTestSuite);
}

return jsunity.done();
