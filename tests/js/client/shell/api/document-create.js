/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertTypeOf, assertNotUndefined, assertNotEqual */

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";

const jsunity = require("jsunity");
let didRegex = /^([0-9a-zA-Z]+)\/([0-9a-zA-Z\-_]+)/;


////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function error_handlingSuite () {
  return {
    test_returns_an_error_if_collection_idenifier_is_missing: function() {
      let cmd = "/_api/document";
      let body = "{}";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_COLLECTION_PARAMETER_MISSING.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_the_collection_identifier_is_unknown: function() {
      let cmd = "/_api/document?collection=123456";
      let body = "{}";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_the_collection_name_is_unknown: function() {
      let cmd = "/_api/document?collection=unknown_collection";
      let body = "{}";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_the_JSON_body_is_corrupted: function() {
      let cn = "UnitTestsCollectionBasics";
      let id = db._create(cn);

      try {
        let cmd = `/_api/document?collection=${id._id}`;
        let body = "{ 1 : 2 }";
        let doc = arango.POST_RAW(cmd, body);
        
        assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
        assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertEqual(doc.headers['content-type'], contentType);
        
        assertEqual(db[cn].count(), 0);
      } finally {
        db._drop(cn);
      }
    },

    test_returns_an_error_if_an_object_sub_attribute_in_the_JSON_body_is_corrupted: function() {
      let cn = "UnitTestsCollectionBasics";
      let id = db._create(cn);

      try {
        let cmd = `/_api/document?collection=${id._id}`;
        let body = "{ \"foo\" : { \"bar\" : \"baz\", \"blue\" : moo } }";
        let doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
        assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertEqual(doc.headers['content-type'], contentType);

        assertEqual(db[cn].count(), 0);
      } finally {
        db._drop(cn);
      }
    },

    test_returns_an_error_if_an_array_attribute_in_the_JSON_body_is_corrupted: function() {
      let cn = "UnitTestsCollectionBasics";
      let id = db._create(cn);
      try {
        let cmd = `/_api/document?collection=${id._id}`;
        let body = "{ \"foo\" : [ 1, 2, \"bar\", moo ] }";
        let doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
        assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertEqual(doc.headers['content-type'], contentType);

        assertEqual(db[cn].count(), 0);

      } finally {
        db._drop(cn);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// known collection identifier, waitForSync = true;
////////////////////////////////////////////////////////////////////////////////;
function known_collection_identifier__waitForSync_EQ_trueSuite () {
  let cn = "UnitTestsCollectionBasics";
  return {
    setUp: function() {
      let cid = db._create(cn, { waitForSync: true } );
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_creating_a_new_document__waitForSyncEQtrue: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE_RAW(location);
      
      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document__setting_compatibility_header__waitForSyncEQtrue: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], `${cn}`);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document_complex_body__waitForSyncEQtrue: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "Hallo" : "Wo\"rld" };
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      cmd = `/_api/document/${did}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['Hallo'], 'Wo"rld');

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document_complex_body__setting_compatibility_header__waitForSyncEQtrue_: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"Wo\\\"rld\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      cmd = `/_api/document/${did}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['Hallo'], 'Wo"rld');

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_umlaut_document__waitForSyncEQtrue: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"öäüÖÄÜßあ寿司\" }";
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      cmd = `/_api/document/${did}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      assertEqual(doc.parsedBody['Hallo'], "öäüÖÄÜßあ寿司");
      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_umlaut_document__setting_compatibility_header__waitForSyncEQtrue: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"öäüÖÄÜßあ寿司\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      cmd = `/_api/document/${did}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      assertEqual(doc.parsedBody['Hallo'], 'öäüÖÄÜßあ寿司');

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_not_normalized_umlaut_document__waitForSyncEQtrue: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"Grüß Gott.\" }";
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      cmd = `/_api/document/${did}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      assertEqual(doc.parsedBody['Hallo'], "Grüß Gott.");

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_not_normalized_umlaut_document__setting_compatibility_header__waitForSyncEQtrue: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"Grüß Gott.\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      cmd = `/_api/document/${did}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      //let newBody = doc.body();
      //newBody = newBody.sub!(/^.*"Hallo":"([^"]*)".*$/, '\1');

      //assertEqual(newBody, "Gr\\u00FC\\u00DF Gott.");

      assertEqual(doc.parsedBody['Hallo'], "Grüß Gott.");

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_document_with_an_existing_id__waitForSyncEQtrue: function() {
      let key = "a_new_key";

      arango.DELETE(`/_api/document/${cn}/${key}`);

      let cmd = `/_api/document?collection=${cn}`;
      let body = { "some stuff" : "goes here", "_key" : key };
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');
      assertEqual(did, `${cn}/${key}`);

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE(`/_api/document/${cn}/${key}`);
    },

    test_creating_a_document_with_an_existing_id__setting_compatibility_header__waitForSyncEQtrue: function() {
      let key = "a_new_key";

      arango.DELETE(`/_api/document/${cn}/${key}`);

      let cmd = `/_api/document?collection=${cn}`;
      let body = { "some stuff" : "goes here", "_key" : key };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');
      assertEqual(did, `${cn}/${key}`);

      let match = didRegex.exec(did);

      assertEqual(match[1], `${cn}`);

      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE(`/_api/document/${cn}/${key}`);
    },

    test_creating_a_document_with_a_duplicate_existing_id__waitForSyncEQtrue: function() {
      let key = "a_new_key";

      arango.DELETE(`/_api/document/${cn}/${key}`);

      let cmd = `/_api/document?collection=${cn}`;
      let body = { "some stuff" : "goes here", "_key" : key };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      // send again;
      doc = arango.POST_RAW(cmd, body);
      assertEqual(doc.code, 409); // conflict
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 409);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
      
      arango.DELETE_RAW(`/_api/document/${cn}/${key}`);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// known collection identifier, waitForSync = false;
////////////////////////////////////////////////////////////////////////////////;
function known_collection_identifier__waitForSync_EQ_falseSuite () {
  let cn = "UnitTestsCollectionUnsynced";
  return {
    setUp: function() {
      let cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_creating_a_new_document__waitForsync_EQ_False: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document__setting_compatibility_header__waitForsync_EQ_False: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document__waitForSync_URL_param_EQ_false: function() {
      let cmd = `/_api/document?collection=${cn}&waitForSync=false`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document__waitForSync_URL_param_EQ_false__setting_compatibility_header: function() {
      let cmd = `/_api/document?collection=${cn}&waitForSync=false`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document__waitForSync_URL_param_EQ_true: function() {
      let cmd = `/_api/document?collection=${cn}&waitForSync=true`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document__waitForSync_URL_param_EQ_true__setting_compatibility_header: function() {
      let cmd = `/_api/document?collection=${cn}&waitForSync=true`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `"${rev}"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document_with_duplicate_attribute_names: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"a\" : 1, \"a\": 2 }";
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_creating_a_new_document_with_duplicate_attribute_names_in_nested_object: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"outer\" : { \"a\" : 1, \"a\": 2 } }";
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// known collection name;
////////////////////////////////////////////////////////////////////////////////;
function known_collection_nameSuite () {
  let cn = "UnitTestsCollectionBasics";
  return {
    setUp: function() {
      db._create(cn, { waitForSync: true});
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_creating_a_new_document: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201, doc);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `\"${rev}\"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },

    test_creating_a_new_document__setting_compatibility_header: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201, doc);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `\"${rev}\"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// unknown collection name;
////////////////////////////////////////////////////////////////////////////////;
function unknown_collection_nameSuite () {
  let cn = `UnitTestsCollectionNamed${internal.time()}`;
  return {
    tearDown: function() {
      db._drop(cn);
    },

    test_returns_an_error_if_collection_is_unknown: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////;
// known collection identifier, overwrite = true;
////////////////////////////////////////////////////////////////////////////////;
function known_collection_identifier__overwrite_EQ_trueSuite () {
  let cn = "UnitTestsCollectionUnsynced";
  let cid;
  return {
    setUp: function() {
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_replace_a_document_by__key: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let key = doc.parsedBody['_key'];
      assertEqual(typeof key, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `\"${rev}\"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      cmd = `/_api/document?collection=${cn}&overwrite=true&waitForSync=false&returnOld=true`;
      body = `{ "_key" : "${key}",  "Hallo" : "ULF" }`;
      let newdoc = arango.POST_RAW(cmd, body, {});

      let newrev = newdoc.parsedBody['_rev'];
      assertEqual(typeof newrev, 'string');
      assertNotEqual(newrev, rev);

      let newoldrev = newdoc.parsedBody['_oldRev'];
      assertEqual(typeof newoldrev, 'string');
      assertEqual(newoldrev, rev);

      newoldrev = newdoc.parsedBody['old']['Hallo'];
      assertEqual(typeof newoldrev, 'string');
      assertEqual(newoldrev, "World");

      let newkey = newdoc.parsedBody['_key'];
      assertEqual(typeof newkey, 'string');
      assertEqual(newkey, key);
      assertEqual(newdoc.code, 202);

      arango.DELETE(location);
      assertEqual(db[cn].count(), 0);
    },

    test_replace_a_document_by__key__return_new___old: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let key = doc.parsedBody['_key'];
      assertEqual(typeof key, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `\"${rev}\"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      cmd = `/_api/document?collection=${cn}&overwrite=true&returnNew=true&returnOld=true&waitForSync=true`;
      body = `{ "_key" : "${key}",  "Hallo" : "ULF" }`;
      let newdoc = arango.POST_RAW(cmd, body, {});

      let newrev = newdoc.parsedBody['_rev'];
      assertEqual(typeof newrev, 'string');
      assertNotEqual(newrev, rev);

      let newoldrev = newdoc.parsedBody['_oldRev'];
      assertEqual(typeof newoldrev, 'string');
      assertEqual(newoldrev, rev);

      let newkey = newdoc.parsedBody['_key'];
      assertEqual(typeof newkey, 'string');
      assertEqual(newkey, key);

      let newnew = newdoc.parsedBody['new'];
      assertEqual(typeof newnew["_key"], 'string');
      assertEqual(newnew["_key"], key);
      assertEqual(newnew["_rev"], newrev);
      assertEqual(newnew["Hallo"], "ULF");

      let newold = newdoc.parsedBody['old'];
      assertEqual(newold["_key"], key);
      assertEqual(newold["_rev"], newoldrev);

      assertEqual(typeof newold["Hallo"], 'string');
      assertEqual(newold["Hallo"], "World");

      assertEqual(newdoc.code, 201);

      arango.DELETE_RAW(location);
      assertEqual(db[cn].count(), 0);
    },

    test_replace_documents_by__key: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body, {});

      assertEqual(doc.code, 202);
      assertEqual(doc.headers['content-type'], contentType);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let rev = doc.parsedBody['_rev'];
      assertEqual(typeof rev, 'string');

      let did = doc.parsedBody['_id'];
      assertEqual(typeof did, 'string');

      let key = doc.parsedBody['_key'];
      assertEqual(typeof key, 'string');

      let match = didRegex.exec(did);

      assertEqual(match[1], cn);

      assertEqual(etag, `\"${rev}\"`);
      assertEqual(location, `/_db/_system/_api/document/${did}`);

      cmd = `/_api/document?collection=${cn}&overwrite=true&returnNew=true&returnOld=true&waitForSync=true`;
      body = `[{ "_key" : "${key}",  "Hallo" : "ULF" }, { "_key" : "${key}",  "Hallo" : "ULFINE" }]`;
      let newdoc = arango.POST_RAW(cmd, body, {});

      let newrev = newdoc.parsedBody[0]['_rev'];
      assertEqual(typeof newrev, 'string', newdoc.parsedBody);
      assertNotEqual(newrev, rev);

      let newoldrev = newdoc.parsedBody[0]['_oldRev'];
      assertEqual(typeof newoldrev, 'string');
      assertEqual(newoldrev, rev);

      let newkey = newdoc.parsedBody[0]['_key'];
      assertEqual(typeof newkey, 'string');
      assertEqual(newkey, key);

      let newnew = newdoc.parsedBody[0]['new'];
      assertEqual(typeof newnew["_key"], 'string');
      assertEqual(newnew["_key"], key);
      assertEqual(newnew["_rev"], newrev);

      let newold = newdoc.parsedBody[0]['old'];
      assertEqual(newold["_key"], key);
      assertEqual(newold["_rev"], newoldrev);

      newrev = newdoc.parsedBody[1]['_rev'];
      assertEqual(typeof newrev, 'string');
      assertNotEqual(newrev, rev);

      assertEqual(newdoc.parsedBody[1]['new']['Hallo'], "ULFINE");
      assertEqual(newdoc.parsedBody[1]['old']['Hallo'], "ULF");


      assertEqual(newdoc.code, 201);

      arango.DELETE_RAW(location);
      assertEqual(db[cn].count(), 0);
    }
  };
}

jsunity.run(error_handlingSuite);
jsunity.run(known_collection_identifier__waitForSync_EQ_trueSuite);
jsunity.run(known_collection_identifier__waitForSync_EQ_falseSuite);
jsunity.run(known_collection_nameSuite);
jsunity.run(unknown_collection_nameSuite);
jsunity.run(known_collection_identifier__overwrite_EQ_trueSuite);
return jsunity.done();
