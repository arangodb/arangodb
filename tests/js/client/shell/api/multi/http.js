/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global db, arango, assertTrue, assertFalse, assertEqual */

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

const internal = require('internal');
const forceJson = internal.options().hasOwnProperty('server.force-json') && internal.options()['server.force-json'];
const contentType = forceJson ? "application/json" :  "application/x-velocypack";
const cacheControl = "no-cache, no-store, must-revalidate, pre-check=0, post-check=0, max-age=0, s-maxage=0";
const contentSecurityPolicy ="frame-ancestors 'self'; form-action 'self';";
const pragma = "no-cache";
const strictTransportSecurity = "max-age=31536000 ; includeSubDomains";
const xContentTypeOptions = "nosniff";
const jsunity = require("jsunity");

function assertCspHeaders(doc, customContentType = contentType) {
  assertEqual(doc.headers['content-type'], customContentType);
  assertEqual(doc.headers['cache-control'], cacheControl);
  assertEqual(doc.headers['content-security-policy'], contentSecurityPolicy);
  assertEqual(doc.headers.expires, 0);
  assertEqual(doc.headers.pragma, pragma);
  assertEqual(doc.headers['strict-transport-security'], strictTransportSecurity);
  assertEqual(doc.headers['x-content-type-options'], xContentTypeOptions);
}



////////////////////////////////////////////////////////////////////////////////;
// checking HTTP HEAD responses;
////////////////////////////////////////////////////////////////////////////////;

function head_requestsSuite () {
  return {
    test_checks_whether_HEAD_returns_a_body_on_2xx: function() {
      let cmd = "/_api/version";
      let doc = arango.HEAD_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc);
    },

    test_checks_whether_HEAD_returns_a_body_on_3xx: function() {
      let cmd = "/_api/collection";
      let doc = arango.HEAD_RAW(cmd);

      assertEqual(doc.code, 405);
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc, "text/plain");
    },

    test_checks_whether_HEAD_returns_a_body_on_4xx_2: function() {
      let cmd = "/_api/cursor";
      let doc = arango.HEAD_RAW(cmd);

      assertEqual(doc.code, 405);
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc, "text/plain");
    },

    test_checks_whether_HEAD_returns_a_body_on_4xx: function() {
      let cmd = "/_api/non-existing-method";
      let doc = arango.HEAD_RAW(cmd);

      assertEqual(doc.code, 404);
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc);
    },

    test_checks_whether_HEAD_returns_a_body_on_an_existing_document: function() {
      let cn = "UnitTestsCollectionHttp";

      // create collection with one document;
      db._create(cn);
      try {
        let cmd = `/_api/document?collection=${cn}`;
        let body = { "Hello" : "World" };
        let doc = arango.POST_RAW(cmd, body);

        let rev = doc.parsedBody._rev;
        let did = doc.parsedBody._id;
        assertEqual(typeof did, 'string', doc);

        // run a HTTP HEAD query on the existing document;
        cmd = "/_api/document/" + did;
        doc = arango.HEAD_RAW(cmd);

        assertEqual(doc.code, 200);
        assertFalse(doc.hasOwnProperty('parsedBody'));
        // run a HTTP HEAD query on the existing document, with wrong precondition;
        cmd = "/_api/document/" + did;
        doc = arango.HEAD_RAW(cmd, {"if-match": rev });

        assertEqual(doc.code, 200, doc);
        assertFalse(doc.hasOwnProperty('parsedBody'));

        assertCspHeaders(doc);
      } finally {
        db._drop(cn);
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// checking HTTP GET responses;
////////////////////////////////////////////////////////////////////////////////;
function get_requestSuite () {
  return {
    test_checks_a_non_existing_URL: function() {
      let cmd = "/xxxx/yyyy";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 404);
      assertEqual(doc.headers['content-type'], contentType);
      assertTrue(doc.parsedBody.error);
      assertEqual(doc.parsedBody.code, 404);

      assertCspHeaders(doc);
    },

    test_checks_whether_GET_returns_a_body_1: function() {
      let cmd = "/_api/non-existing-method";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 404);
      assertTrue(doc.parsedBody.error);
      assertEqual(doc.parsedBody.errorNum, 404);
      assertEqual(doc.parsedBody.code, 404);
      assertCspHeaders(doc);
    },

    test_checks_whether_GET_returns_a_body_2: function() {
      let cmd = "/_api/non-allowed-method";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 404);
      assertTrue(doc.parsedBody.error);
      assertEqual(doc.parsedBody.errorNum, 404);
      assertEqual(doc.parsedBody.code, 404);
      assertCspHeaders(doc);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// checking HTTP OPTIONS;
////////////////////////////////////////////////////////////////////////////////;
function options_requestSuite () {
  let headers = "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT";
  return {

    test_checks_handling_of_an_OPTIONS_request__without_body: function() {
      let cmd = "/_api/version";
      let doc = arango.OPTIONS_RAW(cmd, "");
      assertEqual(doc.headers.allow, headers);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc, "text/plain");
    },

    test_checks_handling_of_an_OPTIONS_request__with_body: function() {
      let cmd = "/_api/version";
      let doc = arango.OPTIONS_RAW(cmd, "some stuff");
      assertEqual(doc.headers.allow, headers);

      assertEqual(doc.code, 200);
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc, "text/plain");
    }
  };
}

