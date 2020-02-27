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

print("santoeuh")
const FoxxManager = require('@arangodb/foxx/manager');
print("santoeuh")
const basePath = path.resolve(internal.pathForTesting('common'), 'test-data', 'apps', 'interface');


const mime = 'image/gif';
const pixelGif = new Buffer('R0lGODlhAQABAIAAAP///wAAACH5BAEAAAAALAAAAAABAAEAAAICRAEAOw==', 'base64');



// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function foxxInterfaceSuite () {
  'use strict';
  const cn = "UnitTestsCollection";
  const mount = '/test-interface';
  const url = `/_db/${db._name()}${mount}/interface-echo`;
  const binUrl = `${url}-bin`;
  return {

    setUpAll: function () {
      print("setup")
      FoxxManager.uninstall(mount, {force: true});
      FoxxManager.install(basePath, mount);
    },

    tearDownAll: function () {
      // FoxxManager.uninstall(mount, {force: true});
    },
    
    testFoxxInterfacePostBody: function () {
      print(origin)
      print(arango.POST(url, {'bla': 'blub'}))
    },
    testFoxxInterfaceGet: function () {
    },
    testFoxxInterfaceHead: function () {
    },
    testFoxxInterfacePostBodyBinary: function () {
      print(origin)
      print(arango.POST(binUrl, pixelGif, {'content-type': mime}));
    },
    testFoxxInterfaceGetBinary: function () {
    },
    testFoxxInterfaceHeadBinary: function () {
    }
  };
}

jsunity.run(foxxInterfaceSuite);

return jsunity.done();
