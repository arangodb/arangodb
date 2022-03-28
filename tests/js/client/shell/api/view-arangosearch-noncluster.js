/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined */

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


let api = "/_api/view";

////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function creationSuite () {
  return {
    tearDown: function() {
      arango.DELETE_RAW(api + '/test');
      arango.DELETE_RAW(api + '/dup');
    },

    test_creating_a_view_without_body: function() {
      let cmd = api;
      let doc = arango.POST_RAW(cmd, "");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_creating_a_view_without_name: function() {
      let cmd = api;
      let body = { "type": "arangosearch", "properties": {} };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_creating_a_view_without_type: function() {
      let cmd = api;
      let body = { "name": "abc", "properties": {} };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_creating_a_view_with_invalid_type: function() {
      let cmd = api;
      let body = { "name": "test", "type": "foobar", "properties": {} };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_creating_a_view_without_properties: function() {
      let cmd = api;
      let body = { "name": "test", "type": "arangosearch" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['name'], "test");
      assertEqual(doc.parsedBody['type'], "arangosearch");

      let cmd2 = api + '/test';
      let doc2 = arango.DELETE_RAW(cmd2);

      assertEqual(doc2.code, 200);
      assertEqual(doc2.headers['content-type'], contentType);
    },

    test_duplicate_name: function() {
      let cmd1 = api;
      let body1 = { "name": "dup", "type": "arangosearch", "properties": {} };
      let doc1 = arango.POST_RAW(cmd1, body1);

      assertEqual(doc1.code, 201);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], "dup");
      assertEqual(doc1.parsedBody['type'], "arangosearch");

      let cmd2 = api;
      let body2 = { "name": "dup", "type": "arangosearch", "properties": {} };
      let doc2 = arango.POST_RAW(cmd2, body2);

      assertEqual(doc2.code, internal.errors.ERROR_HTTP_CONFLICT.code);
      assertEqual(doc2.headers['content-type'], contentType);
      assertTrue(doc2.parsedBody['error']);
      assertEqual(doc2.parsedBody['code'], internal.errors.ERROR_HTTP_CONFLICT.code);
      assertEqual(doc2.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DUPLICATE_NAME.code);

      let cmd3 = api + '/dup';
      let doc3 = arango.DELETE_RAW(cmd3);

      assertEqual(doc3.code, 200);
      assertEqual(doc3.headers['content-type'], contentType);
    }
  };
}
function renameSuite () {
  return {
    tearDown: function() {
      arango.DELETE_RAW(api + '/testView');
      arango.DELETE_RAW(api + '/newName');
      arango.DELETE_RAW(api + '/dup');
      arango.DELETE_RAW(api + '/dup2');
    },

    test_valid_change: function() {
      let cmd1 = api;
      let body1 = { "name": "testView", "type": "arangosearch", "properties": {} };
      let doc1 = arango.POST_RAW(cmd1, body1);

      assertEqual(doc1.code, 201);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], "testView");
      assertEqual(doc1.parsedBody['type'], "arangosearch");

      let cmd2 = api + '/testView/rename';
      let body2 = { "name": "newName" };
      let doc2 = arango.PUT_RAW(cmd2, body2);

      assertEqual(doc2.code, 200);
      assertEqual(doc2.headers['content-type'], contentType);
      assertEqual(doc2.parsedBody['name'], 'newName');

      let cmd3 = api + '/newName';
      let doc3 = arango.DELETE_RAW(cmd3);

      assertEqual(doc3.code, 200);
      assertEqual(doc3.headers['content-type'], contentType);
    },

    test_same_name: function() {
      let cmd1 = api;
      let body1 = { "name": "testView", "type": "arangosearch", "properties": {} };
      let doc1 = arango.POST_RAW(cmd1, body1);

      assertEqual(doc1.code, 201);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], "testView");
      assertEqual(doc1.parsedBody['type'], "arangosearch");

      let cmd2 = api + '/testView/rename';
      let body2 = { "name": "testView" };
      let doc2 = arango.PUT_RAW(cmd2, body2);

      assertEqual(doc2.code, 200);
      assertEqual(doc2.headers['content-type'], contentType);
      assertEqual(doc2.parsedBody['name'], 'testView');

      let cmd3 = api + '/testView';
      let doc3 = arango.DELETE_RAW(cmd3);

      assertEqual(doc3.code, 200);
      assertEqual(doc3.headers['content-type'], contentType);
    },

    test_invalid_name: function() {
      let cmd1 = api;
      let body1 = { "name": "testView", "type": "arangosearch", "properties": {} };
      let doc1 = arango.POST_RAW(cmd1, body1);

      assertEqual(doc1.code, 201);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], "testView");
      assertEqual(doc1.parsedBody['type'], "arangosearch");

      let cmd2 = api + '/testView/rename';
      let body2 = { "name": "g@rb@ge" };
      let doc2 = arango.PUT_RAW(cmd2, body2);

      assertEqual(doc2.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc2.headers['content-type'], contentType);
      assertTrue(doc2.parsedBody['error']);
      assertEqual(doc2.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc2.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_ILLEGAL_NAME.code);

      let cmd3 = api + '/testView';
      let doc3 = arango.DELETE_RAW(cmd3);

      assertEqual(doc3.code, 200);
      assertEqual(doc3.headers['content-type'], contentType);
    },

    test_ren_duplicate_name: function() {
      let cmd1 = api;
      let body1 = { "name": "dup", "type": "arangosearch", "properties": {} };
      let doc1 = arango.POST_RAW(cmd1, body1);

      assertEqual(doc1.code, 201);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], "dup");
      assertEqual(doc1.parsedBody['type'], "arangosearch");

      let cmd2 = api;
      let body2 = { "name": "dup2", "type": "arangosearch", "properties": {} };
      let doc2 = arango.POST_RAW(cmd2, body2);

      assertEqual(doc2.code, 201);
      assertEqual(doc2.headers['content-type'], contentType);
      assertEqual(doc2.parsedBody['name'], "dup2");
      assertEqual(doc2.parsedBody['type'], "arangosearch");

      let cmd3 = api + '/dup2/rename';
      let body3 = { "name": "dup" };
      let doc3 = arango.PUT_RAW(cmd3, body3);

      assertEqual(doc3.code, internal.errors.ERROR_HTTP_CONFLICT.code);
      assertEqual(doc3.headers['content-type'], contentType);
      assertTrue(doc3.parsedBody['error']);
      assertEqual(doc3.parsedBody['code'], internal.errors.ERROR_HTTP_CONFLICT.code);
      assertEqual(doc3.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DUPLICATE_NAME.code);

      let cmd4 = api + '/dup';
      let doc4 = arango.DELETE_RAW(cmd4);

      assertEqual(doc4.code, 200);
      assertEqual(doc4.headers['content-type'], contentType);

      let cmd5 = api + '/dup2';
      let doc5 = arango.DELETE_RAW(cmd5);

      assertEqual(doc5.code, 200);
      assertEqual(doc5.headers['content-type'], contentType);
    }
  };
}

