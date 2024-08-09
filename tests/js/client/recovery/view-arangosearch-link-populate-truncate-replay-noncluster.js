/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertTrue, assertFalse, assertNull, fail */
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
/// @author Andrei Lobov
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const arangodb = require('@arangodb');
const db = arangodb.db;
const internal = require('internal');
const jsunity = require('jsunity');
const cn = 'UnitTestsTruncateDummy';
const vn = cn + 'View';

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  db._drop(cn);
  var c = db._create(cn);

  db._dropView(vn);
  db._createView(vn, 'arangosearch', {});

  var meta = { links: { [cn]: { includeAllFields: true } } };
  db._view(vn).properties(meta);

  // 35k to overcome RocksDB optimization and force use truncate
  for (let i = 0; i < 35000; i++) {
    c.save({ a: "foo_" + i, b: "bar_" + i, c: i, _key: "doc_" + i });
  }

  c.save({ name: "crashme" }, { waitForSync: true });
  global.instanceManager.debugSetFailAt("ArangoSearchTruncateFailure");

  try {
    c.truncate();
  } catch (ex) {}
  return 0;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief test suite
// //////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {


    // //////////////////////////////////////////////////////////////////////////////
    // / @brief test whether we can restore the trx data
    // //////////////////////////////////////////////////////////////////////////////

    testIResearchLinkPopulateTruncate: function () {
      var v = db._view(vn);
      assertEqual(v.name(), vn);
      assertEqual(v.type(), 'arangosearch');
      var p = v.properties().links;
      assertTrue(p.hasOwnProperty(cn));
      var result = db._query("FOR doc IN " + vn + " SEARCH doc.c >= 0 OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO length RETURN length").toArray();
      var expectedResult = db._query("FOR doc IN " + cn + " FILTER doc.c >= 0 COLLECT WITH COUNT INTO length RETURN length").toArray();
      assertEqual(result[0], expectedResult[0]);
    }
 };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief executes the test suite
// //////////////////////////////////////////////////////////////////////////////

jsunity.run(recoverySuite);
return jsunity.done();
