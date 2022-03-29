/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertNotEqual, assertMatch */

// //////////////////////////////////////////////////////////////////////////////
// / @brief 
// /
// /
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
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author 
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const jsunity = require("jsunity");

let api = "/_api/index";
let reFull = new RegExp('^[a-zA-Z0-9_\-]+/[0-9]+$');

////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function error_handlingSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_an_error_if_collection_identifier_is_unknown: function() {
      let cmd = api + "/123456/123456";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], 1203);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    },

    test_returns_an_error_if_index_identifier_is_unknown: function() {
      let cmd = api + `/${cn}/123456`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// creating a unique constraint;
////////////////////////////////////////////////////////////////////////////////;
function creating_unique_constraintsSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_either_201_for_new_or_200_for_unique_old_indexes: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "hash", "unique" : true, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "hash");
      assertTrue(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "hash");
      assertTrue(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertFalse(doc.parsedBody['isNewlyCreated']);
    },

    test_returns_either_201_for_new_or_200_for_old_unique_indexes__sparse_indexes: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "hash", "unique" : true, "fields" : [ "a", "b" ], "sparse" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "hash");
      assertTrue(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "hash");
      assertTrue(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertFalse(doc.parsedBody['isNewlyCreated']);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// creating a hash index;
////////////////////////////////////////////////////////////////////////////////;
function creating_hash_indexesSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_either_201_for_new_or_200_for_old_hash_indexes: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "hash", "unique" : false, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "hash");
      assertFalse(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "hash");
      assertFalse(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertFalse(doc.parsedBody['isNewlyCreated']);
    },

    test_returns_either_201_for_new_or_200_for_old_hash_indexes__sparse_index: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "hash", "unique" : false, "fields" : [ "a", "b" ], "sparse" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "hash");
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "hash");
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertFalse(doc.parsedBody['isNewlyCreated']);
    },

    test_returns_either_201_for_new_or_200_for_old_hash_indexes__mixed_sparsity: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "hash", "unique" : false, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "hash");
      assertFalse(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      body = { "type" : "hash", "unique" : false, "fields" : [ "a", "b" ], "sparse" : true };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertNotEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "hash");
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// creating a skiplist;
////////////////////////////////////////////////////////////////////////////////;
function creating_skiplistsSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_either_201_for_new_or_200_for_old_skiplist_indexes: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : false, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertFalse(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertFalse(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertFalse(doc.parsedBody['isNewlyCreated']);
    },

    test_returns_either_201_for_new_or_200_for_old_skiplist_indexes__sparse_index: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : false, "fields" : [ "a", "b" ], "sparse" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertFalse(doc.parsedBody['isNewlyCreated']);
    },

    test_returns_either_201_for_new_or_200_for_old_skiplist_indexes__mixed_sparsity: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : false, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertFalse(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      body = { "type" : "skiplist", "unique" : false, "fields" : [ "a", "b" ], "sparse" : true };
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertNotEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// creating a unique skiplist;
////////////////////////////////////////////////////////////////////////////////;
function creating_unique_skiplistsSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_either_201_for_new_or_200_for_old_unique_skiplist_indexes: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : true, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertFalse(doc.parsedBody['isNewlyCreated']);
    },

    test_returns_either_201_for_new_or_200_for_old_unique_skiplist_indexes__sparse_index: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : true, "fields" : [ "a", "b" ], "sparse" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertFalse(doc.parsedBody['isNewlyCreated']);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// reading all indexes;
////////////////////////////////////////////////////////////////////////////////;
function reading_all_indexesSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_all_index_for_a_collection_identifier: function() {
      let cmd = api + `?collection=${cn}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      let indexes = doc.parsedBody['indexes'];
      let identifiers = doc.parsedBody['identifiers'];
      indexes.forEach(index => {
        assertMatch(reFull, index['id']);
        assertEqual(identifiers[index['id']], index);
      });
    },

    test_returns_all_index_for_a_collection_name: function() {
      let cmd = api + `?collection=${cn}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      let indexes = doc.parsedBody['indexes'];
      let identifiers = doc.parsedBody['identifiers'];

      indexes.forEach(index => {
        assertMatch(reFull, index['id']);
        assertEqual(identifiers[index['id']], index);
      });
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// reading one index;
////////////////////////////////////////////////////////////////////////////////;
function reading_an_indexSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_primary_index_for_a_collection_identifier: function() {
      let cmd = api + `/${cn}/0`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], `${cn}/0`);
      assertEqual(doc.parsedBody['type'], "primary");
    },

    test_returns_primary_index_for_a_collection_name: function() {
      let cmd = api + `/${cn}/0`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], `${cn}/0`);
      assertEqual(doc.parsedBody['type'], "primary");
    }
  };
}
////////////////////////////////////////////////////////////////////////////////;
// deleting an index;
////////////////////////////////////////////////////////////////////////////////;
function deleting_an_indexSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_deleting_an_index: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : true, "fields" : [ "a", "b" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull,doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);
      assertFalse(doc.parsedBody['sparse']);
      assertEqual(doc.parsedBody['fields'], [ "a", "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);

      let iid = doc.parsedBody['id'];

      cmd = api + `/${iid}`;
      doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull,doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);

      cmd = api + `/${iid}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code);
    }
  };
}

jsunity.run(error_handlingSuite);
jsunity.run(creating_unique_constraintsSuite);
jsunity.run(creating_hash_indexesSuite);
jsunity.run(creating_skiplistsSuite);
jsunity.run(creating_unique_skiplistsSuite);
jsunity.run(reading_all_indexesSuite);
jsunity.run(reading_an_indexSuite);
jsunity.run(deleting_an_indexSuite);
return jsunity.done();