function deletionSuite () {
  return {
    test_deleting_a_non_existent_view: function() {
      let cmd = api + "/foobar";
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    }
  };
}

function retrievalSuite () {
  return {
    test_getting_a_non_existent_view: function() {
      let cmd = api + "/foobar";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },

    test_getting_properties_of_a_non_existent_view: function() {
      let cmd = api + "/foobar/properties";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    }
  };
}

function modificationSuite () {
  return {
    tearDown: function() {
      arango.DELETE_RAW(api + '/lemon');
    },

    test_modifying_view_directly__not_properties: function() {
      let cmd = api + "/foobar";
      let body = { "level": "DEBUG" };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_BAD_PARAMETER.code);
    },

    test_modifying_a_non_existent_view: function() {
      let cmd = api + "/foobar/properties";
      let body = { "level": "DEBUG" };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    },

    test_modifying_a_view_with_unacceptable_properties: function() {
      let cmd1 = api;
      let body1 = { "name": "lemon", "type": "arangosearch", "properties": {} };
      let doc1 = arango.POST_RAW(cmd1, body1);

      assertEqual(doc1.code, 201);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], "lemon");
      assertEqual(doc1.parsedBody['type'], "arangosearch");

      let cmd2 = api + '/lemon/properties';
      let body2 = { "bogus": "junk", "consolidationIntervalMsec": 17 };
      let doc2 = arango.PUT_RAW(cmd2, body2);
      assertEqual(doc2.code, 200);
      assertEqual(doc2.headers['content-type'], contentType);
      assertEqual(doc2.parsedBody['name'], "lemon");
      assertEqual(doc2.parsedBody['type'], "arangosearch");
      assertEqual(doc2.parsedBody['id'], doc1.parsedBody['id']);

      let cmd3 = api + '/lemon/properties';
      let doc4 = arango.GET_RAW(cmd3);
      assertEqual(doc4.parsedBody['consolidationIntervalMsec'], 17);

      let cmd4 = api + '/lemon';
      doc4 = arango.DELETE_RAW(cmd4);

      assertEqual(doc4.code, 200);
      assertEqual(doc4.headers['content-type'], contentType);
    }
  };
}

