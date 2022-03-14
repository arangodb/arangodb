/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined,  assertMatch*/

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
let reFull = /^[a-zA-Z0-9_\-]+\/\d+$/;

////////////////////////////////////////////////////////////////////////////////;
// creating a geo index;
////////////////////////////////////////////////////////////////////////////////;
function creating_geo_indexesSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._drop(cn);
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_either_201_for_new_or_200_for_old_indexes: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "geo", "fields" : [ "a" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "geo");
      assertFalse(doc.parsedBody['geoJson']);
      assertEqual(doc.parsedBody['fields'], [ "a" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);

      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "geo");
      assertFalse(doc.parsedBody['geoJson']);
      assertEqual(doc.parsedBody['fields'], [ "a" ]);
      assertFalse(doc.parsedBody['isNewlyCreated']);
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
    },

    test_creating_geo_index_with_location: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "geo", "fields" : [ "b" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "geo");
      assertFalse(doc.parsedBody['geoJson']);
      assertEqual(doc.parsedBody['fields'], [ "b" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
    },

    test_creating_geo_index_with_location_and_geo_json_EQ_true: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "geo", "fields" : [ "c" ], "geoJson" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "geo");
      assertTrue(doc.parsedBody['geoJson']);
      assertEqual(doc.parsedBody['fields'], [ "c" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
    },

    test_creating_geo_index_with_location_and_geo_json_EQ_false: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "geo", "fields" : [ "d" ], "geoJson" : false };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "geo");
      assertFalse(doc.parsedBody['geoJson']);
      assertEqual(doc.parsedBody['fields'], [ "d" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
    },

    test_creating_geo_index_with_latitude_and_longitude: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "geo", "fields" : [ "e", "f" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "geo");
      assertEqual(doc.parsedBody['fields'], [ "e", "f" ]);
      assertTrue(doc.parsedBody['isNewlyCreated']);
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['sparse']);
    },

    test_creating_geo_index_with_constraint_1: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "geo", "fields" : [ "c" ], "geoJson" : true, "constraint" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "geo");
      assertTrue(doc.parsedBody['geoJson']);
      assertEqual(doc.parsedBody['fields'], [ "c" ]);
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['isNewlyCreated']);
      assertTrue(doc.parsedBody['sparse']);
    },

    test_creating_geo_index_with_constraint_2: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "geo", "fields" : [ "c", "d" ], "geoJson" : false, "unique" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['type'], "geo");
      assertEqual(doc.parsedBody['fields'], [ "c", "d" ]);
      assertFalse(doc.parsedBody['unique']);
      assertTrue(doc.parsedBody['isNewlyCreated']);
      assertTrue(doc.parsedBody['sparse']);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// creating a geo index and unloading;
////////////////////////////////////////////////////////////////////////////////;
function geo_indexes_after_unload_loadSuite () {
  let cn = "UnitTestsCollectionIndexes";
  return {
    setUp: function() {
      db._drop(cn);
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_survives_unload: function() {
      let cmd = api + `?collection=${cn}`;
      let body = { "type" : "geo", "fields" : [ "a" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertMatch(reFull, doc.parsedBody['id']);

      let iid = doc.parsedBody['id'];

      cmd = api + `/${iid}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertMatch(reFull, doc.parsedBody['id']);
      assertEqual(doc.parsedBody['id'], iid);
      assertEqual(doc.parsedBody['type'], "geo");
      assertFalse(doc.parsedBody['geoJson']);
      assertEqual(doc.parsedBody['fields'], [ "a" ]);
      assertTrue(doc.parsedBody['sparse']);
    },
  };
}

jsunity.run(creating_geo_indexesSuite);
jsunity.run(geo_indexes_after_unload_loadSuite);
return jsunity.done();
