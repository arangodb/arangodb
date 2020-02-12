/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTypeOf, assertNotEqual, assertTrue, assertFalse, assertUndefined, fail */

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

//var testHelper = require("@arangodb/test-helper");

//var ArangoCollection = arangodb.ArangoCollection;

const internal = require("internal");
const db = internal.db;
const print = internal.print;
const arangodb = require("@arangodb");
const ERRORS = arangodb.errors;


function ValidationBasicsSuite () {
  const validatorsStart=[
    {
        type : "bool",
        rule : true,
        message : "Hit tautology rule.",
        //description : "This rule is always successful"
    },
    {
        type : "bool",
        rule : false,
        message : "Hit always-fail rule.",
        //description : "This rule always fails"
    },
    {
        type : "aql",
        level : "new",
        rule : "RETURN TRUE",
        message : "Another tautology rule!",
    }
  ];


  const skipOptions = { skipDocumentValidation : true };

  const testCollectionName = "TestValidationCollection";
  var testCollection;

  return {

    setUp : () => {
      try {
        db._drop(testCollectionName);
      } catch (ex) {}
      testCollection = db._create(testCollectionName, { "validators" :  validatorsStart });
    },

    tearDown : () => {
      try {
        db._drop(testCollectionName);
      } catch (ex) {}
    },

    // properties ////////////////////////////////////////////////////////////////////////////////////////
    testProperties : () => {
      const v = validatorsStart;
      var props = testCollection.properties();
      assertTrue(props !== undefined);
      assertEqual(props.validators.length, 3);
      assertEqual(props.validators[0].type, v[0].type);
      assertEqual(props.validators[0].rule, v[0].rule);
      assertEqual(props.validators[0].message, v[0].message);
      assertEqual(props.validators[0].level, "strict");

      assertEqual(props.validators[2].type, v[2].type);
      assertEqual(props.validators[2].level, v[2].level);
    },

    // insert ////////////////////////////////////////////////////////////////////////////////////////////
    testDocumentsShellInsert : () => {
      try {
        testCollection.insert({"foo" : "bar"});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testDocumentsShellInsertSkip : () => {
      testCollection.insert({"foo" : "bar"}, skipOptions);
      assertEqual(testCollection.toArray().length, 1);
    },

    testAQLInsert : () => {
      try {
        db._query(`INSERT {"foo" : "bar"} INTO ${testCollectionName}`);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },

    testAQLInsertSkip : () => {
      db._query(`INSERT {"foo" : "bar"} INTO ${testCollectionName} OPTIONS { "skipDocumentValidation" : true }`);
      assertEqual(testCollection.toArray().length, 1);
    },

    testDocumentsShellUpdate : () => {
      let doc = testCollection.insert({"foo" : "bar"}, skipOptions);
      assertEqual(testCollection.toArray().length, 1);

      try {
        testCollection.update(doc._key, {"foo" : "baz"});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_VALIDATION_FAILED.code, err.errorNum);
      }
    },


////////////////////////////////////////////////////////////////////////////////
  }; // return
} // END - ValidationBasicsSuitr

jsunity.run(ValidationBasicsSuite);

return jsunity.done();
