/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, assertUndefined, arango, Buffer */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
// /
// /
// / DISCLAIMER
// /
// / Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
// / Copyright holder is ArangoDB Inc, Cologne, Germany
// /
// / @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var internal = require('internal');
var arangodb = require('@arangodb');
const path = require('path');
var db = arangodb.db;
var origin = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:');
const FoxxManager = require('@arangodb/foxx/manager');
const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'interface');

const jsonMime = 'application/json';
const vpackMime = "application/x-velocypack";
const binaryMime = 'image/gif';
const textMime = 'text/plain';
const pixelStr = 'R0lGODlhAQABAIAAAP///wAAACH5BAEAAAAALAAAAAABAAEAAAICRAEAOw==';
const pixelGif = new Buffer(pixelStr, 'base64');
const vpackObjectSize = 73;

require("@arangodb/test-helper").waitForFoxxInitialized();

const cmpBuffer = function(a, b) {
  if (a.length !== b.length) {
    return false;
  }
  if (typeof b === 'string') {
    for (let i = 0; i < a.length; ++i) {
      if (a[i] !== b.charCodeAt(i)) {
        return false;
      }
    }
  } else {
    for (let i = 0; i < a.length; ++i) {
      if (a[i] !== b[i]) {
        return false;
      }
    }
  }
  return true;
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function foxxInterfaceSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  const mount = '/test-interface';
  const url = `/_db/${db._name()}${mount}/interface-echo`;
  const binUrl = `${url}-bin`;
  const txtUrl = `${url}-str`;
  return {

    setUpAll: function () {
      FoxxManager.install(basePath, mount);
    },

    tearDownAll: function () {
      FoxxManager.uninstall(mount, {force: true});
    },
    
    testFoxxInterfacePostBody: function () {
      [
        'POST_RAW',
        'PUT_RAW',
        'PATCH_RAW',
        'DELETE_RAW'
      ].forEach((reqMethod) => {
        let res = arango[reqMethod](url,
                                    {
                                      pixelStr
                                    },
                                    {
                                      'content-type': jsonMime
                                    });
        assertEqual(res.code, 200, { meth: reqMethod, replyBody: res.parsedBody });
        assertTrue(typeof res.parsedBody === 'object');
        
        assertEqual(JSON.stringify(res.parsedBody), JSON.stringify({ pixelStr }));
        assertEqual(res.headers['content-length'], vpackObjectSize);
        assertEqual(res.headers['content-type'], vpackMime);
        assertEqual(res.headers['test'], 'header');
        assertEqual(res.headers['request-type'], reqMethod);
      });
    },
    testFoxxInterfaceGet: function () {
      let res = arango.GET_RAW(url);
      assertEqual(res.code, 200, res.parsedBody);
      assertTrue(typeof res.parsedBody === 'object');
      
      assertEqual(JSON.stringify(res.parsedBody), JSON.stringify({ pixelStr }));
      assertEqual(res.headers['content-length'], vpackObjectSize);
      assertEqual(res.headers['content-type'], vpackMime);
      assertEqual(res.headers['test'], 'header');
      assertEqual(res.headers['request-type'], 'GET_RAW');
    },
    testFoxxInterfaceHead: function () {
      let res = arango.HEAD_RAW(url);
      assertEqual(res.code, 200, res.parsedBody);
      assertUndefined(res.body);
      assertEqual(res.headers['content-length'], vpackObjectSize);
      assertEqual(res.headers['content-type'], vpackMime);
      assertEqual(res.headers['test'], 'header');
      assertEqual(res.headers['request-type'], 'HEAD_RAW');
    },
    testFoxxInterfacePostBodyBinary: function () {
      [
        'POST_RAW',
        'PUT_RAW',
        'PATCH_RAW',
        'DELETE_RAW'
      ].forEach((reqMethod) => {
        let res = arango[reqMethod](binUrl,
                                    pixelGif,
                                    {
                                      'content-type': binaryMime
                                    });
        assertEqual(res.code, 200, { meth: reqMethod, replyBody: res.parsedBody });
        assertTrue(res.body instanceof Buffer);
        
        let respBody = new Buffer(res.body);
        assertTrue(cmpBuffer(respBody, pixelGif), "whether the server sent us a proper one pixel gif");
        assertEqual(res.headers['content-length'], pixelGif.length);
        assertEqual(res.headers['content-type'], binaryMime);
        assertEqual(res.headers['test'], 'header');
        assertEqual(res.headers['request-type'], reqMethod);
      });
    },
    testFoxxInterfaceGetBinary: function () {
      let res = arango.GET_RAW(binUrl);
      assertEqual(res.code, 200, res.parsedBody);
      assertTrue(res.body instanceof Buffer);
      
      let respBody = new Buffer(res.body);
      assertTrue(cmpBuffer(respBody, pixelGif), "whether the server sent us a proper one pixel gif");
      assertEqual(res.headers['content-length'], pixelGif.length);
      assertEqual(res.headers['content-type'], binaryMime);
      assertEqual(res.headers['test'], 'header');
      assertEqual(res.headers['request-type'], 'GET_RAW');
    },
    testFoxxInterfaceHeadBinary: function () {
      let res = arango.HEAD_RAW(binUrl);
      assertEqual(res.code, 200, res.parsedBody);
      assertTrue(res.body instanceof Buffer);
      assertEqual(res.body.length, 0);
      
      assertEqual(res.headers['content-length'], pixelGif.length);
      assertEqual(res.headers['content-type'], binaryMime);
      assertEqual(res.headers['test'], 'header');
      assertEqual(res.headers['request-type'], 'HEAD_RAW');
    },

    testFoxxInterfacePostBodyText: function () {
      [
        'POST_RAW',
        'PUT_RAW',
        'PATCH_RAW',
        'DELETE_RAW'
      ].forEach((reqMethod) => {
        let res = arango[reqMethod](txtUrl,
                                    pixelStr,
                                    {
                                      'content-type': textMime
                                    });
        assertEqual(res.code, 200, { meth: reqMethod, replyBody: res.parsedBody });
        assertTrue(typeof res.body === 'string');

        assertEqual(res.body, pixelStr, "whether the server sent us a proper one pixel gif");
        assertEqual(res.headers['content-length'], pixelStr.length);
        assertEqual(res.headers['content-type'], textMime);
        assertEqual(res.headers['test'], 'header');
        assertEqual(res.headers['request-type'], reqMethod);
      });
    },
    testFoxxInterfaceGetText: function () {
      let res = arango.GET_RAW(txtUrl);
      assertEqual(res.code, 200, res.parsedBody);
      assertTrue(typeof res.body === 'string');

      assertEqual(res.body, pixelStr, "whether the server sent us a proper one pixel gif");
      assertEqual(res.headers['content-length'], pixelStr.length);
      assertEqual(res.headers['content-type'], textMime);
      assertEqual(res.headers['test'], 'header');
      assertEqual(res.headers['request-type'], 'GET_RAW');
    },
    testFoxxInterfaceHeadText: function () {
      let res = arango.HEAD_RAW(txtUrl);
      assertEqual(res.code, 200, res.parsedBody);
      assertUndefined(res.body);
      assertEqual(res.headers['content-length'], pixelStr.length);
      assertEqual(res.headers['content-type'], textMime);
      assertEqual(res.headers['test'], 'header');
      assertEqual(res.headers['request-type'], 'HEAD_RAW');
    }
  };
}

jsunity.run(foxxInterfaceSuite);

return jsunity.done();
