/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, fail, arango, assertTrue, assertFalse, assertEqual, assertNotUndefined, assertMatch */

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
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");

const internal = require('internal');
const sleep = internal.sleep;
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";

let rePath = /^\/_db\/[^/]+\/_api\/document\/[a-zA-Z0-9_let :\.\-]+\/\d+$/;
let reFull = /^[a-zA-Z0-9_\-]+\/\d+$/;
let reRev  = /^[-_0-9a-zA-Z]+$/;


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

    test_returns_an_error_if_document_identifier_is_corrupted: function() {
      let cmd = "/_api/document/123456";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_document_identifier_is_corrupted_with_empty_cid: function() {
      let cmd = "/_api/document//123456";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_collection_identifier_is_unknown: function() {
      let cmd = "/_api/document/123456/234567";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if_document_identifier_is_unknown: function() {
      let cmd = `/_api/document/${cid._id}/234567`;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, doc);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);

      assertEqual(cid.count(), 0);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// reading documents;
////////////////////////////////////////////////////////////////////////////////;
function reading_a_documentSuite () {
  let cn = "UnitTestsCollectionBasics";
  let reStart = new RegExp('^' + cn + '/');
  let cid;
  return {
    setUpAll: function() {
      cid = db._create(cn, { waitForSync: true } );
    },

    tearDownAll: function() {
      db._drop(cn);
    },

    test_create_a_document_and_read_it: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = "{ \"Hallo\" : \"World\" }";
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];

      assertMatch(reFull, did);
      assertMatch(reStart, did);

      let rev = doc.parsedBody['_rev'];
      assertMatch(reRev, rev);

      // get document;
      cmd = `/_api/document/${did}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertMatch(reFull, did2);
      assertMatch(reStart, did2);
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertMatch(reRev, rev2);
      assertEqual(rev2, rev);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      assertEqual(etag, `"${rev}"`);

      arango.DELETE_RAW(location);

      assertEqual(cid.count(), 0);
    },

    test_create_a_document_and_read_it__using_collection_name: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      assertMatch(reFull, did);
      assertMatch(reStart, did);

      let rev = doc.parsedBody['_rev'];
      assertMatch(reRev, rev);

      // get document;
      cmd = `/_api/document/${did}`;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertMatch(reFull, did2);
      assertMatch(reStart, did2);
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertMatch(reRev, rev2);
      assertEqual(rev2, rev);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      assertEqual(etag, `"${rev}"`);

      arango.DELETE_RAW(location);

      assertEqual(cid.count(), 0);
    },

    test_create_a_document_and_read_it__use_if_none_match: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      assertMatch(reFull, did);
      assertMatch(reStart, did);

      let rev = doc.parsedBody['_rev'];
      assertMatch(reRev, rev);

      // get document, if-none-match with same rev;
      cmd = `/_api/document/${did}`;
      let hdr = { "if-none-match": `"${rev}"` };
      doc = arango.GET_RAW(cmd, hdr);

      assertEqual(doc.code, 304);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      assertEqual(etag, `"${rev}"`);

      // get document, if-none-match with different rev;
      cmd = `/_api/document/${did}`;
      hdr = { "if-none-match": '"54454"' };
      doc = arango.GET_RAW(cmd, hdr);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      assertEqual(etag, `"${rev}"`);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertMatch(reFull, did2);
      assertMatch(reStart, did2);
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertMatch(reRev, rev2);
      assertEqual(rev2, rev);

      etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      assertEqual(etag, `"${rev}"`);

      arango.DELETE_RAW(location);

      assertEqual(cid.count(), 0);
    },

    test_create_a_document_and_read_it__use_if_match: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      let did = doc.parsedBody['_id'];
      assertMatch(reFull, did);
      assertMatch(reStart, did);

      let rev = doc.parsedBody['_rev'];
      assertMatch(reRev, rev);

      // get document, if-match with same rev;
      cmd = `/_api/document/${did}`;
      let hdr = { "if-match": `"${rev}"` };
      doc = arango.GET_RAW(cmd, hdr);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      let did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertMatch(reFull, did2);
      assertMatch(reStart, did2);
      assertEqual(did2, did);

      let rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertMatch(reRev, rev2);
      assertEqual(rev2, rev);

      let etag = doc.headers['etag'];
      assertEqual(typeof etag, 'string');

      assertEqual(etag, `"${rev}"`);

      // get document, if-match with different rev;
      cmd = `/_api/document/${did}`;
      hdr = { "if-match": '"348574"' };
      doc = arango.GET_RAW(cmd, hdr);

      assertEqual(doc.code, 412);

      did2 = doc.parsedBody['_id'];
      assertEqual(typeof did2, 'string');
      assertMatch(reFull, did2);
      assertMatch(reStart, did2);
      assertEqual(did2, did);

      rev2 = doc.parsedBody['_rev'];
      assertEqual(typeof rev2, 'string');
      assertMatch(reRev, rev2);
      assertEqual(rev2, rev);

      arango.DELETE_RAW(location);

      assertEqual(cid.count(), 0);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// reading all documents;
////////////////////////////////////////////////////////////////////////////////;
function reading_all_documentsSuite () {
  let cn = "UnitTestsCollectionAll";
  let reStart = new RegExp('^/_db/[^/]+/_api/document/' + cn + '/');
  let cid;
  return {
    setUpAll: function() {
      cid = db._create(cn, { waitForSync: true });
    },
    tearDown: function() {
      cid.truncate();
    },

    tearDownAll: function() {
      db._drop(cn);
    },

    test_get_all_documents_of_an_empty_collection: function() {
      // get documents;
      let cmd = "/_api/simple/all-keys";
      let body = { "collection" : cn };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let documents = doc.parsedBody['result'];
      assertTrue(Array.isArray(documents));
      assertEqual(documents.length, 0);

      assertEqual(cid.count(), 0);
    },

    test_get_all_documents_of_an_empty_collection__using_typeEQid: function() {
      // get documents;
      let cmd = "/_api/simple/all-keys";
      let body = { "collection" : cn , "type" : "id" };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let documents = doc.parsedBody['result'];
      assertTrue(Array.isArray(documents));
      assertEqual(documents.length, 0);

      assertEqual(cid.count(), 0);
    },

    test_create_three_documents_and_read_them_using_the_collection_identifier: function() {
      let cmd = `/_api/document?collection=${cid._id}`;

      let location = [];

      [ 1, 2, 3 ].forEach( i => {
        let body = { "Hallo" : `World-${i}` };
        let doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, 201);
        
        location.push(doc.headers['location']);
      });
      
      // get document;
      cmd = "/_api/simple/all-keys";
      let body = { "collection" :  cn };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let documents = doc.parsedBody['result'];
      assertTrue(Array.isArray(documents));
      assertEqual(documents.length, 3);

      documents.forEach(document => {
        assertMatch(rePath, document);
        assertMatch(reStart, document);
      });

      location.forEach(l => {
        arango.DELETE_RAW(l);
      });
      assertEqual(cid.count(), 0);
    },

    test_create_three_documents_and_read_them_using_the_collection_name: function() {
      let cmd = `/_api/document?collection=${cn}`;

      let location = [];

      [ 1, 2, 3 ].forEach( i => {
        let body = { "Hallo" : `World-${i}` };
        let doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, 201);
        
        location.push(doc.headers['location']);
      });

      // get document;
      cmd = "/_api/simple/all-keys";
      let body = { "collection" : cn };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let documents = doc.parsedBody['result'];
      assertTrue(Array.isArray(documents));
      assertEqual(documents.length, 3);

      documents.forEach(document => {
        assertMatch(rePath, document);
        assertMatch(reStart, document);
      });

      location.forEach(l => {
        arango.DELETE_RAW(l);
      });
      assertEqual(cid.count(), 0);
    },

    test_create_three_documents_and_read_them_using_the_collection_name__typeEQid: function() {
      let cmd = `/_api/document?collection=${cn}`;

      let location = [];

      [ 1, 2, 3 ].forEach( i => {
        let body = { "Hallo" : `World-${i}` };
        let doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, 201);
        
        location.push(doc.headers['location']);
      });
      
      // get documents;
      cmd = "/_api/simple/all-keys";
      let body = { "collection" : cn, "type" : "id" };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let documents = doc.parsedBody['result'];
      assertTrue(Array.isArray(documents));
      assertEqual(documents.length, 3);

      let regex = new RegExp('^' + cn + '/\\d+$');

      documents.forEach(document => {
        assertMatch(regex, document);
      });
      cid.truncate();
    },

    test_create_three_documents_and_read_them_using_the_collection_name__typeEQkey: function() {
      let cmd = `/_api/document?collection=${cn}`;

      let location = [];

      [ 1, 2, 3 ].forEach( i => {
        let body = { "Hallo" : `World-${i}` };
        let doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, 201);
        
        location.push(doc.headers['location']);
      });

      // get documents
      cmd = "/_api/simple/all-keys";
      let body = {"collection" : cn, "type": "key" };
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);

      let documents = doc.parsedBody['result'];
      assertTrue(Array.isArray(documents));
      assertEqual(documents.length, 3);

      let regex = new RegExp('^\\d+$');  ;
      documents.forEach(document => {
        assertMatch(regex, document);
      });
      cid.truncate();
    },
  };
}

////////////////////////////////////////////////////////////////////////////////;
// checking document;
////////////////////////////////////////////////////////////////////////////////;
function checking_a_documentSuite () {
  let cn = "UnitTestsCollectionBasics";
  let cid;
  return {
    setUp: function() {
      cid = db._create(cn, { waitForSync: true });
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_create_a_document_and_check_to_read_it: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body, { 'accept-encoding': 'identity' });

      assertEqual(doc.code, 201, doc);
      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      // get document;
      cmd = location;
      doc = arango.GET_RAW(cmd, { 'accept-encoding': 'identity' });

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      let content_length = doc.headers['content-length'];

      // get the document head;
      doc = arango.HEAD_RAW(cmd, { 'accept-encoding': 'identity' });

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.headers['content-length'], content_length);
      assertEqual(doc.body, undefined);

      arango.DELETE_RAW(location);

      assertEqual(cid.count(), 0);
    },

    test_use_an_invalid_revision_for_HEAD: function() {
      let cmd = `/_api/document?collection=${cid._id}`;
      let body = { "Hallo" : "World" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201, doc);
      let location = doc.headers['location'];
      assertEqual(typeof location, 'string');

      // get document;
      cmd = location;
      doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      // get the document head, withdrawn for 3.0;
      doc = arango.HEAD_RAW(cmd + "?rev=abcd");

      assertEqual(doc.code, 200);

      let hdr = { "if-match": "'*abcd'" };
      doc = arango.HEAD_RAW(cmd, hdr);

      assertEqual(doc.code, 412);
      hdr = { "if-match": "nonMatching" };
      doc = arango.HEAD_RAW(cmd, hdr);

      assertEqual(doc.body, undefined);
      assertEqual(doc.code, 412);
    },

    test_use_empty_array_for_documents_read: function () {
      let cmd = `/_api/document/${cid._id}?onlyget=true`;
      let body = [];
      let doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertEqual(doc.parsedBody, []);
    },

  };
}

jsunity.run(error_handlingSuite);
jsunity.run(reading_a_documentSuite);
jsunity.run(reading_all_documentsSuite);
jsunity.run(checking_a_documentSuite);
return jsunity.done();
