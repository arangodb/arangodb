/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertMatch, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, modification blocks
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const db = require("@arangodb").db;
const jsunity = require("jsunity");
const errors = internal.errors;

function aqlErrorMessagesSuite () {
  const collectionName = "UnitTestAqlModify";

  return {
    setUp : function() { 
      db._drop(collectionName);
    },

    tearDown : function() {
      db._drop(collectionName);
    },

    testErrorMessageOnInsertUniqueConstraintViolated : function () {
      let col = db._create(collectionName, {numberOfShards: 3});
      
      col.insert({ _key: "paff" });
      
      let test = function(q) {
        try {
          db._query(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, q);
          assertMatch(/conflicting key/, err.message, q);
          assertMatch(/_key/, err.message, q);
          assertMatch(/paff/, err.message, q);
        }
      };

      test(`INSERT {"_key" : "paff"} INTO ${collectionName}`);
      test(`INSERT {"_key" : "paff"} INTO ${collectionName} RETURN NEW`);
      test(`FOR i IN 1..2 INSERT {"_key" : "paff"} INTO ${collectionName}`);
      test(`FOR i IN 1..2 INSERT {"_key" : "paff"} INTO ${collectionName} RETURN NEW`);
    },
    
    testErrorMessageOnUpdateUniqueConstraintViolated : function () {
      let col = db._create(collectionName, {numberOfShards: 1});
      col.ensureIndex({ type: "hash", fields: ["val"], unique: true });
      
      col.insert({ val: 1 });
      let key = col.insert({ val: 99999 })._key;

      let test = function(q) {
        try {
          db._query(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, q);
          assertMatch(/conflicting key/, err.message, q);
          assertMatch(/'val'/, err.message, q);
        }
      };
      
      test(`UPDATE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName}`);
      test(`UPDATE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName} RETURN OLD`);
      test(`UPDATE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName} RETURN NEW`);
      test(`FOR i IN 1..2 UPDATE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName}`);
      test(`FOR i IN 1..2 UPDATE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName} RETURN OLD`);
      test(`FOR i IN 1..2 UPDATE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName} RETURN NEW`);
    },
    
    testErrorMessageOnReplaceUniqueConstraintViolated : function () {
      let col = db._create(collectionName, {numberOfShards: 1});
      col.ensureIndex({ type: "hash", fields: ["val"], unique: true });
      
      col.insert({ val: 1 });
      let key = col.insert({ val: 99999 })._key;

      let test = function(q) {
        try {
          db._query(q);
          fail();
        } catch (err) {
          assertEqual(err.errorNum, errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, q);
          assertMatch(/conflicting key/, err.message, q);
          assertMatch(/'val'/, err.message, q);
        }
      };
      
      test(`REPLACE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName}`);
      test(`REPLACE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName} RETURN OLD`);
      test(`REPLACE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName} RETURN NEW`);
      test(`FOR i IN 1..2 REPLACE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName}`);
      test(`FOR i IN 1..2 REPLACE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName} RETURN OLD`);
      test(`FOR i IN 1..2 REPLACE {"_key" : "${key}"} WITH { val: 1 } INTO ${collectionName} RETURN NEW`);
    },
    
  };
};

jsunity.run(aqlErrorMessagesSuite);
return jsunity.done();
