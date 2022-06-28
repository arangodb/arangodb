/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTypeOf, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
///
/// @file
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const errors = internal.errors;
const cn = "UnitTestsCollection";

function ComputedValuesTestSuiteAfterCreate() {
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
      assertEqual(colProperties.computedValues[0].name.localeCompare("concatValues"), 0);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({name: "test" + i});
      }
      collection.insert(docs);

      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.concatValues.localeCompare("+"), 0);
      });

      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc"});
      }
      collection.insert(docs);

      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER HAS (doc, "value1") FILTER HAS (doc, "value2")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      res.forEach(el => {
        assertEqual(res.length, 100);
        assertEqual(el.concatValues.localeCompare(el.value1 + '+' + el.value2), 0);
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
      assertEqual(colProperties.computedValues[0].name.localeCompare("concatValues"), 0);


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
        assertEqual(el.concatValues.localeCompare(el.value1 + '+' + el.value2), 0);
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
      } catch (error) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    },

    testAccessNonTopLevel: function() {
      try {
        collection.properties({
          computedValues: [{
            name: "concatValues",
            expression: "RETURN CONCAT(@doc.value1, '+', @doc.value1.value2)",
            override: false
          }]
        });
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
        }
      });
      collection.properties({
        computedValues: [{
          name: "value1",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: true
        }]
      });

      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name.localeCompare("value1"), 0);

      let docs = [];
      for (let i = 0; i < 10; ++i) {
        const newValue = i % 2 ? true : false;
        docs.push({value1: newValue, value2: newValue});
      }
      collection.insert(docs);

      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.value1.localeCompare(`${el.value2}+${el.value2}`), 0);
      });
    },

    testSchemaValidationWithComputedValuesNoOverride: function() {
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
        }
      });
      collection.properties({
        computedValues: [{
          name: "value1",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
          override: false
        }]
      });

      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name.localeCompare("value1"), 0);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        const newValue = i % 2 ? true : false;
        docs.push({value1: newValue, value2: newValue});
      }
      collection.insert(docs);

      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertTypeOf(el.value1, "boolean");
      });
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

      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 2);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name.localeCompare("concatValues"), 0);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name.localeCompare("value3"), 0);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc"});
      }
      collection.insert(docs);

      let res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(res.length, 100);
        assertEqual(el.concatValues.localeCompare(el.value1 + '+' + el.value2), 0);
        assertEqual(el.value3.localeCompare(" "), 0);
      });
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function ComputedValuesOnCreationOverrideTestSuite() {
  'use strict';

  let collection = null;
  return {
    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn, {
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
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
      assertEqual(colProperties.computedValues[0].name.localeCompare("concatValues"), 0);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({name: "test" + i});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.concatValues.localeCompare("+"), 0);
      });

      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc"});
      }
      collection.insert(docs);

      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER HAS (doc, "value1") FILTER HAS (doc, "value2")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.concatValues.localeCompare(el.value1 + '+' + el.value2), 0);
      });
    },

    testOverrideOnAllOperations: function() {
      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name.localeCompare("concatValues"), 0);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc", value3: false, concatValues: "test" + i});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertFalse(el.value3);
        assertEqual(el.concatValues.localeCompare(`${el.value1}+${el.value2}`), 0);
      });

      res = collection.insert({value4: true});
      let docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues.localeCompare("+"), 0);
      res = collection.update(docAttributes["_key"], {value1: "abc123"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues.localeCompare("+"), 0);

      res = collection.replace(docAttributes["_key"], {value1: "abc"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues.localeCompare("abc+"), 0);
    },
  };
}

function ComputedValuesOnCreationNoOverrideTestSuite() {
  'use strict';

  let collection = null;
  return {
    setUp: function() {
      internal.db._drop(cn);
      collection = internal.db._create(cn, {
        computedValues: [{
          name: "concatValues",
          expression: "RETURN CONCAT(@doc.value1, '+', @doc.value2)",
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
      assertEqual(colProperties.computedValues[0].name.localeCompare("concatValues"), 0);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({name: "test" + i});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach(el => {
        assertEqual(el.concatValues.localeCompare("+"), 0);
      });

      docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc"});
      }
      collection.insert(docs);

      res = db._query(`FOR doc IN ${cn} FILTER HAS (doc, "concatValues") FILTER HAS (doc, "value1") FILTER HAS (doc, "value2")  RETURN {value1: doc.value1, value2: doc.value2, concatValues: doc.concatValues}`).toArray();
      assertEqual(res.length, 100);
      res.forEach(el => {
        assertEqual(el.concatValues.localeCompare(el.value1 + '+' + el.value2), 0);
      });
    },

    testNoOverrideOnAllOperations: function() {
      const colProperties = collection.properties();
      assertTrue(colProperties.hasOwnProperty("computedValues"));
      assertEqual(colProperties.computedValues.length, 1);
      assertTrue(colProperties.computedValues[0].hasOwnProperty("name"));
      assertEqual(colProperties.computedValues[0].name.localeCompare("concatValues"), 0);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({value1: "test" + i, value2: "abc", value3: false, concatValues: "test" + i});
      }
      collection.insert(docs);
      let res = db._query(`FOR doc IN ${cn} RETURN doc`).toArray();
      res.forEach((el, index) => {
        assertFalse(el.value3);
        assertEqual(el.concatValues.localeCompare(`test${index}`), 0);
      });

      res = collection.insert({value4: true});
      let docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues.localeCompare("+"), 0);
      res = collection.update(docAttributes["_key"], {value1: "abc123"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues.localeCompare("+"), 0);

      res = collection.replace(docAttributes["_key"], {value1: "abc"});
      docAttributes = collection.document(res["_key"]);
      assertEqual(docAttributes.concatValues.localeCompare("+"), 0);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ComputedValuesOnCreationOverrideTestSuite);
jsunity.run(ComputedValuesOnCreationNoOverrideTestSuite);
jsunity.run(ComputedValuesTestSuiteAfterCreate);

return jsunity.done();
