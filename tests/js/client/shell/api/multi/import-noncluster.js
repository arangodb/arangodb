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

let api = "/_api/import";

////////////////////////////////////////////////////////////////////////////////;
// import with complete=true;
////////////////////////////////////////////////////////////////////////////////;
function import_with_completeEQtruSuite () {
  let cn = "UnitTestsImport";
  return {
    setUp: function() {
      db._drop(cn);
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_regular: function() {
      let cmd = api + `?collection=${cn}&type=auto&details=true&complete=true`;
      let body = "[{\"_key\":\"abc\",\"value1\":25,\"value2\":\"test\",\"allowed\":true},{\"_key\":\"abc\",\"name\":\"baz\"},{\"name\":{\"detailed\":\"detailed name\",\"short\":\"short name\"}}]";

      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_CONFLICT.code);
      assertTrue(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['errorNum'], internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code);

      assertEqual(db[cn].count(), 0);
    },
  };
}

jsunity.run(import_with_completeEQtruSuite);
return jsunity.done();
