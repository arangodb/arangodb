/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global fail, assertTrue, assertFalse, assertEqual, assertNotEqual, assertTypeOf */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the babies interface
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

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const ERRORS = arangodb.errors;
const db = arangodb.db;

function BabiesStandardShardingSuite () {
  'use strict';

  const n = 50;
  const cn = "UnitTestsCollectionBabies";
  let c;

  let keys = [];
  for (let i = 0; i < n; ++i) {
    keys.push("test" + i);
  }

  return {

    setUp : function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 7 });
      for (let i = 0; i < n; ++i) {
        c.insert({ _key: "test" + i, value:  i });
      }
    },

    tearDown : function () {
      db._drop(cn);
    },

    testGetBabiesStandard : function () {
      let docs = c.document(keys);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertEqual(i, docs[i].value);
        assertEqual("test" + i, docs[i]._key);
      }
    },
    
    testGetNonExistingBabiesStandard : function () {
      let keys = [];
      for (let i = 0; i < n; ++i) {
        keys.push("peng" + i);
      }
      let docs = c.document(keys);
      for (let i = 0; i < n; ++i) {
        assertTrue(docs[i].error);
        assertFalse(docs[i].hasOwnProperty("_key"));
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, docs[i].errorNum);
      }
    },
    
    testInsertBabiesStandard : function () {
      c.truncate({ compact: false });
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ _key: "test" + i, value: i });
      }
      docs = c.insert(docs);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertEqual("test" + i, docs[i]._key);
      }
    },
    
    testRemoveBabiesStandard : function () {
      let docs = c.remove(keys);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertEqual("test" + i, docs[i]._key);
      }
    },
    
    testUpdateBabiesStandard : function () {
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value: i * 2 });
      }
      docs = c.update(keys, docs);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertTrue(docs[i].hasOwnProperty("_oldRev"));
        assertEqual("test" + i, docs[i]._key);
      }
    },
    
    testReplaceBabiesStandard : function () {
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value: i * 2 });
      }
      docs = c.replace(keys, docs);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertTrue(docs[i].hasOwnProperty("_oldRev"));
        assertEqual("test" + i, docs[i]._key);
      }
    },
    

  };
}

function BabiesCustomShardingSuite () {
  'use strict';

  const n = 50;
  const cn = "UnitTestsCollectionBabies";
  let c;
  let keys;

  return {

    setUp : function () {
      db._drop(cn);
      c = db._create(cn, { numberOfShards: 7, shardKeys: ["value"] });
      keys = [];
      for (let i = 0; i < n; ++i) {
        keys.push(c.insert({ value:  i })._key);
      }
    },

    tearDown : function () {
      db._drop(cn);
    },

    testGetBabiesCustom : function () {
      let docs = c.document(keys);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertEqual(i, docs[i].value);
        assertEqual(keys[i], docs[i]._key);
      }
    },
    
    testGetNonExistingBabiesCustom : function () {
      let keys = [];
      for (let i = 0; i < n; ++i) {
        keys.push("peng" + i);
      }
      let docs = c.document(keys);
      for (let i = 0; i < n; ++i) {
        assertTrue(docs[i].error);
        assertFalse(docs[i].hasOwnProperty("_key"));
        assertEqual(ERRORS.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, docs[i].errorNum);
      }
    },
    
    testInsertBabiesCustom : function () {
      c.truncate({ compact: false });
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value: i });
      }
      docs = c.insert(docs);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertTrue(docs[i].hasOwnProperty("_key"));
      }
    },
    
    testRemoveBabiesCustom : function () {
      let docs = c.remove(keys);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertEqual(keys[i], docs[i]._key);
      }
    },
    
    testUpdateBabiesCustom : function () {
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value: i, value2: i * 2 });
      }
      docs = c.update(keys, docs);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertTrue(docs[i].hasOwnProperty("_oldRev"));
        assertEqual(keys[i], docs[i]._key);
      }
    },
    
    testReplaceBabiesCustom : function () {
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ value: i, value2: i * 2 });
      }
      docs = c.replace(keys, docs);
      for (let i = 0; i < n; ++i) {
        assertFalse(docs[i].hasOwnProperty("error"));
        assertTrue(docs[i].hasOwnProperty("_oldRev"));
        assertEqual(keys[i], docs[i]._key);
      }
    },
    

  };
}

jsunity.run(BabiesStandardShardingSuite);
jsunity.run(BabiesCustomShardingSuite);

return jsunity.done();
