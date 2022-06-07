/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotEqual, assertNotUndefined */

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

let api = "/_api/collection";


////////////////////////////////////////////////////////////////////////////////;
// reading all collections;
////////////////////////////////////////////////////////////////////////////////;
function all_collectionsSuite () {
  let cid;
  let cols = ["units", "employees", "locations" ];
  return {
    setUp: function() {
      cols.forEach(cn => {
        db._drop(cn);
        db._create(cn);
      });
    },

    tearDown: function() {
      cols.forEach(cn => {
        db._drop(cn);
      });
    },

    test_returns_all_collections: function() {
      let cmd = api;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      let collections = doc.parsedBody["result"];;

      let total = 0;
      let realCollections = [ ];
      db._collections().forEach(collection => {
        if (cols.includes(collection["name"]())) {
          realCollections.push(collection);
        }
        total = total + 1;
      });
      assertEqual(realCollections.length, 3);
      assertTrue(total > 3);
    },

    test_returns_all_collections__exclude_system_collections: function() {
      let cmd = api + '/?excludeSystem=true';
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      let collections = doc.parsedBody["result"];
      let realCollections = [ ];

      let total = 0;
      db._collections().forEach(collection => {
        if (cols.includes(collection["name"]())) {
          realCollections.push(collection);
        }
        total = total + 1;
      });
      assertEqual(realCollections.length, 3);
      assertTrue(total >= 3);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;

function error_handlingSuite () {
  return {
    test_returns_an_error_if_collection_identifier_is_unknown: function() {
      let cmd = api + "/123456";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 404);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], 404);
    },

    test_creating_a_collection_without_name: function() {
      let cmd = api;
      let doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_ILLEGAL_NAME.code);
    },

    test_creating_a_collection_with_an_illegal_name: function() {
      let cmd = api;
      let body = "{ \"name\" : \"1\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_ILLEGAL_NAME.code);
    },

    test_creating_a_collection_with_a_duplicate_name: function() {
      let cn = "UnitTestsCollectionBasics";
      let cid = db._create(cn);

      let cmd = api;
      let body = `{ \"name\" : \"${cn}\" }`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 409);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 409);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DUPLICATE_NAME.code);
    },

    test_creating_a_collection_with_an_illegal_body: function() {
      let cmd = api;
      let body = "{ name : world }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
      assertEqual(doc.parsedBody['errorMessage'], "VPackError error: Expecting '\"' or '}'");
    },

    test_creating_a_collection_with_a_null_body: function() {
      let cmd = api;
      let body = "null";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_ILLEGAL_NAME.code);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// schema validation;
