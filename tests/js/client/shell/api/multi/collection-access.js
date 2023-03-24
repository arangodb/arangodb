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

let api = "/_api/collection";

function readingSuite () {
  let cn = "UnitTestsCollectionAccess";
  let cid;
  return {
    setUp: function() {
      cid = db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_finds_the_collection_by_identifier: function() {
      let cmd = api + "/" + cid._id;
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
    },

    test_retrieves_the_collection_s_figures_by_identifier: function() {
      let cmd = api + "/" + cid._id + "/figures";
      let doc = arango.GET_RAW(cmd);

      assertEqual(doc.code, 200);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 200);
      assertEqual(doc.parsedBody['id'], cid._id);
      assertEqual(doc.parsedBody['name'], cn);
    }
  };
}
jsunity.run(readingSuite);
return jsunity.done();
