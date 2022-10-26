/*jshint globalstrict:false, strict:false */
/*global arango, VPACK_TO_V8, V8_TO_VPACK, assertEqual, assertTrue, assertFalse, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Julia Puget
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


'use strict';

var jsunity = require('jsunity');
var expect = require('chai').expect;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function RequestSuite() {
  return {
    testAdminExecuteWithHeaderVpack: adminExecuteWithHeaderVpack,
    testAdminExecuteWithHeaderVpack2: adminExecuteWithHeaderVpack2,
    testAdminExecuteNoHeaderVpack: adminExecuteNoHeaderVpack,
    testAdminExecuteWithHeaderJson: adminExecuteWithHeaderJson,
    testAdminExecuteNoHeaderJson: adminExecuteNoHeaderJson,
    testAdminExecuteWithHeaderPlainText: adminExecuteWithHeaderPlainText,
    testAdminExecuteWithHeaderPlainText2: adminExecuteWithHeaderPlainText2,
    testAdminExecuteWithWrongHeaderPlainText: adminExecuteWithWrongHeaderPlainText,
    testAdminExecuteNoHeaderPlainText: adminExecuteNoHeaderPlainText,
  };
};


/*
The following tests are for requests to _admin/execute in which the body is in velocypack format.
It's only interpreted as velocypack if the header has the content type for velocypack in the request.
Otherwise, as it happens when there's no content-type in the header, it's interpreted as plain text
 */

function adminExecuteWithHeaderVpack() {
  var path = '/_admin/execute';
  var headers = {
    'content-type': 'application/x-velocypack',
  };

  var obj = "require(\"console\").log(\"abc\");";
  var body = V8_TO_VPACK(obj);

  var res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertNull(res.parsedBody);
};

function adminExecuteWithHeaderVpack2() {
  var path = '/_admin/execute';
  var headers = {
    'content-type': 'application/x-velocypack',
  };

  var obj = "return \"abc\"";
  var body = V8_TO_VPACK(obj);

  var res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

function adminExecuteNoHeaderVpack() {
  var path = '/_admin/execute';

  var obj = "return \"abc\"";
  var body = V8_TO_VPACK(obj);

  var res = arango.POST_RAW(path, body);
  assertEqual(res.code, 500);
  assertTrue(res.error);
};


/*
The following tests are for requests to _admin/execute in which the body is in JSON format.
It's only interpreted as velocypack if the header has the content type for JSON in the request.
Otherwise, as it happens when there's no content-type in the header, it's interpreted as plain text
 */

function adminExecuteWithHeaderJson() {
  var path = '/_admin/execute';
  var headers = {
    'content-type': 'application/json',
  };

  var obj = '"return \"abc\";"';
  var body = V8_TO_VPACK(obj);

  var res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

function adminExecuteNoHeaderJson() {
  var path = '/_admin/execute';

  var obj = '"return \"abc\";"';
  var body = V8_TO_VPACK(obj);

  var res = arango.POST_RAW(path, body);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertNull(res.parsedBody);
};

/*
The following tests are for requests to _admin/execute in which the body is in plain/text format.
It's interpreted as such both if the header's content-type is plain/text or the header has no content-type.
 */

function adminExecuteWithHeaderPlainText() {
  var path = '/_admin/execute';
  var headers = {
    'content-type': 'plain/text',
  };

  var obj = 'return "abc";';
  var body = V8_TO_VPACK(obj);

  var res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

function adminExecuteWithHeaderPlainText2() {
  var path = '/_admin/execute';
  var headers = {
    'content-type': 'plain/text; charset=utf-8',
  };

  var obj = 'return "abc";';
  var body = V8_TO_VPACK(obj);

  var res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

function adminExecuteWithWrongHeaderPlainText() {
  var path = '/_admin/execute';
  var headers = {
    'content-type': 'application/json; abc',
  };

  var obj = 'return "abc";';
  var body = V8_TO_VPACK(obj);

  var res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 400);
  assertTrue(res.error);
  assertNull(res.parsedBody);
};

function adminExecuteNoHeaderPlainText() {
  var path = '/_admin/execute';

  var obj = 'return "abc";';
  var body = V8_TO_VPACK(obj);

  var res = arango.POST_RAW(path, body);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RequestSuite);

return jsunity.done();
