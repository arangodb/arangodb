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
// / @author Wilfried Goesgens
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
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_returns_an_error_if_document_identifier_is_missing: function() {
      let cmd = "/_api/document";
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_document_identifier_is_corrupted_1: function() {
      let cmd = "/_api/document/123456";
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_document_identifier_is_corrupted_2: function() {
      let cmd = "/_api/document//123456";
      let doc = arango.DELETE_RAW(cmd);
      
      if (arango.protocol() === 'vst') {
        assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
        assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
        assertEqual(doc.headers['content-type'], contentType);
      } else {
        assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code, doc);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertEqual(doc.headers['content-type'], contentType);
      }
    },

    test_returns_an_error_if_collection_identifier_is_unknown: function() {
      let cmd = "/_api/document/123456/234567";
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_document_identifier_is_unknown: function() {
      let cmd = `/_api/document/${cid._id}/234567`;
      let doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code, doc);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// deleting documents;
////////////////////////////////////////////////////////////////////////////////;
function deleting_documentsSuite () {
  let cn = "UnitTestsCollectionBasics";
  let cid;
  return {
    setUp: function() {
      cid = db._create(cn, { waitForSync: true });
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_create_a_document_and_delete_it: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // delete document;
      cmd = `/_api/document/${did}`;
      doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      assertEqual(cid.count(), 0);
    },

    test_create_a_document_and_delete_it__using_if_match: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // delete document, different revision;
      cmd = `/_api/document/${did}`;
      let hdr = { "if-match": '"386897"' };
      doc = arango.DELETE_RAW(cmd, "", hdr);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code, doc);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      // delete document, same revision;
      cmd = `/_api/document/${did}`;
      hdr = { "if-match": `"${rev}"` };
      doc = arango.DELETE_RAW(cmd, "", hdr);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      assertEqual(cid.count(), 0);
    },

    test_create_a_document_and_delete_it__using_an_invalid_revision: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // delete document, invalid revision;
      cmd = `/_api/document/${did}`;
      let hdr = { "if-match": `"*abcd"` };
      doc = arango.DELETE_RAW(cmd, "", hdr);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code, doc);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_CONFLICT.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);

      // delete document, invalid revision;
      cmd = `/_api/document/${did}`;
      hdr = { "if-match": "'*abcd'" };
      doc = arango.DELETE_RAW(cmd, "", hdr);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_CONFLICT.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code);

      // delete document, correct revision;
      cmd = `/_api/document/${did}`;
      hdr = { "if-match": `'${rev}'` };
      doc = arango.DELETE_RAW(cmd, "", hdr);

      assertEqual(doc.code, 200);

      assertEqual(cid.count(), 0);
    },

    test_create_a_document_and_delete_it__waitForSync_URL_param_EQ_false: function() {
      let cmd = `/_api/document?collection=${cid._id}&waitForSync=false`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // delete document;
      cmd = `/_api/document/${did}`;
      doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      assertEqual(cid.count(), 0);
    },

    test_create_a_document_and_delete_it__waitForSync_URL_param_EQ_true: function() {
      let cmd = `/_api/document?collection=${cid._id}&waitForSync=true`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      let rev = doc.parsedBody['_rev'];

      // delete document;
      cmd = `/_api/document/${did}`;
      doc = arango.DELETE_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertEqual(rev2, rev);

      assertEqual(cid.count(), 0);
    },
  };
}

jsunity.run(error_handlingSuite);
jsunity.run(deleting_documentsSuite);
return jsunity.done();
