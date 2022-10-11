/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for DOCUMENT function
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let internal = require("internal");
let db = require("@arangodb").db;
let jsunity = require("jsunity");

function ahuacatlDocumentSuite () {
  let cn = "UnitTestsAhuacatlDocument";

  return {

    setUp : function () {
      db._drop(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testDocumentSingleShard : function () {
      let c = db._create(cn, {numberOfShards: 1});
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i, value: i });
      }

      for (let i = 0; i < 100; ++i) {
        let doc = c.document("test" + i);
        assertEqual(i, doc.value);
        assertEqual("test" + i, doc._key);

        let res = AQL_EXECUTE("RETURN DOCUMENT('" + cn + "/test" + i + "')").json;
        doc = res[0];
        assertEqual(i, doc.value);
        assertEqual("test" + i, doc._key);
      }
    },
    
    testDocumentMultipleShards : function () {
      let c = db._create(cn, {numberOfShards: 5});
      for (let i = 0; i < 100; ++i) {
        c.insert({ _key: "test" + i, value: i });
      }

      for (let i = 0; i < 100; ++i) {
        let doc = c.document("test" + i);
        assertEqual(i, doc.value);
        assertEqual("test" + i, doc._key);

        let res = AQL_EXECUTE("RETURN DOCUMENT('" + cn + "/test" + i + "')").json;
        doc = res[0];
        assertEqual(i, doc.value);
        assertEqual("test" + i, doc._key);
      }
    },
    
    testDocumentSingleShardNonDefaultShardKey : function () {
      let c = db._create(cn, {numberOfShards: 1, shardKeys: ["value"]});
      let keys = [];
      for (let i = 0; i < 100; ++i) {
        keys.push(c.insert({ value: i })._key);
      }

      keys.forEach(function(key, i) {
        let doc = c.document(key);
        assertEqual(i, doc.value);
        assertEqual(key, doc._key);

        let res = AQL_EXECUTE("RETURN DOCUMENT('" + cn + "/" + key + "')").json;
        doc = res[0];
        assertEqual(i, doc.value);
        assertEqual(key, doc._key);
      });
    },
    
    testDocumentMultipleShardsNonDefaultShardKey : function () {
      let c = db._create(cn, {numberOfShards: 5, shardKeys: ["value"]});
      let keys = [];
      for (let i = 0; i < 100; ++i) {
        keys.push(c.insert({ value: i })._key);
      }

      keys.forEach(function(key, i) {
        let doc = c.document(key);
        assertEqual(i, doc.value);
        assertEqual(key, doc._key);

        let res = AQL_EXECUTE("RETURN DOCUMENT('" + cn + "/" + key + "')").json;
        doc = res[0];
        assertEqual(i, doc.value);
        assertEqual(key, doc._key);
      });
    },
    
    testDocumentMultipleSingleShardDefaultShardKey : function () {
      let c = db._create(cn, {numberOfShards: 1});
      let ids = [];
      for (let i = 0; i < 100; ++i) {
        ids.push(c.insert({ value: i })._id);
      }

      let res = AQL_EXECUTE("RETURN DOCUMENT(" + JSON.stringify(ids) + ")").json[0];
      ids.forEach(function(id, i) {
        let doc = c.document(id);
        assertEqual(i, doc.value);
        assertEqual(id, doc._id);
        assertEqual(i, res[i].value);
        assertEqual(id, res[i]._id);
      });
    },
    
    testDocumentMultipleMultipleShardsDefaultShardKey : function () {
      let c = db._create(cn, {numberOfShards: 5});
      let ids = [];
      for (let i = 0; i < 100; ++i) {
        ids.push(c.insert({ value: i })._id);
      }

      let res = AQL_EXECUTE("RETURN DOCUMENT(" + JSON.stringify(ids) + ")").json[0];
      ids.forEach(function(id, i) {
        let doc = c.document(id);
        assertEqual(i, doc.value);
        assertEqual(id, doc._id);
        assertEqual(i, res[i].value);
        assertEqual(id, res[i]._id);
      });
    },
    
    testDocumentMultipleSingleShardNonDefaultShardKey : function () {
      let c = db._create(cn, {numberOfShards: 1, shardKeys: ["value"]});
      let ids = [];
      for (let i = 0; i < 100; ++i) {
        ids.push(c.insert({ value: i })._id);
      }

      let res = AQL_EXECUTE("RETURN DOCUMENT(" + JSON.stringify(ids) + ")").json[0];
      ids.forEach(function(id, i) {
        let doc = c.document(id);
        assertEqual(i, doc.value);
        assertEqual(id, doc._id);
        assertEqual(i, res[i].value);
        assertEqual(id, res[i]._id);
      });
    },
    
    testDocumentMultipleMultipleShardsNonDefaultShardKey : function () {
      let c = db._create(cn, {numberOfShards: 5, shardKeys: ["value"]});
      let ids = [];
      for (let i = 0; i < 100; ++i) {
        ids.push(c.insert({ value: i })._id);
      }

      let res = AQL_EXECUTE("RETURN DOCUMENT(" + JSON.stringify(ids) + ")").json[0];
      ids.forEach(function(id, i) {
        let doc = c.document(id);
        assertEqual(i, doc.value);
        assertEqual(id, doc._id);
        assertEqual(i, res[i].value);
        assertEqual(id, res[i]._id);
      });
    }

  };
}

jsunity.run(ahuacatlDocumentSuite);

return jsunity.done();
