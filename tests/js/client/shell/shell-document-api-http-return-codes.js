/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, arango */

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
  
  return {
    setUp: function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 3 });
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testInsertDocument: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test",
        value: 1
      });
      assertEqual(202, result.code);
    },
    
    testInsertInvalidDocumentKey: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test  foo",
        value: 1
      });
      assertEqual(400, result.code);
    },
    
    testInsertConflictDocument: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test1",
        value: 1
      });
      assertEqual(202, result.code);
      
      result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test1",
        value: 2
      });
      assertEqual(409, result.code);
    },
    
    testInsertNonExistingCollection: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn + "moeter"), {
        _key: "test  foo",
        value: 1
      });
      assertEqual(404, result.code);
    },

    testUpdateDocument: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test",
        value: 1
      });
      assertEqual(202, result.code);
      
      result = arango.PATCH_RAW("/_api/document/" + encodeURIComponent(cn) + "/test", {
        _key: "test",
        value: 2
      });
      assertEqual(202, result.code);
    },
    
    testUpdateConflictDocument: function () {
      if (internal.isCluster()) {
        // we need to skip this test in cluster because we cannot create a unique index 
        // on "value" here
        return;
      }

      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test1",
        value: 1
      });
      assertEqual(202, result.code);
      
      result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test2",
        value: 2
      });
      assertEqual(202, result.code);

      db[cn].ensureIndex({ fields: ["value"], type: "persistent", unique: true });
      
      result = arango.PATCH_RAW("/_api/document/" + encodeURIComponent(cn) + "/test1", {
        value: 2
      });
      assertEqual(409, result.code);
    },
    
    testUpdateNonExistingDocument: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test1",
        value: 1
      });
      assertEqual(202, result.code);

      result = arango.PATCH_RAW("/_api/document/" + encodeURIComponent(cn + "moeter") + "/test2", {
        value: 1
      });
      assertEqual(404, result.code);
    },
    
    testUpdateNonExistingCollection: function () {
      let result = arango.PATCH_RAW("/_api/document/" + encodeURIComponent(cn + "moeter") + "/test1", {
        value: 1
      });
      assertEqual(404, result.code);
    },
    
    testReplaceDocument: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test",
        value: 1
      });
      assertEqual(202, result.code);
      
      result = arango.PUT_RAW("/_api/document/" + encodeURIComponent(cn) + "/test", {
        _key: "test",
        value: 2
      });
      assertEqual(202, result.code);
    },
    
    testReplaceConflictDocument: function () {
      if (internal.isCluster()) {
        // we need to skip this test in cluster because we cannot create a unique index 
        // on "value" here
        return;
      }

      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test1",
        value: 1
      });
      assertEqual(202, result.code);
      
      result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test2",
        value: 2
      });
      assertEqual(202, result.code);

      db[cn].ensureIndex({ fields: ["value"], type: "persistent", unique: true });
      
      result = arango.PUT_RAW("/_api/document/" + encodeURIComponent(cn) + "/test1", {
        value: 2
      });
      assertEqual(409, result.code);
    },
    
    testReplaceNonExistingDocument: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test1",
        value: 1
      });
      assertEqual(202, result.code);

      result = arango.PUT_RAW("/_api/document/" + encodeURIComponent(cn + "moeter") + "/test2", {
        value: 1
      });
      assertEqual(404, result.code);
    },
    
    testReplaceNonExistingCollection: function () {
      let result = arango.PUT_RAW("/_api/document/" + encodeURIComponent(cn + "moeter") + "/test1", {
        value: 1
      });
      assertEqual(404, result.code);
    },
    
    testRemoveDocument: function () {
      let result = arango.POST_RAW("/_api/document/" + encodeURIComponent(cn), {
        _key: "test",
        value: 1
      });
      assertEqual(202, result.code);

      result = arango.DELETE_RAW("/_api/document/" + encodeURIComponent(cn) + "/test");
      assertEqual(202, result.code);
    },
    
    testRemoveNonExistingDocument: function () {
      let result = arango.DELETE_RAW("/_api/document/" + encodeURIComponent(cn) + "/testnono");
      assertEqual(404, result.code);
    },
    
    testRemoveNonExistingCollection: function () {
      let result = arango.DELETE_RAW("/_api/document/" + encodeURIComponent(cn + "moeter") + "/test");
      assertEqual(404, result.code);
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
