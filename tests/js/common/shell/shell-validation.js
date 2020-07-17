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

var jsunity = require("jsunity");

const internal = require("internal");
const db = internal.db;
const arangodb = require("@arangodb");
const ERRORS = arangodb.errors;

// helper
const isCluster = internal.isCluster();
let sleepInCluster = () => {
  if (isCluster){
    internal.sleep(2);
  }
};

const goodDoc = { "zahlen" : [1, 2, 3, 4] };
const badDoc = { "zahlen" : "1, 2, 3, 4" };

function ValidationBasicsSuite () {
  const skipOptions = { skipDocumentValidation : true };

  const testCollectionName = "TestValidationCollection";
  var testCollection;
  var validatorJson;

  return {

    setUp : () => {
      try {
        db._drop(testCollectionName);
      } catch (ex) {}
      validatorJson = {
        level : "strict",
        rule: {
          type: "object",
          properties: {
            zahlen : {
              type : "array",
              items : { type : "number", maximum : 6 }
            },
            name : {
              type : "string",
              minLength: 4,
              maxLength: 10
            },
            number : {
              type : "number",
              items : { minimum: 1000000 }
            },
          },
          additionalProperties : false
        },
        message : "Json-Schema validation failed",
      };
      testCollection = db._create(testCollectionName, { schema :  validatorJson, numberOfShards: 3 });
    },

    tearDown : () => {
      try {
        db._drop(testCollectionName);
      } catch (ex) {}
    },

    // properties ////////////////////////////////////////////////////////////////////////////////////////
    testProperties : () => {
      const v = validatorJson;
      var props = testCollection.properties();
      assertNotUndefined(props);
      assertNotUndefined(props.schema);
      assertEqual(props.schema.rule, v.rule);
      assertEqual(props.schema.message, v.message);
      assertEqual(props.schema.level, v.level);
    },

    testPropertiesUpdate : () => {
      let v =  validatorJson;
      v.level = "none";

      testCollection.properties({"schema" : v});

      var props = testCollection.properties();
      assertEqual(props.schema.rule, v.rule);
      assertEqual(props.schema.message, v.message);
      assertEqual(props.schema.level, v.level);
      assertNotUndefined(props);
    },

    testPropertiesUpdateNoObject : () => {
      const v =  "hund";
      try {
        testCollection.properties({"schema" : v});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_BAD_PARAMETER.code, err.errorNum);
      }
    },

    // insert ////////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellInsert : () => {
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testDocumentsShellInsertSkip : () => {
      testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);
    },

    testAQLInsert : () => {
      try {
        db._query(`INSERT {"zahlen" : "1, 2, 3, 4"} INTo ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLInsertSkip : () => {
      db._query(`INSERT {"zahlen" : "1, 2, 3, 4"} INTo ${testCollectionName} OPTIONS { "skipDocumentValidation" : true }`);
      assertEqual(testCollection.toArray().length, 1);
    },

    testDocumentsShellUpdate : () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);

      try {
        testCollection.update(doc._key, badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    // update ////////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellUpdateSkip : () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);

      testCollection.update(doc._key, badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);
    },

    testAQLUpdate : () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);

      try {
        db._query(`UPDATE "${doc._key}" WITH {"zahlen" : "baz"} INTo ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLUpdateSkip : () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);

      db._query(`UPDATE "${doc._key}" WITH {"zahlen" : "baz"} INTo ${testCollectionName} OPTIONS { "skipDocumentValidation" : true }`);
      assertEqual(testCollection.toArray().length, 1);
    },

    // replace ///////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellReplaceSkip : () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);

      testCollection.replace(doc._key, badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);
    },

    testAQLReplace : () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);

      try {
        db._query(`REPLACE "${doc._key}" WITH {"zahlen" : "baz"} INTo ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLReplaceSkip : () => {
      let doc = testCollection.insert(badDoc, skipOptions);
      assertEqual(testCollection.toArray().length, 1);

      db._query(`REPLACE "${doc._key}" WITH {"zahlen" : "baz"} INTo ${testCollectionName} OPTIONS { "skipDocumentValidation" : true }`);
      assertEqual(testCollection.toArray().length, 1);
    },

    // levels ////////////////////////////////////////////////////////////////////////////////////////////
    testLevelNone : () => {
      validatorJson.level = "none";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);
      testCollection.insert(badDoc);
    },

    testLevelNew : () => {
      validatorJson.level = "new";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let  doc = testCollection.insert(badDoc, skipOptions);
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
      testCollection.replace(doc._key, badDoc);
      testCollection.update(doc._key, badDoc);
    },

    testLevelModerateInsert : () => {
      validatorJson.level = "moderate";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let  doc = testCollection.insert(badDoc, skipOptions);
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testLevelModerateModifyBadToGood : () => {
      validatorJson.level = "moderate";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let doc;

      doc = testCollection.insert(badDoc, skipOptions);
      testCollection.update(doc._key, goodDoc);

      doc = testCollection.insert(badDoc, skipOptions);
      testCollection.replace(doc._key, goodDoc);
    },

    testLevelModerateModifyBadWithBad : () => {
      validatorJson.level = "moderate";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let doc;
      let query;

      doc = testCollection.insert(badDoc, skipOptions);
      testCollection.update(doc._key, badDoc);

      doc = testCollection.insert(badDoc, skipOptions);
      testCollection.replace(doc._key, badDoc);

      doc = testCollection.insert(badDoc, skipOptions);
      query = `UPDATE "${doc._key}" WITH { "zahlen" : "sind auch nur numbers" } IN ${testCollectionName}`;
      db._query(query);

      doc = testCollection.insert(badDoc, skipOptions);
      query = `REPLACE "${doc._key}" WITH { "zahlen" : "sind auch nur numbers" } IN ${testCollectionName}`;
      db._query(query);
    },

    testLevelModerateUpdateGoodToBad : () => {
      validatorJson.level = "moderate";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let  doc = testCollection.insert(goodDoc);

      try {
        testCollection.update(doc._key, badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        let query = `UPDATE "${doc._key}" WITH { "zahlen" : "sind auch nur numbers" } IN ${testCollectionName}`;
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testLevelModerateReplaceGoodToBad : () => {
      validatorJson.level = "moderate";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let  doc = testCollection.insert(goodDoc);
      try {
        testCollection.replace(doc._key, badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        let query = `REPLACE "${doc._key}" WITH { "zahlen" : "sind auch nur numbers" } IN ${testCollectionName}`;
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testLevelStict : () => {
      validatorJson.level = "strict";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();
      assertEqual(testCollection.properties().schema.level, validatorJson.level);

      let  doc = testCollection.insert(badDoc, skipOptions);

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
        let query = `REPLACE "${doc._key}" WITH { "zahlen" : "sind auch nur numbers" } IN ${testCollectionName}`;
        db._query(query);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        let query = `UPDATE "${doc._key}" WITH { "zahlen" : "sind auch nur numbers" } IN ${testCollectionName}`;
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
      testCollection.properties({"schema" : { } });
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
      testCollection.properties({"schema" : null });
      sleepInCluster();
      assertEqual(testCollection.properties().schema, null);
      testCollection.insert(badDoc);
      assertEqual(1, testCollection.count());
    },

    // json  ////////////////////////////////////////////////////////////////////////////////////////////
    testJson: () => {
      validatorJson.level = "strict";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();

      let  doc = testCollection.insert(goodDoc, skipOptions);
      try {
        testCollection.insert(badDoc);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testJsonRequire  : () => {
      let p = {
        ...validatorJson.rule,
        required: [ "zahlen", "name" ]
      };
      validatorJson.rule = p;
      validatorJson.level = "strict";

      testCollection.properties({ "schema" : validatorJson });
      sleepInCluster();

      try {
        //name missing
        testCollection.insert({
          "zahlen" : [1,2,3,4,5],
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      try {
        //name too short
        testCollection.insert({
          "zahlen" : [1,2,3,4,5],
          "name" : "ulf"
        });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }

      {
        // good document
        testCollection.insert({
          "zahlen" : [1,2,3,4,5],
          "name" : "guterName"
        });
      }

    },
    // AQL  ////////////////////////////////////////////////////////////////////////////////////////////
    test_SCHEMA_GET: () => {
      validatorJson.level = "strict";
      testCollection.properties({"schema" : validatorJson });
      sleepInCluster();

      // get regular schema
      let res = db._query(`RETURN SCHEMA_GET("${testCollectionName}")`).toArray();
      assertEqual(res[0], validatorJson);
    },

    test_SCHEMA_GET_no_collection: () => {
      // schema on non existing collection
      try {
        db._query(`RETURN SCHEMA_GET("nonExistingTestCollection")`).toArray();
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },

    test_SCHEMA_GET_null: () => {
      // no validation available must return `null`
      testCollection.properties({schema : {}});
      let res = db._query(`
        RETURN SCHEMA_GET("${testCollectionName}")
      `).toArray();
      assertEqual(res[0], null);
    },

    test_SCHEMA_VALIDATE: () => {
      // unset schema
      testCollection.properties({schema : {}});
      sleepInCluster();

      let res;
      // doc is not an object
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          null,
          {
            "rule" : {
              "properties" : {
                "foo" : { "type" : "string" }
              }
            },
            "message" : "validation - failed"
          }
        )
      `).toArray();
      assertEqual([ null ], res);

      // doc is not an object
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          "foo",
          {
            "rule" : {
              "properties" : {
                "foo" : { "type" : "string" }
              }
            },
            "message" : "validation - failed"
          }
        )
      `).toArray();
      assertEqual([ null ], res);
      
      // doc is not an object
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          [],
          {
            "rule" : {
              "properties" : {
                "foo" : { "type" : "string" }
              }
            },
            "message" : "validation - failed"
          }
        )
      `).toArray();
      assertEqual([ null ], res);

      // doc does not match schema
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          { "foo" : 24 },
          {
            "rule" : {
              "properties" : {
                "foo" : { "type" : "string" }
              }
            },
            "message" : "validation - failed"
          }
        )
      `).toArray();
      assertEqual(res[0].valid, false);
      assertEqual(res[0].errorMessage, "validation - failed");

      // doc matches schema
      res = db._query(`
        RETURN SCHEMA_VALIDATE(
          { "foo" : "bar" },
          {
            "rule" : {
              "properties" : {
                "foo" : { "type" : "string" }
              }
            }
          }
      )`).toArray();
      assertEqual(res[0].valid, true);

      // no schema
      res = db._query(
        `RETURN SCHEMA_VALIDATE(
          { "foo" : "bar" },
          null
      )`).toArray();
      assertEqual(res[0].valid, true);
      
      // empty schema object
      res = db._query(
        `RETURN SCHEMA_VALIDATE(
          { "foo" : "bar" },
          {}
      )`).toArray();
      assertEqual(res[0].valid, true);

      // invalid schema
      try {
        db._query(
          `RETURN SCHEMA_VALIDATE(
             {"foo" : "bar"},
             [])
        `).toArray();
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
  }; // return
} // END - ValidationBasicsSuite

jsunity.run(ValidationBasicsSuite);

return jsunity.done();