////////////////////////////////////////////////////////////////////////////////;
function schema_validationSuite () {
  let cn = "UnitTestsCollectionBasics";
  let cid;
  return {
    setUp: function() {
      db._drop(cn);
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_sets_an_invalid_schema: function() {
      let cmd = api + "/" + cn + "/properties";
      let body = "{ \"schema\": { \"rule\": \"peng!\", \"level\": \"strict\", \"message\": \"document has an invalid schema!\" } }";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 400);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_VALIDATION_BAD_PARAMETER.code);
    },

    test_sets_a_valid_schema: function() {
      let cmd = api + "/" + cn + "/properties";
      let body = "{ \"schema\": { \"rule\": { \"properties\": { \"_key\": { \"type\": \"string\" }, \"_rev\": { \"type\": \"string\" }, \"_id\": { \"type\": \"string\" }, \"name\": { \"type\": \"object\", \"properties\": { \"first\": { \"type\": \"string\", \"minLength\": 1, \"maxLength\": 50 }, \"last\": { \"type\": \"string\", \"minLength\": 1, \"maxLength\": 50 } }, \"required\": [\"first\", \"last\"] }, \"status\": { \"enum\": [\"active\", \"inactive\", \"deleted\"] } }, \"additionalProperties\": false, \"required\": [\"name\", \"status\"] }, \"level\": \"strict\", \"message\": \"document has an invalid schema!\" } }";
      let doc = arango.PUT_RAW(cmd, body);
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['schema']['level'], "strict");
      assertEqual(doc.parsedBody['schema']['message'], "document has an invalid schema!");
    },

    test_stores_valid_documents: function() {
      let cmd = api + "/" + cn + "/properties";
      let body = "{ \"schema\": { \"rule\": { \"properties\": { \"_key\": { \"type\": \"string\" }, \"_rev\": { \"type\": \"string\" }, \"_id\": { \"type\": \"string\" }, \"name\": { \"type\": \"object\", \"properties\": { \"first\": { \"type\": \"string\", \"minLength\": 1, \"maxLength\": 50 }, \"last\": { \"type\": \"string\", \"minLength\": 1, \"maxLength\": 50 } }, \"required\": [\"first\", \"last\"] }, \"status\": { \"enum\": [\"active\", \"inactive\", \"deleted\"] } }, \"additionalProperties\": false, \"required\": [\"name\", \"status\"] }, \"level\": \"strict\", \"message\": \"document has an invalid schema!\" } }";
      let doc = arango.PUT_RAW(cmd, body);
      assertEqual(doc.code, 200);

      sleep(2);
      body = "{ \"name\": { \"first\": \"test\", \"last\": \"test\" }, \"status\": \"active\" }";
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);
      assertEqual(doc.code, 202);

      body = "{ \"name\": { \"first\": \"a\", \"last\": \"b\" }, \"status\": \"inactive\" }";
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);
      assertEqual(doc.code, 202);
    },

    test_stores_invalid_documents: function() {
      let cmd = api + "/" + cn + "/properties";
      let body = "{ \"schema\": { \"rule\": { \"properties\": { \"_key\": { \"type\": \"string\" }, \"_rev\": { \"type\": \"string\" }, \"_id\": { \"type\": \"string\" }, \"name\": { \"type\": \"object\", \"properties\": { \"first\": { \"type\": \"string\", \"minLength\": 1, \"maxLength\": 50 }, \"last\": { \"type\": \"string\", \"minLength\": 1, \"maxLength\": 50 } }, \"required\": [\"first\", \"last\"] }, \"status\": { \"enum\": [\"active\", \"inactive\", \"deleted\"] } }, \"additionalProperties\": false, \"required\": [\"name\", \"status\"] }, \"level\": \"strict\", \"message\": \"document has an invalid schema!\" } }";
      let doc = arango.PUT_RAW(cmd, body);

      sleep(2);

      body = "{ \"name\": { \"first\" : \"\", \"last\": \"test\" }, \"status\": \"active\" }";
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);
      assertEqual(doc.code, 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_VALIDATION_FAILED.code);

      body = "{ \"name\": { \"first\" : \"\", \"last\": \"\" }, \"status\": \"active\" }";
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);
      assertEqual(doc.code, 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_VALIDATION_FAILED.code);

      body = "{ \"name\": { \"first\" : \"test\", \"last\": \"test\" } }";
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);
      assertEqual(doc.code, 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_VALIDATION_FAILED.code);

      body = "{ \"name\": { \"first\" : \"test\", \"last\": \"test\" }, \"status\": \"foo\" }";
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);
      assertEqual(doc.code, 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_VALIDATION_FAILED.code);

      body = "{ \"name\": { }, \"status\": \"active\" }";
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);
      assertEqual(doc.code, 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_VALIDATION_FAILED.code);

      body = "{ \"first\": \"abc\", \"last\": \"test\", \"status\": \"active\" }";
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);
      assertEqual(doc.code, 400);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_VALIDATION_FAILED.code);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// reading a collection;
