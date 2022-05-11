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

let api = "/_api/aql-builtin";
function dealing_with_the_builtin_AQL_functionsSuite () {
  return {

    test_fetches_the_list_of_functions: function() {
      let doc = arango.GET_RAW(api);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);

      let functions = doc.parsedBody['functions'];
      let found = { };
      functions.forEach(f => {
        assertEqual(typeof f, 'object');
        assertTrue(f.hasOwnProperty(("name")));
        assertTrue(f.hasOwnProperty(("arguments")));
        
        found[f["name"]] = f["name"];
      });

      // check for a few known functions;
      assertTrue(found.hasOwnProperty( "PI"));
      assertTrue(found.hasOwnProperty( "DEGREES"));
      assertTrue(found.hasOwnProperty( "RADIANS"));
      assertTrue(found.hasOwnProperty( "SIN"));
      assertTrue(found.hasOwnProperty( "COS"));
    }
  };
}

jsunity.run(dealing_with_the_builtin_AQL_functionsSuite);
return jsunity.done();