function add_dropSuite () {
  return {
    tearDown: function() {
      arango.DELETE_RAW(api + '/abc');
    },

    test_creating_a_view: function() {
      let cmd = api;
      let body = { "name": "abc", "type": "arangosearch", "properties": {} };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['name'], "abc");
      assertEqual(doc.parsedBody['type'], "arangosearch");
    },

    test_dropping_a_view: function() {
      let cmd = api;
      let body = { "name": "abc", "type": "arangosearch", "properties": {} };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let cmd1 = api + "/abc";
      let doc1 = arango.DELETE_RAW(cmd1);

      assertEqual(doc1.code, 200);
      assertEqual(doc1.headers['content-type'], contentType);

      let cmd2 = api + "/abc";
      let doc2 = arango.GET_RAW(cmd2);

      assertEqual(doc2.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc2.headers['content-type'], contentType);
      assertTrue(doc2.parsedBody['error']);
      assertEqual(doc2.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    }
  };
}

function add_with_link_to_non_existing_collectionSuite () {
  return {
    tearDown: function() {
      arango.DELETE_RAW(api + '/abc');
    },

    test_creating_a_view_with_link_to_non_existing_collection: function() {
      let cmd = api;
      let body = { "name": "abc", "type": "arangosearch", "links": { "wrong" : {} } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.parsedBody['errorMessage'], "while validating arangosearch link definition, error: collection 'wrong' not found");
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
    }
  };
}

function add_with_link_to_existing_collectionSuite () {
  let cn = "right";
  return {

    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      arango.DELETE_RAW(api + '/abc');
      db._drop(cn);
    },

    test_creating_a_view_with_link_to_existing_collection_drop: function() {
      let cmd = api;
      let body = {
        "name": "abc",
        "type": "arangosearch",
        "links": { "right" : { "includeAllFields": true } }
      };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['name'], "abc");
      assertEqual(doc.parsedBody['type'], "arangosearch");
      assertEqual(typeof doc.parsedBody['links']['right'], 'object');
    },

    test_dropping_a_view_with_link: function() {
      let cmd = api;
      let body ={
        "name": "abc",
        "type": "arangosearch",
        "links": { "right" : { "includeAllFields": true } }
      };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let cmd1 = api + "/abc";
      let doc1 = arango.DELETE_RAW(cmd1);

      assertEqual(doc1.code, 200);
      assertEqual(doc1.headers['content-type'], contentType);

      let cmd2 = api + "/abc";
      let doc2 = arango.GET_RAW(cmd2);

      assertEqual(doc2.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc2.headers['content-type'], contentType);
      assertTrue(doc2.parsedBody['error']);
      assertEqual(doc2.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
    }
  };
}

