/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
/// @author Koushal Kawade
/// @author Copyright 2026, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var db = require("@arangodb").db;

function parallelism() {
    const collName = 'coll';
    const viewName = 'testView';
  return {
    setUpAll: function () {
      db._dropView(viewName);
      db._drop(collName);
      let c = db._create(collName);
      let v = db._createView(viewName, 'arangosearch');

      // commitIntervalMsec set to a small value to ensure new segments are
      // frequently created.
      //
      // consolidationPolicy.maxSkewThreshold also set to a small value to prevent
      // frequent segment consolidation.
      v.properties(JSON.parse(`{
                                "links": {
                                    "${collName}": {
                                        "includeAllFields": true
                                        }
                                    },
                                "conlidationPolicy": {
                                    "type": "tier",
                                    "maxSkewThreshold": 0.01
                                    },
                                "commitIntervalMsec": 10
                                }`));

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({value: "test" + (i % 10), count: i});
        docs.push({name: i.toString(), "address": `${i}_addr`, age: i});
      }

      for (let i = 0; i < 10; ++i) {
        c.insert(docs);

        // Add a delay > commitInterval to allow time for new segment to be
        // created from the docs inserted until now.
        internal.sleep(0.02);
      }

      // sync the views
      db._query(`FOR d IN ${viewName} OPTIONS { waitForSync: true } LIMIT 1 RETURN d`);
    },

    tearDownAll: function () {
      db._dropView(viewName);
      db._drop(collName);
    },

    testNoSortWithoutFiltersExact() {
      let res = db._query(`FOR d IN ${viewName} SEARCH LIKE(d.name, "%9%") OPTIONS { parallelism: 5 } RETURN d`);
      let extra = res.getExtra();
      assertTrue(extra["stats"].hasOwnProperty("searchParallelism"));
      assertTrue(extra["stats"]["searchParallelism"] > 1);
    },
  };
}

jsunity.run(parallelism);

return jsunity.done();

