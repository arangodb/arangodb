/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global fail, assertEqual, assertMatch */

////////////////////////////////////////////////////////////////////////////////
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const ERRORS = arangodb.errors;
const db = arangodb.db;

function ErrorMessagesSuite () {
  'use strict';

  const cn = "UnitTestsCollection";

  return {
    setUp : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },
    
    testSimpleUpdateNotFound : function () {
      try {
        db[cn].update("testi", {});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
        assertMatch(/document not found/, err.errorMessage);
      }
    },
    
    testSimpleReplaceNotFound : function () {
      try {
        db[cn].replace("testi", {});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
        assertMatch(/document not found/, err.errorMessage);
      }
    },

    testSimpleRemoveNotFound : function () {
      try {
        db[cn].remove("testi");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
        assertMatch(/document not found/, err.errorMessage);
      }
    },
    
    testAqlUpdateNotFound : function () {
      try {
        db._query("FOR i IN 1..5 UPDATE CONCAT('test', i) WITH {} IN " + cn);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
        assertMatch(/document not found/, err.errorMessage);
      }
    },
    
    testAqlReplaceNotFound : function () {
      try {
        db._query("FOR i IN 1..5 REPLACE CONCAT('test', i) WITH {} IN " + cn);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
        assertMatch(/document not found/, err.errorMessage);
      }
    },

    testAqlRemoveNotFound : function () {
      try {
        db._query("FOR i IN 1..5 REMOVE CONCAT('test', i) IN " + cn);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, err.errorNum);
        assertMatch(/document not found/, err.errorMessage);
      }
    },
    
  };
}

jsunity.run(ErrorMessagesSuite);

return jsunity.done();
