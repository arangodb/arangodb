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
/// @author Jan Christoph Uhde
/// @author Copyright 2016, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require('jsunity');
const expect = require('chai').expect;

function RequestSuite() {
  return {
    testCursorAPI: cursorAPI,
    testVersionJsonJson: versionJsonJson,
    testVersionVpackJson: versionVpackJson,
    testVersionJsonVpack: versionJsonVpack,
    testVersionVpackVpack: versionVpackVpack,
    testEchoVpackVpack: echoVpackVpack,
    testAdminExecuteWithHeaderVpack: adminExecuteWithHeaderVpack,
    testAdminExecuteWithHeaderVpack2: adminExecuteWithHeaderVpack2,
    testAdminExecuteNoHeaderVpack: adminExecuteNoHeaderVpack,
  };
};

function cursorAPI() {
  const cn = "UnitTestsCollection";

  const path = '/_api/cursor';
  const headers = {
    'content-type': 'application/x-velocypack',
    'accept': 'application/x-velocypack',
    'accept-encoding': 'identity',
  };

  let db = require("@arangodb").db;

  let c = db._create(cn);
  c.insert({ _key: "test", value: 1 });

  try {
    [
      "doc", "ATTRIBUTES(doc)", 
      "UNSET(doc, ['_key'])", 
      "UNSET(doc, ['_id'])", 
      "UNSET(doc, ['_key', '_rev'])", 
      "UNSET (doc, ['_key', '_id'])"
    ].forEach((what) => {
      const body = V8_TO_VPACK({
        query: "FOR doc IN " + cn + " RETURN " + what
      });
      const res = arango.POST_RAW(path, body, headers);

      expect(String(res.headers['content-type'])).to.have.string("application/x-velocypack");
      const obj = VPACK_TO_V8(res.body);
      expect(obj.result).to.be.instanceOf(Array);
    });
  } finally {
    db._drop(cn);
  }
};

function versionJsonJson() {
  const path = '/_api/version';
  const headers = {
    'content-type': 'application/json',
    'accept': 'application/json',
    'accept-encoding': 'identity',
  };

  const res = arango.POST_RAW(path, "", headers);

  expect(String(res.headers['content-type'])).to.have.string("application/json");

  const obj = JSON.parse(res.body);

  expect(obj).to.have.property('server');
  expect(obj).to.have.property('version');
  expect(obj).to.have.property('license');

  expect(obj.server).to.be.equal('arango');
  expect(obj.version).to.match(/[0-9]+\.[0-9]+\.([0-9]+|(milestone|alpha|beta|devel|rc)[0-9]*)/);

  expect(obj.license).to.match(/enterprise|community/g);
};

function versionVpackJson() {
  const path = '/_api/version';
  const headers = {
    'content-type': 'application/x-velocypack',
    'accept': 'application/json',
    'accept-encoding': 'identity',
  };

  const res = arango.POST_RAW(path, "", headers);

  expect(res.body).to.be.a('string');
  expect(String(res.headers['content-type'])).to.have.string("application/json");

  const obj = JSON.parse(res.body);

  expect(obj).to.have.property('server');
  expect(obj).to.have.property('version');
  expect(obj).to.have.property('license');

  expect(obj.server).to.be.equal('arango');
  expect(obj.version).to.match(/[0-9]+\.[0-9]+\.([0-9]+|(milestone|alpha|beta|devel|rc)[0-9]*)/);
  expect(obj.license).to.match(/enterprise|community/g);
};

function versionJsonVpack() {
  const path = '/_api/version';
  const headers = {
    'content-type': 'application/json',
    'accept': 'application/x-velocypack',
    'accept-encoding': 'identity',
  };

  const res = arango.POST_RAW(path, "", headers);

  expect(String(res.headers['content-type'])).to.have.string("application/x-velocypack");

  const obj = VPACK_TO_V8(res.body);

  expect(obj).to.have.property('server');
  expect(obj).to.have.property('version');
  expect(obj).to.have.property('license');

  expect(obj.server).to.be.equal('arango');
  expect(obj.version).to.match(/[0-9]+\.[0-9]+\.([0-9]+|(milestone|alpha|beta|devel|rc)[0-9]*)/);
  expect(obj.license).to.match(/enterprise|community/g);
};

function versionVpackVpack() {
  const path = '/_api/version';
  const headers = {
    'content-type': 'application/x-velocypack',
    'accept': 'application/x-velocypack',
    'accept-encoding': 'identity',
  };

  const res = arango.POST_RAW(path, "", headers);

  expect(String(res.headers['content-type'])).to.have.string("application/x-velocypack");

  const obj = VPACK_TO_V8(res.body);

  expect(obj).to.have.property('server');
  expect(obj).to.have.property('version');
  expect(obj).to.have.property('license');

  expect(obj.server).to.be.equal('arango');
  expect(obj.version).to.match(/[0-9]+\.[0-9]+\.([0-9]+|(milestone|alpha|beta|devel|rc)[0-9]*)/);
  expect(obj.license).to.match(/enterprise|community/g);
};

function echoVpackVpack() {
  const path = '/_admin/echo';
  const headers = {
    'content-type': 'application/x-velocypack',
    'accept': 'application/x-velocypack',
    'accept-encoding': 'identity',
  };

  const obj = {"server": "arango", "version": "3.0.devel"};
  const body = V8_TO_VPACK(obj);

  const res = arango.POST_RAW(path, body, headers);

  expect(String(res.headers['content-type'])).to.have.string("application/x-velocypack");
  const replyBody = VPACK_TO_V8(res.body);
  expect(replyBody.requestBody).to.be.a('string');
};


/*
The following tests are for requests to _admin/execute in which the body is in velocypack format.
It's only interpreted as velocypack if the header has the content type for velocypack in the request.
Otherwise, as it happens when there's no content-type in the header, it's interpreted as JSON and will return an error
 */

function adminExecuteWithHeaderVpack() {
  const path = '/_admin/execute';
  const headers = {
    'content-type': 'application/x-velocypack',
    'accept-encoding': 'identity',
  };

  const obj = "require(\"console\").log(\"abc\");";
  const body = V8_TO_VPACK(obj);

  // correct header set, so everything should work well
  const res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertNull(res.parsedBody);
}

function adminExecuteWithHeaderVpack2() {
  const path = '/_admin/execute';
  const headers = {
    'content-type': 'application/x-velocypack',
    'accept-encoding': 'identity',
  };

  const obj = "return \"abc\"";
  const body = V8_TO_VPACK(obj);

  // correct header set, so everything should work well
  const res = arango.POST_RAW(path, body, headers);
  assertEqual(res.code, 200);
  assertFalse(res.error);
  assertEqual(res.parsedBody, "abc");
}

function adminExecuteNoHeaderVpack() {
  const path = '/_admin/execute';
  const obj = "return \"abc\"";
  const body = V8_TO_VPACK(obj);

  // the request we are making contains binary data,
  // but we are intentionally not setting the correct
  // Content-Type header. the server has no chance of
  // interpreting this properly, so we are expecting a
  // 500 error back.
  const res = arango.POST_RAW(path, body);
  assertEqual(res.code, 500);
  assertTrue(res.error);
}

jsunity.run(RequestSuite);

return jsunity.done();
