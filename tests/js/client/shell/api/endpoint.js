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


let api = "/_api/endpoint";
////////////////////////////////////////////////////////////////////////////////;
// error handling;
////////////////////////////////////////////////////////////////////////////////;


function error_handlingSuite () {
  let name = "UnitTestsDatabase";
  return {

    setUp: function() {
      arango.DELETE_RAW(`/_api/database/${name}`);

      let body = {"name" : name };
      let doc = arango.POST_RAW("/_api/database", body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      let response = doc.parsedBody;
      assertTrue(response["result"]);
      assertFalse(response["error"]);
    },

    tearDown: function() {
      arango.DELETE_RAW(`/_api/database/${name}`);
    },

    test_use_non_system_database: function() {
      let doc = arango.GET_RAW(`/_db/${name}${api}`);

      assertEqual(doc.code, internal.errors.ERROR_HTTP_FORBIDDEN.code);
      assertEqual(doc.headers['content-type'], contentType);
      let response = doc.parsedBody;
      assertTrue(response["error"]);
      assertEqual(response["errorNum"], internal.errors.ERROR_ARANGO_USE_SYSTEM_DATABASE.code);
    },

    test_use_non_existent_database: function() {
      let doc = arango.GET_RAW("/_db/foobar${api}");

      assertEqual(doc.code, internal.errors.ERROR_HTTP_NOT_FOUND.code);
      assertEqual(doc.headers['content-type'], contentType);
      let response = doc.parsedBody;
      assertTrue(response["error"]);
      assertEqual(response["errorNum"], internal.errors.ERROR_ARANGO_DATABASE_NOT_FOUND.code);
    },

  };
}

////////////////////////////////////////////////////////////////////////////////;
// endpoints;
////////////////////////////////////////////////////////////////////////////////;
function retrieving_endpointsSuite () {
  return {
    test_retrieves_endpoints: function() {
      let doc = arango.GET_RAW(api);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      let response = doc.parsedBody;
      assertEqual(typeof response[0]["endpoint"], 'string');
    },

  };
}
jsunity.run(error_handlingSuite);
jsunity.run(retrieving_endpointsSuite);
return jsunity.done();
