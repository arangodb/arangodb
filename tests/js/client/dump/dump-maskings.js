/*jshint globalstrict:false, strict:false, maxlen:4000 */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, assertNotNull */

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
// / @author Frank Celler
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const users = require("@arangodb/users");

function dumpMaskingSuite () {
  'use strict';
  const db = internal.db;

  return {
    testGeneral : function () {
      const c = db._collection("maskings1");
      const d = c.document("1");

      assertNotNull(d, "document '1' was restored");
      assertEqual(d.name, "xxxxo xxxxd  xxxs xs a xxxt a xxxxxxxxl");
      assertEqual(d.blub.name, "xxxlo xxxld  xxis is a xxst in a xxxxxxxxct");
      assertEqual(d.email.length, 5);
      assertEqual(d.email[0], "xxxxing xxxays");
      assertEqual(d.email[1], "xhis is xxxxher one");
      assertEqual(d.email[2].something, "something else");
      assertEqual(d.email[3].email, "within a subject");
      assertEqual(d.email[4].name.length, 2);
      assertEqual(d.email[4].name[0], "xxxxls xxxxin a xxxxxct");
      assertEqual(d.email[4].name[1], "as xxst");
      assertEqual(d.sub.name, "xxis is a xxme xxaf xxxxxxxte");
      assertEqual(d.sub.email.length, 2);
      assertEqual(d.sub.email[0], "in this case as list");
      assertEqual(d.sub.email[1], "with more than one entry");
    },

    testRandomString : function () {
      const c = db._collection("maskings2");
      const d = c.document("2");

      assertNotEqual(d.random, "a");
      assertNotEqual(d.zip, "12345");
      assertNotEqual(d.date, "2018-01-01");
      assertNotEqual(d.integer, 100);
      assertNotEqual(d.ccard, "1234 1234 1234 1234");
      assertNotEqual(d.phone, "abcd 1234");
      assertNotEqual(d.emil, "me@you.here");
    }
};
}

jsunity.run(dumpMaskingSuite);

return jsunity.done();
