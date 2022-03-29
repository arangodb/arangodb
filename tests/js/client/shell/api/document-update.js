/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertNotEqual */

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

////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function error_handlingSuite () {
  let cn = "UnitTestsCollectionBasics";
  let cid;
  return {
    setUp: function() {
      cid = db._create(cn, { waitForSync: true } );
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_an_error_if_document_identifier_is_missing: function() {
      let cmd = "/_api/document";
      let body = "{}";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertEqual(db[cn].count(), 0);
    },

    test_returns_an_error_if_document_identifier_is_corrupted: function() {
      let cmd = "/_api/document/123456";
      let body = "{}";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertEqual(db[cn].count(), 0);
    },

    test_returns_an_error_if_document_identifier_is_corrupted_with_empty_cid: function() {
      let cmd = "/_api/document//123456";
      let body = "{}";
      let doc = arango.PUT_RAW(cmd, body);

      if (arango.protocol() === 'vst') {
        assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
        assertEqual(doc.headers['content-type'], contentType);
      } else {
        assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code);
        assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertEqual(doc.headers['content-type'], contentType);
      }

      assertEqual(db[cn].count(), 0);
    },

    test_returns_an_error_if_collection_identifier_is_unknown: function() {
      let cmd = "/_api/document/123456/234567";
      let body = "{}";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertEqual(db[cn].count(), 0);
    },

    test_returns_an_error_if_document_identifier_is_unknown: function() {
      let cmd = `/_api/document/${cid._id}/234567`;
      let body = "{}";
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertEqual(db[cn].count(), 0);
    },

    test_returns_an_error_if_the_policy_parameter_is_bad: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document, different revision;
      cmd = "/_api/document/${did}?policy=last-write";
      let hdr = { "if-match": `"388576${rev}"` };
      doc = arango.PUT_RAW(cmd, "", hdr);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.headers['content-type'], contentType);

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_create_a_document_and_update_it_with_invalid_JSON: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);
      let did = doc.parsedBody['_id'];

      // update document;
      cmd = `/_api/document/${did}`;
      body = '{ "World" : "Hallo\xff", "World": "blub" }';
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code, doc);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
    },

    test_create_a_document_and_replace_it__using_ignoreRevsEQfalse: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document, different revision;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_rev": "658993", "World" : "Hallo" };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 412);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      // update document, same revision;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_rev": rev, "World" : "Hallo" };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      cmd = `/_api/collection/${cid._id}/properties`;
      body = { "waitForSync" : false };
      doc = arango.PUT_RAW(cmd, body);

      // wait for dbservers to pick up the change;
      sleep(2);

      // update document ;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_rev": rev2, "World" : "Hallo2" };
      let doc3 = arango.PUT_RAW(cmd, body);

      assertEqual(doc3.code, 202);
      assertEqual(doc3.headers['content-type'], contentType);
      let rev3 = doc3.parsedBody['_rev'];

      // update document ;
      cmd = `/_api/document/${did}?ignoreRevs=false&waitForSync=true`;
      body = { "_rev": rev3, "World" : "Hallo3" };
      let doc4 = arango.PUT_RAW(cmd, body);

      assertEqual(doc4.code, 201);
      assertEqual(doc4.headers['content-type'], contentType);
      let rev4 = doc4.parsedBody['_rev'];

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_create_a_document_and_replace_it__using_ignoreRevsEQfalse_with_key: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "_key" : "hello", "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document, different revision;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_key" : "hello", "_rev" : "658993", "World" : "Hallo" };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      // update document, same revision;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_key" : "hello", "_rev": rev, "World" : "Hallo" };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      cmd = `/_api/collection/${cid._id}/properties`;
      body = { "waitForSync" : false };
      doc = arango.PUT_RAW(cmd, body);

      // wait for dbservers to pick up the change;
      sleep(2);

      // update document ;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_key" : "hello", "_rev": rev2, "World" : "Hallo2" };
      let doc3 = arango.PUT_RAW(cmd, body);

      assertEqual(doc3.code, 202);
      assertEqual(doc3.headers['content-type'], contentType);
      let rev3 = doc3.parsedBody['_rev'];

      // update document ;
      cmd = `/_api/document/${did}?ignoreRevs=false&waitForSync=true`;
      body = { "_key" : "hello", "_rev": rev3, "World" : "Hallo3" };
      let doc4 = arango.PUT_RAW(cmd, body);

      assertEqual(doc4.code, 201);
      assertEqual(doc4.headers['content-type'], contentType);
      let rev4 = doc4.parsedBody['_rev'];

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// updating documents;
////////////////////////////////////////////////////////////////////////////////;
function updating_documentSuite () {
  let cn = "UnitTestsCollectionBasics";
  let cid;
  return {
    setUp: function() {
      cid = db._create(cn, { waitForSync: true } );
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_create_a_document_and_update_it: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document;
      cmd = `/_api/document/${did}`;
      body = { "World" : "Hallo" };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },

    test_create_a_document_and_update_it__using_if_match: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // replace document, different revision;
      cmd = `/_api/document/${did}`;
      let hdr = { "if-match": '"658993"' };
      body = { "World" : "Hallo" };
      doc = arango.PUT_RAW(cmd, body, hdr);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      // update document, same revision;
      cmd = `/_api/document/${did}`;
      hdr = { "if-match": rev };
      body = { "World" : "Hallo" };
      doc = arango.PUT_RAW(cmd, body, hdr);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      cmd = `/_api/collection/${cid._id}/properties`;
      body = { "waitForSync" : false };
      doc = arango.PUT_RAW(cmd, body);

      // wait for dbservers to pick up the change;
      sleep(2);

      // update document ;
      cmd = `/_api/document/${did}`;
      hdr = { "if-match": rev2 };
      body = { "World" : "Hallo2" };
      let doc3 = arango.PUT_RAW(cmd, body, hdr);

      assertEqual(doc3.code, 202);
      assertEqual(doc3.headers['content-type'], contentType);
      let rev3 = doc3.parsedBody['_rev'];

      // update document ;
      cmd = `/_api/document/${did}?waitForSync=true`;
      hdr = { "if-match": rev3 };
      body = { "World" : "Hallo3" };
      let doc4 = arango.PUT_RAW(cmd, body, hdr);

      assertEqual(doc4.code, 201);
      assertEqual(doc4.headers['content-type'], contentType);
      let rev4 = doc4.parsedBody['_rev'];

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_create_a_document_and_update_it__using_an_invalid_revision: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document, invalid revision;
      cmd = `/_api/document/${did}`;
      let hdr = { "if-match": '"*abcd"' };
      doc = arango.PUT_RAW(cmd, body, hdr);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_CONFLICT.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);

      // update document, invalid revision;
      cmd = `/_api/document/${did}`;
      hdr = { "if-match": "'*abcd'" };
      doc = arango.PUT_RAW(cmd, body, hdr);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_CONFLICT.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);

      // update document, correct revision;
      cmd = `/_api/document/${did}`;
      hdr = { "if-match": `'${rev}'` };
      doc = arango.PUT_RAW(cmd, body, hdr);

      assertEqual(doc.code, 201);
    },

    test_create_a_document_and_update_it__waitForSync_URL_paramEQfalse: function() {
      let cmd = `/_api/document?collection=${cid._id}&waitForSync=false`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document;
      cmd = `/_api/document/${did}`;
      body = { "World" : "Hallo" };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_create_a_document_and_update_it__waitForSync_URL_paramEQtrue: function() {
      let cmd = `/_api/document?collection=${cid._id}&waitForSync=true`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document;
      cmd = `/_api/document/${did}`;
      body = { "World" : "Hallo" };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      arango.DELETE_RAW(location);

      assertEqual(db[cn].count(), 0);
    },

    test_update_a_document__using_patch: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document;
      cmd = `/_api/document/${did}`;
      body = { "fox" : "Foxy" };
      doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      assertEqual(db[cn].count(), 1);

      doc = arango.GET_RAW(`/_api/document/${did}`);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody['Hallo'], 'World');
      assertEqual(doc.parsedBody['fox'], 'Foxy');
    },

    test_update_a_document__using_patch__keepNull_EQ_true: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document;
      cmd = `/_api/document/${did}?keepNull=true`;
      body = { "fox" : "Foxy", "Hallo" : null };
      doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      assertEqual(db[cn].count(), 1);

      doc = arango.GET_RAW(`/_api/document/${did}`);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody.hasOwnProperty('Hallo')); // nil, but the attribute is there
      assertEqual(doc.parsedBody['fox'], 'Foxy');
    },

    test_update_a_document__using_patch__keepNull_EQ_false: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document;
      cmd = `/_api/document/${did}?keepNull=false`;
      body = { "fox" : "Foxy", "Hallo" : null };
      doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      assertEqual(db[cn].count(), 1);

      doc = arango.GET_RAW(`/_api/document/${did}`);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody.hasOwnProperty('Hallo'));
      assertEqual(doc.parsedBody['fox'], 'Foxy');
    },

    test_update_a_document__using_duplicate_attributes: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let did = doc.parsedBody['_id'];

      // update document;
      cmd = `/_api/document/${did}`;
      body = '{ "a": 1, "a": 2 }';
      doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_update_a_document__using_duplicate_nested_attributes: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let did = doc.parsedBody['_id'];

      // update document with broken json;
      cmd = `/_api/document/${did}`;
      body = '{ "outer" : { "a": 1, "a": 2 } }';
      doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_replace_a_document__using_duplicate_attributes: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let did = doc.parsedBody['_id'];

      // update document with broken json;
      cmd = `/_api/document/${did}`;
      body = '{ "a": 1, "a": 2 }';
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_replace_a_document__using_duplicate_nested_attributes: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let did = doc.parsedBody['_id'];

      // update document with broken json;
      cmd = `/_api/document/${did}`;
      body = '{ "outer" : { "a": 1, "a": 2 } }';
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_CORRUPTED_JSON.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_create_a_document_and_update_it__using_ignoreRevsEQfalse: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document, different revision;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_rev": "658993", "World" : "Hallo" };
      doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      // update document, same revision;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_rev": rev, "World" : "Hallo" };
      doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      cmd = `/_api/collection/${cid._id}/properties`;
      body = { "waitForSync" : false };
      doc = arango.PUT_RAW(cmd, body);

      // wait for dbservers to pick up the change;
      sleep(2);

      // update document ;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_rev": rev2, "World" : "Hallo2" };
      let doc3 = arango.PUT_RAW(cmd, body);

      assertEqual(doc3.code, 202);
      assertEqual(doc3.headers['content-type'], contentType);
      let rev3 = doc3.parsedBody['_rev'];

      // update document ;
      cmd = `/_api/document/${did}?ignoreRevs=false&waitForSync=true`;
      body = { "_rev": rev3, "World" : "Hallo3" };
      let doc4 = arango.PATCH_RAW(cmd, body);

      assertEqual(doc4.code, 201);
      assertEqual(doc4.headers['content-type'], contentType);
      let rev4 = doc4.parsedBody['_rev'];

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },

    test_create_a_document_and_update_it__using_ignoreRevsEQfalse_with_key: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "_key" : "hello", "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // update document, different revision;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_key" : "hello", "_rev" : "658993", "World" : "Hallo" };
      doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      // update document, same revision;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_key" : "hello", "_rev" : rev, "World" : "Hallo" };
      doc = arango.PATCH_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertNotEqual(rev2, rev);

      cmd = `/_api/collection/${cid._id}/properties`;
      body = { "waitForSync" : false };
      doc = arango.PUT_RAW(cmd, body);

      // wait for dbservers to pick up the change;
      sleep(2);

      // update document ;
      cmd = `/_api/document/${did}?ignoreRevs=false`;
      body = { "_key" : "hello", "_rev": rev2, "World" : "Hallo2" };
      let doc3 = arango.PUT_RAW(cmd, body);

      assertEqual(doc3.code, 202);
      assertEqual(doc3.headers['content-type'], contentType);
      let rev3 = doc3.parsedBody['_rev'];

      // update document ;
      cmd = `/_api/document/${did}?ignoreRevs=false&waitForSync=true`;
      body = { "_key" : "hello", "_rev": rev3, "World" : "Hallo3" };
      let doc4 = arango.PATCH_RAW(cmd, body);

      assertEqual(doc4.code, 201);
      assertEqual(doc4.headers['content-type'], contentType);
      let rev4 = doc4.parsedBody['_rev'];

      arango.DELETE(location);

      assertEqual(db[cn].count(), 0);
    },
  };
}

jsunity.run(error_handlingSuite);
jsunity.run(updating_documentSuite);
return jsunity.done();
