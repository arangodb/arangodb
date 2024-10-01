/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNull */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

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
    
    testDocumentEmptyKey : function () {
      let res = db._query("RETURN DOCUMENT('')").toArray();
      let doc = res[0];
      assertNull(doc);
      
      res = db._query("RETURN DOCUMENT('" + cn + "', '')").toArray();
      doc = res[0];
      assertNull(doc);
    },
    
    testDocumentEmptyKeys : function () {
      let res = db._query("RETURN DOCUMENT(['', ''])").toArray();
      let doc = res[0];
      assertEqual([], doc);
      
      res = db._query("RETURN DOCUMENT('" + cn + "', ['', ''])").toArray();
      doc = res[0];
      assertEqual([], doc);
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

        let res = db._query("RETURN DOCUMENT('" + cn + "/test" + i + "')").toArray();
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

        let res = db._query("RETURN DOCUMENT('" + cn + "/test" + i + "')").toArray();
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

        let res = db._query("RETURN DOCUMENT('" + cn + "/" + key + "')").toArray();
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

        let res = db._query("RETURN DOCUMENT('" + cn + "/" + key + "')").toArray();
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

      let res = db._query("RETURN DOCUMENT(" + JSON.stringify(ids) + ")").toArray()[0];
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

      let res = db._query("RETURN DOCUMENT(" + JSON.stringify(ids) + ")").toArray()[0];
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

      let res = db._query("RETURN DOCUMENT(" + JSON.stringify(ids) + ")").toArray()[0];
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

      let res = db._query("RETURN DOCUMENT(" + JSON.stringify(ids) + ")").toArray()[0];
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