////////////////////////////////////////////////////////////////////////////////;
function readingSuite () {
  let cn = "UnitTestsCollectionBasics";
  let cid;
  return {
    setUp: function() {
      db._drop(cn);
      cid = db._create(cn, { waitForSync: true });
    },

    tearDown: function() {
      db._drop(cn);
    },

    // get;
    test_finds_the_collection_by_identifier: function() {
      let cmd = api + `/${cid._id}`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200, doc);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);

      let cmd2 = api + "/" + cn + "/unload";
      doc = arango.PUT_RAW(cmd2, "");

      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      // effectively the status does not play any role for the RocksDB engine,
      // so it is ok if any of the following statuses gets returned;
      // 2 = unloaded, 3 = loaded, 4 = unloading;
      // additionally, in a cluster there is no such thing as one status for a;
      // collection, as each shard can have a different status;
      assertTrue([2,3,4].includes(doc.parsedBody['status']));
    },

    // get;
    test_finds_the_collection_by_name: function() {
      let cmd = api + "/" + cn;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);

      let cmd2 = api + "/" + cn + "/unload";
      doc = arango.PUT_RAW(cmd2, "");

      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      // effectively the status does not play any role for the RocksDB engine,
      // so it is ok if any of the following statuses gets returned;
      // 2 = unloaded, 3 = loaded, 4 = unloading;
      // additionally, in a cluster there is no such thing as one status for a;
      // collection, as each shard can have a different status;
      assertTrue([2,3,4].includes(doc.parsedBody['status']));
    },

    // get count;
    test_checks_the_size_of_a_collection: function() {
      let cmd = api + "/" + cn + "/count";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertEqual(typeof doc.parsedBody['count'], 'number');
    },

    // get count;
    test_checks_the_properties_of_a_collection: function() {
      let cmd = api + "/" + cn + "/properties";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertTrue(doc.parsedBody['waitForSync']);
      assertFalse(doc.parsedBody['isSystem']);
    },

    // get figures;
    test_extracting_the_figures_for_a_collection: function() {
      let cmd = api + "/" + cn + "/figures";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertEqual(typeof doc.parsedBody['count'], 'number');
      assertEqual(doc.parsedBody['count'], 0);
      assertEqual(typeof doc.parsedBody['figures']['indexes']['count'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['indexes']['size'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['cacheSize'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['cacheInUse'], 'boolean');

      assertEqual(typeof doc.parsedBody['figures']['cacheUsage'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['documentsSize'], 'number');
      if (doc.parsedBody['figures']['cacheInUse']) {
        assertEqual(typeof doc.parsedBody['figures']['cacheLifeTimeHitRate'], 'double');
        assertEqual(typeof doc.parsedBody['figures']['cacheWindowedHitRate'], 'double');
      }
      // create a few documents, this should increase counts;
      let docs = [];
      for (let i = 0; i < 10; i++){
        docs.push({ "test" : i});
      }
      db[cn].save(docs);

      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['count'], 'number');
      assertEqual(doc.parsedBody['count'], 10);
      assertEqual(typeof doc.parsedBody['figures']['cacheSize'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['cacheInUse'], 'boolean');
      assertEqual(typeof doc.parsedBody['figures']['cacheUsage'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['documentsSize'], 'number');
      if (doc.parsedBody['figures']['cacheInUse']) {
        assertEqual(typeof doc.parsedBody['figures']['cacheLifeTimeHitRate'], 'double');
        assertEqual(typeof doc.parsedBody['figures']['cacheWindowedHitRate'], 'double');
      }

      // create a few different documents, this should increase counts;
      docs = [];
      for (let i = 0; i < 10; i++){
        docs.push({ "test" : i});
      }
      db[cn].save(docs);

      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['count'], 'number');
      assertEqual(doc.parsedBody['count'], 20);
      assertEqual(typeof doc.parsedBody['figures']['cacheSize'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['cacheInUse'], 'boolean');
      assertEqual(typeof doc.parsedBody['figures']['cacheUsage'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['documentsSize'], 'number');
      if (doc.parsedBody['figures']['cacheInUse']) {
        assertEqual(typeof doc.parsedBody['figures']['cacheLifeTimeHitRate'], 'double');
        assertEqual(typeof doc.parsedBody['figures']['cacheWindowedHitRate'], 'double');
      }

      // delete a few documents, this should change counts;
      let body = "{ \"collection\" : \"" + cn + "\", \"example\": { \"test\" : 5 } }";
      doc = arango.PUT_RAW("/_api/simple/remove-by-example", body);
      // should delete 2 docuemnts:
      assertEqual(doc.parsedBody['deleted'], 2);
      body = "{ \"collection\" : \"" + cn + "\", \"example\": { \"test3\" : 1 } }";
      doc = arango.PUT_RAW("/_api/simple/remove-by-example", body);
      // should delete no documents:
      assertEqual(doc.parsedBody['deleted'], 0);

      doc = arango.GET_RAW(cmd);
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['count'], 'number');
      assertEqual(doc.parsedBody['count'], 18);
      assertEqual(typeof doc.parsedBody['figures']['cacheSize'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['cacheInUse'], 'boolean');
      assertEqual(typeof doc.parsedBody['figures']['cacheUsage'], 'number');
      assertEqual(typeof doc.parsedBody['figures']['documentsSize'], 'number');
      if (doc.parsedBody['figures']['cacheInUse']) {
        assertEqual(typeof doc.parsedBody['figures']['cacheLifeTimeHitRate'], 'double');
        assertEqual(typeof doc.parsedBody['figures']['cacheWindowedHitRate'], 'double');
      }
    },

    // get revision id
    test_extracting_the_revision_id_of_a_collection: function() {
      let cmd = api + "/" + cn + "/revision";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      let r1 = doc.parsedBody['revision'];
      assertEqual(typeof r1, 'string');
      assertTrue(r1.length > 0);

      // create a new document;
      let body = "{ \"test\" : 1 }";
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);

      // fetch revision again;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['revision'], 'string');

      let r2 = doc.parsedBody['revision'];
      assertTrue(r2.length > 0);
      assertNotEqual(r1, r2);;

      // create another document;
      doc = arango.POST_RAW("/_api/document/?collection=" + cn, body);

      // fetch revision again;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['revision'], 'string');

      let r3 = doc.parsedBody['revision'];
      assertTrue(r3.length > 0);
      assertNotEqual(r3, r1);
      assertNotEqual(r3, r2);

      // truncate;
      doc = arango.PUT_RAW("/_api/collection/${cn}/truncate", "");

      // fetch revision again;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['revision'], 'string');

      let r4 = doc.parsedBody['revision'];
      assertTrue(r4.length > 0);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// deleting of collection;
////////////////////////////////////////////////////////////////////////////////;
function deletingSuite () {
  let cn = "UnitTestsCollectionBasics";
  return {
    test_delete_an_existing_collection_by_identifier: function() {
      let cid = db._create(cn);
      let cmd = api + "/" + cn;
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);

      cmd = api + "/" + cn;
      doc = arango.GET_RAW(cmd);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 404);
    },

    test_delete_an_existing_collection_by_name: function() {
      let cid = db._create(cn);
      let cmd = api + "/" + cn;
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);

      cmd = api + "/" + cn;
      doc = arango.GET_RAW(cmd);

      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 404);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// creating a collection;
////////////////////////////////////////////////////////////////////////////////;
function creatingSuite () {
  let cn = "UnitTestsCollectionBasics";
  return {
    test_create_a_collection: function() {
      let cmd = api;
      let body = `{ \"name\" : \"${cn}\" }`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], 'string');
      assertEqual(doc.parsedBody['name'], cn);
      assertFalse(doc.parsedBody['waitForSync']);

      cmd = api + "/" + cn + "/figures";
      doc = arango.GET_RAW(cmd);

      assertFalse(doc.parsedBody['waitForSync']);

      db._drop(cn);
    },

    test_create_a_collection__sync: function() {
      let cmd = api;
      let body = `{ \"name\" : \"${cn}\", \"waitForSync\" : true }`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(typeof doc.parsedBody['id'], 'string');
      assertEqual(doc.parsedBody['name'], cn);
      assertTrue(doc.parsedBody['waitForSync']);

      cmd = api + "/" + cn + "/figures";
      doc = arango.GET_RAW(cmd);

      assertTrue(doc.parsedBody['waitForSync']);

      db._drop(cn);
    },

    test_create_a_collection__invalid_name: function() {
      let cmd = api;
      let body = "{ \"name\" : \"_invalid\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 400);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 400);
    },

    test_create_a_collection__already_existing: function() {
      db._drop(cn);
      let cmd = api;
      let body = `{ \"name\" : \"${cn}\" }`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);

      body = `{ \"name\" : \"${cn}\" }`;
      doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 409);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 409);

      db._drop(cn);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// load a collection;
////////////////////////////////////////////////////////////////////////////////;
function loadingSuite () {
  let cn = "UnitTestsCollectionBasics";
  return {
    test_load_a_collection_by_identifier: function() {
      db._drop(cn);
      let cid = db._create(cn);

      let cmd = api + "/" + cn + "/load";
      let doc = arango.PUT_RAW(cmd, '');

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertEqual(typeof doc.parsedBody['count'], 'number');
      assertEqual(doc.parsedBody['count'], 0);

      db._drop(cn);
    },

    test_load_a_collection_by_name: function() {
      db._drop(cn);
      let cid = db._create(cn);

      let cmd = api + "/" + cn + "/load";
      let doc = arango.PUT_RAW(cmd, '');

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertEqual(typeof doc.parsedBody['count'], 'number');
      assertEqual(doc.parsedBody['count'], 0);

      db._drop(cn);
    },

    test_load_a_collection_by_name_with_explicit_count: function() {
      db._drop(cn);
      let cid = db._create(cn);

      let docs = [];
      for (let i = 0; i < 10; i++){
        docs.push({ "hello" : "world"});
      }
      db[cn].save(docs);

      let cmd = api + "/" + cn + "/load";
      let body = "{ \"count\" : true }";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertEqual(typeof doc.parsedBody['count'], 'number');
      assertEqual(doc.parsedBody['count'], 10);

      db._drop(cn);
    },

    test_load_a_collection_by_name_without_count: function() {
      db._drop(cn);
      let cid = db._create(cn);

      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body);

      cmd = api + "/" + cn + "/load";
      body = "{ \"count\" : false }";
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertEqual(doc.parsedBody['count'], undefined);

      db._drop(cn);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// unloading a collection;
////////////////////////////////////////////////////////////////////////////////;

function unloadingSuite () {
  let cn = "UnitTestsCollectionBasics";
  return {
    test_unload_a_collection_by_identifier: function() {
      db._drop(cn);
      let cid = db._create(cn);

      let cmd = api + "/" + cn + "/unload";
      let doc = arango.PUT_RAW(cmd, '');

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);

      db._drop(cn);
    },

    test_unload_a_collection_by_name: function() {
      db._drop(cn);
      let cid = db._create(cn);

      let cmd = api + "/" + cn + "/unload";
      let doc = arango.PUT_RAW(cmd, '');

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);

      db._drop(cn);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// truncate a collection;
////////////////////////////////////////////////////////////////////////////////;
function truncatingSuite () {
  let cn = "UnitTestsCollectionBasics";
  let cid;
  return {
    setUp: function() {
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_truncate_a_collection_by_identifier: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let docs = [];
      for (let i = 0; i < 10; i++){
        docs.push({ "hello" : "world"});
      }
      db[cn].save(docs);

      assertEqual(cid.count(), 10);

      cmd = api + "/" + cn + "/truncate";
      let doc = arango.PUT_RAW(cmd, '');
      
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);

      assertEqual(cid.count(), 0);

      db._drop(cn);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// properties of a collection;
////////////////////////////////////////////////////////////////////////////////;
function propertiesSuite () {
  let cn = "UnitTestsCollectionBasics";
  return {
    tearDown: function() {
      db._drop(cn);
    },      
    test_changing_the_properties_of_a_collection_by_identifier: function() {
      let cid = db._create(cn);

      let cmd = `/_api/document?collection=${cid._id}`;
      let docs = [];
      for (let i = 0; i < 10; i++){
        docs.push({ "hello" : "world"});
      }
      db[cn].save(docs);

      assertEqual(cid.count(), 10);

      cmd = api + "/" + cn + "/properties";
      let body = "{ \"waitForSync\" : true }";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200, doc);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertTrue(doc.parsedBody['waitForSync']);
      assertFalse(doc.parsedBody['isSystem']);
      assertEqual(doc.parsedBody['keyOptions']['type'], "traditional");
      assertTrue(doc.parsedBody['keyOptions']['allowUserKeys']);

      cmd = api + "/" + cn + "/properties";
      body = "{ \"waitForSync\" : false }";
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertFalse(doc.parsedBody['waitForSync']);
      assertFalse(doc.parsedBody['isSystem']);
      assertEqual(doc.parsedBody['keyOptions']['type'], "traditional");
      assertTrue(doc.parsedBody['keyOptions']['allowUserKeys']);

    },

    test_create_collection_with_explicit_keyOptions_property__traditional_keygen: function() {
      let cmd = "/_api/collection";
      let body = `{ \"name\" : \"${cn}\", \"waitForSync\" : false, \"type\" : 2, \"keyOptions\" : {\"type\": \"traditional\", \"allowUserKeys\": true } }`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200, doc);
      let cid = doc.parsedBody['id'];

      cmd = api + "/" + cn + "/properties";
      body = "{ \"waitForSync\" : true }";
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200, doc);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertTrue(doc.parsedBody['waitForSync']);
      assertEqual(doc.parsedBody['keyOptions']['type'], "traditional");
      assertTrue(doc.parsedBody['keyOptions']['allowUserKeys']);

    },

    test_create_collection_with_empty_keyOptions_property: function() {
      let cmd = "/_api/collection";
      let body = `{ \"name\" : \"${cn}\", \"waitForSync\" : false, \"type\" : 2 }`;
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 200);
      let cid = doc.parsedBody['id'];

      cmd = api + "/" + cn + "/properties";
      body = "{ \"waitForSync\" : true }";
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200, doc);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid);
      assertEqual(doc.parsedBody['name'], cn);
      assertEqual(doc.parsedBody['status'], 3);
      assertTrue(doc.parsedBody['waitForSync']);
      assertEqual(doc.parsedBody['keyOptions']['type'], "traditional");
      assertTrue(doc.parsedBody['keyOptions']['allowUserKeys']);
    }
  };
}

jsunity.run(all_collectionsSuite);
jsunity.run(error_handlingSuite);
jsunity.run(schema_validationSuite);
jsunity.run(readingSuite);
jsunity.run(deletingSuite);
jsunity.run(creatingSuite);
jsunity.run(loadingSuite);
jsunity.run(unloadingSuite);
jsunity.run(truncatingSuite);
jsunity.run(propertiesSuite);

return jsunity.done();
