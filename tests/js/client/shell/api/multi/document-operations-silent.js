/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertEqual, assertTrue, assertUndefined, arango */

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
const errors = internal.errors;
const isCluster = internal.isCluster();
const isEnterprise = internal.isEnterprise();

function SilentTestSuite () {
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
    
    testInsertSingleDocument: function () {
      const doc = { _key: "test" };
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", doc);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);

      result = arango.GET_RAW("/_api/document/" + cn + "/test");
      assertEqual("test", result.parsedBody._key);
    },
    
    testInsertSingleDocumentWithFailure: function () {
      const doc = { _key: 123 };
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", doc);
      assertEqual(400, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, result.parsedBody.errorNum);
    },
    
    testInsertMultipleDocuments: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i });
      }
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
    },
    
    testInsertMultipleDocumentsWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ _key: "test" + i });
      }
      docs.push({ _key: 123 });
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, doc.errorNum);
    },
    
    testInsertMultipleDocumentsWithAllFailures: function () {
      let docs = [];
      for (let i = 0; i < 50; ++i) {
        docs.push({ _key: 123 });
      }
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code] : 50 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(50, result.parsedBody.length);
      for (let i = 0; i < 50; ++i) {
        let doc = result.parsedBody[i];
        assertTrue(doc.error);
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, doc.errorNum);
      }
    },
    
    testRemoveSingleDocument: function () {
      db[cn].insert({ _key: "test" });
      assertEqual(1, db[cn].count());

      let result = arango.DELETE_RAW("/_api/document/" + cn + "/test?silent=true");
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      assertEqual(0, db[cn].count());
    },
    
    testRemoveSingleDocumentWithFailure: function () {
      let result = arango.DELETE_RAW("/_api/document/" + cn + "/notthere?silent=true");
      assertEqual(404, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testRemoveMultipleDocuments: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i });
      }
      db[cn].insert(docs);
      assertEqual(100, db[cn].count());

      let result = arango.DELETE_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);

      assertEqual(0, db[cn].count());
    },
    
    testRemoveMultipleDocumentsWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ _key: "test" + i });
      }
      db[cn].insert(docs);
      assertEqual(99, db[cn].count());

      docs.push({ _key: "doesnotexist" });

      let result = arango.DELETE_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
      assertEqual(0, db[cn].count());
    },
    
    testUpdateSingleDocument: function () {
      db[cn].insert({ _key: "test" });
      assertEqual(1, db[cn].count());

      let result = arango.PATCH_RAW("/_api/document/" + cn + "/test?silent=true", { value: "foo" });
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      assertEqual(1, db[cn].count());
      
      result = arango.GET_RAW("/_api/document/" + cn + "/test");
      assertEqual("test", result.parsedBody._key);
      assertEqual("foo", result.parsedBody.value);
    },
    
    testUpdateSingleDocumentWithFailure: function () {
      let result = arango.PATCH_RAW("/_api/document/" + cn + "/notthere?silent=true", { value: "foo" });
      assertEqual(404, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testUpdateMultipleDocuments: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i });
      }
      db[cn].insert(docs);
      assertEqual(100, db[cn].count());
      
      docs.forEach((doc) => { doc.value = "foo"; });

      let result = arango.PATCH_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);

      let keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + cn + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc) => {
        assertEqual("foo", doc.value);
      });
    },
    
    testUpdateMultipleDocumentsWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ _key: "test" + i });
      }
      db[cn].insert(docs);
      assertEqual(99, db[cn].count());

      docs.forEach((doc) => { doc.value = "foo"; });
      docs.push({ _key: "doesnotexist" });

      let result = arango.PATCH_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
      
      let keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + cn + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc, i) => {
        if (i === 99) {
          assertTrue(doc.error);
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
        } else {
          assertUndefined(doc.error);
          assertUndefined(doc.errorNum);
          assertEqual("foo", doc.value);
        }
      });
    },
    
    testReplaceSingleDocument: function () {
      db[cn].insert({ _key: "test" });
      assertEqual(1, db[cn].count());

      let result = arango.PUT_RAW("/_api/document/" + cn + "/test?silent=true", { value: "foo" });
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      assertEqual(1, db[cn].count());
      
      result = arango.GET_RAW("/_api/document/" + cn + "/test");
      assertEqual("test", result.parsedBody._key);
      assertEqual("foo", result.parsedBody.value);
    },
    
    testReplaceSingleDocumentWithFailure: function () {
      db[cn].insert({ _key: "test" });
      assertEqual(1, db[cn].count());

      let result = arango.PUT_RAW("/_api/document/" + cn + "/notthere?silent=true", { value: "foo" });
      assertEqual(404, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testReplaceMultipleDocuments: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _key: "test" + i });
      }
      db[cn].insert(docs);
      assertEqual(100, db[cn].count());
      
      docs.forEach((doc) => { doc.value = "foo"; });

      let result = arango.PUT_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);

      let keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + cn + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc) => {
        assertEqual("foo", doc.value);
      });
    },
    
    testReplaceMultipleDocumentsWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ _key: "test" + i });
      }
      db[cn].insert(docs);
      assertEqual(99, db[cn].count());

      docs.forEach((doc) => { doc.value = "foo"; });
      docs.push({ _key: "doesnotexist" });

      let result = arango.PUT_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
      
      let keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + cn + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc, i) => {
        if (i === 99) {
          assertTrue(doc.error);
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
        } else {
          assertUndefined(doc.error);
          assertUndefined(doc.errorNum);
          assertEqual("foo", doc.value);
        }
      });
    },
    
  };
}

function SilentTestCustomShardingSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  
  return {
    setUp: function () {
      db._drop(cn);
      db._create(cn, { numberOfShards: 3, shardKeys: ["sk"] });
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testCustomShardingInsertSingleDocument: function () {
      const doc = { sk: "test" };
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", doc);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);

      assertEqual(1, db[cn].count());
    },
    
    testCustomShardingInsertSingleDocumentWithFailure: function () {
      const doc = { _key: "123", sk: "test" };
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", doc);
      assertEqual(400, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, result.parsedBody.errorNum);
    },
    
    testCustomShardingInsertMultipleDocuments: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ sk: "test" + i });
      }
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
    },
    
    testCustomShardingInsertMultipleDocumentsWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ sk: "test" + i });
      }
      docs.push({ _key: 123, sk: "test0" });
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, doc.errorNum);
    },
    
    testCustomShardingInsertMultipleDocumentsWithAllFailures: function () {
      let docs = [];
      for (let i = 0; i < 50; ++i) {
        docs.push({ sk: "test" + i, _key: 123 });
      }
      let result = arango.POST_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code] : 50 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(50, result.parsedBody.length);
      for (let i = 0; i < 50; ++i) {
        let doc = result.parsedBody[i];
        assertTrue(doc.error);
        assertEqual(errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, doc.errorNum);
      }
    },
    
    testCustomShardingRemoveSingleDocument: function () {
      let key = db[cn].insert({ sk: "test" })._key;
      assertEqual(1, db[cn].count());

      let result = arango.DELETE_RAW("/_api/document/" + cn + "/" + encodeURIComponent(key) + "?silent=true");
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      assertEqual(0, db[cn].count());
    },
    
    testCustomShardingRemoveSingleDocumentWithFailure: function () {
      let result = arango.DELETE_RAW("/_api/document/" + cn + "/notthere?silent=true");
      assertEqual(404, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testCustomShardingRemoveMultipleDocuments: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ sk: "test" + i });
      }
      let keys = db[cn].insert(docs).map((doc) => doc._key);
      assertEqual(100, db[cn].count());

      let result = arango.DELETE_RAW("/_api/document/" + cn + "?silent=true", keys);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);

      assertEqual(0, db[cn].count());
    },
    
    testCustomShardingRemoveMultipleDocumentsWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ sk: "test" + i });
      }
      let keys = db[cn].insert(docs).map((doc) => doc._key);
      assertEqual(99, db[cn].count());

      keys.push({ _key: "doesnotexist" });

      let result = arango.DELETE_RAW("/_api/document/" + cn + "?silent=true", keys);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
      assertEqual(0, db[cn].count());
    },

    testCustomShardingUpdateSingleDocument: function () {
      let key = db[cn].insert({ sk: "test" })._key;
      assertEqual(1, db[cn].count());

      let result = arango.PATCH_RAW("/_api/document/" + cn + "/" + encodeURIComponent(key) + "?silent=true", { value: "foo" });
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      assertEqual(1, db[cn].count());
      
      result = arango.GET_RAW("/_api/document/" + cn + "/" + encodeURIComponent(key));
      assertEqual(key, result.parsedBody._key);
      assertEqual("foo", result.parsedBody.value);
    },
    
    testCustomShardingUpdateSingleDocumentWithFailure: function () {
      let result = arango.PATCH_RAW("/_api/document/" + cn + "/notthere?silent=true", { value: "foo" });
      assertEqual(404, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testCustomShardingUpdateMultipleDocuments: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ sk: "test" + i });
      }
      let keys = db[cn].insert(docs).map((doc) => doc._key);
      assertEqual(100, db[cn].count());
      
      docs.forEach((doc, i) => { doc.value = "foo"; doc._key = keys[i]; });

      let result = arango.PATCH_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      
      keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + cn + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc) => {
        assertEqual("foo", doc.value);
      });
    },
    
    testCustomShardingUpdateMultipleDocumentsWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ sk: "test" + i });
      }
      let keys = db[cn].insert(docs).map((doc) => doc._key);
      assertEqual(99, db[cn].count());

      docs.forEach((doc, i) => { doc.value = "foo"; doc._key = keys[i]; });
      docs.push({ _key: "doesnotexist" });

      let result = arango.PATCH_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
      
      keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + cn + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc, i) => {
        if (i === 99) {
          assertTrue(doc.error);
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
        } else {
          assertUndefined(doc.error);
          assertUndefined(doc.errorNum);
          assertEqual("foo", doc.value);
        }
      });
    },
    
    testCustomShardingReplaceSingleDocument: function () {
      let key = db[cn].insert({ sk: "test" })._key;
      assertEqual(1, db[cn].count());

      let result = arango.PUT_RAW("/_api/document/" + cn + "/" + encodeURIComponent(key) + "?silent=true", { value: "foo", sk: "test" });
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      assertEqual(1, db[cn].count());
      
      result = arango.GET_RAW("/_api/document/" + cn + "/" + encodeURIComponent(key));
      assertEqual(key, result.parsedBody._key);
      assertEqual("foo", result.parsedBody.value);
    },
    
    testCustomShardingReplaceSingleDocumentWithFailure: function () {
      db[cn].insert({ sk: "test" });
      assertEqual(1, db[cn].count());

      let result = arango.PUT_RAW("/_api/document/" + cn + "/notthere?silent=true", { value: "foo", sk: "test" });
      assertEqual(404, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testCustomShardingReplaceMultipleDocuments: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ sk: "test" + i });
      }
      let keys = db[cn].insert(docs).map((doc) => doc._key);
      assertEqual(100, db[cn].count());
      
      docs.forEach((doc, i) => { doc.value = "foo"; doc._key = keys[i]; });

      let result = arango.PUT_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      
      keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + cn + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc) => {
        assertEqual("foo", doc.value);
      });
    },
    
    testCustomShardingReplaceMultipleDocumentsWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ sk: "test" + i });
      }
      let keys = db[cn].insert(docs).map((doc) => doc._key);
      assertEqual(99, db[cn].count());

      docs.forEach((doc, i) => { doc.value = "foo"; doc._key = keys[i]; });
      docs.push({ _key: "doesnotexist", sk: "test0" });

      let result = arango.PUT_RAW("/_api/document/" + cn + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
      
      keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + cn + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc, i) => {
        if (i === 99) {
          assertTrue(doc.error);
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
        } else {
          assertUndefined(doc.error);
          assertUndefined(doc.errorNum);
          assertEqual("foo", doc.value);
        }
      });
    },
  };
}

