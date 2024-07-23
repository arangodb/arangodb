/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertNotMatch, arango */

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
/// @author Jan Steemann
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'server.options-api': "public"
  };
}

const jsunity = require('jsunity');

function testSuite() {
  return {
    testApiGetOptions : function() {
      let res = arango.GET("/_admin/options");
      
      assertTrue(res.hasOwnProperty("server.storage-engine"));
      assertEqual("rocksdb", res["server.storage-engine"]);

      Object.keys(res).forEach((key) => {
        assertNotMatch(/(passwd|password|secret)/, key, key);
      });
    },
    
    testApiGetOptionsDescription : function() {
      let res = arango.GET("/_admin/options-description");
      
      assertTrue(res.hasOwnProperty("server.storage-engine"));

      Object.values(res).forEach((value) => {
        assertTrue(value.hasOwnProperty("section"), value);
        assertTrue(value.hasOwnProperty("description"), value);
        assertTrue(value.hasOwnProperty("category"), value);
        assertTrue(value.hasOwnProperty("type"), value);
        assertTrue(value.hasOwnProperty("os"), value);
        assertTrue(value.hasOwnProperty("component"), value);
        assertTrue(value.hasOwnProperty("introducedIn"), value);
        assertTrue(value.hasOwnProperty("dynamic"), value);
      });
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
