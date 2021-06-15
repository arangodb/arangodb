/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual,
   assertNotEqual, arango, print */

// //////////////////////////////////////////////////////////////////////////////
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Manuel Pöter
// //////////////////////////////////////////////////////////////////////////////
const _ = require('lodash');
const jsunity = require('jsunity');
const internal = require('internal');
const arangodb = require('@arangodb');
const utils = require("./utils");
const db = arangodb.db;

'use strict';

let { debugCanUseFailAt,
      debugSetFailAt,
      debugResetRaceControl,
      debugRemoveFailAt,
      debugClearFailAt
    } = require('@arangodb/test-helper');

function ChaosSuite() {
  // generate a random collection name
  const cn = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));

  return {
    setUp: function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testWorkInParallel: function () {
      let code = `
      {
        docs = [];
        let r = Math.floor(Math.random() * 20000) + 1;
        for (let i = 0; i < r; ++i) {
          docs.push({ _key: key() });
        }
        let c = db._collection(${cn});
        let d = Math.random();
        try {
        if (d >= 0.9) {
          db._query("FOR i IN 1.." + (docs.length + 5) + " INSERT {} INTO " + c.name());
          print("INSERTING VIA AQL");
        } else if (d >= 0.8) {
          db._query("FOR doc IN " + c.name() + " LIMIT 138 REMOVE doc IN " + c.name());
          print("REMOVING VIA AQL");
        } else if (d >= 0.7) {
          db._query("FOR doc IN " + c.name() + " LIMIT 1338 REPLACE doc WITH { pfihg: 434, fjgjg: 555 } IN " + c.name());
          print("REPLACING VIA AQL");
        } else if (d >= 0.25) {
          print("INSERTING " + docs.length + " docs INTO " + c.name());
          c.insert(docs);
        } else {
          print("REMOVING " + docs.length + " docs FROM " + c.name());
          c.remove(docs);
        }
        } catch {}
        print("COUNT: ", c.count());
      }`;
      
      let tests = [];
      for (let i = 0; i < 3; ++i) {
        tests.push(["p" + i, code]);
      }
      // run the suite for 5 minutes
      utils.runTests(tests, 5 * 60, cn);
    },
  };
}

jsunity.run(ChaosSuite);

return jsunity.done();