function retrieval_with_linSuite () {
  return {
    tearDown: function() {
      arango.DELETE_RAW(api + '/abc');
      arango.DELETE_RAW(api + '/def');
    },

    test_empty_list: function() {
      let cmd = api;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['result'], []);
    },

    test_short_list: function() {
      let cmd1 = api;
      let body1 ={
        "name": "abc",
        "type": "arangosearch",
        "consolidationIntervalMsec": 10
      };
      let doc1 = arango.POST_RAW(cmd1, body1);
      assertEqual(doc1.code, 201);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], "abc");
      assertEqual(doc1.parsedBody['type'], "arangosearch");

      let cmd2 = api;
      let body2 = {
        "name": "def",
        "type": "arangosearch",
        "consolidationIntervalMsec": 10
      };
      let doc2 = arango.POST_RAW(cmd2, body2);
      assertEqual(doc2.code, 201);
      assertEqual(doc2.headers['content-type'], contentType);
      assertEqual(doc2.parsedBody['name'], "def");
      assertEqual(doc2.parsedBody['type'], "arangosearch");

      let cmd3 = api;
      let doc3 = arango.GET_RAW(cmd3);
      assertEqual(doc3.code, 200);
      assertEqual(doc3.headers['content-type'], contentType);
      assertFalse(doc3.parsedBody['error']);
      assertEqual(doc3.parsedBody['result'][0]['name'], "abc");
      assertEqual(doc3.parsedBody['result'][1]['name'], "def");
      assertEqual(doc3.parsedBody['result'][0]['type'], "arangosearch");
      assertEqual(doc3.parsedBody['result'][1]['type'], "arangosearch");
    },

    test_individual_views_with_link: function() {
      let cmd1 = api;
      let body1 = {
        "name": "abc",
        "type": "arangosearch",
        "consolidationIntervalMsec": 10
      };
      let doc1 = arango.POST_RAW(cmd1, body1);
      assertEqual(doc1.code, 201);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], "abc");
      assertEqual(doc1.parsedBody['type'], "arangosearch");

      let cmd2 = api;
      let body2 = {
        "name": "def",
        "type": "arangosearch",
        "consolidationIntervalMsec": 10
      };
      let doc2 = arango.POST_RAW(cmd2, body2);
      assertEqual(doc2.code, 201);

      cmd1 = api + '/abc';
      doc1 = arango.GET_RAW(cmd1);
      assertEqual(doc1.code, 200);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], "abc");
      assertEqual(doc1.parsedBody['type'], "arangosearch");

      cmd2 = api + '/abc/properties';
      doc2 = arango.GET_RAW(cmd2);
      assertEqual(doc2.code, 200);
      assertEqual(doc2.headers['content-type'], contentType);
      assertEqual(doc2.parsedBody['consolidationIntervalMsec'], 10);

      let cmd3 = api + '/def';
      let doc3 = arango.GET_RAW(cmd3);
      assertEqual(doc3.code, 200);
      assertEqual(doc3.headers['content-type'], contentType);
      assertEqual(doc3.parsedBody['name'], "def");
      assertEqual(doc3.parsedBody['type'], "arangosearch");

      let cmd4 = api + '/def/properties';
      let doc4 = arango.GET_RAW(cmd4);
      assertEqual(doc4.code, 200);
      assertEqual(doc4.headers['content-type'], contentType);
      assertEqual(doc4.parsedBody['consolidationIntervalMsec'], 10);
    }
  };
}

