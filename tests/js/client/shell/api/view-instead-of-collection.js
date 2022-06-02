/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, arango, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
const db = arangodb.db;
const internal = require("internal");

function testSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  const vn = "UnitTestsView";
  
  return {
    setUpAll: function () {
      let c = db._create(cn);
      c.insert({ _key: "test" });
      db._createView(vn, "arangosearch", {});
    },

    tearDownAll: function () {
      db._drop(cn);
      db._dropView(vn);
    },
    
    testGetDocument: function () {
      let result = arango.GET_RAW("/_api/document/" + encodeURIComponent(cn) + "/test");
      assertEqual(200, result.code);
      
      result = arango.GET_RAW("/_api/document/" + encodeURIComponent(vn) + "/test");
      assertEqual(404, result.code);
      assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testInsertDocument: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {});
      assertEqual(202, result.code);
      
      result = arango.POST_RAW("/_api/document/" + encodeURIComponent(vn), {});
      assertEqual(404, result.code);
      assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testUpdateDocument: function () {
      let result = arango.PATCH_RAW("/_api/document/" + encodeURIComponent(cn) + "/test", {});
      assertEqual(202, result.code);
      
      result = arango.PATCH_RAW("/_api/document/" + encodeURIComponent(vn) + "/test", {});
      assertEqual(404, result.code);
      assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testReplaceDocument: function () {
      let result = arango.PUT_RAW("/_api/document/" + encodeURIComponent(cn) + "/test", {});
      assertEqual(202, result.code);
      
      result = arango.PUT_RAW("/_api/document/" + encodeURIComponent(vn) + "/test", {});
      assertEqual(404, result.code);
      assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testRemoveDocument: function () {
      let key = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {}).parsedBody._key;
      let result = arango.DELETE_RAW("/_api/document/" + encodeURIComponent(cn) + "/" + encodeURIComponent(key));
      assertEqual(202, result.code);
      
      key = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {}).parsedBody._key;
      result = arango.DELETE_RAW("/_api/document/" + encodeURIComponent(vn) + "/" + encodeURIComponent(key));
      assertEqual(404, result.code);
      assertEqual(internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testAQLInsertDocument: function () {
      try {
        db._query(`INSERT {} INTO ${vn}`);
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_COLLECTION_TYPE_MISMATCH.code, err.errorNum);
      }
    },

    testAQLUpdateDocument: function () {
      try {
        db._query(`UPDATE 'test' WITH {} IN ${vn}`);
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_COLLECTION_TYPE_MISMATCH.code, err.errorNum);
      }
    },
    
    testAQLReplaceDocument: function () {
      try {
        db._query(`REPLACE 'test' WITH {} IN ${vn}`);
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_COLLECTION_TYPE_MISMATCH.code, err.errorNum);
      }
    },
    
    testAQLRemoveDocument: function () {
      try {
        db._query(`REMOVE 'test' IN ${vn}`);
        fail();
      } catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_COLLECTION_TYPE_MISMATCH.code, err.errorNum);
      }
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
