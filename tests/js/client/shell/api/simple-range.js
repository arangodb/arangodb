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

let api = "/_api/simple";

////////////////////////////////////////////////////////////////////////////////;
// range query;
////////////////////////////////////////////////////////////////////////////////;
function range_querySuite () {
  let cn = "UnitTestsCollectionRange";
  return {
    setUp: function() {
      db._create(cn);
    },

    tearDown: function() {
      db._drop(cn);
    },

    test_finds_the_examples: function() {
      // create data;
      let docs = [];
      [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ].forEach( i => 
        docs.push({ "i" : i })
      );
      db[cn].save(docs);

      // create index;
      let cmd = `/_api/index?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : true, "fields" : [ "i" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201, doc);
      assertEqual(doc.parsedBody['type'], "skiplist");
      assertTrue(doc.parsedBody['unique']);

      // range;
      cmd = api + "/range";
      body = { "collection" : cn, "attribute" : "i", "left" : 2, "right" : 4 };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201, doc);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 2);
      assertEqual(doc.parsedBody['count'], 2);
      let other = [];
      doc.parsedBody['result'].forEach( i => other.push(i['i']));
      assertEqual(other, [2,3]);

      // closed range;
      cmd = api + "/range";
      body = { "collection" : cn, "attribute" : "i", "left" : 2, "right" : 4, "closed" : true };
      doc = arango.PUT_RAW(cmd, body);

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 3);
      assertEqual(doc.parsedBody['count'], 3);
      other = [];
      doc.parsedBody['result'].forEach( i => other.push(i['i']));
      assertEqual(other, [2,3,4]);
    },

    test_finds_the_examples__big_result: function() {
      // create data;
      let docs = [];
      for (let i = 0; i < 499; i++) {
        docs.push({ "i" : i });
      }
      db[cn].save(docs);

      // create index;
      let cmd = `/_api/index?collection=${cn}`;
      let body = { "type" : "skiplist", "unique" : false, "fields" : [ "i" ] };
      let doc = arango.POST_RAW(cmd, body);

      assertEqual(doc.code, 201);

      // range;
      cmd = api + "/range";
      body = { "collection" : cn, "attribute" : "i", "left" : 5, "right" : 498 };
      doc = arango.PUT_RAW(cmd, body);

      let cmp = [ ];
      for (let i = 5; i <= 497; i++){ cmp.push(i); }

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 493);
      assertEqual(doc.parsedBody['count'], 493);
      let other = [];
      doc.parsedBody['result'].forEach( i => other.push(i['i']));
      assertEqual(other, cmp);

      // closed range;
      body = { "collection" : cn, "attribute" : "i", "left" : 2, "right" : 498, "closed" : true };
      doc = arango.PUT_RAW(cmd, body);

      cmp = [ ];
      for (let i = 2; i <= 498; i++){ cmp.push(i); }

      assertEqual(doc.code, 201);
      assertEqual(doc.headers['content-type'], contentType);
      assertFalse(doc.parsedBody['error']);
      assertEqual(doc.parsedBody['code'], 201);
      assertFalse(doc.parsedBody['hasMore']);
      assertEqual(doc.parsedBody['result'].length, 497);
      assertEqual(doc.parsedBody['count'], 497);
      other = [];
      doc.parsedBody['result'].forEach( i => other.push(i['i']));
      assertEqual(other, cmp);
    }
  };
}

jsunity.run(range_querySuite);
return jsunity.done();
