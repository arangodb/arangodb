/*jshint globalstrict:false, strict:false */
/* global fail, getOptions, assertTrue, assertEqual, assertNotEqual */

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
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'javascript.allow-port-testing': false
  };
}
const testPort = require('internal').testPort;
var jsunity = require('jsunity');
var arangodb = require("@arangodb");
function testSuite() {
  function openPortForbidden(port) {
    try {
      let reply = testPort('tcp://0.0.0.0:' + port);
      fail();
    } catch (err) {
      assertEqual(arangodb.ERROR_FORBIDDEN, err.errorNum, 'while probing: ' + port);
    }
  }
  return {
    testOpenPort : function() {
      // The filter will only match the host part. We specify one anyways.
      openPortForbidden(12345);
      openPortForbidden(1234);
      openPortForbidden(123);
      openPortForbidden(12);
    }
  };
}
jsunity.run(testSuite);
return jsunity.done();