function SilentTestSmartGraphSuite () {
  'use strict';
  const vn = "UnitTestsVertex";
  const en = "UnitTestsEdges";
  const gn = "UnitTestsGraph";
  const graphs = require("@arangodb/smart-graph");
  
  return {
    setUp: function () {
      graphs._create(gn, [graphs._relation(en, vn, vn)], null, { numberOfShards: 4, smartGraphAttribute: "sga" });
    },

    tearDown: function () {
      try {
        graphs._drop(gn, true);
      } catch (err) {}
      db._drop(vn);
      db._drop(en);
    },
    
    testSmartGraphInsertSingleEdge: function () {
      const doc = { sga: "test", _from: vn + "/test:test1", _to: vn + "/test:test2" };
      let result = arango.POST_RAW("/_api/document/" + en + "?silent=true", doc);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);

      assertEqual(1, db[en].count());
    },
    
    testSmartGraphInsertSingleEdgeWithInvalidFromTo: function () {
      const doc = { sga: "test" }; // no _from and _to
      let result = arango.POST_RAW("/_api/document/" + en + "?silent=true", doc);
      assertEqual(400, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE.code, result.parsedBody.errorNum);
    },
    
    testSmartGraphInsertSingleEdgeWithFailure: function () {
      const doc = { sga: "test", _from: vn + "/test:test1", _to: vn + "/test:test2", _key: "fuchs" };
      let result = arango.POST_RAW("/_api/document/" + en + "?silent=true", doc);
      assertEqual(400, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, result.parsedBody.errorNum);
    },
    
    testSmartGraphInsertMultipleEdges: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _from: vn + "/test" + i + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      let result = arango.POST_RAW("/_api/document/" + en + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
    },
    
    testSmartGraphInsertMultipleEdgesWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ _from: vn + "/test" + i + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      docs.push({ _key: 123, _from: vn + "/test0:test0", _to: vn + "/test0:test0" });
      let result = arango.POST_RAW("/_api/document/" + en + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, doc.errorNum);
    },
    
    testSmartGraphInsertMultipleEdgesWithAllFailures1: function () {
      let docs = [];
      for (let i = 0; i < 50; ++i) {
        docs.push({ _key: "fuchs", _from: vn + "/test" + i + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      let result = arango.POST_RAW("/_api/document/" + en + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code] : 50 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(50, result.parsedBody.length);
      for (let i = 0; i < 50; ++i) {
        let doc = result.parsedBody[i];
        assertTrue(doc.error);
        assertEqual(errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, doc.errorNum);
      }
    },
    
    testSmartGraphInsertMultipleEdgesWithAllFailures2: function () {
      let docs = [];
      for (let i = 0; i < 50; ++i) {
        docs.push({ _key: "fuchs", _from: vn + "/test" + (i + 1) + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      let result = arango.POST_RAW("/_api/document/" + en + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code] : 50 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(50, result.parsedBody.length);
      for (let i = 0; i < 50; ++i) {
        let doc = result.parsedBody[i];
        assertTrue(doc.error);
        assertEqual(errors.ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY.code, doc.errorNum);
      }
    },
    
    testSmartGraphRemoveSingleEdge: function () {
      let key = db[en].insert({ _from: vn + "/test:test", _to: vn + "/test:test" })._key;
      assertEqual(1, db[en].count());

      let result = arango.DELETE_RAW("/_api/document/" + en + "/" + encodeURIComponent(key) + "?silent=true");
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      assertEqual(0, db[en].count());
    },

    testSmartGraphRemoveSingleEdgeWithFailure: function () {
      let result = arango.DELETE_RAW("/_api/document/" + en + "/test%3Anotthere%3Atest?silent=true");
      assertEqual(404, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testSmartGraphRemoveMultipleEdges: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _from: vn + "/test" + i + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      let keys = db[en].insert(docs).map((doc) => doc._key);
      assertEqual(100, db[en].count());

      let result = arango.DELETE_RAW("/_api/document/" + en + "?silent=true", keys);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);

      assertEqual(0, db[en].count());
    },
    
    testSmartGraphRemoveMultipleEdgesWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ _from: vn + "/test" + i + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      let keys = db[en].insert(docs).map((doc) => doc._key);
      assertEqual(99, db[en].count());

      keys.push("test:doesnotexist:test");

      let result = arango.DELETE_RAW("/_api/document/" + en + "?silent=true", keys);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
      assertEqual(0, db[en].count());
    },
    
    testSmartGraphUpdateSingleEdge: function () {
      let key = db[en].insert({ _from: vn + "/test:test", _to: vn + "/test:test" })._key;
      assertEqual(1, db[en].count());

      let result = arango.PATCH_RAW("/_api/document/" + en + "/" + encodeURIComponent(key) + "?silent=true", { value: "foo" });
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      assertEqual(1, db[en].count());
      
      result = arango.GET_RAW("/_api/document/" + en + "/" + encodeURIComponent(key));
      assertEqual(key, result.parsedBody._key);
      assertEqual("foo", result.parsedBody.value);
    },
    
    testSmartGraphUpdateSingleEdgeWithFailure: function () {
      let result = arango.PATCH_RAW("/_api/document/" + en + "/test%3Anotthere%3Atest?silent=true", { value: "foo" });
      assertEqual(404, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testSmartGraphUpdateMultipleEdges: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _from: vn + "/test" + i + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      let keys = db[en].insert(docs).map((doc) => doc._key);
      assertEqual(100, db[en].count());
      
      docs.forEach((doc, i) => { doc.value = "foo"; doc._key = keys[i]; });

      let result = arango.PATCH_RAW("/_api/document/" + en + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      
      keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + en + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc) => {
        assertEqual("foo", doc.value);
      });
    },
    
    testSmartGraphUpdateMultipleEdgesWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ _from: vn + "/test" + i + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      let keys = db[en].insert(docs).map((doc) => doc._key);
      assertEqual(99, db[en].count());

      docs.forEach((doc, i) => { doc.value = "foo"; doc._key = keys[i]; });
      docs.push({ _key: "test:doesnotexist:test" });

      let result = arango.PATCH_RAW("/_api/document/" + en + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
      
      keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + en + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc, i) => {
        if (i === 99) {
          assertTrue(doc.error);
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
        } else {
          assertUndefined(doc.error);
          assertUndefined(doc.errorNum);
          assertEqual("foo", doc.value);
        }
      });
    },
    
    testSmartGraphReplaceSingleEdge: function () {
      let key = db[en].insert({ _from: vn + "/test:test", _to: vn + "/test:test" })._key;
      assertEqual(1, db[en].count());

      let result = arango.PUT_RAW("/_api/document/" + en + "/" + encodeURIComponent(key) + "?silent=true", { value: "foo", _from: vn + "/test:test", _to: vn + "/test:test" });
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      assertEqual(1, db[en].count());
      
      result = arango.GET_RAW("/_api/document/" + en + "/" + encodeURIComponent(key));
      assertEqual(key, result.parsedBody._key);
      assertEqual("foo", result.parsedBody.value);
    },
    
    testSmartGraphReplaceSingleEdgeWithFailure: function () {
      db[en].insert({ _from: vn + "/test:test", _to: vn + "/test:test" });
      assertEqual(1, db[en].count());

      let result = arango.PUT_RAW("/_api/document/" + en + "/test%3Anotthere%3Atest?silent=true", { value: "foo", _from: vn + "/test:test", _to: vn + "/test:test" });
      assertEqual(404, result.code);
      assertTrue(result.parsedBody.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, result.parsedBody.errorNum);
    },
    
    testSmartGraphReplaceMultipleEdges: function () {
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ _from: vn + "/test" + i + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      let keys = db[en].insert(docs).map((doc) => doc._key);
      assertEqual(100, db[en].count());
      
      docs.forEach((doc, i) => { doc.value = "foo"; doc._key = keys[i]; });

      let result = arango.PUT_RAW("/_api/document/" + en + "?silent=true", docs);
      assertUndefined(result.headers["x-arango-error-codes"]);
      assertEqual(202, result.code);
      assertEqual({}, result.parsedBody);
      
      keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + en + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc) => {
        assertEqual("foo", doc.value);
      });
    },
    
    testSmartGraphReplaceMultipleEdgesWithFailure: function () {
      let docs = [];
      for (let i = 0; i < 99; ++i) {
        docs.push({ _from: vn + "/test" + i + ":test" + i, _to: vn + "/test" + i + ":test" + i });
      }
      let keys = db[en].insert(docs).map((doc) => doc._key);
      assertEqual(99, db[en].count());

      docs.forEach((doc, i) => { doc.value = "foo"; doc._key = keys[i]; });
      docs.push({ _key: "test:doesnotexist:test" });

      let result = arango.PUT_RAW("/_api/document/" + en + "?silent=true", docs);
      assertEqual(202, result.code);
      assertEqual({ [errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code] : 1 }, JSON.parse(result.headers["x-arango-error-codes"]));
      assertTrue(Array.isArray(result.parsedBody));
      assertEqual(1, result.parsedBody.length);
      let doc = result.parsedBody[0];
      assertTrue(doc.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
      
      keys = docs.map((doc) => doc._key);
      result = arango.PUT_RAW("/_api/document/" + en + "?onlyget=true", keys);
      assertEqual(100, result.parsedBody.length);
      result.parsedBody.forEach((doc, i) => {
        if (i === 99) {
          assertTrue(doc.error);
          assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc.errorNum);
        } else {
          assertUndefined(doc.error);
          assertUndefined(doc.errorNum);
          assertEqual("foo", doc.value);
        }
      });
    },
  };
}

jsunity.run(SilentTestSuite);
if (isCluster) {
  jsunity.run(SilentTestCustomShardingSuite);
}
if (isEnterprise) {
  jsunity.run(SilentTestSmartGraphSuite);
}

return jsunity.done();
