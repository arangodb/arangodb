/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global assertTrue, assertEqual, arango, Buffer */

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoTransaction sTests
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require('jsunity');
var internal = require('internal');
var arangodb = require('@arangodb');
const path = require('path');
var db = arangodb.db;
var origin = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:');

const FoxxManager = require('@arangodb/foxx/manager');
const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'interface');


const binaryMime = 'image/gif';
const textMime = 'text/plain';
const pixelStr = 'R0lGODlhAQABAIAAAP///wAAACH5BAEAAAAALAAAAAABAAEAAAICRAEAOw==';
const pixelGif = new Buffer(pixelStr, 'base64');

const cmpBuffer = function(a, b) {
  if (a.length !== b.length) {
    return false;
  }
  for (let i = 0; i < a.length; ++i) {
    if (a[i] !== b[i]) {
      return false;
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
      FoxxManager.uninstall(mount, {force: true});
      FoxxManager.install(basePath, mount);
    },

    tearDownAll: function () {
      // FoxxManager.uninstall(mount, {force: true});
    },
    
    //testFoxxInterfacePostBody: function () {
    //  print(origin)
    //  print(arango.POST(url, {'bla': 'blub'}))
    //},
    //testFoxxInterfaceGet: function () {
    //},
    //testFoxxInterfaceHead: function () {
    //},
    //testFoxxInterfacePostBodyBinary: function () {
    //  [
    //    'POST_RAW',
    //    'PUT_RAW',
    //    'PATCH_RAW',
    //    'DELETE_RAW'
    //  ].forEach((reqMethod) => {
    //    let res = arango[reqMethod](binUrl,
    //                                pixelGif,
    //                                {
    //                                  'content-type': binaryMime
    //                                });
    //    assertEqual(res.code, 200, res.body);
    //    assertTrue(res.body instanceof Buffer);
    //    
    //    let respBody = new Buffer(res.body);
    //    assertTrue(cmpBuffer(respBody, pixelGif), "whether the server sent us a proper one pixel gif");
    //    assertEqual(res.headers['content-length'], pixelGif.length);
    //    assertEqual(res.headers['content-type'], binaryMime);
    //    assertEqual(res.headers['test'], 'header');
    //    assertEqual(res.headers['request-type'], reqMethod);
    //  });
    //},
    //testFoxxInterfaceGetBinary: function () {
    //  let res = arango.GET_RAW(binUrl);
    //  assertEqual(res.code, 200, res.body);
    //  assertTrue(res.body instanceof Buffer);
    //  
    //  let respBody = new Buffer(res.body);
    //  assertTrue(cmpBuffer(respBody, pixelGif), "whether the server sent us a proper one pixel gif");
    //  assertEqual(res.headers['content-length'], pixelGif.length);
    //  assertEqual(res.headers['content-type'], binaryMime);
    //  assertEqual(res.headers['test'], 'header');
    //  assertEqual(res.headers['request-type'], 'GET_RAW');
    //},
    testFoxxInterfaceHeadBinary: function () {
      let res = arango.HEAD_RAW(binUrl);
      assertEqual(res.code, 200, res.body);
      assertTrue(res.body instanceof Buffer);
      
      // let respBody = new Buffer(res.body);
      // assertTrue(cmpBuffer(respBody, pixelGif), "whether the server sent us a proper one pixel gif");
      assertEqual(res.headers['content-length'], pixelGif.length);
      assertEqual(res.headers['content-type'], binaryMime);
      assertEqual(res.headers['test'], 'header');
      assertEqual(res.headers['request-type'], 'HEAD_RAW');
    },

    testFoxxInterfacePostBodyText: function () {
    },
    testFoxxInterfaceGetText: function () {
    },
    testFoxxInterfaceHeadText: function () {
    }


  };
}

jsunity.run(foxxInterfaceSuite);

return jsunity.done();
