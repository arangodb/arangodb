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


let didRegex = /^([0-9a-zA-Z]+)\/([0-9a-zA-Z\-_]+)/;

////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;
function import_handlingSuite () {
  let cn = "UnitTestsCollectionBasics";
  return {
    test_is_able_to_import_json_documents: function() {
      let id = db._create(cn );

      let cmd = `/_api/replication/restore-data?collection=${cn}`;
      let body =
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx1\",\"_rev\":\"_W2GDlX--_j\" , \"foo\" : \"bar1\" }}\n" +
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx2\",\"_rev\":\"_W2GDlX--_k\" , \"foo\" : \"bar1\" }}";

      try {
        let doc = arango.PUT_RAW(cmd, body, { 'content-type': "application/json"} );

        assertEqual(doc.code, 200);

        assertEqual(db[cn].count(), 2);
      } finally {
        db._drop(cn);
      }
    },

    test_returns_an_error_if_an_string_attribute_in_the_JSON_body_is_corrupted: function() {
      let id = db._create(cn);

      let cmd = `/_api/replication/restore-data?collection=${cn}`;
      let body =
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx1\",\"_rev\":\"_W2GDlX--_j\" , \"foo\" : \"bar1\" }}\n" +
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx2\",\"_rev\":\"_W2GDlX--_k\" , \"foo\" : \"bar1\" }}\n" +
          "{ \"type\" : 2300, \"data\" : { \"_key\" : \"1234xxx3\",\"_rev\":\"_W2GDlX--_l\" , \"foo\" : \"duplicate\", \"foo\" : \"bar\xff\" }}";

      try {
        let doc = arango.PUT_RAW(cmd, body, { 'content-type': "application/json"} );

        assertEqual(doc.code, 400, doc);
        assertTrue(doc.parsedBody['error']);
        assertEqual(doc.parsedBody['errorNum'], 600);
        assertEqual(doc.parsedBody['code'], 400);
        assertEqual(doc.headers['content-type'], contentType);

        assertEqual(db[cn].count(), 0);
      } finally {
        db._drop(cn);
      }
    },
  };
}

jsunity.run(import_handlingSuite);
return jsunity.done();