function modificationSuite2 () {
  return {
    tearDown: function() {
      arango.DELETE_RAW(api + '/abc');
    },

    test_change_properties: function() {
      let cmd1 = api;
      let body1 = {
        "name": "abc",
        "type": "arangosearch",
        "consolidationIntervalMsec": 10
      };
      let doc1 = arango.POST_RAW(cmd1, body1);
      assertEqual(doc1.code, 201);

      cmd1 = api + '/abc';
      doc1 = arango.GET_RAW(cmd1);
      assertEqual(doc1.code, 200);

      cmd1 = api + '/abc/properties';
      body1 = { "consolidationIntervalMsec": 7 };
      doc1 = arango.PUT_RAW(cmd1, body1);
      assertEqual(doc1.code, 200);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], 'abc');
      assertEqual(doc1.parsedBody['type'], 'arangosearch');
      assertEqual(typeof doc1.parsedBody, 'object');

      let cmd2 = api + '/abc/properties';
      let doc2 = arango.GET_RAW(cmd2);
      assertEqual(doc2.code, 200);
      assertEqual(doc2.headers['content-type'], contentType);
      assertEqual(doc2.parsedBody['consolidationIntervalMsec'], 7);
    },

    test_ignore_extra_properties: function() {
      let cmd1 = api;
      let body1 = {
        "name": "abc",
        "type": "arangosearch",
        "consolidationIntervalMsec": 10
      };
      let doc1 = arango.POST_RAW(cmd1, body1);
      assertEqual(doc1.code, 201);

      cmd1 = api + '/abc';
      doc1 = arango.GET_RAW(cmd1);
      assertEqual(doc1.code, 200);

      cmd1 = api + '/abc/properties';
      body1 = { "consolidationIntervalMsec": 10, "extra": "foobar" };
      doc1 = arango.PUT_RAW(cmd1, body1);
      assertEqual(doc1.code, 200);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], 'abc');
      assertEqual(doc1.parsedBody['type'], 'arangosearch');
      assertEqual(typeof doc1.parsedBody, 'object');
      assertEqual(doc1.parsedBody['extra'], undefined);

      let cmd2 = api + '/abc/properties';
      let doc2 = arango.GET_RAW(cmd2);
      assertEqual(doc2.code, 200);
      assertEqual(doc2.headers['content-type'], contentType);
      assertEqual(doc2.parsedBody['consolidationIntervalMsec'], 10);
      assertEqual(doc2.parsedBody['extra'], undefined);
    },

    test_accept_updates_via_PATCH_as_well: function() {
      let cmd1 = api;
      let body1 = {
        "name": "abc",
        "type": "arangosearch",
        "consolidationIntervalMsec": 10
      };
      let doc1 = arango.POST_RAW(cmd1, body1);
      assertEqual(doc1.code, 201);

      cmd1 = api + '/abc/properties';
      body1 = { "consolidationIntervalMsec": 3 };
      doc1 = arango.PATCH_RAW(cmd1, body1);
      assertEqual(doc1.code, 200);
      assertEqual(doc1.headers['content-type'], contentType);
      assertEqual(doc1.parsedBody['name'], 'abc');
      assertEqual(doc1.parsedBody['type'], 'arangosearch');
      assertEqual(typeof doc1.parsedBody, 'object');

      let cmd2 = api + '/abc/properties';
      let doc2 = arango.GET_RAW(cmd2);
      assertEqual(doc2.code, 200);
      assertEqual(doc2.headers['content-type'], contentType);
      assertEqual(doc2.parsedBody['consolidationIntervalMsec'], 3);
    }
  };
}

jsunity.run(creationSuite);
jsunity.run(renameSuite);
jsunity.run(deletionSuite);
jsunity.run(retrievalSuite);
jsunity.run(modificationSuite);
jsunity.run(add_dropSuite);
jsunity.run(add_with_link_to_non_existing_collectionSuite);
jsunity.run(add_with_link_to_existing_collectionSuite);
jsunity.run(retrieval_with_linSuite);
jsunity.run(modificationSuite2);
return jsunity.done();
