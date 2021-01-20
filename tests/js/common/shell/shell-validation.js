/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTypeOf, assertNotEqual, assertTrue, assertFalse, assertUndefined, assertNotUndefined, fail */

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
let sleepInCluster = () => {
  if (isCluster) {
    internal.sleep(2);
  }
};

const skipOptions = { "skipDocumentValidation": true };

function ValidationBasicsSuite () {
  const testCollectionName = "TestValidationCollection";
  let testCollection;
  let validatorJson;

  // Same attribute key, so that updating badDoc with goodDoc will succeed
  const goodDoc = { "numArray": [1, 2, 3, 4] };
  const badDoc = { "numArray": "1, 2, 3, 4" };

  return {

    setUp: () => {
      try {
        db._drop(testCollectionName);
      } catch (ex) {}
      validatorJson = {
        "level": "strict",
        "rule": {
          "type": "object",
          "properties": {
            "numArray": {
              "type": "array",
              "items": {
                "type": "number",
                "maximum": 6
              }
            },
            "name": {
              "type": "string",
              "minLength": 4,
              "maxLength": 10
            },
            "number": {
              "type": "number",
              "items": {
                "minimum": 1000000
              }
            }
          },
          "additionalProperties": false
        },
        "message": "Schema validation failed"
      };
      testCollection = db._create(testCollectionName, {
        schema: validatorJson,
        numberOfShards: 3
      });
    },

    tearDown: () => {
      try {
        db._drop(testCollectionName);
      } catch (ex) {}
    },

    // properties ////////////////////////////////////////////////////////////////////////////////////////
    testProperties: () => {
      const v = validatorJson;
      const props = testCollection.properties();
      assertNotUndefined(props);
      assertNotUndefined(props.schema);
      assertEqual(props.schema.rule, v.rule);
      assertEqual(props.schema.message, v.message);
      assertEqual(props.schema.level, v.level);
    },

    testPropertiesUpdate: () => {
      let v = validatorJson;
      v.level = "none";

      testCollection.properties({ "schema": v });

      const props = testCollection.properties();
      assertEqual(props.schema.rule, v.rule);
      assertEqual(props.schema.message, v.message);
      assertEqual(props.schema.level, v.level);
      assertNotUndefined(props);
    },

    testPropertiesUpdateNoObject: () => {
      const v = "hund";
      try {
        testCollection.properties({ "schema": v });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_BAD_PARAMETER.code, err.errorNum);
      }
    },

    // insert ////////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellInsertGood: () => {
      testCollection.insert(goodDoc);
      assertEqual(testCollection.count(), 1);
      let doc = testCollection.any();
      assertTrue(Array.isArray(doc.numArray));
    },

    testDocumentsShellInsertBad: () => {
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testDocumentsShellInsertBadSkip: () => {
      testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.count(), 1);
      let doc = testCollection.any();
      assertFalse(Array.isArray(doc.numArray));
    },

    testAQLInsertGood: () => {
      db._query(`INSERT { "numArray": [1, 2, 3, 4] } INTO ${testCollectionName}`);
      assertEqual(testCollection.count(), 1);
      let doc = testCollection.any();
      assertTrue(Array.isArray(doc.numArray));
    },

    testAQLInsertBad: () => {
      try {
        db._query(`INSERT { "numArray": "1, 2, 3, 4" } INTO ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLInsertBadSkip: () => {
      db._query(`INSERT { "numArray": "1, 2, 3, 4" } INTO ${testCollectionName} OPTIONS { "skipDocumentValidation": true }`);
      assertEqual(testCollection.count(), 1);
      let doc = testCollection.any();
      assertFalse(Array.isArray(doc.numArray));
    },

    // update ////////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellUpdateGood: () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.count(), 1);

      testCollection.update(doc._key, goodDoc);
      doc = testCollection.any();
      assertTrue(Array.isArray(doc.numArray));
    },

    testDocumentsShellUpdateBad: () => {
      let doc = testCollection.insert(goodDoc);
      assertEqual(testCollection.count(), 1);

      try {
        testCollection.update(doc._key, badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testDocumentsShellUpdateBadSkip: () => {
      let doc = testCollection.insert(goodDoc);
      assertEqual(testCollection.count(), 1);

      testCollection.update(doc._key, badDoc, skipOptions);
      doc = testCollection.any();
      assertFalse(Array.isArray(doc.numArray));
    },

    testAQLUpdateGood: () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.count(), 1);

      db._query(`UPDATE "${doc._key}" WITH { "numArray": [1, 2, 3, 4] } IN ${testCollectionName}`);
      doc = testCollection.any();
      assertTrue(Array.isArray(doc.numArray));
    },

    testAQLUpdateBad: () => {
      let doc = testCollection.insert(goodDoc);
      assertEqual(testCollection.count(), 1);

      try {
        db._query(`UPDATE "${doc._key}" WITH { "numArray": "1, 2, 3, 4" } IN ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLUpdateBadSkip: () => {
      let doc = testCollection.insert(goodDoc);
      assertEqual(testCollection.count(), 1);

      db._query(`UPDATE "${doc._key}" WITH { "numArray": "1, 2, 3, 4" } IN ${testCollectionName} OPTIONS { "skipDocumentValidation": true }`);
      doc = testCollection.any();
      assertFalse(Array.isArray(doc.numArray));
    },

    // replace ///////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellReplaceGood: () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.count(), 1);

      testCollection.replace(doc._key, goodDoc);
      doc = testCollection.any();
      assertTrue(Array.isArray(doc.numArray));
    },

    testDocumentsShellReplaceBad: () => {
      let doc = testCollection.insert(goodDoc);
      assertEqual(testCollection.count(), 1);

      try {
        testCollection.replace(doc._key, badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testDocumentsShellReplaceBadSkip: () => {
      let doc = testCollection.insert(goodDoc);
      assertEqual(testCollection.count(), 1);

      testCollection.replace(doc._key, badDoc, skipOptions);
      doc = testCollection.any();
      assertFalse(Array.isArray(doc.numArray));
    },

    testAQLReplaceGood: () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.count(), 1);

      db._query(`REPLACE "${doc._key}" WITH { "numArray": [1, 2, 3, 4] } IN ${testCollectionName}`);
      doc = testCollection.any();
      assertTrue(Array.isArray(doc.numArray));
    },

    testAQLReplaceBad: () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.count(), 1);

      try {
        db._query(`REPLACE "${doc._key}" WITH { "numArray": "1, 2, 3, 4" } IN ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLReplaceBadSkip: () => {
      let doc = testCollection.insert(goodDoc, skipOptions);
      assertEqual(testCollection.count(), 1);

      db._query(`REPLACE "${doc._key}" WITH { "numArray": "1, 2, 3, 4" } IN ${testCollectionName} OPTIONS { "skipDocumentValidation": true }`);
      doc = testCollection.any();
      assertFalse(Array.isArray(doc.numArray));
    },

    // levels ////////////////////////////////////////////////////////////////////////////////////////////
    testLevelNone: () => {
      validatorJson.level = "none";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);
      testCollection.insert(badDoc);
    },

    testLevelNew: () => {
      validatorJson.level = "new";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let doc = testCollection.insert(badDoc, skipOptions);
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
      testCollection.replace(doc._key, badDoc);
      testCollection.update(doc._key, badDoc);
    },

    testLevelModerateInsert: () => {
      validatorJson.level = "moderate";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      testCollection.insert(badDoc, skipOptions);
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testLevelModerateModifyBadToGood: () => {
      validatorJson.level = "moderate";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let doc;

      doc = testCollection.insert(badDoc, skipOptions);
      testCollection.update(doc._key, goodDoc);

      doc = testCollection.insert(badDoc, skipOptions);
      testCollection.replace(doc._key, goodDoc);
    },

    testLevelModerateModifyBadWithBad: () => {
      validatorJson.level = "moderate";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let doc;
      let query;

      doc = testCollection.insert(badDoc, skipOptions);
      testCollection.update(doc._key, badDoc);

      doc = testCollection.insert(badDoc, skipOptions);
      testCollection.replace(doc._key, badDoc);

      doc = testCollection.insert(badDoc, skipOptions);
      query = `UPDATE "${doc._key}" WITH { "numArray": "numbers are just digits" } IN ${testCollectionName}`;
      db._query(query);

      doc = testCollection.insert(badDoc, skipOptions);
      query = `REPLACE "${doc._key}" WITH { "numArray": "numbers are just digits" } IN ${testCollectionName}`;
      db._query(query);
    },

    testLevelModerateUpdateGoodToBad: () => {
      validatorJson.level = "moderate";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let doc = testCollection.insert(goodDoc);

      try {
        testCollection.update(doc._key, badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        let query = `UPDATE "${doc._key}" WITH { "numArray": "numbers are just digits" } IN ${testCollectionName}`;
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testLevelModerateReplaceGoodToBad: () => {
      validatorJson.level = "moderate";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let doc = testCollection.insert(goodDoc);
      try {
        testCollection.replace(doc._key, badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        let query = `REPLACE "${doc._key}" WITH { "numArray": "numbers are just digits" } IN ${testCollectionName}`;
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testLevelStict: () => {
      validatorJson.level = "strict";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let doc = testCollection.insert(badDoc, skipOptions);

      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        testCollection.replace(doc._key, badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        testCollection.update(doc._key, badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        let query = `REPLACE "${doc._key}" WITH { "numArray": "numbers are just digits" } IN ${testCollectionName}`;
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        let query = `UPDATE "${doc._key}" WITH { "numArray": "numbers are just digits" } IN ${testCollectionName}`;
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testRemoveValidation: () => {
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
      testCollection.properties({ "schema": { } });
      sleepInCluster();
      assertEqual(testCollection.properties().schema, null);
      testCollection.insert(badDoc);
      assertEqual(1, testCollection.count());
    },

    testRemoveValidationWithNull: () => {
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
      testCollection.properties({ "schema": null });
      sleepInCluster();
      assertEqual(testCollection.properties().schema, null);
      testCollection.insert(badDoc);
      assertEqual(1, testCollection.count());
    },

    // json ////////////////////////////////////////////////////////////////////////////////////////////
    testJson: () => {
      validatorJson.level = "strict";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();

      testCollection.insert(goodDoc);
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testJsonRequire: () => {
      let p = {
        ...validatorJson.rule,
        required: [ "numArray", "name" ]
      };
      validatorJson.rule = p;
      validatorJson.level = "strict";

      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();

      try {
        // name missing
        testCollection.insert({
          "numArray": [1, 2, 3, 4]
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        // name too short
        testCollection.insert({
          "numArray": [1, 2, 3, 4],
          "name": "Ulf"
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      {
        // good document
        testCollection.insert({
          "numArray": [1, 2, 3, 4],
          "name": "good name"
        });
      }

    },
    // AQL ////////////////////////////////////////////////////////////////////////////////////////////
    testAQLSchemaGet: () => {
      validatorJson.level = "strict";
      testCollection.properties({ "schema": validatorJson });
      sleepInCluster();

      // get regular schema
      let res = db._query(`RETURN SCHEMA_GET("${testCollectionName}")`).toArray();
      assertEqual(res[0], validatorJson);
    },

    testAQLSchemaGetNoCollection: () => {
      // schema on non existing collection
      try {
        db._query(`RETURN SCHEMA_GET("nonExistingTestCollection")`).toArray();
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },

    testAQLSchemaGetNull: () => {
      // no validation available must return `null`
      testCollection.properties({ schema: {} });
      let res = db._query(`
        RETURN SCHEMA_GET("${testCollectionName}")
      `).toArray();
      assertEqual(res[0], null);
    },

    testAqlSchemaValidate: () => {
      // unset schema
      testCollection.properties({ schema: {} });
      sleepInCluster();

      let res;
      // doc is not an object
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          null,
          {
            "rule": {
              "properties": {
                "foo": { "type": "string" }
              }
            },
            "message": "Schema validation failed"
          }
        )
      `).toArray();
      assertEqual([ null ], res);

      // doc is not an object
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          "foo",
          {
            "rule": {
              "properties": {
                "foo": { "type": "string" }
              }
            },
            "message": "Schema validation failed"
          }
        )
      `).toArray();
      assertEqual([ null ], res);

      // doc is not an object
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          [],
          {
            "rule": {
              "properties": {
                "foo": { "type": "string" }
              }
            },
            "message": "Schema validation failed"
          }
        )
      `).toArray();
      assertEqual([ null ], res);

      // doc does not match schema
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          { "foo": 24 },
          {
            "rule": {
              "properties": {
                "foo": { "type": "string" }
              }
            },
            "message": "Schema validation failed"
          }
        )
      `).toArray();
      assertEqual(res[0].valid, false);
      assertEqual(res[0].errorMessage, "Schema validation failed");

      // doc matches schema
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          { "foo": "bar" },
          {
            "rule": {
              "properties": {
                "foo": { "type": "string" }
              }
            }
          }
      )`).toArray();
      assertEqual(res[0].valid, true);

      // no schema
      res = db._query(
        `RETURN SCHEMA_VALIDATE(
          { "foo": "bar" },
          null
      )`).toArray();
      assertEqual(res[0].valid, true);

      // empty schema object
      res = db._query(
        `RETURN SCHEMA_VALIDATE(
          { "foo": "bar" },
          {}
      )`).toArray();
      assertEqual(res[0].valid, true);

      // invalid schema
      try {
        db._query(
          `RETURN SCHEMA_VALIDATE(
             {"foo": "bar"},
             [])
        `).toArray();
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    }

