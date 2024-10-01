/*jshint globalstrict:false, strict:false */
/*global arango, VPACK_TO_V8, V8_TO_VPACK, assertEqual, assertTrue, assertFalse, assertNull */

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
/// @author Julia Puget
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////


'use strict';

const jsunity = require('jsunity');
const path = '/_admin/execute';

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
  };
};


/*
The following tests are for requests to _admin/execute in which the body is in velocypack format.
It's only interpreted as velocypack if the header has the content type for velocypack in the request.
Otherwise, as it happens when there's no content-type in the header, it's interpreted as plain text
 */

function adminExecuteWithHeaderVpack() {
  const headers = {
    'content-type': 'application/x-velocypack',
  };

  const obj = "require(\"console\").log(\"abc\");";
  const body = V8_TO_VPACK(obj);

  const res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertNull(res.parsedBody);
};

function adminExecuteWithHeaderVpack2() {
  const headers = {
    'content-type': 'application/x-velocypack',
  };

  const obj = "return \"abc\"";
  const body = V8_TO_VPACK(obj);

  const res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

function adminExecuteNoHeaderVpack() {
  const obj = "return \"abc\"";
  const body = V8_TO_VPACK(obj);

  const res = arango.POST_RAW(path, body);
  assertEqual(res.code, 500);
  assertTrue(res.error);
};


/*
The following tests are for requests to _admin/execute in which the body is in JSON format.
It's only interpreted as velocypack if the header has the content type for JSON in the request.
Otherwise, as it happens when there's no content-type in the header, it's interpreted as plain text
 */

function adminExecuteWithHeaderJson() {
  const headers = {
    'content-type': 'application/json',
  };

  const body = `"return 'abc';"`;
  const res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

//POST_RAW treats the body as JSON by default
function adminExecuteNoHeaderJson() {
  const body = `"return 'abc';"`;
  const res = arango.POST_RAW(path, body);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

/*
The following tests are for requests to _admin/execute in which the body is in plain/text format.
It's interpreted as such both if the header's content-type is plain/text or the header has no content-type.
 */

function adminExecuteWithHeaderPlainText() {
  const headers = {
    'content-type': 'plain/text',
  };

  const body = 'return "abc";';
  const res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

function adminExecuteWithHeaderPlainText2() {
  const headers = {
    'content-type': 'text/plain; charset=utf-8',
  };

  const body = 'return "abc";';
  const res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

function adminExecuteWithWrongHeaderPlainText() {
  const headers = {
    'content-type': 'text/plain; abc', //still considered as text/plain even with garbage after it
  };

  const body = 'return "abc";';
  const res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(RequestSuite);

return jsunity.done();
