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


////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function testing_keysSuite () {
  let cn = "UnitTestsKey";
  return {
    
    setUpAll: function() {
      db._create(cn, { waitForSync: true });
    },

    tearDown: function() {
      db[cn].truncate();
    },

    tearDownAll: function() {
      db._drop(cn);
    },

    test_returns_an_error_if__key_is_null: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "_key" : null };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if__key_is_a_bool: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "_key" : true };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if__key_is_a_number_1: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "_key" : 12 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if__key_is_a_number_2: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "_key" : 12.554 };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if__key_is_a_list: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "_key" : [ ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if__key_is_an_object: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "_key" : { } };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if__key_is_empty_string: function() {
      let cmd = `/_api/document?collection=${cn}`;
      let body = { "_key" : "" };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
      assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
      assertEqual(doc.headers['content-type'], contentType);
    },

    test_returns_an_error_if__key_contains_invalid_characters: function() {
      let cmd = `/_api/document?collection=${cn}`;

      let keys = [
        " ",
        "   ",
        "abcd ",
        " abcd",
        " abcd ",
        "\\tabcd",
        "\\nabcd",
        "\\rabcd",
        "abcd defg",
        "abcde/bdbg",
        "a/a",
        "/a",
        "adbfbgb/",
        "öööää",
        "müller",
        "\\\"invalid",
        "\\\\invalid",
        "\\\\\\\\invalid",
        "?invalid",
        "#invalid",
        "&invalid",
        "[invalid]",
        "a".repeat(255)
      ];

      keys.forEach( key => {
        let body = { "_key" : key };
        let doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code);
        assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_BAD_PARAMETER.code);
        assertEqual(doc.headers['content-type'], contentType);
      });
    },

    test_test_valid_key_values: function() {
      let cmd = `/_api/document?collection=${cn}`;

      let keys = [
        "0",
        "1",
        "123456",
        "0123456",
        "true",
        "false",
        "a",
        "A",
        "a1",
        "A1",
        "01ab01",
        "01AB01",
        "invalid", // actually valid;
        "INVALID", // actually valid;
        "inValId", // actually valid;
        "abcd-efgh",
        "abcd_efgh",
        "Abcd_Efgh",
        "@",
        "@@",
        "abc@foo.bar",
        "@..abc-@-foo__bar",
        ".foobar",
        "-foobar",
        "_foobar",
        "@foobar",
        "(valid)",
        "%valid",
        "$valid",
        "$$bill,y'all",
        "'valid",
        "'a-key-is-a-key-is-a-key'",
        "m+ller",
        ";valid",
        ",valid",
        "!valid!",
        ":",
        ":::",
        ":-:-:",
        ";",
        ";;;;;;;;;;",
        "(",
        ")",
        "()xoxo()",
        "%",
        "%-%-%-%",
        ":-)",
        "!",
        "!!!!",
        "'",
        "''''",
        "this-key's-valid.",
        "=",
        "==================================================",
        "-=-=-=___xoxox-",
        "*",
        "(*)",
        "****",
        ".",
        "...",
        "-",
        "--",
        "_",
        "__",
        "(".repeat(254), // 254 bytes is the maximum allowed length;
        ")".repeat(254), // 254 bytes is the maximum allowed length;
        ",".repeat(254), // 254 bytes is the maximum allowed length;
        ":".repeat(254), // 254 bytes is the maximum allowed length;
        ";".repeat(254), // 254 bytes is the maximum allowed length;
        "*".repeat(254), // 254 bytes is the maximum allowed length;
        "=".repeat(254), // 254 bytes is the maximum allowed length;
        "-".repeat(254), // 254 bytes is the maximum allowed length;
        "%".repeat(254), // 254 bytes is the maximum allowed length;
        "@".repeat(254), // 254 bytes is the maximum allowed length;
        "'".repeat(254), // 254 bytes is the maximum allowed length;
        ".".repeat(254), // 254 bytes is the maximum allowed length;
        "!".repeat(254), // 254 bytes is the maximum allowed length;
        "_".repeat(254), // 254 bytes is the maximum allowed length;
        "a".repeat(254)  // 254 bytes is the maximum allowed length;
      ];

      keys.forEach( key => {
        let body = { "_key" : key };
        let doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, 201, doc);
        assertEqual(doc.parsedBody['_key'], key);
        assertEqual(doc.headers['content-type'], contentType);
      });
    },

    test_test_duplicate_key_values: function() {
      let cmd = `/_api/document?collection=${cn}`;

      // prefill collection;
      let keys = [
              "0",
              "1",
              "a",
              "b",
              "c",
              "A",
              "B",
              "C",
              "aBcD-"
             ];

      keys.forEach( key => {
        let body = { "_key" : key };
        // insert initially;
        let doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, 201, doc);
        assertEqual(doc.parsedBody['_key'], key);
        assertEqual(doc.headers['content-type'], contentType);

        // insert again. this will fail! ;
        doc = arango.POST_RAW(cmd, body);

        assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);
        assertEqual(doc.parsedBody['code'], internal.errors.ERROR_HTTP_CONFLICT.code);
        assertEqual(doc.headers['content-type'], contentType);
      });
    }
  };
};

jsunity.run(testing_keysSuite);
return jsunity.done();
