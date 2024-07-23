/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertEqual, assertTrue, arango */

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
// / @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');

let canUseFailAt = function() {
  return arango.GET("/_admin/debug/failat") === true;
};

function failAtApiSuite() {
  'use strict';
  return {
    setUp: function () {
      arango.DELETE("/_admin/debug/failat");
    },
    
    tearDown: function () {
      arango.DELETE("/_admin/debug/failat");
    },
    
    testCanUse: function () {
      let res = arango.GET("/_admin/debug/failat");
      assertEqual(typeof res, "boolean");
      assertTrue(res);
    },
    
    testFailurePointsSet: function () {
      let res = arango.GET("/_admin/debug/failat/all");
      assertEqual([], res);

      res = arango.PUT("/_admin/debug/failat/piff", {});
      assertEqual(typeof res, "boolean");
      assertTrue(res);
      res = arango.GET("/_admin/debug/failat/all");
      assertEqual(["piff"], res);
      
      res = arango.PUT("/_admin/debug/failat/der-fuppes-wird-immer-schlimmer", {});
      assertEqual(typeof res, "boolean");
      assertTrue(res);
      res = arango.GET("/_admin/debug/failat/all");
      assertEqual(["der-fuppes-wird-immer-schlimmer", "piff"], res);

      res = arango.DELETE("/_admin/debug/failat/piff");
      assertEqual(typeof res, "boolean");
      assertTrue(res);
      res = arango.GET("/_admin/debug/failat/all");
      assertEqual(["der-fuppes-wird-immer-schlimmer"], res);

      res = arango.DELETE("/_admin/debug/failat");
      assertEqual(typeof res, "boolean");
      assertTrue(res);
      res = arango.GET("/_admin/debug/failat/all");
      assertEqual([], res);
      
      res = arango.DELETE("/_admin/debug/failat/abc");
      assertEqual(typeof res, "boolean");
      assertTrue(res);
      res = arango.GET("/_admin/debug/failat/all");
      assertEqual([], res);
    },
    
  };
}

if (canUseFailAt()) {
  jsunity.run(failAtApiSuite);
}
return jsunity.done();