////////////////////////////////////////////////////////////////////////////////
  }; // return
} // END - ValidationBasicsSuite

function ValidationEdgeSuite () {
  const testCollectionName = "TestValidationEdgeCollection";
  let testCollection;
  let validatorJson;

  const blankEdge = {
    "_from": "vert/A",
    "_to": "vert/B"
  };
  const goodEdge = {
    ...blankEdge,
    "name": "Helge"
  };
  const badEdge = {
    ...blankEdge,
    "additional": true
  };

  return {

    setUp: () => {
      try {
        db._drop(testCollectionName);
      } catch (ex) {}
      validatorJson = {
        "level": "strict",
        "rule": {
          "type": "object",
          "properties": {
            "name": {
              "type": "string"
            }
          },
          "additionalProperties": false
        },
        "message": "Schema validation failed"
      };
      testCollection = db._createEdgeCollection(testCollectionName, {
        schema: validatorJson,
        numberOfShards: 3
      });
    },

    tearDown: () => {
      try {
        db._drop(testCollectionName);
      } catch (ex) {}
    },

    // insert ////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellInsertEdgeGood: () => {
      testCollection.insert(goodEdge);
      assertEqual(testCollection.count(), 1);
    },

    testDocumentsShellInsertEdgeBad: () => {
      try {
        testCollection.insert(badEdge);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testDocumentsShellInsertEdgeBadSkip: () => {
      testCollection.insert(badEdge, skipOptions);
      assertEqual(testCollection.count(), 1);
    },

    testAQLInsertEdgeGood: () => {
      db._query(`INSERT { "_from": "vert/A", "_to": "vert/B", "name": "Helge" } INTO ${testCollectionName}`);
      assertEqual(testCollection.count(), 1);
    },

    testAQLInsertEdgeBad: () => {
      try {
        db._query(`INSERT { "_from": "vert/A", "_to": "vert/B", "additional": true } INTO ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLInsertEdgeBadSkip: () => {
      db._query(`INSERT { "_from": "vert/A", "_to": "vert/B", "additional": true } INTO ${testCollectionName} OPTIONS { "skipDocumentValidation": true }`);
      assertEqual(testCollection.count(), 1);
    },

    // update ////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellUpdateEdgeGood: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      testCollection.update(doc._key, goodEdge);
      doc = testCollection.any();
      assertNotUndefined(doc.name);
    },

    testDocumentsShellUpdateEdgeBad: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      try {
        testCollection.update(doc._key, badEdge);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testDocumentsShellUpdateEdgeBadSkip: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      testCollection.update(doc._key, badEdge, skipOptions);
      doc = testCollection.any();
      assertNotUndefined(doc.additional);
    },

    testAQLUpdateEdgeGood: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      db._query(`UPDATE { "_key": "${doc._key}", "name": "Helge" } IN ${testCollectionName}`);
      doc = testCollection.any();
      assertNotUndefined(doc.name);
    },

    testAQLUpdateEdgeBad: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      try {
        db._query(`UPDATE { "_key": "${doc._key}", "additional": true } IN ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLUpdateEdgeBadSkip: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      db._query(`UPDATE { "_key": "${doc._key}", "additional": true } IN ${testCollectionName} OPTIONS { "skipDocumentValidation": true }`);
      doc = testCollection.any();
      assertNotUndefined(doc.additional);
    },
    // replace ////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellReplaceEdgeGood: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      testCollection.replace(doc._key, goodEdge);
      doc = testCollection.any();
      assertNotUndefined(doc.name);
    },

    testDocumentsShellReplaceEdgeBad: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      try {
        testCollection.replace(doc._key, badEdge);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testDocumentsShellReplaceEdgeBadSkip: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      testCollection.replace(doc._key, badEdge, skipOptions);
      doc = testCollection.any();
      assertNotUndefined(doc.additional);
    },

    testAQLReplaceEdgeGood: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      db._query(`REPLACE { "_key": "${doc._key}", "_from": "vert/A", "_to": "vert/B", "name": "Helge" } IN ${testCollectionName}`);
      doc = testCollection.any();
      assertNotUndefined(doc.name);
    },

    testAQLReplaceEdgeBad: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      try {
        db._query(`REPLACE { "_key": "${doc._key}", "_from": "vert/A", "_to": "vert/B", "additional": true } IN ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLReplaceEdgeBadSkip: () => {
      let doc = testCollection.insert(blankEdge);
      assertEqual(testCollection.count(), 1);

      db._query(`REPLACE { "_key": "${doc._key}", "_from": "vert/A", "_to": "vert/B", "additional": true } IN ${testCollectionName} OPTIONS { "skipDocumentValidation": true }`);
      doc = testCollection.any();
      assertNotUndefined(doc.additional);
    }
////////////////////////////////////////////////////////////////////////////////
  }; // return
} // END - ValidationEdgeSuite

jsunity.run(ValidationBasicsSuite);
jsunity.run(ValidationEdgeSuite);

return jsunity.done();