////////////////////////////////////////////////////////////////////////////////;
// checking CORS requests;
////////////////////////////////////////////////////////////////////////////////;
function CORS_requestSuite () {
  let headers = "DELETE, GET, HEAD, OPTIONS, PATCH, POST, PUT";
  return {

    test_checks_handling_of_a_non_CORS_GET_request: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd);
    
      assertEqual(doc.code, 200);
      assertEqual(doc.headers['access-control-allow-origin'], undefined);
      assertEqual(doc.headers['access-control-allow-methods'], undefined);
      assertEqual(doc.headers['access-control-allow-credentials'], undefined);
      assertCspHeaders(doc);
    },

    test_checks_handling_of_a_CORS_GET_request__with_null_origin: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd, { "Origin": "null" } );

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['access-control-allow-origin'], "null");
      assertEqual(doc.headers['access-control-allow-methods'], undefined);
      assertEqual(doc.headers['access-control-allow-headers'], undefined);
      assertEqual(doc.headers['access-control-allow-credentials'], "false", doc);
      assertEqual(doc.headers['access-control-max-age'], undefined);
      assertCspHeaders(doc);
    },

    test_checks_handling_of_a_CORS_GET_request: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd, { "Origin": "http://127.0.0.1" });

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['access-control-allow-origin'], "http://127.0.0.1");
      assertEqual(doc.headers['access-control-allow-methods'], undefined);
      assertEqual(doc.headers['access-control-allow-headers'], undefined);
      assertEqual(doc.headers['access-control-allow-credentials'], "false");
      assertEqual(doc.headers['access-control-max-age'], undefined);
      assertCspHeaders(doc);
    },

    test_checks_handling_of_a_CORS_GET_request_from_origin_that_is_trusted: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd, { "Origin": "http://was-erlauben-strunz.it" });

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['access-control-allow-origin'], "http://was-erlauben-strunz.it");
      assertEqual(doc.headers['access-control-allow-methods'], undefined);
      assertEqual(doc.headers['access-control-allow-headers'], undefined);
      assertEqual(doc.headers['access-control-allow-credentials'], "true");
      assertEqual(doc.headers['access-control-max-age'], undefined);
      assertCspHeaders(doc);
    },

    test_checks_handling_of_a_CORS_POST_request: function() {
      let cmd = "/_api/version";
      let doc = arango.GET_RAW(cmd,{ "Origin": "http://www.some-url.com/" });

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['access-control-allow-origin'], "http://www.some-url.com/");
      assertEqual(doc.headers['access-control-allow-methods'], undefined);
      assertEqual(doc.headers['access-control-allow-headers'], undefined);
      assertEqual(doc.headers['access-control-allow-credentials'], "false");
      assertEqual(doc.headers['access-control-max-age'], undefined);
      assertCspHeaders(doc);
    },

    test_checks_handling_of_a_CORS_OPTIONS_preflight_request__no_headers: function() {
      let cmd = "/_api/version";
      let doc = arango.OPTIONS_RAW(cmd, "", {
        "origin": "http://from.here.we.come/really/really",
        "access-control-request-method": "delete" });

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['access-control-allow-origin'], "http://from.here.we.come/really/really", doc);
      assertEqual(doc.headers['access-control-allow-methods'], headers);
      assertEqual(doc.headers['access-control-allow-headers'], undefined);
      assertEqual(doc.headers['access-control-allow-credentials'], "false");
      assertEqual(doc.headers['access-control-max-age'], "1800");
      assertEqual(doc.headers.allow, headers);
      assertEqual(doc.headers['content-length'], "0");
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc, "text/plain");
    },

    test_checks_handling_of_a_CORS_OPTIONS_preflight_request__empty_headers: function() {
      let cmd = "/_api/version";
      let doc = arango.OPTIONS_RAW(cmd, "", {
        "oRiGiN": "HTTPS://this.is.our/site-yes",
        "access-control-request-method": "delete",
        "access-control-request-headers": "   "
      });

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['access-control-allow-origin'], "HTTPS://this.is.our/site-yes", doc);
      assertEqual(doc.headers['access-control-allow-methods'], headers);
      assertEqual(doc.headers['access-control-allow-headers'], undefined);
      assertEqual(doc.headers['access-control-allow-credentials'], "false");
      assertEqual(doc.headers['access-control-max-age'], "1800");
      assertEqual(doc.headers.allow, headers);
      assertEqual(doc.headers['content-length'], "0");
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc, "text/plain");
    },

    test_checks_handling_of_a_CORS_OPTIONS_preflight_request__populated_headers: function() {
      let cmd = "/_api/version";
      let doc = arango.OPTIONS_RAW(cmd, "", {
        "ORIGIN": "https://mysite.org",
        "Access-Control-Request-Method": "put",
        "ACCESS-CONTROL-request-headers": "foo,bar,baz"
      });

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['access-control-allow-origin'], "https://mysite.org", doc);
      assertEqual(doc.headers['access-control-allow-methods'], headers);
      assertEqual(doc.headers['access-control-allow-headers'], "foo,bar,baz");
      assertEqual(doc.headers['access-control-allow-credentials'], "false");
      assertEqual(doc.headers['access-control-max-age'], "1800");
      assertEqual(doc.headers.allow, headers);
      assertEqual(doc.headers['content-length'], "0");
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc, "text/plain");
    },

    test_checks_handling_of_a_CORS_OPTIONS_preflight_request: function() {
      let cmd = "/_api/version";
      let doc = arango.OPTIONS_RAW(cmd, "", {
        "ORIGIN": "https://mysite.org",
        "Access-Control-Request-Method": "put" });

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['access-control-allow-origin'], "https://mysite.org", doc);
      assertEqual(doc.headers['access-control-allow-methods'], headers);
      assertEqual(doc.headers['access-control-allow-credentials'], "false");
      assertEqual(doc.headers['access-control-max-age'], "1800");
      assertEqual(doc.headers.allow, headers);
      assertEqual(doc.headers['content-length'], "0");
      assertEqual(doc.parsedBody, undefined);
      assertCspHeaders(doc, "text/plain");
    }
  };
}

jsunity.run(head_requestsSuite);
jsunity.run(get_requestSuite);
jsunity.run(options_requestSuite);
jsunity.run(CORS_requestSuite);
return jsunity.done();
